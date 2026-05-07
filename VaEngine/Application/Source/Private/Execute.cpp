#include "Execute.h"

#include "RHI/RHILoader.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandQueue.h"
#include "RHI/IFence.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/ICommandList.h"
#include "RHI/Buffer/IResourceBuffer.h"
#include "RHI/IRHIResource.h"
#include "RHI/Agent/CubeRenderer.h"

#include <stdexcept>
#include <array>

Execute::~Execute() = default;

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

		cubeRenderer = CubeRenderer::Create();
		cubeRenderer->Initialize(renderDevice.get());

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

	OnPreRender();
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

void Execute::OnPreRender()
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

	IResourceView* rtv = swapChain->GetCurrentBackBufferView();
	commandList->SetRenderTarget(rtv);
	commandList->ClearRenderTargetView(rtv, std::array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}.data());
	commandList->SetViewport(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f);
	commandList->SetScissorRect(0, 0, 1280, 720);

	cubeRenderer->Draw(commandList.get());

	barrier.beforeState = EResourceState::RenderTarget;
	barrier.afterState = EResourceState::Present;
	commandList->SetResourceBarrier(1, &barrier);
	commandList->Close();

	std::vector<ICommandList*> commandLists = { commandList.get() };
	commandQueue->ExecuteCommandLists(static_cast<uint32_t>(commandLists.size()), commandLists);
}

void Execute::OnPostRender()
{
}
