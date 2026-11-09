// C:\EEX\Low-Level Engine\User Interface\UISlider.h (RECONSTRUCTION). A thumb (the UI drag-bar bitmap) moving along
// a horizontal (style 0) or vertical (style 1) track; the scroll bar's middle part. UISlider.cpp
// (0x1007d139..0x1007db5b) holds the ctor and the out-of-line members; the inline members are emitted first by
// UICheckBox.cpp (0x1005ef4b..0x1005f272, in declaration order). The arithmetic members forward to Rust
// (rust/crates/ee_ui/src/slider.rs): the members from mThumbRectangle on are its pinned record EeUiSliderFields. Vtable 0x10098ddc (UIControl's 34 slots, then
// ButtonDown, ButtonUp, ChangeRange). Layout proven by the implicit copy ctor 0x1005efdc (sizeof 0xf8). Access
// levels from the exports. Requires lle/UIControl.h (includes lle/GEBitmap.h:
// the inline dtor dereferences the thumb bitmap).
#ifndef LLE_UISLIDER_H
#define LLE_UISLIDER_H

#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"      // the inline dtor dereferences the thumb bitmap

class LLE_API UISlider : public UIControl {
    friend class UIScrollBar;               // calls the protected page steps and mouse handlers
    friend struct UIScrollBarRust;          // the Rust scroll bar logic's host calls (User Interface/UIScrollBar.cpp)
public:
    UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
             unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UISlider() { this->mThumbBitmap->Dereference(); }  // 0x1005ef4b
    virtual long CopyData();
    virtual long DrawPolygons();
    // the pixel coordinate of a point along the track (0x1005ef68)
    long GetCurrentPoint(const GE2DPoint& inPoint) const
    {
        return UMFloor(this->mSliderStyle == 1 ? inPoint.GetY() : inPoint.GetX());
    }
    long GetCurrentRangeValue() const { return this->mCurrentRangeValue; }  // 0x1005a13b (ICF)
    bool GetIsDragging() const { return this->mIsDragging; }         // 0x1005ef9d
    void GetLocFromRange(long inRangeValue, GE2DPoint& outLocation) const;
    long GetMaxRange() const { return this->mMaxRange; }                // 0x1005a05f (ICF)
    long GetMinRange() const { return this->mMinRange; }                // 0x1005a066 (ICF)
    unsigned long GetPageSize() const { return this->mPageSize; }       // 0x10045c5f (ICF)
    unsigned long GetRange() const { return this->mRange; }             // 0x1005a0b4 (ICF)
    unsigned long GetRangeFromLoc(const GE2DPoint& inLocation) const;
    unsigned long GetSliderStyle() const { return this->mSliderStyle; }  // 0x1005a12d (ICF)
    virtual bool GetTabable() const { return false; }                   // 0x1005efa4
    const GERectangle& GetThumbRectangle() const { return this->mThumbRectangle; }  // 0x100633fc (ICF)
    // 0x1005efa7
    void SetCurrentPoint(long inPoint, GE2DPoint& outLocation) const
    {
        if (this->mSliderStyle == 1)
            outLocation.SetY((float)inPoint);
        else
            outLocation.SetX((float)inPoint);
    }
    void SetCurrentRangeValue(long inValue);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetPageSize(unsigned long inPageSize) { this->mPageSize = inPageSize; }  // 0x1005efc2
    void SetRange(unsigned long inMin, unsigned long inMax);
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual void ButtonDown() {}            // slot 0x88 (0x1000c16a)
    virtual void ButtonUp();                // slot 0x8c
    virtual void ChangeRange(unsigned long inValue);   // slot 0x90
    void DecrementByPageSize();
    void IncrementByPageSize();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetIsDragging(bool inDragging) { this->mIsDragging = inDragging; }  // 0x1005efcf
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}

    GERectangle mThumbRectangle;            // +0xa0
    GERectangle mCopiedThumbRectangle;      // +0xb0 (CopyData)
    GEBitmap* mThumbBitmap;                 // +0xc0 (the manager's drag bar, scaled; referenced)
    unsigned long mSliderStyle;             // +0xc4 0 horizontal, 1 vertical
    float mPixelsPerUnit;                   // +0xc8
    float mUnitsPerPixel;                   // +0xcc
    long mCurrentRangeValue;                // +0xd0
    unsigned long mPageSize;                // +0xd4
    long mRange;                            // +0xd8 (fild dword: signed)
    long mMinRange;                         // +0xdc
    long mMaxRange;                         // +0xe0
    long mMinPixel;                         // +0xe4
    long mMaxPixel;                         // +0xe8
    long mMaxThumbPixel;                    // +0xec
    long mDragOffset;                       // +0xf0
    bool mIsDragging;                       // +0xf4
};

#endif
