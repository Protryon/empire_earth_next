// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaGameEvents.h (RECONSTRUCTION; header and file names inferred,
// from the class prefix EEUIEWNAG). The WON game room form's (0x20) events.
// ProcessEvents: EEUIEWonNetArenaGameEvents.cpp (0x1e9fed..0x1eafcb). Declaration order is alphabetical: the order of
// the ??_G VC6 emits after OpenScreen (UpdateForm's comes after ActivateForm, its first user).
// Requires lle/UIEvent.h.
#ifndef EE_EEUIEWONNETARENAGAMEEVENTS_H
#define EE_EEUIEWONNETARENAGAMEEVENTS_H

#include "lle/LLEForward.h"

// 0x1eadd6: sets inEvent as inControl's trigger event and drops the creator's reference. Name is provisional. A static
// (non-inline) function of this header: every WON screen's OpenScreen calls it out of line, and WNAG's copy sits right
// after its first user (OpenScreen 0x1ea884), before the ??_G block, which is where VC6 emits a deferred static; an
// external function defined after OpenScreen lands after the ??_G block, an inline one is expanded. The copies of the
// other WON units are ICF-folded into WNAG's. Needs lle/UIControl.h and lle/UIEvent.h first.
static void EEUIEWNSetTriggerEvent(UIEvent* inEvent, UIControl* inControl, unsigned long inTrigger)
{
    inControl->SetTriggerEvent(inTrigger, inEvent);
    inEvent->Dereference();
}

class EEUIEWNAGActivateForm : public UIEvent {          // vtable 0x446b58; form 0x20's activate event
public:
    virtual ~EEUIEWNAGActivateForm() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGBack : public UIEvent {                  // vtable 0x446be4; button 18
public:
    virtual ~EEUIEWNAGBack() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGCheckPassword : public UIEvent {         // vtable 0x446b80; check box 28 (game password)
public:
    virtual ~EEUIEWNAGCheckPassword() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGClickGameFilter : public UIEvent {       // vtable 0x446b6c; check boxes 32..34
public:
    virtual ~EEUIEWNAGClickGameFilter() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGCloseForm : public UIEvent {             // vtable 0x446b30
public:
    virtual ~EEUIEWNAGCloseForm() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGDeactivateForm : public UIEvent {        // vtable 0x446b44
public:
    virtual ~EEUIEWNAGDeactivateForm() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGDoubleclickUser : public UIEvent {       // vtable 0x446b94; the users list (control 8)
public:
    virtual ~EEUIEWNAGDoubleclickUser() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGHostOrJoinGame : public UIEvent {        // vtable 0x446ba8; button 17, edit box 24, games box 13
public:
    virtual ~EEUIEWNAGHostOrJoinGame() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGOpenScreen : public UIEvent {            // vtable 0x43e4d0; created by the UI's event factory
public:
    virtual ~EEUIEWNAGOpenScreen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGSendChat : public UIEvent {              // vtable 0x446b1c; edit box 16
public:
    virtual ~EEUIEWNAGSendChat() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGToggleHostScreen : public UIEvent {      // vtable 0x446bd0; radio button 6
public:
    virtual ~EEUIEWNAGToggleHostScreen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGToggleJoinScreen : public UIEvent {      // vtable 0x446bbc; radio button 5
public:
    virtual ~EEUIEWNAGToggleJoinScreen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEWNAGUpdateForm : public UIEvent {            // vtable 0x446b08; form 0x20's update event
public:
    virtual ~EEUIEWNAGUpdateForm() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

#endif
