// EETechTreeRecord.h (RECONSTRUCTION; file name inferred; field names are provisional). One 0x10c-byte
// record of the tech tree database as EETechTreeEpoch::Load 0x1ce10e and EETechTreeNode::Load 0x1cf370 read it
// (FSFile::Read(char*, 0x10c, ...) into a local). Fields by offset, named after the members they are copied to.
// Only the tech tree units include it (an array member costs every includer a VC6 `$E` number).
#ifndef EE_EETECHTREERECORD_H
#define EE_EETECHTREERECORD_H

struct EETechTreeRecord {
    char mName[0x68];                       // +0x00 (not read: the name?)
    long mID;                               // +0x68 -> mU08->mU4C
    long mEpoch;                            // +0x6c -> the epoch's mL14
    long mObsoleteEpoch;                    // +0x70 -> mL18
    long mResearchedEvent;                  // +0x74 -> mU08->mResearchedEvent
    long mObsoleteEvent;                    // +0x78 -> mU08->mObsoleteEvent
    long mCost3;                            // +0x7c -> mU08->mCosts[3]
    long mCost4;                            // +0x80 -> mU08->mCosts[4]
    long mU84;                              // +0x84
    long mCost5;                            // +0x88 -> mU08->mCosts[5]
    long mU8C;                              // +0x8c
    long mCost6;                            // +0x90 -> mU08->mCosts[6]
    long mCost2;                            // +0x94 -> mU08->mCosts[2]
    long mBuildTime;                        // +0x98 -> mU08->mU58
    long mPrerequisite1;                    // +0x9c -> mU08->mU5C
    long mPrerequisite2;                    // +0xa0 -> mU08->mU60
    long mPrerequisite3;                    // +0xa4 -> mU08->mU64
    long mPrerequisite4;                    // +0xa8 -> mU08->mSelectedObjects
    long mUnitClass;                        // +0xac -> mU08->mUnitClass
    long mButton;                           // +0xb0 EEButtonObject::Get -> mButton
    bool mMakesUnit;                        // +0xb4 -> mU08->mB7C
    bool mCountsTowardEpoch;                // +0xb5 -> mU08->mB7D
    bool mBB6;                              // +0xb6
    bool mBB7;                              // +0xb7
    long mNameID;                           // +0xb8 -> mU08->mU74
    long mKind;                             // +0xbc -> mU08->mU48
    long mCostResource;                     // +0xc0 -> mU08->mU38
    long mRequiredStatistic;                // +0xc4 -> mU08->mU3C
    long mRequiredStatisticValue;           // +0xc8 -> mU08->mRequiredStatisticValue
    long mCalamity;                         // +0xcc -> mU08->mFirstNodeID
    long mHotKey;                           // +0xd0 -> mU08->mHotKey
    long mUD4;                              // +0xd4
    long mUD8;                              // +0xd8
    long mUDC;                              // +0xdc
    long mUE0;                              // +0xe0
    float mFactor1;                         // +0xe4 the node Load's cost factor of its second unit class
    float mFactor2;                         // +0xe8 (third)
    float mFactor3;                         // +0xec (fourth)
    float mFactor4;                         // +0xf0 (fifth; the others: 1.0f)
    bool mShowInEditor;                     // +0xf4 -> mU08->mB7E
    bool mObsoletedByDefault;               // +0xf5 -> mU08->mB7F
    bool mAvailableWithEpoch;               // +0xf6 -> mU08->mHasParentalVengeance
    bool mResearchedWithEpoch;              // +0xf7 -> mU08->mHasGroupAttack
    long mUF8;                              // +0xf8
    long mNextEpoch;                        // +0xfc -> the epoch's mU4C
    long mRequiredSOL;                      // +0x100 -> the epoch's mU40
    long mFirstNodeID;                      // +0x104 -> the epoch's mFirstNodeID
    long mDefaultUnitClass;                 // +0x108 -> mU08->mU40
};

static_assert(sizeof(EETechTreeRecord) == 268, "on-disk record: 268 bytes on every target");

#endif
