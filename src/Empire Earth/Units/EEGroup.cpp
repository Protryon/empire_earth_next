// C:\EEX\Empire Earth\Units\EEGroup.cpp (file name inferred: no __FILE__). A group of units: 0x205eda..0x20834c.
// port (the groups lane, round 1): the groups are Rust objects (ee_units group_obj.rs: EEGroup, EESavedGroup,
// EESelectedGroup, EEActiveGroup with their path requests and clusters; ee/EEPlayer.h names them). This unit is the
// C++ their bodies call (ee_units_group_host: the reference counts, the formation, the path and continent managers,
// the user interface, the map's outlines, the saved game's path requests), EEGroup::GetFlags and the active groups'
// saved-game entry points.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEFormation.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMap.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEDatabaseTables.h"
#include <math.h>
#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEMapDimensions.h"

static_assert(sizeof(GERectangle) == 4 * sizeof(float) && offsetof(U2DRectangle<long>, mBottom) == 0 && offsetof(U2DRectangle<long>, mTop) == 3 * sizeof(long), "GetBounds hands Rust the rectangle's four floats; AREA_OUTLINES reads rectangles as bottom, left, right, top");

// EEAIContinentManager::GetPathAreaIDAt (inline, its out-of-line COMDAT 0x208334 follows this unit's template
// block): the path area's id at inTile, 0 if none. The ternary is unsigned short on both arms.
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
unsigned short EEAIContinentManager::GetPathAreaIDAt(const EETileLocation& inTile)
{
    EEAIPathArea* theArea = this->GetPathAreaAt(inTile);
    return theArea != NULL ? theArea->GetID() : (unsigned short)0;
}

// Returns the terrain flags every unit in the group shares (ee_units group_obj.rs flags)
EECPASGFlags EEGroup::GetFlags()
{
    EECPASGFlags theResult;
    ee_units_group_flags(this, reinterpret_cast<uint8_t(*)[17]>(theResult.mFlags));
    return theResult;
}

// The active groups in a saved game (save 0.279, EESaveHasActiveGroups; ee_units group_obj.rs save_all, load_all)
void EESaveActiveGroups(FSFile& outFile) { ee_units_groups_save_active(&outFile, &EESaveFilePrimitive); }

long EELoadActiveGroups(FSFile& inFile) { return ee_units_groups_load_active(&inFile, &EESaveFilePrimitive); }

// The groups' host (ee_units group_host.rs; EE_UNITS_GROUP_OP_*): each operation one C++ statement of the original
// bodies
extern "C" int64_t ee_units_group_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsGroupIo* io)
{
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inA));
    EEFormation* theFormation = reinterpret_cast<EEFormation*>(static_cast<uintptr_t>(inA));
    EEPlayer* thePlayer = reinterpret_cast<EEPlayer*>(static_cast<uintptr_t>(inA));
    EEUnit* theTarget = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inB));
    switch (inOp) {
    case EE_UNITS_GROUP_OP_UNIT_REFERENCE: theUnit->Reference();
        return 0;
    case EE_UNITS_GROUP_OP_UNIT_DEREFERENCE: theUnit->Dereference();
        return 0;
    case EE_UNITS_GROUP_OP_WORLD_UNIT: return (int64_t)(uintptr_t)gWorld.GetUnit((long)inC);
    case EE_UNITS_GROUP_OP_FORMATION_NEW: return (int64_t)(uintptr_t)EEFormation::Create((long)inC);
    case EE_UNITS_GROUP_OP_FORMATION_REFERENCE: theFormation->Reference();
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_DEREFERENCE: theFormation->Dereference();
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_KIND: return theFormation->mType;
    case EE_UNITS_GROUP_OP_FORMATION_CENTER: memcpy(io->p, &theFormation->mCenter, sizeof io->p);
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_SET_CENTER: memcpy(&theFormation->mCenter, io->p, sizeof io->p);
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_PLACE: {
        EEComplexUnit* const* theList = static_cast<EEComplexUnit* const*>(io->list);
        std::vector<EEComplexUnit*> theUnits(theList, theList + io->len);
        std::vector<EEFormationSlot> theSlots;
        theFormation->Place(theUnits, theSlots);
        for (size_t i = 0; i < theSlots.size(); i++)
            ee_units_group_push(io->out, 0, 0, reinterpret_cast<const float(*)[3]>(&theSlots[i].mPosition.mX));
        return 0;
    }
    case EE_UNITS_GROUP_OP_CONTINENT_OF: return gContinentManager.GetContinentID(theUnit);
    case EE_UNITS_GROUP_OP_CONTINENT_AT: return gContinentManager.GetContinent(EETileCoordinate((long)inC, (long)inD), kAIPathNonSpace);
    case EE_UNITS_GROUP_OP_SUBSECTOR_X: return gContinentManager.GetPathSubSectorAt((long)inC, (long)inD)->GetLocation().GetX();
    case EE_UNITS_GROUP_OP_SUBSECTOR_Y: return gContinentManager.GetPathSubSectorAt((long)inC, (long)inD)->GetLocation().GetY();
    case EE_UNITS_GROUP_OP_MAP_WIDTH: return (int64_t)gMapWidth;
    case EE_UNITS_GROUP_OP_MAP_HEIGHT: return (int64_t)gMapHeight;
    case EE_UNITS_GROUP_OP_FORMATION_ORIENTATION: memcpy(const_cast<void*>(io->list), &theFormation->mOrientation, 16 * sizeof(float));
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_SET_ORIENTATION: memcpy(&theFormation->mOrientation, io->list, 16 * sizeof(float));
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_HEADING: io->f = theFormation->mHeading;
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_SET_HEADING: theFormation->mHeading = io->f;
        return 0;
    case EE_UNITS_GROUP_OP_FORMATION_B5C: return theFormation->mB5C;
    case EE_UNITS_GROUP_OP_FORMATION_SET_B5C: theFormation->mB5C = inC != 0;
        return 0;
    case EE_UNITS_GROUP_OP_INTERFACE: return gUserInterface != NULL;
    case EE_UNITS_GROUP_OP_POST_SAVED_GROUP_DO: gUserInterface->PostRefreshSavedGroup((long)inC);
        return 0;
    case EE_UNITS_GROUP_OP_PATH_AREA_AT: return gContinentManager.GetPathAreaIDAt((long)inC, (long)inD);
    // the path requests (their request, cancel, references, fields, save, load and the loaded ones' waits) and
    // the terrain flags: Rust (group_host.rs over ee_sim::astar request.rs)
    // (SetGroupPathGoals: group_host.rs over waypoints.rs)
    case EE_UNITS_GROUP_OP_CLEAR_BUILD_TARGETS: theUnit->ClearBuildTargets();
        return 0;
    case EE_UNITS_GROUP_OP_TIME: return (int64_t)(long)TMGetTime();
    case EE_UNITS_GROUP_OP_POST_SELECTION: gUserInterface->PostMessage424Once();
        return 0;
    case EE_UNITS_GROUP_OP_PLAYER_GROUP: return inC >= 0 && inC < 20 ? (int64_t)(uintptr_t)thePlayer->mGroups[inC] : 0;
    // the selection's (selection.rs)
    case EE_UNITS_GROUP_OP_PLAYER_UNIT: return (int64_t)(uintptr_t)thePlayer->GetUnit((long)inC);
    case EE_UNITS_GROUP_OP_SELECTABLE: return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inB))->mUnitClass->IsSelectable(reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inB)));
    case EE_UNITS_GROUP_OP_DOPPELGANGER: return gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theTarget->mTile), theTarget, thePlayer->GetPlayerID());
    case EE_UNITS_GROUP_OP_LOCAL_SELECTION: return gWorld.mExecutingLocalSelection;
    case EE_UNITS_GROUP_OP_PLAY_SOUND: EEPlayForLocalPlayer(thePlayer, inC == 0 ? theTarget->mStats->GetSelectionSound(-1) : theTarget->mStats->GetAcknowledgeSound(-1));
        return 0;
    case EE_UNITS_GROUP_OP_ACK_DELAY: return (int64_t)(unsigned long)gWorldTable.GetLongValue(kWorldAcknowledgementDelay);
    case EE_UNITS_GROUP_OP_SET_VIEWED_UNIT: thePlayer->mViewedUnit = reinterpret_cast<RTSUnit*>(static_cast<uintptr_t>(inB));
        return 0;
    // RefreshUpgrades' (selection.rs)
    case EE_UNITS_GROUP_OP_CLASS_COUNT: return (int64_t)EEUnitClassCount();
    case EE_UNITS_GROUP_OP_UPGRADE: return (int64_t)(uintptr_t)thePlayer->GetUpgrade((long)inC);
    case EE_UNITS_GROUP_OP_UPGRADE_INIT: reinterpret_cast<EEUnitClassUpgrade*>(static_cast<uintptr_t>(inA))->Init();
        return 0;
    case EE_UNITS_GROUP_OP_SET_HIT_POINTS: reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inA))->SetHitPoints((long)inC);
        return 0;
    case EE_UNITS_GROUP_OP_SET_FLIGHT_TIME: theUnit->SetFlightTime((long)inC);
        return 0;
    case EE_UNITS_GROUP_OP_MAKE_AREA_EFFECTS: theUnit->MakeAreaEffects();
        return 0;
    case EE_UNITS_GROUP_OP_CIVILIZATION_UPGRADES: thePlayer->mCivilization.AddUpgrades(thePlayer->mCivilization.GetPurchasedUpgrades(), thePlayer->GetPlayerID(), false);
        return 0;
    case EE_UNITS_GROUP_OP_PLAYER_SELECTION: return (int64_t)(uintptr_t)(inC == 0 ? thePlayer->mActiveGroup : thePlayer->mSelectedGroup);
    case EE_UNITS_GROUP_OP_CLEAR_AREAS: gWorld.mMap->ClearU1955DC();
        return 0;
    case EE_UNITS_GROUP_OP_AREA_OUTLINES: {
        std::vector<U2DRectangle<long> > theRectangles(static_cast<const U2DRectangle<long>*>(io->list), static_cast<const U2DRectangle<long>*>(io->list) + io->len);
        std::vector<unsigned short> theContinents(static_cast<const unsigned short*>(io->list2), static_cast<const unsigned short*>(io->list2) + io->len);
        gWorld.mMap->BuildAreaOutlines(theRectangles, theContinents);
        return 0;
    }
    case EE_UNITS_GROUP_OP_AREA_EFFECT_RADIUS: io->f = gAEManager.mTemplates[inC]->mRadius;
        return 0;
    // at the first tick (the units are back), before the graph searches are resolved (EEAfterLoadLast)
    case EE_UNITS_GROUP_OP_AFTER_LOAD: EEAfterLoad([inA]() { ee_units_group_restore_saved(reinterpret_cast<void*>(static_cast<uintptr_t>(inA))); });
        return 0;
    }
    return 0;
}
