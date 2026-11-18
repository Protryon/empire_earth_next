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
class EESEARust;
class GEFont;
class UIComboBox;
class UIForm;
class UILBString;

// Whole in Rust (ee_screens editor_unit.rs: its controls, the palettes, sorts and owners and the class list they
// fill, the cargo, the placement state, the mouse and keys): the class holds the object and its members forward. The
// build tools stay by-value C++ members (their Build and Cancel call back PlaceWall, EndWall, PlaceGate, EndGate).
struct EeScreensSeUnit;

class EESEUnit : public EEUIFSEMainGroup {
public:
    EESEUnit(unsigned long& ioIndex, EEUIFScenarioEditor* inForm, GEFont* inFont, UIComboBox* inPlayers);
    virtual ~EESEUnit();                    // +0x00 0x19bb38 (??_G 0x19bb1c)
    virtual void Update();                  // +0x04 0x19bd4a (each frame: the wall/gate ghosts)
    virtual void Leave();                   // +0x08 0x19c07b (leaves the tab)
    virtual void Draw(const GERectangle& inRectangle);        // +0x0c 0x19c1b3 (draws the panel background)
    virtual void Enter();                   // +0x18 0x19c28d (enters the tab)
    virtual void MouseMove(const GE2DPoint& inLocation);      // +0x24 0x19c84b (the mouse over the game view)
    virtual void UpdateKeys();              // +0x28 0x19cada (the keyboard)

    void PlaceWall();                       // 0x19bcd5 (mPlaceWall from mBuildWall's ends)
    void EndWall();
    void PlaceGate();
    void EndGate();
    void ReselectUnitType();                // 0x19c05d (re-selects the unit type at the mouse while placing)
    void ShowCargo(bool inShow);
    void ShowUnitStatus(EEUnit* inUnit);    // 0x19c1a1 (the upgrade panel shows inUnit)
    float GetRotationAngle();               // 0x19c246 (the rotation button's yaw; EESELoadThread Handle402)
    void SelectUnitType(const GE2DPoint& inLocation);   // 0x19c37b (the unit list's selection starts placing it)
    void UpdateSelection();                 // 0x19c535 (the cargo list after the selection changed)
    void RefreshUnitList();                 // 0x19c64b (the unit classes for the mode, sort and layer)

    // the placement state read from outside the tab (the move and placement actions, the boarding finder, the
    // render's lasso, the editor gate): the unit under the mouse while placing (-1: none), the selection tool
    long GetUnitUnderMouse() const;
    void SetUnitUnderMouse(long inUnit);
    EEUICSelectUnits* GetSelectUnits() const;
    // the editor gate's: 1 placing, 2 a wall, 4 a gate, 8 a held gate; the palette, sort and owner last listed
    long GetPlacingFlags() const;
    void GetListState(long& outMode, long& outSort, long& outLayer) const;

    GERhombus mRhombus;                     // +0x04
    EESEUIBuildWall mBuildWall;             // +0x1c
    EESEUIBuildGate mBuildGate;             // +0x3c
    GERectangle mPanelRectangle;            // +0x5c (Enter: (0, 0)-(224, 605))
    UIForm* mForm;                          // the editor form (the panel's background is drawn on it)
    EeScreensSeUnit* mRust;                 // the tab (editor_unit.rs)
};

// the shared control maker's unit tab lists (EESERBUnitTypes.cpp SeplMake): inKind 1 the class list
// (EEUIFSELBUnitList), 2 the cargo list (EEUIFSELBCargoList)
UILBString* EESEUnitNewList(long inKind, unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation,
                            unsigned long inWidth, unsigned long inHeight, UIForm* inForm, unsigned long inIndex);

#endif
