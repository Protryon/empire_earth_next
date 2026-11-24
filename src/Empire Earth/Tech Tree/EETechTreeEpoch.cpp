// C:\EEX\Empire Earth\Tech Tree\EETechTreeEpoch.cpp (file name inferred: no __FILE__; RTTI EETechTreeEpoch >
// EETechTreeNode > RTSTechTreeNode, vtable 0x44612c). One epoch of a player's tech tree: 0x1cdf25..0x1cedce (its key
// chain to EEWorld::GetGameVariant's COMDAT; EETechTreeNode's ctor 0x1cedce opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.


#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/EEButtonObject.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EETechCapture.h"
#include "rust/game/gamecpp.h"
#include "ee/EEUnit.h"
#include "ee/EEStringIDs.h"
#include <iterator>
#include <utility>



// Deletes every tech node of the epoch
void EETechTreeEpoch::DeleteNodes()
{
    ee_gamecpp_tech_epoch_delete_nodes(this, false);
}

// Returns an epoch's cost in a resource scaled by the game variant's epoch cost factors (first, second or
// later epoch advanced in this game)
long EETechTreeEpoch::GetCost(long inResource, long inUnitClass, float inScale)
{
    long theCost = this->GetCostUncaptured(inResource, inUnitClass, inScale);
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EETechCaptureCost(this, 1, inResource, inUnitClass, inScale, theCost);
    return theCost;
}

long EETechTreeEpoch::GetCostUncaptured(long inResource, long inUnitClass, float inScale)
{
    // the scale by the epochs advanced in this game is the Rust tech logic's (ee_tech epoch_cost_scale)
    EEGameVariant* theVariant = EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant);
    unsigned long theEpochs = this->mTree->mCurrentEpoch - this->mTree->mStartEpoch;
    float theScale = ee_tech_epoch_cost_scale((uint32_t)theEpochs, theVariant->GetEpochCostFactor(),
        theVariant->mEpochCostFactor, theVariant->GetSecondEpochCostFactor(), theVariant->GetFirstEpochCostFactor(), inScale);
    return EETechTreeNode::GetCost(inResource, inUnitClass, theScale);
}

// Returns the epoch's research time; computer players need half of it
long EETechTreeEpoch::GetBuildTime()
{
    return ee_gamecpp_tech_epoch_build_time(this);   // (TNMT, TEST ONLY: gamecpp tech_load.rs)
}

// Reads the epoch's record and its tech nodes from the tech tree file (nodes with epoch ids are dropped)
long EETechTreeEpoch::Load(FSFile& inFile, EETechTree* inTree)
{
    return ee_gamecpp_tech_epoch_load(this, &inFile, inTree);   // (gamecpp tech_load.rs)
}

// Unloads and deletes every tech node and the epoch's data
long EETechTreeEpoch::Unload()
{
    return ee_gamecpp_tech_epoch_delete_nodes(this, true);
}

// Appends the ids of the epoch's tech nodes
void EETechTreeEpoch::GetNodeIDs(std::vector<long>& outIDs)
{
    ee_gamecpp_tech_epoch_node_ids(this, &outIDs);
}

// Loads the media of every unit class the epoch's buildings make and the epoch's own media
void EETechTreeEpoch::LoadMedia(EEPlayer* inPlayer)
{
    ee_gamecpp_tech_epoch_load_media(this, inPlayer);
}

// Loads the media of the epoch's nodes of one kind (ee_sim tech node_media.rs)
void EETechTreeEpoch::LoadMediaOfKind(long inKind)
{
    this->EpochWalk(EeTechEpochOp::MediaOfKind, inKind, true, reinterpret_cast<EEComplexUnit*>(this->mTree->GetPlayer()));
}

// Unloads the unit class media the epoch loaded and the epoch's own media
void EETechTreeEpoch::UnloadMedia(EEPlayer* inPlayer)
{
    ee_gamecpp_tech_epoch_unload_media(this, inPlayer);
}

// Unloads the media of the epoch's nodes of one kind
void EETechTreeEpoch::UnloadMediaOfKind(long inKind)
{
    this->EpochWalk(EeTechEpochOp::MediaOfKind, inKind, false, reinterpret_cast<EEComplexUnit*>(this->mTree->GetPlayer()));
}

// Copies another epoch's state and deep-copies its nodes for this tree (the node data stays shared)
EETechTreeEpoch& EETechTreeEpoch::operator=(const EETechTreeEpoch& inOther)
{
    return *(EETechTreeEpoch*)ee_gamecpp_tech_epoch_assign(this, &inOther);   // (gamecpp tech_load.rs)
}

// ------------------------------------------------------------------------- round 10, lane B: the tech tree's C++


static_assert(kTechEpochPrehistoric == 0 && kTechNone == 100000 && kTechFirstNodeID == 1000 &&
              IDS_PREHISTORIC_AGE == 30145, "gamecpp tech_load.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define BNODE ((EETechTreeNode*)p)
#define BEPOCH ((EETechTreeEpoch*)p)
#define BTREE ((EETechTree*)p)

