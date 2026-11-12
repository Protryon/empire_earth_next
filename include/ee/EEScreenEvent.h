// The game screens' one event class (NEW; docs/RUST_PLAN.md "R6 game screens", step 2). A form that moved to Rust
// whole (rust/crates/ee_screens: menus.rs) has no event classes of its own: each of its control and form events is an
// EEScreenEvent carrying the event's code (EE_SCREENS_*, rust/ee_screens.h), and ProcessEvent hands it to the Rust
// dispatcher, which calls back the toolkit and the game (ee_screenscpp_*, UI Events/EEScreens.cpp).
// Requires lle/UIEvent.h.
#ifndef EE_EESCREENEVENT_H
#define EE_EESCREENEVENT_H

class EEScreenEvent : public UIEvent {
public:
    explicit EEScreenEvent(unsigned long inEvent) : mEvent(inEvent) {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    unsigned long mEvent;                   // EE_SCREENS_*
};

// The event of a code: an EEScreenEvent, or a C++ event class for an EE_SCREENS_CPP_* code; NULL for 0
UIEvent* EECreateScreenEvent(unsigned long inEvent);

#endif
