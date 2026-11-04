// The scenario load screen's events (class prefixes EEUIESCL/SCLE; ProcessEvents 0x1e853a..0x1e8a24)
// (RECONSTRUCTION from RTTI/vtables/new sites; header and file names inferred.)
// Classes in ProcessEvent address order (the .cpp's definition order); declaration order, inline ProcessEvents
// and dtors are for the unit to prove (a unit's ??_G COMDATs follow its first user in declaration order).
// Requires lle/UIEvent.h.
#ifndef EE_EEUIESCENARIOLOADEVENTS_H
#define EE_EEUIESCENARIOLOADEVENTS_H

// vtable 0x446a8c; ??_G 0x1e89dd; ProcessEvent 0x1e853a; sizeof 0x2c; ctor 0x1e899b
class EEUIESCLLoadScreen : public UIEvent {
public:
    EEUIESCLLoadScreen(const UString& inExtension, const UString& inDirectory)
        : mExtension(inExtension), mDirectory(inDirectory) {}   // out of line 0x1e899b, after its first user (SCLEOpen)
    virtual ~EEUIESCLLoadScreen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UString mExtension;         // +0x0c the list box's file extension (gScenarioExtension)
    UString mDirectory;         // +0x1c below the startup directory ("data\" "Scenarios\")
};

// vtable 0x44b800; ??_G 0x2741b9; ProcessEvent 0x1e86b6; sizeof 0x30; ctor 0x274170
class EEUIESCLELoadFile : public UIEvent {
public:
    EEUIESCLELoadFile(const UString& inExtension, const UString& inDirectory, UIForm* inForm)
        : mExtension(inExtension), mDirectory(inDirectory), mForm(inForm) {}   // (provisional name; out of line copy 0x274170)
    virtual ~EEUIESCLELoadFile() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    UString mExtension;         // +0x0c appended to the selected name
    UString mDirectory;         // +0x1c EEOpenSaveGameFile's directory
    UIForm* mForm;              // +0x2c (provisional name; ctor stores eax)
};

// vtable 0x44b7ec; ??_G 0x274145; ProcessEvent 0x1e88be; sizeof 0xc; ctor inline; new at 0x2740d6
class EEUIESCLECancel : public UIEvent {
public:
    virtual ~EEUIESCLECancel() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};

// vtable 0x44b06c; ??_G 0x271dda; ProcessEvent 0x1e88e6; sizeof 0xc; ctor inline; new at 0x271b30
class EEUIESCLEOpen : public UIEvent {
public:
    virtual ~EEUIESCLEOpen() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
};
#endif
