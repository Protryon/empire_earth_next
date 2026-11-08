// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGMainGroup.h (RECONSTRUCTION; file name inferred). The main
// group of the in-game form (RTTI EEUIFMGMainGroup > EEUIFMGGroup; vtable 0x44ac84, ctor 0x267ec8). Names are
// provisional.
#ifndef EE_EEUIFMGMAINGROUP_H
#define EE_EEUIFMGMAINGROUP_H

class EEComplexUnit;
class EEPlayer;
class EEUnit;
class EEUIFMainGame;
class EEUIFMGStatusBox;
class GEFont;

extern GEFont* gU531930;                    // 0x531930 (a UI font; Empire Earth.cpp's globals; the queue timers' text)

// The in-game form's group base (RTTI EEUIFMGGroup, vtable 0x44ac48; ctor 0x267c73 stores its one argument at +4;
// sizeof 8). Names are provisional.
class EEUIFMGGroup {
public:
    EEUIFMGGroup(EEUIFMainGame* inOwner);   // 0x267c73 (the main game form passes `this`: 0x26522b)
    virtual ~EEUIFMGGroup();                // +0x00 (??_G 0x267c85)
    virtual void Refresh() = 0;             // +0x04
    virtual void Virtual08() = 0;           // +0x08
    virtual void Select() = 0;              // +0x0c
    virtual void Reset();                   // +0x10 (0x1720c6: empty)
    virtual bool WantsRefresh();            // +0x14 (0xac9f3: false)

    EEUIFMainGame* GetOwner() const { return this->mOwner; }
    EEUIFMainGame* mOwner;                  // +0x04 the main game form
};

class EEUIFMGMainGroup : public EEUIFMGGroup {   // sizeof 0x28 (new site)
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
    // EEUIFMGMainGroup.cpp's other members (provisional names)
    void SetUnit(EEUnit* inUnit);           // 0x268070 (a new selection: the mode mL0C, the status box, the buttons)
    // 0x2682ea (ret 0x10): shows command button inControl for EEButtonObject inButton with its hot keys
    void ShowCommandButton(EEPlayer* inPlayer, EEUnit* inUnit, unsigned long inButton, unsigned long inControl);
    void ShowSpecialButtons(EEUnit* inUnit);  // 0x268dee (unit type 0x42: the three queue timers)
    void Update(EEUnit* inUnit);            // 0x268f3f (the same selection again: the queue counts)

    // the ctor 0x267ec8: EEUIFMGGroup(...), the vptr, then these zeroed, +8 = a new object (ctor 0x26ada8)
    EEUIFMGStatusBox* mStatusBox;           // +0x08 (ee/EEUIFMGStatusBox.h)
    long GetMode() const { return this->mMode; }
    long mMode;                             // +0x0c (not set by the ctor) the mode: 0, 3, 4 (unit type 0x321); 1, 2 tested
    long mHitPoints;                        // +0x10 the unit's hit points
    unsigned long mUnitID;                  // +0x14 the selected unit's id
    unsigned long mPreviousID;              // +0x18 the previous id
    long mL1C;                              // +0x1c (not set by the ctor)
    long mL20;                              // +0x20 the unit class's mL294
    bool mHasUnit;                          // +0x24 a unit is selected
    bool mReset;                            // +0x25 (Virtual10 sets it)
};

#endif
