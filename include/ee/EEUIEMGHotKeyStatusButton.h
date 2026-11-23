// C:\EEX\Empire Earth\User Interface Thread\EEUIEMGHotKeyStatusButton.h (RECONSTRUCTION; file name inferred). The
// hot-key event of a main game command button (RTTI EEUIEMGHotKeyStatusButton > UIEvent; vtable 0x44aca0; sizeof
// 0x14 from EEUIFMGMainGroup.cpp's `new`s). The ctor is inline (every `new` site stores the fields and one vptr);
// ??_G 0x2687db (user-declared empty dtor: three vptr stores) follows its first user
// EEUIFMGMainGroup::Fn2682EA. ProcessEvent 0x2601da (ret 8) lies outside EEUIFMGMainGroup.cpp. Requires
// lle/UIEvent.h, ee/EEUIFMGStatusButton.h.
#ifndef EE_EEUIEMGHOTKEYSTATUSBUTTON_H
#define EE_EEUIEMGHOTKEYSTATUSBUTTON_H

#include "rust/game/screens.h"   // ee_screens_mg_hot_key_event (main_group_calls.rs)

class EEUIEMGHotKeyStatusButton : public UIEvent {
public:
    EEUIEMGHotKeyStatusButton(EEUIFMGStatusButton* inButton, bool inPressed) : mButton(inButton), mPressed(inPressed) {}
    virtual ~EEUIEMGHotKeyStatusButton() {}
    // 0x2601da: pressed: mButton->Fn26F16C() and the local player's statistics Reset(0x14, 1); else Fn26F174()
    // (the button pressed or released by hot key, pressing also resetting the saved-group state: Rust's; kErrorOK)
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return (long)ee_screens_mg_hot_key_event((uintptr_t)this); }

    EEUIFMGStatusButton* mButton;           // +0x0c
    bool mPressed;                          // +0x10
};

#endif
