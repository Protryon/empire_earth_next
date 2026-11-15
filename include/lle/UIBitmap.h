// C:\EEX\Low-Level Engine\User Interface\UIBitmap.h (RECONSTRUCTION). A control that draws one GEBitmap at its
// top left corner. UIBitmap.cpp (0x1005d3a1..0x1005d72a) emits the inline members first, then the U2D key chains,
// the ctor and the out-of-line members. Vtable 0x10098bfc. sizeof 0xb0 (??_E stride, 0x1005d46c).
// Requires lle/UIControl.h, lle/GEBitmap.h.
#ifndef LLE_UIBITMAP_H
#define LLE_UIBITMAP_H

#include "lle/UIControl.h"

class LLE_API UIBitmap : public UIControl {
public:
    UIBitmap(unsigned long inId, GEBitmap* inBitmap, const GERectangle& inRectangle, UIForm* inForm,
             unsigned long inIndex);
    virtual ~UIBitmap();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    GEBitmap* GetBitmap() const { return this->mBitmap; }
    void SetBitmap(GEBitmap* inBitmap)      // 0x1005d3a1
    {
        if (this->mBitmap)
            this->mBitmap->Dereference();
        this->mBitmap = inBitmap;
        inBitmap->Reference();
        this->SetDirty(true);
    }
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}

    GE2DPoint mLocation;                    // +0xa0 mRectangle's top left as of the last CopyData
    GEBitmap* mBitmap;                      // +0xa8 (referenced)
    GEBitmap* mCopiedBitmap;                // +0xac
};

#endif
