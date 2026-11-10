// C:\EEX\Low-Level Engine\User Interface\UIBlinkingButton.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005db0f..0x1005e47c: first includer of UIButton.h (UIBlinkingButton.cpp sorts before UIButton.cpp), so it
// emits UIButton's header-inline members, implicit copy members and vector deleting dtor (0x1005db0f..0x1005df23),
// then UIBlinkingButton's (0x1005df23..0x1005e0c2), the U2D key init chains, the ctor and the out-of-line members.
// UIBlinkingButton::DrawPolygons is ICF-folded into UIButton::DrawPolygons (0x1005eb30, UIButton.cpp's range).
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
#include "lle/TMPoll.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"

// FUNCTION: LLE 0x1005e1e2
// Creates a button that can blink, with a blink period clamped to 50..5000 ms (0 means 250)
UIBlinkingButton::UIBlinkingButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap,
                                   GEBitmap* inPressedBitmap, const GERectangle& inRectangle, UIForm* inForm,
                                   unsigned long inIndex, unsigned long inBorderThickness, unsigned long inBlinkRate)
    : UIButton(inId, inNormalBitmap, inFocusedBitmap, inPressedBitmap, inRectangle, inForm, inIndex,
               inBorderThickness),
      mPoll(250, 0, false)
{
    // the record from mBlinkPhase on (rust/crates/ee_ui/src/button_variants.rs BlinkFields), written whole
#define EE_UI_BLINK_FIELD(member, field)                                                                             \
    static_assert(offsetof(UIBlinkingButton, member) - offsetof(UIBlinkingButton, mBlinkPhase) ==                    \
                      offsetof(EeUiBlinkFields, field),                                                              \
                  "ee_ui/src/button_variants.rs BlinkFields")
    EE_UI_BLINK_FIELD(mBlinkRate, blink_rate);
    EE_UI_BLINK_FIELD(mPressedOpacity, pressed_opacity);
    EE_UI_BLINK_FIELD(mNormalOpacity, normal_opacity);
    EE_UI_BLINK_FIELD(mBgOpacity, bg_opacity);
    EE_UI_BLINK_FIELD(mFullBorderOpacity, full_border_opacity);
    EE_UI_BLINK_FIELD(mSavedAlwaysDirty, saved_always_dirty);
    EE_UI_BLINK_FIELD(mNeedsOpacities, needs_opacities);
    EE_UI_BLINK_FIELD(mIsBlinking, is_blinking);
#undef EE_UI_BLINK_FIELD
    static_assert(sizeof(UIBlinkingButton) - offsetof(UIBlinkingButton, mBlinkPhase) >= sizeof(EeUiBlinkFields),
                  "ee_ui/src/button_variants.rs BlinkFields");
    ee_ui_blink_init(reinterpret_cast<EeUiBlinkFields*>(&this->mBlinkPhase), inBlinkRate, this->mAlwaysDirty);
    this->mPoll.SetTimeDelay(this->mBlinkRate);
}

// FUNCTION: LLE 0x1005e27a
// Starts or stops blinking; blinking keeps the button redrawn every frame
void UIBlinkingButton::SetIsBlinking(bool inBlinking)
{
    if (this->mIsBlinking == inBlinking)
        return;
    this->mIsBlinking = inBlinking;
    this->mPoll.Stop();
    if (this->mIsBlinking) {
        this->mPoll.Start();
        this->SetAlwaysDirty(true);
        this->SetDirty(true);
        this->SetDisplayMode(kDisplayModeShown);
    } else {
        this->SetAlwaysDirty(this->mSavedAlwaysDirty);
        this->SetDirty(true);
    }
}

// FUNCTION: LLE 0x1005e2dc
// Fades the button's background, border and bitmaps in and out while blinking (full otherwise), then copies the button
long UIBlinkingButton::CopyData()
{
    bool thePolled = this->mPoll.Poll() != 0;
    EeUiBlinkShow theShow = ee_ui_blink_copy_data(reinterpret_cast<EeUiBlinkFields*>(&this->mBlinkPhase), thePolled,
                                                  this->mPoll.GetTimeLeft(), this->mState, this->GetBgOpacity(),
                                                  this->GetBorderOpacity());
    this->SetBgOpacity(theShow.background);
    this->SetBorderOpacity(theShow.border);
    if (this->mPressedBitmap)
        this->mPressedBitmap->SetOpacity(theShow.pressed);
    if (this->mNormalBitmap)
        this->mNormalBitmap->SetOpacity(theShow.normal);
    return UIButton::CopyData();
}

// Draws the control, then the copied button bitmap; ICF-folded into UIButton::DrawPolygons (0x1005eb30): the same body
long UIBlinkingButton::DrawPolygons()
{
    long theResult = UIControl::DrawPolygons();
    if (this->mCopiedBitmap) {
        this->mCopiedBitmap->SetLocation(this->mBitmapLocation);
        theResult = this->GetRasterizer()->Draw(this->mCopiedBitmap, this->GetScreenIndex());
    }
    return theResult;
}

// FUNCTION: LLE 0x1005e477
// Draws the button's text (UIButton's)
long UIBlinkingButton::DrawTextA()
{
    return UIButton::DrawTextA();
}

// Header-inline members first emitted here (UIButton.h, then UIBlinkingButton.h; names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005db0f
// SYNTHETIC: LLE 0x1005db36
// SYNTHETIC: LLE 0x1005db4c
// SYNTHETIC: LLE 0x1005db63
// SYNTHETIC: LLE 0x1005db97
// SYNTHETIC: LLE 0x1005dbb4
// SYNTHETIC: LLE 0x1005dbe8
// SYNTHETIC: LLE 0x1005dc1c
// SYNTHETIC: LLE 0x1005dd3c
// SYNTHETIC: LLE 0x1005dec2
// SYNTHETIC: LLE 0x1005df23
// SYNTHETIC: LLE 0x1005df2a
// SYNTHETIC: LLE 0x1005dfc6
// SYNTHETIC: LLE 0x1005e05c
// SYNTHETIC: LLE 0x1005e061

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1005e0c2
// SYNTHETIC: LLE 0x1005e0cc
// SYNTHETIC: LLE 0x1005e0dc
// SYNTHETIC: LLE 0x1005e0e8
// SYNTHETIC: LLE 0x1005e0f2
// SYNTHETIC: LLE 0x1005e0fc
// SYNTHETIC: LLE 0x1005e10c
// SYNTHETIC: LLE 0x1005e118
// SYNTHETIC: LLE 0x1005e122
// SYNTHETIC: LLE 0x1005e12c
// SYNTHETIC: LLE 0x1005e13c
// SYNTHETIC: LLE 0x1005e148
// SYNTHETIC: LLE 0x1005e152
// SYNTHETIC: LLE 0x1005e15c
// SYNTHETIC: LLE 0x1005e16c
// SYNTHETIC: LLE 0x1005e178
// SYNTHETIC: LLE 0x1005e182
// SYNTHETIC: LLE 0x1005e18c
// SYNTHETIC: LLE 0x1005e19c
// SYNTHETIC: LLE 0x1005e1a8
// SYNTHETIC: LLE 0x1005e1b2
// SYNTHETIC: LLE 0x1005e1bc
// SYNTHETIC: LLE 0x1005e1cc
// SYNTHETIC: LLE 0x1005e1d8
