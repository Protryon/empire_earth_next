// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildGate.h (RECONSTRUCTION; file name inferred). The gate
// placement mode: follows the mouse with a gate ghost on the wall under it, builds on a left
// click, cancels on a right click (RTTI EEUIBuildGate, no base; vtable 0x444f5c; sizeof 0x20: by-value members of
// EEUIFMainGame +0x4b4 and of the scenario editor form +0x3c). EEUIBuildGate.cpp is 0x25619b..0x25643d (the members in
// alphabetical order). Slot and member names are provisional. Requires lle/GE2DPoint.h.
#ifndef EE_EEUIBUILDGATE_H
#define EE_EEUIBUILDGATE_H
#include "ee/EETechTreeTechID.h"

class EEUnitClass;

class EEUIBuildGate {
public:
    // inline: EEUIFMainGame's ctor 0x262e73 stores +0x10 then the vptr (0x262ed5)
    EEUIBuildGate() : mTechnology(kTechNone) {}
    virtual ~EEUIBuildGate() {}             // +0x00 (??_G 0x19cc1c in the scenario editor form's unit: the vptr store)
    virtual void Cancel();                  // +0x04 0x2561b6 (ICF: EEUIBuildWall's too) the main form leaves the mode
    virtual void Initialize(long inPlayer); // +0x08 0x2561ff
    virtual void Terminate();               // +0x0c 0x256245
    virtual void Update(const GE2DPoint& inLocation);   // +0x10 0x25627d (the mouse location)
    virtual void Build(const GE2DPoint& inLocation, long inDirection);   // +0x14 0x25619b
    void ClearGhostUnits();                 // 0x2561c9 (un-highlights the walls, clears the ghosts)

    void SetUnitClass(EEUnitClass* inClass) { this->mUnitClass = inClass; }
    EEUnitClass* mUnitClass;                // +0x04 the gate's class (its +0x1bc type: the media)
    long mTileX;                            // +0x08 (= -99) the tile of the last build
    long mTileY;                            // +0x0c (= -99)
    long mTechnology;                       // +0x10 (= 100000)
    long mWallType;                         // +0x14 the highlighted walls' unit type (0x21176e/0x211957)
    long mDirection;                        // +0x18 (= 2) the gate's direction
    long mPlayer;                           // +0x1c the player index
};

#endif
