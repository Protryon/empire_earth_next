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
#include "rust/ee_tech.h"

class EEGraphicEffect;
class EEMedia;
class EEComplexUnit;
class GE2DPoint;

// One area-effect template record as EEAEManager::Load 0x27b079 reads it (0xbc bytes, zeroed first; its name is
// printed when Init fails: "Error reading area effect template %s"). Field names follow the template members they
// fill (Init 0x27c489). Provisional name; layout inferred. The Rust EeAreaEffectData has the same layout.
struct EEAreaEffectData {
    char mName[0x68];                       // +0x00
    int32_t mID;                            // +0x68
    int32_t mKind;                          // +0x6c (> 0 for a template: EEAEManager::Load skips the others)
    int32_t mShape;                         // +0x70
    int32_t mGraphicEffect;                 // +0x74 (EEGraphicEffect::Get index, none if <= 0)
    int32_t mUnitSet;                       // +0x78
    int32_t mMode;                          // +0x7c
    int32_t mGraphicEffect18;               // +0x80 (EEGraphicEffect::Get index)
    int32_t mMedia;                         // +0x84 (EEMedia::Get index)
    int32_t mMaxLevel;                      // +0x88
    int32_t mLevel;                         // +0x8c
    int32_t mUnusedValue1;                  // +0x90
    int32_t mUnusedValue2;                  // +0x94
    int32_t mUnusedValue3;                  // +0x98
    float mRadius;                          // +0x9c
    float mRed;                             // +0xa0
    float mGreen;                           // +0xa4
    float mBlue;                            // +0xa8
    float mAlpha;                           // +0xac
    float mUnusedFloat;                     // +0xb0
    bool mUnusedFlag1;                      // +0xb4
    bool mUnusedFlag2;                      // +0xb5
    bool mUnusedFlag3;                      // +0xb6
    bool mUnusedFlag4;                      // +0xb7
    bool mUnusedFlag5;                      // +0xb8
};
static_assert(sizeof(EEAreaEffectData) == 188, "on-disk record: 188 bytes on every target");
static_assert(sizeof(EEAreaEffectData) == sizeof(EeAreaEffectData), "the Rust record's layout");

// Inherited from EeAreaEffectTemplateFields (the original's offsets): mKind +0x20 (EEE2DMarchingIcons's area-effect
// type), mShape +0x24 (1 a square, 2 a circle), mMode +0x28 (AffectsPlayer's relation), mRadius +0x2c, mColorAlpha
// +0x30, mID +0x34 (EEE2DMarchingIcons::mIconCount), mUnitSet +0x38, mMaxLevel +0x3c, mLevel +0x40, mL44..mL4C, mRed
// .. mAlpha +0x50.., mF60, mB64..mB68, and the color (+0x08 GEColor) as mColorRed, mColorGreen, mColorBlue.
class EEAreaEffectTemplate : public UReferenceCounter, public EeAreaEffectTemplateFields {
public:
    EEAreaEffectTemplate();                 // 0x27c0f3
    virtual ~EEAreaEffectTemplate();        // 0x27c199 (??_G 0x27c17d): dereferences both graphic effects
    // 0x27c5ab: EEGraphicEffect::Load(inArg) on both graphic effects, then mMedia's LoadMedia
    void Load(bool inIsOnDemand);
    void Unload();                          // 0x27c5de: EEGraphicEffect::Unload on both, then mMedia's UnloadMedia
    // 0x27c1c5: player inOther counts for inPlayer under mMode (4 everyone, 1 itself, 2 allies, 3 players it is at
    // war with)
    bool AffectsPlayer(long inPlayer, long inOther);
    bool AffectsPlayerReversed(long inPlayer, long inOther);  // 0x27c233: 1 the others, 2 the enemies, 3 the friends
    // 0x27c334: inPoint lies within inScale * mRadius of inCenter (a square or a circle), and on inCenter's continent
    // unless mUnitSet is 4 (a space dock or turret inUnit carried by a unit also passes on that unit's planet)
    bool IsWithin(const GE2DPoint& inCenter, const GE2DPoint& inPoint, float inScale, EEComplexUnit* inUnit);
    bool Init(const EEAreaEffectData& inData);   // 0x27c489 (EEAEManager::Load): the members from inData

    // +0x14 (referenced; EEComplexUnit::GetEffect 0x1f9df3 hands it out, EEAreaEffect::ShowOn plays it on a unit)
    EEGraphicEffect* mGraphicEffect;
    EEGraphicEffect* mGraphicEffect18;      // +0x18 (referenced)
    EEMedia* mMedia;                        // +0x1c (EEMedia::Get; not dereferenced)
};

#endif
