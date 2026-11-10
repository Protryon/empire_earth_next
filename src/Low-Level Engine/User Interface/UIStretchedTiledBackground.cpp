// C:\EEX\Low-Level Engine\User Interface\UIStretchedTiledBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007e312..0x1007e47c: ctor, dtor, DrawBackground. No key chains or COMDATs.
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
#include "lle/UIStretchedTiledBackground.h"

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top); guarded: a golden driver includes several
// background units in one translation unit
#ifndef UI_BACKGROUND_RECT_HELPERS
#define UI_BACKGROUND_RECT_HELPERS
static inline EeUiRect UIBgRect(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
static inline GERectangle UIBgRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
#endif

// FUNCTION: LLE 0x1007e312
// Creates a background tiled a given number of times across and down, stretching the tiles to fit
UIStretchedTiledBackground::UIStretchedTiledBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                                       const GETextureDescription& inDescription,
                                                       const GERectangle& inTextureRectangle, float inHorizontalCount,
                                                       float inVerticalCount, const UILayoutUtility& inLayout,
                                                       UIForm* inForm, unsigned long inDrawPriority, bool inVisible)
    : UITiledBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                        inDrawPriority, inVisible)
{
    this->mVerticalCount = inVerticalCount;
    this->mHorizontalCount = inHorizontalCount;
}

// FUNCTION: LLE 0x1007e351
// Destroys the background (members only)
UIStretchedTiledBackground::~UIStretchedTiledBackground()
{
}

// FUNCTION: LLE 0x1007e35c
// Draws the stretched tiles that overlap the given rectangle; returns the draw result (0 when outside)
long UIStretchedTiledBackground::DrawBackground(const GERectangle& inRectangle)
{
    EeUiTilePlan thePlan;
    if (!ee_ui_background_tiled_plan(3, this->mHorizontalCount, this->mVerticalCount, UIBgRect(this->mRectangle), UIBgRect(this->mTextureRectangle),
                                     UIBgRect(inRectangle), &thePlan))
        return 0;
    return this->DrawTiledBackground(UIBgRect(thePlan.clip), thePlan.first_row, thePlan.last_row, thePlan.first_column,
                                     thePlan.last_column, thePlan.w, thePlan.h);
}
