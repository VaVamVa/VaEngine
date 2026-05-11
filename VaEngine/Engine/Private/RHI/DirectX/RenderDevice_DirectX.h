#pragma once

#include "RHI/IRenderDevice.h"
#include "Common_DirectX.h"

class RenderDevice_DirectX : public IRenderDevice
{
public:
	void Initialize() override;
	void Shutdown() override;
	
	std::unique_ptr<ISwapChain>		CreateSwapChain(const SwapChainDesc& desc) override;
	std::unique_ptr<ICommandQueue>	CreateCommandQueue(const CommandQueueDesc& desc) override;
	std::unique_ptr<IFence>			CreateFence() override;
	std::unique_ptr<ICommandAlloc>	CreateCommandAllocator(const CommandAllocDesc& desc) override;
	std::unique_ptr<ICommandList>	CreateCommandList(const CommandListDesc& desc) override;
	std::unique_ptr<IBuffer>  CreateBuffer(const BufferDesc& desc) override;
	std::unique_ptr<IShader>  CreateShader(const ShaderDesc& desc) override;
	std::unique_ptr<IBindingLayout>  CreateBindingLayout(const BindingEntry* entries, uint32_t count, bool isCompute = false) override;
	std::unique_ptr<IPipelineState>  CreatePipelineState(const PipelineStateDesc& desc) override;
	std::unique_ptr<IPipelineState>  CreateComputePipelineState(const ComputePipelineStateDesc& desc) override;
	std::unique_ptr<ITexture>        CreateTexture() override;
	std::unique_ptr<ITexture>        CreateTextureFloat() override;
	std::unique_ptr<ITexture2DArray> CreateTexture2DArray() override;
	std::unique_ptr<ITextureUAV>     CreateTextureUAV() override;
	std::unique_ptr<IDepthBuffer>    CreateDepthBuffer(uint32_t width, uint32_t height, EPixelFormat format) override;
	std::unique_ptr<IResourceView>   CreateBufferSRV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes) override;
	std::unique_ptr<IResourceView>   CreateBufferUAV(IBuffer* buffer, uint32_t numElements, uint32_t strideBytes) override;

	ID3D12Device*         GetDevice()        const { return device.Get(); }
	IDXGIFactory6*        GetFactory()       const { return factory.Get(); }
	IDXGIAdapter4*        GetAdapter()       const { return adapter.Get(); }
	ID3D12DescriptorHeap* GetGlobalSRVHeap() const { return globalSrvHeap.Get(); }

	struct SRVDescriptor {
		D3D12_CPU_DESCRIPTOR_HANDLE cpu;
		D3D12_GPU_DESCRIPTOR_HANDLE gpu;
	};
	SRVDescriptor AllocateSRVDescriptor();

private:
	void EnableDebugLayer();
	void CreateFactory();
	void PickAdapter();
	void CreateDevice();

private:
	ComPtr<IDXGIFactory6>        factory;
	ComPtr<IDXGIAdapter4>        adapter;
	ComPtr<ID3D12Device>         device;

	ComPtr<ID3D12DescriptorHeap> globalSrvHeap;
	uint32_t                     srvDescriptorSize = 0;
	uint32_t                     srvAllocIndex     = 0;
	static constexpr uint32_t    GLOBAL_SRV_HEAP_SIZE = 1024;
};