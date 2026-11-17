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

#include "rust/ui/buttonobj.h"
#include "rust/ui/checkobj.h"

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

// Checks the button: unchecks its checked neighbour, swaps to the checked bitmaps and fires the checked trigger
void UIRadioButton::Check() { EE_UI_RADIO(RadioCheck); }
// Snapshots the button and its label (deleted once its text is cleared); a checked push button loses its border
long UIRadioButton::CopyData() { return (long)EE_UI_RADIO(RadioCopyData); }
// Draws the button (its bitmap scaled to the control height when scaled), then its label
long UIRadioButton::DrawPolygons() { return (long)EE_UI_RADIO(RadioDrawPolygons); }
// Draws the button's text and its label
long UIRadioButton::DrawTextA() { return (long)EE_UI_RADIO(RadioDrawText); }
bool UIRadioButton::GetIsChecked() const { return EE_UI_RADIO(RadioIsChecked) != 0; }
UITextLabel* UIRadioButton::GetTextLabel() const { return (UITextLabel*)EE_UI_RADIO(TextLabel); }
bool UIRadioButton::GetTabable() const { return EE_UI_RADIO(RadioTabable) != 0; }
// Checks the button (the virtual Check), then releases it
void UIRadioButton::ButtonUp() { EE_UI_RADIO(RadioButtonUp); }
// Presses the button, except a checked push button
long UIRadioButton::MouseLButtonDown() { return (long)EE_UI_RADIO(RadioMouseLButtonDown); }
// Releases the button (plain button behaviour)
long UIRadioButton::MouseLButtonUp() { return UIButton::MouseLButtonUp(); }
// Sets the pressed bitmap shown while checked
void UIRadioButton::SetDownCheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetDownChecked, (intptr_t)inBitmap); }
// Sets the pressed bitmap shown while unchecked
void UIRadioButton::SetDownUncheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetDownUnchecked, (intptr_t)inBitmap); }
// Moves the button, keeping its label to the right of the bitmap
void UIRadioButton::SetLocation(const GE2DPoint& inLocation)
{
    EE_UI_RADIO(RadioSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// Sets the label text, creating the label right of the bitmap on first use; empty text hides the label
void UIRadioButton::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_RADIO(RadioSetText, (intptr_t)&inText, (intptr_t)inFont);
}
// Sets the normal bitmap shown while checked
void UIRadioButton::SetUpCheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetUpChecked, (intptr_t)inBitmap); }
// Sets the normal bitmap shown while unchecked
void UIRadioButton::SetUpUncheckedBitmap(GEBitmap* inBitmap) { EE_UI_RADIO(RadioSetUpUnchecked, (intptr_t)inBitmap); }
// Unchecks the button: swaps to the unchecked bitmaps, restores its border and fires the unchecked trigger
void UIRadioButton::UnCheck() { EE_UI_RADIO(RadioUnCheck); }
