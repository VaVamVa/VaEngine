#pragma once

#include "Mesh/IMesh.h"
#include "RHI/IBuffer.h"

#include <memory>
#include <vector>
#include <cstdint>

struct SkinnedMeshData
{
    std::vector<uint8_t>  vertices;
    std::vector<uint16_t> indices;
    uint32_t              vertexStride = 0;
};

class IRenderDevice;

class SkinnedMesh : public IMesh
{
public:
    void Initialize(IRenderDevice* device, const SkinnedMeshData& data);
    void Draw(ICommandList* cmdList) override;
    void DrawInstanced(ICommandList* cmdList, uint32_t instanceCount) override;

private:
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t                 vertexByteSize = 0;
    uint32_t                 indexCount     = 0;
    uint32_t                 vertexStride   = 0;
};
