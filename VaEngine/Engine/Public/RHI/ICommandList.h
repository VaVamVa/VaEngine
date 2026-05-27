#pragma once

#include <cstdint>
#include "Common_RHI.h"

class IResourceView;
class IBuffer;

class ICommandList
{
public:
	virtual ~ICommandList() = default;

	virtual void Register(class IRenderDevice* device, const struct CommandListDesc& desc) = 0;

	// 명령 리스트에 명령을 기록하기 시작하는 함수
	virtual void Begin(class ICommandAlloc* inAllocator) = 0;

	// 명령 리스트에 명령 기록을 완료하는 함수
	virtual void Close() = 0;

#pragma region Commands
	// 상태 전이
	virtual void SetResourceBarrier(uint32_t numBarriers, const struct ResourceBarrier* inBarriers) = 0;

	// 화면을 특정 색으로 지우는 명령
	virtual void ClearRenderTargetView(IResourceView* inViewHandle, const float inColor[4]) = 0;

	// 렌더 타겟 바인딩 (Vulkan: render pass attachment, DX12: OMSetRenderTargets)
	virtual void SetRenderTarget(IResourceView* rtv) = 0;

	// 뷰포트 설정
	virtual void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth) = 0;

	// 시저 렉트 설정
	virtual void SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom) = 0;

	// 버텍스 버퍼 바인딩 — slot 0 고정 (DX12: IASetVertexBuffers, Vulkan: vkCmdBindVertexBuffers)
	virtual void SetVertexBuffer(IBuffer* vb, uint32_t stride, uint32_t totalSize) = 0;

	// 버텍스 버퍼 바인딩 — 슬롯 지정, byteOffset 지원 (인스턴스 버퍼 등)
	virtual void SetVertexBufferAt(IBuffer* vb, uint32_t slot, uint32_t stride, uint32_t totalSize, uint32_t byteOffset = 0) = 0;

	// 인덱스 버퍼 바인딩 (DX12: IASetIndexBuffer, Vulkan: vkCmdBindIndexBuffer)
	virtual void SetIndexBuffer(IBuffer* ib, EIndexFormat format, uint32_t totalSize) = 0;

	// 상수 버퍼 바인딩 (DX12: SetGraphicsRootConstantBufferView, Vulkan: descriptor set bind)
	virtual void SetConstantBuffer(IBuffer* cb, uint32_t slot) = 0;

	// Graphics root SRV 바인딩 — StructuredBuffer / ByteAddressBuffer 등 (DX12: SetGraphicsRootShaderResourceView)
	virtual void SetGraphicsSRV(class IResourceView* view, uint32_t slot) = 0;

	// 인덱스 드로우 (instanceCount = 1 고정)
	virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, int32_t baseVertex = 0) = 0;

	// 인스턴스 드로우
	virtual void DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0, int32_t baseVertex = 0, uint32_t startInstance = 0) = 0;

	// 버텍스 버퍼 없는 드로우 (SV_VertexID 기반 메시리스 렌더링용)
	virtual void DrawInstanced(uint32_t vertexCount, uint32_t instanceCount) = 0;

	// 프리미티브 토폴로지 설정 (DX12: IASetPrimitiveTopology, Vulkan: vkCmdSetPrimitiveTopologyEXT)
	virtual void SetPrimitiveTopology(EPrimitiveTopology topology) = 0;

	// 렌더 패스 시작 — RTV 바인딩 + loadAction에 따라 클리어 수행
	virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;

	// 렌더 패스 종료 (DX12: no-op, Vulkan: vkCmdEndRenderPass)
	virtual void EndRenderPass() = 0;

	// Buffer-to-buffer 전체 복사 (DX12: CopyBufferRegion, Vulkan: vkCmdCopyBuffer)
	// 호출 전 src는 CopySource, dst는 CopyDest 상태여야 함.
	virtual void CopyBuffer(IBuffer* dst, IBuffer* src, uint64_t bytes) = 0;

	// Upload 버퍼 → Texture 서브리소스 복사 (DX12: CopyTextureRegion, Vulkan: vkCmdCopyBufferToImage)
	// 호출 전 srcBuffer는 CopySource, dstTexture는 CopyDest 상태여야 함.
	// rowPitch는 D3D12_TEXTURE_DATA_PITCH_ALIGNMENT(256) 단위 정렬 값을 전달해야 함.
	virtual void CopyBufferToTexture(
		IRHIResource* dstTexture,
		uint32_t      dstSubresource,
		IRHIResource* srcBuffer,
		uint64_t      srcOffset,
		uint32_t      width,
		uint32_t      height,
		uint32_t      rowPitch) = 0;

#pragma endregion

#pragma region Compute Commands
	// Compute root에 CBV 바인딩 (graphics용 SetConstantBuffer와 별도 — DX12 root signature가 분리됨)
	virtual void SetComputeConstantBuffer(IBuffer* cb, uint32_t slot) = 0;

	// Compute root에 buffer SRV 바인딩 (StructuredBuffer / ByteAddressBuffer 입력)
	virtual void SetComputeSRV(IResourceView* view, uint32_t slot) = 0;

	// Compute root에 buffer UAV 바인딩 (RWStructuredBuffer / RWByteAddressBuffer 출력)
	virtual void SetComputeUAV(IResourceView* view, uint32_t slot) = 0;

	// 디스패치 — (X*Y*Z) * (groupCount.x * y * z) 만큼의 thread를 GPU에 풀어놓음
	virtual void Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

	// UAV 동기화 — compute 쓰기 → 후속 패스 읽기 사이에 삽입
	virtual void UAVBarrier(IRHIResource* resource) = 0;
#pragma endregion
};