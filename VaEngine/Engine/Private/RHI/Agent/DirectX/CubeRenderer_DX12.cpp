#include "CubeRenderer_DX12.h"
#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "Mesh/Cube.h"
#include "RHI/DirectX/CommandList_DirectX.h"

#include <DirectXMath.h>
#include <stdexcept>
#include <vector>
#include <cstdint>

std::unique_ptr<CubeRenderer> CubeRenderer::Create()
{
	return std::make_unique<CubeRenderer_DX12>();
}

void CubeRenderer_DX12::Initialize(IRenderDevice* device)
{
	// 바인딩 레이아웃 (b0: MVP 상수버퍼, t0: Diffuse 텍스처)
	BindingEntry bindings[] = {
		{ EBindingType::ConstantBuffer, 0, EShaderStage::Vertex },
		{ EBindingType::Texture,        0, EShaderStage::Pixel  },
	};
	bindingLayout = device->CreateBindingLayout(bindings, 2);

	// PSO — TEXCOORD 추가 (stride: 3+4+2 floats = 36 bytes)
	VertexInputDesc inputs[] = {
		{ "POSITION", 0, EPixelFormat::R32G32B32_FLOAT,    0  },
		{ "COLOR",    0, EPixelFormat::R32G32B32A32_FLOAT, 12 },
		{ "TEXCOORD", 0, EPixelFormat::R32G32_FLOAT,       28 },
	};
	PipelineStateDesc psoDesc = {
		.vsPath           = DX_SHADER_DIR L"/CubeShader.hlsl",
		.psPath           = DX_SHADER_DIR L"/CubeShader.hlsl",
		.vertexInputs     = inputs,
		.vertexInputCount = 3,
		.rtvFormat        = EPixelFormat::R8G8B8A8_UNORM,
		.bindingLayout    = bindingLayout.get()
	};
	pipelineState = device->CreatePipelineState(psoDesc);

	// 상수 버퍼 + 메시
	constantBuffer = device->CreateConstantBuffer(sizeof(DirectX::XMFLOAT4X4));
	mesh = std::make_unique<Cube>();
	mesh->Initialize(device);

	// 체커보드 텍스처 (256×256 RGBA, 32px 타일)
	constexpr uint32_t W = 256, H = 256, TILE = 32;
	std::vector<uint32_t> pixels(W * H);
	for (uint32_t y = 0; y < H; ++y)
		for (uint32_t x = 0; x < W; ++x)
			pixels[y * W + x] = ((x / TILE + y / TILE) % 2) ? 0xFFFFFFFF : 0xFF3F3FBF;
	texture = device->CreateTexture();
	texture->LoadFromMemory(device, pixels.data(), W, H);
}

void CubeRenderer_DX12::Draw(ICommandList* cmdList)
{
	// MVP 행렬 계산 및 상수 버퍼 업로드
	auto now = std::chrono::steady_clock::now();
	float elapsed = std::chrono::duration<float>(now - startTime).count();

	using namespace DirectX;
	XMMATRIX model = XMMatrixRotationY(elapsed * XM_PIDIV2);
	XMMATRIX view  = XMMatrixLookAtLH(
		XMVectorSet(0.0f, 1.5f, -3.0f, 0.0f),
		XMVectorZero(),
		XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f)
	);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

	XMFLOAT4X4 mvp;
	XMStoreFloat4x4(&mvp, model * view * proj);
	constantBuffer->SetData(&mvp, sizeof(mvp));

	pipelineState->Bind(cmdList);
	constantBuffer->Bind(cmdList, 0);
	texture->Bind(cmdList, 1);

	static_cast<CommandList_DirectX*>(cmdList)->GetHandle()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mesh->Draw(cmdList);
}
