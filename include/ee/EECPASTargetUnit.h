// C:\EEX\Empire Earth\AI Thread\EECPASTargetUnit.h (RECONSTRUCTION; file name inferred). A
// target of a computer player's attack sub group (RTTI EECPASTargetUnit, no base; vtable 0x43bf30: the destructor
// only; sizeof 0x14). The sub groups take them from a UMemoryPool<EECPASTargetUnit> (EECPAttackGroup.cpp's
// template-static chain 0x5ac57; allocation 0x61f5d builds one inline: +0x04..+0x10 = 0 before the vptr, an init list);
// the pool's ??_E 0x62492 stores the vptr and calls 0x51952 (the destructor's body is a call: an inline dtor calling
// an out-of-line member). The sub group units' slots score it (EECPAttackSubGroupUnit +0x08, +0x24, +0x2c, ...).
// Names are provisional.
#ifndef EE_EECPASTARGETUNIT_H
#define EE_EECPASTARGETUNIT_H

class EEComplexUnit;

class EECPASTargetUnit {
public:
    EECPASTargetUnit() : mUnit(NULL), mUnitID(0), mNumNearbyTargets(0), mNumAttackers(0) {}
    virtual ~EECPASTargetUnit() { this->Clear(); }
    // 0x51952 (in EECPASGTransport.cpp's range): mUnit->Dereference() when set, then mUnit = 0, mL08 = 0
    void Clear();
    // 0x5196b (EECPASTargetUnit.cpp; the sub group's pool code 0x5d08f calls it): Clear(), then mUnit = inUnit
    // (referenced) and mL08 = its mUnitID (provisional name)
    void SetUnit(EEComplexUnit* inUnit);

    EEComplexUnit* GetUnit() const { return this->mUnit; }
    EEComplexUnit* mUnit;                   // +0x04 (referenced: RTSUnit::Dereference)
    long mUnitID;                           // +0x08
    // inline: the no-/Og priority functor 0x408fd7 reads it through return temps (twice per compare) (provisional name)
    long GetNumNearbyTargets() const { return this->mNumNearbyTargets; }
    long mNumNearbyTargets;                 // +0x0c
    long mNumAttackers;                     // +0x10
};

#endif
