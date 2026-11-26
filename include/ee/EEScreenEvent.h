// The game screens' one event class (NEW; docs/RUST_PLAN.md "R6 game screens", step 2). A form that moved to Rust
// whole (rust/crates/ee_game/src/screens: menus.rs) has no event classes of its own: each of its control and form events is an
// EEScreenEvent carrying the event's code (EE_SCREENS_*, rust/ee_screens.h), and ProcessEvent hands it to the Rust
// dispatcher, which calls back the toolkit and the game (ee_screenscpp_*, UI Events/EEScreens.cpp).
// Requires lle/UIEvent.h.
#ifndef EE_EESCREENEVENT_H
#define EE_EESCREENEVENT_H
#include "rust/game/screens.h"
#include "lle/UIEvent.h"

// An event of the game's C++ carries two arguments (UI lane round 16, ee_screens cpp_events.rs EE_SCREENS_EV_*: the
// original's EEUIECancel (the form to open), EEUIELoadUnit, EEUIEMGHotKeyStatusButton (the button, pressed),
// EEUIEProcessTimerExpired and EEUIEndCinematicModeEvent)
class EEScreenEvent : public UIEvent {
public:
    explicit EEScreenEvent(unsigned long inEvent, uintptr_t inA = 0, uintptr_t inB = 0) : mEvent(inEvent), mA(inA), mB(inB) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        return ee_screens_event_args(this->mEvent, this->mA, this->mB, inForm, inEntity);
    }

    unsigned long mEvent;                   // EE_SCREENS_*
    uintptr_t mA;
    uintptr_t mB;
};

// The event of a code: an EEScreenEvent, or a C++ event class for an EE_SCREENS_CPP_* code; NULL for 0
UIEvent* EECreateScreenEvent(unsigned long inEvent);

// A form's middle-button grab scroll (0 the main game form's, 1 the scenario editor's; ee_screens camera.rs): its event
// made with the form, its record (not grabbing, no saved mouse move event) fresh; released with the form
UIEvent* EECreateGrabScroll(unsigned long inForm);
void EEReleaseGrabScroll(unsigned long inForm);

#endif
