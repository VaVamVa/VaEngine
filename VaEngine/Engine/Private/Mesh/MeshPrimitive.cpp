#include "Mesh/MeshPrimitive.h"
#include "RHI/IRenderDevice.h"
#include "RHI/ICommandList.h"
#include "RHI/Common_RHI.h"

void MeshPrimitive::Initialize(IRenderDevice* device)
{
	BuildGeometry();

	ResourceBufferDesc vbDesc = {
		.size   = vertexData.size(),
		.usage  = EBufferUsage::VertexBuffer,
		.access = EMemoryAccess::Upload,
		.stride = vertexStride
	};
	vertexBuffer = device->CreateBuffer(vbDesc);
	vertexBuffer->Upload(vertexData.data(), vertexData.size());

	ResourceBufferDesc ibDesc = {
		.size   = indexData.size() * sizeof(uint16_t),
		.usage  = EBufferUsage::IndexBuffer,
		.access = EMemoryAccess::Upload,
		.stride = sizeof(uint16_t)
	};
	indexBuffer = device->CreateBuffer(ibDesc);
	indexBuffer->Upload(indexData.data(), indexData.size() * sizeof(uint16_t));
}

void MeshPrimitive::Draw(ICommandList* cmdList)
{
	cmdList->SetVertexBuffer(vertexBuffer.get(), vertexStride, (uint32_t)vertexData.size());
	cmdList->SetIndexBuffer(indexBuffer.get(), EIndexFormat::UInt16, (uint32_t)(indexData.size() * sizeof(uint16_t)));
	cmdList->DrawIndexed((uint32_t)indexData.size());
}
