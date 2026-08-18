#pragma once

#include "IMesh.h"
#include "MeshData.h"
#include "RHI/Buffer/IBuffer.h"

#include <memory>

class IRenderDevice;

class MeshPrimitive : public IMesh
{
public:
	void Initialize(IRenderDevice* device, const MeshData& data);
	void Draw(ICommandList* cmdList) override;
	void DrawInstanced(ICommandList* cmdList, uint32_t instanceCount) override;

private:
	std::unique_ptr<IBuffer> vertexBuffer;
	std::unique_ptr<IBuffer> indexBuffer;
	uint32_t                 vertexByteSize = 0;
	uint32_t                 indexCount     = 0;
	uint32_t                 vertexStride   = 0;
};
