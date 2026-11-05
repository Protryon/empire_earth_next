// C:\EEX\Low-Level Engine\Graphics Engine\GEFont.h (RECONSTRUCTION). GEFont.cpp 0x100151b7..0x100152e4 holds the
// ctor and dtor; the rest is inline (GEBoundingBox.cpp emits it first, 0x10010718..).
#ifndef LLE_GEFONT_H
#define LLE_GEFONT_H

#include "lle/GEFontBitmap.h"

struct PlatFont;   // platform/PlatFont.h (a GDI HFONT in the original)

class LLE_API GEFont : public UReferenceCounter {   // sizeof 0x30 (vector deleting dtor 0x107fc)
public:
    GEFont(const UString& inName, unsigned long inSize, unsigned long inStyle, unsigned long inQuality);
    virtual ~GEFont();
    bool CompareFont(const UString& inName, unsigned long inStyle, unsigned long inQuality, unsigned long inSize)
        { return this->mQuality == inQuality && this->mStyle == inStyle && this->mSize == inSize && this->mName == inName; }
    bool GetHasFontBitmap() const { return this->mFontBitmap != NULL; }
    void SetFontBitmap(GEFontBitmap* inBitmap) { this->mFontBitmap = inBitmap; inBitmap->Reference(); }
    GEFontBitmap* GetFontBitmap() const { return this->mFontBitmap; }
    const UString& GetFontName() const { return this->mName; }
    unsigned long GetFontQuality() const { return this->mQuality; }
    unsigned long GetFontSize() const { return this->mSize; }
    unsigned long GetFontStyle() const { return this->mStyle; }
    long GetHeight() const { return this->mHeight; }
    PlatFont* GetOSFont() const { return this->mFont; }
private:
    UString mName;                          // +0x08
    PlatFont* mFont;                        // +0x18 (an HFONT in the original)
    unsigned long mQuality;                 // +0x1c 0 default, 1 draft, 2 antialiased
    unsigned long mStyle;                   // +0x20 bit 0 bold, bit 1 italic, bit 2 underline
    GEFontBitmap* mFontBitmap;              // +0x24
    unsigned long mSize;                    // +0x28 point height
    long mHeight;                           // +0x2c tmHeight
};

// GEFont ctor inQuality: the CreateFont quality each case picks (GEFont.cpp 0x100151b7)
enum GEFontQuality {
    kFontQualityDefault = 0,    // GEFont::GEFont case -> DEFAULT_QUALITY
    kFontQualityDraft = 1,      // GEFont::GEFont case -> DRAFT_QUALITY
    kFontQualityAntialiased = 2,  // GEFont::GEFont case -> ANTIALIASED_QUALITY (the only antialiased one)
};

// GEFont::mStyle (+0x20) bits: the GEFont ctor passes them to CreateFont; UIFontTable::Create builds them from the dbfonts flags
enum GEFontStyle {
    kFontStyleBold = 1,     // GEFont ctor: CreateFont weight FW_BOLD
    kFontStyleItalic = 2,   // GEFont ctor: CreateFont fdwItalic = (inStyle >> 1) & 1
    kFontStyleUnderline = 4,  // GEFont ctor: CreateFont fdwUnderline = (inStyle >> 2) & 1
    kFontStyleShadow = 0x10,  // DX7Rasterizer::Draw (Draw2D.cpp:252) draws the text once more at +1,+1 in black or white (provisional name)
};

#endif
