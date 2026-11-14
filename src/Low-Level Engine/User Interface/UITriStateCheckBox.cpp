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

// FUNCTION: LLE 0x100807f4
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

// FUNCTION: LLE 0x10080916
// Sets the check box width, shrinking the text label to fit beside the box
void UITriStateCheckBox::ChangeWidth(unsigned long inWidth) { EE_UI_TRI(TriChangeWidth, (intptr_t)inWidth); }
// FUNCTION: LLE 0x100809ab
// Copies the draw state of the box and its label; deletes a label whose text was cleared
long UITriStateCheckBox::CopyData() { return (long)EE_UI_TRI(TriCopyData); }
// FUNCTION: LLE 0x100809ec
// Draws the box's polygons, then the label's
long UITriStateCheckBox::DrawPolygons() { return (long)EE_UI_TRI(TriDrawPolygons); }
// FUNCTION: LLE 0x10080a05
// Draws the control text, then the label's text
long UITriStateCheckBox::DrawTextA() { return (long)EE_UI_TRI(TriDrawText); }
unsigned long UITriStateCheckBox::GetCheckState() const { return (unsigned long)EE_UI_TRI(TriCheckState); }
UITextLabel* UITriStateCheckBox::GetTextLabel() const { return (UITextLabel*)EE_UI_TRI(TextLabel); }
// FUNCTION: LLE 0x10080a1d
// Sets the check state, shows its bitmaps and fires the checked trigger (an unknown state only stores the value)
void UITriStateCheckBox::SetCheckState(unsigned long inState) { EE_UI_TRI(TriSetCheckState, (intptr_t)inState); }
// FUNCTION: LLE 0x10080ac1
// Moves the check box and its text label
void UITriStateCheckBox::SetLocation(const GE2DPoint& inLocation)
{
    EE_UI_TRI(TriSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
}
// FUNCTION: LLE 0x10080b1c
// Sets the label text, creating the label on first use; empty text hides the label
void UITriStateCheckBox::SetText(const UWideString& inText, GEFont* inFont)
{
    EE_UI_TRI(TriSetText, (intptr_t)&inText, (intptr_t)inFont);
}
// Cycles the check, then releases the button
void UITriStateCheckBox::ButtonUp() { EE_UI_TRI(TriButtonUp); }
// FUNCTION: LLE 0x10080be6
// Advances the check state off -> on -> not -> off
void UITriStateCheckBox::CycleCheck() { EE_UI_TRI(TriCycle); }

// Header-inline members first emitted here (bodies in lle/UITriStateCheckBox.h)
// FUNCTION: LLE 0x100805c4
// FUNCTION: LLE 0x100805e6
// FUNCTION: LLE 0x100805ea

// Compiler-generated: the implicit copy ctor/operator=, ??_E and the U2D key init chains (names in
// analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100805fb
// SYNTHETIC: LLE 0x1008063a
// SYNTHETIC: LLE 0x10080673
// SYNTHETIC: LLE 0x100806d4
// SYNTHETIC: LLE 0x100806de
// SYNTHETIC: LLE 0x100806ee
// SYNTHETIC: LLE 0x100806fa
// SYNTHETIC: LLE 0x10080704
// SYNTHETIC: LLE 0x1008070e
// SYNTHETIC: LLE 0x1008071e
// SYNTHETIC: LLE 0x1008072a
// SYNTHETIC: LLE 0x10080734
// SYNTHETIC: LLE 0x1008073e
// SYNTHETIC: LLE 0x1008074e
// SYNTHETIC: LLE 0x1008075a
// SYNTHETIC: LLE 0x10080764
// SYNTHETIC: LLE 0x1008076e
// SYNTHETIC: LLE 0x1008077e
// SYNTHETIC: LLE 0x1008078a
// SYNTHETIC: LLE 0x10080794
// SYNTHETIC: LLE 0x1008079e
// SYNTHETIC: LLE 0x100807ae
// SYNTHETIC: LLE 0x100807ba
// SYNTHETIC: LLE 0x100807c4
// SYNTHETIC: LLE 0x100807ce
// SYNTHETIC: LLE 0x100807de
// SYNTHETIC: LLE 0x100807ea
