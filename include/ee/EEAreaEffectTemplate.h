// EEAreaEffectTemplate.h (RECONSTRUCTION; file name inferred; provisional class name). What an
// EEAreaEffect's +0x10 points at (EEAEManager::CreateAreaEffect builds effects from templates by index; RTTI
// EEAreaEffectTemplate > UReferenceCounter, vtable 0x44be78: ??_G 0x27c17d, +0x04 Load 0x27c5ab, +0x08 Unload
// 0x27c5de). EEE2DMarchingIcons (its ctor 0xe3b1d and Virtual3C 0xe4132) looks up the template whose mKind is its
// area-effect type and takes its icon count.
//
// port (calamities lane, round 3): the numbers are the Rust record EeAreaEffectTemplateFields (ee_tech
// area_effect.rs: the ctor's defaults, Init, the player and area tests); the graphic effects and media stay here.
// Requires lle/LowLevelEngine.h.
#ifndef EE_EEAREAEFFECTTEMPLATE_H
#define EE_EEAREAEFFECTTEMPLATE_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include "rust/sim/tech.h"
#include "rust/game/screens.h"

class EEGraphicEffect;
class EEMedia;
class EEComplexUnit;
class GE2DPoint;

// One area-effect template record as EEAEManager::Load 0x27b079 reads it (0xbc bytes, zeroed first; its name is
// printed when Init fails: "Error reading area effect template %s"). Field names follow the template members they
// fill (Init 0x27c489). Provisional name; layout inferred. The Rust EeAreaEffectData has the same layout.
// the record is the Rust one (ee_sim tech area_effect.rs: the same field names; 188 bytes on every target)
typedef EeAreaEffectData EEAreaEffectData;

// Inherited from EeAreaEffectTemplateFields (the original's offsets): mKind +0x20 (EEE2DMarchingIcons's area-effect
// type), mShape +0x24 (1 a square, 2 a circle), mMode +0x28 (AffectsPlayer's relation), mRadius +0x2c, mColorAlpha
// +0x30, mID +0x34 (EEE2DMarchingIcons::mIconCount), mUnitSet +0x38, mMaxLevel +0x3c, mLevel +0x40, mL44..mL4C, mRed
// .. mAlpha +0x50.., mF60, mB64..mB68, and the color (+0x08 GEColor) as mColorRed, mColorGreen, mColorBlue.
class EEAreaEffectTemplate : public UReferenceCounter, public EeAreaEffectTemplateFields {
public:
    EEAreaEffectTemplate() { ee_screens_aet_construct((uintptr_t)this); }
    virtual ~EEAreaEffectTemplate() { ee_screens_aet_destroy((uintptr_t)this); }        // 0x27c199 (??_G 0x27c17d): dereferences both graphic effects
    // 0x27c334: inPoint lies within inScale * mRadius of inCenter (a square or a circle), and on inCenter's continent
    // unless mUnitSet is 4 (a space dock or turret inUnit carried by a unit also passes on that unit's planet)
    bool Init(const EEAreaEffectData& inData) { return ee_screens_aet_init((uintptr_t)this, (uintptr_t)&inData); }   // 0x27c489 (EEAEManager::Load): the members from inData

    // +0x14 (referenced; EEComplexUnit::GetEffect 0x1f9df3 hands it out, EEAreaEffect::ShowOn plays it on a unit)
    EEGraphicEffect* mGraphicEffect;
    EEGraphicEffect* mGraphicEffect18;      // +0x18 (referenced)
    EEMedia* mMedia;                        // +0x1c (EEMedia::Get; not dereferenced)
};

#endif
