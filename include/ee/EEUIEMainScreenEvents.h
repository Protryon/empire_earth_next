// The main screen's events (class prefixes EEUIESSE/MSE/MS; ProcessEvents 0x1e1b8d..0x1e2292). The main menu's own
// (MSE: 0x1e1e48..0x1e2144) and the multiplayer screen's open event (MS) are Rust's (ee_screens menus.rs,
// ee/EEScreenEvent.h); the loading screen's open event stays.
// (RECONSTRUCTION from RTTI/vtables/new sites; header and file names inferred.)
// Classes in ProcessEvent address order (the .cpp's definition order); declaration order, inline ProcessEvents
// and dtors are for the unit to prove (a unit's ??_G COMDATs follow its first user in declaration order).
// Requires lle/UIEvent.h.
#ifndef EE_EEUIEMAINSCREENEVENTS_H
#define EE_EEUIEMAINSCREENEVENTS_H

// vtable 0x44b044; ??_G 0x271cd8; ProcessEvent 0x1e1b8d; sizeof 0xc; ctor inline; new at 0x271b7a
class EEUIESSEOpen : public UIEvent {
public:
    virtual ~EEUIESSEOpen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

#endif
