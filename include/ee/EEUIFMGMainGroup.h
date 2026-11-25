// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.h (RECONSTRUCTION; file name inferred). The main
// group of the in-game form (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84, ctor 0x267ec8). Names are
// provisional.
#ifndef EE_EEUIFMGMAINGROUP_H
#define EE_EEUIFMGMAINGROUP_H

#include "rust/game/screens.h"   // EeScreensMainGroupFields

class EEUnit;
class EEUIFMainGame;
class EEUIFMGStatusBox;
#include "lle/LLEForward.h"

extern GEFont* gU531930;                    // 0x531930 (a UI font; Empire Earth.cpp's globals; the queue timers' text)

// The original's panel groups (EEUIFMGGroup, vtable 0x44ac48, and the five other kinds EEUIFMGGStatistics,
// EEUIFMGGChat, EEUIFMGGCivBuilder, EEUIFMGGDiplomacy, EEUIFMGGOptions, 8 bytes each) are gone (UI lane round 16): the
// main game form holds the main group (mMainGroup); the other five are kinds Rust runs by the form's mCurrentGroup
// (ee_screens panel_groups.rs: Refresh and Select; their Virtual08 and Reset were empty, WantsRefresh false).

// the plain members (+0x0c mMode .. +0x25 mReset: the mode, the shown unit's hit points and ids, mL1C, mL20, the
// flags) are the Rust record EeScreensMainGroupFields (ee_screens main_form_fields.rs), under their names
class EEUIFMGMainGroup : public EeScreensMainGroupFields {
public:
    EEUIFMGMainGroup(EEUIFMainGame* inOwner);   // 0x267ec8 (the main game form's CreateGroup(0))
    // (??_G 0x267f1c)
    ~EEUIFMGMainGroup() { delete this->mStatusBox; }
    // The group's members but ShowCommands (the original's Refresh, SetUnit, GetSelectedUnit, Open, ShowCommandButton,
    // ShowSpecialButtons, Update, WantsRefresh): ee_screens main_group.rs over the host below
    void Refresh() { ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_REFRESH, NULL, 0); }
    void Reset() { this->mReset = true; }  // 0x267f17 (inline provisional name)
    bool WantsRefresh() { return ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_WANTS_REFRESH, NULL, 0) != 0; }
    // 0x268162: the first selected unit with a +0x3c value, else the first selected unit, else the local player's
    // +0x9d4 (does not read `this`)
    EEUnit* GetSelectedUnit()
    {
        return reinterpret_cast<EEUnit*>(ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_SELECTED_UNIT, NULL, 0));
    }
    void Open(EEUIFMainGame* inForm) { ee_screens_main_group_member(this, EE_SCREENS_MGR_DO_OPEN, NULL, 0); }       // 0x2681c2 (the main game form's Open; provisional name)
    // Lays out the command and research buttons of the player's selected unit (civ power buttons, the class's
    // commands, its available techs with hot keys, airport timers); hides them all for others (main_group_calls.rs)
    // (main_group_calls.rs)
    void ShowCommands(EEUnit* inUnit) { ee_screens_mg_show_commands((uintptr_t)this, (uintptr_t)inUnit); }      // 0x268806 (EEMSavedGroupSelect; provisional name)
    // (SetUnit 0x268070, ShowCommandButton 0x2682ea, ShowSpecialButtons 0x268dee, Update 0x268f3f: Rust's whole,
    // ee_screens main_group.rs)

    // the ctor 0x267ec8: the owner, the record zeroed, +8 = a new object (ctor 0x26ada8)
    EEUIFMainGame* mOwner;                  // +0x04 the main game form
    EEUIFMGStatusBox* mStatusBox;           // +0x08 (ee/EEUIFMGStatusBox.h)
    long GetMode() const { return this->mMode; }
};

#endif
