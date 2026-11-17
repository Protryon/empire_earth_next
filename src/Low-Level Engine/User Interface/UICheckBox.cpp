// C:\EEX\Low-Level Engine\User Interface\UICheckBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005ef4b..0x100602e4: the header-inline members (and implicit copy members, ??_E) of the classes this unit is the
// first to include, in include order: UISlider, UIScrollButton, UIScrollBar, UITextLabel, UICheckBox; then the U2D
// key init chains (0x1005fb21, the rasterizer header comes next), UIRadioButton's inline members (0x1005fc41); then
// its own code (0x1005fdef: the ctor, then the members) and the vector<UWideString*>::_Tidy COMDAT.
//
// UICheckBox is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record is the Rust button's
// and every method below forwards to it; the label is a part the Rust button holds (UIButton.cpp's host makes it).
// The constructor's UIButton part (the box rectangle, the manager's check bitmaps) stays here.
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
#include "UIButtonRust.h"

typedef EeUiButtonCall C;
#define EE_UI_CHECK(c, ...) UIButtonRust::Call(this, C::c, ##__VA_ARGS__)

// Creates an unchecked check box with an optional text label to the right of the box bitmap
UICheckBox::UICheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                       UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inScaleBitmap)
    : UIButton(inId, inForm->GetManager()->GetCheckOff(), NULL, inForm->GetManager()->GetCheckDownOff(),
               UIButtonRust::Rect(ee_ui_button_box_rect(UIButtonRust::Rect(inRectangle), inScaleBitmap)),
               inForm, inIndex, inBorderThickness)
{
    EE_UI_CHECK(CheckInit, (intptr_t)&inText, (intptr_t)inFont, inRectangle.GetHeight(), inScaleBitmap ? 1.0f : 0.0f);
}

// Resizes the check box and its label to a new width. No marker: ICF-folded into UITriStateCheckBox::ChangeWidth
// (0x10080916), the same body over the same offsets.
void UICheckBox::ChangeWidth(unsigned long inWidth) { EE_UI_CHECK(CheckChangeWidth, (intptr_t)inWidth); }
// Copies the button and its label for the draw pass, deleting the label once the text was cleared
long UICheckBox::CopyData() { return (long)EE_UI_CHECK(CheckCopyData); }
// Draws the box bitmap (scaled to the control height when scaling) and the label
long UICheckBox::DrawPolygons() { return (long)EE_UI_CHECK(CheckDrawPolygons); }
// Draws the control's text and the label. No marker: ICF-folded into UITriStateCheckBox::DrawTextA (0x10080a05).
long UICheckBox::DrawTextA() { return (long)EE_UI_CHECK(CheckDrawText); }
bool UICheckBox::GetIsChecked() const { return EE_UI_CHECK(CheckIsChecked) != 0; }
UITextLabel* UICheckBox::GetTextLabel() const { return (UITextLabel*)EE_UI_CHECK(TextLabel); }
// Checks or unchecks the box (unless disabled), swapping its bitmaps and firing the checked or unchecked trigger
void UICheckBox::SetChecked(bool inChecked) { EE_UI_CHECK(CheckSetChecked, inChecked); }
// Enables or disables the box; a disabled box shows pressed, an enabled one normal
// Fixed (kept): enabling shows the box normal, not pressed
void UICheckBox::SetDisabled(bool inDisabled) { EE_UI_CHECK(CheckSetDisabled, inDisabled); }
// Moves the check box and its label
void UICheckBox::SetLocation(const GE2DPoint& inLocation)
{
    EE_UI_CHECK(CheckSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// Sets the label text, creating the label on first use; empty text hides the label
void UICheckBox::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_CHECK(CheckSetText, (intptr_t)&inText, (intptr_t)inFont);
}
// Toggles the check, then releases the button
void UICheckBox::ButtonUp() { EE_UI_CHECK(CheckButtonUp); }
// Presses the box unless it is disabled
void UICheckBox::ButtonDown() { EE_UI_CHECK(CheckButtonDown); }
// Handles a left-button press unless the box is disabled
long UICheckBox::MouseLButtonDown() { return (long)EE_UI_CHECK(CheckMouseLButtonDown); }
// Handles a left-button release unless the box is disabled
long UICheckBox::MouseLButtonUp() { return (long)EE_UI_CHECK(CheckMouseLButtonUp); }
// Handles mouse movement unless the box is disabled
long UICheckBox::MouseMove(const GE2DPoint& inLocation)
{
    return (long)EE_UI_CHECK(CheckMouseMove, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// Shows the box pressed
void UICheckBox::MouseOver() { UIButton::MouseOver(); }
