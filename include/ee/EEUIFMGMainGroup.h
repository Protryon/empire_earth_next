// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.h (RECONSTRUCTION; file name inferred). The main
// group of the in-game form (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84, ctor 0x267ec8). Names are
// provisional.
#ifndef EE_EEUIFMGMAINGROUP_H
#define EE_EEUIFMGMAINGROUP_H

#include "rust/game/screens.h"   // EeScreensMainGroupFields

class EEComplexUnit;
class EEPlayer;
class EEUnit;
class EEUIFMainGame;
class EEUIFMGStatusBox;
class GEFont;

extern GEFont* gU531930;                    // 0x531930 (a UI font; Empire Earth.cpp's globals; the queue timers' text)

// The in-game form's group base (RTTI EEUIFMGGroup, vtable 0x44ac48; ctor 0x267c73 stores its one argument at +4;
// sizeof 8; its unit EEUIFMGGroup.cpp 0x267c73..0x267ca8 held only the ctor and dtor, now inline). Names are
// provisional.
class EEUIFMGGroup {
public:
    EEUIFMGGroup(EEUIFMainGame* inOwner) : mOwner(inOwner) {}   // 0x267c73 (the main game form passes `this`: 0x26522b)
    virtual ~EEUIFMGGroup() {}              // +0x00 (??_G 0x267c85, dtor 0x267ca1)
    virtual void Refresh() = 0;             // +0x04
    virtual void Virtual08() = 0;           // +0x08
    virtual void Select() = 0;              // +0x0c
    virtual void Reset() {}                 // +0x10 (0x1720c6: empty)
    virtual bool WantsRefresh() { return false; }   // +0x14 (0xac9f3: false)

    EEUIFMainGame* mOwner;                  // +0x04 the main game form
};

// The five other panel groups, one class by kind (EE_SCREENS_GROUP_STATISTICS, _CHAT, _DIPLOMACY, _OPTIONS,
// _CIV_BUILDER: mGroups[1..5]): the original EEUIFMGGStatistics (0x267ca8..0x267ec8, vtable 0x44ac64),
// EEUIFMGGChat (0x267b77, 0x44abd8), EEUIFMGGCivBuilder (0x267bb6), EEUIFMGGDiplomacy (0x267bf5) and
// EEUIFMGGOptions (0x267c34), 8 bytes each. Refresh and Select go to ee_screens panel_groups.rs (only the statistics
// group has texts); Virtual08 was empty and WantsRefresh false in all five.
class EEUIFMGGRust : public EEUIFMGGroup {
public:
    EEUIFMGGRust(EEUIFMainGame* inOwner, unsigned long inKind) : EEUIFMGGroup(inOwner), mKind(inKind) {}
    virtual void Refresh();                 // +0x04
    virtual void Virtual08() {}             // +0x08 (empty)
    virtual void Select();                  // +0x0c

    unsigned long mKind;                    // the group's index in the main game form's mGroups
};

// the plain members (+0x0c mMode .. +0x25 mReset: the mode, the shown unit's hit points and ids, mL1C, mL20, the
// flags) are the Rust record EeScreensMainGroupFields (ee_screens main_form_fields.rs), under their names
class EEUIFMGMainGroup : public EEUIFMGGroup, public EeScreensMainGroupFields {
public:
    EEUIFMGMainGroup(EEUIFMainGame* inOwner);   // 0x267ec8 (the main game form's CreateGroup(0))
    virtual ~EEUIFMGMainGroup();            // +0x00 (??_G 0x267f1c)
    virtual void Refresh();                 // +0x04 0x267f61
    virtual void Virtual08();               // +0x08 (empty)
    virtual void Select();                  // +0x0c (empty)
    virtual void Reset() { this->mReset = true; }  // +0x10 0x267f17 (inline provisional name)
    virtual bool WantsRefresh();            // +0x14 0x26915e
    // 0x268162: the first selected unit with a +0x3c value, else the first selected unit, else the local player's
    // +0x9d4 (does not read `this`)
    EEUnit* GetSelectedUnit();
    void Open(EEUIFMainGame* inForm);       // 0x2681c2 (the main game form's Open; provisional name)
    void ShowCommands(EEUnit* inUnit);      // 0x268806 (EEMSavedGroupSelect; provisional name)
    // (SetUnit 0x268070, ShowCommandButton 0x2682ea, ShowSpecialButtons 0x268dee, Update 0x268f3f: Rust's whole,
    // ee_screens main_group.rs)

    // the ctor 0x267ec8: EEUIFMGGroup(...), the vptr, the record zeroed, +8 = a new object (ctor 0x26ada8)
    EEUIFMGStatusBox* mStatusBox;           // +0x08 (ee/EEUIFMGStatusBox.h)
    long GetMode() const { return this->mMode; }
};

#endif
