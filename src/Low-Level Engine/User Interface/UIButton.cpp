// C:\EEX\Low-Level Engine\User Interface\UIButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005e47c..0x1005ef4b: the U2D key init chains (U2DRectangle.h, via the rasterizer header), the ctor, the dtor
// and the out-of-line members in alphabetical order. The header-inline members are emitted first by
// UIBlinkingButton.cpp (it sorts before UIButton.cpp). UICheckBox.cpp's inline block starts at 0x1005ef4b.
//
// The button's logic is Rust (rust/crates/ee_ui/src/button.rs, docs/RUST_PLAN.md R6): the state machine over the
// mouse, the keyboard and tabbing, SetLocation's text rectangle and what CopyData shows. The members below forward to
// it with the object's members from mTextRectangle on as the pinned record EeUiButtonFields (UIButtonRust.h). What
// stays here touches other objects: the bitmaps' and texts' lifetimes, the sounds, drawing, and the ee_uicpp_button_*
// host calls through which Rust reaches the form, the capture, the trigger events and the virtual members.
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
#include "lle/GEText.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

// --- the host calls of ee_ui/src/button_ffi.rs -----------------------------------------------------------------------

extern "C" unsigned long ee_uicpp_button_tab_stop(void* inButton) { return UIButtonRust::Button(inButton)->GetForm()->GetTabStop(); }
extern "C" bool ee_uicpp_button_tabbing_on(void* inButton) { return UIButtonRust::Button(inButton)->GetForm()->GetTabbingOn(); }
extern "C" void ee_uicpp_button_set_tab_stop(void* inButton, unsigned long inIndex)
{
    UIButtonRust::Button(inButton)->GetForm()->SetTabStop(inIndex);
}
extern "C" bool ee_uicpp_button_tabable(void* inButton) { return UIButtonRust::Button(inButton)->GetTabable(); }
extern "C" void ee_uicpp_button_set_dirty(void* inButton) { UIButtonRust::Button(inButton)->SetDirty(true); }
extern "C" void ee_uicpp_button_play(void* inButton, unsigned long inSound)
{
    UIButtonRust::Button(inButton)->GetSoundManager()->Play(inSound, 1.0f, 0.0f, 1.0f, false, NULL);
}
extern "C" void ee_uicpp_button_capture_mouse(void* inButton) { UIButtonRust::CaptureMouse(inButton); }
extern "C" void ee_uicpp_button_release_mouse(void* inButton) { UIButtonRust::Button(inButton)->ReleaseCapturedMouse(); }
extern "C" void ee_uicpp_button_capture_keyboard(void* inButton) { UIButtonRust::CaptureKeyboard(inButton); }
extern "C" void ee_uicpp_button_release_keyboard(void* inButton) { UIButtonRust::Button(inButton)->ReleaseCapturedKeyboard(); }
extern "C" void ee_uicpp_button_fire(void* inButton, unsigned long inEvent) { UIButtonRust::Button(inButton)->FireTriggerEvent(inEvent); }
extern "C" EeUiPoint ee_uicpp_button_mouse_location(void* inButton) { return UIButtonRust::Point(UIButtonRust::MouseLocation(inButton)); }
extern "C" bool ee_uicpp_button_has_mouse_capture(void* inButton) { return UIButtonRust::Button(inButton)->GetHasMouseCapture(); }
extern "C" bool ee_uicpp_button_right_button_down(void* inButton) { return UIButtonRust::Button(inButton)->GetRightMouseButtonDown(); }
extern "C" bool ee_uicpp_button_key(void* inButton, int inKey) { return UIButtonRust::Key(inKey); }
extern "C" void ee_uicpp_button_button_down(void* inButton) { UIButtonRust::ButtonDown(inButton); }
extern "C" void ee_uicpp_button_button_up(void* inButton) { UIButtonRust::ButtonUp(inButton); }
extern "C" void ee_uicpp_button_mouse_over(void* inButton) { UIButtonRust::MouseOver(inButton); }
extern "C" void ee_uicpp_button_mouse_move(void* inButton, float inX, float inY) { UIButtonRust::MouseMove(inButton, inX, inY); }
extern "C" void ee_uicpp_button_base_set_location(void* inButton, float inX, float inY)
{
    UIButtonRust::Button(inButton)->UIControl::SetLocation(GE2DPoint(inX, inY));
}
extern "C" long ee_uicpp_button_base_copy_data(void* inButton) { return UIButtonRust::Button(inButton)->UIControl::CopyData(); }
extern "C" void ee_uicpp_button_set_copied_bitmap(void* inButton, void* inBitmap)
{
    UIButtonRust::SetCopiedBitmap(inButton, static_cast<GEBitmap*>(inBitmap));
}
extern "C" const char* ee_uicpp_button_text(void* inButton) { return UIButtonRust::Text(inButton)->GetText(); }
extern "C" void ee_uicpp_button_text_extent(void* inButton, float* outWidth, float* outHeight)
{
    const GERectangle& theExtent = UIButtonRust::Text(inButton)->GetTextExtent();
    *outWidth = theExtent.GetWidth();
    *outHeight = theExtent.GetHeight();
}
void UIButtonRust::CopyText(void* inButton, bool inFocused)
{
    UIButton* theButton = Button(inButton);
    *theButton->mCopiedText = *theButton->mText;
    if (inFocused)
        theButton->mCopiedText->SetFgColor(theButton->mFocusedTextColor);
}
extern "C" void ee_uicpp_button_copy_text(void* inButton, bool inFocused) { UIButtonRust::CopyText(inButton, inFocused); }

// The check box's and radio button's label (UICheckBox.cpp, UIRadioButton.cpp): the context names it
extern "C" void ee_uicpp_label_set_text(void* inButton, void* inLabel)
{
    UIButtonRust::Label* theLabel = static_cast<UIButtonRust::Label*>(inLabel);
    (*theLabel->mLabel)->SetText(*theLabel->mText);   // the public SetText(text, true)
}
extern "C" void ee_uicpp_label_create(void* inButton, void* inLabel, const EeUiRect* inRectangle)
{
    UIButtonRust::Label* theLabel = static_cast<UIButtonRust::Label*>(inLabel);
    UIButton* theButton = UIButtonRust::Button(inButton);
    *theLabel->mLabel = new UITextLabel(*theLabel->mText, theLabel->mFont, theButton->GetId(),
                                        *reinterpret_cast<const GERectangle*>(inRectangle), theButton->GetForm(), 0,
                                        theButton->GetIndex(), 0);
}
extern "C" void ee_uicpp_label_set_location(void* inButton, void* inLabel, float inX, float inY)
{
    (*static_cast<UIButtonRust::Label*>(inLabel)->mLabel)->SetLocation(GE2DPoint(inX, inY));
}

// FUNCTION: LLE 0x1005e59c
// Creates a button with normal, focused and pressed bitmaps (pressed defaults to normal) and no text or sounds
UIButton::UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                   unsigned long inBorderThickness)
    : UIControl(kControlTypeButton, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    ee_ui_button_init(reinterpret_cast<EeUiButtonFields*>(&this->mTextRectangle),
                      reinterpret_cast<const EeUiRect*>(&this->mRectangle), inBorderThickness);
    this->mBorderType = 1;
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
    if (!UIsNone32(this->mButtonDownSound))
        this->GetSoundManager()->Unload(this->mButtonDownSound);
    if (!UIsNone32(this->mButtonUpSound))
        this->GetSoundManager()->Unload(this->mButtonUpSound);
}

// FUNCTION: LLE 0x1005e7bf
// Takes the tab stop if tabbable, shows the button pressed and plays the button-down sound
void UIButton::ButtonDown()
{
    ee_ui_button_button_down(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005e832
// Shows the button focused or normal; plays the button-up sound if one is set
void UIButton::ButtonUp()
{
    ee_ui_button_button_up(UIButtonRust::Refs(this));   // Fixed (kept): plays the button-up sound
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
    return ee_ui_button_copy_data(UIButtonRust::Refs(this));
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
    return ee_ui_button_mouse_l_button_down(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005ebf8
// Releases the mouse and keyboard; if the pointer is still over the button, releases it and fires the button-up trigger
long UIButton::MouseLButtonUp()
{
    return ee_ui_button_mouse_l_button_up(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005ec54
// While the button is held, shows it pressed when the pointer is over it (or a key holds it), else focused or normal
long UIButton::MouseMove(const GE2DPoint& inLocation)
{
    return ee_ui_button_mouse_move(UIButtonRust::Refs(this), UIButtonRust::Point(inLocation));
}

// FUNCTION: LLE 0x1005ece9
// Shows the button pressed
void UIButton::MouseOver()
{
    ee_ui_button_mouse_over(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005ed04
// Loads the button-down sound, unloading the previous one
void UIButton::SetButtonDownSound(const UString& inName)
{
    if (!UIsNone32(this->mButtonDownSound))
        this->GetSoundManager()->Unload(this->mButtonDownSound);
    this->mButtonDownSound = this->GetSoundManager()->Load(inName, 0, 1);
}

// FUNCTION: LLE 0x1005ed3f
// Loads the button-up sound, unloading the previous one
void UIButton::SetButtonUpSound(const UString& inName)
{
    if (!UIsNone32(this->mButtonUpSound))
        this->GetSoundManager()->Unload(this->mButtonUpSound);
    this->mButtonUpSound = this->GetSoundManager()->Load(inName, 0, 1);
}

// FUNCTION: LLE 0x1005ed7a
// Moves the button and its text rectangle
void UIButton::SetLocation(const GE2DPoint& inLocation)
{
    ee_ui_button_set_location(UIButtonRust::Refs(this), UIButtonRust::Point(inLocation));
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
    ee_ui_button_tabbed_to(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005ee77
// Shows the button normal when tabbed away from, releasing the mouse, keyboard and any press
void UIButton::TabbedFrom()
{
    ee_ui_button_tabbed_from(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x1005eea6
// Space or Enter presses the button; releasing them after a key press on it clicks it
long UIButton::UpdateKeyboard()
{
    // Fixed (kept): only a release after a key press on the button clicks it
    return ee_ui_button_update_keyboard(UIButtonRust::Refs(this));
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
