// C:\EEX\Low-Level Engine\User Interface\UITiledBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007fb46..0x1007fd7d: ctor, dtor, DrawBackground, DrawTiledBackground. No key chains or COMDATs.
// UITLScrolling.cpp's ctor starts at 0x1007fd7d.
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

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top); guarded: a golden driver includes several
// background units in one translation unit
#ifndef UI_BACKGROUND_RECT_HELPERS
#define UI_BACKGROUND_RECT_HELPERS
static inline EeUiRect UIBgRect(const GERectangle& r) { return EeUiRect{r.mBottom, r.mLeft, r.mRight, r.mTop}; }
static inline GERectangle UIBgRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }
#endif

// FUNCTION: LLE 0x1007fb46
// Creates a tiled bitmap background from a texture file and layout
UITiledBackground::UITiledBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                     const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                                     const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority,
                                     bool inVisible)
    : UIBitmapBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm,
                         inDrawPriority, inVisible)
{
}

// FUNCTION: LLE 0x1007fb76
// Destroys the tiled background (base class only)
UITiledBackground::~UITiledBackground()
{
}

// FUNCTION: LLE 0x1007fb81
// Draws the tiles that cover the part of the given rectangle inside the background; returns 0 when none overlap
long UITiledBackground::DrawBackground(const GERectangle& inRectangle)
{
    EeUiTilePlan thePlan;
    if (!ee_ui_background_tiled_plan(0, 0.0f, 0.0f, UIBgRect(this->mRectangle), UIBgRect(this->mTextureRectangle),
                                     UIBgRect(inRectangle), &thePlan))
        return 0;
    return this->DrawTiledBackground(UIBgRect(thePlan.clip), thePlan.first_row, thePlan.last_row, thePlan.first_column,
                                     thePlan.last_column, thePlan.w, thePlan.h);
}

// FUNCTION: LLE 0x1007fc8d
// Draws each tile of the given row and column range stretched and clipped to the clip rectangle
long UITiledBackground::DrawTiledBackground(const GERectangle& inClip, unsigned long inFirstRow,
                                            unsigned long inLastRow, unsigned long inFirstColumn,
                                            unsigned long inLastColumn, float inWidth, float inHeight)
{
    return ee_ui_background_draw_tiled(this, UIBgRect(this->mRectangle), UIBgRect(inClip), inFirstRow, inLastRow, inFirstColumn,
                                       inLastColumn, inWidth, inHeight);
}
