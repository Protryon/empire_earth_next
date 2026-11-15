// C:\EEX\Empire Earth\AI Thread\EECPResourceCenter.h (RECONSTRUCTION; file name inferred).
// One of a computer player's resource centers (EEComputerPlayer +0xb80; no RTTI: not polymorphic; ctor 0x8bc3c; class
// static key "EECPResourceCenter-Data", guarded chain 0x8bbbf; EECPResourceCenter.cpp 0x8bbbf..0x8e702). It owns a
// referenced list of units (its buildings and workers) and a main unit. Names are provisional.
// Requires lle/LowLevelEngine.h, lle/FSFile.h, <vector>, ee/EETileCoordinate.h.
#ifndef EE_EECPRESOURCECENTER_H
#define EE_EECPRESOURCECENTER_H

#include <vector>
#include "ee/EETileCoordinate.h"
#include "ee/EEUnitHandle.h"

class EEComputerPlayer;
class EEUnit;
class EEComplexUnit;
class FSFile;

class EECPResourceCenter {
public:
    EECPResourceCenter(EEComputerPlayer* inPlayer, EEUnit* inUnit);
    ~EECPResourceCenter();                  // 0x8bc71 (dereferences every unit)
    void AddUnit(EEUnit* inUnit);           // 0x8bcae (references it, push_back)
    bool Contains(EEUnit* inUnit);
    void AdoptDropSite(EEComplexUnit* inUnit);  // 0x8bd2b (ret 4): adopts a building as the main unit when it lies within the radius of a unit
    bool FindBuildSite(long inUnitClass, EETileCoordinate& outTile);
    long GetMinEffectRadius(long inUnitClass);  // 0x8cbec (ret 4): the smallest area effect template radius of the class's upgrade
    float GetMoraleFill();
    EEUnit* FindResourceUnit(unsigned long inUnitType);  // 0x8d065 (ret 4): the first unit of that type with +0x40 > 0
    bool IsInAreaEffect(long inX, long inY, long inUnitClass, long inKind);  // 0x8d0a2 (ret 0x10): area effect over the class's footprint
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);
    bool IsWithinRadius(EEUnit* inUnit);    // 0x8d2bc (ret 4): the unit lies within the radius of the centre
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void SetUnit(EEComplexUnit* inUnit);
    void SetActive(bool inFlag, long inArg2);
    void ReleaseUnit(EEComplexUnit* inUnit);
    void GetCenter(EETileCoordinate& outCenter);

    static const UString sData;             // "EECPResourceCenter-Data" (guarded chain 0x8bbbf)

    EEComputerPlayer* mPlayer;              // +0x00
    EEUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EEUnitHandle<EEUnit> > mUnits;   // +0x04 (referenced; Load reads the ids, Restore turns them into referenced units; handles since R5a.4, the vector's order kept)
    EEComplexUnit* GetUnit() const { return this->mUnit; }
    EEUnitHandle<EEComplexUnit> mUnit;      // +0x14 the centre's main unit (referenced; a handle since R5a.4)
    bool mIsActive;                         // +0x18 (saved from version 0x8d)
};

#endif
