// C:\EEX\Low-Level Engine\User Interface\UIScalableButton.h (RECONSTRUCTION). A UIButton whose bitmaps are drawn in
// three horizontal slices (left cap, stretched middle, right cap) so the button can be wider than its art. Not
// exported (RTTI .?AVUIScalableButton@@ only); UIControlsTable::CreateButton creates it. UIScalableButton.cpp
// (0x1007c38e..0x1007c901). Vtable 0x10099ac0 (??_G in slot 0; CopyData, DrawPolygons, SetLocation overridden).
// sizeof 0x144. Requires lle/UIButton.h.
//
// The class is whole in Rust (rust/crates/ee_ui/src/variantobj.rs, R6 ui round 12): its methods forward to the
// Rust button.
#ifndef LLE_UISCALABLEBUTTON_H
#define LLE_UISCALABLEBUTTON_H

#include "lle/UIButton.h"

class UIScalableButton : public UIButton {
public:
    UIScalableButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                     GEBitmap* inPressedBitmap, const GERectangle& inNormalRectangle,
                     const GERectangle& inFocusedRectangle, const GERectangle& inPressedRectangle,
                     const GERectangle& inTextRectangle, const GERectangle& inRectangle, UIForm* inForm,
                     unsigned long inIndex);
    virtual ~UIScalableButton() {}
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual void SetLocation(const GE2DPoint& inLocation);
    // the original's members (+0x104 mNormalRectangle, +0x114 mFocusedRectangle, +0x124 mPressedRectangle: each
    // bitmap's slice rectangle; +0x134 mCopiedRectangle, the current state's as of the last CopyData; sizeof 0x144)
    // are the Rust button's (variantobj.rs)
};

#endif
