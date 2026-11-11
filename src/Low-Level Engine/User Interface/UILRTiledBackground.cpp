// C:\EEX\Low-Level Engine\User Interface\UILRTiledBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10078a70..0x10078c81: UITiledBackground's implicit copy members and ??_E (first includer of UITiledBackground.h),
// then its own implicit copy ctor and ??_E, the ctor, dtor and DrawBackground. No key chains. UIManager.cpp's key
// chains start at 0x10078c81.
#include <windows.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/UIBitmapBackground.h"
#include "lle/UITiledBackground.h"
#include "lle/UILRTiledBackground.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top); guarded: a golden driver includes several
// background units in one translation unit
#ifndef UI_BACKGROUND_RECT_HELPERS
#define UI_BACKGROUND_RECT_HELPERS
static inline EeUiRect UIBgRect(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
static inline GERectangle UIBgRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
#endif

// FUNCTION: LLE 0x10078b83
// Creates a left-to-right tiled background (the texture repeats across its layout rectangle)
UILRTiledBackground::UILRTiledBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                         const GETextureDescription& inDescription,
                                         const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout,
                                         UIForm* inForm, unsigned long inDrawPriority, bool inVisible)
    : UITiledBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                        inDrawPriority, inVisible)
{
}

// FUNCTION: LLE 0x10078bb3
// Destroys the background (the base class does the work)
UILRTiledBackground::~UILRTiledBackground()
{
}

// FUNCTION: LLE 0x10078bbe
// Redraws the tiles of its single row that overlap the rectangle
long UILRTiledBackground::DrawBackground(const GERectangle& inRectangle)
{
    EeUiTilePlan thePlan;
    if (!ee_ui_background_tiled_plan(1, 0.0f, 0.0f, UIBgRect(this->mRectangle), UIBgRect(this->mTextureRectangle),
                                     UIBgRect(inRectangle), &thePlan))
        return 0;
    return this->DrawTiledBackground(UIBgRect(thePlan.clip), thePlan.first_row, thePlan.last_row, thePlan.first_column,
                                     thePlan.last_column, thePlan.w, thePlan.h);
}

// Implicit copy members and ??_E of UITiledBackground and UILRTiledBackground
// FUNCTION: LLE 0x10078a70
// FUNCTION: LLE 0x10078a91
// SYNTHETIC: LLE 0x10078aac
// FUNCTION: LLE 0x10078b07
// SYNTHETIC: LLE 0x10078b28
