#include "Execute.h"

#include "Manager/ApplicationManager.h"
#include "Scene/RenderScene.h"

#include "RHI/RHILoader.h"
#include "RHI/IRHIResource.h"
#include "RHI/Shader/IShader.h"

#include "Utilities/Locator.h"
#include "Utilities/DebuggingHelper.h"

#include <stdexcept>
#include <vector>
#include <string>

std::unique_ptr<IExecute> IExecute::Create(ApplicationManager* app)
{
	return std::make_unique<Execute>(app);
}

Execute::Execute(ApplicationManager* app)
	: app(app)
{
}

Execute::~Execute() = default;

void Execute::OnInitialize(NativeDisplayInfo displayInfo)
{
	VA_LOG("Execute", "Initializing Execute...");

	keyInput     = IKeyInput::Create();
	pointerInput = IPointerInput::Create(displayInfo);
	inputSystem  = std::make_unique<InputSystem>(keyInput.get(), pointerInput.get());

	Locator<InputSystem>::Register(inputSystem.get());
	Locator<IPointerInput>::Register(pointerInput.get());

	time = ITime::Create();
	time->Start();
	Locator<ITimeReader>::Register(time.get());

	if (renderDevice = RHI::CreateRenderDevice())
	{
		renderDevice->Initialize();

		CommandQueueDesc queueDesc = {
			.type  = ECommandQueueType::Graphics,
			.flags = 0
		};
		commandQueue = renderDevice->CreateCommandQueue(queueDesc);

		frameFence = renderDevice->CreateFence();

		SwapChainDesc swapChainDesc{
			.displayInfo      = displayInfo,
			.RegisteredQueue  = commandQueue.get(),
			.bufferCount      = 2,
			.width            = 1280,
			.height           = 720,
			.pixelFormat      = EPixelFormat::R8G8B8A8_UNORM,
			.bIsWindowed      = true
		};
		swapChain = renderDevice->CreateSwapChain(swapChainDesc);

		commandAllocator = renderDevice->CreateCommandAllocator({ .type = ECommandQueueType::Graphics });
		commandList      = renderDevice->CreateCommandList({ .type = ECommandQueueType::Graphics });

#ifdef USE_DIRECTX
		VA_LOG("RHI", "Active Backend: DirectX 12");
		renderer.Initialize(renderDevice.get(), {
			SHADER_DIR L"/CubeShader_VS.cso",
			SHADER_DIR L"/CubeShader_PS.cso",
			"VSMain",
			"PSMain"
		});
		animationRenderer.Initialize(renderDevice.get(), {
			SHADER_DIR L"/AnimationDemo_VS.cso",
			SHADER_DIR L"/AnimationDemo_PS.cso",
			"VSMain",
			"PSMain"
			});
#elif defined(USE_VULKAN)
		VA_LOG("RHI", "Active Backend: Vulkan");
		renderer.Initialize(renderDevice.get(), {}, 1280, 720);  // TODO: Vulkan SPIR-V 경로
#endif

		app->OnInitialize(renderDevice.get());
		return;
	}

	throw std::runtime_error("Failed to create Render Device");
}

void Execute::OnDestroy()
{
	VA_LOG("Execute", "Destroying Execute...");
	app->OnDestroy();

	Locator<InputSystem>::Unregister();
	Locator<IPointerInput>::Unregister();

	OnRelease();

	if (renderDevice)
	{
		renderDevice->Shutdown();
	}
}

void Execute::OnLoop()
{
	DebuggingHelper::Clear();

	OnPreUpdate();
	OnUpdate();

	OnPreRender();
	OnRender();
	OnPostRender();

	FinishFrame();
}

void Execute::OnSuspend()
{
	VA_LOG("System", "Application Suspended");
	if (time) time->Stop();
}

void Execute::OnResume()
{
	VA_LOG("System", "Application Resumed");
	if (time) time->Start();
}

void Execute::OnRelease()
{
	if (commandQueue && frameFence && commandList)
	{
		uint64_t waitValue = frameFence->GetCompletedValue() < INT64_MAX
			? frameFence->GetCompletedValue() + 1 : INT64_MAX;
		commandQueue->Signal(frameFence.get(), waitValue);
		frameFence->Wait(waitValue);
	}
}

void Execute::OnPreUpdate()
{
	frameFence->Wait(currentFenceValue);
}

void Execute::OnUpdate()
{
	time->Update();
	keyInput->Update();
	pointerInput->Update();
	inputSystem->Update();

	app->OnUpdate(time->Delta());

	// Frame Stats
	float fps = 1.0f / time->Delta();
	VA_DRAW_TEXT(std::format("FPS: {:.1f} ({:.2f} ms)", fps, time->Delta() * 1000.0f), { 10, 10 });
}

void Execute::OnPreRender()
{
}

void Execute::OnRender()
{
	commandAllocator->Reset();
	commandList->Begin(commandAllocator.get());

	IRHIResource* backBuffer = swapChain->GetCurrentBackBuffer();

	renderGraph.Reset();
	renderGraph.ImportResource(backBuffer, EResourceState::Present);

	RenderScene scene;
	app->SubmitRenderState(&scene);
	scene.SortCommands();

	VA_DRAW_TEXT(std::format("Draw Calls: {}", scene.GetCommands().size()), { 10, 30 });

	FrameOutput output{
		.backBuffer     = backBuffer,
		.backBufferView = swapChain->GetCurrentBackBufferView(),
		.clearColor     = { 0.1f, 0.2f, 0.3f, 1.0f },
		.width          = 1280,
		.height         = 720
	};
	renderer.AddPasses(renderGraph, output);
	animationRenderer.AddPasses(renderGraph, output);

	renderGraph.Compile(renderDevice.get());
	renderGraph.Execute(commandList.get(), scene);

	ResourceBarrier presentBarrier{
		.resource    = backBuffer,
		.beforeState = renderGraph.GetCurrentState(backBuffer),
		.afterState  = EResourceState::Present
	};
	commandList->SetResourceBarrier(1, &presentBarrier);
	commandList->Close();

	std::vector<ICommandList*> cmdLists = { commandList.get() };
	commandQueue->ExecuteCommandLists(static_cast<uint32_t>(cmdLists.size()), cmdLists);
}

void Execute::OnPostRender()
{
}

void Execute::FinishFrame()
{
	swapChain->Present(true);
	commandQueue->Signal(frameFence.get(), ++currentFenceValue);
}
