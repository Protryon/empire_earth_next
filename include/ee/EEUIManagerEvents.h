// C:\EEX\Empire Earth\User Interface Thread\EEUIManagerEvents.h (RECONSTRUCTION; file name inferred). Event
// classes EEUIManager.cpp's factories create that no event header declared yet (each would belong to its form's
// event header; kept together here until those units are written). Requires lle/UIEvent.h,
// ee/EEUserInterface.h, ee/EEUIFMGUnitHack.h.
#ifndef EE_EEUIMANAGEREVENTS_H
#define EE_EEUIMANAGEREVENTS_H

class EEUIECEClose : public UIEvent {
public:
    virtual ~EEUIECEClose() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMGDisplayCivBuilder : public UIEvent {
public:
    virtual ~EEUIEMGDisplayCivBuilder() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMGDisplayDiplomacy : public UIEvent {
public:
    virtual ~EEUIEMGDisplayDiplomacy() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMGDisplayStatistics : public UIEvent {
public:
    virtual ~EEUIEMGDisplayStatistics() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMGEndScenarioDialog : public UIEvent {
public:
    virtual ~EEUIEMGEndScenarioDialog() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMSETestButton : public UIEvent {
public:
    virtual ~EEUIEMSETestButton() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIESPELearningScenarios : public UIEvent {
public:
    virtual ~EEUIESPELearningScenarios() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIESPLECancel : public UIEvent {
public:
    virtual ~EEUIESPLECancel() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUISIEMouseOutButton : public UIEvent {
public:
    virtual ~EEUISIEMouseOutButton() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUISIEMouseOverButton : public UIEvent {
public:
    virtual ~EEUISIEMouseOverButton() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIECECampaignNameChanged : public UIEvent {
public:
    virtual ~EEUIECECampaignNameChanged() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

class EEUIEMGIncrementRallyTimer : public UIEvent {
public:
    EEUIEMGIncrementRallyTimer(unsigned char inTimer) { this->mTimer = inTimer; }  // body: stored after the vptr
    virtual ~EEUIEMGIncrementRallyTimer() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    unsigned char mTimer;                   // +0x0c (1..3)
};

// ProcessEvent 0x273345 inline (emitted after the main game factory): the unit hack panel loads the unit data
class EEUIELoadUnit : public UIEvent {
public:
    virtual ~EEUIELoadUnit() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        ((EEUIFMainGame*)inForm)->mUnitHack->LoadUnitData();
        return kErrorOK;
    }
};

// ProcessEvent 0x2724af inline: opens the form it was made for (the save/load screens' cancel)
class EEUIECancel : public UIEvent {
public:
    EEUIECancel(UIForm* inForm) : mForm(inForm) {}   // init list: stored before the vptr (EECreateSaveLoadEvent 0x672296)
    virtual ~EEUIECancel() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) { return gUserInterface->OpenForm(this->mForm); }

    UIForm* mForm;                          // +0x0c
};

#endif
