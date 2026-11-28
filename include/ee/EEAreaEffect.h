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
#include <stdio.h>

#include "ee/EEAreaEffectTemplate.h"   // mTemplate's members (LoadMedia, UnloadMedia, IsInRange)
#include "rust/sim/records.h"

class EEComplexUnit;
class EEUnit;

// From the original: ctor 0x27bf4b (the template, referenced; mLocation = (0, 0), mUnit = 0, mPlayer = 0, then
// LoadMedia(false)), dtor 0x27bf9f (dereferences the template and the unit, then UnloadMedia), ??_G 0x27bf83.
// Inherited from EeAreaEffectFields: mKind (the template's kind), mPlayer (the owner's player id), and the morale
// from buildings' mMax, mCount, mDirty.
class EEAreaEffect : public UReferenceCounter, public EeAreaEffectFields {
public:
    EEAreaEffect(EEAreaEffectTemplate* inTemplate) { ee_screens_ae_construct((uintptr_t)this, (uintptr_t)inTemplate); }   // 0x27bf4b (and each kind's ctor)
    virtual ~EEAreaEffect() { ee_screens_ae_destroy((uintptr_t)this); }
    // +0x08 0x27bfcd (the cloaking 0x27aa5f, the healing 0x27ad20): whether the effect covers inUnit, for player
    // inPlayer from inPoint (EEUnitClass 0x631f1e)
    bool Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint)
    {
        return ee_screens_ae_covers((uintptr_t)this, inPlayer, (uintptr_t)inUnit, inPoint.mX, inPoint.mY);
    }
    void Dump(FILE* inFile);                // +0x10 0x27c014: the out-of-synch dump (4 fprintf)
    // Returns a sync checksum of the location's float bits, the player and the owning unit id
    long Checksum();                        // +0x14 0x27c073 (EEComplexUnit::GetChecksum sums it)
    // +0x30 (the morale kinds 0x27b2e3 kind 1, the economy 0x27ac2c kind 2, the cloaking 0x27ab3a kind 3): a
    // modifier of inValue for inKind into outValue (+0x2c and +0x34 were false for every kind: no caller)
    void MarkDirty() { ee_area_effect_mark_dirty(this); }                       // +0x3c (the morale from buildings 0x27b956)
    void Tick(EEComplexUnit* inUnit, float inElapsed)
    {
        ee_screens_ae_tick((uintptr_t)this, (uintptr_t)inUnit, inElapsed);
    }   // +0x48 (the healing 0x27ad56)
    long GetLevel() { return (long)ee_screens_ae_level((uintptr_t)this); }                        // +0x4c of the morale kinds (0x27b2dc, 0x27b949, 0x27bf22)

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
    // Lets every effect modify a value of the given kind, showing each effect that applied
    // Adds an area effect to the unit's list (taking a reference)
    void Add(EEAreaEffect* inEffect) { UReferenceContainer<EEAreaEffect*>::Add(inEffect); }       // 0x27c607 (keeps a reference)
    // Ticks every effect in the list on the unit
    void Tick(EEComplexUnit* inUnit, float inElapsed) { ee_screens_ael_tick(this, inUnit, inElapsed); }       // 0x27c7c9: each effect's Tick
    // Removes the first effect of the given kind; returns whether one was found
    bool Remove(long inKind) { return ee_screens_ael_remove(this, inKind); }               // 0x27c6f3: the first effect of that kind
    // Marks every effect in the list for a recount
    void MarkDirty() { ee_screens_ael_mark_dirty(this); }                       // 0x27c768: each effect's MarkDirty (EEPSuggestRecomputeAllAreaEffects)
    // Moves every effect in the list to a new location and recounts it
    void MoveAndUpdate(const GE2DPoint& inPoint, float inValue)
    {
        ee_screens_ael_move_and_update(this, inPoint.mX, inPoint.mY, inValue);
    }  // 0x27c783 (EEComplexUnit::UpdateAreaEffects)
};

#endif
