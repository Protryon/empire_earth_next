// EETechTreeRecord.h (RECONSTRUCTION; file name inferred; field names are provisional). One 0x10c-byte
// record of the tech tree database as EETechTreeEpoch::Load 0x1ce10e and EETechTreeNode::Load 0x1cf370 read it
// (FSFile::Read(char*, 0x10c, ...) into a local). Fields by offset, named after the members they are copied to.
// Only the tech tree units include it (an array member costs every includer a VC6 `$E` number).
#ifndef EE_EETECHTREERECORD_H
#define EE_EETECHTREERECORD_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

struct EETechTreeRecord {
    char mLabel[0x68];                      // +0x00 (not read: the name?)
    int32_t mID;                            // +0x68 -> mU08->mU4C
    int32_t mEpoch;                         // +0x6c -> the epoch's mL14
    int32_t mObsoleteEpoch;                 // +0x70 -> mL18
    int32_t mResearchedEvent;               // +0x74 -> mU08->mResearchedEvent
    int32_t mObsoleteEvent;                 // +0x78 -> mU08->mObsoleteEvent
    int32_t mCost3;                         // +0x7c -> mU08->mCosts[3]
    int32_t mCost4;                         // +0x80 -> mU08->mCosts[4]
    int32_t mUnused1;                       // +0x84
    int32_t mCost5;                         // +0x88 -> mU08->mCosts[5]
    int32_t mUnused2;                       // +0x8c
    int32_t mCost6;                         // +0x90 -> mU08->mCosts[6]
    int32_t mCost2;                         // +0x94 -> mU08->mCosts[2]
    int32_t mBuildTime;                     // +0x98 -> mU08->mU58
    int32_t mPrerequisite1;                 // +0x9c -> mU08->mU5C
    int32_t mPrerequisite2;                 // +0xa0 -> mU08->mU60
    int32_t mPrerequisite3;                 // +0xa4 -> mU08->mU64
    int32_t mPrerequisite4;                 // +0xa8 -> mU08->mSelectedObjects
    int32_t mUnitClass;                     // +0xac -> mU08->mUnitClass
    int32_t mButton;                        // +0xb0 EEButtonObject::Get -> mButton
    bool mMakesUnit;                        // +0xb4 -> mU08->mB7C
    bool mCountsTowardEpoch;                // +0xb5 -> mU08->mB7D
    bool mUnused3;                          // +0xb6
    bool mUnused4;                          // +0xb7
    int32_t mNameID;                        // +0xb8 -> mU08->mU74
    int32_t mKind;                          // +0xbc -> mU08->mU48
    int32_t mCostResource;                  // +0xc0 -> mU08->mU38
    int32_t mRequiredStatistic;             // +0xc4 -> mU08->mU3C
    int32_t mRequiredStatisticValue;        // +0xc8 -> mU08->mRequiredStatisticValue
    int32_t mCalamity;                      // +0xcc -> mU08->mFirstNodeID
    int32_t mHotKey;                        // +0xd0 -> mU08->mHotKey
    int32_t mTitleID;                       // +0xd4 epoch records: the epoch's title string (0: 30145 + the epoch;
                                            // unused in the original, 0 in its data)
    int32_t mUnused6;                       // +0xd8
    int32_t mUnused7;                       // +0xdc
    int32_t mUnused8;                       // +0xe0
    float mFactor1;                         // +0xe4 the node Load's cost factor of its second unit class
    float mFactor2;                         // +0xe8 (third)
    float mFactor3;                         // +0xec (fourth)
    float mFactor4;                         // +0xf0 (fifth; the others: 1.0f)
    bool mShowInEditor;                     // +0xf4 -> mU08->mB7E
    bool mObsoletedByDefault;               // +0xf5 -> mU08->mB7F
    bool mAvailableWithEpoch;               // +0xf6 -> mU08->mHasParentalVengeance
    bool mResearchedWithEpoch;              // +0xf7 -> mU08->mHasGroupAttack
    int32_t mUnused9;                       // +0xf8
    int32_t mNextEpoch;                     // +0xfc -> the epoch's mU4C
    int32_t mRequiredSOL;                   // +0x100 -> the epoch's mU40
    int32_t mFirstNodeID;                   // +0x104 -> the epoch's mFirstNodeID
    int32_t mDefaultUnitClass;              // +0x108 -> mU08->mU40
};

static_assert(sizeof(EETechTreeRecord) == 268, "on-disk record: 268 bytes on every target");

#endif
