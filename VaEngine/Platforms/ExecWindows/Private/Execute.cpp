#include "Execute.h"
#include <stdexcept>

#include "RHI/RHILoader.h"
#include "Interfaces/IRenderDevice.h"
#include "Interfaces/ICommandQueue.h"
#include "Interfaces/IFence.h"
#include "Interfaces/ISwapChain.h"
#include "Interfaces/ICommandList.h"

void Execute::OnInitialize(NativeDisplayInfo displayInfo)
{
	if (renderDevice = RHI::CreateRenderDevice())
	{
		hWnd = static_cast<HWND>(displayInfo.Handle);
		hInstance = static_cast<HINSTANCE>(displayInfo.Display);

		renderDevice->Initialize();

		CommandQueueDesc queueDesc = {};
		queueDesc.type = static_cast<uint32_t>(ECommandQueueType::Graphics);
		queueDesc.flags = 0;
		commandQueue = renderDevice->CreateCommandQueue(queueDesc);

		frameFence = renderDevice->CreateFence();

		SwapChainDesc swapChainDesc = {};
		swapChainDesc.displayInfo = displayInfo;
		swapChainDesc.RegisteredQueue = commandQueue.get();
		swapChainDesc.bufferCount = 2;
		swapChainDesc.width = 1280;
		swapChainDesc.height = 720;
		swapChainDesc.pixelFormat = EPixelFormat::R8G8B8A8_UNORM;
		swapChainDesc.bIsWindowed = true;
		swapChain = renderDevice->CreateSwapChain(swapChainDesc);

		return;
	}

	throw std::runtime_error("Failed to create Render Device");
}

void Execute::OnDestroy()
{
	//TODO : GPU 리소스 해제 선행

	commandList.reset();
	swapChain.reset();
	frameFence.reset();
	commandQueue.reset();

	renderDevice->Shutdown();
	renderDevice.reset();
}

void Execute::OnLoop()
{
	OnUpdate();
	OnRender();
	OnPostRender();
	
	swapChain->Present(true);
}

void Execute::OnSuspend()
{
}

void Execute::OnResume()
{
}


void Execute::OnUpdate()
{
}

void Execute::OnRender()
{
}

void Execute::OnPostRender()
{
}
