// C:\EEX\Low-Level Engine\Graphics Engine\GEFontBitmap.h (RECONSTRUCTION). GEFontBitmap.cpp 0x100152e4..0x100166d3.
// The implicit copy members (0x1001064c/0x1001068b) are emitted by GEBoundingBox.cpp's inline block. The class is
// a shell over a whole Rust object (ee_ge fbobj.rs).
#ifndef LLE_GEFONTBITMAP_H
#define LLE_GEFONTBITMAP_H

#include <stdint.h>
#include "rust/ee_ge.h"   // the font bitmap (ee_ge fbobj.rs), its host's EeGeFbOp

class GEFont;
class GERasterizer;
class GERectangle;
class GEText;

// port (R4.3, tenth part): the glyph cells (the original's GEFBCharacter, sizeof 0x2c: texture coordinates, the cell
// rectangle, the referenced texture, the scales) live in the Rust object (EeGeFbCharacter).
class LLE_API GEFontBitmap : public UReferenceCounter {   // sizeof 0x24 (vector deleting dtor 0x106be)
public:
    GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen);
    bool operator==(GEFont* inFont);
    long Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen);
    long Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer, unsigned long inScreen);
    GEFontBitmap(const GEFontBitmap&) = delete;
    GEFontBitmap& operator=(const GEFontBitmap&) = delete;
protected:
    virtual ~GEFontBitmap();                // 0x159ac (protected: released through Dereference)
private:
    // (CreateNewFontTexture 0x100159cd and CreateSurfaceAlpha 0x10015ac5 are the Rust object's)
    // The calls the font bitmap makes on other objects (EeGeFbOp), in the original's statements
    static intptr_t Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b);

    EeGeFontBitmap* mRust;                  // the cells, the drawing bitmap, the first character (ee_ge fbobj.rs)
};

#endif
