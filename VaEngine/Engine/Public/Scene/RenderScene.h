#pragma once

#include "Math/Container.h"
#include "Render/ILight.h"
#include "Animation/AnimController.h"
#include "Utilities/DebuggingHelper.h"
#include "RHI/Texture/ITexture.h"

#include <algorithm>
#include <vector>
#include <cstdint>
#include <format>

class IMesh;
class ITexture;
class ITexture2DArray;
class SkinnedMesh;
class IBuffer;

struct LightingState
{
	DirectionalLightData           dirLight;
	std::vector<PointLightData>    pointLights;
	std::vector<SpotLightData>     spotLights;
};

// 정렬 키 레이아웃: [ Layer(4) | Translucency(1) | Pass(3) | MaterialID(16) | Depth(40) ]
// 오름차순 정렬 기준:
//   - Layer 낮을수록 먼저 렌더링 (배경 → 전경)
//   - Translucency 0(불투명) → 1(반투명)
//   - 불투명: Depth 작을수록 먼저 (front-to-back, 오버드로 최소화)
//   - 반투명: Depth 클수록 먼저 (back-to-front, 알파 블렌딩 정확도)
using RenderSortKey = uint64_t;
using SceneObjectID = uint64_t;

// Client(Application)가 오브젝트별로 설정하는 렌더링 속성.
// depth·translucent·pass 등 Engine이 자동 결정하는 값은 포함하지 않는다.
struct RenderObjectDesc
{
	uint8_t       layer      = 0;   // 렌더링 레이어 (0-15)
	uint16_t      materialID = 0;   // PSO 배칭용 (향후 확장)
	SceneObjectID objectID   = 0;   // 피킹/선택용 ID
};

struct SortKeyDesc
{
	uint8_t  layer       = 0;     // 0-15: 렌더링 레이어
	bool     translucent = false; // false=불투명, true=반투명
	uint8_t  pass        = 0;     // 0-7: 렌더 패스 종류
	uint16_t materialID  = 0;     // 0-65535: PSO/머티리얼 배칭용
	float    depth       = 0.0f;  // [0,1] 정규화된 카메라 공간 깊이
};

inline RenderSortKey MakeSortKey(const SortKeyDesc& desc)
{
	constexpr uint64_t kMaxDepth40 = (1ULL << 40) - 1;
	const float d = std::clamp(desc.depth, 0.0f, 1.0f);

	// 불투명: front-to-back (가까울수록 먼저 = depth 작을수록 낮은 키)
	// 반투명: back-to-front  (멀수록 먼저    = depth 클수록 낮은 키 → 반전)
	const uint64_t depthBits = desc.translucent
		? static_cast<uint64_t>((1.0f - d) * static_cast<float>(kMaxDepth40))
		: static_cast<uint64_t>(d           * static_cast<float>(kMaxDepth40));

	return  (static_cast<uint64_t>(desc.layer               & 0xF)     << 60)
	      | (static_cast<uint64_t>(desc.translucent ? 1 : 0)           << 59)
	      | (static_cast<uint64_t>(desc.pass                & 0x7)     << 56)
	      | (static_cast<uint64_t>(desc.materialID          & 0xFFFF)  << 40)
	      | depthBits;
}

struct RenderCommand
{
	RenderSortKey    sortKey       = 0;
	SceneObjectID    objectID      = 0;
	IMesh*           mesh          = nullptr;
	Matrix4x4        worldMatrix;
	ITexture*        texture       = nullptr;

	// Skinned Mesh Data (Optional)
	SkinnedMesh*     skinnedMesh   = nullptr;
	ITexture2DArray* transformsMap = nullptr;
	IBuffer*         tweenBuffer   = nullptr;
	uint32_t         instanceCount = 1;
};

struct CameraData
{
	Matrix4x4 view;
	Matrix4x4 proj;
	float     eyePos[3] = {};
	float     farZ      = 1000.0f;
};

class RenderScene
{
public:
	void SetCamera(const Matrix4x4& view, const Matrix4x4& proj, const Vector3& eye = {}, float farZ = 1000.0f)
	{
		camera.view       = view;
		camera.proj       = proj;
		camera.eyePos[0]  = eye.x;
		camera.eyePos[1]  = eye.y;
		camera.eyePos[2]  = eye.z;
		camera.farZ       = farZ;
	}

	void AddCommand(const RenderCommand& cmd)
	{
		commands.push_back(cmd);
	}


	void AddMesh(IMesh* mesh, const Matrix4x4& worldMatrix,
	             ITexture* texture = nullptr, const RenderObjectDesc& desc = {})
	{
		RenderCommand cmd;
		cmd.objectID    = desc.objectID;
		cmd.mesh        = mesh;
		cmd.worldMatrix = worldMatrix;
		cmd.texture     = texture;
		cmd.sortKey     = CalculateSortKey(worldMatrix, texture, desc.layer, desc.materialID);
		commands.push_back(cmd);
	}

	void SortCommands()
	{
		if (commands.empty()) return;

		VA_DRAW_PANEL(15, std::format("Scene: Sorting {} commands...", commands.size()));

		std::sort(commands.begin(), commands.end(),
			[](const RenderCommand& a, const RenderCommand& b)
			{
				return a.sortKey < b.sortKey;
			});

		for (size_t i = 0; i < std::min<size_t>(5, commands.size()); ++i)
		{
			VA_DRAW_PANEL(17 + i * 2, std::format("  [{}] Key: 0x{:016x}", i, commands[i].sortKey));
		}
	}

	void AddSkinnedMesh(SkinnedMesh* mesh, const Matrix4x4& worldMatrix,
	                    ITexture* texture, ITexture2DArray* transformsMap,
	                    IBuffer* tweenBuffer, uint32_t instanceCount = 1,
	                    const RenderObjectDesc& desc = {})
	{
		RenderCommand cmd;
		cmd.objectID      = desc.objectID;
		cmd.skinnedMesh   = mesh;
		cmd.texture       = texture;
		cmd.transformsMap = transformsMap;
		cmd.tweenBuffer   = tweenBuffer;
		cmd.worldMatrix   = worldMatrix;
		cmd.instanceCount = instanceCount;
		cmd.sortKey       = CalculateSortKey(worldMatrix, texture, desc.layer, desc.materialID);
		commands.push_back(cmd);
	}

	void SetLighting(const LightingState& lighting) { lightingState = lighting; }

	void SetSkybox(ITexture* tex) { skyTexture = tex; }

	void Clear()
	{
		commands.clear();
		skyTexture = nullptr;
	}

	const CameraData&                        GetCamera()          const { return camera; }
	const std::vector<RenderCommand>&        GetCommands()        const { return commands; }
	const LightingState&                     GetLighting()        const { return lightingState; }
	ITexture*                                GetSkybox()          const { return skyTexture; }

private:
	RenderSortKey CalculateSortKey(const Matrix4x4& worldMatrix, ITexture* texture,
	                               uint8_t layer, uint16_t materialID = 0) const
	{
		SortKeyDesc desc;

		// 1. Layer & Translucency
		desc.layer       = layer;
		desc.translucent = (texture != nullptr && texture->HasAlpha());
		desc.pass        = 0;
		desc.materialID  = materialID;

		// 2. Depth calculation (distance from eye)
		Vector3 objPos = { worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
		Vector3 camPos = { camera.eyePos[0], camera.eyePos[1], camera.eyePos[2] };
		
		float dist = Vector3::Distance(objPos, camPos);
		desc.depth = std::clamp(dist / camera.farZ, 0.0f, 1.0f);

		return MakeSortKey(desc);
	}

	CameraData                        camera;
	LightingState                     lightingState;
	std::vector<RenderCommand>        commands;
	ITexture*                         skyTexture = nullptr;
};
