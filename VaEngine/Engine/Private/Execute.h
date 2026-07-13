#pragma once

#include "Interfaces/IExecute.h"
#include "Render/SceneRenderer.h"
#include "Render/RenderGraph.h"

#include "RHI/IRenderDevice.h"
#include "RHI/ICommandQueue.h"
#include "RHI/IFence.h"
#include "RHI/ISwapChain.h"
#include "RHI/ICommandAlloc.h"
#include "RHI/ICommandList.h"

#include "System/IKeyInput.h"
#include "System/IPointerInput.h"
#include "System/InputSystem.h"
#include "System/ITime.h"
#include "System/TimerSystem.h"

#include "Utilities/DebuggingHelper.h"  // VA_DEBUG

#include <memory>
#include <cstdint>

class ApplicationManager;

class Execute : public IExecute
{
public:
	explicit Execute(ApplicationManager* app);
	~Execute() override;

	void OnInitialize(NativeDisplayInfo displayInfo) override;
	void OnDestroy() override;

	void OnLoop() override;
	void OnSuspend() override;
	void OnResume() override;

private:
	void OnRelease();

	void OnPreUpdate();
	void OnUpdate();
	void OnPreRender();
	void OnRender();
	void OnPostRender();
	void FinishFrame();


	ApplicationManager*              app;

	std::unique_ptr<IRenderDevice>   renderDevice;
	std::unique_ptr<ICommandQueue>   commandQueue;
	std::unique_ptr<IFence>          frameFence;
	uint64_t                         currentFenceValue = 0;
	std::unique_ptr<ISwapChain>      swapChain;
	std::unique_ptr<ICommandAlloc>   commandAllocator;
	std::unique_ptr<ICommandList>    commandList;

	std::unique_ptr<IKeyInput>       keyInput;
	std::unique_ptr<IPointerInput>   pointerInput;
	std::unique_ptr<InputSystem>     inputSystem;
	std::unique_ptr<ITime>           time;
	std::unique_ptr<TimerSystem>     timerSystem;

	SceneRenderer                    sceneRenderer;
	RenderGraph                      renderGraph;

#if VA_DEBUG
	// 로그에서 프레임 경계를 확인하기 위한 카운터 — OnLoop() 시작 시 VA_LOG로 찍는다.
	uint64_t                         frameNumber = 0;
#endif
};
