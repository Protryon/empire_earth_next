// C:\EEX\Low-Level Engine\User Interface\UISlider.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007d139..0x1007db5b: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members are emitted first by UICheckBox.cpp (0x1005ef4b..).
#include <windows.h>

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

// FUNCTION: LLE 0x1007d259
// Creates a horizontal (style 0) or vertical slider with a square thumb, an empty range and no page size
UISlider::UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
                   unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeSlider, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mSliderStyle = inSliderStyle;
    this->mIsDragging = false;
    this->mCurrentRangeValue = 0;
    this->mDragOffset = 0;
    this->mBorderType = 0;
    float theThickness;
    if (this->mSliderStyle == 0) {
        theThickness = inRectangle.GetHeight();
        this->mMinPixel = UMFloor(inRectangle.GetLeft());
        // Fixed: lacked the + 1 that the vertical case and SetLocation add (the track was a pixel short)
        this->mMaxPixel = UMFloor(inRectangle.GetRight()) + 1;
    } else {
        theThickness = inRectangle.GetWidth();
        this->mMinPixel = UMFloor(inRectangle.GetTop());
        this->mMaxPixel = UMFloor(this->mRectangle.GetBottom()) + 1;
    }
    this->mMaxThumbPixel = UMFloor(this->mMaxPixel - theThickness);
    float theRight = this->mRectangle.GetLeft() + theThickness;
    float theBottom = this->mRectangle.GetTop() + theThickness;
    this->mThumbRectangle = GERectangle(this->mRectangle.GetLeft(), this->mRectangle.GetTop(), theRight, theBottom);
    this->mThumbBitmap = inForm->GetManager()->CreateBitmapForScaledControl(inForm->GetManager()->GetDragBarBitmap(),
                                                                      this->mThumbRectangle);
    this->SetRange(0, 0);
    this->mPageSize = 0;
}

// FUNCTION: LLE 0x1007d3fa
// Ends a thumb drag, or pages toward a click on the track outside the thumb
void UISlider::ButtonUp()
{
    if (this->mIsDragging) {
        this->mIsDragging = false;
        return;
    }
    if (!this->mRectangle.Contains(this->GetMouseLocation()))
        return;
    if (this->mSliderStyle == 1) {
        if (this->GetMouseLocation().GetY() < this->mThumbRectangle.GetTop())
            this->DecrementByPageSize();
        else
            this->IncrementByPageSize();
    } else {
        if (this->GetMouseLocation().GetX() < this->mThumbRectangle.GetLeft())
            this->DecrementByPageSize();
        else
            this->IncrementByPageSize();
    }
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
    long theValue = this->mCurrentRangeValue - this->mPageSize;
    if (theValue <= this->mMinRange)
        this->SetCurrentRangeValue(this->mMinRange);
    else
        this->SetCurrentRangeValue(theValue);
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
    this->SetCurrentPoint(UMFloor((inRangeValue - this->mMinRange) * this->mPixelsPerUnit + this->mMinPixel), outLocation);
    if (this->mSliderStyle == 0)
        outLocation.SetY(this->mRectangle.GetTop());
    else
        outLocation.SetX(this->mRectangle.GetLeft());
}

// FUNCTION: LLE 0x1007d588
// Returns the value for a thumb position (exactly the minimum and maximum at the track ends)
unsigned long UISlider::GetRangeFromLoc(const GE2DPoint& inLocation) const
{
    long thePoint = this->GetCurrentPoint(inLocation);
    float theValue;
    if (thePoint == this->mMaxThumbPixel)
        theValue = this->mMaxRange;
    else if (thePoint == this->mMinPixel)
        theValue = this->mMinRange;
    else
        theValue = (thePoint - this->mMinPixel) * this->mUnitsPerPixel + this->mMinRange;
    return UMFloor(theValue);
}

// FUNCTION: LLE 0x1007d60b
// Moves the value up by a page, stopping at the maximum
void UISlider::IncrementByPageSize()
{
    if ((long)(this->mCurrentRangeValue + this->mPageSize) >= this->mMaxRange)
        this->SetCurrentRangeValue(this->mMaxRange);
    else
        this->SetCurrentRangeValue(this->mCurrentRangeValue + this->mPageSize);
}

// FUNCTION: LLE 0x1007d62d
// Captures the mouse and starts a thumb drag when pressed on the thumb; returns OK
long UISlider::MouseLButtonDown()
{
    this->CaptureMouse();
    if (this->mThumbRectangle.Contains(this->GetMouseLocation())) {
        this->mIsDragging = true;
        this->mDragOffset = this->GetCurrentPoint(this->GetMouseLocation()) - this->GetCurrentPoint(this->mThumbRectangle.GetTopLeft());
    }
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
    GE2DPoint theLocation = this->mThumbRectangle.GetTopLeft();
    long thePoint = this->GetCurrentPoint(inLocation) - this->mDragOffset;
    if (this->mIsDragging && this->mMaxThumbPixel >= this->mMinPixel) {
        if (thePoint < this->mMinPixel)
            thePoint = this->mMinPixel;
        else if (thePoint > this->mMaxThumbPixel)
            thePoint = this->mMaxThumbPixel;
        this->SetCurrentPoint(thePoint, theLocation);
        float theRight = theLocation.GetX() + this->mThumbRectangle.GetWidth();
        float theBottom = theLocation.GetY() + this->mThumbRectangle.GetHeight();
        this->mThumbRectangle = GERectangle(theLocation.GetX(), theLocation.GetY(), theRight, theBottom);
        this->ChangeRange(this->GetRangeFromLoc(this->mThumbRectangle.GetTopLeft()));
        this->SetDirty(true);
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1007d819
// Sets the value, clamped to the range, and moves the thumb to match
void UISlider::SetCurrentRangeValue(long inValue)
{
    GE2DPoint theLocation = this->mThumbRectangle.GetTopLeft();
    if (inValue < this->GetMinRange())
        inValue = this->GetMinRange();
    else if (inValue > this->GetMaxRange())
        inValue = this->GetMaxRange();
    if (this->GetCurrentRangeValue() == inValue)
        return;
    this->SetDirty(true);
    this->ChangeRange(inValue);
    if (this->GetCurrentRangeValue() == this->GetMinRange())
        this->SetCurrentPoint(this->mMinPixel, theLocation);
    else if (this->GetCurrentRangeValue() == this->GetMaxRange())
        this->SetCurrentPoint(this->mMaxThumbPixel, theLocation);
    else
        this->GetLocFromRange(this->GetCurrentRangeValue(), theLocation);
    float theRight = theLocation.GetX() + this->mThumbRectangle.GetWidth();
    float theBottom = theLocation.GetY() + this->mThumbRectangle.GetHeight();
    this->mThumbRectangle = GERectangle(theLocation.GetX(), theLocation.GetY(), theRight, theBottom);
}

// FUNCTION: LLE 0x1007d920
// Moves the slider, recomputing its pixel range, thumb position and thumb scale
void UISlider::SetLocation(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation;
    UIControl::SetLocation(inLocation);
    float theThickness;
    if (this->mSliderStyle == 0) {
        theThickness = this->mRectangle.GetHeight();
        this->mMinPixel = UMFloor(inLocation.GetX());
        this->mMaxPixel = UMFloor(this->mRectangle.GetRight()) + 1;
    } else {
        theThickness = this->mRectangle.GetWidth();
        this->mMinPixel = UMFloor(inLocation.GetY());
        this->mMaxPixel = UMFloor(this->mRectangle.GetBottom()) + 1;
    }
    this->mMaxThumbPixel = UMFloor(this->mMaxPixel - theThickness);
    long thePixelRange = this->mMaxThumbPixel - this->mMinPixel;
    long theRange = this->mRange;
    if (theRange != 0) {
        this->mPixelsPerUnit = (float)thePixelRange / theRange;
        this->mUnitsPerPixel = (float)theRange / thePixelRange;
    } else {
        this->mPixelsPerUnit = 0.0f;
        this->mUnitsPerPixel = 0.0f;
    }
    this->GetLocFromRange(this->mCurrentRangeValue, theLocation);
    float theRight = theLocation.GetX() + theThickness;
    float theBottom = theLocation.GetY() + theThickness;
    this->mThumbRectangle = GERectangle(theLocation.GetX(), theLocation.GetY(), theRight, theBottom);
    GEBitmap* theDragBar = this->mForm->GetManager()->GetDragBarBitmap();
    this->mThumbBitmap->SetXYScale(theDragBar->GetXScale() * theThickness * 0.05f,
                             theDragBar->GetYScale() * theThickness * 0.05f);
}

// FUNCTION: LLE 0x1007dacb
// Sets the value range, recomputing the pixel scale and clamping the current value
void UISlider::SetRange(unsigned long inMin, unsigned long inMax)
{
    if (inMin == this->mMinRange && inMax == this->mMaxRange)
        return;
    long thePixelRange = this->mMaxThumbPixel - this->mMinPixel;
    this->mMinRange = inMin;
    this->mMaxRange = inMax;
    this->mRange = inMax - inMin;
    if (this->mRange != 0) {
        this->mPixelsPerUnit = (float)thePixelRange / this->mRange;
        this->mUnitsPerPixel = (float)this->mRange / thePixelRange;
    } else {
        this->mPixelsPerUnit = 0.0f;
        this->mUnitsPerPixel = 0.0f;
    }
    if (this->mCurrentRangeValue < (long)inMin)
        this->SetCurrentRangeValue(inMin);
    else if (this->mCurrentRangeValue > (long)inMax)
        this->SetCurrentRangeValue(inMax);
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
