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

	ID3D12Device*	GetDevice() const { return device.Get(); }
	IDXGIFactory6*	GetFactory() const { return factory.Get(); }
	IDXGIAdapter4*	GetAdapter() const { return adapter.Get(); }


private:
	void EnableDebugLayer();
	void CreateFactory();
	void PickAdapter();
	void CreateDevice();
	
private:
	ComPtr<IDXGIFactory6>	factory;
	ComPtr<IDXGIAdapter4>	adapter;
	ComPtr<ID3D12Device>	device;
};