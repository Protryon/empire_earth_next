// C:\EEX\Low-Level Engine\User Interface\UITiledBackground.h (RECONSTRUCTION). A bitmap background repeated over the
// rectangle. Vtable 0x10099a0c (UIBitmapBackground's 8 slots, then DrawTiledBackground). Requires
// lle/UIBitmapBackground.h.
//
// Whole in Rust (bgobj.rs, R6 ui round 20). The original's three subclasses UILRTiledBackground (tiled left to
// right), UITBTiledBackground (top to bottom) and UIStretchedTiledBackground (a given number of tiles across and down)
// only chose the tiling of DrawBackground: they are the tiling the constructor takes (the record's mTiling,
// mHorizontalCount, mVerticalCount).
#ifndef LLE_UITILEDBACKGROUND_H
#define LLE_UITILEDBACKGROUND_H

#include "lle/UIBitmapBackground.h"

// UITiledBackground's tiling (bgobj.rs mTiling)
enum UITiling {
    kTilingBoth = 0,        // UITiledBackground
    kTilingLeftRight = 1,   // was UILRTiledBackground
    kTilingTopBottom = 2,   // was UITBTiledBackground
    kTilingStretched = 3,   // was UIStretchedTiledBackground (inAcross x inDown tiles)
};

class LLE_API UITiledBackground : public UIBitmapBackground {
public:
    UITiledBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                      const GETextureDescription& inDescription, const GERectangle& inTextureRectangle,
                      const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, bool inVisible,
                      UITiling inTiling = kTilingBoth, float inAcross = 0.0f, float inDown = 0.0f);
    virtual long DrawBackground(const GERectangle& inRectangle);
    // slot 0x20: draws the tiles [inFirstRow, inLastRow] x [inFirstColumn, inLastColumn] clipped to inClip
    virtual long DrawTiledBackground(const GERectangle& inClip, unsigned long inFirstRow, unsigned long inLastRow,
                                     unsigned long inFirstColumn, unsigned long inLastColumn, float inWidth,
                                     float inHeight);
};

#endif
