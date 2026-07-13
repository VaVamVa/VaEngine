#include "IBLRenderer.h"

#include "RHI/ICommandList.h"
#include "RHI/Pipeline/PipelineDesc.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"
#include "RHI/Common_RHI.h"

#include <algorithm>
#include <cstring>

namespace {
    struct CB_PrefilterParams { float roughness; float pad[3]; };
}

void IBLRenderer::Initialize(IRenderDevice* device,
                             const ShaderDesc& irradianceShaderDesc,
                             const ShaderDesc& prefilterShaderDesc,
                             const ShaderDesc& integrateBRDFShaderDesc)
{
    // ── Stage A: Diffuse Irradiance ──────────────────────────────────────────
    // root 0: t0 소스 파노라마, root 1: u0 출력 irradiance
    BindingEntry irrBindings[] = {
        { EBindingType::Texture,    0, EShaderStage::Compute },
        { EBindingType::TextureUAV, 0, EShaderStage::Compute },
    };
    irradianceBindingLayout = device->CreateBindingLayout(irrBindings, 2, /*isCompute*/ true);
    irradianceShader = device->CreateShader(irradianceShaderDesc);
    ComputePipelineStateDesc irrPsoDesc{};
    irrPsoDesc.shader        = irradianceShader.get();
    irrPsoDesc.bindingLayout = irradianceBindingLayout.get();
    irradiancePipelineState = device->CreateComputePipelineState(irrPsoDesc);

    // ── Stage B: Specular Prefiltered Map ────────────────────────────────────
    // root 0: t0 소스 파노라마, root 1: u0 출력(밉별 재바인딩), root 2: b0 CB_PrefilterParams(roughness)
    BindingEntry prefilterBindings[] = {
        { EBindingType::Texture,        0, EShaderStage::Compute },
        { EBindingType::TextureUAV,     0, EShaderStage::Compute },
        { EBindingType::ConstantBuffer, 0, EShaderStage::Compute },
    };
    prefilterBindingLayout = device->CreateBindingLayout(prefilterBindings, 3, /*isCompute*/ true);
    prefilterShader = device->CreateShader(prefilterShaderDesc);
    ComputePipelineStateDesc prefPsoDesc{};
    prefPsoDesc.shader        = prefilterShader.get();
    prefPsoDesc.bindingLayout = prefilterBindingLayout.get();
    prefilterPipelineState = device->CreateComputePipelineState(prefPsoDesc);

    prefilterParamsBuffer = device->CreateBuffer({
        .size   = sizeof(CB_PrefilterParams),
        .usage  = EBufferUsage::ConstantBuffer,
        .access = EMemoryAccess::Upload,
        .stride = 0
    });

    // ── Stage B: BRDF LUT (전역, 스카이박스 무관) ────────────────────────────
    // root 0: u0 출력만 — 소스 텍스처 입력 없음
    BindingEntry brdfBindings[] = {
        { EBindingType::TextureUAV, 0, EShaderStage::Compute },
    };
    brdfBindingLayout = device->CreateBindingLayout(brdfBindings, 1, /*isCompute*/ true);
    brdfShader = device->CreateShader(integrateBRDFShaderDesc);
    ComputePipelineStateDesc brdfPsoDesc{};
    brdfPsoDesc.shader        = brdfShader.get();
    brdfPsoDesc.bindingLayout = brdfBindingLayout.get();
    brdfPipelineState = device->CreateComputePipelineState(brdfPsoDesc);

    // 폴백 리소스만 생성하고 컴퓨트 디스패치는 절대 실행하지 않는다 — 스카이박스가 없으면
    // 계산 자체를 막아달라는 요구사항. SRV 바인딩 대상으로는 유효해야 하므로 리소스는 만들되,
    // gIBLEnabled==0이면 EvalAmbientIBL이 이 텍스처들을 아예 읽지 않으므로 내용은 무관하다.
    defaultIrradiance = device->CreateTextureUAV();
    defaultIrradiance->Create(device, EPixelFormat::R16G16B16A16_FLOAT, 1, 1);

    defaultPrefiltered = device->CreateTextureUAV();
    defaultPrefiltered->Create(device, EPixelFormat::R16G16B16A16_FLOAT, 1, 1, /*arraySize*/ 1, /*mipLevels*/ 1);

    // BRDF LUT — 스카이박스 유무와 무관하게 항상 1회 계산(roughness·NdotV만의 함수이므로
    // "계산 자체를 막는" 대상이 아니다 — Stage A/B의 스카이박스별 캐시와는 다른 결정).
    brdfLUT = device->CreateTextureUAV();
    brdfLUT->Create(device, EPixelFormat::R32G32_FLOAT, kBRDFLUTSize, kBRDFLUTSize);
    ComputeBRDFLUT(device);
}

uint32_t IBLRenderer::EnsurePrecomputed(IRenderDevice* device, ITexture* sourcePanorama)
{
    if (!sourcePanorama)
        return kInvalidHandle;

    for (uint32_t i = 0; i < static_cast<uint32_t>(targets.size()); ++i)
    {
        if (targets[i].sourcePanorama == sourcePanorama)
            return i;
    }

    IBLTarget target;
    target.sourcePanorama = sourcePanorama;

    target.irradianceMap = device->CreateTextureUAV();
    target.irradianceMap->Create(device, EPixelFormat::R16G16B16A16_FLOAT, kIrradianceWidth, kIrradianceHeight);
    ComputeIrradiance(device, sourcePanorama, target.irradianceMap.get());

    target.prefilteredMap = device->CreateTextureUAV();
    target.prefilteredMap->Create(device, EPixelFormat::R16G16B16A16_FLOAT,
                                  kPrefilterWidth, kPrefilterHeight,
                                  /*arraySize*/ 1, kPrefilterMipLevels);
    ComputePrefiltered(device, sourcePanorama, target.prefilteredMap.get());

    targets.push_back(std::move(target));
    return static_cast<uint32_t>(targets.size() - 1);
}

ITextureUAV* IBLRenderer::GetIrradianceMap(uint32_t handle) const
{
    if (handle < targets.size())
        return targets[handle].irradianceMap.get();
    return defaultIrradiance.get();
}

ITextureUAV* IBLRenderer::GetPrefilteredMap(uint32_t handle) const
{
    if (handle < targets.size())
        return targets[handle].prefilteredMap.get();
    return defaultPrefiltered.get();
}

void IBLRenderer::ComputeIrradiance(IRenderDevice* device, ITexture* source, ITextureUAV* output)
{
    device->ImmediateSubmit([&](ICommandList* cmdList)
    {
        irradiancePipelineState->Bind(cmdList);
        source->Bind(cmdList, 0, /*isCompute*/ true);     // root 0 → t0
        output->BindUAV(cmdList, 1, /*isCompute*/ true);  // root 1 → u0

        // 실제 출력 크기는 셰이더가 RWTexture2D::GetDimensions()로 직접 조회해 경계 검사하므로,
        // 여기서는 최대 해상도(kIrradianceWidth×kIrradianceHeight) 기준으로 그룹 수만 넉넉히 계산한다
        // (폴백의 1x1 출력처럼 더 작은 대상은 초과 스레드가 경계 검사에서 조용히 반환됨).
        const uint32_t groupX = (kIrradianceWidth  + 7) / 8;
        const uint32_t groupY = (kIrradianceHeight + 7) / 8;
        cmdList->Dispatch(groupX, groupY, 1);
    });
}

void IBLRenderer::ComputePrefiltered(IRenderDevice* device, ITexture* source, ITextureUAV* output)
{
    device->ImmediateSubmit([&](ICommandList* cmdList)
    {
        prefilterPipelineState->Bind(cmdList);
        source->Bind(cmdList, 0, /*isCompute*/ true);  // root 0 → t0 (밉 전체에서 공유)

        for (uint32_t mip = 0; mip < kPrefilterMipLevels; ++mip)
        {
            const float roughness = static_cast<float>(mip) / static_cast<float>(kPrefilterMipLevels - 1);
            const CB_PrefilterParams params{ roughness, {} };
            prefilterParamsBuffer->Upload(&params, sizeof(params));

            cmdList->SetComputeConstantBuffer(prefilterParamsBuffer.get(), 2);  // root 2 → b0
            output->BindUAV(cmdList, 1, /*isCompute*/ true, mip);               // root 1 → u0 (해당 밉)

            const uint32_t mipWidth  = std::max(1u, kPrefilterWidth  >> mip);
            const uint32_t mipHeight = std::max(1u, kPrefilterHeight >> mip);
            const uint32_t groupX = (mipWidth  + 7) / 8;
            const uint32_t groupY = (mipHeight + 7) / 8;
            cmdList->Dispatch(groupX, groupY, 1);
        }
    });
}

void IBLRenderer::ComputeBRDFLUT(IRenderDevice* device)
{
    device->ImmediateSubmit([&](ICommandList* cmdList)
    {
        brdfPipelineState->Bind(cmdList);
        brdfLUT->BindUAV(cmdList, 0, /*isCompute*/ true);  // root 0 → u0

        const uint32_t groupX = (kBRDFLUTSize + 7) / 8;
        const uint32_t groupY = (kBRDFLUTSize + 7) / 8;
        cmdList->Dispatch(groupX, groupY, 1);
    });
}
