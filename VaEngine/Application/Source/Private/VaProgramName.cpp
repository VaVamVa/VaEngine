#include "VaProgramName.h"

#include "System/InputSystem.h"
#include "System/IPointerInput.h"
#include "Utilities/Locator.h"

#include "Light/LightManager.h"
#include "GameManager/CameraManager.h"

#include "Utilities/Time.h"

#include "Math/Container.h"
#include "WorldObjects/WO_Cube.h"
#include "WorldObjects/WO_Tower.h"
#include "WorldObjects/WO_Kachujin.h"

#include "RHI/IRenderDevice.h"
#include "RHI/Texture/ITexture.h"

#include "Utilities/Helpers.h"

#include "Utilities/DebuggingHelper.h"
#include "Math/Viewport.h"
#include "System/TimerSystem.h"

void VaProgramName::OnInitialize(IRenderDevice* device)
{
	cameraManager = new CameraManager();
	cameraManager->OnInitialize(device);

	kachujin = new WO_Kachujin();
	kachujin->Initialize(device);
	kachujin->transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	kachujin->transform.SetScale(Vector3(0.01f, 0.01f, 0.01f));
	clipIndex = 0;
	blendAlpha = 0;
	
	InputContext animCtx("AnimationTest");
	animCtx.MapKeyBool("BlendAlphaTest", EKeyCode::Z, EInputTrigger::Down, [this]()
		{
			blendAlpha += 0.1f;
			kachujin->SetBlendAlpha(blendAlpha);
		});
	animCtx.MapKeyBool("BlendAlphaTest", EKeyCode::X, EInputTrigger::Down, [this]()
		{
			blendAlpha -= 0.1f;
			kachujin->SetBlendAlpha(blendAlpha);
		});
	kachujin->SetBlendAlpha(blendAlpha);

	cube = new WO_Cube();
	cube->Initialize(device);
	cube->transform.SetPosition(Vector3(0.f, 6.f, 0.f));

	tower = new WO_Tower();
	tower->Initialize(device);
	tower->transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f));
	tower->transform.SetEulerDeg(0.f, 90.f, 90.f);
	
	skyTexture = device->CreateTextureFloat().release();
	skyTexture->LoadFromFile(device, ASSETS_DIR "HDR/sundowner_overlook_4k.hdr");

	lightManager = new LightManager();
	lightManager->GetDirectionalLight()->SetEnabled(true);

	IPointLight* pt = lightManager->AddPointLight();
	pt->SetPosition(2.0f, -2.0f, -2.0f);
	pt->SetRange(15.0f);
	pt->SetAttenuation(1.0f, 0.09f, 0.032f);
	pt->SetDiffuse(1.0f, 0.8f, 0.6f);
	pt->SetSpecular(1.0f, 0.8f, 0.6f);
	pt->SetEnabled(false);


	InputContext pickCtx("CursorPick");
	pickCtx.MapPointerButton("Pick", EPointerButton::Primary, EInputTrigger::Down, [this]() { PointerPickTest(); });
	Locator<InputSystem>::Get().PushContext(pickCtx);

	clipAutoHandle = Locator<TimerSystem>::Get().SetRepeating(5.0f, [this]()
	{
		clipIndex = (clipIndex + 1) % kachujin->GetClipCount();
		//kachujin->PlayTween(clipIndex, 0.3f);
		kachujin->PlayBlend(clipIndex, (clipIndex + 1) % kachujin->GetClipCount(), (clipIndex + 2) % kachujin->GetClipCount(),
			0.5f, 0.5f, 0.5f);
	});
}

void VaProgramName::OnUpdate(float deltaTime)
{
	cube->transform.Rotate(Vector3::Up, Math::PI * 0.5f * deltaTime);
	kachujin->Update(deltaTime);
	cameraManager->OnUpdate(deltaTime);

	KachujinClipTest();
#if VA_DEBUG
	if (Locator<TimerSystem>::Get().IsActive(pickRayLineHandle))
	{

		VA_DRAW_LINE_DEPTH(pickRayOrigin, pickRayOrigin + pickRayDir * 20.0f, { 1.0f, 1.0f, 0.0f, 1.0f });
	}
	VA_DRAW_PANEL(5, std::format("BlendAlpha: {:.2f}", blendAlpha));
#endif
}

void VaProgramName::SubmitRenderState(RenderScene* scene)
{
	scene->SetSkybox(skyTexture);

	lightManager->Submit(scene);
	cameraManager->SubmitRenderState(scene);

	cube->AddToScene(*scene);
	tower->AddToScene(*scene);
	kachujin->AddToScene(*scene);
}

void VaProgramName::OnDestroy()
{
	Locator<TimerSystem>::Get().ClearTimer(clipAutoHandle);
	Locator<TimerSystem>::Get().ClearTimer(clipLabelHandle);
#if VA_DEBUG
	Locator<TimerSystem>::Get().ClearTimer(pickRayLineHandle);
#endif

	cameraManager->OnDestroy();

	SAFE_DELETE(cameraManager);
	SAFE_DELETE(cube);
	SAFE_DELETE(tower);
	SAFE_DELETE(kachujin);
	SAFE_DELETE(lightManager);
	SAFE_DELETE(skyTexture);
}

void VaProgramName::PointerPickTest()
{
	if (Locator<IPointerInput>::Get().GetCursorMode() != ECursorMode::Free) return;

	const PointerState ps = Locator<IPointerInput>::Get().GetState();
	Vector3 rayOrigin, rayDir;
	Viewport::ScreenToRay(ps.x, ps.y,
		cameraManager->GetView(), cameraManager->GetProjection(),
		1280, 720, rayOrigin, rayDir);

#if VA_DEBUG
	pickRayLineHandle = Locator<TimerSystem>::Get().SetTimer(3.0f, [this]() { Locator<TimerSystem>::Get().ClearTimer(pickRayLineHandle); });
#endif

	const Vector3   center = kachujin->transform.GetPosition() + Vector3(0.0f, 1.0f, 0.0f);
	constexpr float radius = 0.8f;
	const Vector3   oc = rayOrigin - center;
	const float     bHalf = rayDir.Dot(oc);
	// discriminant = b^2 - 4ac (a=1, c=oc·oc - r^2)에서 4는 생략
	const float     disc = bHalf * bHalf - (oc.Dot(oc) - radius * radius);

	pickRayOrigin = rayOrigin;
	pickRayDir    = rayDir;

	if (disc >= 0.0f)
	{
		clipLabelText    = kachujin->GetClipName(clipIndex);
		clipLabelHeadPos = kachujin->transform.GetPosition() + Vector3(0.0f, 2.0f, 0.0f);

		Locator<TimerSystem>::Get().ClearTimer(clipLabelHandle);
		clipLabelHandle = Locator<TimerSystem>::Get().SetTimer(3.0f, [&](){ KachujinClipTest(); });
	}
}

void VaProgramName::KachujinClipTest()
{
	if (!Locator<TimerSystem>::Get().IsActive(clipLabelHandle)) return;

	Vector2 screenPos;
	if (Viewport::WorldToScreen(clipLabelHeadPos,
	                            cameraManager->GetView(), cameraManager->GetProjection(),
	                            1280, 720, screenPos))
	{
		VA_DRAW_TEXT(clipLabelText, screenPos, { 1.0f, 1.0f, 0.2f, 1.0f });
	}

}

void VaProgramName::InitTestSkyTexture(IRenderDevice* device)
{
	// 테스트용 sky 텍스처 — 2×1 float4 그라디언트 (파란 하늘 / 주황 지평선)
	// 실제 HDRi 파일 사용 시: skyTexture->LoadFromFile(device, ASSETS_DIR "HDR/filename.hdr")
	constexpr uint32_t W = 4, H = 2;
	float pixels[W * H * 4] = {
		0.3f, 0.5f, 1.0f, 1.0f,  0.3f, 0.5f, 1.0f, 1.0f,  // 윗줄 — 하늘색
		0.3f, 0.5f, 1.0f, 1.0f,  0.3f, 0.5f, 1.0f, 1.0f,
		1.0f, 0.6f, 0.2f, 1.0f,  1.0f, 0.6f, 0.2f, 1.0f,  // 아랫줄 — 지평선 주황
		1.0f, 0.6f, 0.2f, 1.0f,  1.0f, 0.6f, 0.2f, 1.0f,
	};
	skyTexture = device->CreateTextureFloat().release();
	skyTexture->LoadFromMemory(device, pixels, W, H);
}