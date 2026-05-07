#pragma once

#include "IMesh.h"
#include "RHI/Buffer/IResourceBuffer.h"

#include <vector>
#include <cstdint>
#include <memory>

class MeshPrimitive : public IMesh
{
public:
	void Initialize(IRenderDevice* device) override;
	void Draw(ICommandList* cmdList) override;

protected:
	virtual void BuildGeometry() = 0;

	std::vector<uint8_t>  vertexData;
	std::vector<uint16_t> indexData;
	uint32_t              vertexStride = 0;

private:
	std::unique_ptr<IResourceBuffer> vertexBuffer;
	std::unique_ptr<IResourceBuffer> indexBuffer;
};
