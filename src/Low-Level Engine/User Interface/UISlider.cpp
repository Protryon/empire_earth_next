// C:\EEX\Low-Level Engine\User Interface\UISlider.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007d139..0x1007db5b: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members are emitted first by UICheckBox.cpp (0x1005ef4b..).
//
// The slider's arithmetic is Rust (rust/crates/ee_ui/src/slider.rs, docs/RUST_PLAN.md R6): the members below forward
// to it with the object's own members from mThumbRectangle on as the pinned record EeUiSliderFields. What stays here
// touches other objects: the base class, the thumb bitmap, the mouse capture, ChangeRange and drawing; the
// ee_uicpp_slider_* functions are Rust's way back into the virtual members.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"

// The slider seen from the Rust host calls: the protected members as public (a view of the same object, no data)
struct UISliderAccess : UISlider {
    using UISlider::ChangeRange;
    EeUiSliderFields* Fields() { return reinterpret_cast<EeUiSliderFields*>(&this->mThumbRectangle); }
    const EeUiRect* Rectangle() const { return reinterpret_cast<const EeUiRect*>(&this->mRectangle); }
    static void CheckLayout()
    {
        // the record's offsets from mThumbRectangle (ee_ui/src/slider.rs SliderFields)
#define EE_UI_SLIDER_FIELD(member, field)                                                                             \
    static_assert(offsetof(UISliderAccess, member) - offsetof(UISliderAccess, mThumbRectangle) ==                    \
                      offsetof(EeUiSliderFields, field),                                                              \
                  "ee_ui/src/slider.rs")
        EE_UI_SLIDER_FIELD(mCopiedThumbRectangle, copied_thumb);
        EE_UI_SLIDER_FIELD(mThumbBitmap, thumb_bitmap);
        EE_UI_SLIDER_FIELD(mSliderStyle, style);
        EE_UI_SLIDER_FIELD(mPixelsPerUnit, pixels_per_unit);
        EE_UI_SLIDER_FIELD(mUnitsPerPixel, units_per_pixel);
        EE_UI_SLIDER_FIELD(mCurrentRangeValue, current);
        EE_UI_SLIDER_FIELD(mPageSize, page_size);
        EE_UI_SLIDER_FIELD(mRange, range);
        EE_UI_SLIDER_FIELD(mMinRange, min_range);
        EE_UI_SLIDER_FIELD(mMaxRange, max_range);
        EE_UI_SLIDER_FIELD(mMinPixel, min_pixel);
        EE_UI_SLIDER_FIELD(mMaxPixel, max_pixel);
        EE_UI_SLIDER_FIELD(mMaxThumbPixel, max_thumb_pixel);
        EE_UI_SLIDER_FIELD(mDragOffset, drag_offset);
        EE_UI_SLIDER_FIELD(mIsDragging, is_dragging);
#undef EE_UI_SLIDER_FIELD
        static_assert(sizeof(UISliderAccess) - offsetof(UISliderAccess, mThumbRectangle) >= sizeof(EeUiSliderFields),
                      "ee_ui/src/slider.rs");
    }
};

static UISliderAccess* Access(UISlider* inSlider) { return static_cast<UISliderAccess*>(inSlider); }
static const UISliderAccess* Access(const UISlider* inSlider) { return static_cast<const UISliderAccess*>(inSlider); }
static EeUiPoint UIPoint(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }

// The host calls of ee_ui/src/slider_ffi.rs
extern "C" void ee_uicpp_slider_set_dirty(void* inSlider)
{
    static_cast<UISlider*>(inSlider)->SetDirty(true);
}

extern "C" void ee_uicpp_slider_change_range(void* inSlider, unsigned long inValue)
{
    Access(static_cast<UISlider*>(inSlider))->ChangeRange(inValue);
}

extern "C" void ee_uicpp_slider_base_set_location(void* inSlider, float inX, float inY)
{
    static_cast<UISlider*>(inSlider)->UIControl::SetLocation(GE2DPoint(inX, inY));
}

// FUNCTION: LLE 0x1007d259
// Creates a horizontal (style 0) or vertical slider with a square thumb, an empty range and no page size
UISlider::UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
                   unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeSlider, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    UISliderAccess::CheckLayout();
    this->mBorderType = 0;
    // Fixed (in the C++ before the move): the horizontal track lacked the + 1 the vertical case and SetLocation add
    ee_ui_slider_init(Access(this)->Fields(), Access(this)->Rectangle(), inSliderStyle);
    this->mThumbBitmap = inForm->GetManager()->CreateBitmapForScaledControl(inForm->GetManager()->GetDragBarBitmap(),
                                                                            this->mThumbRectangle);
    this->SetRange(0, 0);
    this->mPageSize = 0;
}

// FUNCTION: LLE 0x1007d3fa
// Ends a thumb drag, or pages toward a click on the track outside the thumb
void UISlider::ButtonUp()
{
    ee_ui_slider_button_up(this, Access(this)->Fields(), Access(this)->Rectangle(), UIPoint(this->GetMouseLocation()));
}

// FUNCTION: LLE 0x1007d468
// Sets the current value and fires the range-changed trigger
void UISlider::ChangeRange(unsigned long inValue)
{
    this->mCurrentRangeValue = inValue;
    this->FireTriggerEvent(kControlTriggerRangeChanged);
}

// FUNCTION: LLE 0x1007d47f
// Snapshots the slider and its thumb rectangle
long UISlider::CopyData()
{
    long theResult = UIControl::CopyData();
    this->mCopiedThumbRectangle = this->mThumbRectangle;
    return theResult;
}

// FUNCTION: LLE 0x1007d4ab
// Moves the value down by a page, stopping at the minimum
void UISlider::DecrementByPageSize()
{
    ee_ui_slider_decrement_by_page_size(this, Access(this)->Fields(), Access(this)->Rectangle());
}

// FUNCTION: LLE 0x1007d4cb
// Draws the track and the thumb
long UISlider::DrawPolygons()
{
    UIControl::DrawPolygons();
    this->mThumbBitmap->SetLocation(this->mCopiedThumbRectangle.GetTopLeft());
    return this->GetRasterizer()->Draw(this->mThumbBitmap, this->GetScreenIndex());
}

// FUNCTION: LLE 0x1007d520
// Returns the thumb position for a value along the slider's axis
void UISlider::GetLocFromRange(long inRangeValue, GE2DPoint& outLocation) const
{
    EeUiPoint thePoint = UIPoint(outLocation);
    ee_ui_slider_loc_from_range(const_cast<UISliderAccess*>(Access(this))->Fields(), Access(this)->Rectangle(),
                                inRangeValue, &thePoint);
    outLocation = GE2DPoint(thePoint.x, thePoint.y);
}

// FUNCTION: LLE 0x1007d588
// Returns the value for a thumb position (exactly the minimum and maximum at the track ends)
unsigned long UISlider::GetRangeFromLoc(const GE2DPoint& inLocation) const
{
    return ee_ui_slider_range_from_loc(const_cast<UISliderAccess*>(Access(this))->Fields(), UIPoint(inLocation));
}

// FUNCTION: LLE 0x1007d60b
// Moves the value up by a page, stopping at the maximum
void UISlider::IncrementByPageSize()
{
    ee_ui_slider_increment_by_page_size(this, Access(this)->Fields(), Access(this)->Rectangle());
}

// FUNCTION: LLE 0x1007d62d
// Captures the mouse and starts a thumb drag when pressed on the thumb; returns OK
long UISlider::MouseLButtonDown()
{
    this->CaptureMouse();
    ee_ui_slider_press(Access(this)->Fields(), UIPoint(this->GetMouseLocation()));
    this->ButtonDown();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007d6d7
// Releases the mouse capture and finishes the press (drag end or page step); returns OK
long UISlider::MouseLButtonUp()
{
    this->ReleaseCapturedMouse();
    this->ButtonUp();
    return kErrorOK;
}

// FUNCTION: LLE 0x1007d6ed
// Drags the thumb with the mouse within the track, updating the value; returns OK
long UISlider::MouseMove(const GE2DPoint& inLocation)
{
    ee_ui_slider_mouse_move(this, Access(this)->Fields(), Access(this)->Rectangle(), UIPoint(inLocation));
    return kErrorOK;
}

// FUNCTION: LLE 0x1007d819
// Sets the value, clamped to the range, and moves the thumb to match
void UISlider::SetCurrentRangeValue(long inValue)
{
    ee_ui_slider_set_current_range_value(this, Access(this)->Fields(), Access(this)->Rectangle(), inValue);
}

// FUNCTION: LLE 0x1007d920
// Moves the slider, recomputing its pixel range, thumb position and thumb scale
void UISlider::SetLocation(const GE2DPoint& inLocation)
{
    float theThickness = ee_ui_slider_set_location(this, Access(this)->Fields(), Access(this)->Rectangle(),
                                                   UIPoint(inLocation));
    GEBitmap* theDragBar = this->mForm->GetManager()->GetDragBarBitmap();
    this->mThumbBitmap->SetXYScale(theDragBar->GetXScale() * theThickness * 0.05f,
                                   theDragBar->GetYScale() * theThickness * 0.05f);
}

// FUNCTION: LLE 0x1007dacb
// Sets the value range, recomputing the pixel scale and clamping the current value
void UISlider::SetRange(unsigned long inMin, unsigned long inMax)
{
    ee_ui_slider_set_range(this, Access(this)->Fields(), Access(this)->Rectangle(), inMin, inMax);
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007d139
// SYNTHETIC: LLE 0x1007d143
// SYNTHETIC: LLE 0x1007d153
// SYNTHETIC: LLE 0x1007d15f
// SYNTHETIC: LLE 0x1007d169
// SYNTHETIC: LLE 0x1007d173
// SYNTHETIC: LLE 0x1007d183
// SYNTHETIC: LLE 0x1007d18f
// SYNTHETIC: LLE 0x1007d199
// SYNTHETIC: LLE 0x1007d1a3
// SYNTHETIC: LLE 0x1007d1b3
// SYNTHETIC: LLE 0x1007d1bf
// SYNTHETIC: LLE 0x1007d1c9
// SYNTHETIC: LLE 0x1007d1d3
// SYNTHETIC: LLE 0x1007d1e3
// SYNTHETIC: LLE 0x1007d1ef
// SYNTHETIC: LLE 0x1007d1f9
// SYNTHETIC: LLE 0x1007d203
// SYNTHETIC: LLE 0x1007d213
// SYNTHETIC: LLE 0x1007d21f
// SYNTHETIC: LLE 0x1007d229
// SYNTHETIC: LLE 0x1007d233
// SYNTHETIC: LLE 0x1007d243
// SYNTHETIC: LLE 0x1007d24f
