// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBar.h (RECONSTRUCTION; file name inferred).
// An improvement button drawn as an icon, a status bar and two value labels (RTTI EEUIFMGStatusBar > EEUIBUpgrade;
// vtable 0x44acc4, no new slots; EEUIFMGStatusBar.cpp 0x2691b2..0x269ca8). Members past EEUIBUpgrade (sizeof 0x130)
// from the ctor 0x2691b2 (ESVERIFY names mBar, mValue) and the dtor 0x269808; other names are provisional. Requires
// ee/EEUIBUpgrade.h.
#ifndef EE_EEUIFMGSTATUSBAR_H
#define EE_EEUIFMGSTATUSBAR_H
#include "rust/game/screens.h"

#include "lle/LLEForward.h"
#include "ee/EEUIBUpgrade.h"
#include "lle/UITextLabel.h"

class EEUIFMGStatusBar : public EEUIBUpgrade, public EeScreensStatusBarFields {
public:
    // 0x2691b2: args3..5 unread; arg3 actual layout address, pointer-vs-reference spelling inferred
    // Creates an improvement button drawn as an icon, a green status bar and two value labels (and a queued count),
    // laid out for the main game or the scenario editor (status_bar_calls.rs)
    EEUIFMGStatusBar(unsigned long inId, unsigned long inIndex, UILayoutUtility* inLayout, unsigned long inScreenWidth,
                     unsigned long inScreenHeight, const GERectangle& inRectangle, UIForm* inForm, bool inScenarioEditor,
                     bool inEnabled, bool inArg10)
        : EEUIBUpgrade(inId, inRectangle.GetTopLeft(), (unsigned long)inRectangle.GetWidth(),
                       (unsigned long)inRectangle.GetHeight(), inForm, inIndex, inScenarioEditor, inScenarioEditor == true),
          EeScreensStatusBarFields{NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false, false, inEnabled}
    {
        ee_screens_sba_construct((uintptr_t)this, (uintptr_t)&inRectangle, (uintptr_t)inForm, inScenarioEditor, inArg10);
    }
    // Destroys the status bar button and its parts
    virtual ~EEUIFMGStatusBar() { ee_screens_sba_destroy((uintptr_t)this); }
    // Snapshots the button and its parts for drawing; shows the number of queued improvements
    virtual long CopyData() { return (long)ee_screens_sba_copy_data((uintptr_t)this); }                // +0x14 0x269896
    // Draws the button face (when enabled), the icon, the bar and the labels
    virtual long DrawPolygons() { return (long)ee_screens_sba_draw_polygons((uintptr_t)this); }            // +0x18 0x26993f
    // Draws the parts' texts, the second label when it has text, and the queued count
    virtual long DrawTextA() { return (long)ee_screens_sba_draw_text((uintptr_t)this); }               // +0x1c 0x26997f
    // The presses and the drag: inside an enabled button the icon nudged down-right as pressed (left and right presses),
    // back on the release, following the mouse while captured; then the button's own (ee_screens upgrade.rs bar_mouse over
    // the improvement button's host)
    virtual long MouseLButtonDown() { return this->BarMouse(EE_SCREENS_UP_BAR_L_DOWN, NULL); }        // +0x34 0x269a40
    virtual long MouseLButtonUp() { return this->BarMouse(EE_SCREENS_UP_BAR_L_UP, NULL); }          // +0x38 0x269a9c
    virtual long MouseMove(const GE2DPoint& inLocation) { return this->BarMouse(EE_SCREENS_UP_BAR_MOVE, &inLocation); }   // +0x40 0x269afa
    virtual long MouseRButtonDown() { return this->BarMouse(EE_SCREENS_UP_BAR_R_DOWN, NULL); }        // +0x44 0x269b8b
    virtual long MouseRButtonUp() { return this->BarMouse(EE_SCREENS_UP_BAR_R_UP, NULL); }          // +0x48 0x269be6
    // the mouse handler EE_SCREENS_UP_BAR_* in Rust (ee_screens upgrade.rs bar_mouse; MoveIcon 0x269c43 is its)
    long BarMouse(uint32_t inWhich, const GE2DPoint* inLocation)
    {
        return (long)ee_screens_upgrade_bar_mouse(EEUIBUpgradeCall(this, NULL, inLocation).Ptr(), inWhich);
    }
    // (SetIconBitmap 0x26c42a is Rust's: ee_screens status_box_calls.rs set_icon_bitmap)
    // inline: the value labels' first lines (out of line after their first user, EEUIFMGStatusBox::Fn26CBE1:
    // 0x26dac9 / 0x26dad8, in this order) (provisional names)
    const UWideString& GetValueText() const { return static_cast<UITextLabel*>(this->mValue)->GetText(0); }
    const UWideString& GetValue2Text() const { return static_cast<UITextLabel*>(this->mValue2)->GetText(0); }
};

#endif
