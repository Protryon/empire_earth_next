// C:\EEX\Empire Earth\Scenario Editor\EESERBUnitTypes.h (RECONSTRUCTION; file name inferred).
// The small control and action classes of the scenario editor's player tab (EESERBUnitTypes.cpp 0x173218..0x17acf6,
// EESEPlayerScreen's unit). Names are provisional; layouts only as far as the unit's code shows them. Requires
// lle/UIRadioButton.h, lle/UIComboBox.h, lle/UIButton.h, ee/EEAction.h.
#ifndef EE_EESERBUNITTYPES_H
#define EE_EESERBUNITTYPES_H

class GEBitmap;
class EEUIFScenarioEditor;

// 0x5318cc: the world's player count the editor's player tab sets (EESEAChangeWorldPlayerCount; provisional name)
extern unsigned long& gMaxPlayers;

// RTTI EESERBUnitTypes > UIRadioButton; vtable 0x44303c; sizeof 0x114 (no members). The out-of-line ctor 0x173218
// (`ret 0x2c`) builds the rectangle (location, width, height; unsigned: fild qword) and passes inScaleBitmap false.
// ??_G 0x173295 stores the vptr first: a user-declared empty dtor. Check (+0x94) 0x1732b8 refreshes the player tab.
class EESERBUnitTypes : public UIRadioButton {
public:
    EESERBUnitTypes(unsigned long inID, const UWideString& inText, GEFont* inFont, const GE2DPoint& inLocation,
                    unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex,
                    unsigned long inBorderThickness, bool inIsPushButton, bool inIsChecked);   // 0x173218
    virtual ~EESERBUnitTypes() {}
    virtual void Check();                   // +0x94 0x1732b8
};

// RTTI EESECBMaxPlayer > UIComboBox; vtable 0x443140; ctor 0x175001 (`ret 0x2c`, EESETSAComboBox's shape),
// ??_G 0x175079 (vptr first: a user-declared empty dtor). ChangeSelection (+0x8c) 0x1732d7: the player tab's player
// count = the selection + 2.
class EESECBMaxPlayer : public UIComboBox {
public:
    EESECBMaxPlayer(unsigned long inID, unsigned long inTextBoxType, const std::vector<UWideString>& inStrings,
                    GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                    unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars,
                    unsigned long inBorderThickness);   // 0x175001
    virtual ~EESECBMaxPlayer() {}
protected:
    virtual void ChangeSelection();         // +0x8c 0x1732d7
};

// RTTI EESEAChangeWorldPlayerCount > EEAction; vtable 0x44310c (??_G EEAction's 0x1e1b69, +0x08 the shared empty
// slot); sizeof 0x10. Built inline by EESEPlayerScreen::SetPlayerCount 0x173653 and posted to the UI thread.
class EESEAChangeWorldPlayerCount : public EEAction {
public:
    EESEAChangeWorldPlayerCount(unsigned long inCount) : mCount(inCount) {}
    virtual void Virtual4();                // +0x04 0x1737a3
    virtual void Virtual8() {}              // +0x08 (0x1720c6)

    unsigned long mCount;                   // +0x0c
};

// RTTI EESEUIBDiplomacy > UIButton; vtable 0x443298; sizeof 0x108. A 22x22 two-state button (mState 0/1, help
// strings 0x2198/0x2199) over three static bitmap pairs (0x531fc0/0x531fc8/0x531fd0; loaded by 0x1768a6, released by
// 0x176b18). ctor 0x176818 (`ret 0x14`), ??_G 0x176883. Names are provisional.
class EESEUIBDiplomacy : public UIButton {
public:
    EESEUIBDiplomacy(unsigned long inID, const GE2DPoint& inLocation, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness);   // 0x176818
    virtual ~EESEUIBDiplomacy() {}
    static void LoadBitmaps(EEUIFScenarioEditor* inEditor);   // 0x1768a6 (provisional name)
    static void ReleaseBitmaps();           // 0x176b18
    void UpdateBitmaps();                   // 0x176b49
protected:
    virtual void ButtonUp();                // +0x8c 0x176baf
public:
    unsigned long mState;                   // +0x104

    static GEBitmap* sNormalBitmaps[2];     // 0x531fc0
    static GEBitmap* sFocusedBitmaps[2];    // 0x531fc8
    static GEBitmap* sPressedBitmaps[2];    // 0x531fd0
};

// RTTI EESEEChangeStartEpoch > UIEvent; vtable from the 0x178a29 control builder; ??_G 0x179a00 (vptr first: a
// user-declared empty dtor). ProcessEvent 0x178958: the start epoch combo box (inEntity) changed: refill the end
// epoch combo box (+0x0c) with the epochs from the start on (strings 0x75c1.., 15 epochs), keeping its epoch.
class EESEEChangeStartEpoch : public UIEvent {
public:
    virtual ~EESEEChangeStartEpoch() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);   // +0x04 0x178958

    UIComboBox* mEndEpochComboBox;          // +0x0c
};

#endif
