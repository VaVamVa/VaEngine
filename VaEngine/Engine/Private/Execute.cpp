#include "Execute.h"

#include "Manager/ApplicationManager.h"
#include "Scene/RenderScene.h"

#include "RHI/RHILoader.h"
#include "RHI/BaseRHIResource.h"
#include "RHI/Shader/IShader.h"

#include "Demo/HelloCompute.h"
#include "Render/PresentTransitionPass.h"

#include "Utilities/Locator.h"
#include "Utilities/DebuggingHelper.h"

#include <stdexcept>
#include <vector>
#include <string>
#include <format>

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

		// RHI Compute 인프라 자기 검증 (1회 실행, VA_DRAW_PANEL에 PASS/FAIL 표시)
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
		sceneRenderer.Initialize(renderDevice.get(), 1280, 720);
		sceneRenderer.InitializeGBuffer(renderDevice.get(), {
			SHADER_DIR L"/GBuffer_VS.cso",
			SHADER_DIR L"/GBuffer_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeLighting(renderDevice.get(), {
			.csPath  = SHADER_DIR L"/DeferredLighting_CS.cso",
			.csEntry = "CSMain"
		});
		sceneRenderer.InitializeBlit(renderDevice.get(), {
			SHADER_DIR L"/Blit_VS.cso",
			SHADER_DIR L"/Blit_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeSky(renderDevice.get(), {
			SHADER_DIR L"/Sky_VS.cso",
			SHADER_DIR L"/Sky_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeForward(renderDevice.get(), {
			SHADER_DIR L"/ForwardOpaque_VS.cso",
			SHADER_DIR L"/ForwardOpaque_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeTransparentForward(renderDevice.get(), {
			SHADER_DIR L"/ForwardTransparent_VS.cso",
			SHADER_DIR L"/ForwardTransparent_PS.cso",
			"VSMain",
			"PSMain"
		}, {
			SHADER_DIR L"/OITComposite_VS.cso",
			SHADER_DIR L"/OITComposite_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeAnimation(renderDevice.get(), {
			SHADER_DIR L"/AnimationDemo_VS.cso",
			SHADER_DIR L"/AnimationDemo_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeGBufferSkinned(renderDevice.get(), {
			SHADER_DIR L"/GBufferSkinned_VS.cso",
			SHADER_DIR L"/GBufferSkinned_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeShadowMap(renderDevice.get(), {
			SHADER_DIR L"/ShadowMap_VS.cso",
			SHADER_DIR L"/ShadowMap_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeShadowMapSkinned(renderDevice.get(), {
			SHADER_DIR L"/ShadowMapSkinned_VS.cso",
			SHADER_DIR L"/ShadowMapSkinned_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeIBL(renderDevice.get(), {
			.csPath  = SHADER_DIR L"/IrradianceConvolve_CS.cso",
			.csEntry = "CSMain"
		}, {
			.csPath  = SHADER_DIR L"/PrefilterSpecular_CS.cso",
			.csEntry = "CSMain"
		}, {
			.csPath  = SHADER_DIR L"/IntegrateBRDF_CS.cso",
			.csEntry = "CSMain"
		});
		sceneRenderer.InitializeSSAO(renderDevice.get(),
			{ SHADER_DIR L"/SSAO_VS.cso",     SHADER_DIR L"/SSAO_PS.cso",     "VSMain", "PSMain" },
			{ SHADER_DIR L"/SSAOBlur_VS.cso", SHADER_DIR L"/SSAOBlur_PS.cso", "VSMain", "PSMain" });
		sceneRenderer.InitializeBloom(renderDevice.get(),
			{ SHADER_DIR L"/Bloom_BrightPass_VS.cso", SHADER_DIR L"/Bloom_BrightPass_PS.cso", "VSMain", "PSMain" },
			{ SHADER_DIR L"/Bloom_BlurH_VS.cso",       SHADER_DIR L"/Bloom_BlurH_PS.cso",       "VSMain", "PSMain" },
			{ SHADER_DIR L"/Bloom_BlurV_VS.cso",       SHADER_DIR L"/Bloom_BlurV_PS.cso",       "VSMain", "PSMain" },
			{ SHADER_DIR L"/Bloom_Composite_VS.cso",   SHADER_DIR L"/Bloom_Composite_PS.cso",   "VSMain", "PSMain" });
		sceneRenderer.InitializeDebugLines(renderDevice.get(), {
			SHADER_DIR L"/DebugLine_VS.cso",
			SHADER_DIR L"/DebugLine_PS.cso",
			"VSMain",
			"PSMain"
		});
		sceneRenderer.InitializeDebugText(renderDevice.get(), {
			SHADER_DIR L"/Glyph_VS.cso",
			SHADER_DIR L"/Glyph_PS.cso",
			"VSMain",
			"PSMain"
		}, _FILES_DIR "Font/NotoSansKR-Regular.ttf");
#elif defined(USE_VULKAN)
		VA_LOG("RHI", "Active Backend: Vulkan");
		// TODO: Vulkan SPIR-V 경로
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
#if VA_DEBUG
	VA_LOG("Frame", std::format("--- Frame {} start ---", ++frameNumber));
#endif

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

	HelloCompute::RenderResult();
}

void Execute::OnPreRender()
{
}

void Execute::OnRender()
{
	commandAllocator->Reset();
	commandList->Begin(commandAllocator.get());

	BaseRHIResource* backBuffer = swapChain->GetCurrentBackBuffer();

	renderGraph.Reset();

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
	sceneRenderer.AddPasses(renderGraph, output, scene, renderDevice.get());

	// 백버퍼를 Present 상태로 전환하는 것도 그래프의 마지막 Pass로 등록 — Compile()이 다른
	// 리소스와 동일한 경로로 배리어를 자동 계산한다(수동 배리어 코드 불필요).
	renderGraph.AddPass<PresentTransitionPass>(backBuffer);

	renderGraph.Compile(renderDevice.get());
	renderGraph.Execute(commandList.get(), scene);

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
