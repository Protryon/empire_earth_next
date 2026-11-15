// C:\EEX\Empire Earth\AI Thread\EECPILocation.h (RECONSTRUCTION; file name inferred). A location in the computer
// player's intelligence (RTTI EECPILocation, vtable 0x43a6b4: the destructor only; sizeof 0x18; EECPILocation.cpp
// 0x80816..0x80c69, key "EECPILocation-Data"): a map tile plus what is there (an area serial and its kind). Both ctors
// call the out-of-line Clear (0x46527: ICF-folded into a twin in EECPASGLand's range); the copy ctor 0x8088d copies
// the fields in the body; ??_G 0x41387 (emitted at its first user, EECPAGOrder.cpp). Member types from Load/Save
// (FSFile Read/Write slots). Names are provisional.
// Requires lle/FSFile.h.
#ifndef EE_EECPILOCATION_H
#define EE_EECPILOCATION_H

#include "ee/EETileLocation.h"

class EEAIPathSubSector;
class EEUnit;

class EECPILocation {
public:
    EECPILocation(const EETileLocation& inTile);   // 0x80848
    EECPILocation();                        // 0x80873
    EECPILocation(const EECPILocation& inLocation);   // 0x8088d
    // inline: its first user (EECPAGOrder's dtor 0x41369) emits ??_G 0x41387; EECPAGTask's ??_G expands it
    virtual ~EECPILocation() {}
    EECPILocation& operator=(const EECPILocation& inLocation);   // 0x80c42
    void Clear();                           // 0x46527 (ICF: the kept copy is in EECPASGLand's range)
    EEAIPathSubSector* GetPathSubSector() const;   // 0x4e0b1 (ICF: the kept copy is in EECPASGSea's range)
    bool IsValid();                         // 0x808c2: the tile is on the map (else a random point of the area)
    bool IsAdjacent(const EECPILocation& inLocation) const;   // 0x80926: both tile deltas within 1
    bool IsConnected(const EECPILocation& inLocation) const;  // 0x80951: same or neighbouring path sub-sectors
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // 0x80984
    void MoveIntoArea();                    // 0x809c7: a random point of the area, if it still exists
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);                // 0x80a01 (returns 0)
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x80a0b
    void Update();                          // 0x80a57: re-finds mSerial for mType at mTile (Restore calls it)
    const EETileLocation& GetTile() const { return this->mTile; }  // inline (Update keeps &mTile in a register) (provisional name)
    void SetUnit(EEUnit* inUnit);           // 0x80be4: Clear, then the unit's tile (+0x1c/+0x20)
    void ResetToArea();                     // 0x80c04: tile (0, 0), then a random point of the area
    bool IsOnMap();                         // 0x488b6 (ICF; EECPDefenseCommander after ResetToArea) (provisional name)

    EETileLocation mTile;                   // +0x04 (U2DPoint<long>::Load/Save 0x12e04/0x12e38)
    unsigned long mSerial;                  // +0x0c an EEAIArea serial (EEAIContinentManager::GetArea)
    long mType;                             // +0x10 the area kind (0..10; Update's switch)
    unsigned long mPlayer;                  // +0x14 a gWorld.mPlayers index
};

// port: set while a game saved from version 0.263 on is loaded (EEWorld::StartGame) until its first tick: Update keeps
// the saved area (EECPILocation.cpp)
extern bool gLoadKeepsLocations;

#endif
