#include "Execute.h"

#include "Manager/ApplicationManager.h"
#include "Scene/RenderScene.h"

#include "RHI/RHILoader.h"
#include "RHI/IRHIResource.h"
#include "RHI/Shader/IShader.h"

#include "Demo/HelloCompute.h"

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
	DebuggingHelper::InitLog(_FILES_DIR "Log/");
	VA_LOG("Execute", "Initializing Execute...");

	keyInput     = IKeyInput::Create();
	pointerInput = IPointerInput::Create(displayInfo);
	inputSystem  = std::make_unique<InputSystem>(keyInput.get(), pointerInput.get());

	Locator<InputSystem>::Register(inputSystem.get());
	Locator<IPointerInput>::Register(pointerInput.get());

	timerSystem = std::make_unique<TimerSystem>();
	Locator<TimerSystem>::Register(timerSystem.get());

	time = ITime::Create();
	time->Start();
	Locator<ITimeReader>::Register(time.get());

	if (renderDevice = RHI::CreateRenderDevice())
	{
		renderDevice->Initialize();

		// Step 2 — RHI Compute 인프라 자기 검증 (1회 실행, VA_DRAW_PANEL에 PASS/FAIL 표시)
		HelloCompute::Run(renderDevice.get());

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
			SHADER_DIR L"/ForwardOpaque_VS.cso",
			SHADER_DIR L"/ForwardOpaque_PS.cso",
			"VSMain",
			"PSMain"
		});
		renderer.InitializeSky(renderDevice.get(), {
			SHADER_DIR L"/Sky_VS.cso",
			SHADER_DIR L"/Sky_PS.cso",
			"VSMain",
			"PSMain"
		});
		renderer.InitializeDebugLines(renderDevice.get(), {
			SHADER_DIR L"/DebugLine_VS.cso",
			SHADER_DIR L"/DebugLine_PS.cso",
			"VSMain",
			"PSMain"
		});
		renderer.InitializeDebugText(renderDevice.get(), {
			SHADER_DIR L"/Glyph_VS.cso",
			SHADER_DIR L"/Glyph_PS.cso",
			"VSMain",
			"PSMain"
		}, _FILES_DIR "Font/NotoSansKR-Regular.ttf");
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

	Locator<TimerSystem>::Unregister();
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

	timerSystem->Update(time->Delta());
	app->OnUpdate(time->Delta());

	// Frame Stats
	float fps = 1.0f / time->Delta();
	VA_DRAW_PANEL(0, std::format("FPS: {:.1f} ({:.2f} ms)", fps, time->Delta() * 1000.0f));

	// Step 2 검증 결과 (Run() 한 번 실행, 매 프레임 panel에 재출력)
	HelloCompute::RenderResult();
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

	VA_DRAW_PANEL(1, std::format("Draw Calls: {}", scene.GetCommands().size()));

	FrameOutput output{
		.backBuffer     = backBuffer,
		.backBufferView = swapChain->GetCurrentBackBufferView(),
		.clearColor     = { 0.1f, 0.2f, 0.3f, 1.0f },
		.width          = 1280,
		.height         = 720
	};
	renderer.AddPasses(renderGraph, output, scene);
	animationRenderer.AddPasses(renderGraph, output, scene);
	renderer.AddDebugLinePasses(renderGraph, output);  // 모든 geometry 이후 — depth test 정확도 보장

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
