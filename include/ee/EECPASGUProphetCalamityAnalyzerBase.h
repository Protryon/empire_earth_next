// C:\EEX\Empire Earth\AI Thread\EECPASGUProphetCalamityAnalyzerBase.h (RECONSTRUCTION; file name inferred). How
// a prophet (EECPASGUProphet) rates casting one calamity: RTTI
// EECPASGUProphetCalamityAnalyzerBase (no base; vtable 0x439684; sizeof 0x3c) and its 13 kinds (sizeof 0x3c; Teleport and
// ShieldBattery 0x40). EECPASGUProphet::CreateAnalyzer (+0x84 0xb375, EECPASGUMechProphet's 0x7cae) news one per
// calamity kind: the base ctor 0x90d4 out of line, then the kind's vptr (inline kind ctors). The base ctor's home
// unit is EECPASGUPCTargetIsUnitSetScreenBase.cpp's range (first called from 0x7cae, before it). Slot signatures
// from the bodies; provisional names. Requires ee/EEUnit.h (EEComplexUnit),
// <vector>.
#ifndef EE_EECPASGUPROPHETCALAMITYANALYZERBASE_H
#define EE_EECPASGUPROPHETCALAMITYANALYZERBASE_H

#include "lle/U2DPoint.h"           // mTile (no statics)

class EEComplexUnit;
class EECalamityClass;
class EECPASGUProphet;
class EECPASGUPCTargetIsUnitSetScreenBase;
class EECPASTargetUnit;
class EECPAttackSubGroupUnit;
class EEUnitSet;

class EECPASGUProphetCalamityAnalyzerBase {
public:
    // 0x90d4: mTile (0, 0) and mTechID in the init list; then mProphet, mUnitSet = 0, mCalamityIndex (the tech
    // node's +0x08->+0x44), the manager's calamity class into mCalamityClass (0xc473), mUnitSet =
    // gUnitSetManager.GetSet(mCalamityClass->Virtual1C()), mL24 = mCalamityClass->Virtual10(), the rest 0
    EECPASGUProphetCalamityAnalyzerBase(EECPASGUProphet* inProphet, long inTechID);
    // ??_G 0x7e96 (the kinds without own members share it): the own vptr, then ReleaseUnit() (a call)
    virtual ~EECPASGUProphetCalamityAnalyzerBase() { this->ReleaseUnit(); }
    // +0x04 0x9162: inValue scaled by the prophet's +0x2d4 / mL24, clamped
    virtual float ScaleByPower(float inValue);
    virtual bool CanAfford();               // +0x08 0x91b5 (the prophet's +0x2d4 >= mL24)
    // +0x0c 0x91cc: ReleaseUnit(), then mF20 = 0, mB34 = false, mTile = (0, 0), mL28 = mL2C = mL30 = 0
    virtual void Reset();
    // +0x10 0x91ef: Gather()s the targets through a ScreenBase(mProphet, this, true); the first target's unit or 0
    virtual EEComplexUnit* PickTarget();
    virtual float Evaluate() = 0;           // +0x14
    virtual bool IsApplicable();            // +0x18 0x18aa9 (`mov al, 1`, ICF-shared)
    virtual void Virtual1C();               // +0x1c 0xe123d (`ret`, ICF-shared)

    // score modifiers several kinds chain in Virtual14 (provisional names): 0xaa8b (Earthquake, Fire, Famine,
    // NanoVirus, Inferno), 0x718e (NanoVirus, Inferno)
    float ApplyTownPenalty(float inScore);
    float ApplyClusterBonus(float inScore);
    // inline: the screens' Contains calls load the unit type before the set pointer (0x9661, 0xa4a0)
    EEUnitSet* GetUnitSet() const { return this->mUnitSet; }
    // inline (Teleport::Fn06B9E 0x406b9e loads the argument into a register before the object: the argument comes
    // from a getter) (provisional name)
    EEComplexUnit* GetUnit() const { return this->mUnit; }
    void ReleaseUnit();                     // 0x9303: mUnit->Dereference() when set, mUnit = 0
    void SetUnit(EEComplexUnit* inUnit);    // 0x969b: the referenced mUnit becomes inUnit (provisional name)
    // 0x92a2 (ret 8): appends the sub group's targets that pass inScreen
    void Gather(std::vector<EECPASTargetUnit*>& outTargets, EECPASGUPCTargetIsUnitSetScreenBase* inScreen);

    // +0x04 the target tile (init list; one point: Teleport::Virtual28 0x6b24 passes &mTile as EECanStandOn's
    // const U2DPoint<long>&) (provisional name)
    U2DPoint<long> mTile;
    EECPASGUProphet* GetProphet() const { return this->mProphet; }
    EECPASGUProphet* mProphet;              // +0x0c
    EECalamityClass* GetCalamityClass() const { return this->mCalamityClass; }
    EECalamityClass* mCalamityClass;        // +0x10 (filled by EECalamityManager 0xc473)
    EEUnitSet* mUnitSet;                    // +0x14 (EECPASGUPCTargetIsUnitSetScreenBase reads it)
    long mTechID;                           // +0x18 (init list)
    long mCalamityIndex;                    // +0x1c
    float mScore;                           // +0x20
    long mCost;                             // +0x24 (mCalamityClass->Virtual10())
    long mL28;                              // +0x28
    long mL2C;                              // +0x2c
    long mL30;                              // +0x30
    bool mIsExcluded;                       // +0x34
    bool mB35;                              // +0x35
    EEComplexUnit* mUnit;                   // +0x38 (referenced; ReleaseUnit)
};

// The kinds: inline ctors (the base ctor, then the vptr); slot bodies by vtable.

// vtable 0x43963c
class EECPASGUProphetCalamityAnalyzerAntiMatterStorm : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerAntiMatterStorm(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xac63 (ICF: Hurricane's)
    virtual float Evaluate();               // +0x14 0x769e
    virtual bool IsApplicable();            // +0x18 0x7e85
    // Virtual14's modifiers (provisional names)
    float AddTargetVeterancy(float inScore);  // 0x760e: + each gathered target's unit +0x33e when mCounts.mL18 > 0
    float ApplyAircraftBonus(float inScore);  // 0x773d: + mCounts.mL18 * 0.05, mL1C * 0.1, mL20 * 0.3
};

// vtable 0x4398ac
class EECPASGUProphetCalamityAnalyzerEarthquake : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerEarthquake(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0x9909
    virtual float Evaluate();               // +0x14 0x9d77
    virtual bool IsApplicable();            // +0x18 0xb4c4 (ICF: Fire's)
    // Virtual14's modifiers (provisional names)
    float ApplyLargestTownBonus(float inScore);  // 0x9831 (an OnlyBuildings screen)
};

// vtable 0x439918
class EECPASGUProphetCalamityAnalyzerFamine : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerFamine(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xa74c
    virtual float Evaluate();               // +0x14 0xaa09
    virtual bool IsApplicable();            // +0x18 0xb513 (ICF: Plague's)
    // Virtual14's modifiers (provisional names)
    float ApplyCitizenBonus(float inScore);  // 0xaaba
    float ApplySplashBonus(float inScore);  // 0xab2b
};

// vtable 0x439838
class EECPASGUProphetCalamityAnalyzerFire : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerFire(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0x9e07
    virtual float Evaluate();               // +0x14 0xa0ba
    virtual bool IsApplicable();            // +0x18 0xb4c4 (ICF: Earthquake's)
    // Virtual14's modifiers (provisional names)
    float ApplySplashBonus(float inScore);  // 0xa131
};

// vtable 0x4398d0
class EECPASGUProphetCalamityAnalyzerHurricane : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerHurricane(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xac63 (ICF: AntiMatterStorm's)
    virtual float Evaluate();               // +0x14 0xaf05
    virtual bool IsApplicable();            // +0x18 0xb562
    // Virtual14's modifiers (provisional names)
    float AddTargetVeterancy(float inScore);  // 0xabd3
    float ApplyShipFactor(float inScore);   // 0xaf78
};

// vtable 0x439618
class EECPASGUProphetCalamityAnalyzerInferno : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerInferno(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0x7236 (ICF: NanoVirus's)
    virtual float Evaluate();               // +0x14 0x74f3
    virtual bool IsApplicable();            // +0x18 0x7e74
    // 0x759d: * (1 + 0.025 * the targets of category 6 or 0x16) (provisional name)
    float ApplyCitizenBonus(float inScore);
};

// vtable 0x439660
class EECPASGUProphetCalamityAnalyzerNanoVirus : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerNanoVirus(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0x7236 (ICF: Inferno's)
    virtual float Evaluate();               // +0x14 0x70f3
    virtual bool IsApplicable();            // +0x18 0x7e63 (the sub group's +0x140 > 0)
};

// vtable 0x43985c
class EECPASGUProphetCalamityAnalyzerNegativeMorale : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerNegativeMorale(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xb5b1 (ICF: ShieldBattery's, Teleport's, TimeWarp's)
    virtual float Evaluate();               // +0x14 0xb0d0
    virtual bool IsApplicable();            // +0x18 0xb5c9
    virtual float PickBest(EEComplexUnit*& outUnit);    // +0x20 0xafb5 (outUnit = 0 first)
    virtual float ScoreTarget(EEComplexUnit* inUnit);   // +0x24 0xb169 (0 when inUnit is 0)
};

// vtable 0x4398f4
class EECPASGUProphetCalamityAnalyzerPlague : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerPlague(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xa1d9
    virtual float Evaluate();               // +0x14 0xa4e4
    virtual bool IsApplicable();            // +0x18 0xb513 (ICF: Famine's)
    // Virtual14's modifiers (provisional names)
    float ApplyStrongTownPenalty(float inScore);  // 0xa554: * 0.02 when the prophet stands in one of its player's towns
    float ApplyRandomFactor(float inScore);  // 0xa583: * a random 0.45..0.9
    float ApplyCitizenBonus(float inScore);  // 0xa5a9: * (1 + 0.055 * the targets of category 6 or 0x16)
    float ApplySplashBonus(float inScore);  // 0xa61a
};

// vtable 0x439580; sizeof 0x40
class EECPASGUProphetCalamityAnalyzerShieldBattery : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerShieldBattery(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID), mTarget(NULL) {}
    virtual ~EECPASGUProphetCalamityAnalyzerShieldBattery();   // ??_G 0x7eb8 calls it
    // +0x0c 0x7e03 (ICF: Teleport's): mTarget->Dereference() when set, mTarget = 0, then the base Reset()
    virtual void Reset();
    virtual EEComplexUnit* PickTarget();    // +0x10 0xb5b1 (ICF)
    virtual float Evaluate();               // +0x14 0x79b0
    virtual float PickBest(EEComplexUnit*& outUnit);    // +0x20 0x7799
    virtual float PickAlly(EEComplexUnit*& outUnit);    // +0x24 0x78ae (a local set<UString>)
    virtual float ScoreTarget(EEComplexUnit* inUnit);   // +0x28 0x7a9d
    virtual float ScoreAlly(EECPAttackSubGroupUnit* inUnit); // +0x2c 0x7bab actual subgroup-unit argument; independently introduced slot
    void SetTarget(EEComplexUnit* inUnit);  // ICF-folded into Teleport's 0x6afd
    // 0x797c (provisional name): inUnit's unit alive, +0x347 clear, on the map, and in mUnitSet
    bool MayShield(EECPAttackSubGroupUnit* inUnit);

    EEComplexUnit* mTarget;                 // +0x3c (referenced)
};

// vtable 0x4395e0; sizeof 0x40
class EECPASGUProphetCalamityAnalyzerTeleport : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerTeleport(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID), mTarget(NULL) {}
    virtual ~EECPASGUProphetCalamityAnalyzerTeleport();   // ??_G 0x7e1f calls 0x7e3b
    virtual void Reset();                   // +0x0c 0x7e03 (ICF: ShieldBattery's)
    virtual EEComplexUnit* PickTarget();    // +0x10 0xb5b1 (ICF)
    virtual float Evaluate();               // +0x14 0x6c84
    virtual bool IsApplicable();            // +0x18 0x7dd3 (ICF: TimeWarp's)
    virtual float PickBest(EEComplexUnit*& outUnit);    // +0x20 0x68b4
    virtual float PickAlly(EEComplexUnit*& outUnit);    // +0x24 0x69c9
    virtual bool CanPlace(EEComplexUnit* inUnit);    // +0x28 0x6b24 (the unit: EECanStandOn's and EEFindPlacement 0x243561's EEComplexUnit*)
    virtual float ScoreTarget(EEComplexUnit* inUnit);   // +0x2c 0x6d6d
    virtual float ScoreAlly(EEComplexUnit* inUnit);     // +0x30 0x6f1a
    // 0x6afd: the referenced mTarget becomes inUnit (ShieldBattery's twin is ICF-folded into it)
    void SetTarget(EEComplexUnit* inUnit);
    bool MayGo(EECPAttackSubGroupUnit* inUnit);     // 0x6b9e (this-call, ret 4; Virtual24's unit filter) (provisional name)

    EEComplexUnit* mTarget;                 // +0x3c (referenced)
};

// vtable 0x4395b4
class EECPASGUProphetCalamityAnalyzerTimeWarp : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerTimeWarp(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0xb5b1 (ICF)
    virtual float Evaluate();               // +0x14 0x66c6
    virtual bool IsApplicable();            // +0x18 0x7dd3 (ICF: Teleport's)
    virtual float PickBest(EEComplexUnit*& outUnit);    // +0x20 0x65b0
    virtual float ScoreTarget(EEComplexUnit* inUnit);   // +0x24 0x675f
};

// vtable 0x439888
class EECPASGUProphetCalamityAnalyzerVolcano : public EECPASGUProphetCalamityAnalyzerBase {
public:
    EECPASGUProphetCalamityAnalyzerVolcano(EECPASGUProphet* inProphet, long inTechID)
        : EECPASGUProphetCalamityAnalyzerBase(inProphet, inTechID) {}
    virtual EEComplexUnit* PickTarget();    // +0x10 0x9518
    virtual float Evaluate();               // +0x14 0x975e
    virtual bool IsApplicable();            // +0x18 0x96c2
    // Virtual14's modifiers (provisional names)
    float ApplyChokePointBonus(float inScore, bool inArg, bool& outFound);  // 0x9318 (ret 0xc)
    float ApplyWeakTownPenalty(float inScore);  // 0x9802: * 0.25 when the prophet stands in one of its player's towns
};

#endif
