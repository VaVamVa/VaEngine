#include "Execute.h"

#include "RHI/RHILoader.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandQueue.h"
#include "RHI/IFence.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/ICommandList.h"

#include "RHI/IRHIResource.h"

#include <stdexcept>
#include <array>

void Execute::OnInitialize(NativeDisplayInfo displayInfo)
{
	if (renderDevice = RHI::CreateRenderDevice())
	{
		renderDevice->Initialize();

		CommandQueueDesc queueDesc = {
			.type = ECommandQueueType::Graphics,
			.flags = 0
		};
		commandQueue = renderDevice->CreateCommandQueue(queueDesc);

		frameFence = renderDevice->CreateFence();

		SwapChainDesc swapChainDesc {
			.displayInfo = displayInfo,
			.RegisteredQueue = commandQueue.get(),
			.bufferCount = 2,
			.width = 1280,
			.height = 720,
			.pixelFormat = EPixelFormat::R8G8B8A8_UNORM,
			.bIsWindowed = true
		};
		swapChain = renderDevice->CreateSwapChain(swapChainDesc);

		commandAllocator = renderDevice->CreateCommandAllocator({ .type = ECommandQueueType::Graphics });
		commandList = renderDevice->CreateCommandList({ .type = ECommandQueueType::Graphics });

		return;
	}

	throw std::runtime_error("Failed to create Render Device");
}

void Execute::OnDestroy()
{
	OnRelease();

	if (renderDevice)
	{
		renderDevice->Shutdown();
	}
}

void Execute::OnLoop()
{
	OnPreUpdate();
	OnUpdate();

	while (frameFence->GetCompletedValue() < currentFenceValue)
	{
		// Wait for GPU to finish processing commands before Next Rendering
	}

	OnRender();
	OnPostRender();
	
	swapChain->Present(true);

	commandQueue->Signal(frameFence.get(), ++currentFenceValue);
}

void Execute::OnSuspend()
{
}

void Execute::OnResume()
{
}

void Execute::OnRelease()
{
	if (commandQueue && frameFence && commandList)
	{
		uint64_t waitValue = frameFence->GetCompletedValue() < INT64_MAX
			? frameFence->GetCompletedValue() + 1 : INT64_MAX;
		commandQueue->Signal(frameFence.get(), waitValue);
		if (frameFence->GetCompletedValue() < waitValue)
		{
			while (frameFence->GetCompletedValue() < waitValue)
			{
				// Wait for GPU to finish processing commands before releasing resources
			}
		}
	}
}

void Execute::OnPreUpdate()
{
}

void Execute::OnUpdate()
{
}

void Execute::OnRender()
{
	commandAllocator->Reset();
	commandList->Begin(commandAllocator.get());

	ResourceBarrier barrier {
		.resource = swapChain->GetCurrentBackBuffer(),
		.beforeState = EResourceState::Present,
		.afterState = EResourceState::RenderTarget
	};
	commandList->SetResourceBarrier(1, &barrier);

	// Rendering commands would go here
	commandList->ClearRenderTargetView(
		swapChain->GetCurrentBackBufferView(), // RTV 핸들 (swap chain을 일단 넘김)
		std::array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}.data() // Clear color
	);
	std::vector<ICommandList*> commandLists = { commandList.get() };
	

	barrier.beforeState = EResourceState::RenderTarget;
	barrier.afterState = EResourceState::Present;
	commandList->SetResourceBarrier(1, &barrier);
	commandList->Close();

	commandQueue->ExecuteCommandLists(static_cast<uint32_t>(commandLists.size()), commandLists);
}

void Execute::OnPostRender()
{
}
