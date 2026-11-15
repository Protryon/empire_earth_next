// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007badf..0x1007c38e: the U2D key init chains, the ctor, then the out-of-line members in alphabetical order. The
// header-inline members (and the implicit copy members, ??_E) are emitted first by UICheckBox.cpp (0x1005fc41..).
// The next unit (0x1007c38e..0x1007c901) is UIScalableButton.cpp (not exported; RTTI .?AVUIScalableButton@@).
//
// UIRadioButton is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record is the Rust
// button's and every method below forwards to it, the radio walk over the form's siblings included (UIButton.cpp's
// host answers RadioOf and calls a sibling's virtual UnCheck). The constructor's UIButton part stays here.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_ui.h"

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

typedef EeUiButtonCall C;
#define EE_UI_RADIO(c, ...) UIButtonRust::Call(this, C::c, ##__VA_ARGS__)

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
    EE_UI_RADIO(RadioInit, (intptr_t)&inText, (intptr_t)inFont, inRectangle.GetHeight(), inIsPushButton ? 1.0f : 0.0f,
                inIsChecked ? 1.0f : 0.0f, inScaleBitmap ? 1.0f : 0.0f);
}

// FUNCTION: LLE 0x1007be03
// Checks the button: unchecks its checked neighbour, swaps to the checked bitmaps and fires the checked trigger
void UIRadioButton::Check() { EE_UI_RADIO(RadioCheck); }
// FUNCTION: LLE 0x1007bed5
// Snapshots the button and its label (deleted once its text is cleared); a checked push button loses its border
long UIRadioButton::CopyData() { return (long)EE_UI_RADIO(RadioCopyData); }
// FUNCTION: LLE 0x1007bf2d
// Draws the button (its bitmap scaled to the control height when scaled), then its label
long UIRadioButton::DrawPolygons() { return (long)EE_UI_RADIO(RadioDrawPolygons); }
// FUNCTION: LLE 0x1007bfbf
// Draws the button's text and its label
long UIRadioButton::DrawTextA() { return (long)EE_UI_RADIO(RadioDrawText); }
bool UIRadioButton::GetIsChecked() const { return EE_UI_RADIO(RadioIsChecked) != 0; }
UITextLabel* UIRadioButton::GetTextLabel() const { return (UITextLabel*)EE_UI_RADIO(TextLabel); }
bool UIRadioButton::GetTabable() const { return EE_UI_RADIO(RadioTabable) != 0; }
// Checks the button (the virtual Check), then releases it
void UIRadioButton::ButtonUp() { EE_UI_RADIO(RadioButtonUp); }
// FUNCTION: LLE 0x1007bfd8
// Presses the button, except a checked push button
long UIRadioButton::MouseLButtonDown() { return (long)EE_UI_RADIO(RadioMouseLButtonDown); }
// FUNCTION: LLE 0x1007bff0
// Releases the button (plain button behaviour)
long UIRadioButton::MouseLButtonUp() { return UIButton::MouseLButtonUp(); }
// FUNCTION: LLE 0x1007bff5
// (RemoveOtherChecks: the Rust button's radio walk, checkobj.rs)
// FUNCTION: LLE 0x1007c066
// Sets the pressed bitmap shown while checked
void UIRadioButton::SetDownCheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetDownChecked, (intptr_t)inBitmap); }
// FUNCTION: LLE 0x1007c098
// Sets the pressed bitmap shown while unchecked
void UIRadioButton::SetDownUncheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetDownUnchecked, (intptr_t)inBitmap); }
// FUNCTION: LLE 0x1007c0ca
// Moves the button, keeping its label to the right of the bitmap
void UIRadioButton::SetLocation(const GE2DPoint& inLocation)
{
    EE_UI_RADIO(RadioSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// FUNCTION: LLE 0x1007c146
// Sets the label text, creating the label right of the bitmap on first use; empty text hides the label
void UIRadioButton::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_RADIO(RadioSetText, (intptr_t)&inText, (intptr_t)inFont);
}
// FUNCTION: LLE 0x1007c25d
// Sets the normal bitmap shown while checked
void UIRadioButton::SetUpCheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetUpChecked, (intptr_t)inBitmap); }
// FUNCTION: LLE 0x1007c28f
// Sets the normal bitmap shown while unchecked
void UIRadioButton::SetUpUncheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetUpUnchecked, (intptr_t)inBitmap); }
// FUNCTION: LLE 0x1007c2c1
// Unchecks the button: swaps to the unchecked bitmaps, restores its border and fires the unchecked trigger
void UIRadioButton::UnCheck() { EE_UI_RADIO(RadioUnCheck); }

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
