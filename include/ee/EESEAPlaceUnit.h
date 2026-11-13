// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPlaceUnit.h (RECONSTRUCTION; file name inferred). The
// editor's placement actions, held by the unit tab (EESEUnit +0x11c/+0x120/+0x124): RTTI
// EESEAPlaceUnit (vtable 0x444f04) > EEAction, sizeof 0x1c: a vector at +0x0c (0x50-byte entries, _Buy 0x19cb8b).
// The gate's and the wall's (EESEAPlaceGate 0x444f18, EESEAPlaceWall 0x444f2c) are whole in Rust (ee/EESEARust.h). The inline ctors and ??_Gs are emitted by
// EESEUnit.cpp (0x19b9b4..0x19bada); Virtual4/Virtual0C live in EESEAPlace*.cpp (0x1a9112.., 0x1a9650.., 0x1aa0d1..).
// Names are provisional. Requires ee/EEAction.h, <vector>, ee/EETileCoordinate.h, lle/GETransformation.h,
// lle/GE2DPoint.h.
#ifndef EE_EESEAPLACEUNIT_H
#define EE_EESEAPLACEUNIT_H

#include "ee/EEUnitHandle.h"

class EEComplexUnit;

// one placed (or undone) unit (0x50 bytes): ctor COMDAT 0x1a984e (+0 = 0, GETransformation(), +0x44/+0x48 = 0,
// +0x4c = 7), implicit operator= 0x1a9870
struct EESEAPlacedUnit {
    EESEAPlacedUnit();                      // 0x1a984e (EESEAPlaceUnit.cpp, after its first user Virtual4)
    EEUnitHandle<EEComplexUnit> mContainer; // +0x00 the transport/building it was put into (referenced; a handle since R5a.4)
    GETransformation mTransformation;       // +0x04
    long mPlayer;                           // +0x44
    long mUnitType;                         // +0x48
    long mAnimation;                        // +0x4c (= 7)
};

class EESEAPlaceUnit : public EEAction {
public:
    EESEAPlaceUnit() {}
    virtual ~EESEAPlaceUnit() {}            // +0x00 (??_G 0x19b9e4: the vptr store first)
    virtual void Virtual4();                // +0x04 0x1a9650
    virtual void Virtual8() {}              // +0x08 (the shared empty 0x1720c6)
    virtual bool Place(long inPlayer);      // +0x0c 0x1a98a2 (ret 4, al: placed)

    std::vector<EESEAPlacedUnit> mUnits;    // +0x0c
};

#endif
