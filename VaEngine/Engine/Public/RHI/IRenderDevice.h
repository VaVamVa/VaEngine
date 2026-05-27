#pragma once

#include <functional>
#include <memory>
#include "Common_RHI.h"

#include "RHI/ICommandQueue.h"
#include "RHI/IFence.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/ICommandList.h"
#include "RHI/Buffer/IBuffer.h"
#include "RHI/Shader/IShader.h"
#include "RHI/Pipeline/IBindingLayout.h"
#include "RHI/Pipeline/IPipelineState.h"
#include "RHI/Pipeline/ComputePipelineDesc.h"
#include "RHI/Texture/ITexture.h"
#include "RHI/Texture/ITexture2DArray.h"
#include "RHI/Texture/ITextureUAV.h"
#include "RHI/Buffer/IDepthBuffer.h"
#include "RHI/Buffer/IColorBuffer.h"
#include "RHI/IResourceView.h"

/*
RenderDevice는 그래픽 API와 상호 작용하는 핵심 클래스입니다.
이 클래스는 DirectX 12 장치(ID3D12Device)를 관리하고, 스왑 체인, 명령 큐, 명령 리스트, 펜스 등의 리소스를 생성하는 팩토리의 역할을 합니다.
*/
class IRenderDevice
{
public:
	virtual ~IRenderDevice() = default;

	virtual void Initialize() = 0;
	virtual void Shutdown() = 0;

	[[nodiscard]] virtual std::unique_ptr<ICommandQueue>	CreateCommandQueue(const CommandQueueDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IFence>			CreateFence() = 0;
	[[nodiscard]] virtual std::unique_ptr<ISwapChain>		CreateSwapChain(const SwapChainDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<ICommandAlloc>	CreateCommandAllocator(const CommandAllocDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<ICommandList>		CreateCommandList(const CommandListDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IBuffer>           CreateBuffer(const BufferDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IShader>           CreateShader(const ShaderDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IBindingLayout>    CreateBindingLayout(const BindingEntry* entries, uint32_t count, bool isCompute = false) = 0;
	[[nodiscard]] virtual std::unique_ptr<IPipelineState>    CreatePipelineState(const PipelineStateDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<IPipelineState>    CreateComputePipelineState(const ComputePipelineStateDesc& desc) = 0;
	[[nodiscard]] virtual std::unique_ptr<ITexture>          CreateTexture() = 0;
	[[nodiscard]] virtual std::unique_ptr<ITexture>          CreateTextureFloat() = 0;
	[[nodiscard]] virtual std::unique_ptr<ITexture2DArray>   CreateTexture2DArray() = 0;
	[[nodiscard]] virtual std::unique_ptr<ITextureUAV>       CreateTextureUAV() = 0;
	[[nodiscard]] virtual std::unique_ptr<IDepthBuffer>      CreateDepthBuffer(uint32_t width, uint32_t height, EPixelFormat format) = 0;
	[[nodiscard]] virtual std::unique_ptr<IColorBuffer>      CreateColorBuffer(EPixelFormat format, uint32_t width, uint32_t height) = 0;

	// Compute / structured-buffer 리소스 뷰 생성
	// strideBytes == 0 이면 raw byte buffer (ByteAddressBuffer / RWByteAddressBuffer), > 0 이면 structured buffer.
	[[nodiscard]] virtual std::unique_ptr<IResourceView>     CreateBufferSRV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes) = 0;
	[[nodiscard]] virtual std::unique_ptr<IResourceView>     CreateBufferUAV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes) = 0;

	// 복사 명령을 ICommandList 람다로 받아 즉시 실행하고 GPU 완료까지 대기.
	// 호출 전 업로드 버퍼와 목적지 리소스의 배리어 상태를 올바르게 설정해야 함.
	virtual void ImmediateSubmit(std::function<void(ICommandList*)> recordFn) = 0;
};