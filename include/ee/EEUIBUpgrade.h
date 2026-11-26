// C:\EEX\Empire Earth\User Interface Thread\EEUIBUpgrade.h (RECONSTRUCTION; file name inferred). The in-game
// improvement button (RTTI EEUIBUpgrade > UIButton; vtable 0x449d54; sizeof 0x130; EEUIFMGAttribute derives from
// it). EEUIBUpgrade.cpp is 0x256845..0x257223. Names of the new virtuals and members are provisional (the
// out-of-line members sit in alphabetical order, which fixes where the new virtuals' names sort). Requires
// lle/UIButton.h.
#ifndef EE_EEUIBUPGRADE_H
#define EE_EEUIBUPGRADE_H
#include "rust/game/screens.h"
#include "lle/UIButton.h"

class EEUnit;
class EEUIBUpgrade;

// One call into ee_screens upgrade.rs: the button, the out text (the tooltip, the refusal's reason, the new cost;
// NULL for none, the call's own mReason for a reason nobody reads), the held text, the mouse location
struct EEUIBUpgradeCall : public EeScreensUpgradeCallObjects {   // the pointers: the Rust record (hud_fields.rs)
    UWideString mText;
    UWideString mReason;
    EEUIBUpgradeCall(EEUIBUpgrade* inButton, UWideString* inOut = NULL, const GE2DPoint* inLocation = NULL)
        : EeScreensUpgradeCallObjects{inButton, inOut, inLocation} {}
    EEUIBUpgradeCall(EEUIBUpgrade* inButton, bool) : EeScreensUpgradeCallObjects{inButton} { this->mOut = &this->mReason; }
    // the record of a call (a temporary's, for the full expression)
    EEUIBUpgradeCall* Ptr() { return this; }
};


class EEUIBUpgrade : public UIButton, public EeScreensUpgradeFields {
public:
    // Creates an improvement button, faintly tinted with a thin border, without a cost label
    EEUIBUpgrade(unsigned long inId, const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inHeight,
                 UIForm* inForm, unsigned long inIndex, bool inScenarioEditor, unsigned long inBorderThickness)
        : UIButton(inId, NULL, NULL, NULL, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex,
                   inBorderThickness),
          EeScreensUpgradeFields{0, NULL, 0, 0, 0, inScenarioEditor, false}
    {
        ee_screens_upb_construct((uintptr_t)this);
    }
    // Snapshots the button and its cost label for drawing
    virtual long CopyData() { return (long)ee_screens_upb_copy_data((uintptr_t)this); }
    // Draws the button text and its cost label
    virtual long DrawTextA() { return (long)ee_screens_upb_draw_text((uintptr_t)this); }
    // (the presses, availability, tooltip, level tracking and cost update: ee_screens upgrade.rs over the host,
    // EEUIBUpgrade.cpp)
    virtual bool GetHelpString(UWideString& outString) const
    {
        return ee_screens_upgrade_help(EEUIBUpgradeCall(const_cast<EEUIBUpgrade*>(this), &outString).Ptr());
    }
    // Marks the button and its cost label dirty
    virtual void SetDirty(bool inDirty) { ee_screens_upb_set_dirty((uintptr_t)this, inDirty); }
    void UpdateCost(const UWideString& inText) { ee_screens_upgrade_update_cost(EEUIBUpgradeCall(this, const_cast<UWideString*>(&inText)).Ptr()); }
    // Presses the button unless the improvement is unavailable, and updates the help text
    virtual void ButtonDown() { ee_screens_upb_button_down((uintptr_t)this); }
    virtual void ButtonUp() { ee_screens_upgrade_button_up(EEUIBUpgradeCall(this, true).Ptr()); }
    // the in-class overrides follow the ctor with the ??_G (0x256943, 0x256949)
    virtual long MouseLButtonDown() { return UIButton::MouseLButtonDown(); }
    virtual long MouseLButtonUp() { return UIButton::MouseLButtonUp(); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)ee_screens_upgrade_mouse_move(EEUIBUpgradeCall(this, NULL, &inLocation).Ptr()); }
    // Shows the hover state only when the improvement can be bought
    virtual void MouseOver() { ee_screens_upb_mouse_over((uintptr_t)this); }
    // Presses the button on a right press, capturing the mouse
    virtual long MouseRButtonDown() { return (long)ee_screens_upb_right_down((uintptr_t)this); }
    virtual long MouseRButtonUp() { return (long)ee_screens_upgrade_mouse_r_up(EEUIBUpgradeCall(this).Ptr()); }
    virtual ~EEUIBUpgrade() {}
    // +0x94: the unit the improvement applies to (the selected group's first unit, or the scenario editor's first
    // selected unit) and whether it can be raised; outReason gets the player's reason why not
    virtual bool GetCanUpgrade(EEUnit*& outUnit, UWideString* outReason)
    {
        return ee_screens_upgrade_can_upgrade(EEUIBUpgradeCall(this, outReason).Ptr(), reinterpret_cast<int64_t*>(&outUnit));
    }
    // +0x98: whether the button is greyed out (the improvement can no longer be raised)
    virtual bool GetDisabled() { return ee_screens_upgrade_disabled(EEUIBUpgradeCall(this).Ptr()); }
    // the base and protected calls of the Rust bodies' shims (EEUIBUpgrade.cpp, hud_bodies.rs)
    static void BaseButtonDown(EEUIBUpgrade* inButton) { inButton->UIButton::ButtonDown(); }
    static void BaseMouseOver(EEUIBUpgrade* inButton) { inButton->UIButton::MouseOver(); }
    static void ButtonDownOf(EEUIBUpgrade* inButton) { inButton->ButtonDown(); }
    static void CaptureOf(EEUIBUpgrade* inButton) { inButton->CaptureMouse(); }

};

#endif
