// C:\EEX\Low-Level Engine\User Interface\UIBFull.h (RECONSTRUCTION). A full-form background cut into a grid of
// bitmaps, one texture file per cell (<name>_<row>_<column><ext>). Vtable 0x10098bd4 (UIBackground's 7 slots, then
// SetBitmapTexture); the original's sizeof was 0x84 (??_E stride). Requires lle/UIBitmapBackground.h.
// Whole in Rust (bgobj.rs, R6 ui round 20): the grid is the record's mBitmaps, mColumns, mRows.
#ifndef LLE_UIBFULL_H
#define LLE_UIBFULL_H

#include "lle/UIBitmapBackground.h"   // UITextureBackground


class LLE_API UIBFull : public UITextureBackground {
public:
    UIBFull(unsigned long inBackgroundID, const UString& inTextureFileName, const GETextureDescription& inDescription, const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout, UIForm* inForm, unsigned long inDrawPriority, unsigned long inColumns, unsigned long inRows, bool inVisible) : UITextureBackground(inBackgroundID, inTextureFileName, inDescription, inTextureRectangle, inLayout, inForm, inDrawPriority, inVisible) { this->Call(EeUiBgCall::BFullInit, inColumns, inRows); }
    virtual ~UIBFull() { this->Call(EeUiBgCall::BFullRelease); }
    virtual long DrawBackground(const GERectangle& inRectangle) { return (long)this->CallRect(EeUiBgCall::BFullDraw, inRectangle); }
    virtual long LoadBackground() { return (long)this->Call(EeUiBgCall::BFullLoad); }
    virtual void SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow) { this->Call(EeUiBgCall::BFullSetBitmapTexture, (intptr_t)inTexture, inColumn, UIF4(UIF4::Bits((int32_t)inRow))); } // slot 0x1c
    void SetNumberColumnsAndRows(unsigned long inColumns, unsigned long inRows) { this->Call(EeUiBgCall::BFullSetGrid, inColumns, inRows); }
    virtual long UnloadBackground() { return (long)this->Call(EeUiBgCall::BFullUnload); }
};

#endif
