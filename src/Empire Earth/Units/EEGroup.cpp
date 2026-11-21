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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/GERectangle.h"
#include "lle/FSFile.h"

#include "ee/EEUnitKind.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEFormation.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMap.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAIKindIDs.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include <math.h>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEMapDimensions.h"
#include "rust/sim/units.h"

static_assert(sizeof(GERectangle) == 4 * sizeof(float), "GetBounds hands Rust the rectangle's four floats");

// EEAIContinentManager::GetPathAreaIDAt (inline, its out-of-line COMDAT 0x208334 follows this unit's template
// block): the path area's id at inTile, 0 if none. The ternary is unsigned short on both arms.
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
unsigned short EEAIContinentManager::GetPathAreaIDAt(const EETileLocation& inTile)
{
    EEAIPathArea* theArea = this->GetPathAreaAt(inTile);
    return theArea != NULL ? theArea->GetID() : (unsigned short)0;
}

// The (x, y) overload (the group path's step: UMFloor values)
inline unsigned short EEAIContinentManager::GetPathAreaIDAt(long inX, long inY)
{
    return this->GetPathAreaIDAt(EETileLocation(inX, inY));
}

// Returns the terrain flags every unit in the group shares (ee_units group_obj.rs flags)
EECPASGFlags EEGroup::GetFlags()
{
    EECPASGFlags theResult;
    ee_units_group_flags(this, reinterpret_cast<uint8_t(*)[17]>(theResult.mFlags));
    return theResult;
}

// The active groups in a saved game (save 0.279, EESaveHasActiveGroups; ee_units group_obj.rs save_all, load_all)
void EESaveActiveGroups(FSFile& outFile)
{
    ee_units_groups_save_active(&outFile, &EESaveFilePrimitive);
}

long EELoadActiveGroups(FSFile& inFile)
{
    return ee_units_groups_load_active(&inFile, &EESaveFilePrimitive);
}

// The groups' host (ee_units group_host.rs; EE_UNITS_GROUP_OP_*): each operation one C++ statement of the original
// bodies
extern "C" int64_t ee_units_group_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, EeUnitsGroupIo* io)
{
    EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inA));
    EEFormation* theFormation = reinterpret_cast<EEFormation*>(static_cast<uintptr_t>(inA));
    EEAutoPathRequest* thePath = reinterpret_cast<EEAutoPathRequest*>(static_cast<uintptr_t>(inA));
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
    case EE_UNITS_GROUP_OP_TERRAIN_FLAGS: {
        EECPASGFlags theFlags;
        theUnit->mUnitClass->GetTerrainFlags(theUnit, theFlags);
        memcpy(io->flags, theFlags.mFlags, 17);
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
    case EE_UNITS_GROUP_OP_PATH_REQUEST: {
        EETileCoordinate theFrom;
        EETileCoordinate theTo;
        theFrom.mX = (long)io->a;
        theFrom.mY = (long)io->b;
        theTo.mX = (long)io->c;
        theTo.mY = (long)io->d;
        EECPASGFlags theFlags;
        memcpy(theFlags.mFlags, io->flags, 17);
        return (int64_t)(uintptr_t)gPathManager.RequestPath((long)inC, theFrom, theTo, &theFlags, 1, -1, NULL, 0);
    }
    case EE_UNITS_GROUP_OP_PATH_CANCEL: gPathManager.CancelAutoPathRequest(thePath);
        return 0;
    case EE_UNITS_GROUP_OP_PATH_REFERENCE: thePath->Reference();
        return 0;
    case EE_UNITS_GROUP_OP_PATH_DEREFERENCE: thePath->Dereference();
        return 0;
    case EE_UNITS_GROUP_OP_PATH_DONE: return thePath->mIsDone;
    case EE_UNITS_GROUP_OP_PATH_ID: return thePath->mRequestID;
    case EE_UNITS_GROUP_OP_PATH_TILES:
        for (size_t i = 0; i < thePath->mPath.size(); i++)
            ee_units_group_push(io->out, thePath->mPath[i].GetX(), thePath->mPath[i].GetY(), NULL);
        return 0;
    case EE_UNITS_GROUP_OP_SET_GROUP_PATH: {
        const float* thePoints = static_cast<const float*>(io->list);
        std::vector<GE3DPoint> thePoints3;
        for (size_t i = 0; i < io->len; i++)
            thePoints3.push_back(GE3DPoint(thePoints[3 * i], thePoints[3 * i + 1], thePoints[3 * i + 2]));
        theUnit->SetGroupPathGoals(thePoints3);
        return 0;
    }
    case EE_UNITS_GROUP_OP_CLEAR_BUILD_TARGETS: theUnit->ClearBuildTargets();
        return 0;
    case EE_UNITS_GROUP_OP_TIME: return (int64_t)(long)TMGetTime();
    case EE_UNITS_GROUP_OP_POST_SELECTION: gUserInterface->PostMessage424Once();
        return 0;
    case EE_UNITS_GROUP_OP_PLAYER_GROUP: return inC >= 0 && inC < 20 ? (int64_t)(uintptr_t)thePlayer->mGroups[inC] : 0;
    // the selection's (selection.rs)
    case EE_UNITS_GROUP_OP_PLAYER_UNIT: return (int64_t)(uintptr_t)thePlayer->GetUnit((long)inC);
    case EE_UNITS_GROUP_OP_SELECTABLE:
        return reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inB))->mUnitClass->IsSelectable(reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(inB)));
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
    case EE_UNITS_GROUP_OP_PLAYER_SELECTION:
        return (int64_t)(uintptr_t)(inC == 0 ? thePlayer->mActiveGroup : thePlayer->mSelectedGroup);
    case EE_UNITS_GROUP_OP_CLEAR_AREAS: gWorld.mMap->ClearU1955DC();
        return 0;
    case EE_UNITS_GROUP_OP_AREA_OUTLINES: {
        const int64_t* theAreas = static_cast<const int64_t*>(io->list);
        const unsigned short* theContinentIDs = static_cast<const unsigned short*>(io->list2);
        std::vector<U2DRectangle<long> > theRectangles;
        std::vector<unsigned short> theContinents;
        for (size_t i = 0; i < io->len; i++) {
            U2DRectangle<long> theArea;
            theArea.mLeft = (long)theAreas[4 * i];
            theArea.mRight = (long)theAreas[4 * i + 1];
            theArea.mTop = (long)theAreas[4 * i + 2];
            theArea.mBottom = (long)theAreas[4 * i + 3];
            theRectangles.push_back(theArea);
            theContinents.push_back(theContinentIDs[i]);
        }
        gWorld.mMap->BuildAreaOutlines(theRectangles, theContinents);
        return 0;
    }
    case EE_UNITS_GROUP_OP_AREA_EFFECT_RADIUS: io->f = gAEManager.mTemplates[inC]->mRadius;
        return 0;
    case EE_UNITS_GROUP_OP_SAVE_PATH:
        EESaveAutoPathRequest(*reinterpret_cast<FSFile*>(static_cast<uintptr_t>(inA)),
                              *reinterpret_cast<EEAutoPathRequest*>(static_cast<uintptr_t>(inB)));
        return 0;
    case EE_UNITS_GROUP_OP_LOAD_PATH: {
        EEAutoPathRequest* theRequest = NULL;
        long theResult = EELoadAutoPathRequest(*reinterpret_cast<FSFile*>(static_cast<uintptr_t>(inA)), theRequest);
        io->a = (int64_t)(uintptr_t)theRequest;
        return theResult;
    }
    case EE_UNITS_GROUP_OP_AWAIT_PATH: {
        // the group referenced for the wait (group_obj.rs restore_active); the attach releases it
        EeUnitsGroup* theGroup = reinterpret_cast<EeUnitsGroup*>(static_cast<uintptr_t>(inB));
        int64_t theKey = inD;
        EEAwaitAutoPathRequest((long)inC, [theGroup, theKey](EEAutoPathRequest* inRequest) {
            ee_units_group_path_resolved(theGroup, theKey, inRequest);
        });
        return 0;
    }
    case EE_UNITS_GROUP_OP_AFTER_LOAD: {
        // at the first tick (the units are back), before the graph searches are resolved (EEAfterLoadLast)
        void* theSaved = reinterpret_cast<void*>(static_cast<uintptr_t>(inA));
        EEAfterLoad([theSaved]() { ee_units_group_restore_saved(theSaved); });
        return 0;
    }
    }
    return 0;
}
