// C:\EEX\Empire Earth\Scenario Editor\EESEUnit.h (RECONSTRUCTION; file name inferred).
// The editor's unit tab group (RTTI EESEUnit > EEUIFSEMainGroup; vtable 0x444e9c, sizeof 0x140, ctor 0x19a93e); the
// editor's tab group 3. EESEUnit.cpp is 0x199afd..0x19cc39 (its helper controls first, then the ctor and the members).
// Layout from the ctor and the dtor 0x19bb38; member names are provisional. Requires ee/EEUIFSEMainGroup.h,
// lle/GERectangle.h (includes the build-mode headers itself).
#ifndef EE_EESEUNIT_H
#define EE_EESEUNIT_H

#include "lle/GERhombus.h"
#include "ee/EEUIBuildWall.h"
#include "ee/EEUIBuildGate.h"
#include "ee/EESEUIBuildWall.h"

class EEUnit;
class EEUIFScenarioEditor;
class EEUIFSELBUnitList;
class EEUIFSELBCargoList;
class EEUIFSEBUnitType;
class EEUIFSEBRotation;
class EEUIFSEBRemoveCargo;
class EEUIFSEUpgradePanel;
class EEUICSelectUnits;
class EESEUICCommandUnits;
class EESEAMouseClick;
class EESEALasso;
class EESEAPlaceUnit;
class EESEARust;
class EESEAMoveUnit;
class GEBitmap;
class GEFont;
class UIComboBox;
class UIEvent;
class UIRadioButton;
class UITextLabel;

class EESEUnit : public EEUIFSEMainGroup {
public:
    EESEUnit(unsigned long& ioIndex, EEUIFScenarioEditor* inForm, GEFont* inFont, UIComboBox* inPlayers);   // 0x19a93e
    virtual ~EESEUnit();                    // +0x00 0x19bb38 (??_G 0x19bb1c)
    virtual void Update();                  // +0x04 0x19bd4a (each frame: the wall/gate ghosts)
    virtual void Leave();                   // +0x08 0x19c07b (leaves the tab)
    virtual void Draw(const GERectangle& inRectangle);        // +0x0c 0x19c1b3 (draws the panel background)
    virtual void Enter();                   // +0x18 0x19c28d (enters the tab)
    virtual void MouseMove(const GE2DPoint& inLocation);      // +0x24 0x19c84b (the mouse over the game view)
    virtual void UpdateKeys();              // +0x28 0x19cada (the keyboard)

    // the player the tab edits: 0 with the world radio button, else the player combo box's selection + 1 (inline;
    // defined in EESEUnit.cpp, its only user)
    long GetCurrentPlayer();
    void PlaceWall();                       // 0x19bcd5 (mPlaceWall from mBuildWall's ends)
    void EndWall();                         // 0x19bd2d
    void PlaceGate();                       // 0x19bfdc
    void EndGate();                         // 0x19c026
    void ReselectUnitType();                // 0x19c05d (re-selects the unit type at the mouse while placing)
    void ShowCargo(bool inShow);            // 0x19c141
    // 0x19c1a1: forwards inUnit to +0xb4's 0x1a551a (EEAUpgradeImprovement::Virtual4 after an editor upgrade)
    // (provisional name)
    void ShowUnitStatus(EEUnit* inUnit);
    long GetLayer();                        // 0x19c221 (0 world, 1 land/2 sea by the two radio buttons)
    // 0x19c246 (+0xb8's 0x19a5d7: a float by +0x114's case; EESELoadThread Handle402) (provisional name)
    float GetRotationAngle();
    long GetSortMode();                     // 0x19c251 (0..3 by the three sort radio buttons)
    long GetUnitTypeMode();                 // 0x19c287 (the unit type buttons' global 0x531ff0)
    void SelectUnitType(const GE2DPoint& inLocation);   // 0x19c37b (the unit list's selection starts placing it)
    // 0x19c535: refreshes the tab after the selection changed (the lasso/click actions) (provisional name)
    void UpdateSelection();
    void RefreshUnitList();                 // 0x19c64b (the unit classes for the mode, sort and layer)

    GERhombus mRhombus;                     // +0x04
    EESEUIBuildWall mBuildWall;             // +0x1c
    EESEUIBuildGate mBuildGate;             // +0x3c
    GERectangle mPanelRectangle;            // +0x5c (Virtual18: (0, 0)-(224, 605))
    long mL6C;                              // +0x6c (zeroed by the ctor)
    long mL70;                              // +0x70
    long mL74;                              // +0x74
    long mL78;                              // +0x78
    long mL7C;                              // +0x7c
    long mL80;                              // +0x80
    long mL84;                              // +0x84
    GEBitmap* mBackground;                  // +0x88 ("textures\scenunit")
    EEUIFSELBUnitList* mUnitList;           // +0x8c (0x3f1)
    EEUIFSEBUnitType* mUnitTypeButtons[6];  // +0x90 (0x406..0x40b)
    UIRadioButton* mAlphaSortButton;        // +0xa8 (0x3ee; the dtor deletes the three one by one)
    UIRadioButton* mEpochSortButton;        // +0xac (0x3ef)
    UIRadioButton* mFamilySortButton;       // +0xb0 (0x3f0)
    EEUIFSEUpgradePanel* mUpgradePanel;     // +0xb4 (new 0x60, ctor 0x1a4e99)
    EEUIFSEBRotation* mRotation;            // +0xb8 (0x3f5)
    EEUIFSELBCargoList* mCargoList;         // +0xbc (0x3f2)
    EEUIFSEBRemoveCargo* mRemoveCargo;      // +0xc0 (0x3f4)
    UIEvent* mSortEvent;                    // +0xc4 (EEUIFSEESortUnitList)
    UITextLabel* mSortLabel;                // +0xc8 (0x3ed)
    UIRadioButton* mWorldButton;            // +0xcc (1000)
    UIRadioButton* mSeaButton;              // +0xd0 (0x3ea)
    UITextLabel* mTitleLabel;               // +0xd4 (0x3f3)
    UIComboBox* mPlayerComboBox;            // +0xd8 (the editor's)
    GEBitmap* mUnitTypeBitmaps[12];         // +0xdc (normal, pressed) x 6 (flat: the dtor 0x59bb38 loops 12 x 4 bytes)
    EEUICSelectUnits* mSelectUnits;         // +0x10c (0x3f6)
    EESEUICCommandUnits* mCommandUnits;     // +0x110 (0x3f7)
    EESEAMouseClick* mMouseClick;           // +0x114
    EESEALasso* mLasso;                     // +0x118
    EESEAPlaceUnit* mPlaceUnit;             // +0x11c
    EESEARust* mPlaceGate;                  // +0x120
    EESEARust* mPlaceWall;                  // +0x124
    EESEAMoveUnit* mMoveUnit;               // +0x128 (new 0x20, ctor 0x1a75e0)
    long mUnitTypeMode;                     // +0x12c (6: none yet)
    long mSortMode;                         // +0x130 (3)
    long mLayer;                            // +0x134 (2)
    long mUnitUnderMouse;                   // +0x138 (-1: the unit id under the mouse)
    bool mPlacing;                          // +0x13c
    bool mPlacingWall;                      // +0x13d
    bool mPlacingGate;                      // +0x13e
    bool mIsGateHeld;                       // +0x13f
};

#endif
