// The save/load screens' events (class prefixes EEUIESLE/SL; ProcessEvents 0x1e688c..0x1e71cc, EEUIESaveLoadEvents.cpp)
// (RECONSTRUCTION from RTTI/vtables/new sites; header and file names inferred.)
// Classes in ProcessEvent address order (the .cpp's definition order); every ??_G and inline ctor is emitted by the
// creating units (0x27xxxx). Member names from the ProcessEvents.
// Requires lle/UIEvent.h.
#ifndef EE_EEUIESAVELOADEVENTS_H
#define EE_EEUIESAVELOADEVENTS_H

class UIFileListBox;

// vtable 0x44b134; ??_G 0x272484; ProcessEvent 0x1d2aa1 (ICF-shared with EEUIECCCECancel, EEUIECSSEBack); sizeof 0xc; ctor inline; new at 0x27225b
class EEUIESLECancelSave : public UIEvent {
public:
    virtual ~EEUIESLECancelSave() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44ae7c; ??_G 0x271527; ProcessEvent 0x1e688c (clears the unit's "saved" flag); sizeof 0xc; ctor inline; new at 0x2714eb
class EEUIESLEActivateForm : public UIEvent {
public:
    virtual ~EEUIESLEActivateForm() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b148; ??_G 0x27230e; ProcessEvent 0x1e68a5; sizeof 0x10; ctor inline; new at 0x271f8f, 0x2740a5, 0x274409
// (the new sites store the form's control 4; DisplayQuestion hands the event to the static 0x1e6898, which calls
// mList->DeleteSelectedFile())
class EEUIESLEDeleteFile : public UIEvent {
public:
    EEUIESLEDeleteFile(UIFileListBox* inList) : mList(inList) {}   // init list: the new sites store mList before the vptr (0x274429)
    virtual ~EEUIESLEDeleteFile() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UIFileListBox* mList;       // +0x0c
};

// vtable 0x44b15c; ??_G 0x272382; ProcessEvent 0x1e68f5; sizeof 0x30; ctor 0x272339
class EEUIESLELoadFile : public UIEvent {
public:
    EEUIESLELoadFile(const UString& inExtension, const UString& inDirectory, UIForm* inForm)
        : mExtension(inExtension), mDirectory(inDirectory), mForm(inForm) {}   // (provisional name; out of line copy 0x272339)
    virtual ~EEUIESLELoadFile() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UString mExtension;         // +0x0c appended to the selected name
    UString mDirectory;         // +0x1c EEOpenSaveGameFile's directory
    UIForm* mForm;              // +0x2c (provisional name; ctor stores eax)
};

// vtable 0x44b170; ??_G 0x272412; ProcessEvent 0x1e6a39; sizeof 0x30; ctor 0x2723c9
class EEUIESLESaveFile : public UIEvent {
public:
    EEUIESLESaveFile(const UString& inExtension, const UString& inDirectory, UIForm* inForm)
        : mExtension(inExtension), mDirectory(inDirectory), mForm(inForm) {}   // (provisional name; out of line copy 0x2723c9)
    virtual ~EEUIESLESaveFile() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UString mExtension;         // +0x0c appended to the typed name
    UString mDirectory;         // +0x1c below the startup directory (the scenario save's EEASaveGame directory)
    UIForm* mForm;              // +0x2c (provisional name; ctor stores eax)
};

// vtable 0x44b10c; ??_G 0x272459; ProcessEvent 0x1e717c; sizeof 0xc; ctor inline; new at 0x2722c1
class EEUIESLDisplayNewFile : public UIEvent {
public:
    virtual ~EEUIESLDisplayNewFile() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};
#endif
