// C:\EEX\Low-Level Engine\User Interface\UIBFull.h (RECONSTRUCTION). A full-form background cut into a grid of
// bitmaps, one texture file per cell (<name>_<row>_<column><ext>). UIBFull.cpp (0x1005c84e..0x1005d3a1) emits
// UITextureBackground's and its own inline members first. Vtable 0x10098bd4 (UIBackground's 7 slots, then
// SetBitmapTexture). sizeof 0x84 (??_E stride). Requires lle/UITextureBackground.h.
#ifndef LLE_UIBFULL_H
#define LLE_UIBFULL_H

#include "lle/UITextureBackground.h"

class GEBitmap;
class GETexture;

class LLE_API UIBFull : public UITextureBackground {
    friend struct UIBFullRust;              // UIBFull.cpp: a Rust-placed tile's draw (ee_ui background.rs)
public:
    UIBFull(unsigned long inBackgroundID, const UString& inTextureFileName, const GETextureDescription& inDescription,
            const GERectangle& inTextureRectangle, const UILayoutUtility& inLayout, UIForm* inForm,
            unsigned long inDrawPriority, unsigned long inColumns, unsigned long inRows, bool inVisible);
    virtual ~UIBFull();
    virtual long DrawBackground(const GERectangle& inRectangle);
    GEBitmap* GetBackgroundBitmap(unsigned long inColumn, unsigned long inRow) const
        { return this->mBitmaps[this->mColumns * inRow + inColumn]; }
    unsigned long GetNumberColumns() const { return this->mColumns; }
    unsigned long GetNumberRows() const { return this->mRows; }
    virtual long LoadBackground();
    virtual void SetBitmapTexture(GETexture* inTexture, unsigned long inColumn, unsigned long inRow);   // slot 0x1c
    void SetNumberColumnsAndRows(unsigned long inColumns, unsigned long inRows);
    virtual long UnloadBackground();
protected:
    GEBitmap** mBitmaps;                    // +0x78 mColumns * mRows, row major
    unsigned long mColumns;                 // +0x7c
    unsigned long mRows;                    // +0x80
};

#endif
