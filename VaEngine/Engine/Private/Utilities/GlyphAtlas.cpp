#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "GlyphAtlas.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

void GlyphAtlas::Initialize(IRenderDevice* device, const char* ttfPath, float fontSize)
{
    // TTF 파일 로드
    std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        throw std::runtime_error("GlyphAtlas: TTF 파일을 열 수 없음");

    auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);
    fontBuffer.resize(fileSize);
    file.read(reinterpret_cast<char*>(fontBuffer.data()), fileSize);

    // stbtt 초기화
    fontInfo = new stbtt_fontinfo();
    auto* info = static_cast<stbtt_fontinfo*>(fontInfo);
    if (!stbtt_InitFont(info, fontBuffer.data(), stbtt_GetFontOffsetForIndex(fontBuffer.data(), 0)))
        throw std::runtime_error("GlyphAtlas: stbtt_InitFont 실패");

    scale = stbtt_ScaleForPixelHeight(info, fontSize);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    lineHeight = static_cast<int>((ascent - descent + lineGap) * scale);

    // Atlas CPU 버퍼 초기화 (RGBA8, 투명)
    atlasPixels.assign(ATLAS_W * ATLAS_H * 4, 0);

    // ASCII pre-bake (0x20 ~ 0x7E)
    for (uint32_t cp = 0x20; cp <= 0x7E; ++cp)
        BakeGlyph(cp);

    // GPU 텍스처 최초 업로드
    atlasTexture = device->CreateTexture();
    atlasTexture->LoadFromMemory(device, atlasPixels.data(), ATLAS_W, ATLAS_H);
    dirty = false;
}

const GlyphInfo* GlyphAtlas::GetGlyph(uint32_t codepoint)
{
    auto it = glyphs.find(codepoint);
    if (it != glyphs.end())
        return &it->second;

    BakeGlyph(codepoint);

    it = glyphs.find(codepoint);
    return (it != glyphs.end()) ? &it->second : nullptr;
}

void GlyphAtlas::FlushIfDirty(IRenderDevice* device)
{
    if (!dirty) return;
    atlasTexture->LoadFromMemory(device, atlasPixels.data(), ATLAS_W, ATLAS_H);
    dirty = false;
}

void GlyphAtlas::BakeGlyph(uint32_t codepoint)
{
    if (glyphs.count(codepoint)) return;

    auto* info = static_cast<stbtt_fontinfo*>(fontInfo);
    int advance, lsb;
    stbtt_GetCodepointHMetrics(info, static_cast<int>(codepoint), &advance, &lsb);

    int w = 0, h = 0, xoff = 0, yoff = 0;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(info, 0, scale,
                                                static_cast<int>(codepoint),
                                                &w, &h, &xoff, &yoff);

    GlyphInfo glyph = {};
    glyph.advance  = static_cast<int>(advance * scale);
    glyph.bearingX = xoff;
    glyph.bearingY = yoff;
    glyph.width    = w;
    glyph.height   = h;

    if (bitmap && w > 0 && h > 0)
    {
        // Shelf packing: 현재 행에 공간 없으면 다음 행으로
        if (curX + w + 1 > ATLAS_W)
        {
            curX = 1;
            curY += rowHeight + 1;
            rowHeight = 0;
        }

        // Atlas 꽉 찬 경우 스킵 (공간 확장은 미지원)
        if (curY + h + 1 <= ATLAS_H)
        {
            // 알파 → RGBA8 확장 (R=G=B=255, A=alpha)
            for (int py = 0; py < h; ++py)
            {
                for (int px = 0; px < w; ++px)
                {
                    uint8_t alpha = bitmap[py * w + px];
                    int idx = ((curY + py) * ATLAS_W + (curX + px)) * 4;
                    atlasPixels[idx + 0] = 255;
                    atlasPixels[idx + 1] = 255;
                    atlasPixels[idx + 2] = 255;
                    atlasPixels[idx + 3] = alpha;
                }
            }

            glyph.u0 = static_cast<float>(curX)     / ATLAS_W;
            glyph.v0 = static_cast<float>(curY)     / ATLAS_H;
            glyph.u1 = static_cast<float>(curX + w) / ATLAS_W;
            glyph.v1 = static_cast<float>(curY + h) / ATLAS_H;

            rowHeight = std::max(rowHeight, h);
            curX += w + 1;
            dirty = true;
        }

        stbtt_FreeBitmap(bitmap, nullptr);
    }

    glyphs[codepoint] = glyph;
}

std::vector<uint32_t> GlyphAtlas::ParseUTF8(const std::string& text)
{
    std::vector<uint32_t> codepoints;
    const auto* s   = reinterpret_cast<const uint8_t*>(text.data());
    const auto* end = s + text.size();

    while (s < end)
    {
        uint32_t cp = 0;
        if (*s < 0x80)
        {
            cp = *s++;
        }
        else if ((*s & 0xE0) == 0xC0 && s + 1 < end)
        {
            cp  = (*s++ & 0x1F) << 6;
            cp |= (*s++ & 0x3F);
        }
        else if ((*s & 0xF0) == 0xE0 && s + 2 < end)
        {
            cp  = (*s++ & 0x0F) << 12;
            cp |= (*s++ & 0x3F) << 6;
            cp |= (*s++ & 0x3F);
        }
        else if ((*s & 0xF8) == 0xF0 && s + 3 < end)
        {
            cp  = (*s++ & 0x07) << 18;
            cp |= (*s++ & 0x3F) << 12;
            cp |= (*s++ & 0x3F) << 6;
            cp |= (*s++ & 0x3F);
        }
        else
        {
            ++s;
            continue;
        }
        codepoints.push_back(cp);
    }
    return codepoints;
}
