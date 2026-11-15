// C:\EEX\Empire Earth\AI Thread\EECPASGAir.h (RECONSTRUCTION; file name inferred). RTTI
// EECPASGAir > EECPAttackSubGroup > UReferenceCounter; vtable 0x43a6d8; sizeof 0x1e8; ctor 0x4192c (inGroup, inArg2:
// EECPAttackSubGroup's, then the members), ??_G 0x4194f, dtor 0x4196b. An air attack sub group (no member of its
// own). Adds slots from +0x1dc. Slot signatures: the introducing class's (ee/EECPAttackSubGroup.h, checked over
// every body). Names are provisional.
#ifndef EE_EECPASGAIR_H
#define EE_EECPASGAIR_H

#include "ee/EECPAttackSubGroup.h"

class EECPASGAir : public EECPAttackSubGroup {
public:
    EECPASGAir(EECPAttackGroup* inGroup, long inKind);   // 0x4192c
    virtual ~EECPASGAir();                  // +0x00 (0x4194f)
    virtual void RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent, long inTownID); // +0x08 0x41976
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x41a2e
    virtual void MoveToNearestTown();       // +0x18 0xe123d (`ret`)
    // +0x1c 0x41a66
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights);   // +0x54 0x41d18
    virtual void Disband();                 // +0x68 0x41d2f
    virtual float GetGuardChokepointPercentage();  // +0xa8 0x41ed0
    virtual float GetGuardContinentPercentage();  // +0xb0 0x41948 (`fld dword ptr [0x83948c]; ret`)
    virtual float GetGuardTownPercentage();  // +0xb4 0x41ee4
    virtual bool CanReach(const EECPILocation* inLocation);    // +0xc8 0x42000
    virtual bool GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction);  // +0xe4 0x42220
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x4249f
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x42578
    virtual void SetDestination(const EECPILocation& inLocation);  // +0xfc 0x42759
    // +0x100 0x42a47
    virtual void SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag);
    virtual bool IsAwayFromTowns();         // +0x124 0x27c0b4 (`xor al, al; ret`)
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x4321b
    virtual bool RegroupHere();             // +0x144 0x41ce6
    virtual bool TakeRequestedUnits();      // +0x154 0x41dbd
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies);  // +0x158 0x41f4a
    virtual void RequestRemoval();          // +0x160 0x423d4
    virtual void Update(long inArg1);       // +0x168 0x5019d
    virtual bool DecideSplit();             // +0x16c 0x27c0b4 (`xor al, al; ret`)
    virtual void MoveToDestination();       // +0x170 0x44a2c
    virtual void RequestPath(void* inFrom, void* inTo);   // +0x178 0x42cac
    virtual void AssignPrejudices();        // +0x198 0x42e77
    virtual bool IsTerritoryTaskValid(long inArg1);  // +0x1c4 0x4e240
    virtual bool IsLocationTaskValid(long inArg1);  // +0x1cc 0x4327d
    virtual long CountUnitsInRange(const EECPILocation& inLocation);  // +0x1dc 0x42142 (how many units can reach it)
    virtual bool IsInRange(const EECPILocation& inLocation);    // +0x1e0 0x42564 (CountSubmarines != 0)
    virtual void FilterReachableLocations(const std::vector<EECPILocation>& inLocations, std::vector<EECPILocation>& outLocations); // +0x1e4 0x42d81
    virtual void Launch(unsigned long inTime);       // +0x1e8 0x41ef8 (each unit's mHotKey)
    virtual void GoIdle();                  // +0x1ec 0x42273
    virtual bool MatchTargetsToUnits(bool inArg1, bool inArg2, bool inArg3); // +0x1f0 0x4259e
};

#endif
