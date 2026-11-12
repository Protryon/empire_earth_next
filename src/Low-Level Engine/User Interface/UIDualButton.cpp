// C:\EEX\Low-Level Engine\User Interface\UIDualButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10069cc7..0x10069d6b: the ctor, the dtor and the right-button overrides. No inline block and no init chains
// (the class's implicit copy members are emitted first by UIControlsTable.cpp); UIEditBox.cpp's U2D key chains
// start at 0x10069d6b.
//
// The right-button handlers are Rust (rust/crates/ee_ui/src/button.rs, docs/RUST_PLAN.md R6).
#include <windows.h>
#include <stddef.h>

#include "rust/ee_ui.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIDualButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

// FUNCTION: LLE 0x10069cc7
// Creates a button that also reacts to the right mouse button
UIDualButton::UIDualButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                           GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm,
                           unsigned long inIndex, unsigned long inBorderThickness)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap, inRectangle, inForm, inIndex,
               inBorderThickness)
{
}

// FUNCTION: LLE 0x10069cf7
// Destroys the dual button (members only)
UIDualButton::~UIDualButton()
{
}

// FUNCTION: LLE 0x10069d02
// Presses the button on a right click, capturing the mouse and firing the right-button-down trigger
long UIDualButton::MouseRButtonDown()
{
    return ee_ui_dual_button_mouse_r_button_down(UIButtonRust::Refs(this));
}

// FUNCTION: LLE 0x10069d24
// Releases the mouse; a right release over the button pops it up and fires the right-button-up trigger
long UIDualButton::MouseRButtonUp()
{
    return ee_ui_dual_button_mouse_r_button_up(UIButtonRust::Refs(this));
}
