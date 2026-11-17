// C:\EEX\Low-Level Engine\User Interface\UIScrollButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007d013..0x1007d139: the ctor and ButtonDown only. No U2D key init chains: the unit does not include the
// rasterizer header. The header-inline members are emitted first by UICheckBox.cpp (0x1005f272..).
//
// UIScrollButton is whole in Rust (rust/crates/ee_ui/src/variantobj.rs, R6 ui round 12): the constructor's UIButton
// part stays here; the record, the polls and the stepping are the Rust button's.
#include <windows.h>
#include <stddef.h>

#include "rust/ui/buttonobj.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UISlider.h"
#include "lle/UIScrollButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

typedef EeUiButtonCall C;

// Creates a 20x20 scroll arrow button that steps a slider down or up, with a 500 ms delay and 100 ms repeat
UIScrollButton::UIScrollButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inPressedBitmap,
                               const GE2DPoint& inLocation, UIForm* inForm, UISlider* inSlider,
                               unsigned long inDecrement, unsigned long inIndex)
    : UIButton(inId, inNormalBitmap, NULL, inPressedBitmap,
               GERectangle(inLocation, GE2DPoint(inLocation.GetX() + 19.0f,
                                                 inLocation.GetY() + 19.0f)),
               inForm, inIndex, 0)
{
    UIButtonRust::Call(this, C::ScrollInit, (intptr_t)inSlider, (intptr_t)inDecrement);
}

// Steps the slider while held: once on press, again after the 500 ms delay, then every 100 ms
void UIScrollButton::ButtonDown() { UIButtonRust::Call(this, C::ScrollButtonDown); }
// Stops the delay and the repeat, then releases the button
void UIScrollButton::ButtonUp() { UIButtonRust::Call(this, C::ScrollButtonUp); }
