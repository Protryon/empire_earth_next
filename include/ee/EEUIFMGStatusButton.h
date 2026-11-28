// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusButton.h (RECONSTRUCTION; file name inferred). The main game
// form's command buttons (controls 0x4e..0x56 and 0x6e..0xcd; RTTI EEUIFMGStatusButton > UIButton, vtable 0x44ad98;
// EEUIFMGStatusButton.cpp 0x26de97..0x271319). A button shows either a tech tree node
// (a unit to build, a technology to research: mPowerRegeneration) or an EEButtonObject command (mB148, mButton). Layout from the
// ctor 0x26de97 and the dtor 0x26e225 (sizeof 0x14c); provisional names. Requires lle/UIButton.h, lle/UIEvent.h.
#ifndef EE_EEUIFMGSTATUSBUTTON_H
#define EE_EEUIFMGSTATUSBUTTON_H

#include "rust/game/records.h"
#include "rust/game/screens.h"   // EeScreensStatusButtonFields

#include "lle/UIButton.h"

class EEUIFMGStatusButton : public UIButton, public EeScreensStatusButtonFields, public EeScreensStatusButtonObjects {
public:
    // 0x26de97 (ret 0xc; the user-defined control factory)
    EEUIFMGStatusButton(UIManager* inManager, const GERectangle& inRectangle, UIControlsTable& inTable);
    // Destroys the command button and its parts
    virtual ~EEUIFMGStatusButton() { ee_screens_sbt_destroy((uintptr_t)this); }
    // Presses a command or tech button: runs the command, starts placing a building, wall or gate, queues the
    // research (Shift: five), or starts a calamity when a selected unit can pay it; refusals beep and explain in the chat
    // (ee_screens status_button.rs)
    virtual void ButtonUp();                // +0x8c 0x26e274
    // Snapshots the button for drawing: whether it is available, its icon and the queued count; the availability of the button: a command button by its own test (mB148), else its technology through the
    // player's WriteDetailsHook, or for a calamity with a cost whether a selected unit can pay it (each branch stores
    // mAvailable; VC6 merges the stores)
    virtual long CopyData();                // +0x14 0x26e734
    // Draws the button's icon, dithered over the last drawn rectangle when unavailable (status_button_calls.rs)
    virtual long DrawPolygons() { return (long)ee_screens_sbt_draw_polygons((uintptr_t)this); }            // +0x18 0x26eaf4
    // Draws the queued count over the button
    virtual long DrawTextA() { return (long)ee_screens_sbt_draw_text((uintptr_t)this); }               // +0x1c 0x26ebb5
    virtual bool GetHelpString(UWideString& outString) const;   // +0x20 0x26ebdf
    // Presses the button from its hot key
    void HotKeyDown() { this->ButtonDown(); }                      // 0x26f16c (the hot-key event, pressed)
    // Presses a tech button on a right press (ee_screens hud_more.rs)
    virtual long MouseRButtonDown() { return (long)ee_screens_sbm_right_down((uintptr_t)this); }   // +0x44 0x26f186
    virtual long MouseRButtonUp();          // +0x48 0x26f1a4
    // for the presses' host (ee_screens status_button.rs): UIButton::ButtonUp, and the mouse in the rectangle
    bool IsMouseInside() const { return this->GetRectangle().Contains(this->GetMouseLocation()); }
    // Moves the button, its icon and its queue count text
    virtual void SetLocation(const GE2DPoint& inLocation)
    {
        ee_screens_sbt_set_location((uintptr_t)this, inLocation.GetX(), inLocation.GetY());
    }   // +0x6c 0x26fb19
    // Releases a pressed button when tabbing away (ee_screens hud_more.rs)
    virtual void TabbedFrom() { ee_screens_sbm_tabbed_from((uintptr_t)this); }   // +0x78 0x26fb79
    // the base's press and tab for the Rust bodies' shims
    void BaseButtonDown() { UIButton::ButtonDown(); }
    void BaseTabbedFrom() { UIButton::TabbedFrom(); }
    void BaseCaptureMouse() { this->CaptureMouse(); }
    // Handles tabbing to the button
    virtual void TabbedTo() { UIButton::TabbedTo(); }                // +0x74 0x26fb8d

    // the plain members (+0x118 mLastErrorTime, +0x12c mTechnology .. +0x14b mDrawIcon) are the Rust record
    // EeScreensStatusButtonFields' (ee_screens main_form_fields.rs), under their names
};

// (RTTI EEUIEndCinematicModeEvent, vtable 0x44ae68, ProcessEvent 0x26fb93: the screen event EE_SCREENS_EV_END_CINEMATIC,
// ee/EEScreenEvent.h)

#endif
