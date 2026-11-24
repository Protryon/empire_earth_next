// C:\EEX\Empire Earth\Tech Tree\EETechTree.cpp (__FILE__ 0x4c5c08; RTTI EETechTree > RTSTechTree, vtables 0x446114 /
// 0x446120). A player's tech tree: 0x1cba2f..0x1cdf25 (the eight key chains and sUnitClassInfo's chain, the ctor ..
// EEEvent::GetEffect's COMDAT; EETechTreeEpoch.cpp's key chain 0x1cdf25 opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.
// port (R5b.4, round 29): the tree's walks, its saved game, its checksum and the media table (sUnitClassInfo) are Rust
// (ee_tech nodes.rs) over the records the classes derive from and EETechHost below.

#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEEvent.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EmpireEarth.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"
#include "ee/EEButtonObject.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAEManager.h"
#include "rust/game/gamecpp.h"

// The walks' host (ee_tech nodes.rs EE_TECH_HOST_*): the tree's containers, the entry points the simulation capture
// wraps (called here so their records keep their order), the world and the capture's walk records
int32_t EETechHost(void* inTree, int32_t inOp, EeTechHostIo* io)
{
    return (int32_t)ee_gamecpp_tech_host(inTree, inOp, io);
}

// Returns the owner's tech tree node of the technology that makes the unit's class (was EECPEngineHelpers.cpp's:
// it reaches EETechTreeEpochOf and EETechTreeEpoch::Get through the inline EETechTree::Get)
EETechTreeNode* EEUnit::GetTechTreeNode()
{
    return this->GetPlayer()->GetTechTree()->Get(this->GetUnitClass()->GetTechnology());
}

// Creates an empty tech tree spanning every epoch
EETechTree::EETechTree()
    : mPlayer(NULL)
{
    ee_gamecpp_tech_tree_init(this);   // (gamecpp tech_load.rs)
}

// Destroys the tech tree and its epochs (and the saved game's lists, Rust's)
EETechTree::~EETechTree()
{
    ee_tech_tree_fields_free(this);
}

// Reads every epoch block of the tech tree file (the original: exactly 15), sets the epoch count, and resets the
// epoch range to all epochs
long EETechTree::Load(FSFileSpec& inFile)
{
    return ee_gamecpp_tech_load_file(this, &inFile);
}

// Indexes, per unit type, the techs whose research event changes its model or sounds or upgrades it into
// another class (the shared table the media-mask queries read; ee_tech nodes.rs build_unit_class_info)
void EETechTree::BuildUnitClassInfo()
{
    EETechTreeWalk(&gTechTree, EeTechTreeOp::BuildUnitClassInfo);
}

// Unloads every epoch's media
void EETechTree::UnloadAll()
{
    ee_gamecpp_tech_unload_all(this);
}

// Collects the ids of every node of every epoch
void EETechTree::GetNodeIDs(std::vector<long>& outIDs)
{
    ee_gamecpp_tech_node_ids(this, true, 0, &outIDs);
}

// Collects the node ids of one epoch
void EETechTree::GetEpochNodeIDs(long inEpoch, std::vector<long>& outIDs)
{
    ee_gamecpp_tech_node_ids(this, false, inEpoch, &outIDs);
}

// Adds a sync checksum of every tech's and epoch's researched/obsolete state, weighted by position (ee_tech
// tree_checksum)
void EETechTree::GetChecksum(ESChecksum& ioChecksum)
{
    ioChecksum.AddChecksum("EETechTree", (unsigned long)(uint32_t)EETechTreeWalk(this, EeTechTreeOp::Checksum));
}

// Adds up the resource costs (kinds 2..6) of a tech and every unresearched prerequisite and upgrade path to it
void EETechTree::SumCosts(long inID, long inUnitClass, long* ioCosts)
{
    ee_gamecpp_tech_sum_costs_captured(this, inID, inUnitClass, ioCosts);   // (TSUM, TEST ONLY: gamecpp tech_load.rs)
}

void EETechTree::SumCostsUncaptured(long inID, long inUnitClass, long* ioCosts)
{
    ee_gamecpp_tech_sum_costs(this, inID, inUnitClass, ioCosts);
}

// Tells whether a tech's model or sound effect for a unit class is the latest one the tree still uses (ee_tech
// is_latest_effect over the media table)
bool EETechTree::IsLatestEffect(EETechTree* inTree, long inID, long inUnitClass, long inKind)
{
    return ee_tech_is_latest_effect(inTree, &EETechHost, inTree, (int32_t)inID, (int32_t)inUnitClass, (int32_t)inKind);
}

// Returns the tree's player (by index when no player pointer is set)
EEPlayer* EETechTree::GetPlayer()
{
    if (this->mPlayer != NULL)
        return this->mPlayer;
    return gWorld.mPlayers[this->mPlayerIndex];
}

// Advances to the next epoch unless the end epoch is reached
void EETechTree::AdvanceEpoch()
{
    EETechTreeWalk(this, EeTechTreeOp::AdvanceEpoch);
}

// Reapplies a loaded tree's state: editor-obsoleted techs, the epoch range, research, each epoch's counters,
// then refreshes the interface
void EETechTree::ApplySavedState(bool inApplyObsoleted, bool inApplyResearch)
{
    EETechTreeWalk(this, EeTechTreeOp::ApplySavedState, inApplyObsoleted ? 1 : 0, inApplyResearch ? 1 : 0);
}

// Reads the saved research, obsolete and editor-obsoleted tech ids, the current/start/end epochs and each epoch's
// state (14 epochs before version 0.0xea); marks every epoch up to the current one researched (ee_tech tree_load)
long EETechTree::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_tech_tree_save(this, &inFile, true, inMajorVersion, inMinorVersion);   // (gamecpp tech_load.rs)
}

// Makes a tech or epoch obsolete in its epoch
void EETechTree::MakeObsolete(long inID)
{
    EETechTreeWalk(this, EeTechTreeOp::MakeObsolete, inID);
}

// Sets a prerequisite of a tech in its epoch
void EETechTree::SetPrerequisite(long inID, long inPrerequisite)
{
    EETechTreeWalk(this, EeTechTreeOp::SetPrerequisite, inID, inPrerequisite);
}

// Researches a tech or epoch; a researched tech that counts toward the next epoch adds to its count, an epoch
// updates statistics series 0xc
void EETechTree::Research(long inID, EEComplexUnit* inUnit)
{
    EETechTreeWalk(this, EeTechTreeOp::Research, inID, 0, inUnit);   // (TTRS, TEST ONLY: ee_sim tech nodes_capture.rs)
}

// Writes the tree to the saved game: the ids of researched, obsolete or editor-obsoleted techs with their flags,
// the epochs, then each epoch's state; returns the last epoch's write result (ee_tech tree_save)
long EETechTree::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_tech_tree_save(this, &outFile, false, 0, 0);   // (gamecpp tech_load.rs)
}

// Moves the tree forward to an epoch (never back), recording it in statistics series 0xc and researching up to it
void EETechTree::SetEpoch(long inEpoch)
{
    EETechTreeWalk(this, EeTechTreeOp::SetEpoch, inEpoch);
}

// Sets the start and end epochs and researches up to the start
void EETechTree::SetEpochRange(long inStart, long inEnd)
{
    EETechTreeWalk(this, EeTechTreeOp::SetEpochRange, inStart, inEnd);
}

// Sets the epoch range and the current epoch to its start, without researching
void EETechTree::ResetEpochRange(long inStart, long inEnd)
{
    EETechTreeWalk(this, EeTechTreeOp::ResetEpochRange, inStart, inEnd);
}

// Makes a node available or unavailable unless it has a research delay
void EETechTree::SetNodeAvailable(long inID, bool inValue)
{
    EETechTreeWalk(this, EeTechTreeOp::SetNodeAvailable, inID, inValue ? 1 : 0);
}

// Sets the tree's player and player index
void EETechTree::SetPlayer(EEPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
    if (inPlayer != NULL)
        this->SetPlayerIndex(inPlayer->mPlayerID);
}

// For the local player's tree, shows the new epoch's name and refreshes the main game form's group panel
void EETechTree::UpdateInterface(bool inEpochChanged)
{
    ee_gamecpp_tech_update_interface(this, inEpochChanged);
}

// Copies another tree's epochs, epoch range and player; the copy is not marked loaded
EETechTree& EETechTree::operator=(const EETechTree& inOther)
{
    return *(EETechTree*)ee_gamecpp_tech_tree_assign(this, &inOther);
}

// ------------------------------------------------------------------------- round 9, lane D: the tech tree's host


// an epoch's nodes into the walk's buffer (at most its count); the epoch's node count
int32_t EETechEpochNodes(EETechTreeEpoch* inEpoch, EeTechHostIo* io)
{
    uint32_t k = 0;
    for (std::map<long, EETechTreeNode*>::iterator it = inEpoch->mNodes.begin(); it != inEpoch->mNodes.end() && k < io->count; ++it)
        io->nodes[k++] = EETechRef(it->second);
    return (int32_t)inEpoch->mNodes.size();
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define TREE ((EETechTree*)p)
#define TNODE ((EETechTreeNode*)p)
#define TPL ((EEPlayer*)p)
#define TFILE ((FSFileSpec*)p)
#define TEFFECT ((EEEventEffect*)p)

