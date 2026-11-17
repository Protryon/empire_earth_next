// New (port): the C++ side of the Rust tech tree (rust/crates/ee_sim/src/tech nodes.rs; docs/RUST_PLAN.md R5b.2, R5b.4 round
// 29). The tree's, the epochs' and the nodes' plain members are Rust records the classes derive from (ee/EEPlayer.h);
// the bodies are Rust, over one host (EETechHost, EETechTree.cpp) for the containers, the captured entry points, the
// events, the interface, the player, the media and the file. Include after ee/EEPlayer.h (and ee/EEEvent.h for the
// effect view).
#ifndef EE_EETECHRUST_H
#define EE_EETECHRUST_H

#include "rust/sim/tech.h"

// the host of every walk (ctx: the tree)
int32_t EETechHost(void* inTree, int32_t inOp, EeTechHostIo* io);

// a node (or an epoch) as the walks take it: the object and its two records
inline EeTechNodeRef EETechRef(EETechTreeNode* inNode)
{
    EeTechNodeRef theRef = {inNode, inNode, inNode->mData};
    return theRef;
}

// the walks of a node, an epoch and a tree (ee_tech_walk_node/_epoch/_tree over the node's tree)
inline int32_t EETechNodeWalk(EETechTreeNode* inNode, EeTechNodeOp inOp, long inA = 0, bool inB = false,
                              EEComplexUnit* inUnit = NULL)
{
    return ee_tech_walk_node(inNode->mTree, &EETechHost, inOp, inNode->mTree, EETechRef(inNode), (int32_t)inA, inB,
                             inUnit);
}

inline int32_t EETechEpochWalk(EETechTreeEpoch* inEpoch, EeTechEpochOp inOp, long inA = 0, bool inB = false,
                               EEComplexUnit* inUnit = NULL)
{
    return ee_tech_walk_epoch(inEpoch->mTree, &EETechHost, inOp, inEpoch->mTree, EETechRef(inEpoch), (int32_t)inA,
                              inB, inUnit);
}

inline int32_t EETechTreeWalk(EETechTree* inTree, EeTechTreeOp inOp, long inA = 0, long inB = 0,
                              EEComplexUnit* inUnit = NULL)
{
    return ee_tech_walk_tree(inTree, &EETechHost, inOp, inTree, (int32_t)inA, (int32_t)inB, inUnit);
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
