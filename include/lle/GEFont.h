// C:\EEX\Low-Level Engine\Graphics Engine\GEFont.h (RECONSTRUCTION). A platform font, its name, size, style and
// quality, and the glyph bitmap the font table gives it.
// port (R4.3, sixteenth part): the font is a whole Rust object (ee_ge smallobj.rs; GEFont.cpp deleted): its only member
// besides the reference count is the object, the methods forward, and GEObjectHost (GEBitmap.cpp) makes the calls on
// the name, the bitmap and the platform font.
#ifndef LLE_GEFONT_H
#define LLE_GEFONT_H
#include <stdint.h>
#include "lle/LLEForward.h"
#include "lle/GERust.h"
#include "rust/render/gestate.h"
#include "rust/render/ge.h"   // the font bitmap (ee_ge fbobj.rs), its host's EeGeFbOp

// C:\EEX\Low-Level Engine\Graphics Engine\GEFontBitmap.h (RECONSTRUCTION; folded here in LLE/UI round 2).
// GEFontBitmap.cpp 0x100152e4..0x100166d3. The implicit copy members (0x1001064c/0x1001068b) are emitted by
// GEBoundingBox.cpp's inline block. The class is a shell over a whole Rust object (ee_ge fbobj.rs).
// port (R4.3, tenth part): the glyph cells (the original's GEFBCharacter, sizeof 0x2c: texture coordinates, the cell
// rectangle, the referenced texture, the scales) live in the Rust object (EeGeFbCharacter).
class LLE_API GEFontBitmap : public UReferenceCounter {   // sizeof 0x24 (vector deleting dtor 0x106be)
public:
    // renders every glyph of the font into 256x256 alpha textures and records each glyph's cell (ee_ge fbobj.rs;
    // the object is NULL while it is made)
    GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen)
        : mRust(NULL) { this->mRust = ee_ge_fb_new(&GEFontBitmap::Host, this, inRasterizer, inFont, inScreen); }
    bool operator==(GEFont* inFont) { return true; }
    long Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen) { return ee_ge_fb_draw(this->mRust, inText, NULL, inRasterizer, inScreen); }
    long Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer, unsigned long inScreen)
        { return ee_ge_fb_draw(this->mRust, inText, GERust(inRectangle), inRasterizer, inScreen); }
    // 0x159ac (protected: released through Dereference): its drawing bitmap released
    virtual ~GEFontBitmap() { ee_ge_fb_destroy(this->mRust); }
    // (CreateNewFontTexture 0x100159cd and CreateSurfaceAlpha 0x10015ac5 are the Rust object's)
    // The calls the font bitmap makes on other objects (EeGeFbOp), in the original's statements
    static intptr_t Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b);

    URust<EeGeFontBitmap> mRust;                  // the cells, the drawing bitmap, the first character (ee_ge fbobj.rs)
};

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
