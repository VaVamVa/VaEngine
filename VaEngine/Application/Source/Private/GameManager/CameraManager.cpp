#include "GameManager/CameraManager.h"

#include "Camera/FreeCamera.h"

#include "System/InputSystem.h"
#include "Utilities/Locator.h"


CameraManager::~CameraManager() = default;

void CameraManager::OnInitialize(IRenderDevice* device)
{
	InputContext ctx("FreeCamera");

	ctx.MapKey("MoveForward", EKeyCode::W, +1.0f);
	ctx.MapKey("MoveForward", EKeyCode::S, -1.0f);
	ctx.MapKey("MoveRight", EKeyCode::D, +1.0f);
	ctx.MapKey("MoveRight", EKeyCode::A, -1.0f);
	ctx.MapKey("MoveUp", EKeyCode::E, +1.0f);
	ctx.MapKey("MoveUp", EKeyCode::Q, -1.0f);
	ctx.MapPointer("LookX", EPointerSource::DeltaX);
	ctx.MapPointer("LookY", EPointerSource::DeltaY);
	ctx.MapPointerButton("FreeCamEnable", EPointerButton::Secondary, EInputTrigger::Down, [this]()
		{
			Locator<IPointerInput>::Get().SetCursorMode(ECursorMode::Centered);
			camera->SetEnabled(true);
		});
	ctx.MapPointerButton("FreeCamEnable", EPointerButton::Secondary, EInputTrigger::Up, [this]()
		{
			Locator<IPointerInput>::Get().SetCursorMode(ECursorMode::Free);
			camera->SetEnabled(false);
		});

	Locator<InputSystem>::Get().PushContext(ctx);

	camera = std::make_unique<FreeCamera>(Math::ToRadian(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
}

void CameraManager::OnUpdate(float deltaTime)
{
	camera->Update(deltaTime);
}

void CameraManager::SubmitRenderState(RenderScene* scene)
{
	scene->SetCamera(camera->GetView(), camera->GetProjection(), camera->GetPosition());
}

void CameraManager::OnDestroy()
{
	camera.reset();
}

Matrix4x4 CameraManager::GetView()       const { return camera->GetView(); }
Matrix4x4 CameraManager::GetProjection() const { return camera->GetProjection(); }
Vector3   CameraManager::GetPosition()   const { return camera->GetPosition(); }
float     CameraManager::GetYaw()        const { return camera->GetYaw(); }
float     CameraManager::GetPitch()      const { return camera->GetPitch(); }
