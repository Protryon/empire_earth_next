// C:\EEX\Empire Earth\AI Thread\EECPAttackSubGroupUnit.h (RECONSTRUCTION; file name inferred). One unit of a
// computer player's attack sub group (RTTI EECPAttackSubGroupUnit > UReferenceCounter; vtable 0x43bf4c, 31 slots;
// sizeof 0x84; EECPAttackSubGroupUnit.cpp 0x62f8a..0x66af0): ctors
// 0x6319f (unit, sub group) and 0x632c1 (sub group; the load path), dtor 0x63360. The EECPASGU* classes
// (0x5d47..0xe9a1) derive from it and set mType (+0x54) in their ctor bodies. Slot signatures from every
// implementation (`ret N`, argument reads, the value left at `ret`); slot and
// member names are provisional unless a comment names the evidence. Requires lle/LowLevelEngine.h
// (UReferenceCounter, UString, FSFile, <vector>), ee/EECPILocation.h.
#ifndef EE_EECPATTACKSUBGROUPUNIT_H
#define EE_EECPATTACKSUBGROUPUNIT_H

#include <set>

class EEComplexUnit;
class EECPAttackSubGroup;
class EECPASTargetUnit;
class EEUnitClass;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

// A unit kind the sub group unit prefers or avoids (RTTI EECPAttackSubGroupUnitPrejudice, vtable 0x43a8d4: the
// destructor only; 0x10-byte elements of EECPAttackSubGroupUnit::mPrejudices). ??_G 0x431fe (no member destructor,
// own vptr stored: a user-declared empty inline dtor) is emitted by its first user, EECPASGAir.cpp.
class EECPAttackSubGroupUnitPrejudice {
public:
    EECPAttackSubGroupUnitPrejudice() {}
    // user-declared, members assigned in the body: vector::insert 0x437b4 stores the vptr between the member copies
    // (the same body-copy style as EECPIUnitTypeCount)
    EECPAttackSubGroupUnitPrejudice(const EECPAttackSubGroupUnitPrejudice& inOther)
    {
        this->mUnitType = inOther.mUnitType;
        this->mUnitLine = inOther.mUnitLine;
        this->mUnitFamily = inOther.mUnitFamily;
    }
    virtual ~EECPAttackSubGroupUnitPrejudice() {}
    // 0x63166 (EECPAttackSubGroupUnit.cpp's helpers): mL08 set -> inClass->mAIKind == mL08; else mL0C set ->
    // inClass->mFamily == mL0C; else inClass->mUnitType == mUnitType
    bool Matches(EEUnitClass* inClass);

    long mUnitType;                         // +0x04 (EEUnitClass +0x1bc)
    long mUnitLine;                         // +0x08 (EEUnitClass +0x1d0)
    long mUnitFamily;                       // +0x0c (EEUnitClass +0x1c8)
};

class EECPAttackSubGroupUnit : public UReferenceCounter {
public:
    // 0x6319f: references inUnit (+0x44) and inSubGroup (+0x4c, may be 0); mType = 0; ends with Virtual20()
    EECPAttackSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);
    // 0x632c1: the load path (references inSubGroup)
    EECPAttackSubGroupUnit(EECPAttackSubGroup* inSubGroup);
    virtual ~EECPAttackSubGroupUnit();      // +0x00 (dtor 0x63360)
    virtual void SpreadContagion();         // +0x04 0x6346c (Virtual0C(true), then 0x65eb6 and Virtual3C(it, false, false))
    virtual float ScoreTarget(EECPASTargetUnit* inTarget);  // +0x08 0x63494 (a score of inTarget)
    virtual EEComplexUnit* PickContagionTarget(bool inScore);  // +0x0c 0x63b33 (picks a unit from the sub group's targets)
    // +0x10 0x63d50 (ret 8): outTile is cleared to (0, 0) first; returns the chosen target or 0
    virtual EECPASTargetUnit* ChooseTarget(bool inTakeFirst, EETileCoordinate& outTile);
    virtual bool Retarget();                // +0x14 0x6449e (mL48 != 0 at the end)
    virtual long Virtual18();               // +0x18 0xfca98 (`xor eax, eax`, ICF-shared)
    virtual void Flee();                    // +0x1c 0x645ba
    virtual void RandomizePersonality();    // +0x20 0x64f98 (the ctor calls it)
    virtual long GetTargetWeight(EECPASTargetUnit* inTarget);  // +0x24 0x64dcb (an __ftol'd amount)
    virtual void SetState(long inState, unsigned long inTime);    // +0x28 0x652f6 (enters state inState)
    virtual bool CanAttack(EECPASTargetUnit* inTarget);    // +0x2c 0x6577b
    virtual bool IsInDanger();              // +0x30 0x657e5
    // +0x34 0x659a0: walks mPrejudices from the back until one Matches(inArg->mUnit->mUnitClass): the argument is an
    // EECPASTargetUnit* (retyped together with EECPASGUAirFighter/AirBomber)
    virtual void ApplyPrejudices(EECPASTargetUnit* inTarget);
    // +0x38 0x659ee / +0x40 0x65c48 / +0x44 0x65d37: the persistence trio (mLocation's Load/Restore/Save first;
    // Restore resolves the saved unit ids of +0x44/+0x48/+0x50 and returns 0)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual void MoveTo(EEComplexUnit* inUnit, bool inArg2, bool inUseDestination);  // +0x3c 0x65b2d
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual void Virtual48();               // +0x48 0xe123d (`ret`, ICF-shared)
    virtual bool IsContagionTargetLost();   // +0x4c 0x65ee2
    virtual bool CheckCondition(long inState, unsigned long inTime);  // +0x50 0x65f65 (can enter inState)
    // +0x54 0x66209: the escorted unit (mEscort) is farther than inDistanceSquared (GE3DPoint::DistanceToSquared)
    virtual bool IsFarFromEscort(float inDistanceSquared);
    virtual bool IsTargetValid();           // +0x58 0x6629a
    virtual float Virtual5C(EECPASTargetUnit* inTarget);   // +0x5c 0x63b2a (returns 1.0f; argument unread)
    virtual float GetCounterFactor(EECPASTargetUnit* inTarget);  // +0x60 0x633ad (0, 1 or 0.5 by the two unit types)
    virtual float GetTargetDamage(EECPASTargetUnit* inTarget);  // +0x64 0x63408 (1 - min(1, hit points / maximum))
    virtual float GetImportanceRatio(EECPASTargetUnit* inTarget);  // +0x68 0x63431 (a ratio of the two classes' +0x244)
    virtual void Update(unsigned long inTime);      // +0x6c 0x6544c (ends: mNextUpdateTime = inTime + 1000)
    virtual bool IsExposedToCalamity();     // +0x70 0x65859
    virtual bool ShouldRetreat();           // +0x74 0x27c0b4 (`xor al, al`, ICF-shared)
    virtual void Retreat();                 // +0x78 0x66295 (`Virtual1C();` as a tail jump)
    // non-virtual helpers the kinds call (provisional names): 0x65c2f dereferences mL48 and clears it and
    // +0x78; 0x65eb6 calls it, then mL48 = inUnit (referenced)
    void ReleaseTarget();                   // 0x65c2f
    void SetTarget(EEComplexUnit* inUnit);  // 0x65eb6
    // 0x705f (`mov eax, [ecx+0x44]; ret`, called by the prophet analyzers; provisional name)
    EEComplexUnit* GetUnit() { return this->mUnit; }
    // (EECPAttackSubGroupUnit.cpp; provisional names)
    float GetToHit(EECPASTargetUnit* inTarget);   // 0x63aef (gToHitTable's chance against inTarget)
    bool IsDone();                          // 0x652e0 (mHasGroupFlee or mUnit dead; many sub group callers)
    void ReleaseSubGroup();                 // 0x65d0c
    void ReleaseUnit();                     // 0x65d22
    void SetUnit(EEComplexUnit* inUnit);    // 0x65ecc (references inUnit)
    bool TryCastCalamity();                 // 0x66066 (Virtual14 first: a prophet unit casts a calamity on a target)

    std::set<UString> mU08;                 // +0x08 (_Tree_val + _Init in the ctor; dtor 0x66347)
    std::vector<EECPAttackSubGroupUnitPrejudice> mPrejudices;   // +0x14 (_Buy(0) 0x88ba6; Virtual34 walks it)
    EECPILocation mLocation;                // +0x24 (ctor 0x80873; Load/Restore/Save first)
    long mL3C;                              // +0x3c (init list)
    long mL40;                              // +0x40 (init list)
    EEComplexUnit* mUnit;                   // +0x44 (referenced)
    EEComplexUnit* GetTarget() const { return this->mTarget; }
    EEComplexUnit* mTarget;                 // +0x48 (referenced; restored by id through dynamic_cast<EEComplexUnit*>)
    EECPAttackSubGroup* GetSubGroup() const { return this->mSubGroup; }
    EECPAttackSubGroup* mSubGroup;          // +0x4c (referenced; may be 0)
    // +0x50: another unit of the sub group (Restore: mSubGroup's 0x5e01b by unit; dereferenced in the dtor)
    EECPAttackSubGroupUnit* mEscort;
    long mType;                             // +0x54 the unit kind (0 base, 2 aircraft carrier, 5 sea, 9 hero, 0xb heli)
    long mState;                            // +0x58 (0, 700, 800, ...: slot +0x28 enters one)
    long mPostAttackMode;                   // +0x5c (Virtual28: 1 or 2)
    float mCourage;                         // +0x60 (= 0.5)
    float GetAggression() const { return this->mAggression; }
    float mAggression;                      // +0x64 (= 0.5)
    float mF68;                             // +0x68 (= 0.5)
    float mF6C;                             // +0x6c (= 0.5)
    unsigned long mNextUpdateTime;          // +0x70 (slot +0x6c: inTime + 1000)
    long mTargetPickResult;                 // +0x74
    unsigned long mNextRetargetTime;        // +0x78 (saved as unsigned long)
    unsigned long GetImportance() const { return this->mImportance; }
    unsigned long mImportance;              // +0x7c (the unit class's +0x244 times a factor; saved as unsigned long)
    bool GetIsImportant() const { return this->mIsImportant; }
    bool mIsImportant;                      // +0x80
    bool mNeedsOrders;                      // +0x81
    bool mIsOutOfFight;                     // +0x82
};

// Attack subgroup unit conditions (EECPAttackSubGroupUnit::CheckCondition 0x465f65 and the EECPASGU* overrides). Names
// are provisional (by behaviour).
enum EECPASGUCondition {
    kASGUConditionAlways = 0,                // CheckCondition 0x465f65 case 0: true (provisional name)
    kASGUConditionHasTarget = 1,             // a live mTarget (provisional name)
    kASGUConditionHasEscort = 2,             // a live escort unit (provisional name)
    kASGUConditionSubGroupHasTargets = 3,    // mSubGroup->GetNumTargets() > 0 (provisional name)
    kASGUConditionRetargetDue = 5,           // inTime >= mNextRetargetTime (provisional name)
    kASGUConditionInDanger = 6,              // IsInDanger (provisional name)
    kASGUConditionContagionTargetFound = 301,  // PickContagionTarget(false) != NULL (provisional name)
    kASGUConditionContagionTargetLost = 302,  // IsContagionTargetLost (provisional name)
    kASGUConditionOnMap = 700,               // the unit's tile x != -10000 (provisional name)
    kASGUConditionPostAttackRetarget = 800,  // mPostAttackMode == 2 (provisional name)
    kASGUConditionPostAttackFlee = 801,      // mPostAttackMode == 1 (provisional name)
    kASGUConditionNearPathEnd = 900,         // the unit has at most 2 waypoints left (provisional name)
    kASGUConditionCalamityFlags = 901,       // the unit's mCalamityFlags != 0 (provisional name)
    kASGUConditionExposedToCalamity = 902,   // IsExposedToCalamity (provisional name)
    kASGUConditionShouldRetreat = 904,       // ShouldRetreat (provisional name)
};

// Attack subgroup unit kinds (EECPAttackSubGroupUnit::mType): EECPCreateSubGroupUnitOfKind 0x45661e creates the class of the kind (names: those RTTI classes; 0 the base EECPAttackSubGroupUnit), each ctor stores its kind.
enum EECPASGUKind {
    kASGUKindDefault = 0,        // the base class EECPAttackSubGroupUnit (EECPCreateSubGroupUnitOfKind 0x45661e case 0 / default; its ctors store 0) (provisional name)
    kASGUKindAirBomber = 1,      // EECPCreateSubGroupUnitOfKind 0x45661e case 1: new EECPASGUAirBomber; its ctor stores 1 (RTTI)
    kASGUKindAircraftCarrier = 2,  // EECPCreateSubGroupUnitOfKind 0x45661e case 2: new EECPASGUAircraftCarrier; its ctor stores 2 (RTTI)
    kASGUKindAirFighter = 3,     // EECPCreateSubGroupUnitOfKind 0x45661e case 3: new EECPASGUAirFighter; its ctor stores 3 (RTTI)
    kASGUKindTransport = 4,      // EECPCreateSubGroupUnitOfKind 0x45661e case 4: new EECPASGUTransport; its ctor stores 4 (RTTI)
    kASGUKindSea = 5,            // EECPCreateSubGroupUnitOfKind 0x45661e case 5: new EECPASGUSea; its ctor stores 5 (RTTI)
    kASGUKindPriest = 6,         // EECPCreateSubGroupUnitOfKind 0x45661e case 6: new EECPASGUPriest; its ctor stores 6 (RTTI)
    kASGUKindMechPriest = 7,     // EECPCreateSubGroupUnitOfKind 0x45661e case 7: new EECPASGUMechPriest; its ctor stores 7 (RTTI)
    kASGUKindProphet = 8,        // EECPCreateSubGroupUnitOfKind 0x45661e case 8: new EECPASGUProphet; its ctor stores 8 (RTTI)
    kASGUKindHero = 9,           // EECPCreateSubGroupUnitOfKind 0x45661e case 9: new EECPASGUHero; its ctor stores 9 (RTTI)
    kASGUKindMechProphet = 10,   // EECPCreateSubGroupUnitOfKind 0x45661e case 10: new EECPASGUMechProphet; its ctor stores 10 (RTTI)
    kASGUKindHelicopter = 11,    // EECPCreateSubGroupUnitOfKind 0x45661e case 11: new EECPASGUHelicopter; its ctor stores 11 (RTTI)
    kASGUKindSpace = 12,         // EECPCreateSubGroupUnitOfKind 0x45661e case 12: new EECPASGUSpace; its ctor stores 12 (RTTI)
    kASGUKindSpaceCarrier = 13,  // EECPCreateSubGroupUnitOfKind 0x45661e case 13: new EECPASGUSpaceCarrier; its ctor stores 13 (RTTI)
};

// Attack subgroup unit states (EECPAttackSubGroupUnit::mState): SetState 0x4652f6 runs the entry action, Update
// 0x46544c switches on it. Names are provisional (by behaviour).
enum EECPASGUState {
    kASGUStateIdle = 0,             // SetState 0x4652f6: drops the escort and the target; Update 0x46544c -> 800 when the subgroup has targets (provisional name)
    kASGUStateEscort = 101,         // SetState 0x4652f6: MoveTo the escort unit (provisional name)
    kASGUStateFlee = 105,           // SetState 0x4652f6: Flee; re-entered while IsInDanger (condition 6) (provisional name)
    kASGUStateAttack = 300,         // SetState 0x4652f6: Retarget and the next retarget time; Update re-enters it when the retarget time is due (condition 5) (provisional name)
    kASGUStateSpreadContagion = 301,  // SetState 0x4652f6: SpreadContagion (entered when a contagion target is found, condition 301) (provisional name)
    kASGUStateContained = 700,      // Update 0x46544c forces it while the unit is off the map (tile x -10000: inside a container); -> idle when back on the map (condition 700) (provisional name)
    kASGUStateChooseAction = 800,   // SetState 0x4652f6: ReleaseTarget, mPostAttackMode 1 when no target was picked, else 2; Update -> 105 flee / 300 attack / 700 by that mode (provisional name)
    kASGUStateAvoidCalamity = 901,  // SetState 0x4652f6: Flee; entered when exposed to a calamity (condition 902) (provisional name)
    kASGUStateRetreat = 905,        // SetState 0x4652f6: Retreat; entered when ShouldRetreat (condition 904) (provisional name)
};

#endif
