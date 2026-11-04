// C:\EEX\Low-Level Engine\User Interface\UIButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005e47c..0x1005ef4b: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members are emitted first by
// UIBlinkingButton.cpp (it sorts before UIButton.cpp). UICheckBox.cpp's inline block starts at 0x1005ef4b.
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
#include "lle/GEText.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"

// FUNCTION: LLE 0x1005e59c
// Creates a button with normal, focused and pressed bitmaps (pressed defaults to normal) and no text or sounds
UIButton::UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness)
    : UIControl(kControlTypeButton, inId, inRectangle, inForm, inIndex, inBorderThickness), mButtonDownSound(-1),
      mButtonUpSound(-1)
{
    this->mMouseDown = false;
    this->mHasCopiedText = false;
    this->mText = NULL;
    this->mCopiedText = NULL;
    this->mState = kButtonStateNormal;
    this->mCopiedBitmap = NULL;
    this->mBorderType = 1;
    this->mKeyDown = false;
    this->mFocusedTextColor = GEColor(1.0f, 1.0f, 0.0f);
    this->mNormalBitmap = inNormalBitmap;
    if (this->mNormalBitmap)
        this->mNormalBitmap->Reference();
    if (inPressedBitmap) {
        this->mPressedBitmap = inPressedBitmap;
        this->mPressedBitmap->Reference();
    } else {
        this->mPressedBitmap = this->mNormalBitmap;
        if (this->mPressedBitmap)
            this->mPressedBitmap->Reference();
    }
    this->mFocusedBitmap = inFocusedBitmap;
    if (this->mFocusedBitmap)
        this->mFocusedBitmap->Reference();
    this->mTextRectangle.mLeft = this->mRectangle.GetLeft() + inBorderThickness;
    this->mTextRectangle.mTop = this->mRectangle.GetTop() + inBorderThickness;
    this->mTextRectangle.mRight = this->mRectangle.GetRight() - inBorderThickness;
    this->mTextRectangle.mBottom = this->mRectangle.GetBottom() - inBorderThickness;
}

// FUNCTION: LLE 0x1005e70b
// Destroys the button, releasing its texts, bitmaps and sounds
UIButton::~UIButton()
{
    if (this->mText) {
        delete this->mText;
        delete this->mCopiedText;
    }
    if (this->mNormalBitmap)
        this->mNormalBitmap->Dereference();
    if (this->mPressedBitmap)
        this->mPressedBitmap->Dereference();
    if (this->mFocusedBitmap)
        this->mFocusedBitmap->Dereference();
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Dereference();
    if (this->mButtonDownSound != -1)
        this->GetSoundManager()->Unload(this->mButtonDownSound);
    if (this->mButtonUpSound != -1)
        this->GetSoundManager()->Unload(this->mButtonUpSound);
}

// FUNCTION: LLE 0x1005e7bf
// Takes the tab stop if tabbable, shows the button pressed and plays the button-down sound
void UIButton::ButtonDown()
{
    if (this->mForm->GetTabStop() != this->mIndex && this->GetTabable())
        this->mForm->SetTabStop(this->mIndex);
    if (this->mState != kButtonStatePressed) {
        this->mBorderType = 0;
        this->mState = kButtonStatePressed;
        this->SetDirty(true);
    }
    if (this->mButtonDownSound != -1)
        this->GetSoundManager()->Play(this->mButtonDownSound, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: LLE 0x1005e832
// Shows the button focused or normal; plays the button-up sound if one is set
void UIButton::ButtonUp()
{
    if (this->mForm->GetTabbingOn() && this->mForm->GetTabStop() == this->mIndex) {
        if (this->mState != kButtonStateFocused) {
            this->mState = kButtonStateFocused;
            this->SetDirty(true);
        }
    } else if (this->mState != kButtonStateNormal) {
        this->mState = kButtonStateNormal;
        this->SetDirty(true);
    }
    this->mBorderType = 1;
    if (this->mButtonUpSound != -1)   // Fixed: used to play the button-down sound here
        this->GetSoundManager()->Play(this->mButtonUpSound, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: LLE 0x1005e8b4
// Changes the font of the button text
void UIButton::ChangeFont(GEFont* inFont)
{
    if (this->mText) {
        this->mText->SetFont(inFont);
        this->mCopiedText->SetFont(inFont);
    }
}

// FUNCTION: LLE 0x1005e8dd
// Copies the bitmap for the button state and the centred text (offset 1 pixel when pressed) for the draw pass
long UIButton::CopyData()
{
    long theResult = UIControl::CopyData();
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Dereference();
    switch (this->mState) {
    case kButtonStateNormal:
        this->mCopiedBitmap = this->mNormalBitmap;
        break;
    case kButtonStateFocused:
        this->mCopiedBitmap = this->mFocusedBitmap ? this->mFocusedBitmap : this->mNormalBitmap;
        break;
    case kButtonStatePressed:
        this->mCopiedBitmap = this->mPressedBitmap;
        break;
    }
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Reference();
    this->mBitmapLocation.SetX(this->mBitmapDrawOffset.GetX() + this->mRectangle.GetLeft() + this->mBorderThickness);
    this->mBitmapLocation.SetY(this->mBitmapDrawOffset.GetY() + this->mRectangle.GetTop() + this->mBorderThickness);
    if (this->mText && this->mText->GetText().GetLength() > 0) {
        this->mCopiedTextRectangle = this->mTextRectangle;
        if (this->mState == kButtonStatePressed) {
            // the original expands SetTopLeft/SetBottomRight inline (stores top then left, bottom then right):
            // they are header-inline in the real GERectangle.h (GEBitmap.cpp emits them in its GERectangle block)
            GE2DPoint theTopLeft = this->mCopiedTextRectangle.GetTopLeft();
            this->mCopiedTextRectangle.SetTopLeft(GE2DPoint(theTopLeft.GetX() + 1.0f, theTopLeft.GetY() + 1.0f));
            GE2DPoint theBottomRight = this->mCopiedTextRectangle.GetBottomRight();
            this->mCopiedTextRectangle.SetBottomRight(GE2DPoint(theBottomRight.GetX() + 1.0f, theBottomRight.GetY() + 1.0f));
        }
        if (this->mText->GetTextExtent().GetWidth() < this->mCopiedTextRectangle.GetWidth())
            this->mCopiedTextRectangle.SetLeft(this->mCopiedTextRectangle.GetWidth() * 0.5f + this->mCopiedTextRectangle.GetLeft() -
                                         this->mText->GetTextExtent().GetWidth() * 0.5f);
        if (this->mText->GetTextExtent().GetHeight() < this->mCopiedTextRectangle.GetHeight())
            this->mCopiedTextRectangle.SetTop(this->mCopiedTextRectangle.GetHeight() * 0.5f + this->mCopiedTextRectangle.GetTop() -
                                        this->mText->GetTextExtent().GetHeight() * 0.5f);
        *this->mCopiedText = *this->mText;
        if (this->mState == kButtonStateFocused || this->mState == kButtonStatePressed)
            this->mCopiedText->SetFgColor(this->mFocusedTextColor);
        this->mHasCopiedText = true;
    } else
        this->mHasCopiedText = false;
    return theResult;
}

// FUNCTION: LLE 0x1005eb30
// Draws the control, then the copied button bitmap at its location
long UIButton::DrawPolygons()
{
    long theResult = UIControl::DrawPolygons();
    if (this->mCopiedBitmap) {
        this->mCopiedBitmap->SetLocation(this->mBitmapLocation);
        theResult = this->GetRasterizer()->Draw(this->mCopiedBitmap, this->GetScreenIndex());
    }
    return theResult;
}

// FUNCTION: LLE 0x1005eb80
// Draws the control's text, then the copied button text clipped to its rectangle
long UIButton::DrawTextA()
{
    long theResult = UIControl::DrawTextA();
    if (this->mHasCopiedText) {
        this->mCopiedText->SetLocation(this->mCopiedTextRectangle.GetTopLeft());
        theResult = this->GetForm()->DrawClippedText(this->mCopiedTextRectangle, this->mCopiedText, this);
    }
    return theResult;
}

// FUNCTION: LLE 0x1005ebcf
// Captures the mouse, presses the button and fires the button-down trigger
long UIButton::MouseLButtonDown()
{
    this->mMouseDown = true;
    this->CaptureMouse();
    this->ButtonDown();
    this->FireTriggerEvent(kControlTriggerButtonDown);
    return kErrorOK;
}

// FUNCTION: LLE 0x1005ebf8
// Releases the mouse and keyboard; if the pointer is still over the button, releases it and fires the button-up trigger
long UIButton::MouseLButtonUp()
{
    this->MouseMove(this->GetMouseLocation());
    this->mMouseDown = false;
    this->mKeyDown = false;
    this->ReleaseCapturedMouse();
    this->ReleaseCapturedKeyboard();
    if (this->mRectangle.Contains(this->GetMouseLocation())) {
        this->ButtonUp();
        this->FireTriggerEvent(kControlTriggerButtonUp);
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1005ec54
// While the button is held, shows it pressed when the pointer is over it (or a key holds it), else focused or normal
long UIButton::MouseMove(const GE2DPoint& inLocation)
{
    if (!(this->mMouseDown || this->GetHasMouseCapture() && this->GetRightMouseButtonDown()))
        return kErrorOK;
    if (this->mRectangle.Contains(inLocation) || this->mKeyDown)
        this->MouseOver();
    else {
        if (this->mForm->GetTabbingOn() && this->mForm->GetTabStop() == this->mIndex) {
            if (this->mState != kButtonStateFocused) {
                this->mState = kButtonStateFocused;
                this->SetDirty(true);
            }
        } else if (this->mState != kButtonStateNormal) {
            this->mState = kButtonStateNormal;
            this->SetDirty(true);
        }
        this->mBorderType = 1;
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x1005ece9
// Shows the button pressed
void UIButton::MouseOver()
{
    if (this->mState == kButtonStatePressed)
        return;
    this->mBorderType = 0;
    this->mState = kButtonStatePressed;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1005ed04
// Loads the button-down sound, unloading the previous one
void UIButton::SetButtonDownSound(const UString& inName)
{
    if (this->mButtonDownSound != -1)
        this->GetSoundManager()->Unload(this->mButtonDownSound);
    this->mButtonDownSound = this->GetSoundManager()->Load(inName, 0, 1);
}

// FUNCTION: LLE 0x1005ed3f
// Loads the button-up sound, unloading the previous one
void UIButton::SetButtonUpSound(const UString& inName)
{
    if (this->mButtonUpSound != -1)
        this->GetSoundManager()->Unload(this->mButtonUpSound);
    this->mButtonUpSound = this->GetSoundManager()->Load(inName, 0, 1);
}

// FUNCTION: LLE 0x1005ed7a
// Moves the button and its text rectangle
void UIButton::SetLocation(const GE2DPoint& inLocation)
{
    UIControl::SetLocation(inLocation);
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + this->GetBorderThickness());
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + this->GetBorderThickness());
    this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - this->GetBorderThickness());
    this->mTextRectangle.SetBottom(this->GetRectangle().GetBottom() - this->GetBorderThickness());
}

// FUNCTION: LLE 0x1005edf1
// Sets the button text, creating the text objects with the font on first use
void UIButton::SetText(const UWideString& inText, GEFont* inFont)
{
    if (this->mText == NULL) {
        this->mText = new GEText(inFont);
        this->mCopiedText = new GEText(inFont);
    }
    this->mText->SetText(inText);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1005ee67
// Shows the button focused when tabbed to
void UIButton::TabbedTo()
{
    this->mState = kButtonStateFocused;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1005ee77
// Shows the button normal when tabbed away from, releasing the mouse, keyboard and any press
void UIButton::TabbedFrom()
{
    this->mState = kButtonStateNormal;
    this->ReleaseCapturedMouse();
    this->ReleaseCapturedKeyboard();
    this->mMouseDown = false;
    this->mKeyDown = false;
    this->SetDirty(true);
}

// FUNCTION: LLE 0x1005eea6
// Space or Enter presses the button; releasing them clicks it (an Enter release clicks even without a key press)
long UIButton::UpdateKeyboard()
{
    if (UIControl::sSpaceKey->JustPressed() || UIControl::sEnterKey->JustPressed()) {
        this->mKeyDown = true;
        this->CaptureKeyboard();
        this->ButtonDown();
        if (!this->mMouseDown)
            this->FireTriggerEvent(kControlTriggerButtonDown);
    } else if (this->mKeyDown && UIControl::sSpaceKey->JustUnpressed() || UIControl::sEnterKey->JustUnpressed()) {
        this->mMouseDown = false;
        this->ReleaseCapturedMouse();
        this->ReleaseCapturedKeyboard();
        this->mKeyDown = false;
        this->ButtonUp();
        this->FireTriggerEvent(kControlTriggerButtonUp);
    }
    return kErrorOK;
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005e47c
// SYNTHETIC: LLE 0x1005e486
// SYNTHETIC: LLE 0x1005e496
// SYNTHETIC: LLE 0x1005e4a2
// SYNTHETIC: LLE 0x1005e4ac
// SYNTHETIC: LLE 0x1005e4b6
// SYNTHETIC: LLE 0x1005e4c6
// SYNTHETIC: LLE 0x1005e4d2
// SYNTHETIC: LLE 0x1005e4dc
// SYNTHETIC: LLE 0x1005e4e6
// SYNTHETIC: LLE 0x1005e4f6
// SYNTHETIC: LLE 0x1005e502
// SYNTHETIC: LLE 0x1005e50c
// SYNTHETIC: LLE 0x1005e516
// SYNTHETIC: LLE 0x1005e526
// SYNTHETIC: LLE 0x1005e532
// SYNTHETIC: LLE 0x1005e53c
// SYNTHETIC: LLE 0x1005e546
// SYNTHETIC: LLE 0x1005e556
// SYNTHETIC: LLE 0x1005e562
// SYNTHETIC: LLE 0x1005e56c
// SYNTHETIC: LLE 0x1005e576
// SYNTHETIC: LLE 0x1005e586
// SYNTHETIC: LLE 0x1005e592
