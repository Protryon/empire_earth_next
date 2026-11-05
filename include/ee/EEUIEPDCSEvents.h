// The civilization choice form's events (class prefix EEUIEPDCS; form 0x23; ProcessEvents 0x1e5e2e..0x1e6711)
// (RECONSTRUCTION from RTTI/vtables/new sites; header and file names inferred.)
// EEUIEPDCSEvents.cpp defines FormOpen, SelectCiv and Ok out of line. Declaration order = the order of the ??_G
// COMDATs after FormOpen (SelectCiv 0x1e6021, Ok 0x1e604c, Cancel 0x1e6077, ClearCivStats 0x1e60e7).
// ClearCivStats and LoadCiv have implicit dtors (their ??_G store no vptr of their own) and in-class ProcessEvents
// (each sits between its first user and its ??_G); Cancel's in-class ProcessEvent is ICF-folded into
// EEUIEEEEMainMenu's (0x1d6811).
// Requires lle/UIEvent.h, lle/UIForm.h, lle/UIManager.h, ee/EEUIManager.h, ee/EECBUIListBox.h, ee/EEUserInterface.h,
// ee/EECivilizationManager.h, lle/UITextLabel.h, lle/UResourceFile.h.
#ifndef EE_EEUIEPDCSEVENTS_H
#define EE_EEUIEPDCSEVENTS_H
#include "ee/EECivUpgrade.h"
#include "ee/EEStringIDs.h"
#include "lle/UIControl.h"
// vtable 0x44ba70; ??_G 0x277e0d; ProcessEvent 0x1e5e2e; sizeof 0xc; ctor inline; new at 0x277d2f, 0x27fae6
class EEUIEPDCSFormOpen : public UIEvent {
public:
    virtual ~EEUIEPDCSFormOpen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44ba5c; ??_G 0x277e38 (after EEUserInterface 0x277c1f, its first user); ProcessEvent 0x27cf38; sizeof 0xc
class EEUIEPDCSFormClose : public UIEvent {
public:
    virtual ~EEUIEPDCSFormClose() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x4469e0; ??_G 0x1e6021; ProcessEvent 0x1e610c; sizeof 0xc; ctor inline; new at 0x1e5f11
class EEUIEPDCSSelectCiv : public UIEvent {
public:
    virtual ~EEUIEPDCSSelectCiv() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x4469f4; ??_G 0x1e604c; ProcessEvent 0x1e64ed; sizeof 0xc; ctor inline; new at 0x1e5e81
class EEUIEPDCSOk : public UIEvent {
public:
    virtual ~EEUIEPDCSOk() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x446a08; ??_G 0x1e6077; ProcessEvent ICF-folded (0x1d6811); sizeof 0xc; ctor inline; new at 0x1e5e44
class EEUIEPDCSCancel : public UIEvent {
public:
    virtual ~EEUIEPDCSCancel() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        inForm->GetManager()->Pop();
        return 0;
    }
};

// vtable 0x4469cc; ProcessEvent 0x1e60a2 and ??_G 0x1e60e7 after FormOpen; sizeof 0xc; ctor inline; new at 0x1e5f44
class EEUIEPDCSClearCivStats : public UIEvent {
public:
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        unsigned long theSelection;
        UIForm* theForm = gUIManager->GetFormByID(0x23);
        EECBUIListBox* theStats = (EECBUIListBox*)theForm->GetControl(4);
        if (((EECBUIListBox*)theForm->GetControl(3))->GetCurrentSelection(theSelection) != kErrorOK || theSelection == 0)
            theStats->DeleteAllEntries();
        return 0;
    }
};

// vtable 0x446a1c; ctor 0x1e639a (out of line after its first user, SelectCiv), ProcessEvent 0x1e63c9, ??_G 0x1e64bf;
// sizeof 0x1c
class EEUIEPDCSLoadCiv : public UIEvent {
public:
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        UWideString theMessage;
        UWideString theName;
        std::vector<EECivUpgrade> theUpgrades;
        UIForm* theForm = gUIManager->GetFormByID(0x23);
        EECBUIListBox* theCivs = (EECBUIListBox*)theForm->GetControl(3);
        EECBUIListBox* theStats = (EECBUIListBox*)theForm->GetControl(4);
        UITextLabel* theLabel = (UITextLabel*)theForm->GetControl(5);
        bool theLoaded;
        if (theCivs && theStats && theLabel) {
            theLoaded = EELoadCivilization(this->mFile, EEGetCivilizationDirectory(false), theName, theUpgrades) == kErrorOK;
            if (theLoaded == true) {
                theStats->SetCivilizationUpgrades(theUpgrades);
                theLabel->SetDisplayMode(kDisplayModeHidden);
            } else {
                theCivs->ResetSelection();
                gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, theMessage);
                theLabel->SetText(theMessage);
            }
        }
        return 0;
    }

    UString mFile;                          // +0x0c the selected civilization's file (SelectCiv sets it)
};

#endif
