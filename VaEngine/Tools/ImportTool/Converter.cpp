#include "Converter.h"

#include "Mesh/MeshData.h"
#include "Mesh/SkinnedVertex.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>

// ── 텍스처 슬롯 처리 ────────────────────────────────────────────────────────

static TextureSlot ProcessTexture(
    const aiScene*     scene,
    const aiMaterial*  mat,
    aiTextureType      type,
    const std::string& matName,
    const std::string& typeSuffix)
{
    if (mat->GetTextureCount(type) == 0)
        return {};

    aiString aiPath;
    mat->GetTexture(type, 0, &aiPath);
    const std::string pathStr = aiPath.C_Str();

    const aiTexture* embedded = scene->GetEmbeddedTexture(pathStr.c_str());

    TextureSlot slot;

    if (embedded)
    {
        // 출력 파일명 결정
        if (embedded->mFilename.length > 0)
            slot.filename = std::filesystem::path(
                embedded->mFilename.C_Str()).filename().string();
        else
        {
            std::string ext = embedded->achFormatHint[0]
                ? std::string(".") + embedded->achFormatHint
                : ".png";
            slot.filename = matName + "_" + typeSuffix + ext;
        }

        if (embedded->mHeight == 0)
        {
            // 압축 embedded (PNG/JPG 바이트 그대로)
            const auto* data = reinterpret_cast<const uint8_t*>(embedded->pcData);
            slot.bytes.assign(data, data + embedded->mWidth);
        }
        else
        {
            // 비압축 embedded (RGBA 픽셀 배열)
            slot.width  = embedded->mWidth;
            slot.height = embedded->mHeight;
            const auto* data = reinterpret_cast<const uint8_t*>(embedded->pcData);
            slot.bytes.assign(data, data + embedded->mWidth * embedded->mHeight * 4);
        }
    }
    else
    {
        // 외부 파일 — 파일명만 보관
        slot.filename = std::filesystem::path(pathStr).filename().string();
    }

    return slot;
}

// ── 재질 추출 ────────────────────────────────────────────────────────────────

static void ExtractMaterials(
    const aiScene*                     scene,
    std::vector<MaterialIntermediate>& out)
{
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* mat = scene->mMaterials[i];
        MaterialIntermediate mi;

        aiString name;
        mat->Get(AI_MATKEY_NAME, name);
        mi.name = name.C_Str();

        // 색상
        aiColor3D color;
        if (mat->Get(AI_MATKEY_COLOR_AMBIENT,  color) == AI_SUCCESS)
            { mi.ambient[0]=color.r;  mi.ambient[1]=color.g;  mi.ambient[2]=color.b; }
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE,  color) == AI_SUCCESS)
            { mi.diffuse[0]=color.r;  mi.diffuse[1]=color.g;  mi.diffuse[2]=color.b; }
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
            { mi.specular[0]=color.r; mi.specular[1]=color.g; mi.specular[2]=color.b; }
        float shininess = 0.0f;
        mat->Get(AI_MATKEY_SHININESS, shininess);
        mi.specular[3] = shininess;
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
            { mi.emissive[0]=color.r; mi.emissive[1]=color.g; mi.emissive[2]=color.b; }

        // 텍스처
        mi.diffuseTex  = ProcessTexture(scene, mat, aiTextureType_DIFFUSE,  mi.name, "diffuse");
        mi.specularTex = ProcessTexture(scene, mat, aiTextureType_SPECULAR, mi.name, "specular");
        mi.normalTex   = ProcessTexture(scene, mat, aiTextureType_NORMALS,  mi.name, "normal");
        if (mi.normalTex.IsEmpty())
            mi.normalTex = ProcessTexture(scene, mat, aiTextureType_HEIGHT, mi.name, "normal");

        out.push_back(std::move(mi));
    }
}

// ── 메시 순회 ────────────────────────────────────────────────────────────────

static void TraverseNode(
    const aiScene*                    scene,
    const aiNode*                     node,
    std::vector<SubMeshIntermediate>& out)
{
    for (uint32_t m = 0; m < node->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[m]];

        if (mesh->mNumVertices > 65535)
        {
            std::cerr << "[ImportTool] Skip \"" << mesh->mName.C_Str()
                      << "\" - vertex count " << mesh->mNumVertices
                      << " exceeds uint16 limit (65535)\n";
            continue;
        }

        SubMeshIntermediate sub;
        sub.name = mesh->mName.C_Str();

        if (scene->mNumMaterials > 0 && mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiString matName;
            scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, matName);
            sub.materialName = matName.C_Str();
        }

        const uint32_t stride = sizeof(PrimitiveVertex);
        sub.vertices.resize(mesh->mNumVertices * stride);

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            PrimitiveVertex pv{};

            pv.pos[0] = mesh->mVertices[v].x;
            pv.pos[1] = mesh->mVertices[v].y;
            pv.pos[2] = mesh->mVertices[v].z;

            if (mesh->HasNormals())
            {
                pv.normal[0] = mesh->mNormals[v].x;
                pv.normal[1] = mesh->mNormals[v].y;
                pv.normal[2] = mesh->mNormals[v].z;
            }

            if (mesh->HasVertexColors(0))
            {
                pv.color[0] = mesh->mColors[0][v].r;
                pv.color[1] = mesh->mColors[0][v].g;
                pv.color[2] = mesh->mColors[0][v].b;
                pv.color[3] = mesh->mColors[0][v].a;
            }
            else
            {
                pv.color[0] = pv.color[1] = pv.color[2] = pv.color[3] = 1.0f;
            }

            if (mesh->HasTextureCoords(0))
            {
                pv.uv[0] = mesh->mTextureCoords[0][v].x;
                pv.uv[1] = mesh->mTextureCoords[0][v].y;
            }

            std::memcpy(sub.vertices.data() + v * stride, &pv, stride);
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            assert(face.mNumIndices == 3);
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[0]));
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[1]));
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[2]));
        }

        out.push_back(std::move(sub));
    }

    for (uint32_t c = 0; c < node->mNumChildren; ++c)
        TraverseNode(scene, node->mChildren[c], out);
}

// ── Inspect (경량 로드: 본/애니메이션 유무만 확인) ───────────────────────────

FbxTraits Converter::Inspect(const std::string& path)
{
    Assimp::Importer importer;
    // 헤더 수준 파싱만 수행 — 정점/재질 처리 생략
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->mRootNode)
        return {};

    bool hasBones = false;
    for (uint32_t m = 0; m < scene->mNumMeshes && !hasBones; ++m)
        hasBones = (scene->mMeshes[m]->mNumBones > 0);

    bool hasRealAnimations = false;
    for (uint32_t a = 0; a < scene->mNumAnimations && !hasRealAnimations; ++a)
    {
        const aiAnimation* anim = scene->mAnimations[a];
        const double tps = (anim->mTicksPerSecond > 0.0) ? anim->mTicksPerSecond : 25.0;
        if (anim->mDuration / tps > 0.5)
            hasRealAnimations = true;
    }
    return { hasBones, hasRealAnimations };
}

// ── 진입점 ───────────────────────────────────────────────────────────────────

ConvertResult Converter::Convert(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate          |
        aiProcess_GenNormals           |
        aiProcess_GenUVCoords          |
        aiProcess_ConvertToLeftHanded  |
        aiProcess_JoinIdenticalVertices);

    if (!scene || !scene->mRootNode)
    {
        std::cerr << "[ImportTool] Failed to load: " << path
                  << "\n  " << importer.GetErrorString() << "\n";
        return {};
    }

    ConvertResult result;
    TraverseNode(scene, scene->mRootNode, result.meshes);
    ExtractMaterials(scene, result.materials);

    std::cout << "[ImportTool] Loaded \"" << path
              << "\" - " << result.meshes.size()    << " sub-mesh(es)"
              << ", "   << result.materials.size() << " material(s)\n";
    return result;
}

// ── ConvertSkinned 헬퍼 ───────────────────────────────────────────────────────

static void AiMatToFloat16(const aiMatrix4x4& m, float out[16])
{
    // Assimp column-vector (row-major storage) → DX row-vector: transpose
    out[0]  = m.a1; out[1]  = m.b1; out[2]  = m.c1; out[3]  = m.d1;
    out[4]  = m.a2; out[5]  = m.b2; out[6]  = m.c2; out[7]  = m.d2;
    out[8]  = m.a3; out[9]  = m.b3; out[10] = m.c3; out[11] = m.d3;
    out[12] = m.a4; out[13] = m.b4; out[14] = m.c4; out[15] = m.d4;
}

static aiVector3D SampleVec3(const aiVectorKey* keys, uint32_t count, double t)
{
    if (count == 0) return { 0.0f, 0.0f, 0.0f };
    if (count == 1 || t <= keys[0].mTime) return keys[0].mValue;
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        if (t < keys[i + 1].mTime)
        {
            const double dt  = keys[i + 1].mTime - keys[i].mTime;
            const float  fac = (dt > 0.0) ? static_cast<float>((t - keys[i].mTime) / dt) : 0.0f;
            const auto& a = keys[i].mValue;
            const auto& b = keys[i + 1].mValue;
            return { a.x + fac * (b.x - a.x),
                     a.y + fac * (b.y - a.y),
                     a.z + fac * (b.z - a.z) };
        }
    }
    return keys[count - 1].mValue;
}

static aiQuaternion SampleQuat(const aiQuatKey* keys, uint32_t count, double t)
{
    if (count == 0) return { 1.0f, 0.0f, 0.0f, 0.0f };
    if (count == 1 || t <= keys[0].mTime) return keys[0].mValue;
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        if (t < keys[i + 1].mTime)
        {
            const double dt  = keys[i + 1].mTime - keys[i].mTime;
            const float  fac = (dt > 0.0) ? static_cast<float>((t - keys[i].mTime) / dt) : 0.0f;
            aiQuaternion result;
            aiQuaternion::Interpolate(result, keys[i].mValue, keys[i + 1].mValue, fac);
            return result.Normalize();
        }
    }
    return keys[count - 1].mValue;
}

// ── ConvertSkinned ────────────────────────────────────────────────────────────

SkinnedConvertResult Converter::ConvertSkinned(const std::string& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate           |
        aiProcess_GenNormals            |
        aiProcess_GenUVCoords           |
        aiProcess_ConvertToLeftHanded   |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights);

    if (!scene || !scene->mRootNode)
    {
        std::cerr << "[ImportTool] Failed to load: " << path
                  << "\n  " << importer.GetErrorString() << "\n";
        return {};
    }

    SkinnedConvertResult result;

    // ── 본 이름 수집 + offset 행렬 수집 ─────────────────────────────────────
    std::set<std::string>              boneNameSet;
    std::map<std::string, aiMatrix4x4> boneOffsetMap;

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];
        for (uint32_t b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            const std::string name = bone->mName.C_Str();
            boneNameSet.insert(name);
            boneOffsetMap.emplace(name, bone->mOffsetMatrix);
        }
    }

    // 메시 본이 없는 animation-only FBX일 때만 채널에서 보강
    if (boneNameSet.empty())
    {
        for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
        {
            const aiAnimation* anim = scene->mAnimations[a];
            for (uint32_t c = 0; c < anim->mNumChannels; ++c)
                boneNameSet.insert(anim->mChannels[c]->mNodeName.C_Str());
        }
    }

    if (boneNameSet.empty())
    {
        std::cout << "[ImportTool] No bones in: " << path << "\n";
        return {};
    }

    // ── 본 계층 구성 (DFS → 위상 정렬 순서 보장) ────────────────────────────
    std::map<std::string, int> boneIdxMap;

    std::function<void(const aiNode*, int)> buildBones =
        [&](const aiNode* node, int parentIdx)
        {
            const std::string name = node->mName.C_Str();
            int myIdx = parentIdx;

            if (boneNameSet.count(name))
            {
                BoneIntermediate bone;
                bone.name        = name;
                bone.parentIndex = parentIdx;

                auto it = boneOffsetMap.find(name);
                if (it != boneOffsetMap.end())
                    AiMatToFloat16(it->second, bone.offsetMatrix);
                else
                {
                    bone.offsetMatrix[0]  = 1.0f;
                    bone.offsetMatrix[5]  = 1.0f;
                    bone.offsetMatrix[10] = 1.0f;
                    bone.offsetMatrix[15] = 1.0f;
                }

                // bindPose: 현재 파이프라인에서 미사용 — 단위행렬
                bone.bindPose[0]  = 1.0f;
                bone.bindPose[5]  = 1.0f;
                bone.bindPose[10] = 1.0f;
                bone.bindPose[15] = 1.0f;

                myIdx = static_cast<int>(result.bones.size());
                boneIdxMap[name] = myIdx;
                result.bones.push_back(bone);
            }

            for (uint32_t c = 0; c < node->mNumChildren; ++c)
                buildBones(node->mChildren[c], myIdx);
        };

    buildBones(scene->mRootNode, -1);

    // ── 스키닝 메시 추출 ─────────────────────────────────────────────────────
    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];

        if (mesh->mNumVertices > 65535)
        {
            std::cerr << "[ImportTool] Skip \"" << mesh->mName.C_Str()
                      << "\" - vertex count exceeds uint16 limit\n";
            continue;
        }

        // 정점별 본 데이터 수집 (최대 4개)
        struct VertexBoneData { uint32_t idx[4] = {}; float weight[4] = {}; uint32_t count = 0; };
        std::vector<VertexBoneData> vbd(mesh->mNumVertices);

        for (uint32_t b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            auto it = boneIdxMap.find(bone->mName.C_Str());
            if (it == boneIdxMap.end()) continue;

            for (uint32_t w = 0; w < bone->mNumWeights; ++w)
            {
                const aiVertexWeight& vw = bone->mWeights[w];
                VertexBoneData& vb = vbd[vw.mVertexId];
                if (vb.count < 4)
                {
                    vb.idx[vb.count]    = static_cast<uint32_t>(it->second);
                    vb.weight[vb.count] = vw.mWeight;
                    ++vb.count;
                }
            }
        }

        constexpr uint32_t kStride = sizeof(SkinnedVertex);
        SkinnedSubMeshIntermediate sub;
        sub.name = mesh->mName.C_Str();

        if (scene->mNumMaterials > 0 && mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiString matName;
            scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, matName);
            sub.materialName = matName.C_Str();
        }

        sub.vertices.resize(mesh->mNumVertices * kStride);

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            SkinnedVertex sv{};

            sv.pos[0] = mesh->mVertices[v].x;
            sv.pos[1] = mesh->mVertices[v].y;
            sv.pos[2] = mesh->mVertices[v].z;

            if (mesh->HasNormals())
            {
                sv.normal[0] = mesh->mNormals[v].x;
                sv.normal[1] = mesh->mNormals[v].y;
                sv.normal[2] = mesh->mNormals[v].z;
            }

            if (mesh->HasVertexColors(0))
            {
                sv.color[0] = mesh->mColors[0][v].r;
                sv.color[1] = mesh->mColors[0][v].g;
                sv.color[2] = mesh->mColors[0][v].b;
                sv.color[3] = mesh->mColors[0][v].a;
            }
            else
            {
                sv.color[0] = sv.color[1] = sv.color[2] = sv.color[3] = 1.0f;
            }

            if (mesh->HasTextureCoords(0))
            {
                sv.uv[0] = mesh->mTextureCoords[0][v].x;
                sv.uv[1] = mesh->mTextureCoords[0][v].y;
            }

            for (uint32_t b = 0; b < 4; ++b)
            {
                sv.boneIndex[b]  = vbd[v].idx[b];
                sv.boneWeight[b] = vbd[v].weight[b];
            }

            std::memcpy(sub.vertices.data() + v * kStride, &sv, kStride);
        }

        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[0]));
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[1]));
            sub.indices.push_back(static_cast<uint16_t>(face.mIndices[2]));
        }

        result.meshes.push_back(std::move(sub));
    }

    // ── 재질 추출 ────────────────────────────────────────────────────────────
    ExtractMaterials(scene, result.materials);

    // ── 애니메이션 클립 추출 ─────────────────────────────────────────────────
    constexpr float kFrameRate = 30.0f;
    const uint32_t  boneCount  = static_cast<uint32_t>(result.bones.size());

    for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
    {
        const aiAnimation* anim = scene->mAnimations[a];
        const double tps = (anim->mTicksPerSecond > 0.0) ? anim->mTicksPerSecond : 25.0;
        const float  duration   = static_cast<float>(anim->mDuration / tps);
        const uint32_t frameCount = static_cast<uint32_t>(duration * kFrameRate) + 1;

        std::map<std::string, const aiNodeAnim*> channelMap;
        for (uint32_t c = 0; c < anim->mNumChannels; ++c)
        {
            const aiNodeAnim* ch = anim->mChannels[c];
            channelMap[ch->mNodeName.C_Str()] = ch;
        }

        AnimClipIntermediate clip;
        clip.name       = anim->mName.C_Str();
        clip.duration   = duration;
        clip.frameRate  = kFrameRate;
        clip.frameCount = frameCount;
        clip.boneCount  = boneCount;
        clip.boneNames.reserve(boneCount);
        for (uint32_t b = 0; b < boneCount; ++b)
            clip.boneNames.push_back(result.bones[b].name);
        clip.keyframes.resize(boneCount, std::vector<KeyframeSRT>(frameCount));

        for (uint32_t b = 0; b < boneCount; ++b)
        {
            auto chIt = channelMap.find(result.bones[b].name);

            for (uint32_t f = 0; f < frameCount; ++f)
            {
                const double t = (static_cast<double>(f) / kFrameRate) * tps;
                KeyframeSRT& kf = clip.keyframes[b][f];

                if (chIt != channelMap.end())
                {
                    const aiNodeAnim* ch = chIt->second;
                    aiVector3D   s = SampleVec3(ch->mScalingKeys,
                                                ch->mNumScalingKeys, t);
                    aiQuaternion r = SampleQuat(ch->mRotationKeys,
                                                ch->mNumRotationKeys, t);
                    aiVector3D   p = SampleVec3(ch->mPositionKeys,
                                                ch->mNumPositionKeys, t);

                    kf.scale[0] = s.x;  kf.scale[1] = s.y;  kf.scale[2] = s.z;
                    kf.rotation[0] = r.x; kf.rotation[1] = r.y;
                    kf.rotation[2] = r.z; kf.rotation[3] = r.w;
                    kf.translation[0] = p.x;
                    kf.translation[1] = p.y;
                    kf.translation[2] = p.z;
                }
                else
                {
                    // 애니메이션 채널 없음 → rest pose (단위 SRT)
                    kf.scale[0] = kf.scale[1] = kf.scale[2] = 1.0f;
                    kf.rotation[3] = 1.0f;
                }
            }
        }

        result.clips.push_back(std::move(clip));
    }

    std::cout << "[ImportTool] Skinned \"" << path
              << "\" - " << result.bones.size()  << " bone(s)"
              << ", "    << result.meshes.size() << " mesh(es)"
              << ", "    << result.clips.size()  << " clip(s)\n";
    return result;
}
