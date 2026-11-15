// C:\EEX\Low-Level Engine\User Interface\UIBlinkingButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005db0f..0x1005e47c: first includer of UIButton.h (UIBlinkingButton.cpp sorts before UIButton.cpp), so it
// emits UIButton's header-inline members, implicit copy members and vector deleting dtor (0x1005db0f..0x1005df23),
// then UIBlinkingButton's (0x1005df23..0x1005e0c2), the U2D key init chains, the ctor and the out-of-line members.
// UIBlinkingButton::DrawPolygons is ICF-folded into UIButton::DrawPolygons (0x1005eb30, UIButton.cpp's range).
//
// UIBlinkingButton is whole in Rust (rust/crates/ee_ui/src/variantobj.rs, R6 ui round 12): its record and its poll
// (the original's embedded TMPoll, now made and deleted by the Rust button) are the Rust button's.
#include <windows.h>
#include <stddef.h>

#include "rust/ee_ui.h"
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
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UITextLabel.h"
#include "UIButtonRust.h"

typedef EeUiButtonCall C;
#define EE_UI_BLINK(c, ...) UIButtonRust::Call(this, C::c, ##__VA_ARGS__)

// Creates a button that can blink, with a blink period clamped to 50..5000 ms (0 means 250)
UIBlinkingButton::UIBlinkingButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm,
                                   unsigned long inIndex, unsigned long inBorderThickness, unsigned long inBlinkRate)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap, inRectangle, inForm, inIndex,
               inBorderThickness)
{
    EE_UI_BLINK(BlinkInit, (intptr_t)inBlinkRate);
}

// Starts or stops blinking; blinking keeps the button redrawn every frame
void UIBlinkingButton::SetIsBlinking(bool inBlinking) { EE_UI_BLINK(BlinkSetIsBlinking, inBlinking); }
bool UIBlinkingButton::GetIsBlinking() { return EE_UI_BLINK(BlinkIsBlinking) != 0; }

// Fades the button's background, border and bitmaps in and out while blinking (full otherwise), then copies the button
long UIBlinkingButton::CopyData() { return (long)EE_UI_BLINK(BlinkCopyData); }

// Draws the control, then the copied button bitmap; ICF-folded into UIButton::DrawPolygons (0x1005eb30): the same body
long UIBlinkingButton::DrawPolygons()
{
    return UIButton::DrawPolygons();
}

// Draws the button's text (UIButton's)
long UIBlinkingButton::DrawTextA()
{
    return UIButton::DrawTextA();
}
