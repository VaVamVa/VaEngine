#include "Mesh/SkinnedMesh.h"
#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Common_RHI.h"

void SkinnedMesh::Initialize(IRenderDevice* device, const SkinnedMeshData& data)
{
    vertexStride   = data.vertexStride;
    vertexByteSize = static_cast<uint32_t>(data.vertices.size());
    indexCount     = static_cast<uint32_t>(data.indices.size());

    BufferDesc vbDesc = {
        .size   = data.vertices.size(),
        .usage  = EBufferUsage::VertexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = data.vertexStride
    };
    vertexBuffer = device->CreateBuffer(vbDesc);
    vertexBuffer->Upload(data.vertices.data(), data.vertices.size());

    BufferDesc ibDesc = {
        .size   = data.indices.size() * sizeof(uint16_t),
        .usage  = EBufferUsage::IndexBuffer,
        .access = EMemoryAccess::Upload,
        .stride = sizeof(uint16_t)
    };
    indexBuffer = device->CreateBuffer(ibDesc);
    indexBuffer->Upload(data.indices.data(), data.indices.size() * sizeof(uint16_t));
}

void SkinnedMesh::Draw(ICommandList* cmdList)
{
    cmdList->SetVertexBuffer(vertexBuffer.get(), vertexStride, vertexByteSize);
    cmdList->SetIndexBuffer(indexBuffer.get(), EIndexFormat::UInt16, indexCount * sizeof(uint16_t));
    cmdList->DrawIndexed(indexCount);
}

void SkinnedMesh::DrawInstanced(ICommandList* cmdList, uint32_t instanceCount)
{
    cmdList->SetVertexBuffer(vertexBuffer.get(), vertexStride, vertexByteSize);
    cmdList->SetIndexBuffer(indexBuffer.get(), EIndexFormat::UInt16, indexCount * sizeof(uint16_t));
    cmdList->DrawIndexedInstanced(indexCount, instanceCount);
}

void SkinnedMesh::CreateBonePalette(IRenderDevice* device, uint32_t inMaxInstances)
{
    // BonePaletteCompute.hlsl MAX_MODEL_TRANSFORMS와 동일 — indexing 통일 위해 mesh의 실제 본 수와 무관
    constexpr uint32_t kBonesPerInstance = 250;
    constexpr uint32_t kFloat4PerBone    = 4;
    constexpr uint32_t kFloat4Stride     = sizeof(float) * 4;

    maxInstances = inMaxInstances;
    const uint32_t numFloat4 = inMaxInstances * kBonesPerInstance * kFloat4PerBone;

    bonePaletteBuffer = device->CreateBuffer({
        .size   = static_cast<uint64_t>(numFloat4) * kFloat4Stride,
        .usage  = EBufferUsage::UnorderedAccess,
        .access = EMemoryAccess::Default,
        .stride = kFloat4Stride
    });

    bonePaletteUAV = device->CreateBufferUAV(bonePaletteBuffer.get(), numFloat4, kFloat4Stride);
    bonePaletteSRV = device->CreateBufferSRV(bonePaletteBuffer.get(), numFloat4, kFloat4Stride);
}
