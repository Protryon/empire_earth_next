// Platform layer: fonts and text drawing into pixel buffers (see platform/Platform.h), the replacement of GDI text
// (CreateFont, GetTextMetrics, GetTextExtentPoint32, ExtTextOut on a DIB) for the renderer's text path and the
// engine's GEFont / GEFontBitmap. The same implementation on every system (src/Platform/Common/PlatFont.cpp, on
// stb_truetype), so text looks and measures the same everywhere.
//
// Faces are TrueType data the caller registers (PlatFontRegister: the build embeds them, or they are read from the
// data root). The game asks for "Arial" (and a few others): with Liberation Sans / Serif / Mono registered under the
// names they are metric-compatible with ("Arial", "Times New Roman", "Courier New"), every advance width and line
// height equals the Windows font's at the same size. Text is single-byte Windows-1252 (platform/PlatString.h).
#ifndef PLATFORM_PLATFONT_H
#define PLATFORM_PLATFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registers a TrueType face for inFace (case-insensitive) in one style. The data is used in place: it must stay
// valid while fonts of the face exist. -> nonzero on success (the data is a font).
int PlatFontRegister(const char* inFace, int inBold, int inItalic, const void* inTrueType, uint32_t inSize);
// The faces asked for under another name: PlatFontAlias("MS Sans Serif", "Arial"). -> nonzero on success.
int PlatFontAlias(const char* inFace, const char* inRegisteredFace);

typedef struct PlatFont PlatFont;
// A font as CreateFont makes one: inHeight < 0 is the character (em) height in pixels, > 0 the cell height (ascent
// plus descent), 0 a default of 12 pixels. A face that is not registered falls back to the first registered one
// (GDI's font mapper also always finds some font) -> NULL only when no face is registered at all.
PlatFont* PlatFontCreate(const char* inFace, int32_t inHeight, int inBold, int inItalic, int inUnderline,
                         int inAntialiased);
void PlatFontDestroy(PlatFont* inFont);

// GDI's TEXTMETRIC subset the game reads: the cell (tmHeight = tmAscent + tmDescent), the leading, the widths and
// the character range (Windows-1252: 0x20..0xFF).
typedef struct PlatFontMetrics {
    int32_t mHeight;
    int32_t mAscent;
    int32_t mDescent;
    int32_t mInternalLeading;
    int32_t mExternalLeading;
    int32_t mAveCharWidth;
    int32_t mMaxCharWidth;
    uint8_t mFirstChar;
    uint8_t mLastChar;
} PlatFontMetrics;
void PlatFontGetMetrics(const PlatFont* inFont, PlatFontMetrics* outMetrics);
// The width of inLength bytes of text in pixels (GetTextExtentPoint32's cx; its cy is mHeight)
int32_t PlatFontMeasure(const PlatFont* inFont, const char* inText, uint32_t inLength);

// A 32-bit pixel buffer to draw into: BGRA in memory (A8R8G8B8 little-endian, the game's 32-bit surfaces)
typedef struct PlatFontTarget {
    void* mPixels;
    int32_t mPitch;                         // bytes per row
    int32_t mWidth;
    int32_t mHeight;
} PlatFontTarget;
typedef struct PlatFontRect {
    int32_t mLeft, mTop, mRight, mBottom;   // right and bottom exclusive
} PlatFontRect;
// Draws text with the cell's top-left at (inX, inY), as ExtTextOut with TA_TOP | TA_LEFT: glyphs in inColor
// (0x00RRGGBB); with inOpaque the cell rectangle is first filled with inBackground (ETO_OPAQUE / OPAQUE mode).
// Antialiased fonts blend their coverage, the others set the pixels a glyph covers by half or more. inClip (NULL:
// the whole target) limits every pixel written. The alpha byte of written pixels becomes 0xFF.
void PlatFontDraw(const PlatFont* inFont, const char* inText, uint32_t inLength, int32_t inX, int32_t inY,
                  uint32_t inColor, int inOpaque, uint32_t inBackground, const PlatFontRect* inClip,
                  const PlatFontTarget* inTarget);

#ifdef __cplusplus
}
#endif

#endif
