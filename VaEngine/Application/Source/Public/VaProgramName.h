#pragma once

#include "Manager/ApplicationManager.h"
#include "Math/Container.h"
#include "System/TimerSystem.h"

#include <memory>
#include <string>

class CameraManager;
class WO_Cube;
class WO_Tower;
class WO_Kachujin;
class WO_Katana;
class LightManager;
class ITexture;

class VaProgramName : public ApplicationManager
{
public:
	void OnInitialize(IRenderDevice* device) override;
	void OnUpdate(float deltaTime) override;
	void SubmitRenderState(RenderScene* scene) override;
	void OnDestroy() override;

private:
	void PointerPickTest();
	void KachujinClipTest();
	void InitTestSkyTexture(IRenderDevice* device);

	void RenderDebug();

private:
	CameraManager*	cameraManager;
	LightManager* 	lightManager;

	WO_Cube*	  	transparent_cube1;
	WO_Cube*	  	transparent_cube2;
	WO_Cube*	  	floor;
	WO_Tower*		tower;
	WO_Kachujin*	kachujin;
	WO_Katana*		katana;
	uint32_t		clipIndex = 0;
	float			blendAlpha = 0;
	bool			ssaoEnabled   = true;   // Num2 키로 토글 — 런타임 비교분석용
	bool			iblEnabled    = true;   // Num1 키로 토글 — 런타임 비교분석용
	bool			showCascades  = false;  // Num3 키로 토글 — CSM 캐스케이드 색상 오버레이(디버그)
	uint32_t		activeCascadeCount = 4; // Num4(-1)/Num5(+1) 키로 조정 — CSM 활성 캐스케이드 개수(1~8)

	ITexture*		skyTexture = nullptr;

	TimerSystem::Handle clipAutoHandle  = TimerSystem::Invalid;  // 5초 클립 자동 전환
	TimerSystem::Handle clipLabelHandle = TimerSystem::Invalid;  // 3초 라벨
#if VA_DEBUG
	TimerSystem::Handle pickRayLineHandle = TimerSystem::Invalid;  // 1초 픽 레이 디버그
#endif

	std::string clipLabelText;
	Vector3     clipLabelHeadPos;  // 라벨 월드 좌표
	Vector3     pickRayOrigin;     // 마지막 픽 레이
	Vector3     pickRayDir;
};
