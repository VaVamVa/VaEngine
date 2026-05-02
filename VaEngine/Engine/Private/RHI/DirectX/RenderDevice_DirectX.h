#pragma once

#include "Common_DirectX.h"
#include "Interfaces/IRenderDevice.h"

class RenderDevice_DirectX : public IRenderDevice
{
public:
	void Initialize() override;
	void Shutdown() override;
	
	std::unique_ptr<ISwapChain>		CreateSwapChain(const SwapChainDesc& desc) override;
	std::unique_ptr<ICommandQueue>	CreateCommandQueue(const CommandQueueDesc& desc) override;
	std::unique_ptr<IFence>			CreateFence() override;
	std::unique_ptr<ICommandList>	CreateCommandList(const CommandListDesc& desc) override;

	ID3D12Device* GetDevice() const { return device.Get(); }

private:
	void EnableDebugLayer();
	void CreateDevice();
	void CreateFactory();
	void PickAdapter();
	
private:
	ComPtr<ID3D12Device>	device;
	ComPtr<IDXGIAdapter4>	adapter;
	ComPtr<IDXGIFactory6>	factory;
};