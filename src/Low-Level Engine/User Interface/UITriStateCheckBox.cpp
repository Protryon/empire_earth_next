// C:\EEX\Low-Level Engine\User Interface\UITriStateCheckBox.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100805c4..0x10080c07: the header-inline members (dtor, GetType, ButtonUp), the implicit copy ctor/operator= and
// ??_E (this unit is the first includer of UITriStateCheckBox.h), the U2D key init chains (0x100806d4), then the ctor
// and the out-of-line members (alphabetical, the protected CycleCheck last).
//
// UITriStateCheckBox is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record is the Rust
// button's and every method below forwards to it. The constructor's UIButton part stays here.
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
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UITriStateCheckBox.h"
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
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "UIButtonRust.h"

typedef EeUiButtonCall C;
#define EE_UI_TRI(c, ...) UIButtonRust::Call(this, C::c, ##__VA_ARGS__)

// Creates a three-state check box (off, on, not) at most 19 pixels square with an optional text label beside it
UITriStateCheckBox::UITriStateCheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont,
                                       const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                                       unsigned long inBorderThickness)
    : UIButton(inId, inForm->GetManager()->GetCheckOff(), NULL, inForm->GetManager()->GetCheckDownOff(),
               UIButtonRust::Rect(ee_ui_button_box_rect(UIButtonRust::Rect(inRectangle), false)), inForm, inIndex,
               inBorderThickness)
{
    EE_UI_TRI(TriInit, (intptr_t)&inText, (intptr_t)inFont, inRectangle.GetHeight());
}

// Sets the check box width, shrinking the text label to fit beside the box
void UITriStateCheckBox::ChangeWidth(unsigned long inWidth) { EE_UI_TRI(TriChangeWidth, (intptr_t)inWidth); }
// Copies the draw state of the box and its label; deletes a label whose text was cleared
long UITriStateCheckBox::CopyData() { return (long)EE_UI_TRI(TriCopyData); }
// Draws the box's polygons, then the label's
long UITriStateCheckBox::DrawPolygons() { return (long)EE_UI_TRI(TriDrawPolygons); }
// Draws the control text, then the label's text
long UITriStateCheckBox::DrawTextA() { return (long)EE_UI_TRI(TriDrawText); }
unsigned long UITriStateCheckBox::GetCheckState() const { return (unsigned long)EE_UI_TRI(TriCheckState); }
UITextLabel* UITriStateCheckBox::GetTextLabel() const { return (UITextLabel*)EE_UI_TRI(TextLabel); }
// Sets the check state, shows its bitmaps and fires the checked trigger (an unknown state only stores the value)
void UITriStateCheckBox::SetCheckState(unsigned long inState) { EE_UI_TRI(TriSetCheckState, (intptr_t)inState); }
// Moves the check box and its text label
void UITriStateCheckBox::SetLocation(const GE2DPoint& inLocation)
{
    EE_UI_TRI(TriSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// Sets the label text, creating the label on first use; empty text hides the label
void UITriStateCheckBox::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_TRI(TriSetText, (intptr_t)&inText, (intptr_t)inFont);
}
// Cycles the check, then releases the button
void UITriStateCheckBox::ButtonUp() { EE_UI_TRI(TriButtonUp); }
