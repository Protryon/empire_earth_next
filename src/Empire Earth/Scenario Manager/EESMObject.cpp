// C:\EEX\Empire Earth\Scenario Manager\EESMObject.cpp (file name inferred: no __FILE__): 0x1c5f73..0x1ca4ee,
// scenario objects and triggers (the trigger entries, once EESMTConditionWrapper.cpp and EESMTEffectWrapper.cpp, are
// Rust records).
// Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EESMObject.h"
#include <algorithm>
#include "ee/EESMTrigger.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"

#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPlayer.h"
#include "ee/EESMArea.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioHost.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECombatGoals.h"
#include "ee/EESMCondition.h"
#include "lle/U2DSparseArray.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"

// Creates an object named None that picks units on the map, owned by and selected by player 7, with no filters
// (the numbers: ee_scenario fields.rs object_defaults)
EESMObject::EESMObject() : EeScenarioObjectFields(ee_scenario_object_defaults())
{
    gResource->GetString(IDS_NONE, this->mName);
}

namespace {

// a unit id handed over as 32 bits (ee_scenario objeval.rs), as the list holds it
EEUnit* SMUnit(int32_t inID)
{
    return gWorld.GetUnit((unsigned long)(uint32_t)inID);
}

// a chain unit for objeval.rs: its id low, its class's type high; -1 at the end
int64_t SMChainUnit(EEUnit* inUnit)
{
    return inUnit == NULL ? -1 : (int64_t)(uint32_t)inUnit->GetUnitID() | ((int64_t)(uint32_t)inUnit->mUnitClass->mUnitType << 32);
}

}   // namespace

// Evaluate's and FindPlacement's reads, each the original's expression (objeval.rs EE_SCENARIO_READ_OBJ_* and the unit
// reads, placement.rs)
int64_t EESMObjectRead(EESMHostContext* inContext, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, int32_t inD)
{
    switch (inOp) {
    case EE_SCENARIO_READ_OBJ_UNIT_COUNT: return (int64_t)inContext->mObject->mUnits.size();
    case EE_SCENARIO_READ_OBJ_UNIT_AT: return (int64_t)(uint32_t)inContext->mObject->GetUnitAt((unsigned long)inA);
    case EE_SCENARIO_READ_OBJ_EVALUATE: return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA)->Evaluate(inB != 0);
    case EE_SCENARIO_READ_UNIT_CLASS_TYPE: return SMUnit(inA)->mUnitClass->mUnitType;
    case EE_SCENARIO_READ_UNIT_PLAYER: return SMUnit(inA)->mPlayer->GetPlayerID();
    case EE_SCENARIO_READ_UNIT_COMMAND: return SMUnit(inA)->mComplexUnit->GetCommandType();
    case EE_SCENARIO_READ_UNIT_ATTRIBUTE:
        return inContext->mObject->GetUnitAttribute((unsigned long)inB, inC != 0, SMUnit(inA), 0, 0);
    case EE_SCENARIO_READ_UNIT_SEEN_BY: return EECombatIsSeenBy(SMUnit(inA), gWorld.mPlayers[inB], false);
    case EE_SCENARIO_READ_MAP_SIZE: return EESMPack((long)gWorld.mMap->mL195610, (long)gWorld.mMap->mL195614);
    case EE_SCENARIO_READ_AREA_CONTAINS_TILE:
        return gWorld.mScenarioManager->GetArea((unsigned long)inA)->ContainsTile(U2DPoint<long>(inB, inC));
    case EE_SCENARIO_READ_OBJECT_UNIT_COUNT: return (int64_t)gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA)->mUnits.size();
    case EE_SCENARIO_READ_OBJECT_UNIT_AT:
        return (int64_t)(uint32_t)gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA)->mUnits[(size_t)inB];
    case EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK: return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA)->mSelectedByCheck;
    case EE_SCENARIO_READ_UNIT_TILE_OF: return EESMPack(SMUnit(inA)->mTileX, SMUnit(inA)->mTileY);
    case EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT: return EESMBits(SMUnit(inA)->mComplexUnit->GetLineOfSight());
    case EE_SCENARIO_READ_UNIT_RANGE: return EESMBits(SMUnit(inA)->mStats->mRange);
    case EE_SCENARIO_READ_UNIT_TILE_SIZE: return SMUnit(inA)->mUnitClass->mTileSize;
    case EE_SCENARIO_READ_CLASS_TILE_SIZE: return gUnitClassTable[inA]->mTileSize;
    case EE_SCENARIO_READ_BLOCK_TAKES: return ee_units_uu_block_takes(0, inA, inB, inC, inD, inD, 0);
    case EE_SCENARIO_READ_MAP_TILE_EXISTS: return gWorld.mMap->GetTile(inA, inB) != NULL;
    case EE_SCENARIO_READ_UNIT_CAN_REACH_TILE:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->CanReachTile(inB, inC);
    case EE_SCENARIO_READ_UNIT_IN_REACH:
        return SMUnit(inA)->mComplexUnit != NULL && SMUnit(inA)->mComplexUnit->IsInReach(SMUnit(inB));
    }
    return 0;
}

// the object reads' single statements (ee_scenario object_reads.rs EE_SCENARIO_WZ_*: the Rust composes the
// sequences of Evaluate's, FindPlacement's and the conditions' reads), on handles; through the world channel
UWideString sWZText[2];        // the chat test's two texts (the original's locals)
U2DPoint<long> sWZPoint;   // the centres' point (the original's local)
std::vector<U2DPoint<long> > sWZTiles;   // the host's tile list (the original's local)
template<class T> T* SMWzOf(int64_t inHandle) { return (T*)(intptr_t)inHandle; }
int64_t SMWzPtr(const void* inObject) { return (int64_t)(intptr_t)inObject; }
extern "C" int64_t ee_scenariocpp_world_object(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_WZ_UNIT: return SMWzPtr(SMUnit((int32_t)a[0]));
    case EE_SCENARIO_WZ_UNIT_HIT_POINTS: return SMWzOf<EEUnit>(a[0])->GetHitPoints();
    case EE_SCENARIO_WZ_UNIT_IS_TREE: return SMWzOf<EEUnit>(a[0])->mUnitClass->mIsTree;
    case EE_SCENARIO_WZ_UNIT_COMPLEX: return SMWzOf<EEUnit>(a[0])->mComplexUnit != NULL;
    case EE_SCENARIO_WZ_UNIT_CARRIED: return SMWzOf<EEUnit>(a[0])->mComplexUnit->mPreviousPart != NULL;
    case EE_SCENARIO_WZ_PLAYER_SLOT: return SMWzPtr(gWorld.mPlayers[a[0]]);
    case EE_SCENARIO_WZ_GROUP_CONTAINS: return SMWzOf<EEPlayer>(a[0])->mActiveGroup->Contains(SMWzOf<EEUnit>(a[1])->mComplexUnit);
    case EE_SCENARIO_WZ_VIEWED_UNIT: return SMWzOf<EEPlayer>(a[0])->mViewedUnit == SMWzOf<EEUnit>(a[1]);
    case EE_SCENARIO_WZ_FIRST_UNIT: return SMWzPtr(SMWzOf<EEPlayer>(a[0])->mFirstUnit);
    case EE_SCENARIO_WZ_NEXT_UNIT: return SMWzPtr(static_cast<EEUnit*>(SMWzOf<EEUnit>(a[0])->mNext));
    case EE_SCENARIO_WZ_CURSOR: return SMWzPtr(inContext->mCursor);
    case EE_SCENARIO_WZ_SET_CURSOR: inContext->mCursor = SMWzOf<EEUnit>(a[0]);
        return 0;
    case EE_SCENARIO_WZ_CHAIN_UNIT: return SMChainUnit(SMWzOf<EEUnit>(a[0]));
    case EE_SCENARIO_WZ_AREA: return SMWzPtr(gWorld.mScenarioManager->GetArea((unsigned long)a[0]));
    case EE_SCENARIO_WZ_AREA_CONTAINS_UNIT: return SMWzOf<EESMArea>(a[0])->ContainsUnit(SMWzOf<EEUnit>(a[1]));
    case EE_SCENARIO_WZ_TILE: return SMWzPtr(gWorld.mMap->GetTile((long)a[0], (long)a[1]));
    case EE_SCENARIO_WZ_TILE_TERRAIN: return SMWzOf<EEMapTile>(a[0])->mTerrainType;
    case EE_SCENARIO_WZ_FOOTPRINT_DISTANCE:
        return EESMBits(EEGetFootprintDistance(static_cast<EEComplexUnit*>(SMWzOf<EEUnit>(a[0])), (long)a[1], (long)a[2], SMWzOf<long>(a[3]), SMWzOf<long>(a[4])));
    case EE_SCENARIO_WZ_NEAREST_TILE: SMWzOf<EEUnit>(a[0])->GetNearestFootprintTile(SMWzOf<EEUnit>(a[1]), SMWzOf<long>(a[2]), SMWzOf<long>(a[3]));
        return 0;
    case EE_SCENARIO_WZ_CHAT_TEXT: sWZText[0] = inContext->mCondition->mChatText;
        return 0;
    case EE_SCENARIO_WZ_CHAT_LAST: sWZText[1] = gWorld.mScenarioManager->mLastChatText;
        return 0;
    case EE_SCENARIO_WZ_TEXT_LOWER: PlatMbsLwr((unsigned char*)(const char*)sWZText[a[0]]);
        return 0;
    case EE_SCENARIO_WZ_TEXT_FOUND: return sWZText[1].Find(sWZText[0], 0) != sWZText[1].GetLength();
    case EE_SCENARIO_WZ_POINT_X: return sWZPoint.mX;
    case EE_SCENARIO_WZ_POINT_Y: return sWZPoint.mY;
    case EE_SCENARIO_WZ_PREVIOUS_X: return (long)inContext->mUnit->GetComplexUnit()->mU0DC.GetX();
    case EE_SCENARIO_WZ_PREVIOUS_Y: return (long)inContext->mUnit->GetComplexUnit()->mU0DC.GetY();
    case EE_SCENARIO_WZ_UNIT_VARIABLE: return inContext->mUnit->GetComplexUnit()->mUnitVariables[(size_t)a[0]];
    case EE_SCENARIO_WZ_UNIT_SCALE: return EESMBits(inContext->mUnit->GetComplexUnit()->mPhysicalModel.GetScale());
    case EE_SCENARIO_WZ_TILES_CLEAR: sWZTiles.clear();
        return 0;
    case EE_SCENARIO_WZ_TILES_AREA: gWorld.mScenarioManager->GetArea((unsigned long)a[0])->GetTiles(sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_NEAR_POINT: ee_units_uu_get_tiles_near_point(SMWzOf<const float[3]>(a[0]), EESMFloat(a[1]), &sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_COUNT: return (int64_t)sWZTiles.size();
    case EE_SCENARIO_WZ_TILE_X: return sWZTiles[(size_t)a[0]].mX;
    case EE_SCENARIO_WZ_TILE_Y: return sWZTiles[(size_t)a[0]].mY;
    case EE_SCENARIO_WZ_CONTINENT_CENTER: gContinentManager.GetContinentCenter((unsigned short)a[0], sWZPoint);
        return 0;
    case EE_SCENARIO_WZ_TOWN_CENTER: sWZPoint = gWorld.GetPlayer((unsigned long)a[0])->GetTown((unsigned long)a[1])->GetHullCenter();
        return 0;
    case EE_SCENARIO_WZ_TILES_CONTINENT: gContinentManager.GetContinentTiles((unsigned short)a[0], sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_TOWN: gWorld.GetPlayer((unsigned long)a[0])->GetTown((unsigned long)a[1])->GetTiles(sWZTiles);
        return 0;
    }
    return 0;
}

// Evaluate's changes of the object's list (objeval.rs EE_SCENARIO_WRITE_OBJ_*)
void EESMObjectWrite(EESMHostContext* inContext, uint32_t inOp, int64_t inB)
{
    std::vector<unsigned long>& theUnits = inContext->mObject->mUnits;
    switch (inOp) {
    case EE_SCENARIO_WRITE_OBJ_CLEAR: theUnits.clear(); break;
    case EE_SCENARIO_WRITE_OBJ_COPY_PARENT:
        gWorld.mScenarioManager->GetScenarioObject((unsigned long)inB)->GetUnits(theUnits);
        break;
    case EE_SCENARIO_WRITE_OBJ_ERASE: theUnits.erase(theUnits.begin() + (size_t)inB); break;
    case EE_SCENARIO_WRITE_OBJ_PUSH: theUnits.push_back((unsigned long)(uint32_t)inB); break;
    case EE_SCENARIO_WRITE_OBJ_SHUFFLE: EEAIRandomShuffle(theUnits.begin(), theUnits.end()); break;
    case EE_SCENARIO_WRITE_OBJ_TRUNCATE: theUnits.erase(theUnits.begin() + (size_t)inB, theUnits.end()); break;
    }
}


// Adds to, subtracts from or sets one unit's attribute (hit points, cargo, flight time, garrison, ammo, scale, power,
// rot time, built state, variables) by a fixed, random or percentage amount
// Original cdecl helper; random range and the percentage flag are independent of add/subtract/assign mode.
// (the draws, the clamps and which value changes are ee_scenario unit_change.rs's, the unit's reads and writes
// EESMEffect.cpp's EE_SCENARIO_WORLD_CU_*; entry.rs adds the simulation capture's SMCU record)
void EEChangeUnit(unsigned long inAttribute, bool inPercentage, EEUnit* inUnit,
                long inMinimum, unsigned long inMaximum, bool inRandom, long inMode)
{
    ee_scenario_en_change_unit(inAttribute, inPercentage, inUnit, inMinimum, inMaximum, inRandom, inMode);
}

// ---- the trigger's members (ee_scenario trigger_members.rs: each a forward; the vectors and strings stay here, reached
// by ee_scenariocpp_trigger's single statements)

// the trigger members' single statements (trigger_members.rs EE_SCENARIO_TR_*): on trigger inTrigger, entry i, value v
extern "C" int64_t ee_scenariocpp_trigger(uint32_t inOp, EESMTrigger* t, int64_t i, int64_t v)
{
    switch (inOp) {
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_COUNT: return (int64_t)t->mConditions.size();
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_PUSH_NEW: t->mConditions.push_back(new EESMTConditionWrapper());
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_BACK_EXISTS: return t->mConditions.back() != NULL;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_DELETE_BACK: delete t->mConditions.back();
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_POP: t->mConditions.pop_back();
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_DELETE_AT: delete t->mConditions[(size_t)i];
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_ERASE: t->mConditions.erase(t->mConditions.begin() + (size_t)i);
        return 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_FIRST: return t->mConditions[(size_t)i]->mIsNot = v != 0;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_SECOND: return (int64_t)(t->mConditions[(size_t)i]->mValue = (unsigned long)v);
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SET_OPERATOR: return t->mConditions[(size_t)i]->mOperator = (long)v;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_FIRST: return t->mConditions[(size_t)i]->mIsNot;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_SECOND: return (int64_t)t->mConditions[(size_t)i]->mValue;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_OPERATOR: return t->mConditions[(size_t)i]->mOperator;
    case EE_SCENARIO_TR_CONDITIONS + EE_SCENARIO_TR_AT: return (int64_t)(intptr_t)t->mConditions[(size_t)i];
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_COUNT: return (int64_t)t->mEffects.size();
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_PUSH_NEW: t->mEffects.push_back(new EESMTEffectWrapper());
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_BACK_EXISTS: return t->mEffects.back() != NULL;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_DELETE_BACK: delete t->mEffects.back();
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_POP: t->mEffects.pop_back();
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_DELETE_AT: delete t->mEffects[(size_t)i];
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_ERASE: t->mEffects.erase(t->mEffects.begin() + (size_t)i);
        return 0;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_FIRST: return (int64_t)(t->mEffects[(size_t)i]->mDelay = (unsigned long)v);
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_SECOND: return (int64_t)(t->mEffects[(size_t)i]->mEffectIndex = (unsigned long)v);
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SET_OPERATOR: return t->mEffects[(size_t)i]->mOperator = (long)v;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_FIRST: return (int64_t)t->mEffects[(size_t)i]->mDelay;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_SECOND: return (int64_t)t->mEffects[(size_t)i]->mEffectIndex;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_OPERATOR: return t->mEffects[(size_t)i]->mOperator;
    case EE_SCENARIO_TR_EFFECTS + EE_SCENARIO_TR_AT: return (int64_t)(intptr_t)t->mEffects[(size_t)i];
    case EE_SCENARIO_TR_NAME_EMPTY: t->SetName("");
        return 0;
    case EE_SCENARIO_TR_TEXT_EMPTY: t->SetText("");
        return 0;
    case EE_SCENARIO_TR_COPY_NAME: t->mName = static_cast<EESMTrigger*>((void*)(intptr_t)i)->mName;
        return 0;
    case EE_SCENARIO_TR_COPY_TEXT: t->mText = static_cast<EESMTrigger*>((void*)(intptr_t)i)->mText;
        return 0;
    case EE_SCENARIO_TR_FIELDS: return (int64_t)(intptr_t) static_cast<EeScenarioTriggerFields*>(t);
    case EE_SCENARIO_TR_ADD_TIMED: gWorld.mScenarioManager->AddTimedEffect((long)i, (unsigned long)v, t->mIsInCinematic);
        return 0;
    case EE_SCENARIO_TR_WORLD_CINEMATIC: return gWorld.mInCinematic;
    }
    return 0;
}
