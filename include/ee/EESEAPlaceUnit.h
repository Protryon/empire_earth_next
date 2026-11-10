// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPlaceUnit.h (RECONSTRUCTION; file name inferred). The
// editor's placement actions, held by the unit tab (EESEUnit +0x11c/+0x120/+0x124): RTTI
// EESEAPlaceUnit (vtable 0x444f04), EESEAPlaceGate (0x444f18), EESEAPlaceWall (0x444f2c), each > EEAction, sizeof
// 0x1c: a vector at +0x0c (EESEAPlaceUnit: 0x50-byte entries, _Buy 0x19cb8b; EESEAPlaceGate 0x14, EESEAPlaceWall 0x18:
// the entries decoded from EESEAPlace*.cpp). The inline ctors and ??_Gs are emitted by
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

// one placed gate (0x14 bytes): ctor COMDAT 0x1a91b8 zeroes all five, operator= 0x1a95d6
struct EESEAPlaceGateEntry {
    EESEAPlaceGateEntry();                  // 0x1a91b8 (EESEAPlaceGate.cpp, after its first user Virtual4)
    // empty user-declared dtor: vector<EESEAPlaceGateEntry>::_Tidy keeps the frame (push ebp ... leave) that the
    // ICF-shared _Tidy 0xa9c2c has; without it the instantiation is 37 bytes, frameless (probe)
    ~EESEAPlaceGateEntry() {}
    // user-declared (field moves, COMDAT 0x1a95d6 called out of line; ICF-shared with EEFormationSlot's)
    EESEAPlaceGateEntry& operator=(const EESEAPlaceGateEntry& inEntry)
    {
        this->mTile = inEntry.mTile;
        this->mPlayer = inEntry.mPlayer;
        this->mTechnology = inEntry.mTechnology;
        this->mDirection = inEntry.mDirection;
        return *this;
    }
    EETileCoordinate mTile;                 // +0x00
    long mPlayer;                           // +0x08
    long mTechnology;                       // +0x0c
    long mDirection;                        // +0x10
};

// one placed wall line (0x18 bytes): ctor COMDAT 0x1aa1a9 (two GE2DPoint zeroes, then +0x10/+0x14 = 0)
struct EESEAPlaceWallEntry {
    EESEAPlaceWallEntry();                  // 0x1aa1a9 (EESEAPlaceWall.cpp, after its first user Virtual4)
    GE2DPoint mStart;                       // +0x00
    GE2DPoint mEnd;                         // +0x08
    long mPlayer;                           // +0x10
    long mTechnology;                       // +0x14
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

class EESEAPlaceGate : public EEAction {
public:
    EESEAPlaceGate() {}
    virtual ~EESEAPlaceGate() {}            // +0x00 (??_G 0x19ba46)
    virtual void Virtual4();                // +0x04 0x1a9112
    virtual void Virtual8() {}              // +0x08
    // +0x0c 0x1a91cb: the tile, the player, the technology, the direction (al: placed)
    virtual bool Place(const EETileCoordinate& inTile, long inPlayer, long inTechnology, long inDirection);

    std::vector<EESEAPlaceGateEntry> mTiles;    // +0x0c
};

class EESEAPlaceWall : public EEAction {
public:
    EESEAPlaceWall() {}
    virtual ~EESEAPlaceWall() {}            // +0x00 (??_G 0x19baa8)
    virtual void Virtual4();                // +0x04 0x1aa0d1
    virtual void Virtual8() {}              // +0x08
    // +0x0c 0x1aa1c7: the line's ends, the player, the technology (returns true)
    virtual bool Place(const GE2DPoint& inStart, const GE2DPoint& inEnd, long inPlayer, long inTechnology);

    std::vector<EESEAPlaceWallEntry> mTiles;    // +0x0c
};

#endif
