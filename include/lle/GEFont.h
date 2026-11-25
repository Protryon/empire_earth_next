// C:\EEX\Low-Level Engine\Graphics Engine\GEFont.h (RECONSTRUCTION). A platform font, its name, size, style and
// quality, and the glyph bitmap the font table gives it.
// port (R4.3, sixteenth part): the font is a whole Rust object (ee_ge smallobj.rs; GEFont.cpp deleted): its only member
// besides the reference count is the object, the methods forward, and GEObjectHost (GEBitmap.cpp) makes the calls on
// the name, the bitmap and the platform font.
#ifndef LLE_GEFONT_H
#define LLE_GEFONT_H
#include "lle/LLEForward.h"
#include "rust/render/gestate.h"

#include "lle/GEFontBitmap.h"   // rust/ee_ge.h

struct PlatFont;   // platform/PlatFont.h (a GDI HFONT in the original)

class LLE_API GEFont : public UReferenceCounter {
public:
    // a font as CreateFont made one (name, pixel size, bold/italic/underline style, quality: only the antialiased
    // quality is antialiased) and its height (tmHeight)
    GEFont(const UString& inName, unsigned long inSize, unsigned long inStyle, unsigned long inQuality)
        : mRust(ee_ge_game_font_new(this, &inName, inSize, inStyle, inQuality)) {}
    virtual ~GEFont() { ee_ge_font_destroy(this->mRust); }
    bool CompareFont(const UString& inName, unsigned long inStyle, unsigned long inQuality, unsigned long inSize)
        { return ee_ge_font_compare(this->mRust, &inName, inStyle, inQuality, inSize); }
    void SetFontBitmap(GEFontBitmap* inBitmap) { ee_ge_font_set_bitmap(this->mRust, inBitmap); }
    GEFontBitmap* GetFontBitmap() const { return static_cast<GEFontBitmap*>(ee_ge_font_bitmap(this->mRust)); }
    const UString& GetFontName() const { return *static_cast<const UString*>(ee_ge_font_name(this->mRust)); }
    unsigned long GetFontQuality() const { return (unsigned long)ee_ge_font_value(this->mRust, EeGeFontValue::Quality); }
    unsigned long GetFontSize() const { return (unsigned long)ee_ge_font_value(this->mRust, EeGeFontValue::Size); }
    unsigned long GetFontStyle() const { return (unsigned long)ee_ge_font_value(this->mRust, EeGeFontValue::Style); }
    long GetHeight() const { return ee_ge_font_value(this->mRust, EeGeFontValue::Height); }
    EeGeFont* GetRust() const { return this->mRust; }
    URust<EeGeFont> mRust;                        // the font (ee_ge smallobj.rs)
};

#endif
