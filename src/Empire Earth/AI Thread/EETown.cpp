// C:\EEX\Empire Earth\AI Thread\EETown.cpp (file name inferred: no __FILE__; RTTI class EETown > EEAIArea,
// vtable 0x43c57c). A player's town: 0xabbaf..0xac7b9 (EEPathManager.cpp's COMDATs end at 0xabbaf, EEAEExtraDamageFunctor's
// ctor starts at 0xac7b9).
// Port (the AI Thread cluster, round 2): the bodies are Rust's (ee_sim::cp town.rs); this file is the town's host
// (ee_cp_town_host: the units, the area the town is, the continent manager, the patrol groups) and IsTownUnit.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EETown.h"

static void* EETownSelf(EETown* inTown)
{
    return static_cast<void*>(inTown);
}

static void* EETownArea(EETown* inTown)
{
    return static_cast<void*>(static_cast<EEAIArea*>(inTown));
}

#define EETOWN_ARGS EETownSelf(this), EETownArea(this), static_cast<EeTownFields*>(this)

// Creates a player's town with the given id: an empty town area with no patrol group, not near water
EETown::EETown(EEPlayer* inPlayer, long inTownID)
    : EeTownFields()
{
    this->mPlayer = inPlayer;
    this->mTownID = inTownID;
    this->SetAreaType(kAIAreaTown);
    this->mPatrolGroup = NULL;
    this->Reset();
}

EETown::~EETown()
{
    this->Reset();
}

bool EETown::AddUnit(EEUnit* inUnit)
{
    return ee_cp_town_add_unit(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

bool EETown::AddTile(const U2DPoint<long>& inTile)
{
    return ee_cp_town_add_tile(EETOWN_ARGS, inTile.mX, inTile.mY);
}

void EETown::Merge(EETown* inOther)
{
    ee_cp_town_merge(EETOWN_ARGS, (uint64_t)(uintptr_t)inOther, static_cast<EeTownFields*>(inOther));
}

void EETown::Reset()
{
    ee_cp_town_reset(EETOWN_ARGS);
}

bool EETown::UpdateNearWater()
{
    return ee_cp_town_update_near_water(EETOWN_ARGS);
}

U2DPoint<long> EETown::GetRandomPoint()
{
    EeHerdPoint thePoint;
    ee_cp_town_random_point(EETOWN_ARGS, &thePoint);
    return U2DPoint<long>(thePoint.mX, thePoint.mY);
}

bool EETown::RemoveUnit(EEUnit* inUnit)
{
    return ee_cp_town_remove_unit(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

void EETown::RefreshPatrolGroup()
{
    ee_cp_town_refresh_patrol_group(EETOWN_ARGS);
}

bool EETown::RemoveHullTile(const U2DPoint<long>& inTile)
{
    return ee_cp_town_remove_hull_tile(EETOWN_ARGS, inTile.mX, inTile.mY);
}

bool EETown::CanTake(EEUnit* inUnit)
{
    return ee_cp_town_can_take(EETOWN_ARGS, (uint64_t)(uintptr_t)inUnit);
}

// NEAR: theResult is kept in memory where the original keeps it in bl (the zero register of the NULL tests)
// Returns whether a unit is a placed, immobile, non-ambient land building on a land continent (walls only when
// a human player owns them)
bool EETown::IsTownUnit(EEUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    bool theResult = false;
    if (inUnit->mTile != NULL && theClass->mFamily != kUnitFamilyAmbient && (theClass->IsLandBuilding() || theClass->mCategory == kUnitCategoryBuilding19) &&
        theClass->mMaxVelocity == 0.0f && (!theClass->mIsWall || inUnit->mPlayer->mHumanPlayer != NULL)) {
        const EETileCoordinate& theTile = inUnit->FindAreaTile(true, 1);
        if (theTile.mX < gMapWidth && theTile.mY < gMapHeight) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTile.mX, theTile.mY);
            if (theContinent != NULL)
                theResult = theContinent->GetCategory() == kContinentCategoryLand;
        }
    }
    return theResult;
}

// ---------------------------------------------------------------------------------------------------------------
// The town's host (ee_sim::cp town.rs EE_CP_TOWN_*): `a` a unit (EEUnit*) or a town. The area's and the continent
// manager's operations are Rust's (town_game.rs area_op)

static std::vector<U2DPoint<long> > EETownInputPoints(const TownIo* io)
{
    std::vector<U2DPoint<long> > thePoints;
    for (size_t i = 0; i < io->input_len; i++)
        thePoints.push_back(U2DPoint<long>((long)io->input[2 * i], (long)io->input[2 * i + 1]));
    return thePoints;
}

extern "C" int64_t ee_cp_town_host(void* inTown, int32_t inOp, uint64_t inA, uint64_t inB, TownIo* io)
{
    EETown* me = static_cast<EETown*>(inTown);
    EEUnit* theUnit = reinterpret_cast<EEUnit*>((uintptr_t)inA);
    int64_t* v = io->v;
    (void)inB;
    switch (inOp) {
    case EE_CP_TOWN_UNIT: {
        EETechTreeNode* theNode = theUnit->GetTechTreeNode();
        EETileCoordinate theArea = theUnit->FindAreaTile(true, 1);
        v[0] = theUnit->GetUnitClass()->GetInfluence();
        v[1] = theNode != NULL && theNode->mData->mKind == 4 ? 1 : 0;
        v[2] = theUnit->GetUnitClass()->GetIsWall() ? 1 : 0;
        v[3] = theUnit->GetUnitClass()->GetUnitType();
        v[4] = theArea.mX;
        v[5] = theArea.mY;
        v[6] = (int64_t)(uintptr_t)static_cast<EEPlayer*>(theUnit->mPlayer);
        return 0;
    }
    case EE_CP_TOWN_IS_TOWN_UNIT:
        return EETown::IsTownUnit(theUnit) ? 1 : 0;
    case EE_CP_TOWN_PLAYER:
        return (int64_t)(uintptr_t)me->mPlayer;
    case EE_CP_TOWN_HAS_PATROL:
        return me->mPatrolGroup != NULL ? 1 : 0;
    case EE_CP_TOWN_FIND_PATROL:   // FindPatrolGroup 0xac116: an own path point's group on a hull vertex, referenced
        for (unsigned long i = 0; i < me->GetHullPointCount() && me->mPatrolGroup == NULL; i++) {
            EEMapTile* theMapTile = gWorld.GetMap()->GetTile(me->GetHullPoint(i).GetX(), me->GetHullPoint(i).GetY());
            if (theMapTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theMapTile->GetUnits(); theNode != NULL && me->mPatrolGroup == NULL;
                 theNode = theNode->GetNext()) {
                EEUnit* theTileUnit = theNode->GetUnit();
                if (theTileUnit != NULL && theTileUnit->GetUnitClass()->GetUnitType() == kUnitTypePathPoint
                    && theTileUnit->GetPlayer() == me->mPlayer && theTileUnit->GetComplexUnit()->GetPatrolGroup() != NULL) {
                    me->mPatrolGroup = theTileUnit->GetComplexUnit()->GetPatrolGroup();
                    ee_units_pg_reference(me->mPatrolGroup);
                }
            }
        }
        return 0;
    case EE_CP_TOWN_NEW_PATROL:
        me->mPatrolGroup = ee_units_pg_new();
        ee_units_pg_set_town_patrol(me->mPatrolGroup, true);
        me->mPlayer->AddPatrolGroup(me->mPatrolGroup);
        return 0;
    case EE_CP_TOWN_SET_PATROL:
        EEPGSetPatrolPoints(me->mPatrolGroup, EETownInputPoints(io), me->mPlayer->GetPlayerID());
        return 0;
    case EE_CP_TOWN_DROP_PATROL:
        if (me->mPatrolGroup != NULL) {
            ee_units_pg_dereference(me->mPatrolGroup);
            me->mPatrolGroup = NULL;
        }
        return 0;
    case EE_CP_TOWN_FLAGS:
        return (gLoadingSavedGame ? 1 : 0) | (gWorldClearing ? 2 : 0);
    }
    return 0;
}
