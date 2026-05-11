#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SubMeshIntermediate
{
    std::string           name;
    std::string           materialName;
    std::vector<uint8_t>  vertices;
    std::vector<uint16_t> indices;
};

struct TextureSlot
{
    std::string          filename;   // 출력 파일명
    std::vector<uint8_t> bytes;      // non-empty = embedded
    uint32_t             width  = 0; // 0 = 압축 embedded or 외부 파일
    uint32_t             height = 0;

    bool IsEmpty()    const { return filename.empty(); }
    bool IsEmbedded() const { return !bytes.empty(); }
    bool IsRGBA()     const { return width > 0 && height > 0; }
};

struct MaterialIntermediate
{
    std::string name;

    float ambient[4]  = { 0.1f, 0.1f, 0.1f, 1.0f };
    float diffuse[4]  = { 0.8f, 0.8f, 0.8f, 1.0f };
    float specular[4] = { 0.5f, 0.5f, 0.5f, 1.0f };  // [3] = shininess
    float emissive[4] = {};

    TextureSlot diffuseTex;
    TextureSlot normalTex;
    TextureSlot specularTex;
};

struct ConvertResult
{
    std::vector<SubMeshIntermediate>  meshes;
    std::vector<MaterialIntermediate> materials;
};

// ── 스키닝/애니메이션 중간 데이터 ─────────────────────────────────────────────

struct BoneIntermediate
{
    std::string name;
    int         parentIndex      = -1;
    float       bindPose[16]     = {};  // Row-major (단위행렬 기본값)
    float       offsetMatrix[16] = {};  // Inverse Bind Pose
};

struct KeyframeSRT
{
    float scale[3]       = { 1.0f, 1.0f, 1.0f };
    float rotation[4]    = { 0.0f, 0.0f, 0.0f, 1.0f }; // xyzw
    float translation[3] = {};
};

struct SkinnedSubMeshIntermediate
{
    std::string           name;
    std::string           materialName;
    std::vector<uint8_t>  vertices;      // SkinnedVertex × stride=80
    std::vector<uint16_t> indices;
};

struct AnimClipIntermediate
{
    std::string                           name;
    float                                 duration   = 0.0f;
    float                                 frameRate  = 30.0f;
    uint32_t                              frameCount = 0;
    uint32_t                              boneCount  = 0;
    std::vector<std::string>              boneNames;           // [boneIndex]
    std::vector<std::vector<KeyframeSRT>> keyframes;  // [boneIndex][frameIndex]
};

struct SkinnedConvertResult
{
    std::vector<BoneIntermediate>           bones;
    std::vector<SkinnedSubMeshIntermediate> meshes;
    std::vector<MaterialIntermediate>       materials;
    std::vector<AnimClipIntermediate>       clips;
};

// FBX 경량 로드 — 본/애니메이션 유무만 확인
struct FbxTraits
{
    bool hasBones;
    bool hasAnimations;
};

class Converter
{
public:
    FbxTraits            Inspect        (const std::string& path);
    ConvertResult        Convert        (const std::string& path);
    SkinnedConvertResult ConvertSkinned (const std::string& path);
};
