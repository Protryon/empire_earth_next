// EEUnitGoal.h (RECONSTRUCTION; file name inferred). What a unit is doing (RTTI EEUnitGoal; vtable 0x43cc64)
// Layout proven: EEUGAttackGround sizeof 0x2c = its new-site size, ctor 0x23c589 exact in EEUnitGoal.cpp; EEUGGather
// sizeof 0x44 = its new-site size, ctor 0x23c63f exact in EEUnitGoal.cpp; EEUGCastCalamity sizeof 0x34 = its new-site
// size, ctor 0x2209c9 exact in EEUGCastCalamity.cpp; EEUGAttackUnit sizeof 0x44 = its new-site size, ctor 0x23c5b2
// exact in EEUnitGoal.cpp; EEUGHealUnit sizeof 0x40 = its new-site size, ctor 0x2252d0 exact in EEUGHealUnit.cpp.
// Member names are provisional.
// and the goals the calamities hand out. Only what the decompiled units use; provisional names. The goal classes
// have one header each (ee/EEUGAttackGround.h, EEUGPatrol.h, EEUGGather.h, EEUGCastCalamity.h, EEUGAttackUnit.h,
// EEUGLocation.h, EEUGHealUnit.h, EEUGExplore.h, ...).
#ifndef EE_EEUNITGOAL_H
#define EE_EEUNITGOAL_H

#include <stdio.h>                      // FILE (the out-of-synch dumps)

class EEComplexUnit;
class EEUnit;
class EEPPatrolGroup;
class FSFile;
class GE3DPoint;
class UWideString;

class EEUnitGoal {
public:
    // 0x23c375 (out of line): +0x04 = true, +0x05 = inKind, +0x08 = 5000, +0x0c = 0, +0x10 = gPhysics's +0xd0,
    // +0x14 = ++(EEUnitGoal.cpp's goal counter), +0x18 = 0, +0x1c = inArg, +0x1d = false (provisional names)
    EEUnitGoal(unsigned char inKind, bool inArg);
    // 0x23c3e6: a default goal of kind inKind (EEGoalQueueContainer's Load 0x20588b then calls its Load), 0 for an
    // unknown kind; it hands the counter value back (provisional name)
    static EEUnitGoal* CreateGoal(unsigned char inKind);
    // Slots by offset (vtable 0x43cc64; provisional names; argument types from EEUnitGoal.cpp's and
    // EEUGAttackGround.cpp's bodies). The inline bodies are emitted with the first vtable that needs them:
    // EECCMeteorStorm.cpp's
    // EEUGAttackGround (0xb2a89..0xb2aa7). +0x10/+0x28/+0x6c/+0x70 are _purecall in EEUnitGoal's own vtable. The
    // trivial ones are ICF-folded (EEUnitGoal.cpp defines them).
    // inline: its ??_G 0x2276c4 stores the vptr itself (first emitted by EEUGLocation.cpp; EEUGAttackGround's folds in)
    virtual ~EEUnitGoal() {}                // +0x00
    virtual void AddCreatedUnit(EEComplexUnit* inUnit);  // +0x04 (empty; the load goals' container takes the unit)
    // +0x08 0x23c3c2: Virtual28's location into outLocation, then tells inUnit's active group
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);
    // inline: the copies (Virtual10) pass it through a byte temporary, the other bools straight (EEUGAttackGround
    // 0x21cf93, EEUGAttackUnit 0x21d67e, EEUGLocation 0x2277ef, EEUGPatrol 0x227f73) (provisional name)
    bool GetB1C() const { return this->mB1C; }
    virtual bool IsActive(unsigned long* outValue) { return this->mIsActive; }  // +0x0c (inline 0xb2a89; EEUGGather stores 5000 through it)
    virtual EEUnitGoal* Clone() = 0;        // +0x10 a copy (EEMScrambleAircraft queues its result)
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 (empty; the goals fprintf their state) (provisional name)
    virtual long GetOutOfSynchChecksum();   // +0x18 (0) (provisional name)
    virtual void GetDebugLine(UWideString& ioText);  // +0x1c (empty; the goals append a description)
    virtual void GetDebugText(UWideString& ioText);  // +0x20 (empty)
    virtual bool IsLoadGoal();              // +0x24 (false)
    virtual void GetGoalLocation(GE3DPoint& outLocation) = 0;  // +0x28 (the goal's location)
    virtual long GetMaxPathAttempts();      // +0x2c 0x23c6cd (3)
    virtual long GetStartAction();          // +0x30 (0: the action Virtual54 starts)
    virtual long GetStartTime() { return this->mStartTime; }  // +0x34 (inline 0xb2a8f)
    virtual unsigned long GetCreationTime();  // +0x38 (mL10)
    virtual void SetOwner(EEComplexUnit* inUnit);    // +0x3c (empty; the cast-calamity goal's new source)
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 (empty)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x23c6d1
    virtual void PathTaken(EEComplexUnit* inUnit);   // +0x48 (empty; EEUGGather reads the unit)
    virtual void Abandon(EEComplexUnit* inUnit);     // +0x4c (empty; the load goals compare it with their container)
    virtual long RepairPointers(unsigned long, unsigned long);  // +0x50 (0)
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 0x23c79e: inUnit starts Virtual30's action
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x23c7bd
    virtual void SetPathAttempts(long inValue) { this->mPathAttempts = inValue; }  // +0x5c (inline 0xb2a93)
    virtual void SetStartTime(long inValue) { this->mStartTime = inValue; }  // +0x60 (inline 0xb2a9d)
    // +0x64 0x23c82c: whether the goal handles event inEvent for inUnit (the base: 0x2336 only)
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);
    virtual void RemovedFromQueue(EEComplexUnit* inUnit);  // +0x68 (empty; the goal queue passes its unit when it drops a goal)
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*) = 0;  // +0x6c
    virtual bool Suits(EEUnit* inUnit) = 0;       // +0x70
    virtual void Work(EEComplexUnit* inUnit);        // +0x74 (empty; the heal goal heals every second)

    bool mIsActive;                         // +0x04
    unsigned char GetKind() const { return this->mKind; }
    unsigned char mKind;                    // +0x05
    unsigned long mAction;                  // +0x08 (FSFile Read/Write(unsigned long))
    unsigned long mStartTime;               // +0x0c (saved from minor version 0x9d)
    unsigned long mCreationTime;            // +0x10 (saved from 0xa4)
    unsigned long mID;                      // +0x14 the goal's number (saved from 0x64)
    long mPathAttempts;                     // +0x18
    bool mB1C;                              // +0x1c (saved from 0xf)
    bool mB1D;                              // +0x1d (saved from 0x87)

    // (unguarded: a class static only renumbers an includer's `$E` symbols, never its bytes)
    static const UString sData;             // "EEUnitGoal-Data" (EEUnitGoal.cpp's init chain 0x23c332) (provisional name)
};

// Exploring: ee/EEUGExplore.h (it embeds a U2DSparseArrayPointContainer, so it needs lle/U2DSparseArray.h, which
// a game unit includes only when it constructs that header's keys, so it is kept out of this header).
class EEUGExplore;

// EEUnitGoal::mKind: EEUnitGoal::CreateGoal 0x23c3e6 creates the goal class of each kind.
enum EEUnitGoalKind {
    kGoalLocation = 1,       // EEUnitGoal::CreateGoal 0x23c3e6 case 1: new EEUGLocation (RTTI)
    kGoalAttackUnit = 2,     // EEUnitGoal::CreateGoal 0x23c3e6 case 2: new EEUGAttackUnit (RTTI)
    kGoalAttackGround = 3,   // EEUnitGoal::CreateGoal 0x23c3e6 case 3: new EEUGAttackGround (RTTI)
    kGoalBuildRepair = 4,    // EEUnitGoal::CreateGoal 0x23c3e6 case 4: new EEUGBuildRepair (RTTI)
    kGoalGather = 6,         // EEUnitGoal::CreateGoal 0x23c3e6 case 6: new EEUGGather (RTTI)
    kGoalLoadTransport = 7,  // EEUnitGoal::CreateGoal 0x23c3e6 case 7: new EEUGLoadTransport (RTTI)
    kGoalUnloadTransport = 8,  // EEUnitGoal::CreateGoal 0x23c3e6 case 8: new EEUGUnloadTransport (RTTI)
    kGoalPatrol = 9,         // EEUnitGoal::CreateGoal 0x23c3e6 case 9: new EEUGPatrol (RTTI)
    kGoalExplore = 10,       // EEUnitGoal::CreateGoal 0x23c3e6 case 10: new EEUGExplore (RTTI)
    kGoalCastCalamity = 11,  // EEUnitGoal::CreateGoal 0x23c3e6 case 11: new EEUGCastCalamity (RTTI)
    kGoalLoadGarrison = 12,  // EEUnitGoal::CreateGoal 0x23c3e6 case 12: new EEUGLoadGarrison (RTTI)
    kGoalHealUnit = 15,      // EEUnitGoal::CreateGoal 0x23c3e6 case 15: new EEUGHealUnit (RTTI)
};

#endif
