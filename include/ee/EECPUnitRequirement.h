// C:\EEX\Empire Earth\AI Thread\EECPUnitRequirement.h (RECONSTRUCTION; file name inferred).
// One unit requirement of a computer player's combat plans (RTTI EECPUnitRequirement, no base; vtable 0x43bc18: the
// destructor only; sizeof 0x28: vector<EECPUnitRequirement> steps 0x28). Its members live in EECPAttackCommander.cpp's
// range: default ctor 0x51d19 (-1, -1, 0, 0, 0x23, 0, 0x17, 0, 0), Clear 0x51d49 (the same values), ??_G 0x51d71
// (inline empty dtor), copy ctor 0x528b1 and operator= 0x5296a (both user-written field copies in the order 04, 08, 18,
// 14, 1c, 24, 0c, 10, 20). Member names are provisional.
#ifndef EE_EECPUNITREQUIREMENT_H
#define EE_EECPUNITREQUIREMENT_H

class EECPUnitRequirement {
public:
    EECPUnitRequirement();                  // 0x51d19
    EECPUnitRequirement(const EECPUnitRequirement& inOther);   // 0x528b1
    virtual ~EECPUnitRequirement() {}       // +0x00 (??_G 0x51d71)
    EECPUnitRequirement& operator=(const EECPUnitRequirement& inOther);   // 0x5296a
    void Clear();                           // 0x51d49

    long mL04;                              // +0x04 (-1)
    long mL08;                              // +0x08 (-1)
    long GetNumLeft() const { return this->mNumLeft; }
    long mNumLeft;                          // +0x0c
    long mNumWanted;                        // +0x10
    long GetCategory() const { return this->mCategory; }
    long mCategory;                         // +0x14 (0x23)
    long GetUnitClass() const { return this->mUnitClass; }
    long mUnitClass;                        // +0x18
    long mFamily;                           // +0x1c (0x17)
    long GetUnitKind() const { return this->mUnitKind; }
    long mUnitKind;                         // +0x20
    long GetKind() const { return this->mKind; }
    long mKind;                             // +0x24
};

// Unit requirement kinds (EECPUnitSelector::AllocateRequirements 0x498731).
enum EECPUnitRequirementKind {
    kRequirementCounter = 0,       // AllocateRequirements 0x498731 case 0 -> AllocateCounterUnits (counters of the target's unit types) (provisional name)
    kRequirementHeld = 1,          // case 1 -> AllocateHeldUnits (classes of the group's own units) (provisional name)
    kRequirementSpecial = 2,       // case 2 -> AllocateSpecialUnits (priests/prophets/heroes/carriers; setrilo AttackGatherSpecialUnits) (provisional name)
    kRequirementAlreadyDefined = 3,  // case 3 -> AllocateAlreadyDefinedUnits (trace-named); a given unit class (mUnitClass)
};

#endif
