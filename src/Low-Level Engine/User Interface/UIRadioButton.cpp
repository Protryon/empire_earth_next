// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007badf..0x1007c38e: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members (and the implicit copy members, ??_E) are emitted first by UICheckBox.cpp (0x1005fc41..).
// The next unit (0x1007c38e..0x1007c901) is UIScalableButton.cpp (not exported; RTTI .?AVUIScalableButton@@).
//
// The check logic and the label's geometry are Rust (rust/crates/ee_ui/src/button.rs, docs/RUST_PLAN.md R6): the box
// rectangle, Check, UnCheck, RemoveOtherChecks, the checked push button's border and ignored press, SetLocation and
// SetText forward to it (UIButtonRust.h; the record from mOtherNormalBitmap on is EeUiRadioFields). The bitmaps'
// reference counts, the label object and drawing stay here.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_rust.h"

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
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIRadioButton.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "UIButtonRust.h"

void* UIButtonRust::RadioFields(UIRadioButton* inButton)
{
#define EE_UI_RADIO_FIELD(member, field)                                                                              \
    static_assert(offsetof(UIRadioButton, member) - offsetof(UIRadioButton, mOtherNormalBitmap) ==                    \
                      offsetof(EeUiRadioFields, field),                                                               \
                  "ee_ui/src/button.rs")
    EE_UI_RADIO_FIELD(mOtherNormalBitmap, other_normal_bitmap);
    EE_UI_RADIO_FIELD(mOtherPressedBitmap, other_pressed_bitmap);
    EE_UI_RADIO_FIELD(mTextLabel, text_label);
    EE_UI_RADIO_FIELD(mIsChecked, is_checked);
    EE_UI_RADIO_FIELD(mHasText, has_text);
    EE_UI_RADIO_FIELD(mIsPushButton, is_push_button);
    EE_UI_RADIO_FIELD(mScaleBitmap, scale_bitmap);
#undef EE_UI_RADIO_FIELD
    static_assert(sizeof(UIRadioButton) - offsetof(UIRadioButton, mOtherNormalBitmap) >= sizeof(EeUiRadioFields),
                  "ee_ui/src/button.rs");
    return &inButton->mOtherNormalBitmap;
}
UITextLabel** UIButtonRust::RadioLabel(UIRadioButton* inButton) { return &inButton->mTextLabel; }

// The radio button's own host calls (ee_ui/src/button_ffi.rs)
static UIRadioButton* RadioButton(void* inButton) { return static_cast<UIRadioButton*>(UIButtonRust::Button(inButton)); }
// Check's and UnCheck's swap: the normal and pressed bitmaps trade places with the other check state's
extern "C" void ee_uicpp_radio_swap_bitmaps(void* inButton)
{
    UIRadioButton* theButton = RadioButton(inButton);
    UIButtonRust::RadioSwap(theButton);
}
extern "C" long ee_uicpp_radio_control_count(void* inButton) { return (long)RadioButton(inButton)->GetForm()->GetControlCount(); }
extern "C" bool ee_uicpp_radio_is_radio(void* inButton, long inIndex)
{
    UIControl* theControl = RadioButton(inButton)->GetForm()->GetControl(inIndex);
    return theControl && theControl->GetType() == kControlTypeRadioButton;
}
extern "C" bool ee_uicpp_radio_checked(void* inButton, long inIndex)
{
    return static_cast<UIRadioButton*>(RadioButton(inButton)->GetForm()->GetControl(inIndex))->GetIsChecked();
}
extern "C" void ee_uicpp_radio_uncheck(void* inButton, long inIndex)
{
    UIButtonRust::RadioUnCheck(static_cast<UIRadioButton*>(RadioButton(inButton)->GetForm()->GetControl(inIndex)));
}

void UIButtonRust::RadioSwap(UIRadioButton* inButton)
{
    GEBitmap* theNormalBitmap = inButton->mNormalBitmap;
    if (theNormalBitmap)
        theNormalBitmap->Reference();
    GEBitmap* thePressedBitmap = inButton->mPressedBitmap;
    if (thePressedBitmap)
        thePressedBitmap->Reference();
    inButton->SetNormalBitmap(inButton->mOtherNormalBitmap);
    inButton->SetPressedBitmap(inButton->mOtherPressedBitmap);
    if (inButton->mOtherNormalBitmap)
        inButton->mOtherNormalBitmap->Dereference();
    inButton->mOtherNormalBitmap = theNormalBitmap;
    if (inButton->mOtherNormalBitmap)
        inButton->mOtherNormalBitmap->Reference();
    if (inButton->mOtherPressedBitmap)
        inButton->mOtherPressedBitmap->Dereference();
    inButton->mOtherPressedBitmap = thePressedBitmap;
    if (inButton->mOtherPressedBitmap)
        inButton->mOtherPressedBitmap->Reference();
    inButton->SetDirty(true);
    if (theNormalBitmap)
        theNormalBitmap->Dereference();
    if (thePressedBitmap)
        thePressedBitmap->Dereference();
}
void UIButtonRust::RadioUnCheck(UIRadioButton* inButton) { inButton->UnCheck(); }

// The Rust view of this radio button, with a label context for SetText/SetLocation
#define EE_UI_RADIO_REFS(label) UIButtonRust::Refs(this, UIButtonRust::RadioFields(this), label)

// FUNCTION: LLE 0x1007bbff
// Creates a radio button with the shared radio bitmaps (checked ones when checked), 19x19 unless scaled, and a label
UIRadioButton::UIRadioButton(unsigned long inId, const UWideString& inText, GEFont* inFont,
                             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                             unsigned long inBorderThickness, bool inIsPushButton, bool inIsChecked,
                             bool inScaleBitmap)
    : UIButton(inId, inForm->GetManager()->GetRadioOff(), NULL, inForm->GetManager()->GetRadioDownOff(),
               UIButtonRust::Rect(ee_ui_button_box_rect(UIButtonRust::Rect(inRectangle), inScaleBitmap)),
               inForm, inIndex, inBorderThickness)
{
    this->mTextLabel = NULL;
    this->mHasText = false;
    this->mIsPushButton = inIsPushButton;
    this->mIsChecked = inIsChecked;
    this->mScaleBitmap = inScaleBitmap;
    this->mOtherNormalBitmap = this->mForm->GetManager()->GetRadioOn();
    this->mOtherNormalBitmap->Reference();
    this->mOtherPressedBitmap = this->mForm->GetManager()->GetRadioDownOn();
    this->mOtherPressedBitmap->Reference();
    if (this->mIsChecked) {
        GEBitmap* theNormalBitmap = this->mNormalBitmap;
        if (theNormalBitmap)
            theNormalBitmap->Reference();
        GEBitmap* thePressedBitmap = this->mPressedBitmap;
        if (thePressedBitmap)
            thePressedBitmap->Reference();
        this->SetNormalBitmap(this->mOtherNormalBitmap);
        this->SetPressedBitmap(this->mOtherPressedBitmap);
        this->mOtherNormalBitmap->Dereference();
        this->mOtherNormalBitmap = theNormalBitmap;
        this->mOtherNormalBitmap->Reference();
        this->mOtherPressedBitmap->Dereference();
        this->mOtherPressedBitmap = thePressedBitmap;
        this->mOtherPressedBitmap->Reference();
        if (theNormalBitmap)
            theNormalBitmap->Dereference();
        if (thePressedBitmap)
            thePressedBitmap->Dereference();
    }
    UIButtonRust::BitmapOffset(this, inRectangle.GetHeight(), inBorderThickness, this->mScaleBitmap, false);
    this->SetText(inText, inFont);
}

// FUNCTION: LLE 0x1007be03
// Checks the button: unchecks its checked neighbour, swaps to the checked bitmaps and fires the checked trigger
void UIRadioButton::Check()
{
    ee_ui_radio_check(EE_UI_RADIO_REFS(NULL));
}

// FUNCTION: LLE 0x1007bed5
// Snapshots the button and its label (deleted once its text is cleared); a checked push button loses its border
long UIRadioButton::CopyData()
{
    long theResult = ee_ui_radio_copy_data(EE_UI_RADIO_REFS(NULL));
    if (this->mHasText)
        theResult = this->mTextLabel->CopyData();
    else if (this->mTextLabel) {
        delete this->mTextLabel;
        this->mTextLabel = NULL;
    }
    return theResult;
}

// FUNCTION: LLE 0x1007bf2d
// Draws the button (its bitmap scaled to the control height when scaled), then its label
long UIRadioButton::DrawPolygons()
{
    long theResult;
    if (this->mScaleBitmap && this->mCopiedBitmap) {
        float theXScale = this->mCopiedBitmap->GetXScale();
        float theYScale = this->mCopiedBitmap->GetYScale();
        float theScale[2];   // the box's height over 20 pixels (ee_ui draw.rs)
        ee_ui_box_bitmap_scale(theXScale, theYScale, this->GetCopiedDrawRectangle().GetHeight(), false, 0, theScale);
        this->mCopiedBitmap->SetXYScale(theScale[0], theScale[1]);
        theResult = UIButton::DrawPolygons();
        this->mCopiedBitmap->SetXYScale(theXScale, theYScale);
    } else
        theResult = UIButton::DrawPolygons();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawPolygons();
    return theResult;
}

// FUNCTION: LLE 0x1007bfbf
// Draws the button's text and its label
long UIRadioButton::DrawTextA()
{
    long theResult = UIButton::DrawTextA();
    if (this->mTextLabel)
        theResult = this->mTextLabel->DrawTextA();
    return theResult;
}

// FUNCTION: LLE 0x1007bfd8
// Presses the button, except a checked push button
long UIRadioButton::MouseLButtonDown()
{
    return ee_ui_radio_mouse_l_button_down(EE_UI_RADIO_REFS(NULL));
}

// FUNCTION: LLE 0x1007bff0
// Releases the button (plain button behaviour)
long UIRadioButton::MouseLButtonUp()
{
    return UIButton::MouseLButtonUp();
}

// FUNCTION: LLE 0x1007bff5
// Unchecks the first checked radio button among the adjacent radio controls before, then after, this one
void UIRadioButton::RemoveOtherChecks()
{
    ee_ui_radio_remove_other_checks(EE_UI_RADIO_REFS(NULL));
}

// FUNCTION: LLE 0x1007c066
// Sets the pressed bitmap shown while checked
void UIRadioButton::SetDownCheckedBitmap(GEBitmap* inBitmap)
{
    if (this->mIsChecked) {
        if (this->mPressedBitmap)
            this->mPressedBitmap->Dereference();
        this->mPressedBitmap = inBitmap;
        if (this->mPressedBitmap)
            this->mPressedBitmap->Reference();
    } else {
        if (this->mOtherPressedBitmap)
            this->mOtherPressedBitmap->Dereference();
        this->mOtherPressedBitmap = inBitmap;
        if (this->mOtherPressedBitmap)
            this->mOtherPressedBitmap->Reference();
    }
}

// FUNCTION: LLE 0x1007c098
// Sets the pressed bitmap shown while unchecked
void UIRadioButton::SetDownUncheckedBitmap(GEBitmap* inBitmap)
{
    if (this->mIsChecked) {
        if (this->mOtherPressedBitmap)
            this->mOtherPressedBitmap->Dereference();
        this->mOtherPressedBitmap = inBitmap;
        if (this->mOtherPressedBitmap)
            this->mOtherPressedBitmap->Reference();
    } else {
        if (this->mPressedBitmap)
            this->mPressedBitmap->Dereference();
        this->mPressedBitmap = inBitmap;
        if (this->mPressedBitmap)
            this->mPressedBitmap->Reference();
    }
}

// FUNCTION: LLE 0x1007c0ca
// Moves the button, keeping its label to the right of the bitmap
void UIRadioButton::SetLocation(const GE2DPoint& inLocation)
{
    UIButtonRust::Label theLabel = {UIButtonRust::RadioLabel(this), NULL, NULL};
    ee_ui_radio_set_location(EE_UI_RADIO_REFS(&theLabel), UIButtonRust::Point(inLocation));
}

// FUNCTION: LLE 0x1007c146
// Sets the label text, creating the label right of the bitmap on first use; empty text hides the label
void UIRadioButton::SetText(const UWideString& inText, GEFont* inFont)
{
    UIButtonRust::Label theLabel = {UIButtonRust::RadioLabel(this), &inText, inFont};
    ee_ui_radio_set_text(EE_UI_RADIO_REFS(&theLabel), inText);
}

// FUNCTION: LLE 0x1007c25d
// Sets the normal bitmap shown while checked
void UIRadioButton::SetUpCheckedBitmap(GEBitmap* inBitmap)
{
    if (this->mIsChecked) {
        if (this->mNormalBitmap)
            this->mNormalBitmap->Dereference();
        this->mNormalBitmap = inBitmap;
        if (this->mNormalBitmap)
            this->mNormalBitmap->Reference();
    } else {
        if (this->mOtherNormalBitmap)
            this->mOtherNormalBitmap->Dereference();
        this->mOtherNormalBitmap = inBitmap;
        if (this->mOtherNormalBitmap)
            this->mOtherNormalBitmap->Reference();
    }
}

// FUNCTION: LLE 0x1007c28f
// Sets the normal bitmap shown while unchecked
void UIRadioButton::SetUpUncheckedBitmap(GEBitmap* inBitmap)
{
    if (this->mIsChecked) {
        if (this->mOtherNormalBitmap)
            this->mOtherNormalBitmap->Dereference();
        this->mOtherNormalBitmap = inBitmap;
        if (this->mOtherNormalBitmap)
            this->mOtherNormalBitmap->Reference();
    } else {
        if (this->mNormalBitmap)
            this->mNormalBitmap->Dereference();
        this->mNormalBitmap = inBitmap;
        if (this->mNormalBitmap)
            this->mNormalBitmap->Reference();
    }
}

// FUNCTION: LLE 0x1007c2c1
// Unchecks the button: swaps to the unchecked bitmaps, restores its border and fires the unchecked trigger
void UIRadioButton::UnCheck()
{
    ee_ui_radio_uncheck(EE_UI_RADIO_REFS(NULL));
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007badf
// SYNTHETIC: LLE 0x1007bae9
// SYNTHETIC: LLE 0x1007baf9
// SYNTHETIC: LLE 0x1007bb05
// SYNTHETIC: LLE 0x1007bb0f
// SYNTHETIC: LLE 0x1007bb19
// SYNTHETIC: LLE 0x1007bb29
// SYNTHETIC: LLE 0x1007bb35
// SYNTHETIC: LLE 0x1007bb3f
// SYNTHETIC: LLE 0x1007bb49
// SYNTHETIC: LLE 0x1007bb59
// SYNTHETIC: LLE 0x1007bb65
// SYNTHETIC: LLE 0x1007bb6f
// SYNTHETIC: LLE 0x1007bb79
// SYNTHETIC: LLE 0x1007bb89
// SYNTHETIC: LLE 0x1007bb95
// SYNTHETIC: LLE 0x1007bb9f
// SYNTHETIC: LLE 0x1007bba9
// SYNTHETIC: LLE 0x1007bbb9
// SYNTHETIC: LLE 0x1007bbc5
// SYNTHETIC: LLE 0x1007bbcf
// SYNTHETIC: LLE 0x1007bbd9
// SYNTHETIC: LLE 0x1007bbe9
// SYNTHETIC: LLE 0x1007bbf5
