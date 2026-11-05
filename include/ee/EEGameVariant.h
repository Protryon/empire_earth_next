// C:\EEX\Empire Earth\...\EEGameVariant.h (RECONSTRUCTION; file path inferred; provisional names). A game variant
// (RTTI EEGameVariant, ctor/dtor around 0xd6910..0xd693c). The game keeps
// the variants in a vector<EEGameVariant*> at 0x4e7f78 (cleared at 0xd6b34); GetGameVariant 0xb0e20 returns entry i
// (an ICF-kept copy of an identical getter in the calamities' range).
#ifndef EE_EEGAMEVARIANT_H
#define EE_EEGAMEVARIANT_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

class FSFileSpec;

// One record of db\dbgamevariant.dat (3 records of 0x114 bytes: "Scenario", "Tournament", "Standard"; read whole by
// the loader 0xd693c into a local, then copied field by field into a new EEGameVariant). Layout from the file
// (dbgamevariant) and the loader's copies; names are provisional.
struct EEGameVariantData {
    char mLabel[100];                       // +0x00
    int32_t mUnusedDatabaseID;              // +0x64
    int32_t mIndex;                         // +0x68 (the loader skips a record whose index is negative) -> +0x70
    float mFactor0;                         // +0x6c -> +0x04 (factor 0)
    int32_t mSet0;                          // +0x70 -> +0x48 (then (long, float) pairs: +0x70/+0x74 .. +0xb8/+0xbc)
    float mFactor1;                         // +0x74 -> +0x08
    int32_t mSet1;                          // +0x78 -> +0x4c
    float mFactor2;                         // +0x7c -> +0x0c
    int32_t mSet2;                          // +0x80 -> +0x50
    float mFactor3;                         // +0x84 -> +0x10
    int32_t mSet3;                          // +0x88 -> +0x54
    float mFactor4;                         // +0x8c -> +0x14
    int32_t mSet4;                          // +0x90 -> +0x58
    float mFactor5;                         // +0x94 -> +0x18
    int32_t mSet5;                          // +0x98 -> +0x5c
    float mFactor6;                         // +0x9c -> +0x1c
    int32_t mSet6;                          // +0xa0 -> +0x60
    float mFactor7;                         // +0xa4 -> +0x20
    int32_t mSet7;                          // +0xa8 -> +0x64
    float mFactor8;                         // +0xac -> +0x24
    int32_t mSet8;                          // +0xb0 -> +0x68
    float mFactor9;                         // +0xb4 -> +0x28
    int32_t mSet9;                          // +0xb8 -> +0x6c
    float mDepositFactor;                   // +0xbc -> +0x2c
    float mEpochCostFactor;                 // +0xc0 -> +0x30
    float mWorkTimeFactor;                  // +0xc4 -> +0x34
    int32_t mMoraleBonus;                   // +0xc8 -> +0x74
    int32_t mNameID;                        // +0xcc -> +0x78 (gResource string id: 31255 / 31254)
    float mFirstEpochCostFactor;            // +0xd0 -> +0x38
    float mSecondEpochCostFactor;           // +0xd4 -> +0x3c
    float mCostSetFactor;                   // +0xd8 -> +0x40
    int32_t mCostSet;                       // +0xdc -> +0x7c
    float mEarthquakeDamageFactor;          // +0xe0 -> +0x44
    int32_t mUnused1;                       // +0xe4 (0 in every record; not copied)
    int32_t mUnused2;                       // +0xe8
    int32_t mUnused3;                       // +0xec
    int32_t mUnused4;                       // +0xf0
    int32_t mUnused5;                       // +0xf4
    int32_t mUnused6;                       // +0xf8
    int32_t mUnused7;                       // +0xfc
    int32_t mUnused8;                       // +0x100
    int32_t mUnused9;                       // +0x104
    int32_t mUnused10;                      // +0x108
    bool mSelectable;                       // +0x10c -> +0x80
    int32_t mUnused11;                      // +0x110 (0 in every record)
};
static_assert(sizeof(EEGameVariantData) == 276, "on-disk record: 276 bytes on every target");

// RTTI EEGameVariant (vtable 0x43e51c, one slot: ??_G 0xd6919; sizeof 0x84: the loader's `new`). The ctor 0xd6910 and
// dtor 0xd6935 only store the vptr (out of line, user-declared empty). Every member is copied from an
// EEGameVariantData record by the loader 0xd693c (types from the copy instructions: mov = long, fld/fstp = float).
// Layout gap-free; names are provisional unless a reader names them.
class EEGameVariant {
public:
    EEGameVariant();                        // 0xd6910
    virtual ~EEGameVariant();               // +0x00 0xd6935 (??_G 0xd6919)
    static EEGameVariant* GetGameVariant(long inIndex);   // 0xb0e20
    static long Load(FSFileSpec* inFile);   // 0xd693c (provisional names)
    static void Unload();                   // 0xd6b34
    float GetFactor(long inUnitType);       // 0xd6b74: the float of the first (set, factor) pair whose set holds it
    // inline: the earthquake's Update loads it as an FP value before its fimul (provisional name)
    float GetEarthquakeDamageFactor() const { return this->mEarthquakeDamageFactor; }
    // inline: the tech tree's cost factors load first, before the other operands (provisional names)
    float GetEpochCostFactor() const { return this->mEpochCostFactor; }
    float GetFirstEpochCostFactor() const { return this->mFirstEpochCostFactor; }
    float GetSecondEpochCostFactor() const { return this->mSecondEpochCostFactor; }
    float GetCostSetFactor() const { return this->mCostSetFactor; }

    // +0x04..+0x28 the (set, factor) pairs' factors and +0x48..+0x6c their unit sets (record +0x6c/+0x70 ..
    // +0xb4/+0xb8): GetFactor 0xd6b74 scans mSets[i] and returns `fld [this + i*4 + 4]` (the loader copies
    // the first factor with mov, which VC6 emits for this float copy too: Load stays exact; provisional names)
    float mFactors[10];                     // +0x04
    float mDepositFactor;                   // +0x2c (record +0xbc)
    // the tech tree's cost factors (EETechTreeEpoch::GetCost 0x1ce038, EETechTreeNode::GetCost 0x1cef26; provisional
    // name)
    float mEpochCostFactor;                 // +0x30
    float mWorkTimeFactor;                  // +0x34
    float mFirstEpochCostFactor;            // +0x38
    float mSecondEpochCostFactor;           // +0x3c
    float mCostSetFactor;                   // +0x40 (the unit set's cost factor)
    float mEarthquakeDamageFactor;          // +0x44 the earthquake's damage factor (provisional name)
    long mSets[10];                         // +0x48 (a gUnitSetManager set id; > 0: used)
    long mIndex;                            // +0x70 (record +0x68)
    long mMoraleBonus;                      // +0x74 (record +0xc8)
    long mNameID;                           // +0x78 its name's string id (the lobby's variant box)
    long mCostSet;                          // +0x7c a gUnitSetManager set (EETechTreeNode::GetCost; provisional name)
    bool mSelectable;                       // +0x80 the lobby's variant list skips the others (provisional name)
};

// 0x4e7f78: every variant (GetGameVariant's table; 0xd6b34 deletes them; provisional name)
extern std::vector<EEGameVariant*> gGameVariants;
inline EEGameVariant* EEGetGameVariantAt(unsigned long inIndex) { return gGameVariants[inIndex]; }   // inline (lobby 0x5dde02) (provisional name)
// 0x1304b4: gGameVariants.size() (the lobby's variant box; provisional name)
unsigned long EEGetNumberOfGameVariants();

#endif
