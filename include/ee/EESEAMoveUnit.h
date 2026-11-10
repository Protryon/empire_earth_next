// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAMoveUnit.h (RECONSTRUCTION; file name inferred). The unit tab's
// move action (RTTI EESEAMoveUnit > EEAction; vtable 0x44597c, sizeof 0x20: EESEUnit's ctor news 0x20 into +0x128).
// Everything is in EESEAMoveUnit.cpp 0x1a75e0..0x1a8284 (ctor, ??_G, Virtual4, the entry's implicit operator=, pick
// up / put down / cancel, then the vector<EESEAMoveUnitEntry> COMDATs). Member names
// follow EESEUnit.cpp's declaration; all names are provisional.
// Requires ee/EEAction.h, lle/GETransformation.h, <vector>.
#ifndef EE_EESEAMOVEUNIT_H
#define EE_EESEAMOVEUNIT_H

#include "ee/EEUnitHandle.h"

class EEUnit;
class EEComplexUnit;

// one moved unit (sizeof 0x54: _Buy's 0x54 stride; the implicit copy ctor 0x1a817c and operator= 0x1a7946)
struct EESEAMoveUnitEntry {
    GETransformation mTransformation;       // +0x00 where it is put down
    EEUnitHandle<EEUnit> mUnit;             // +0x40 (referenced; a handle since R5a.4: an undo record outlives ticks)
    EEUnitHandle<EEComplexUnit> mContainer; // +0x44 the transport/building it is put into (referenced; 0 for none; a handle)
    long mTileX;                            // +0x48
    long mTileY;                            // +0x4c
    float mYaw;                             // +0x50
};

class EESEAMoveUnit : public EEAction {
public:
    EESEAMoveUnit();                        // 0x1a75e0 (the first function of EESEAMoveUnit.cpp)
    virtual ~EESEAMoveUnit() {}             // +0x00 (??_G 0x1a7614: its own vptr first)
    virtual void Virtual4();                // +0x04 0x1a7646
    virtual void Virtual8() {}              // +0x08 (the shared empty slot 0x1720c6)
    void PickUp(unsigned long inUnitID);    // 0x1a797a (picks a unit up: it and its +0x1d0 chain)
    void PutDown();                         // 0x1a79fd (puts them down where the ghost units are)
    void Cancel();                          // 0x1a7bee (cancels: deletes the picked-up unit)

    std::vector<EESEAMoveUnitEntry> mEntries;   // +0x0c
    bool mIsMoving;                         // +0x1c a unit is being moved
};

#endif
