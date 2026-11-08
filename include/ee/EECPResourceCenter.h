// C:\EEX\Empire Earth\AI Thread\EECPResourceCenter.h (RECONSTRUCTION; file name inferred).
// One of a computer player's resource centers (EEComputerPlayer +0xb80; no RTTI: not polymorphic; ctor 0x8bc3c; class
// static key "EECPResourceCenter-Data", guarded chain 0x8bbbf; EECPResourceCenter.cpp 0x8bbbf..0x8e702). It owns a
// referenced list of units (its buildings and workers) and a main unit. Names are provisional.
// Requires lle/LowLevelEngine.h, lle/FSFile.h, <vector>, ee/EETileCoordinate.h.
#ifndef EE_EECPRESOURCECENTER_H
#define EE_EECPRESOURCECENTER_H

#include <vector>
#include "ee/EETileCoordinate.h"

class EEComputerPlayer;
class EEUnit;
class EEComplexUnit;
class FSFile;

class EECPResourceCenter {
public:
    EECPResourceCenter(EEComputerPlayer* inPlayer, EEUnit* inUnit);   // 0x8bc3c (ret 8)
    ~EECPResourceCenter();                  // 0x8bc71 (dereferences every unit)
    void AddUnit(EEUnit* inUnit);           // 0x8bcae (references it, push_back)
    bool Contains(EEUnit* inUnit);          // 0x8bcc5
    void AdoptDropSite(EEComplexUnit* inUnit);  // 0x8bd2b (ret 4): adopts a building as the main unit when it lies within the radius of a unit
    bool FindBuildSite(long inUnitClass, EETileCoordinate& outTile);  // 0x8bdfa (ret 8)
    long GetMinEffectRadius(long inUnitClass);  // 0x8cbec (ret 4): the smallest area effect template radius of the class's upgrade
    float GetMoraleFill();                  // 0x8cc5f
    EEUnit* FindResourceUnit(unsigned long inUnitType);  // 0x8d065 (ret 4): the first unit of that type with +0x40 > 0
    bool IsInAreaEffect(long inX, long inY, long inUnitClass, long inKind);  // 0x8d0a2 (ret 0x10): area effect over the class's footprint
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x8d126
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x8d1bd
    bool IsWithinRadius(EEUnit* inUnit);    // 0x8d2bc (ret 4): the unit lies within the radius of the centre
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x8d306
    void SetUnit(EEComplexUnit* inUnit);    // 0x8d3c3 (ret 4)
    void SetActive(bool inFlag, long inArg2); // 0x8d3e4 (ret 8)
    void ReleaseUnit(EEComplexUnit* inUnit);   // 0x8d408 (ret 4)
    void GetCenter(EETileCoordinate& outCenter);  // 0x8cdbf (ret 4)

    static const UString sData;             // "EECPResourceCenter-Data" (guarded chain 0x8bbbf)

    EEComputerPlayer* mPlayer;              // +0x00
    EEUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEUnit*> mUnits;            // +0x04 (Load reads the ids, Restore turns them into referenced units)
    EEComplexUnit* GetUnit() const { return this->mUnit; }
    EEComplexUnit* mUnit;                   // +0x14 the centre's main unit (referenced)
    bool mIsActive;                         // +0x18 (saved from version 0x8d)
};

#endif
