// EEAreaEffectTemplate.h (RECONSTRUCTION; file name inferred; provisional class name). What an
// EEAreaEffect's +0x10 points at (EEAEManager::CreateAreaEffect builds effects from templates by index; RTTI
// EEAreaEffectTemplate > UReferenceCounter, vtable 0x44be78: ??_G 0x27c17d, +0x04 Load 0x27c5ab, +0x08 Unload
// 0x27c5de). EEE2DMarchingIcons (its ctor 0xe3b1d and Virtual3C 0xe4132) looks up the template whose mKind is its
// area-effect type and takes its icon count. The ctor 0x27c0f3 and dtor 0x27c199 give the
// layout below; provisional names. Requires lle/LowLevelEngine.h, lle/GEColor.h.
#ifndef EE_EEAREAEFFECTTEMPLATE_H
#define EE_EEAREAEFFECTTEMPLATE_H

class EEGraphicEffect;
class EEMedia;
class EEComplexUnit;
class GE2DPoint;

// One area-effect template record as EEAEManager::Load 0x27b079 reads it (0xbc bytes, zeroed first; its name is
// printed when Init fails: "Error reading area effect template %s"). Field names follow the template members they
// fill (Init 0x27c489). Provisional name; layout inferred.
struct EEAreaEffectData {
    char mName[0x68];                       // +0x00
    long mID;                               // +0x68
    long mKind;                             // +0x6c (> 0 for a template: EEAEManager::Load skips the others)
    long mShape;                            // +0x70
    long mGraphicEffect;                    // +0x74 (EEGraphicEffect::Get index, none if <= 0)
    long mUnitSet;                          // +0x78
    long mMode;                             // +0x7c
    long mGraphicEffect18;                  // +0x80 (EEGraphicEffect::Get index)
    long mMedia;                            // +0x84 (EEMedia::Get index)
    long mMaxLevel;                         // +0x88
    long mLevel;                            // +0x8c
    long mUnusedValue1;                     // +0x90
    long mUnusedValue2;                     // +0x94
    long mUnusedValue3;                     // +0x98
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

class EEAreaEffectTemplate : public UReferenceCounter {
public:
    EEAreaEffectTemplate();                 // 0x27c0f3
    virtual ~EEAreaEffectTemplate();        // +0x00 0x27c199 (??_G 0x27c17d): dereferences both graphic effects
    // +0x04 0x27c5ab: EEGraphicEffect::Load(inArg) on both graphic effects, then mMedia's +0x04 (EEAreaEffect::LoadMedia)
    virtual void Load(bool inIsOnDemand);
    virtual void Unload();                  // +0x08 0x27c5de: EEGraphicEffect::Unload on both, then mMedia's +0x08
    // 0x27c1c5 (ret 8): player inOther counts for inPlayer under mMode (players by id from gWorld's table:
    // 4 everyone, 1 itself, 2 allies, 3 players it is at war with) (provisional name)
    bool AffectsPlayer(long inPlayer, long inOther);
    bool AffectsPlayerReversed(long inPlayer, long inOther);  // 0x27c233 (ret 8): the same test, the other way round (provisional name)
    // 0x27c28e (ret 0x10; EEAreaEffect::Covers): inUnit is in mUnitSet, AffectsPlayer accepts its owner, it is on the
    // map (+0x34c), and it lies within inScale * mRadius of inPoint (0x27c334) (provisional name)
    bool CoversUnit(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint, float inScale);
    // 0x27c334 (ret 0x10): inPoint lies within inScale * mRadius of inCenter (mL24 1: a square, 2: a circle), and on
    // inCenter's continent unless mUnitSet is 4 (some unit kinds of inUnit's complex unit by its +0x35c) (provisional
    // name)
    bool IsWithin(const GE2DPoint& inCenter, const GE2DPoint& inPoint, float inScale, EEComplexUnit* inUnit);
    bool Init(const EEAreaEffectData& inData);   // 0x27c489 (ret 4; EEAEManager::Load): the members from inData

    // +0x08 (GEColor's default ctor, then assigned white again in the ctor body; Init sets it from mF50..mF58)
    GEColor mColor;
    // +0x14 (referenced; EEComplexUnit::GetEffect 0x1f9df3 hands it out, EEAreaEffect::ShowOn plays it on a unit)
    EEGraphicEffect* mGraphicEffect;
    EEGraphicEffect* mGraphicEffect18;      // +0x18 (referenced)
    EEMedia* mMedia;                        // +0x1c (EEMedia::Get; Load/Unload through its +0x04/+0x08; not dereferenced)
    long mKind;                             // +0x20 (= 0; compared with EEE2DMarchingIcons::mAreaEffectType)
    long mShape;                            // +0x24 (= 1; IsWithin's shape: 1 square, 2 circle)
    long mMode;                             // +0x28 (= 2; AffectsPlayer's player relation)
    float mRadius;                          // +0x2c (= 1.0f; EEAreaEffect::GetRadius scales it)
    float mColorAlpha;                      // +0x30 (= 0.15f; Init: mAlpha)
    long mID;                               // +0x34 (EEE2DMarchingIcons::mIconCount; the ctor leaves it) (provisional name)
    long mUnitSet;                          // +0x38 (= -1; EEUnitSetManager::Contains)
    long mMaxLevel;                         // +0x3c (= -1; EEAECloaking's Virtual18)
    long mLevel;                            // +0x40 (= -1; EEAEHealing's Virtual18, EEAEMorale's Virtual4C)
    long mL44;                              // +0x44 (= -1)
    long mL48;                              // +0x48 (= -1)
    long mL4C;                              // +0x4c (= -1)
    float mRed;                             // +0x50 (= -1.0f)
    float mGreen;                           // +0x54 (= -1.0f)
    float mBlue;                            // +0x58 (= -1.0f)
    float mAlpha;                           // +0x5c (= -1.0f)
    float mF60;                             // +0x60 (= -1.0f)
    bool mB64;                              // +0x64 (= false)
    bool mB65;                              // +0x65
    bool mB66;                              // +0x66
    bool mB67;                              // +0x67
    bool mB68;                              // +0x68
};

#endif
