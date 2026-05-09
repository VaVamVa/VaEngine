#include "VaProgramName.h"

#include "System/InputSystem.h"
#include "System/IPointerInput.h"

#include "Light/LightManager.h"
#include "GameManager/CameraManager.h"

#include "Utilities/Time.h"

#include "Math/Container.h"
#include "WorldObjects/WO_Cube.h"
#include "WorldObjects/WO_Tower.h"
#include "WorldObjects/WO_Kachujin.h"

#include "Utilities/Helpers.h"

void VaProgramName::OnInitialize(IRenderDevice* device)
{
	cameraManager = new CameraManager();
	cameraManager->OnInitialize(device);

	kachujin = new WO_Kachujin();
	kachujin->Initialize(device);
	kachujin->transform.SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	kachujin->transform.SetScale(Vector3(0.01f, 0.01f, 0.01f));
	clipIndex = 0;

	cube = new WO_Cube();
	cube->Initialize(device);
	cube->transform.SetPosition(Vector3(-5.0f, 0.0f, 0.0f));

	tower = new WO_Tower();
	tower->Initialize(device);
	tower->transform.SetPosition(Vector3(5.0f, 0.0f, 0.0f));

	lightManager = new LightManager();
	lightManager->GetDirectionalLight()->SetEnabled(true);

	IPointLight* pt = lightManager->AddPointLight();
	pt->SetPosition(2.0f, -2.0f, -2.0f);
	pt->SetRange(15.0f);
	pt->SetAttenuation(1.0f, 0.09f, 0.032f);
	pt->SetDiffuse(1.0f, 0.8f, 0.6f);
	pt->SetSpecular(1.0f, 0.8f, 0.6f);
	pt->SetEnabled(false);
}

void VaProgramName::OnUpdate(float deltaTime)
{
	time += deltaTime;
	if (time > 5.0f)
	{
		time -= 5.0f;
		clipIndex = (clipIndex + 1) % kachujin->GetClipCount();
		kachujin->PlayTween(clipIndex, 0.3f);
	}
	

	cube->transform.Rotate(Vector3::Up, Math::PI * 0.5f * deltaTime);
	kachujin->Update(deltaTime);

	cameraManager->OnUpdate(deltaTime);
}

void VaProgramName::SubmitRenderState(RenderScene* scene)
{
	lightManager->Submit(scene);
	cameraManager->SubmitRenderState(scene);

	scene->AddMesh(cube->GetMesh(), cube->transform.GetMatrix());

	for (const auto& mesh : tower->GetMeshes())
		scene->AddMesh(mesh.get(), tower->transform.GetMatrix(), tower->GetTexture());

	kachujin->AddToScene(*scene);
}

void VaProgramName::OnDestroy()
{
	cameraManager->OnDestroy();

	SAFE_DELETE(cameraManager);
	SAFE_DELETE(cube);
	SAFE_DELETE(tower);
	SAFE_DELETE(kachujin);
	SAFE_DELETE(lightManager);
}
