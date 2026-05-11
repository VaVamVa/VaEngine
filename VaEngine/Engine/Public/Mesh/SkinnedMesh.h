#pragma once

#include "Mesh/IMesh.h"
#include "RHI/IBuffer.h"
#include "RHI/IResourceView.h"

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

    // Step (Y) — 본 팔레트 buffer를 mesh가 소유.
    // 같은 mesh를 공유하는 모든 인스턴스가 이 buffer의 영역(인스턴스 0..maxInstances-1)을 분할 사용.
    // boneCount는 셰이더 indexing 통일을 위해 MAX_MODEL_TRANSFORMS(250) 고정 — 실제 본 수가 적으면 일부 영역 미사용.
    void CreateBonePalette(IRenderDevice* device, uint32_t maxInstances);

    IBuffer*       GetBonePaletteBuffer() const { return bonePaletteBuffer.get(); }
    IResourceView* GetBonePaletteUAV()    const { return bonePaletteUAV.get(); }
    IResourceView* GetBonePaletteSRV()    const { return bonePaletteSRV.get(); }
    uint32_t       GetMaxInstances()      const { return maxInstances; }

private:
    std::unique_ptr<IBuffer> vertexBuffer;
    std::unique_ptr<IBuffer> indexBuffer;
    uint32_t                 vertexByteSize = 0;
    uint32_t                 indexCount     = 0;
    uint32_t                 vertexStride   = 0;

    // Bone palette (compute → graphics 공유)
    std::unique_ptr<IBuffer>       bonePaletteBuffer;
    std::unique_ptr<IResourceView> bonePaletteUAV;
    std::unique_ptr<IResourceView> bonePaletteSRV;
    uint32_t                       maxInstances = 0;
};
