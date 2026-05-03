#include "Pipeline_Vulkan.h"

#include <fstream>
#include <stdexcept>
#include <array>

// ============================================================
// 소멸자
// ============================================================
Pipeline_Vulkan::~Pipeline_Vulkan()
{
    Shutdown();
}

// ============================================================
// Initialize
// ============================================================
void Pipeline_Vulkan::Initialize(
    VkPhysicalDevice physicalDevice, VkDevice device,
    VkRenderPass renderPass, VkExtent2D viewportExtent,
    const std::string& vertSpvPath, const std::string& fragSpvPath,
    void* assetManager)
{
    deviceHandle       = device;
    physicalDevHandle  = physicalDevice;
    assetManagerHandle = assetManager; // Android: AAssetManager*, PC: nullptr

    // 셰이더가 접근하는 Descriptor(Uniform Buffer 등)의 레이아웃 선언
    CreateDescriptorSetLayout();

    // Descriptor Set을 할당할 Pool 생성
    CreateDescriptorPool();

    // 그래픽스 파이프라인(셰이더, 정점 입력, 래스터라이저 등) 생성
    CreateGraphicsPipeline(renderPass, viewportExtent, vertSpvPath, fragSpvPath);
}

// ============================================================
// Shutdown
// ============================================================
void Pipeline_Vulkan::Shutdown()
{
    if (deviceHandle == VK_NULL_HANDLE) { return; }
    if (descriptorPool      != VK_NULL_HANDLE) { vkDestroyDescriptorPool(deviceHandle, descriptorPool, nullptr);           descriptorPool = VK_NULL_HANDLE; }
    if (pipeline            != VK_NULL_HANDLE) { vkDestroyPipeline(deviceHandle, pipeline, nullptr);                       pipeline = VK_NULL_HANDLE; }
    if (pipelineLayout      != VK_NULL_HANDLE) { vkDestroyPipelineLayout(deviceHandle, pipelineLayout, nullptr);           pipelineLayout = VK_NULL_HANDLE; }
    if (descriptorSetLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(deviceHandle, descriptorSetLayout, nullptr); descriptorSetLayout = VK_NULL_HANDLE; }
}

// ============================================================
// CreateDescriptorSet
//
// 새 Descriptor Set을 Pool에서 할당하고, Uniform Buffer와 연결합니다.
// DX12의 "Constant Buffer Descriptor를 Descriptor Heap에 쓰는" 과정에 해당합니다.
// ============================================================
VkDescriptorSet Pipeline_Vulkan::CreateDescriptorSet(VkBuffer uniformBuffer, VkDeviceSize uniformBufferSize)
{
    // ---- Descriptor Set 할당 ----
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &descriptorSetLayout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(deviceHandle, &allocInfo, &descriptorSet));

    // ---- Uniform Buffer 연결 ----
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range  = uniformBufferSize;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet          = descriptorSet;
    descriptorWrite.dstBinding      = 0; // binding=0 (셰이더의 layout(binding=0))
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo     = &bufferInfo;

    vkUpdateDescriptorSets(deviceHandle, 1, &descriptorWrite, 0, nullptr);

    return descriptorSet;
}

// ============================================================
// [내부] CreateDescriptorSetLayout
//
// 셰이더의 binding=0에 Uniform Buffer가 있음을 선언합니다.
// DX12의 Root Signature에서 CBV를 b0에 선언하는 것과 동일합니다.
// ============================================================
void Pipeline_Vulkan::CreateDescriptorSetLayout()
{
    // binding=0: Uniform Buffer (MVP 행렬)
    // Vertex Shader에서 사용됨을 선언합니다.
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding            = 0;
    uboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount    = 1;
    uboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT; // Vertex Shader에서만 사용
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &uboLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(deviceHandle, &layoutInfo, nullptr, &descriptorSetLayout));
}

// ============================================================
// [내부] CreateDescriptorPool
//
// Descriptor Set을 할당하기 위한 Pool을 생성합니다.
// DX12의 Descriptor Heap과 유사합니다.
// ============================================================
void Pipeline_Vulkan::CreateDescriptorPool()
{
    // UNIFORM_BUFFER 타입 Descriptor를 최대 4개까지 할당할 수 있는 Pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 4; // 스왑체인 이미지 수 (최대)

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes    = &poolSize;
    poolInfo.maxSets       = 4; // 최대 Descriptor Set 수

    VK_CHECK(vkCreateDescriptorPool(deviceHandle, &poolInfo, nullptr, &descriptorPool));
}

// ============================================================
// [내부] CreateGraphicsPipeline
//
// Vulkan 그래픽스 파이프라인의 모든 단계를 구성합니다.
// DX12의 D3D12_GRAPHICS_PIPELINE_STATE_DESC를 채우고 CreatePipelineState()를
// 호출하는 것과 동일합니다.
//
// 파이프라인 단계 (간략):
//   [Input Assembler] → [Vertex Shader] → [Rasterizer] → [Fragment Shader] → [Output Merger]
// ============================================================
void Pipeline_Vulkan::CreateGraphicsPipeline(
    VkRenderPass renderPass, VkExtent2D viewportExtent,
    const std::string& vertSpvPath, const std::string& fragSpvPath)
{
    // ---- 1. 셰이더 모듈 생성 ----
    // SPIR-V 바이트코드를 로드하여 VkShaderModule로 변환합니다.
    // DX12의 DXIL bytecode를 D3D12_SHADER_BYTECODE에 담는 것과 유사합니다.
    // assetManagerHandle: PC에서는 nullptr(파일 시스템), Android에서는 AAssetManager*(APK assets)
    auto vertCode   = LoadSpirvFile(vertSpvPath, assetManagerHandle);
    auto fragCode   = LoadSpirvFile(fragSpvPath, assetManagerHandle);
    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);

    // 셰이더 스테이지 정보 (어떤 스테이지에서 어떤 셰이더를 사용할지)
    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertModule;
    vertStageInfo.pName  = "main"; // 셰이더 진입점 함수 이름

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragModule;
    fragStageInfo.pName  = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = { vertStageInfo, fragStageInfo };

    // ---- 2. 정점 입력 레이아웃 ----
    // DX12의 D3D12_INPUT_LAYOUT_DESC + D3D12_INPUT_ELEMENT_DESC에 해당합니다.
    // 셰이더 layout(location=N)과 Vertex 구조체의 필드가 일치해야 합니다.

    // Binding: 버퍼 하나가 어떤 주기로 데이터를 공급하는지 (Per-Vertex)
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;                         // 버퍼 슬롯 0번
    bindingDesc.stride    = sizeof(Pipeline_Vulkan::Vertex); // 정점 하나의 크기 (바이트)
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 정점마다 데이터 진행

    // Attribute: 정점 내 각 필드의 위치와 포맷
    std::array<VkVertexInputAttributeDescription, 2> attributeDescs{};

    // location=0: Position (vec3 = float×3)
    attributeDescs[0].binding  = 0;
    attributeDescs[0].location = 0;
    attributeDescs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[0].offset   = offsetof(Pipeline_Vulkan::Vertex, position);

    // location=1: Color (vec3 = float×3)
    attributeDescs[1].binding  = 0;
    attributeDescs[1].location = 1;
    attributeDescs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescs[1].offset   = offsetof(Pipeline_Vulkan::Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions    = attributeDescs.data();

    // ---- 3. Input Assembly ----
    // 정점들을 어떤 도형으로 조립할지 (삼각형, 선, 점 등)
    // DX12의 D3D12_PRIMITIVE_TOPOLOGY_TYPE에 해당합니다.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 정점 3개마다 삼각형
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ---- 4. Viewport & Scissor (Dynamic State로 설정) ----
    // DynamicState로 등록하면 파이프라인 재생성 없이 CommandBuffer에서 변경 가능합니다.
    VkViewport viewport{};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(viewportExtent.width);
    viewport.height   = static_cast<float>(viewportExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = viewportExtent;

    // Viewport와 Scissor는 Dynamic State로 설정 (Resize 대응)
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates    = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // ---- 5. Rasterizer ----
    // 삼각형을 픽셀로 변환하는 방식을 설정합니다.
    // DX12의 D3D12_RASTERIZER_DESC에 해당합니다.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL; // 채우기 (선: LINE, 점: POINT)
    rasterizer.lineWidth               = 1.0f;
    // 뒷면(시계 반대 방향으로 보이는 면) 제거 - 성능 최적화
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
    // 정면 판별 기준: 시계 방향(DX12 기본값과 동일)
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // ---- 6. Multisampling (MSAA 없음) ----
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ---- 7. Color Blend ----
    // 픽셀을 렌더 타겟에 쓰는 방식. 불투명 렌더링이므로 블렌딩 없음.
    // DX12의 D3D12_BLEND_DESC에 해당합니다.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE; // 블렌딩 없음

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // ---- 8. Pipeline Layout (Descriptor Set + Push Constants) ----
    // DX12의 Root Signature 생성에 해당합니다.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 1;
    pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Push Constants 없음

    VK_CHECK(vkCreatePipelineLayout(deviceHandle, &pipelineLayoutInfo, nullptr, &pipelineLayout));

    // ---- 9. 파이프라인 생성 ----
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages             = shaderStages.data();
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicStateInfo;
    pipelineInfo.layout              = pipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0; // 사용할 Subpass 인덱스
    // basePipelineHandle: 파이프라인 상속 (유사한 파이프라인 생성 시 성능 향상 가능)
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    // VK_NULL_HANDLE: Pipeline Cache (빌드 결과 캐싱으로 재시작 시 빠른 로딩 가능, 지금은 미사용)
    VK_CHECK(vkCreateGraphicsPipelines(deviceHandle, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    // 파이프라인 생성 후 셰이더 모듈은 더 이상 필요 없으므로 해제합니다.
    vkDestroyShaderModule(deviceHandle, vertModule, nullptr);
    vkDestroyShaderModule(deviceHandle, fragModule, nullptr);
}

// ============================================================
// [내부 유틸] CreateShaderModule
// ============================================================
VkShaderModule Pipeline_Vulkan::CreateShaderModule(const std::vector<char>& spirvCode)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size();
    // reinterpret_cast: char → uint32_t* (SPIR-V는 uint32_t 단위)
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(spirvCode.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(deviceHandle, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

// ============================================================
// [내부 유틸] LoadSpirvFile
//
// 플랫폼에 따라 SPIR-V 바이트코드를 다른 방법으로 로드합니다.
//
//   PC (Windows 등):
//     일반 파일 시스템에서 std::ifstream으로 읽습니다.
//     path = "Shaders/Vulkan/mesh.vert.spv" (실행파일 기준 상대경로)
//
//   Android:
//     APK 내 assets/ 폴더에서 AAssetManager로 읽습니다.
//     AAsset: Android APK 내부 파일 핸들 (Windows의 HANDLE에 대응)
//     path = "Shaders/Vulkan/mesh.vert.spv" (assets/ 기준 상대경로)
// ============================================================
std::vector<char> Pipeline_Vulkan::LoadSpirvFile(const std::string& path, void* assetManager)
{
#ifdef ANDROID
    // ---- Android: APK assets에서 로드 ----
    //
    // AAssetManager_open: assets/ 폴더의 파일을 열어 AAsset* 핸들을 반환합니다.
    // AASSET_MODE_BUFFER: 파일 전체를 메모리에 버퍼링 (소규모 파일에 적합)
    AAsset* asset = AAssetManager_open(
        static_cast<AAssetManager*>(assetManager),
        path.c_str(),
        AASSET_MODE_BUFFER);

    if (!asset)
    {
        throw std::runtime_error("[Vulkan] APK 에셋에서 SPIR-V를 열 수 없습니다: " + path);
    }

    // AAsset_getLength: 파일 크기를 바이트 단위로 반환
    size_t size = static_cast<size_t>(AAsset_getLength(asset));
    std::vector<char> buffer(size);

    // AAsset_read: 데이터를 buffer로 복사
    AAsset_read(asset, buffer.data(), size);

    // AAsset_close: 파일 핸들 반환 (std::ifstream의 close()에 대응)
    AAsset_close(asset);
    return buffer;

#else
    // ---- PC: 파일 시스템에서 로드 ----
    //
    // ate: 파일 끝에서 열어 파일 크기를 tellg()로 즉시 파악
    // binary: 줄바꿈 변환 없이 바이트 단위로 읽기 (SPIR-V는 바이너리)
    (void)assetManager;
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("[Vulkan] SPIR-V 파일을 열 수 없습니다: " + path);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
#endif
}
