// C:\EEX\Low-Level Engine\User Interface\UICheckBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005ef4b..0x100602e4: the header-inline members (and implicit copy members, ??_E) of the classes this unit is the
// first to include, in include order: UISlider, UIScrollButton, UIScrollBar, UITextLabel, UICheckBox; then the U2D
// key init chains (0x1005fb21, the rasterizer header comes next), UIRadioButton's inline members (0x1005fc41); then
// its own code (0x1005fdef: the ctor, then the members) and the vector<UWideString*>::_Tidy COMDAT.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/UIRadioButton.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

// FUNCTION: LLE 0x1005fdef
// Creates an unchecked check box with an optional text label to the right of the box bitmap
UICheckBox::UICheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                       UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inScaleBitmap)
    : UIButton(inId, inForm->GetManager()->GetCheckOff(), NULL, inForm->GetManager()->GetCheckDownOff(),
               GERectangle(inRectangle.GetTopLeft(),
                           GE2DPoint(!inScaleBitmap && inRectangle.GetWidth() <= kScrollBarWidth
                                         ? inRectangle.GetLeft() + 19.0f
                                         : inRectangle.GetRight(),
                                     !inScaleBitmap && inRectangle.GetHeight() <= kScrollBarWidth
                                         ? inRectangle.GetTop() + 19.0f
                                         : inRectangle.GetBottom())),
               inForm, inIndex, inBorderThickness)
{
    this->mIsChecked = false;
    this->mIsDisabled = false;
    this->mTextLabel = NULL;
    this->mHasText = false;
    this->mScaleBitmap = inScaleBitmap;
    if (!this->mScaleBitmap) {
        float theHeight = inRectangle.GetHeight();
        if (theHeight > kScrollBarWidth)
            this->SetBitmapDrawOffset(GE2DPoint(0.0f, (theHeight - 2.0f * inBorderThickness) * 0.5f - 10.0f));
    }
    this->SetText(inText, inFont);
}

// Resizes the check box and its label to a new width. No marker: ICF-folded into UITriStateCheckBox::ChangeWidth
// (0x10080916), the same body over the same offsets.
void UICheckBox::ChangeWidth(unsigned long inWidth)
{
    GERectangle theRectangle;
    if (this->mTextLabel) {
        theRectangle = this->mTextLabel->GetRectangle();
        theRectangle.SetRight(theRectangle.GetLeft() + inWidth - kScrollBarWidth);
        this->mTextLabel->SetRectangle(theRectangle);
    }
    theRectangle = this->mRectangle;
    theRectangle.SetRight(theRectangle.GetLeft() + inWidth - 1.0f);
    UIControl::SetRectangle(theRectangle);
}

// FUNCTION: LLE 0x1005ff2c
// Copies the button and its label for the draw pass, deleting the label once the text was cleared
long UICheckBox::CopyData()
{
    long theResult = UIButton::CopyData();
    if (this->mHasText)
        theResult = this->mTextLabel->CopyData();
    else if (this->mTextLabel) {
        delete this->mTextLabel;
        this->mTextLabel = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x1005ff6d
// Draws the box bitmap (scaled to the control height when scaling) and the label
long UICheckBox::DrawPolygons()
{
    long theResult;
    if (this->mScaleBitmap && this->mCopiedBitmap) {
        float theXScale = this->mCopiedBitmap->GetXScale();
        float theYScale = this->mCopiedBitmap->GetYScale();
        float theBorder = 2 * this->GetBorderThickness();
        this->mCopiedBitmap->SetXYScale(theXScale * ((this->GetCopiedDrawRectangle().GetHeight() - theBorder) * 0.05f),
                                  theYScale * ((this->GetCopiedDrawRectangle().GetHeight() - theBorder) * 0.05f));
        theResult = UIButton::DrawPolygons();
        this->mCopiedBitmap->SetXYScale(theXScale, theYScale);
    } else
        theResult = UIButton::DrawPolygons();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawPolygons();
    return theResult;
}

// Draws the control's text and the label. No marker: ICF-folded into UITriStateCheckBox::DrawTextA (0x10080a05).
long UICheckBox::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x10060019
// Checks or unchecks the box (unless disabled), swapping its bitmaps and firing the checked or unchecked trigger
void UICheckBox::SetChecked(bool inChecked)
{
    if (this->mIsDisabled)
        return;
    this->mIsChecked = inChecked;
    if (inChecked) {
        this->SetNormalBitmap(this->mForm->GetManager()->GetCheckOn());
        this->SetPressedBitmap(this->mForm->GetManager()->GetCheckDownOn());
    } else {
        this->SetNormalBitmap(this->mForm->GetManager()->GetCheckOff());
        this->SetPressedBitmap(this->mForm->GetManager()->GetCheckDownOff());
    }
    if (this->mIsChecked)
        this->FireTriggerEvent(kControlTriggerChecked);
    else
        this->FireTriggerEvent(kControlTriggerUnchecked);
}

// FUNCTION: LLE 0x1006009b
// Enables or disables the box; a disabled box shows pressed, an enabled one normal
void UICheckBox::SetDisabled(bool inDisabled)
{
    this->mIsDisabled = inDisabled;
    if (!inDisabled) {
        // Fixed: enabling used to show the box pressed as well, until the next button-up or mouse move
        if (this->mState == kButtonStatePressed) {
            this->mBorderType = 1;
            this->mState = kButtonStateNormal;
            this->SetDirty(true);
        }
        return;
    }
    if (this->mState == kButtonStatePressed)
        return;
    this->mBorderType = 0;
    this->mState = kButtonStatePressed;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x100600c2
// Moves the check box and its label
void UICheckBox::SetLocation(const GE2DPoint& inLocation)
{
    GE2DPoint theLocation;
    UIControl::SetLocation(inLocation);
    if (this->mTextLabel == NULL)
        return;
    if (this->mScaleBitmap)
        theLocation.SetX(this->mRectangle.GetHeight() + this->mRectangle.GetLeft() + this->GetBorderThickness());
    else
        theLocation.SetX(this->mRectangle.GetLeft() + this->GetBorderThickness() + kScrollBarWidth);
    theLocation.SetY(this->mRectangle.GetTop() + this->GetBorderThickness());
    this->mTextLabel->SetLocation(theLocation);
}

// FUNCTION: LLE 0x1006013e
// Sets the label text, creating the label on first use; empty text hides the label
void UICheckBox::SetText(const UWideString& inText, GEFont* inFont)
{
    if (inText.GetLength() > 0) {
        if (this->mTextLabel) {
            this->mTextLabel->SetText(inText, true);
            this->SetDirty(true);
        } else {
            float theLeft;
            if (this->mScaleBitmap == true)
                theLeft = this->mRectangle.GetHeight() + this->mRectangle.GetLeft() + this->GetBorderThickness();
            else
                theLeft = this->mRectangle.GetLeft() + this->GetBorderThickness() + kScrollBarWidth;
            this->mTextLabel = new UITextLabel(inText, inFont, this->GetId(),
                                         GERectangle(theLeft, this->mRectangle.GetTop() + this->GetBorderThickness(),
                                                     this->mRectangle.GetRight() - this->GetBorderThickness(),
                                                     this->mRectangle.GetHeight() - 2 * this->GetBorderThickness() +
                                                         this->mRectangle.GetTop()),
                                         this->GetForm(), 0, this->GetIndex(), 0);
        }
        this->mHasText = true;
        this->SetDirty(true);
    } else
        this->mHasText = false;
}

// FUNCTION: LLE 0x10060256
// Flips the check state unless the box is disabled
void UICheckBox::ToggleCheck()
{
    if (!this->mIsDisabled)
        this->SetChecked(!this->mIsChecked);
}

// FUNCTION: LLE 0x10060270
// Presses the box unless it is disabled
void UICheckBox::ButtonDown()
{
    if (!this->mIsDisabled)
        UIButton::ButtonDown();
}

// FUNCTION: LLE 0x1006027f
// Handles a left-button press unless the box is disabled
long UICheckBox::MouseLButtonDown()
{
    if (!this->mIsDisabled)
        return UIButton::MouseLButtonDown();
    return kErrorOK;
}

// FUNCTION: LLE 0x10060290
// Handles a left-button release unless the box is disabled
long UICheckBox::MouseLButtonUp()
{
    if (!this->mIsDisabled)
        return UIButton::MouseLButtonUp();
    return kErrorOK;
}

// FUNCTION: LLE 0x100602a1
// Handles mouse movement unless the box is disabled
long UICheckBox::MouseMove(const GE2DPoint& inLocation)
{
    if (!this->mIsDisabled)
        return UIButton::MouseMove(inLocation);
    return kErrorOK;
}

// FUNCTION: LLE 0x100602ba
// Shows the box pressed
void UICheckBox::MouseOver()
{
    UIButton::MouseOver();
}

// Header-inline members first emitted here (bodies in lle/UISlider.h, UIScrollButton.h, UIScrollBar.h, UITextLabel.h,
// UICheckBox.h, UIRadioButton.h)
// FUNCTION: LLE 0x1005ef4b
// FUNCTION: LLE 0x1005ef68
// FUNCTION: LLE 0x1005ef9d
// FUNCTION: LLE 0x1005efa4
// FUNCTION: LLE 0x1005efa7
// FUNCTION: LLE 0x1005efc2
// FUNCTION: LLE 0x1005efcf
// FUNCTION: LLE 0x1005f272
// FUNCTION: LLE 0x1005f2a6
// FUNCTION: LLE 0x1005f3b0
// FUNCTION: LLE 0x1005f3bb
// FUNCTION: LLE 0x1005f3c6
// FUNCTION: LLE 0x1005f3d8
// FUNCTION: LLE 0x1005f3ee
// FUNCTION: LLE 0x1005f401
// FUNCTION: LLE 0x1005f538
// FUNCTION: LLE 0x1005f53f
// FUNCTION: LLE 0x1005f558
// FUNCTION: LLE 0x1005f56b
// FUNCTION: LLE 0x1005f572
// FUNCTION: LLE 0x1005f584
// FUNCTION: LLE 0x1005f5a0
// FUNCTION: LLE 0x1005f5bc
// FUNCTION: LLE 0x1005f5e4
// FUNCTION: LLE 0x1005f617
// FUNCTION: LLE 0x1005f625
// FUNCTION: LLE 0x1005f666
// FUNCTION: LLE 0x1005f9da
// FUNCTION: LLE 0x1005f9fc
// FUNCTION: LLE 0x1005fa03
// FUNCTION: LLE 0x1005fa07
// FUNCTION: LLE 0x1005fc41
// FUNCTION: LLE 0x1005fc81
// FUNCTION: LLE 0x1005fc88
// FUNCTION: LLE 0x1005fc94
// FUNCTION: LLE 0x1005fc98
// FUNCTION: LLE 0x1005fcac
// FUNCTION: LLE 0x1005fcb1

// Compiler-generated: implicit copy ctors / operator=, ??_E, and the U2D key init chains (names in names.txt)
// SYNTHETIC: LLE 0x1005efdc
// SYNTHETIC: LLE 0x1005f0c3
// SYNTHETIC: LLE 0x1005f1f2
// SYNTHETIC: LLE 0x1005f2bf
// SYNTHETIC: LLE 0x1005f30a
// SYNTHETIC: LLE 0x1005f34f
// SYNTHETIC: LLE 0x1005f417
// SYNTHETIC: LLE 0x1005f47a
// SYNTHETIC: LLE 0x1005f4d7
// SYNTHETIC: LLE 0x1005f689
// SYNTHETIC: LLE 0x1005f7a5
// SYNTHETIC: LLE 0x1005f979
// SYNTHETIC: LLE 0x1005fa18
// SYNTHETIC: LLE 0x1005fa6f
// SYNTHETIC: LLE 0x1005fac0
// SYNTHETIC: LLE 0x1005fcb6
// SYNTHETIC: LLE 0x1005fd25
// SYNTHETIC: LLE 0x1005fd8e
// SYNTHETIC: LLE 0x1005fb21
// SYNTHETIC: LLE 0x1005fb2b
// SYNTHETIC: LLE 0x1005fb3b
// SYNTHETIC: LLE 0x1005fb47
// SYNTHETIC: LLE 0x1005fb51
// SYNTHETIC: LLE 0x1005fb5b
// SYNTHETIC: LLE 0x1005fb6b
// SYNTHETIC: LLE 0x1005fb77
// SYNTHETIC: LLE 0x1005fb81
// SYNTHETIC: LLE 0x1005fb8b
// SYNTHETIC: LLE 0x1005fb9b
// SYNTHETIC: LLE 0x1005fba7
// SYNTHETIC: LLE 0x1005fbb1
// SYNTHETIC: LLE 0x1005fbbb
// SYNTHETIC: LLE 0x1005fbcb
// SYNTHETIC: LLE 0x1005fbd7
// SYNTHETIC: LLE 0x1005fbe1
// SYNTHETIC: LLE 0x1005fbeb
// SYNTHETIC: LLE 0x1005fbfb
// SYNTHETIC: LLE 0x1005fc07
// SYNTHETIC: LLE 0x1005fc11
// SYNTHETIC: LLE 0x1005fc1b
// SYNTHETIC: LLE 0x1005fc2b
// SYNTHETIC: LLE 0x1005fc37

// vector<UWideString*> COMDAT first emitted here
// TEMPLATE: LLE 0x100602bf
