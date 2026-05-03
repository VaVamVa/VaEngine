#pragma once

#include "Common_Vulkan.h"

#include <string>
#include <vector>

// ============================================================
// Pipeline_Vulkan
//
// Vulkan의 그래픽스 파이프라인 전체를 관리합니다.
//
// DX12             │ Vulkan
// ─────────────────────────────────────────────────────────
// D3D12_GRAPHICS_PIPELINE_STATE_DESC │ VkGraphicsPipelineCreateInfo
// ID3D12PipelineState                │ VkPipeline
// ID3D12RootSignature                │ VkPipelineLayout + VkDescriptorSetLayout
// Shader bytecode (DXIL)             │ SPIR-V bytecode + VkShaderModule
//
// Vulkan 파이프라인의 특징:
//   DX12와 달리 Vulkan 파이프라인은 훨씬 많은 상태를 미리 결정합니다.
//   (Viewport, Scissor, Rasterizer, Blend, Depth 등 모두 생성 시 고정)
//   이는 드라이버 최적화를 위한 설계로, 런타임 상태 변경을 최소화합니다.
//   일부 상태(Viewport, Scissor)는 DynamicState로 설정해 런타임에 변경 가능합니다.
//
// Descriptor Set:
//   DX12의 Root Signature/Descriptor Heap에 해당합니다.
//   셰이더가 접근하는 리소스(Uniform Buffer, Texture 등)를 기술합니다.
// ============================================================
class Pipeline_Vulkan
{
public:
    // 정점 하나를 기술하는 구조체 (셰이더 입력과 레이아웃이 일치해야 합니다)
    struct Vertex
    {
        float position[3]; // location=0: (x, y, z)
        float color[3];    // location=1: (r, g, b)
    };

    // Uniform Buffer로 전달되는 MVP 행렬 (셰이더의 UniformBufferObject와 일치해야 합니다)
    struct UniformBufferObject
    {
        float model[16];      // 4×4 행렬 (row-major)
        float view[16];
        float projection[16];
    };

    ~Pipeline_Vulkan();

    // 파이프라인 초기화
    // renderPass:   SwapChain_Vulkan::GetRenderPass()로 얻은 RenderPass
    // vertSpvPath, fragSpvPath: 컴파일된 SPIR-V 파일 경로
    // assetManager: Android에서 APK assets을 읽기 위한 AAssetManager*.
    //               PC(Windows)에서는 nullptr (기본값).
    void Initialize(VkPhysicalDevice physicalDevice, VkDevice device,
                    VkRenderPass renderPass, VkExtent2D viewportExtent,
                    const std::string& vertSpvPath, const std::string& fragSpvPath,
                    void* assetManager = nullptr);

    void Shutdown();

    // ---- Getter ----
    VkPipeline           GetPipeline()           const { return pipeline; }
    VkPipelineLayout     GetPipelineLayout()     const { return pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return descriptorSetLayout; }
    VkDescriptorPool     GetDescriptorPool()     const { return descriptorPool; }

    // Uniform Buffer용 Descriptor Set 생성 + Uniform Buffer 연결
    VkDescriptorSet CreateDescriptorSet(VkBuffer uniformBuffer, VkDeviceSize uniformBufferSize);

private:
    void CreateDescriptorSetLayout();
    void CreateDescriptorPool();
    void CreateGraphicsPipeline(VkRenderPass renderPass, VkExtent2D viewportExtent,
                                const std::string& vertSpvPath, const std::string& fragSpvPath);

    VkShaderModule CreateShaderModule(const std::vector<char>& spirvCode);

    // assetManager: Android에서 AAssetManager*, PC에서 nullptr
    static std::vector<char> LoadSpirvFile(const std::string& path, void* assetManager);

private:
    VkDevice              deviceHandle      = VK_NULL_HANDLE;
    VkPhysicalDevice      physicalDevHandle = VK_NULL_HANDLE;

    // AAssetManager*: Android에서 Initialize 시 전달받아 보관.
    // LoadSpirvFile이 APK assets에서 셰이더를 읽을 때 사용합니다.
    void*                 assetManagerHandle = nullptr;

    // ---- 파이프라인 핵심 객체 ----

    // VkDescriptorSetLayout: 셰이더가 접근하는 리소스(Uniform, Texture 등)의 레이아웃.
    // DX12의 Root Signature에서 Parameter/Descriptor Range에 해당합니다.
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    // VkPipelineLayout: 파이프라인이 사용하는 모든 Descriptor Set Layout의 집합.
    // Push Constant 범위도 여기서 선언합니다.
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    // VkPipeline: 렌더링 파이프라인 전체 상태를 담은 핵심 객체.
    // DX12의 ID3D12PipelineState에 해당합니다.
    VkPipeline pipeline = VK_NULL_HANDLE;

    // VkDescriptorPool: Descriptor Set을 할당하기 위한 메모리 풀.
    // DX12의 Descriptor Heap과 유사합니다.
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};
