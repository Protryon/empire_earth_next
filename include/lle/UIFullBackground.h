// C:\EEX\Low-Level Engine\User Interface\UIFullBackground.h (RECONSTRUCTION). A full-screen background cut into a
// grid of bitmaps, one texture file per cell (<name>_<row>_<column><ext>), scaled from a designed size to the screen.
// A root class (RTTI: no base). Vtable 0x100999e8: ??_E, LoadBackground, DrawBackground, UnloadBackground; the
// original's sizeof was 0x2c (??_E stride).
// Whole in Rust (bgobj.rs, R6 ui round 20): its members are the backgrounds' record (the file name, form, grid and
// designed size); UIBackground.cpp forwards.
#ifndef LLE_UIFULLBACKGROUND_H
#define LLE_UIFULLBACKGROUND_H

#include "rust/ui/bgobj.h"

class GERectangle;
class UIForm;
struct GETextureDescription;

class LLE_API UIFullBackground : protected EeUiBackgroundFields {
public:
    UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows, UIForm* inForm,
                     unsigned long inDesignedWidth, unsigned long inDesignedHeight);
    UIFullBackground(const UIFullBackground&) = delete;
    UIFullBackground& operator=(const UIFullBackground&) = delete;
    virtual ~UIFullBackground();
    virtual long LoadBackground(const GETextureDescription& inDescription);
    virtual long DrawBackground(const GERectangle& inRectangle);
    virtual long UnloadBackground();
};

#endif
