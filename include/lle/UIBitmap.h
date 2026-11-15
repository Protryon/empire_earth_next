// C:\EEX\Low-Level Engine\User Interface\UIBitmap.h (RECONSTRUCTION). A control that draws one GEBitmap at its
// top left corner. Vtable 0x10098bfc. The original's sizeof was 0xb0 (??_E stride, 0x1005d46c).
//
// Whole in Rust (rust/crates/ee_ui/src/mediaobj.rs, R6 ui round 21): the members are EeUiBitmapFields, the class's
// private base; every method forwards there (UIBitmap.cpp, with UIMovie's), and the Rust record calls out through
// UIMediaHost. Requires lle/UIControl.h.
#ifndef LLE_UIBITMAP_H
#define LLE_UIBITMAP_H

#include "lle/UIControl.h"

class LLE_API UIBitmap : public UIControl, private EeUiBitmapFields {
    friend struct UIMediaHost;              // UIBitmap.cpp: the Rust record's calls out
public:
    UIBitmap(unsigned long inId, GEBitmap* inBitmap, const GERectangle& inRectangle, UIForm* inForm,
             unsigned long inIndex);
    UIBitmap(const UIBitmap&) = delete;
    UIBitmap& operator=(const UIBitmap&) = delete;
    virtual ~UIBitmap();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    GEBitmap* GetBitmap() const { return static_cast<GEBitmap*>(this->mBitmap); }
    void SetBitmap(GEBitmap* inBitmap);
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
};

#endif
