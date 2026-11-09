// New (port): the views the tech tree, EEEvent.cpp and the calamity classes hand to the Rust tech logic
// (rust/crates/ee_tech; docs/RUST_PLAN.md R5b.2). Include after ee/EEPlayer.h (and ee/EEEvent.h for the effect
// view). The C++ classes keep their members; these copy what a decision reads, in the C++'s widths (long is 32 bits
// in the Windows build; the values are tech ids, epochs and flags).
#ifndef EE_EETECHRUST_H
#define EE_EETECHRUST_H

#include "rust/ee_rust.h"

inline EeTechTreeState EETechTreeView(const EETechTree* inTree)
{
    EeTechTreeState theState;
    theState.current_epoch = (int32_t)inTree->mCurrentEpoch;
    theState.start_epoch = (int32_t)inTree->mStartEpoch;
    theState.end_epoch = (int32_t)inTree->mEndEpoch;
    theState.epochs_before_wonders = (int32_t)inTree->mEpochsBeforeWonders;
    theState.epoch_count = (int32_t)gEpochCount;
    return theState;
}

inline EeTechNode EETechNodeView(const EETechTreeNode* inNode)
{
    const RTSTechTreeNodeData* d = inNode->mData;
    EeTechNode theNode;
    theNode.id = (int32_t)d->mID;
    theNode.kind = (int32_t)d->mKind;
    theNode.epoch = (int32_t)inNode->mEpoch;
    theNode.obsolete_epoch = (int32_t)inNode->mObsoleteEpoch;
    theNode.research_delay = (int32_t)inNode->mResearchDelay;
    theNode.prerequisites[0] = (int32_t)d->mPrerequisite1;
    theNode.prerequisites[1] = (int32_t)d->mPrerequisite2;
    theNode.prerequisites[2] = (int32_t)d->mPrerequisite3;
    theNode.prerequisites[3] = (int32_t)d->mPrerequisite4;
    theNode.prerequisite_flags = inNode->mPrerequisiteFlags;
    theNode.researched = inNode->mIsResearched;
    theNode.obsolete = inNode->mIsObsolete;
    theNode.available = inNode->mIsAvailable;
    theNode.editor_obsoleted = inNode->mScenEditorObsoleted;
    theNode.makes_unit = d->mMakesUnit;
    theNode.counts_toward_epoch = d->mCountsTowardEpoch;
    theNode.pad = 0;
    return theNode;
}

inline EeTechEpochCounter EETechEpochCounterView(const EETechTreeEpoch* inEpoch)
{
    EeTechEpochCounter theCounter;
    theCounter.required = (int32_t)inEpoch->mRequiredSOL;
    theCounter.current = (int32_t)inEpoch->mCurrentSOL;
    theCounter.counting = inEpoch->mIsCountingSOL;
    theCounter.available = inEpoch->mIsAvailable;
    theCounter.pad[0] = theCounter.pad[1] = 0;
    return theCounter;
}

#ifdef EE_EVENT_H
inline EeTechEffectValues EETechEffectView(const EEEventEffect* inEffect)
{
    EeTechEffectValues theValues;
    theValues.value = inEffect->mValue;
    theValues.offset = inEffect->mOffset;
    theValues.factor = inEffect->mFactor;
    return theValues;
}
#endif

#endif
