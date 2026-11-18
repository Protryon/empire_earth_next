// C:\EEX\Low-Level Engine\User Interface\UIBFull.h (RECONSTRUCTION). A full-form background cut into a grid of
// bitmaps, one texture file per cell (<name>_<row>_<column><ext>). Vtable 0x10098bd4 (UIBackground's 7 slots, then
// SetBitmapTexture); the original's sizeof was 0x84 (??_E stride). Requires lle/UITextureBackground.h.
// Whole in Rust (bgobj.rs, R6 ui round 20): the grid is the record's mBitmaps, mColumns, mRows.
#ifndef LLE_UIBFULL_H
#define LLE_UIBFULL_H

#include "lle/UITextureBackground.h"

class GEBitmap;
class GETexture;

class LLE_API UIBFull : public UITextureBackground {
public:
    UIBFull(unsigned long inBackgroundID, const UString& inTextureFileName, const GETextureDescription& inDescription,
            const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout, UIForm* inForm,
            unsigned long inDrawPriority, unsigned long inColumns, unsigned long inRows, bool inVisible);
    virtual ~UIBFull();
    virtual long DrawBackground(const GERectangle& inRectangle);
    virtual long LoadBackground();
    virtual void SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow);   // slot 0x1c
    void SetNumberColumnsAndRows(unsigned long inColumns, unsigned long inRows);
    virtual long UnloadBackground();
};

#endif
