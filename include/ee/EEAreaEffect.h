// EEAreaEffect.h (RECONSTRUCTION; file name inferred). A timed effect on a unit or an area (RTTI EEAreaEffect >
// UReferenceCounter; vtable 0x44be24) and a unit's list of them (RTTI EEUnitAreaEffectList >
// UReferenceContainer<EEAreaEffect*>; vtable 0x446e6c). Requires lle/LowLevelEngine.h.
//
// port (calamities lane, round 3): the eight kinds the original derived (EEAECloaking 0x44ba94, EEAEHealing 0x44baec,
// EEAEEconomyFromUnits 0x44bb80, EEAEConversionImmunity 0x44bbd8, EEAECalamityImmunity 0x44bc28, EEAEMorale 0x44bc78,
// EEAEMoraleFromBuildings 0x44bd6c, EEAEMoraleFromMilitaryHeroes 0x44bdc8) are this one class over the Rust record
// EeAreaEffectFields, keyed by the template's kind (mKind): every slot's body is ee_tech area_effect.rs's, and its
// world calls are answered by the host in EEAreaEffect.cpp. The slots are plain members now (the original's offsets
// in the comments).
#ifndef EE_EEAREAEFFECT_H
#define EE_EEAREAEFFECT_H

#include "ee/UReferenceContainer.h"
#include "rust/ee_tech.h"
#include <stdio.h>

class EEAreaEffectTemplate;
class EEComplexUnit;
class EEUnit;
#include "lle/GE2DPoint.h"              // mLocation by value (every includer already includes it)

// From the original: ctor 0x27bf4b (the template, referenced; mLocation = (0, 0), mUnit = 0, mPlayer = 0, then
// LoadMedia(false)), dtor 0x27bf9f (dereferences the template and the unit, then UnloadMedia), ??_G 0x27bf83.
// Inherited from EeAreaEffectFields: mKind (the template's kind), mPlayer (the owner's player id), and the morale
// from buildings' mMax, mCount, mDirty.
class EEAreaEffect : public UReferenceCounter, public EeAreaEffectFields {
public:
    EEAreaEffect(EEAreaEffectTemplate* inTemplate);   // 0x27bf4b (and each kind's ctor)
    virtual ~EEAreaEffect();
    void AppliedTo(EEComplexUnit* inUnit);  // +0x04 (the cloaking 0x27aa51: SetCloakableChain(true))
    // +0x08 0x27bfcd (the cloaking 0x27aa5f, the healing 0x27ad20): whether the effect covers inUnit, for player
    // inPlayer from inPoint (EEUnitClass 0x631f1e)
    bool Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint);
    void ShowOn(EEUnit* inUnit);            // +0x0c 0x27bff1: the template's graphic effect on inUnit
    void Dump(FILE* inFile);                // +0x10 0x27c014: the out-of-synch dump (4 fprintf)
    long Checksum();                        // +0x14 0x27c073 (EEComplexUnit::GetChecksum sums it)
    long GetStrength();                     // +0x18: which of two effects of a kind a unit keeps
    float GetRadius();                      // +0x1c 0x27c0a0: GetScale() * the template's radius
    bool IsImmunity();                      // +0x20: the two immunities
    void SetUnit(EEComplexUnit* inUnit);    // +0x24 0x27c0cb (referenced)
    void LoadMedia(bool inIsOnDemand);      // +0x28 0x27c0dc: mTemplate->Load(inArg)
    // +0x30 (the morale kinds 0x27b2e3 kind 1, the economy 0x27ac2c kind 2, the cloaking 0x27ab3a kind 3): a
    // modifier of inValue for inKind into outValue (+0x2c and +0x34 were false for every kind: no caller)
    bool Modify(long inKind, long inValue, long& outValue);
    void RemovedFrom(EEComplexUnit* inUnit);   // +0x38 (the cloaking 0x27ab53)
    void MarkDirty();                       // +0x3c (the morale from buildings 0x27b956)
    void UnloadMedia();                     // +0x40 0x27c0eb: mTemplate->Unload()
    void Recount(long inArg);               // +0x44 (the morale from buildings 0x27b95b; inArg unread)
    void Tick(EEComplexUnit* inUnit, float inElapsed);   // +0x48 (the healing 0x27ad56)
    long GetLevel();                        // +0x4c of the morale kinds (0x27b2dc, 0x27b949, 0x27bf22)

    // 0x27b480 (EEAEManager::FindEffect): the template's range test of inPoint around mLocation
    bool IsInRange(const GE2DPoint& inPoint);
    float GetScale();                       // 0x27c0b7: the unit's area-effect scale, 1.0 without a unit

    // +0x08 (= 0, 0; EEUnitAreaEffectList::MoveAndUpdate assigns it)
    GE2DPoint mLocation;
    // +0x10 (referenced; EEE2DMarchingIcons 0xe3b1d/0xe4132: its kind and icon count; ee/EEAreaEffectTemplate.h)
    EEAreaEffectTemplate* mTemplate;
    EEComplexUnit* mUnit;                   // +0x14 (referenced; the owner unit)
};

// sizeof 0x14 (EEComplexUnit holds two, at +0xe4 and +0xf8; the ctor stores vtable 0x446e6c at both). The
// container's members do it all: no members of its own.
class EEUnitAreaEffectList : public UReferenceContainer<EEAreaEffect*> {
public:
    // 0x27c6b9: each effect's Modify of inKind, showing each that applied
    void ApplyModifiers(long inKind, const long& inDamage, long& outDamage, EEComplexUnit* inUnit);
    void Add(EEAreaEffect* inEffect);       // 0x27c607 (keeps a reference)
    // 0x27c623: inEffect, replacing an effect of the same kind (the morale kinds count as one) whose strength is
    // smaller; nothing when the old one's is at least as large
    void AddAreaEffect(EEAreaEffect* inEffect);
    void Tick(EEComplexUnit* inUnit, float inElapsed);       // 0x27c7c9: each effect's Tick
    bool Remove(long inKind);               // 0x27c6f3: the first effect of that kind
    void KeepStrongest();                   // 0x27c721: the list rebuilt through AddAreaEffect
    void MarkDirty();                       // 0x27c768: each effect's MarkDirty (EEPSuggestRecomputeAllAreaEffects)
    void MoveAndUpdate(const GE2DPoint& inPoint, float inValue);  // 0x27c783 (EEComplexUnit::UpdateAreaEffects)
};

#endif
