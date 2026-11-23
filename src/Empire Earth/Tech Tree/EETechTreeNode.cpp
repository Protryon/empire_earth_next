// C:\EEX\Empire Earth\Tech Tree\EETechTreeNode.cpp (file name inferred: no __FILE__; RTTI EETechTreeNode >
// RTSTechTreeNode, vtables 0x446150 / 0x446174). One node of a player's tech tree: 0x1cedce..0x1cfeef (the ctor to the
// inline RTSTechTreeNode members that close the unit; the next unit, EEUIECampaignEditorEvents.cpp, opens with its
// globals chain 0x1cfeef). Classes in ee/EEPlayer.h. Unit flags: the exe's module set.

#include <windows.h>
#include <stdlib.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "lle/UIHotKey.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameVariant.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechCapture.h"
#include "ee/EETechRust.h"
#include "rust/game/gamecpp.h"
// Creates an empty tech node, allowed and obsolete only after the space epoch
EETechTreeNode::EETechTreeNode()
    : mTree(NULL), mButton(NULL)
{
    // the record's values (allowed, obsolete after the space epoch): RTSTechTreeNode's ee_tech_node_fields_init
}

// Returns the node's research time
long EETechTreeNode::GetBuildTime()
{
    return this->mData->mBuildTime;
}

// Destroys the node, unloading its unit media and giving its unit classes their default buttons back
EETechTreeNode::~EETechTreeNode()
{
    ee_gamecpp_tech_node_free(this, true);   // (gamecpp tech_load.rs)
}

// Frees the node's per-unit-class cost tables
void EETechTreeNode::ClearClassCosts()
{
    ee_gamecpp_tech_node_free(this, false);
}

// Deletes the node's data
long EETechTreeNode::Unload()
{
    return ee_gamecpp_tech_node_unload(this);
}

// Returns a node's cost in a resource for a unit class: per-class cost, scaled for unit-making nodes by wonder
// epoch, cost set and upgrade cost scale, and by the just-in-time manufacturing factor for rally-point classes
long EETechTreeNode::GetCost(long inResource, long inUnitClass, float inScale)
{
    long theCost = this->GetCostUncaptured(inResource, inUnitClass, inScale);
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EETechCaptureCost(this, 0, inResource, inUnitClass, inScale, theCost);
    return theCost;
}

long EETechTreeNode::GetCostUncaptured(long inResource, long inUnitClass, float inScale)
{
    return ee_gamecpp_tech_node_cost(this, inResource, inUnitClass, inScale);
}

// Builds the "Food: n Gold: n ..." cost text of the node for a unit class (wood shown as carbon when used; ee_sim
// tech node_media.rs)
void EETechTreeNode::GetCostString(UWideString& outString, long inUnitClass, float inScale)
{
    char theText[512];
    ee_tech_cost_string(this, [](void* inNode, int32_t inResource, int32_t inClass, float inCostScale) {
        return (int32_t) static_cast<EETechTreeNode*>(inNode)->GetCost(inResource, inClass, inCostScale);
    }, (int32_t)inUnitClass, inScale, gWorld.mUsingCarbon, reinterpret_cast<uint8_t*>(theText), sizeof(theText));
    outString = theText;
}

// Reads the node's record from the tech tree file, with a cost table per unit class scaled by its factor
long EETechTreeNode::Load(FSFile& inFile, EETechTree* inTree)
{
    return ee_gamecpp_tech_node_load(this, &inFile, inTree);   // (gamecpp tech_load.rs)
}

// Tells whether the node can be researched now (wonders only after enough epochs or in the last epoch)
bool EETechTreeNode::GetIsAvailable()
{
    return EETechNodeWalk(this, EeTechNodeOp::IsAvailable) != 0;
}

// Tells whether the node lists a unit class
bool EETechTreeNode::Contains(long inUnitClass)
{
    std::vector<unsigned long>::iterator theUnitClass;
    theUnitClass = std::find(this->mData->mUnitClasses.begin(), this->mData->mUnitClasses.end(), inUnitClass);
    return theUnitClass != this->mData->mUnitClasses.end();
}

// Tells whether the node is obsolete, by flag or because its obsolete epoch has passed
bool EETechTreeNode::GetIsObsolete()
{
    return EETechNodeWalk(this, EeTechNodeOp::IsObsolete) != 0;
}

// Tells whether the node makes a building or mine whose media this game needs
bool EETechTreeNode::NeedsUnitMedia()
{
    return ee_gamecpp_tech_needs_unit_media(this, true);   // (TNMT, TEST ONLY: gamecpp tech_load.rs)
}

bool EETechTreeNode::NeedsUnitMediaUncaptured()
{
    return ee_gamecpp_tech_needs_unit_media(this, false);
}

// Tells whether the node depends on a tech, directly or through its prerequisites (ee_tech depends_on)
bool EETechTreeNode::DependsOn(long inID)
{
    return EETechNodeWalk(this, EeTechNodeOp::DependsOn, inID) != 0;
}

// Loads the node's media: the unit class and its buttons, and the media its research event uses (ee_sim tech
// node_media.rs)
void EETechTreeNode::LoadMedia(EEPlayer* inPlayer)
{
    EETechNodeWalk(this, EeTechNodeOp::Media, 1, false, reinterpret_cast<EEComplexUnit*>(inPlayer));
}

// Satisfies one prerequisite (an epoch or one of the four techs); with all met the node becomes available
// (event 0x7d1, statistics series 0x16)
void EETechTreeNode::SetPrerequisite(long inID)
{
    EETechNodeWalk(this, EeTechNodeOp::SetPrerequisite, inID);   // (TNPR, TEST ONLY: ee_sim tech nodes_capture.rs)
}

// Makes the node available with every prerequisite met
void EETechTreeNode::MakeAvailable()
{
    EETechNodeWalk(this, EeTechNodeOp::MakeAvailable);
}

// Researches the node if available: fires its event; returns 2 when already researched (repeatable kind 3 excepted),
// 9 when not possible; counts down a research delay
long EETechTreeNode::ResearchNode(EEComplexUnit* inUnit)
{
    return EETechNodeWalk(this, EeTechNodeOp::Research, 0, false, inUnit);   // (TNRS, TEST ONLY: nodes_capture.rs)
}

// Sets the scenario editor's obsoleted flag; recursively obsoletes the nodes depending on it, or un-obsoletes
// its prerequisites
void EETechTreeNode::SetScenEditorObsoleted(bool inValue, bool inRecurse)
{
    EETechNodeWalk(this, EeTechNodeOp::SetEditorObsoleted, inValue ? 1 : 0, inRecurse);
}

// Makes the node obsolete (obsolete event, unloading its unit media unless kept) or clears the flag
void EETechTreeNode::SetObsolete(bool inValue, bool inKeepMedia)
{
    EETechNodeWalk(this, EeTechNodeOp::SetObsolete, inValue ? 1 : 0, inKeepMedia);
}

// Gives the node a new button and swaps it into its unit classes
void EETechTreeNode::SetButton(unsigned long inButton)
{
    ee_gamecpp_tech_node_set_button(this, inButton);
}

// Unloads the node's media: the unit class and its buttons, and the media its research event uses
void EETechTreeNode::UnloadMedia(EEPlayer* inPlayer)
{
    EETechNodeWalk(this, EeTechNodeOp::Media, 0, false, reinterpret_cast<EEComplexUnit*>(inPlayer));
}

// Copies another node's state and shares its data (with its class cost tables)
EETechTreeNode& EETechTreeNode::operator=(const EETechTreeNode& inOther)
{
    return *(EETechTreeNode*)ee_gamecpp_tech_node_assign(this, &inOther);   // (gamecpp tech_load.rs)
}

// Returns the tech node's hot key, or NULL (an inline COMDAT the original emitted in EEUIFMGMainGroup.cpp)
UIHotKey* EETechTreeNode::GetHotKey()
{
    return (UIHotKey*)ee_gamecpp_db_hot_key(this->mData->mHotKey);
}

// ------------------------------------------------------------------------- round 9, lane D: the nodes' and epochs' C++

double EECore9OpEF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define TNODE ((EETechTreeNode*)p)
#define TEPOCH ((EETechTreeEpoch*)p)
#define TVARIANT EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)

double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TkEpochCostFactor: return gWorldTable.GetFloatValue(kWorldUnitEpochCostFactor);
    case O::TkCostSetFactor: return TVARIANT->GetCostSetFactor();
    case O::TkStatsCostScale: return ((EEUnitStats*)p)->GetCostScale();
    case O::TkJitFactor: return gWorld.mGameOptions.mResources == 4 ? gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactorDeathmatch) : gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactor);
    default: return EECore9OpEF(inOp, p, a, f);
    }
}
