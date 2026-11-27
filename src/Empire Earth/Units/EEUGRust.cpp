// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 and 17): the goals whose class moved whole into Rust
// (ee/EEUGRust.h). Each virtual is one call of the Rust body (ee_units rust_goal.rs, by mKind) over the host below:
// one operation callback whose cases are the reads and world calls the bodies make (the units, the patrol group's
// walk, the calamity classes and manager, the explore goal's map, path and continent queries, the transport
// classes, the garrisons, the typed file calls, the texts). The base goal's own fields, save and checksum stay
// EEUnitGoal's.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUGRust.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEEvent.h"
#include "ee/EmpireEarth.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EERules.h"
#include "ee/EEAIForest.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEStringIDs.h"
#include "lle/SESoundManager.h"
#include "lle/UResourceFile.h"

// the location goal's host hands the unit's transformation to ee_ge's GeTransformation (the same 4x4 floats)
static_assert(sizeof(GETransformation) == 64, "GETransformation is ee_ge's GeTransformation");

long gSpaceUnloadSoundID = 498;             // 0x4c8fa8 (.data = 0x1f2; the teleporter's unload sound)

// ---- the gather goal's world work (EEUGGather.cpp's helpers; the goal itself is gather.rs)

// EEAIContinentManager's inline forest id (ee/EEAIContinentManager.h declares it; EEAIForest is complete here). Its
// out-of-line COMDAT is 0x625272 (GatherNextTree's first call; the second is expanded)
inline unsigned short EEAIContinentManager::GetForestIDAt(const EETileLocation& inTile)
{
    EEAIForest* theForest = this->GetForestAt(inTile);
    return theForest != NULL ? theForest->GetID() : 0;
}

namespace {

// 0x622ff0 GatherNextTree's world part: a carried load of another type dropped; a tree with no free spot gives way to
// the first gatherable tree of its forest on the line from a nearby drop site (or the gatherer) to it (the result,
// NULL for none); another resource but an animal becomes the gathered one
EEUnit* EEGatherNextTree(EEComplexUnit* inUnit, EEUnit* inResource)
{
    return (EEUnit*)(uintptr_t)ee_units_gather_next_tree((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inResource);   // uu_gather_tree.rs
}

}  // namespace

namespace {

// where a body's file calls and text go: rust_goal.rs EeUnitsGoalContext (the file calls are Rust's, goal_obj.rs
// file_call; the text is a UWideString*, the dump a FILE*)
static_assert(sizeof(EeUnitsGoalContext) == 3 * sizeof(void*), "EeUnitsGoalContext: file, text, dump");

UWideString& TextOf(void* inContext) { return *static_cast<UWideString*>(static_cast<EeUnitsGoalContext*>(inContext)->text); }

uint64_t Handle(const void* inObject) { return reinterpret_cast<uintptr_t>(inObject); }

template <class T> T* ObjectOf(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

// (a GE3DPoint is its three floats)
void PointOut(EeUnitsGoalIo* ioArgs, const GE3DPoint& inPoint) { memcpy(ioArgs->f, &inPoint, 3 * sizeof(float)); }

// the tiles a split operation hands in place (goal_split.rs: `io.text` names its buffer of U2DPoint<long>)
static_assert(sizeof(U2DPoint<long>) == 2 * sizeof(long), "a tile is two longs (goal_split.rs's c_long pairs)");
U2DPoint<long>* TilesAt(EeUnitsGoalIo* ioArgs) { return reinterpret_cast<U2DPoint<long>*>(const_cast<uint8_t*>(ioArgs->text)); }

// the statements' shared locals (goal_more.rs: each cleared or set where the original declared its local)
static GE3DPoint sGoalPoint;
static GE3DPoint sGoalCastLocation;
static std::vector<EEUnit*> sGoalUnits;
static std::vector<EEComplexUnit*> sGoalComplexes;
static std::vector<EETileCoordinate> sGoalTiles;
static EEEffectHandle sGoalEffect;
static EEUnit* sGoalBlocker;
static long sGoalBlockTile[2];   // x, y

void LongPairOut(EeUnitsGoalIo* ioArgs, const U2DPoint<long>& inPoint)
{
    ioArgs->i[0] = inPoint.mX;
    ioArgs->i[1] = inPoint.mY;
}

// FOUNDATION_STARTED's notice of another player's wonder (its chat line)
void WonderStartedChat(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    UWideString theFormat;
    UWideString theName;
    UWideString theText;
    gResource->GetString(IDS_HAS_STARTED_CONSTRUCTING_WONDER, theFormat);
    gResource->GetString(inTarget->mUnitClass->mNameStringID, theName);
    sprintf(theText, (const char*)theFormat, (const char*)inUnit->mPlayer->mName, (const char*)theName);
    gUserInterface->AddChatLogEntry(theText, 1);
}

// one operation (EE_UNITS_GOAL_HOST_*) of a body: inA, inB the handles it names, ioArgs the rest
// (the plain reads of a unit, its class, stats and player are Rust's views: rust_goal.rs read_unit; the compound
// operations are split into these statements in Rust, in the original's order: goal_split.rs)
int64_t GoalOp(void* inContext, int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsGoalIo* ioArgs)
{
    GE3DPoint thePoint(ioArgs->f[0], ioArgs->f[1], ioArgs->f[2]);
    switch (inOp) {
    case EE_UNITS_GOAL_HOST_REFERENCE:   // (a patrol group's: rust_goal.rs, a Rust object)
        ObjectOf<EEUnit>(inA)->Reference();
        return 0;
    case EE_UNITS_GOAL_HOST_DEREFERENCE: ObjectOf<EEUnit>(inA)->Dereference(); return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_COMPLEX: return Handle(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_X_FIND_PLAIN: return Handle(static_cast<EEUnit*>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_X_TELL_GROUP: ObjectOf<EEComplexUnit>(inA)->mActiveGroup->TellGroup(ObjectOf<EEComplexUnit>(inA), thePoint); return 0;
    // (the file calls: Rust's, goal_obj.rs file_call)
    case EE_UNITS_GOAL_HOST_X_HAS_TEXT: return static_cast<EeUnitsGoalContext*>(inContext)->text != NULL;
    case EE_UNITS_GOAL_HOST_X_APPEND_TEXT: TextOf(inContext) += std::string((const char*)ioArgs->text, ioArgs->len).c_str(); return 0;
    case EE_UNITS_GOAL_HOST_X_APPEND_DUMP: fputs(std::string((const char*)ioArgs->text, ioArgs->len).c_str(), static_cast<FILE*>(static_cast<EeUnitsGoalContext*>(inContext)->dump)); return 0;
    case EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME: TextOf(inContext) += (const char*)ObjectOf<EEUnit>(inA)->mUnitClass->mName; return 0;

    // the patrol's (the groups' own calls are Rust's: rust_goal.rs)
    case EE_UNITS_GOAL_HOST_PLAYER_GROUP: return Handle(ObjectOf<EEComplexUnit>(inA)->GetPlayer()->GetPatrolGroup((unsigned long)inB));

    // the cast calamity's
    case EE_UNITS_GOAL_HOST_X_PLAYER_TECH: return Handle(ObjectOf<EEComplexUnit>(inA)->mPlayer->mTechnologies->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_NODE_CALAMITY: return ObjectOf<EETechTreeNode>(inA)->mData->mCalamity;
    case EE_UNITS_GOAL_HOST_X_CALAMITY_CLASS_OF: return Handle(EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_CALAMITY_PLACE: ObjectOf<EECalamityClass>(inA)->Place(ObjectOf<EEComplexUnit>(inB), (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1]); return 0;
    case EE_UNITS_GOAL_HOST_X_CALAMITY_TARGET_KIND: return ObjectOf<EECalamityClass>(inA)->GetTargetKind();
    case EE_UNITS_GOAL_HOST_X_CALAMITY_COST: return ObjectOf<EECalamityClass>(inA)->GetCost();
    case EE_UNITS_GOAL_HOST_X_CALAMITY_RANGE: return ObjectOf<EECalamityClass>(inA)->mRange;
    case EE_UNITS_GOAL_HOST_X_CREATE_UNIT_CALAMITY: return EECalamityManager::GetInstance()->CreateUnitCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1], (unsigned long)ioArgs->i[2], (unsigned long)ioArgs->i[3]);
    case EE_UNITS_GOAL_HOST_X_CREATE_LOCATION_CALAMITY: return EECalamityManager::GetInstance()->CreateLocationCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1], (long)ioArgs->i[2], (long)ioArgs->i[3], (unsigned long)ioArgs->i[4]);
    case EE_UNITS_GOAL_HOST_X_CREATE_GLOBAL_CALAMITY: return EECalamityManager::GetInstance()->CreateGlobalCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1], (unsigned long)ioArgs->i[3]);
    case EE_UNITS_GOAL_HOST_RESET_SAVED_GROUP: ObjectOf<EEComplexUnit>(inA)->mPlayer->mSavedGroups.Reset(6, 1); return 0;
    case EE_UNITS_GOAL_HOST_CAN_CAST_AT: return ObjectOf<EECalamityClass>(inA)->EECalamityClass::CanCastAt(ObjectOf<EEComplexUnit>(inB), sGoalCastLocation, (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1], true, NULL) == 0;
    case EE_UNITS_GOAL_HOST_FACES_TARGET: return ObjectOf<EEComplexUnit>(inA)->GetUnitClass()->FacesTarget(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), false);
    case EE_UNITS_GOAL_HOST_FACES_POINT: return ObjectOf<EEComplexUnit>(inA)->GetUnitClass()->FacesPoint(ObjectOf<EEComplexUnit>(inA), thePoint, false);
    case EE_UNITS_GOAL_HOST_X_IN_CONTACT_RANGE: return EEIsInContactRange(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), ioArgs->f[0]);
    // the explore goal's
    case EE_UNITS_GOAL_HOST_X_PATH_AREA_AT: return Handle(gContinentManager.GetPathAreaAt(ObjectOf<EEComplexUnit>(inA)));
    case EE_UNITS_GOAL_HOST_X_AREA_TOP: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mTop;
    case EE_UNITS_GOAL_HOST_X_AREA_BOTTOM: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mBottom;
    case EE_UNITS_GOAL_HOST_X_AREA_LEFT: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mLeft;
    case EE_UNITS_GOAL_HOST_X_AREA_RIGHT: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mRight;
    case EE_UNITS_GOAL_HOST_X_MAP_TILE: return Handle(::gWorld.mMap->GetTile((long)ioArgs->i[0], (long)ioArgs->i[1]));
    case EE_UNITS_GOAL_HOST_X_EDGE_TILE: return ::gWorld.mMap->IsEdgeTile((long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TILE_VISIBLE: return ObjectOf<EEMapTile>(inB)->IsVisibleTo(ObjectOf<EEComplexUnit>(inA)->mPlayer);
    case EE_UNITS_GOAL_HOST_X_SPACE_MAP: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_GOAL_HOST_X_CAN_MOVE_ON_TERRAIN: return ObjectOf<EEComplexUnit>(inA)->mUnitClass->CanMoveOnTerrain(ObjectOf<EEComplexUnit>(inA), (long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_X_MAP_WIDTH: return gMapWidth;
    case EE_UNITS_GOAL_HOST_X_MAP_HEIGHT: return gMapHeight;
    case EE_UNITS_GOAL_HOST_X_MAP_L195614: return ::gWorld.mMap->mL195614;
    case EE_UNITS_GOAL_HOST_X_MAP_L195610: return ::gWorld.mMap->mL195610;
    case EE_UNITS_GOAL_HOST_X_CONTINENT_AT: return Handle(static_cast<EEAIArea*>(gContinentManager.GetContinentAt(ObjectOf<EEComplexUnit>(inA))));
    case EE_UNITS_GOAL_HOST_X_RANDOM_CONTINENT: return Handle(static_cast<EEAIArea*>(static_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom))));
    case EE_UNITS_GOAL_HOST_X_AREA_RANDOM_POINT: LongPairOut(ioArgs, ObjectOf<EEAIArea>(inA)->GetRandomPoint()); return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_NEARBY_TARGET: return Handle(EEFindNearbyTarget(ObjectOf<EEComplexUnit>(inA)));
    case EE_UNITS_GOAL_HOST_X_NOTICE_FOREIGN_UNIT: ObjectOf<EEComplexUnit>(inA)->mPlayer->NoticeForeignUnit(ObjectOf<EEUnit>(inB)); return 0;
    // the unload transport's
    case EE_UNITS_GOAL_HOST_X_IS_TRANSPORT: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass()) != NULL;
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_UNLOAD_AT: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->CanUnloadAt(ObjectOf<EEComplexUnit>(inA), (long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TILE_FREE: return EEIsTileFree(ObjectOf<EEComplexUnit>(inA), (long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_UNLOAD_TILES: EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->FindUnloadTiles(ObjectOf<EEComplexUnit>(inA), &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2], &TilesAt(ioArgs)[3]); return 0;
    case EE_UNITS_GOAL_HOST_X_AIRPLANE_UNLOAD_TILES: EEUnitClassOfKind<EE_CLASS_KIND_AIRPLANE>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->FindUnloadTiles(ObjectOf<EEComplexUnit>(inA), &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2], &TilesAt(ioArgs)[3]); return 0;
    case EE_UNITS_GOAL_HOST_X_CAN_UNLOAD_NEAR: return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->CanUnloadNear(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEComplexUnit>(inA)->GetCargo(0), &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2]);
    case EE_UNITS_GOAL_HOST_UNLOAD_AT: EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->UnloadAt(ObjectOf<EEComplexUnit>(inA), (long)ioArgs->i[0], (long)ioArgs->i[1]); return 0;
    case EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND: return gSpaceUnloadSoundID;
    case EE_UNITS_GOAL_HOST_X_SOUND_GET: return Handle(EESoundObject::Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS:
        ObjectOf<EEComplexUnit>(inA)->mUnitClass->GetSoundParameters(ObjectOf<EEComplexUnit>(inA), ObjectOf<EESoundObject>(inB), &ioArgs->f[0], &ioArgs->f[1], &ioArgs->f[2]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SOUND_PLAY: ObjectOf<EESoundObject>(inA)->Play(ioArgs->f[0], ioArgs->f[1], ioArgs->f[2]); return 0;
    case EE_UNITS_GOAL_HOST_X_SOUND_PLAY_PLAIN: ObjectOf<EESoundObject>(inA)->Play(); return 0;

    // the load goals' (the unit and class reads give zeros for no unit; the class's are Rust's views)
    case EE_UNITS_GOAL_HOST_X_NO_WAYPOINTS: return ObjectOf<EEComplexUnit>(inA)->mWaypoints.empty();
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_LOAD:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->CanLoad(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEComplexUnit>(inB));
    // goal_more.rs's (BOARDING, the rendezvous units, SURROUNDING_TILES, NEXT_RESOURCE, RESOURCE_OF_KIND, MINING_BEAM)
    case EE_UNITS_GOAL_HOST_X_SET_GOAL_POINT: sGoalPoint = thePoint; return 0;
    case EE_UNITS_GOAL_HOST_X_GOAL_POINT_OUT: PointOut(ioArgs, sGoalPoint); return 0;
    case EE_UNITS_GOAL_HOST_X_BOARDING_FIND: EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inB)->GetUnitClass())->FindBoardingLocation(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEComplexUnit>(inB), thePoint, sGoalPoint); return 0;
    case EE_UNITS_GOAL_HOST_X_UNITS_CLEAR: sGoalUnits.clear(); return 0;
    case EE_UNITS_GOAL_HOST_X_UNITS_PUSH: sGoalUnits.push_back(ObjectOf<EEUnit>(inB)); return 0;
    case EE_UNITS_GOAL_HOST_X_COMPLEXES_CLEAR: sGoalComplexes.clear(); return 0;
    case EE_UNITS_GOAL_HOST_X_COMPLEXES_PUSH: sGoalComplexes.push_back(ObjectOf<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_GOAL_HOST_X_NEXT_RESOURCE: return Handle(ObjectOf<EEComplexUnit>(inA)->mUnitClass->FindNextResource(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), (long)ioArgs->i[0], &sGoalUnits));
    case EE_UNITS_GOAL_HOST_X_RESOURCE_OF_KIND: return Handle(ObjectOf<EEComplexUnit>(inA)->mUnitClass->FindResourceOfKind(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), ObjectOf<EEUnit>((uint64_t)ioArgs->i[3]), (long)ioArgs->i[0], (unsigned long)ioArgs->i[1], &sGoalUnits, ioArgs->i[2] != 0));
    case EE_UNITS_GOAL_HOST_X_TILES_CLEAR: sGoalTiles.clear(); return 0;
    case EE_UNITS_GOAL_HOST_X_SURROUNDING_FILL: ObjectOf<EEComplexUnit>(inA)->mUnitClass->GetSurroundingTiles(ObjectOf<EEComplexUnit>(inA), sGoalTiles, 1, 0); return 0;
    case EE_UNITS_GOAL_HOST_X_TILES_COUNT: return (int64_t)sGoalTiles.size();
    case EE_UNITS_GOAL_HOST_X_TILE_X: return sGoalTiles[(size_t)ioArgs->i[0]].GetX();
    case EE_UNITS_GOAL_HOST_X_TILE_Y: return sGoalTiles[(size_t)ioArgs->i[0]].GetY();
    case EE_UNITS_GOAL_HOST_X_MINING_BEAM_EFFECT: ::gWorld.GetEffectManager()->CreateEffect(ObjectOf<EEComplexUnit>(inA), sGoalPoint, EEGraphicEffect::Get(kGraphicEffectSpaceCitizenMiningBeam), &sGoalEffect, 0); return 0;
    case EE_UNITS_GOAL_HOST_X_SET_FIRE_EFFECT: ObjectOf<EEComplexUnit>(inA)->mFireEffect = sGoalEffect; return 0;
    case EE_UNITS_GOAL_HOST_X_RESET_EFFECT: sGoalEffect = EEEffectHandle(); return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_RENDEZVOUS: EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->FindRendezvousTiles(&sGoalComplexes, ObjectOf<EEComplexUnit>(inA), &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1]); return 0;
    case EE_UNITS_GOAL_HOST_X_TECH_TREE_GET: return Handle(ObjectOf<EEComplexUnit>(inA)->mPlayer->GetTechTree()->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS: return EEEvent::GetEventEffect(EEEvent::Get(ObjectOf<EETechTreeNode>(inA)->mData->mResearchedEvent)->GetEffect(0))->mNewUnitClass;
    case EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE: return ObjectOf<EETechTreeNode>(inA)->GetIsAvailable();
    case EE_UNITS_GOAL_HOST_X_REPLACE_UNIT: EEReplaceUnit(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEComplexUnit>(inA)->mPlayer->mPlayerID, ObjectOf<EEUnitClass>(inB)); return 0;
    case EE_UNITS_GOAL_HOST_GENERATE: ioArgs->f[0] = gPhysics->mRandom.Generate(); return 0;
    // the build/repair goal's
    // (EE_UNITS_GOAL_HOST_BUILD_UNIT's reads are Rust's views: rust_goal.rs build_unit_fields)
    case EE_UNITS_GOAL_HOST_X_NODE_ID: return ObjectOf<EETechTreeNode>(inA)->mData->mID;
    case EE_UNITS_GOAL_HOST_X_NODE_KIND: return ObjectOf<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME: return ObjectOf<EETechTreeNode>(inA)->GetBuildTime();
    case EE_UNITS_GOAL_HOST_IN_WORK_REACH: return EEIsInWorkReach(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS: ObjectOf<EEPlayer>(inA)->mTechnologies->Research(ObjectOf<EEComplexUnit>(inB)->mUnitClass->GetTechnology(), NULL); return 0;
    case EE_UNITS_GOAL_HOST_X_CHECK_BUTTONS: ObjectOf<EEPlayer>(inA)->CheckButtonsPreloaded((long)ioArgs->i[0]); return 0;
    case EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE: return ObjectOf<EEPlayer>(inA)->mTechnologies->Get((long)ioArgs->i[0])->GetIsAvailable();
    case EE_UNITS_GOAL_HOST_X_SPACE_WALL: EECreateSpaceWallEnergyWall(ObjectOf<EEComplexUnit>(inA)); return 0;
    case EE_UNITS_GOAL_HOST_X_PLAY_SAMPLE: gSoundManager->Play(ObjectOf<EESoundObject>(inA)->mSampleID, ObjectOf<EESoundObject>(inA)->mVolume, 0.0f, 1.0f, false, NULL); return 0;
    case EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM: return Handle(gUIManager->GetMainGameForm());
    case EE_UNITS_GOAL_HOST_X_REFRESH_GROUP: gUIManager->GetMainGameForm()->mRefreshGroup = true; return 0;
    case EE_UNITS_GOAL_HOST_X_SCALED_COST: return ObjectOf<EEComplexUnit>(inA)->GetPlayer()->ScaleCost(ObjectOf<EETechTreeNode>(inB)->GetCost((unsigned long)ioArgs->i[0], 0x20, 1.0f));
    case EE_UNITS_GOAL_HOST_X_ADD_RESOURCE: ObjectOf<EEComplexUnit>(inA)->GetPlayer()->AddResource((unsigned long)ioArgs->i[0], (long)ioArgs->i[1]); return 0;
    case EE_UNITS_GOAL_HOST_GATE_SIDE_PART: return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(ObjectOf<EEComplexUnit>(inA)->mUnitClass)->IsSidePart(ObjectOf<EEComplexUnit>(inA));
    case EE_UNITS_GOAL_HOST_X_ADD_EVENT: ObjectOf<EEPlayer>(inA)->mEvents.AddEvent(U2DPoint<long>(ObjectOf<EEUnit>(inB)->mTileX, ObjectOf<EEUnit>(inB)->mTileY), 2, ObjectOf<EEPlayer>(inA), (long)ioArgs->i[0], false); return 0;
    case EE_UNITS_GOAL_HOST_NEAREST_SITE: return Handle(ObjectOf<EEComplexUnit>(inA)->mPlayer->GetNearestConstructionSite(ObjectOf<EEComplexUnit>(inA)));
    case EE_UNITS_GOAL_HOST_CAN_PRODUCE: return ObjectOf<EEUnit>(inA)->mPlayer->CanProduce(ObjectOf<EEUnit>(inB)->mUnitClass->GetTechnology(), 0x20, 1, true, 3, NULL);
    case EE_UNITS_GOAL_HOST_CAN_AFFORD_REPAIR: return ObjectOf<EEComplexUnit>(inA)->mPlayer->CanAffordRepair(ObjectOf<EEUnit>(inB), ObjectOf<EEComplexUnit>(inA), (unsigned long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_PAY: ObjectOf<EEPlayer>(inA)->Pay(ObjectOf<EETechTreeNode>(inB), 0x20, ioArgs->f[0]); return 0;
    case EE_UNITS_GOAL_HOST_X_WONDER_CHAT: WonderStartedChat(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB)); return 0;
    case EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA: ObjectOf<EEComplexUnit>(inA)->mPlayer->LoadUnitClassMedia(ObjectOf<EEUnit>(inB)->mUnitClass->GetUnitType()); return 0;
    case EE_UNITS_GOAL_HOST_X_LOAD_MEDIA: ObjectOf<EEComplexUnit>(inA)->LoadMedia(false); return 0;
    case EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS:
        gDataPreLoading->PostUnloadUnitClass(ObjectOf<EEComplexUnit>(inA)->mPlayer->mPlayerID, ObjectOf<EEComplexUnit>(inA)->mUnitClass->GetUnitType());
        return 0;
    // the gather goal's
    case EE_UNITS_GOAL_HOST_FIND_STORAGE: return Handle(ObjectOf<EEComplexUnit>(inA)->mUnitClass->FindStorage(ObjectOf<EEComplexUnit>(inA), ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_X_IS_HUMAN: return ObjectOf<EEComplexUnit>(inA)->mPlayer->IsHuman();
    case EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP: return Handle(ObjectOf<EEComputerPlayer>(inA)->mActiveGroup);
    case EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS: return ObjectOf<EESelectedGroup>(inA)->GetNumUnits();
    case EE_UNITS_GOAL_HOST_X_GROUP_CLEAR: ObjectOf<EESelectedGroup>(inA)->Clear(); return 0;
    case EE_UNITS_GOAL_HOST_X_CITIZEN_FIND_STORAGE:
        return Handle(EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(ObjectOf<EEComplexUnit>(inA)->GetUnitClass())->FindStorage(ObjectOf<EEComplexUnit>(inA), ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_NEXT_TREE: return Handle(EEGatherNextTree(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB)));
    case EE_UNITS_GOAL_HOST_X_FIND_ACTION: return Handle(ObjectOf<EEComplexUnit>(inA)->mUnitClass->FindAction((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_SET_ATTACK_GOAL:
        ObjectOf<EEComplexUnit>(inA)->SetGoal(EEUGRust::NewAttackUnit(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), ioArgs->i[0] != 0, false), 1, 0);
        return 0;
    case EE_UNITS_GOAL_HOST_LAST_WAYPOINT: PointOut(ioArgs, GE3DPoint(ObjectOf<EEComplexUnit>(inA)->mWaypoints.back().mLocation)); return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST: return Handle(EEFindLoadedGhost((unsigned long)inA));
    case EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST_UNIT: return Handle(EEFindLoadedGhostUnit((unsigned long)inA));
    case EE_UNITS_GOAL_HOST_LINE_TO_UNIT_BLOCKED: return EEIsLineToUnitBlocked(ObjectOf<EEComplexUnit>(inA), ObjectOf<EEUnit>(inB), &sGoalBlocker, &sGoalBlockTile[0], &sGoalBlockTile[1]);
    // the gathering's (world.rs: BeginGathering, Work, SearchNavalResource)
    case EE_UNITS_GOAL_HOST_TAKE_HIT: ObjectOf<EEUnit>(inA)->TakeHit(ObjectOf<EEComplexUnit>(inB), UMFloatToLong(ObjectOf<EEComplexUnit>(inB)->mStats->mAttack), 0, 1); return 0;
    case EE_UNITS_GOAL_HOST_REGROW: ObjectOf<EEUnit>(inA)->RegrowForest(ObjectOf<EEComplexUnit>(inB)); return 0;
    case EE_UNITS_GOAL_HOST_X_WORK_TIME_FACTOR: ioArgs->f[0] = EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->mWorkTimeFactor; return 0;
    case EE_UNITS_GOAL_HOST_HIGH_TECH_TOOLS: return EEGetRules().Epoch(ObjectOf<EEComplexUnit>(inA)->mPlayer->mTechnologies->mCurrentEpoch).mHighTechTools;
    // the location goal's
    // the world searches' queries (world.rs): unit lists into the sink, tiles as ee_units_goal_push_tile
    default:   // the combat goals' operations (EEUGRustCombat.cpp)
        return EECombatGoalHostOp(inOp, inA, inB, ioArgs);
    }
    return 0;
}

// one body (EE_UNITS_GOAL_*) of inGoal; inA, inB, ioArgs as it takes them; the file or the text it writes
int64_t Goal(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0,
             EeUnitsGoalIo* ioArgs = NULL, FSFile* inFile = NULL, UWideString* inText = NULL, FILE* inDump = NULL)
{
    EeUnitsGoalContext theContext = {inFile, inText, inDump};
    EeUnitsGoalHost theHost = {&theContext, GoalOp};
    EeUnitsGoalIo theArgs = {};
    return ee_units_rust_goal(inOperation, inGoal->mKind, Handle(inGoal), inA, inB,
                              ioArgs != NULL ? ioArgs : &theArgs, &theHost);
}

}  // namespace

// the goal's members are the Rust object's head (ee_units goal_obj.rs)
static_assert(sizeof(EEUnitGoal) == sizeof(EeUnitsGoalBase), "EEUnitGoal is EeUnitsGoalBase");

// the world host of the goals' context-free calls (EEUnitGoal::Call)
static const int sGoalHostRegistered = (ee_units_goal_register_host(GoalOp), 0);

// (CreateGoal: ee_units goal_calls.rs ee_units_goal_create)

// (the location goal, location.rs: its constructors and accessors are inline in ee/EEUnitGoal.h)

EEUGRust* EEUGRust::NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                            bool inArg)
{
    std::sort(inUnits.begin(), inUnits.end(),
              [](EEComplexUnit* inFirst, EEComplexUnit* inSecond) { return inFirst->mUnitID < inSecond->mUnitID; });
    inUnits.erase(std::unique(inUnits.begin(), inUnits.end()), inUnits.end());
    return reinterpret_cast<EEUGRust*>(ee_units_goal_new_load(inKind, inArg, Handle(inContainer), &inUnits));   // goal_calls.rs
}

void EEUGRust::DumpOutOfSynch(FILE* inFile) { Goal(EE_UNITS_GOAL_DUMP, this, 0, 0, NULL, NULL, NULL, inFile); }

void EEUGRust::GetDebugLine(UWideString& ioText) { Goal(EE_UNITS_GOAL_DEBUG_LINE, this, 0, 0, NULL, NULL, &ioText); }

// ---- the C++ of EEGatherNextTree's Rust body (ee_units uu_gather_tree.rs; EE_UNITS_GT_*): one statement each

static std::vector<EEUnit*> sGtUnits;                 // the units in the radius
static std::vector<U2DPoint<long> > sGtTiles;         // the tile line
static U2DPoint<long> sGtTarget;                      // the line's target tile
static GE3DPoint sGtSpot;                             // the free spot asked

template <class T> static T* GtObj(uint64_t inHandle) { return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle)); }

extern "C" int64_t ee_units_gt_host(int32_t inOp, uint64_t inA, uint64_t inB, int64_t inC, int64_t inD, float (*ioF)[4])
{
    switch (inOp) {
    case EE_UNITS_GT_FREE_SPOT: return ee_units_uu_spot(1, inA, inB, (float(*)[3])&sGtSpot.mX);
    case EE_UNITS_GT_FOREST_ID: return gContinentManager.GetForestIDAt(U2DPoint<long>((long)inC, (long)inD));
    case EE_UNITS_GT_FOREST: return (int64_t)(uintptr_t)gContinentManager.GetForests()[(unsigned short)inC];
    case EE_UNITS_GT_FOREST_TILES: return GtObj<EEAIForest>(inA)->GetNumTiles();
    case EE_UNITS_GT_UNITS_IN_RADIUS: return ee_units_uu_units_in_radius((*ioF)[0], (*ioF)[1], (*ioF)[2], &sGtUnits);
    case EE_UNITS_GT_LIST_UNIT: return (int64_t)(uintptr_t)sGtUnits[(size_t)inC];
    case EE_UNITS_GT_IS_DROP_SITE: return GtObj<EEUnit>(inA)->GetUnitClass()->IsDropSite();
    case EE_UNITS_GT_LINE_CLEAR: sGtTiles.clear(); return 0;
    case EE_UNITS_GT_LINE_TARGET: sGtTarget = U2DPoint<long>((long)inC, (long)inD); return 0;
    case EE_UNITS_GT_LINE_TILES: return (U2DPoint<long>((long)inC, (long)inD).GetLineTiles(sGtTarget, 1.0f, sGtTiles), (int64_t)sGtTiles.size());
    case EE_UNITS_GT_LINE_TILE: return (int64_t)(((uint64_t)(uint32_t)sGtTiles[(size_t)inC].mY << 32) | (uint32_t)sGtTiles[(size_t)inC].mX);
    case EE_UNITS_GT_CAN_GATHER: return GtObj<EEComplexUnit>(inA)->CanGather(GtObj<EEUnit>(inB), (long)inC, (long)inD, false, false);
    case EE_UNITS_GT_HAS_GATHER_POINT: return EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(GtObj<EEComplexUnit>(inA)->GetUnitClass())->HasGatherPoint(GtObj<EEComplexUnit>(inA), GtObj<EEUnit>(inB));
    }
    return 0;
}
