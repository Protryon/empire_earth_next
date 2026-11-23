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
#include "ee/EETechRust.h"
#include "rust/game/gamecpp.h"
#include "ee/EEUnit.h"
#include "ee/EEStringIDs.h"
#include <iterator>
#include <utility>

#include <stddef.h>
// the file's record as Rust reads it (ee_tech nodes.rs EeTechTreeRecord) is the data tools' (ee/EETechTreeRecord.h)
#define EE_TECH_RECORD_SAME(f) static_assert(offsetof(EeTechTreeRecord, f) == offsetof(EETechTreeRecord, f), #f)
static_assert(sizeof(EeTechTreeRecord) == sizeof(EETechTreeRecord), "the tech tree record");
EE_TECH_RECORD_SAME(mID);
EE_TECH_RECORD_SAME(mCost2);
EE_TECH_RECORD_SAME(mButton);
EE_TECH_RECORD_SAME(mMakesUnit);
EE_TECH_RECORD_SAME(mCalamity);
EE_TECH_RECORD_SAME(mTitleID);
EE_TECH_RECORD_SAME(mFactor1);
EE_TECH_RECORD_SAME(mShowInEditor);
EE_TECH_RECORD_SAME(mResearchedWithEpoch);
EE_TECH_RECORD_SAME(mNextEpoch);
EE_TECH_RECORD_SAME(mDefaultUnitClass);
#undef EE_TECH_RECORD_SAME


// Creates an empty epoch node (no techs counted toward it yet)
EETechTreeEpoch::EETechTreeEpoch()
{
    // the counters start at 0 (the record's init)
}

// Destroys the epoch and the tech nodes it owns
EETechTreeEpoch::~EETechTreeEpoch()
{
    this->DeleteNodes();
}

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

// Returns how many techs counting toward this epoch were researched
long EETechTreeEpoch::GetCurrentSOL()
{
    return this->mCurrentSOL;
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

// Tells whether the epoch can be researched: available, not obsolete, not researched
bool EETechTreeEpoch::GetIsAvailable()
{
    return EETechNodeWalk(this, EeTechNodeOp::IsAvailable) != 0;
}

// Tells whether the epoch is obsolete
bool EETechTreeEpoch::GetIsObsolete()
{
    return this->mIsObsolete;
}

// Counts one more tech toward the epoch; when enough are researched the epoch becomes available (event 0x7d2)
void EETechTreeEpoch::AddSOL()
{
    if (this->mTree != NULL)   // (TSOL, TEST ONLY: ee_sim tech nodes_capture.rs)
        EETechEpochWalk(this, EeTechEpochOp::AddSol);
}

// Loads the media of every unit class the epoch's buildings make and the epoch's own media
void EETechTreeEpoch::LoadMedia(EEPlayer* inPlayer)
{
    ee_gamecpp_tech_epoch_load_media(this, inPlayer);
}

// Loads the media of the epoch's nodes of one kind (ee_sim tech node_media.rs)
void EETechTreeEpoch::LoadMediaOfKind(long inKind)
{
    EETechEpochWalk(this, EeTechEpochOp::MediaOfKind, inKind, true, reinterpret_cast<EEComplexUnit*>(this->mTree->GetPlayer()));
}

// Makes the epoch available once enough techs count toward it, and gives its nodes the epoch prerequisite
void EETechTreeEpoch::UpdateAvailability()
{
    if (this->mTree != NULL)   // (TSOL, TEST ONLY: ee_sim tech nodes_capture.rs)
        EETechEpochWalk(this, EeTechEpochOp::UpdateAvailability);
}

// Researches the epoch itself
long EETechTreeEpoch::ResearchNode(EEComplexUnit* inUnit)
{
    long theID = this->mData->mID;
    return this->Research(theID, inUnit);
}

// Researches the epoch's nodes of one kind (only those researched with the epoch when asked)
void EETechTreeEpoch::ResearchKind(long inKind, bool inEpochOnly)
{
    EETechEpochWalk(this, EeTechEpochOp::ResearchKind, inKind, inEpochOnly);
}

// Researches a node of the epoch, or the epoch itself: fires its event, makes it current, obsoletes outdated nodes;
// returns 2 when already researched, 9 when not possible
long EETechTreeEpoch::Research(long inID, EEComplexUnit* inUnit)
{
    return EETechEpochWalk(this, EeTechEpochOp::Research, inID, false, inUnit);   // (TNRS, TEST ONLY: nodes_capture.rs)
}

// Unloads the unit class media the epoch loaded and the epoch's own media
void EETechTreeEpoch::UnloadMedia(EEPlayer* inPlayer)
{
    ee_gamecpp_tech_epoch_unload_media(this, inPlayer);
}

// Unloads the media of the epoch's nodes of one kind
void EETechTreeEpoch::UnloadMediaOfKind(long inKind)
{
    EETechEpochWalk(this, EeTechEpochOp::MediaOfKind, inKind, false, reinterpret_cast<EEComplexUnit*>(this->mTree->GetPlayer()));
}

// Copies another epoch's state and deep-copies its nodes for this tree (the node data stays shared)
EETechTreeEpoch& EETechTreeEpoch::operator=(const EETechTreeEpoch& inOther)
{
    return *(EETechTreeEpoch*)ee_gamecpp_tech_epoch_assign(this, &inOther);   // (gamecpp tech_load.rs)
}

// ------------------------------------------------------------------------- round 10, lane B: the tech tree's C++

int64_t EECore10OpB8(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEPCalamityHelper.cpp

static_assert(kTechEpochPrehistoric == 0 && kTechNone == 100000 && kTechFirstNodeID == 1000 &&
              IDS_PREHISTORIC_AGE == 30145, "gamecpp tech_load.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define BNODE ((EETechTreeNode*)p)
#define BEPOCH ((EETechTreeEpoch*)p)
#define BTREE ((EETechTree*)p)

// the tech tree's loads, copies, costs and sweep (gamecpp tech_load.rs, tech_capture.rs; p the node, the epoch, the
// tree or the player): one statement each
int64_t EECore10OpB7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TbNewData: return P64(BNODE->mData = new RTSTechTreeNodeData);
    case O::TbDeleteData: delete std::exchange(BNODE->mData, nullptr);
        break;
    case O::TbShareData: BNODE->mData = ((const EETechTreeNode*)a[0])->mData;
        break;
    case O::TbSetTree: BNODE->mTree = (EETechTree*)a[0];
        break;
    case O::TbNewNode: return P64(new EETechTreeNode);
    case O::TbClassesResize: BNODE->mData->mUnitClasses.resize((unsigned long)a[0], 0);
        break;
    case O::TbClassRead: return ((FSFile*)a[0])->Read(BNODE->mData->mUnitClasses[a[1]]);
    case O::TbClassCount: return (int64_t)BNODE->mData->mUnitClasses.size();
    case O::TbClassAt: return (int64_t)BNODE->mData->mUnitClasses[a[0]];
    case O::TbCostsInsert: BNODE->mData->mClassCosts.insert(std::map<long, long*>::value_type((long)BNODE->mData->mUnitClasses[a[0]], new long[7]{(long)a[1], (long)a[2], (long)a[3], (long)a[4], (long)a[5], (long)a[6], (long)a[7]}));
        break;
    case O::TbCostsCount: return (int64_t)BNODE->mData->mClassCosts.size();
    case O::TbCostsDelete: delete[] std::next(BNODE->mData->mClassCosts.begin(), a[0])->second;
        break;
    case O::TbCostsClear: BNODE->mData->mClassCosts.clear();
        break;
    case O::TbButton: return P64(BNODE->mButton);
    case O::TbCopyButton: BNODE->mButton = ((const EETechTreeNode*)a[0])->mButton;
        break;
    case O::TbSetButton: BNODE->mButton = EEButtonObject::Get((unsigned long)a[0]);
        break;
    case O::TbButtonGet: return P64(EEButtonObject::Get((unsigned long)a[0]));
    case O::TbSwapButton: ((EEPlayer*)p)->SwapClassButtonMedia((long)a[0], (EEButtonObject*)a[1], (EEButtonObject*)a[2]);
        break;
    case O::TbGlobalTree: return P64(&gTechTree);
    case O::TbEpochInsert: BEPOCH->mNodes.insert(std::map<long, EETechTreeNode*>::value_type(((EETechTreeNode*)a[0])->mData->mID, (EETechTreeNode*)a[0]));
        break;
    case O::TbSetPlayerIndex: BTREE->mPlayerIndex = (long)a[0];
        break;
    case O::TbCopyPlayer: BTREE->mPlayer = ((const EETechTree*)a[0])->mPlayer;
        break;
    case O::TbIsHuman: return ((EEPlayer*)p)->IsHuman();
    case O::TbClassFamily: return ((EEUnitClass*)p)->mFamily;
    case O::TbNodeIsObsolete: return BNODE->GetIsObsolete();
    case O::TbNodeDependsOn: return BNODE->DependsOn((long)a[0]);
    case O::TbNodeGetCost: return BNODE->GetCost((long)a[0], (long)a[1], f[0]);
    case O::TbTreeChecksum: return (int64_t)(uint32_t)EETechTreeWalk(BTREE, EeTechTreeOp::Checksum);
    case O::TbWorldPlayers: return (int64_t)gWorld.GetNumPlayers();
    case O::TbWorldPlayer: return P64(gWorld.GetPlayer((unsigned long)a[0]));
    case O::TbPlayerTree: return P64(((EEPlayer*)p)->GetTechTree());
    default: return EECore10OpB8(inOp, p, a, f);
    }
    return 0;
}

