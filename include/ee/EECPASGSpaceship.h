// C:\EEX\Empire Earth\AI Thread\EECPASGSpaceship.h (RECONSTRUCTION; file name inferred).
// RTTI EECPASGSpaceship > EECPAttackSubGroup > UReferenceCounter; vtable 0x43b3f4; sizeof 0x1e8; ctor 0x4f0d1
// (inGroup, inArg2: EECPAttackSubGroup's, then the members), ??_G 0x4f0ed, dtor 0x4f109. A spaceship attack sub
// group (no member of its own). Adds slots from +0x1dc. Slot signatures: the introducing class's
// (ee/EECPAttackSubGroup.h, checked over every body). Names are provisional.
#ifndef EE_EECPASGSPACESHIP_H
#define EE_EECPASGSPACESHIP_H

#include "ee/EECPAttackSubGroup.h"

class EECPASGSpaceship : public EECPAttackSubGroup {
public:
    EECPASGSpaceship(EECPAttackGroup* inGroup, long inKind);   // 0x4f0d1
    virtual ~EECPASGSpaceship();            // +0x00 (0x4f0ed)
    virtual void RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent, long inTownID); // +0x08 0x41976
    virtual void MoveToNearestTown();       // +0x18 0xe123d (`ret`)
    virtual bool FindPosition(EETileLocation& outTile, bool inArg2, bool inForce); // +0x40 0x4f114
    virtual long GetNumTargets();           // +0x64 0x4f128
    virtual void PickSplitUnits(std::vector<EEComplexUnit*>& outUnits);  // +0x94 0x4f236
    virtual bool CanReach(const EECPILocation* inLocation);    // +0xc8 0x4f5b7
    virtual bool IsAt(const EECPILocation& inLocation);         // +0xcc 0x4dff2
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x4f603
    virtual bool IsTaskActive(const EECPAGTask& inTask); // +0xf0 0x4fabf
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x4e125
    virtual void SetDestination(const EECPILocation& inLocation);  // +0xfc 0x4e139 (`push dword ptr [esp + 4]; call 0x45f14a; ret`)
    virtual bool IsAwayFromTowns();         // +0x124 0x27c0b4 (`xor al, al; ret`)
    virtual bool TestCondition(long inState, long inTime); // +0x12c 0x4e203
    virtual void PlaceFormation(const EECPILocation& inLocation);  // +0x150 0x4dd9f
    virtual bool TakeRequestedUnits();      // +0x154 0x4f153
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies);  // +0x158 0x4f444
    virtual void Update(long inArg1);       // +0x168 0x4f70e
    virtual bool DecideSplit();             // +0x16c 0x27c0b4 (`xor al, al; ret`)
    virtual void RequestPath(void* inFrom, void* inTo);   // +0x178 0x4fafa
    virtual bool IsTerritoryTaskValid(long inArg1);  // +0x1c4 0x4e240
    virtual bool IsLocationTaskValid(long inArg1);  // +0x1cc 0x4e26a
    virtual bool IsUnitTaskValid(long inTime);  // +0x1d4 0x4e42f
    virtual void IsAtLocation(const EECPILocation& inLocation);  // +0x1dc 0x4dfe6 (`push dword ptr [esp + 4]; call 0x45e430; ret`)
};

#endif
