#pragma once

#include "RHI/IRenderDevice.h"
#include "RHI/Texture/ITexture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct GlyphInfo
{
    float u0, v0, u1, v1;  // Atlas UV 범위 [0, 1]
    int   width,  height;  // 글리프 비트맵 크기 (픽셀)
    int   bearingX;        // 펜 위치 기준 X 오프셋
    int   bearingY;        // 펜 위치 기준 Y 오프셋 (보통 음수 = baseline 위)
    int   advance;         // 다음 글자까지 X 이동량 (픽셀)
};

class GlyphAtlas
{
public:
    void Initialize(IRenderDevice* device, const char* ttfPath, float fontSize);

    // 코드포인트에 해당하는 GlyphInfo 반환. 없으면 래스터라이징 후 dirty 플래그 설정.
    const GlyphInfo* GetGlyph(uint32_t codepoint);

    // 새 글리프가 추가된 경우 Atlas 텍스처를 GPU에 재업로드.
    void FlushIfDirty(IRenderDevice* device);

    ITexture* GetTexture()    const { return atlasTexture.get(); }
    int       GetLineHeight() const { return lineHeight; }

    // UTF-8 문자열 → 코드포인트 배열
    static std::vector<uint32_t> ParseUTF8(const std::string& text);

private:
    void BakeGlyph(uint32_t codepoint);

    // stb_truetype 폰트 데이터 (불투명 포인터로 보관)
    std::vector<uint8_t> fontBuffer;
    void*                fontInfo = nullptr;  // stbtt_fontinfo*
    float                scale    = 1.0f;
    int                  lineHeight = 0;

    // Atlas CPU 버퍼 (RGBA8)
    static constexpr int ATLAS_W = 512;
    static constexpr int ATLAS_H = 512;
    std::vector<uint8_t> atlasPixels;
    int curX = 1, curY = 1, rowHeight = 0;
    bool dirty = false;

    std::unordered_map<uint32_t, GlyphInfo> glyphs;
    std::unique_ptr<ITexture>               atlasTexture;
};
