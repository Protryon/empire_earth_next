// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007c901..0x1007d013: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members are emitted first by UICheckBox.cpp (0x1005f3b0..).
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
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UIScrollBar.h"

// The arrow buttons' size, a literal here: UIScrollBar::SetLocation and the ctor keep it in an FP register across
// two uses, which VC6 does for a literal (__real@4@4003a0.., 0x1009916c) but not for a named const float such as
// kScrollBarWidth (provisional name).
#define kScrollButtonSize 20.0f

// FUNCTION: LLE 0x1007ca21
// Creates a vertical (style 1) or horizontal scroll bar: a slider between two arrow buttons
// Fixed: the rectangle's size was `x + (style == 1) ? 19 : length - 1` (a precedence slip), which made it
// 19 x (length - 1) for both styles; a horizontal bar is now (length - 1) x 19
UIScrollBar::UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle,
                         UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
    : UIControl(kControlTypeScrollBar, inId,
                GERectangle(inLocation, inStyle == 1 ? 19.0f : inLength - 1.0f,
                            inStyle == 1 ? inLength - 1.0f : 19.0f),
                inForm, inIndex, 0)
{
    GERectangle theSliderRectangle;
    GE2DPoint theSliderLocation(inLocation);
    GE2DPoint theIncrementLocation(inLocation);
    GEBitmap* theDecrementNormal;
    GEBitmap* theDecrementPressed;
    GEBitmap* theIncrementNormal;
    GEBitmap* theIncrementPressed;
    this->mSliderPressed = false;
    this->mDecrementPressed = false;
    this->mIncrementPressed = false;
    if (inStyle == 1) {
        theSliderLocation.SetY(inLocation.GetY() + kScrollButtonSize);
        theIncrementLocation.SetY(inLocation.GetY() + inLength - kScrollButtonSize);
        theIncrementNormal = this->mForm->GetManager()->GetSBBitmapDownNormal();
        theDecrementNormal = this->mForm->GetManager()->GetSBBitmapUpNormal();
        theIncrementPressed = this->mForm->GetManager()->GetSBBitmapDownPressed();
        theDecrementPressed = this->mForm->GetManager()->GetSBBitmapUpPressed();
    } else {
        theSliderLocation.SetX(inLocation.GetX() + kScrollButtonSize);
        theIncrementLocation.SetX(inLocation.GetX() + inLength - kScrollButtonSize);
        theIncrementNormal = this->mForm->GetManager()->GetSBBitmapRightNormal();
        theDecrementNormal = this->mForm->GetManager()->GetSBBitmapLeftNormal();
        theIncrementPressed = this->mForm->GetManager()->GetSBBitmapRightPressed();
        theDecrementPressed = this->mForm->GetManager()->GetSBBitmapLeftPressed();
    }
    theSliderRectangle.SetTop(theSliderLocation.GetY());
    theSliderRectangle.SetLeft(theSliderLocation.GetX());
    theSliderRectangle.SetRight(theSliderLocation.GetX() + (inStyle == 1 ? kScrollButtonSize : inLength - 40.0f));
    theSliderRectangle.SetBottom(theSliderLocation.GetY() + (inStyle == 1 ? inLength - 40.0f : kScrollButtonSize));
    this->mSlider = new UISlider(this->GetId(), theSliderRectangle, inStyle, inForm, this->GetIndex(), inBorderThickness);
    this->mDecrementButton = new UIScrollButton(this->GetId(), theDecrementNormal, theDecrementPressed, inLocation, inForm, this->mSlider,
                                          1, this->GetIndex());
    this->mIncrementButton = new UIScrollButton(this->GetId(), theIncrementNormal, theIncrementPressed, theIncrementLocation,
                                          inForm, this->mSlider, 0, this->GetIndex());
}

// FUNCTION: LLE 0x1007cc8b
// Destroys the scroll bar with its slider and arrow buttons
UIScrollBar::~UIScrollBar()
{
    delete this->mSlider;
    delete this->mIncrementButton;
    delete this->mDecrementButton;
}

// FUNCTION: LLE 0x1007cccd
// Snapshots the bar, its arrow buttons and its slider; returns the slider's result
long UIScrollBar::CopyData()
{
    UIControl::CopyData();
    this->mIncrementButton->CopyData();
    this->mDecrementButton->CopyData();
    return this->mSlider->CopyData();
}

// FUNCTION: LLE 0x1007ccf8
// Draws the bar, the slider and the arrow buttons; returns the last result
long UIScrollBar::DrawPolygons()
{
    UIControl::DrawPolygons();
    this->mSlider->DrawPolygons();
    this->mIncrementButton->DrawPolygons();
    return this->mDecrementButton->DrawPolygons();
}

// FUNCTION: LLE 0x1007cd23
// Draws the text of the arrow buttons, the slider and the bar
long UIScrollBar::DrawTextA()
{
    this->mDecrementButton->DrawTextA();
    this->mIncrementButton->DrawTextA();
    this->mSlider->DrawTextA();
    return UIControl::DrawTextA();
}

// FUNCTION: LLE 0x1007cd51
// Moves the slider value by an amount
void UIScrollBar::IncrementBy(long inAmount)
{
    if (this->mSlider)
        this->mSlider->SetCurrentRangeValue(this->mSlider->GetCurrentRangeValue() + inAmount);
}

// FUNCTION: LLE 0x1007cd6e
// Presses the part under the mouse (an arrow button, else the slider) and captures the mouse
long UIScrollBar::MouseLButtonDown()
{
    long theResult;
    if (this->mDecrementButton->GetRectangle().Contains(this->GetMouseLocation())) {
        theResult = this->mDecrementButton->MouseLButtonDown();
        this->mDecrementPressed = true;
    } else if (this->mIncrementButton->GetRectangle().Contains(this->GetMouseLocation())) {
        theResult = this->mIncrementButton->MouseLButtonDown();
        this->mIncrementPressed = true;
    } else {
        theResult = this->mSlider->MouseLButtonDown();
        this->mSliderPressed = true;
    }
    this->CaptureMouse();
    return theResult;
}

// FUNCTION: LLE 0x1007cdf4
// Releases the pressed part and the mouse capture
long UIScrollBar::MouseLButtonUp()
{
    long theResult = kErrorOK;
    if (this->mSliderPressed)
        theResult = this->mSlider->MouseLButtonUp();
    if (this->mIncrementPressed)
        theResult = this->mIncrementButton->MouseLButtonUp();
    if (this->mDecrementPressed)
        theResult = this->mDecrementButton->MouseLButtonUp();
    this->mSliderPressed = false;
    this->mIncrementPressed = false;
    this->mDecrementPressed = false;
    this->ReleaseCapturedMouse();
    return theResult;
}

// FUNCTION: LLE 0x1007ce5f
// Drags the slider when held or under the mouse, and repeats a held arrow button while the mouse stays on it
long UIScrollBar::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mSliderPressed || this->mSlider->GetRectangle().Contains(this->GetMouseLocation()))
        theResult = this->mSlider->MouseMove(inLocation);
    if (this->mIncrementPressed && this->mIncrementButton->GetRectangle().Contains(this->GetMouseLocation()))
        this->mIncrementButton->ButtonDown();
    if (this->mDecrementPressed && this->mDecrementButton->GetRectangle().Contains(this->GetMouseLocation()))
        this->mDecrementButton->ButtonDown();
    return theResult;
}

// FUNCTION: LLE 0x1007cf07
// Moves the scroll bar, laying out its arrow buttons and slider for its style
void UIScrollBar::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    if (this->mSlider->GetSliderStyle() == 1) {
        this->mIncrementButton->SetLocation(GE2DPoint(inLocation.GetX(), this->mRectangle.GetBottom() - kScrollButtonSize));
        this->mSlider->SetRectangle(GERectangle(GE2DPoint(inLocation.GetX(), inLocation.GetY() + kScrollButtonSize),
                                          GE2DPoint(inLocation.GetX() + kScrollButtonSize - 1.0f,
                                                    this->mRectangle.GetBottom() - kScrollButtonSize - 1.0f)));
    } else {
        this->mIncrementButton->SetLocation(GE2DPoint(this->mRectangle.GetRight() - kScrollButtonSize, inLocation.GetY()));
        this->mSlider->SetRectangle(GERectangle(GE2DPoint(inLocation.GetX() + kScrollButtonSize, inLocation.GetY()),
                                          GE2DPoint(this->mRectangle.GetRight() - kScrollButtonSize - 1.0f,
                                                    inLocation.GetY() + kScrollButtonSize - 1.0f)));
    }
    this->mDecrementButton->SetLocation(inLocation);
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007c901
// SYNTHETIC: LLE 0x1007c90b
// SYNTHETIC: LLE 0x1007c91b
// SYNTHETIC: LLE 0x1007c927
// SYNTHETIC: LLE 0x1007c931
// SYNTHETIC: LLE 0x1007c93b
// SYNTHETIC: LLE 0x1007c94b
// SYNTHETIC: LLE 0x1007c957
// SYNTHETIC: LLE 0x1007c961
// SYNTHETIC: LLE 0x1007c96b
// SYNTHETIC: LLE 0x1007c97b
// SYNTHETIC: LLE 0x1007c987
// SYNTHETIC: LLE 0x1007c991
// SYNTHETIC: LLE 0x1007c99b
// SYNTHETIC: LLE 0x1007c9ab
// SYNTHETIC: LLE 0x1007c9b7
// SYNTHETIC: LLE 0x1007c9c1
// SYNTHETIC: LLE 0x1007c9cb
// SYNTHETIC: LLE 0x1007c9db
// SYNTHETIC: LLE 0x1007c9e7
// SYNTHETIC: LLE 0x1007c9f1
// SYNTHETIC: LLE 0x1007c9fb
// SYNTHETIC: LLE 0x1007ca0b
// SYNTHETIC: LLE 0x1007ca17
