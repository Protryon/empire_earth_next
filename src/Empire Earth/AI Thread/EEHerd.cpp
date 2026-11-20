// C:\EEX\Empire Earth\AI Thread\EEHerd.cpp (file name inferred: no __FILE__; RTTI class EEHerd > EEAIArea,
// vtable 0x43c2e8; the save key "EEHerd-SaveLoadData"). A herd of wild animals as an AI area: 0xa1910..0xa543d
// (EECPWallManager/EEHerd boundary 0xa1910; EEHumanPlayer's ctor starts at 0xa543d).
// Port (the AI Thread cluster, round 2): the herd's bodies are Rust's (ee_sim::cp herd.rs); the class forwards to
// them, and this file is the herd's host (ee_cp_herd_host): the units, the goals, the formations, the map, the
// continent manager and the area the herd is.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEFormation.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEHerd.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUnitCollector.h"   // EEGetUnitsInRectangle's template body (EEHerdUnitFilter's instance)

// The unit filter of Chase/GetPrey (by reference to the collector; operator() COMDAT 0xa5238 and its test 0xa526b
// after the collector): a living sea-going unit on the map, on mContinent (0: any), either a fishing boat (mHuntable:
// unit types 0x6c, 0x6d, 0xd3) or a non-gaia unit that is no submarine or sea animal (categories 10, 0x1b), moving
// when mMoving (provisional name)
struct EEHerdUnitFilter {
    EEHerdUnitFilter(unsigned short inContinent, bool inHuntable, bool inMoving)
        : mContinent(inContinent), mHuntable(inHuntable), mMoving(inMoving) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return (inUnit->mUnitClass->mDomains & kMovementSea) && (unsigned long)inUnit->mTileX < gMapWidth &&
            (unsigned long)inUnit->mTileY < gMapHeight ? this->Accept(inUnit) : false;
    }
    bool Accept(EEComplexUnit* inUnit) const
    {
        if ((this->mHuntable ? inUnit->mUnitClass->mUnitType >= kUnitTypeS02FishingBoatStone && (inUnit->mUnitClass->mUnitType <= kUnitTypeS04FishingBoatBronze ||
                                                                      inUnit->mUnitClass->mUnitType == kUnitTypeS11FishingBoatModern)
                          : inUnit->mUnitClass->GetCategory() != kUnitCategorySubmergedNaval && inUnit->mUnitClass->GetCategory() != kUnitCategorySeaAnimal &&
                                inUnit->mPlayer->mPlayerID != 0) &&
            (this->mContinent == 0 || gContinentManager.GetContinentID(inUnit) == this->mContinent) &&
            (!this->mMoving || ((long)inUnit->mWaypoints.size() > 1) == true) && inUnit->mHitPoints > 0)
            return true;
        return false;
    }
    unsigned short mContinent;
    bool mHuntable;
    bool mMoving;
};

const UString EEHerd::sData("EEHerd-SaveLoadData");
// 0x4e5e40, right after sData (0x4e5e30 + 0x10) in the original's initialised .data, value 0; its only use clears it
// (EEWorld.cpp). Owner by placement only (EECalamity's statics start at 0x4e5e48): PROVISIONAL
long gHerdUnreadValue = 0;

static void* EEHerdSelf(EEHerd* inHerd)
{
    return static_cast<void*>(inHerd);
}

EEHerd::EEHerd()
{
    ee_cp_herd_init(this);
    this->SetAreaType(kAIAreaHerd);
    ee_cp_herd_construct_default(EEHerdSelf(this), this);
}

EEHerd::EEHerd(EEPlayer* inPlayer, long inAnimal, unsigned short inID)
{
    ee_cp_herd_init(this);
    this->SetAreaType(kAIAreaHerd);
    ee_cp_herd_construct(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inPlayer, inAnimal, inID);
}

EEHerd::~EEHerd()
{
    ee_cp_herd_clear(EEHerdSelf(this), this);
    ee_cp_herd_free(this);
}

bool EEHerd::AddUnit(EEComplexUnit* inUnit)
{
    return ee_cp_herd_add_unit(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit);
}

void EEHerd::HandleUnitHit(EEComplexUnit* inUnit)
{
    ee_cp_herd_handle_unit_hit(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit);
}

void EEHerd::AttackBack(EEComplexUnit* inUnit, bool inAll)
{
    ee_cp_herd_attack_back(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit, inAll);
}

bool EEHerd::CanTake(EEComplexUnit* inUnit, bool inCheckRange)
{
    return ee_cp_herd_can_take(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit, inCheckRange);
}

bool EEHerd::HasUnit(EEComplexUnit* inUnit)
{
    return ee_cp_herd_has_unit(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit);
}

bool EEHerd::RemoveUnit(EEComplexUnit* inUnit)
{
    return ee_cp_herd_remove_unit(EEHerdSelf(this), this, (uint64_t)(uintptr_t)inUnit);
}

void EEHerd::Update(unsigned long inTime)
{
    ee_cp_herd_update(EEHerdSelf(this), this, inTime);
}

void EEHerd::Create(const std::vector<U2DPoint<long> >& inTiles)
{
    std::vector<EeHerdPoint> theTiles(inTiles.size());
    for (size_t i = 0; i < inTiles.size(); i++)
        theTiles[i] = EeHerdPoint{inTiles[i].mX, inTiles[i].mY};
    ee_cp_herd_create(EEHerdSelf(this), this, theTiles.data(), theTiles.size());
}

U2DPoint<long> EEHerd::GetRandomPoint()
{
    EeHerdPoint thePoint;
    ee_cp_herd_random_point(EEHerdSelf(this), this, &thePoint);
    return U2DPoint<long>(thePoint.mX, thePoint.mY);
}

// Returns whether a unit is a placed animal or fish of a known animal type
bool EEHerd::IsAnimalOnMap(EEUnit* inUnit)
{
    return (inUnit->mUnitClass->mFamily == kUnitFamilyAnimal || inUnit->mUnitClass->mFamily == kUnitFamilyFish) &&
        EEAnimal::Find(inUnit->mUnitClass->mUnitType) && inUnit->mTileX != -10000 && inUnit->mTileY != -10000;
}

long EEHerd::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_cp_herd_load(EEHerdSelf(this), this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion, &inFile,
                           &EESaveFilePrimitive);
}

long EEHerd::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_cp_herd_save(EEHerdSelf(this), this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion, &outFile,
                           &EESaveFilePrimitive);
}

long EEHerd::RepairPointers(long inMajorVersion, long inMinorVersion)
{
    return ee_cp_herd_repair_pointers(EEHerdSelf(this), this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

void EEHerd::GetUnits(std::vector<EEComplexUnit*>& outUnits)
{
    for (size_t i = 0, n = ee_cp_herd_unit_count(this); i < n; i++) {
        EEComplexUnit* theUnit;
        EEUnitHandleResolve(ee_cp_herd_unit_handle(this, i), theUnit);
        outUnits.push_back(theUnit);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The herd's host (ee_sim::cp herd.rs EE_CP_HERD_*): `a`, `b` units (EEComplexUnit*), herds, animals or media

static EEComplexUnit* EEHerdUnit(uint64_t inHandle)
{
    return reinterpret_cast<EEComplexUnit*>((uintptr_t)inHandle);
}

static uint64_t EEHerdHandle(const void* inObject)
{
    return (uint64_t)(uintptr_t)inObject;
}

static void EEHerdPushUnits(void* inList, const std::vector<EEComplexUnit*>& inUnits)
{
    for (size_t i = 0; i < inUnits.size(); i++)
        ee_cp_herd_push(inList, (int64_t)EEHerdHandle(inUnits[i]), 0, 0, 0);
}

static uint32_t EEHerdBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof(theBits));
    return theBits;
}

// GiveGoal (0xa31b4): a live, enabled, placed animal takes the goal and starts the action; else the goal is deleted
static bool EEHerdGiveGoal(EEComplexUnit* inUnit, EEUGRust* inGoal, bool inKeepCurrent, long inAction)
{
    if (inUnit && inUnit->mHitPoints > 0 && !inUnit->mIsDisabled && inUnit->mTileX != -10000) {
        inUnit->SetGoal(inGoal, inKeepCurrent, 0);
        inUnit->StartAction(inAction);
        inUnit->mUnitClass->ActionChanged(inUnit);
        return true;
    }
    if (inGoal)
        delete inGoal;
    return false;
}

// The area operations of the random point (EETown::GetRandomPoint 0xac203, the herd's by ICF; ee_sim::cp herd.rs
// random_area_point) and of the herd: an area's hull, the map, the draws
extern "C" int64_t ee_cp_area_host(void* inArea, int32_t inOp, EeHerdIo* io)
{
    EEAIArea* me = static_cast<EEAIArea*>(inArea);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_HERD_RANDOM_LONG:
        return gPhysics->GetRandom((long)v[0], (long)v[1]);
    case EE_CP_HERD_HULL_CONTAINS:
        return me->HullContains(U2DPoint<long>((long)v[0], (long)v[1])) ? 1 : 0;
    case EE_CP_HERD_HULL_BOUNDS: {
        const U2DRectangle<long>& theBounds = me->GetHullBounds();
        io->v[0] = theBounds.mBottom;
        io->v[1] = theBounds.mLeft;
        io->v[2] = theBounds.mRight;
        io->v[3] = theBounds.mTop;
        return 0;
    }
    case EE_CP_HERD_BASE_POINT: {
        U2DPoint<long> thePoint = me->AreaRandomPoint();
        io->v[0] = thePoint.mX;
        io->v[1] = thePoint.mY;
        return 0;
    }
    case EE_CP_HERD_MAP_TILE_AT:
        return gWorld.mMap->GetTileAtLocation(U2DPoint<long>((long)v[0], (long)v[1])) != NULL ? 1 : 0;
    case EE_CP_HERD_HAS_MAP:
        return gWorld.mMap != NULL ? 1 : 0;
    }
    return 0;
}

// (the area's, the continent manager's, the registry's and the players' reads are Rust's: herd_game.rs world_op)
extern "C" int64_t ee_cp_herd_host(void* inHerd, int32_t inOp, uint64_t inA, uint64_t inB, EeHerdIo* io)
{
    EEHerd* me = static_cast<EEHerd*>(inHerd);
    EEComplexUnit* theUnit = EEHerdUnit(inA);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_HERD_UNIT: {
        EeHerdUnit& r = io->unit;
        r.unit_id = theUnit->mUnitID;
        r.handle = EEUnitHandleID(theUnit);
        r.tile_x = theUnit->mTileX;
        r.tile_y = theUnit->mTileY;
        r.hit_points = theUnit->mHitPoints;
        r.growth = theUnit->mGrowth;
        r.graze_time = theUnit->mGrazeTime;
        r.grow_time = theUnit->mGrowTime;
        r.command = theUnit->mCommand ? theUnit->mCommand->mType : 0;
        r.waypoints = (int64_t)theUnit->mWaypoints.size();
        r.held = (int64_t)theUnit->mHeldUnits.size();
        r.holder = EEHerdHandle(theUnit->mHolder);
        r.herd = EEHerdHandle(theUnit->mHerd);
        r.player = EEHerdHandle(static_cast<EEPlayer*>(theUnit->mPlayer));
        r.last_attacker = EEHerdHandle(theUnit->mLastAttacker);
        r.model = EEHerdHandle((EEMedia*)theUnit->mStats->mModel);
        r.attack = theUnit->mUnitClass->mAttack;
        r.ammo_type = theUnit->mUnitClass->mAmmoType;
        r.family = theUnit->mUnitClass->mFamily;
        r.animal_id = theUnit->mUnitClass->GetAnimalID();
        r.disabled = theUnit->mIsDisabled;
        r.initialized = theUnit->GetStats()->GetIsInitialized();
        r.has_goal = theUnit->GetGoal() != NULL;
        r.regrowing = theUnit->GetIsRegrowing();
        r.b34f = theUnit->mB34F;
        r.can_be_attacked = theUnit->mUnitClass->CanBeAttacked();
        return 0;
    }
    case EE_CP_HERD_REF:
        theUnit->mReferenceCount++;
        return 0;
    case EE_CP_HERD_DEREF:
        theUnit->Dereference();
        return 0;
    case EE_CP_HERD_SET_HERD:
        theUnit->SetHerd(reinterpret_cast<EEHerd*>((uintptr_t)inB));
        return 0;
    case EE_CP_HERD_SET_GRAZE:
        theUnit->SetGrazeTime((unsigned long)v[0]);
        return 0;
    case EE_CP_HERD_SET_GROWTH:
        theUnit->SetGrowth(io->f[0]);
        return 0;
    case EE_CP_HERD_SET_GROW_TIME:
        theUnit->SetGrowTime((unsigned long)v[0]);
        return 0;
    case EE_CP_HERD_RANDOM_FLOAT:
        io->f[0] = gPhysics->GetRandom(io->f[0], io->f[1]);
        return 0;
    case EE_CP_HERD_RANDOM:
        io->f[0] = gPhysics->GetRandom();
        return 0;
    case EE_CP_HERD_GENERATE:
        io->f[0] = gPhysics->mRandom.Generate();
        return 0;
    case EE_CP_HERD_AREA_POINT: {
        U2DPoint<long> thePoint = reinterpret_cast<EEAIArea*>((uintptr_t)inA)->GetRandomPoint();
        io->v[0] = thePoint.mX;
        io->v[1] = thePoint.mY;
        return 0;
    }
    case EE_CP_HERD_MAP_EDGE:
        return (int64_t)EEHerdHandle(gContinentManager.GetRandomContinent(kContinentCategoryMapEdge, gPhysics->mRandom));
    case EE_CP_HERD_UNITS_IN_RECT: {
        U2DRectangle<long> theRect;
        theRect.mBottom = (long)v[0];
        theRect.mLeft = (long)v[1];
        theRect.mRight = (long)v[2];
        theRect.mTop = (long)v[3];
        std::vector<EEComplexUnit*> theUnits;
        EEHerdUnitFilter theFilter((unsigned short)v[4], (v[5] & 1) != 0, (v[5] & 2) != 0);
        EEGetUnitsInRectangle(theRect, theUnits, theFilter, false);
        EEHerdPushUnits(io->list, theUnits);
        return 0;
    }
    case EE_CP_HERD_FORMATION: {
        std::vector<EEComplexUnit*> theUnits;
        for (size_t i = 0; i < io->input_len; i++)
            theUnits.push_back(EEHerdUnit(io->input[i]));
        std::vector<EEFormationSlot> theSlots;
        EEFormation* theFormation = EEFormation::Create(6);
        GE3DPoint theCenter(io->f[0], io->f[1], io->f[2]);
        if (v[0] == 0) {   // Chase (the original never released this formation)
            theFormation->mCenter.mX = theCenter.mX;
            theFormation->mCenter.mY = theCenter.mY;
            theFormation->mCenter.mZ = theCenter.mZ;
            theFormation->SetMinRadius(gPhysics->GetRandom(1.25f, 2.75f));
            theFormation->Place(theUnits, theSlots);
        } else if (v[0] == 1) {   // MoveTo
            if (v[1] != 0) {
                theFormation->mCenter = theCenter;
                theFormation->mMinRadius = io->f[3];
                theFormation->Place(theUnits, theSlots);
            }
            theFormation->Dereference();
        } else {   // Surround
            theFormation->SetCenter(theCenter);
            theFormation->SetMinRadius(gPhysics->GetRandom(1.25f, 2.75f));
            theFormation->Place(theUnits, theSlots);
            theFormation->Dereference();
        }
        EEHerdPushUnits(io->list2, theUnits);
        for (size_t i = 0; i < theSlots.size(); i++)
            ee_cp_herd_push(io->list, EEHerdBits(theSlots[i].mPosition.mX), EEHerdBits(theSlots[i].mPosition.mY),
                            EEHerdBits(theSlots[i].mPosition.mZ), EEHerdBits(theSlots[i].mHeading));
        return 0;
    }
    case EE_CP_HERD_STAND: {   // CanStandOn 0xa1e75: walkable terrain, passable units and no other herd member there
        U2DPoint<long> theAt((long)v[0], (long)v[1]);
        bool theResult = false;
        EETile* theTile = gWorld.mMap->GetTileAtLocation(theAt);
        if (theTile) {
            long theType = theTile->mTerrainType;
            theResult = theUnit->GetUnitClass()->CanMoveOnTerrain(theUnit, theType);
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode && theResult == true; theNode = theNode->mNext) {
                EEComplexUnit* theOwner = theNode->mUnit->mComplexUnit;
                theResult = theNode->mUnit->IsPassableBy(theUnit);
                if (theResult == true && theOwner && theUnit->mHolder != theOwner
                    && std::find(io->input, io->input + io->input_len, EEHerdHandle(theOwner)) != io->input + io->input_len
                    && theOwner->mHolder != theUnit)
                    theResult = false;
            }
        }
        return theResult ? 1 : 0;
    }
    case EE_CP_HERD_MEDIA_LOAD:
        reinterpret_cast<EEMedia*>((uintptr_t)inA)->LoadMedia();
        return 0;
    case EE_CP_HERD_MEDIA_UNLOAD:
        reinterpret_cast<EEMedia*>((uintptr_t)inA)->UnloadMedia();
        return 0;
    case EE_CP_HERD_ANIMAL:
        return (int64_t)EEHerdHandle(EEAnimal::Get((long)v[0]));
    case EE_CP_HERD_ANIMAL_FIELDS:
        return (int64_t)EEHerdHandle(static_cast<EeDataAnimalFields*>(reinterpret_cast<EEAnimal*>((uintptr_t)inA)));
    case EE_CP_HERD_ANIMAL_MEDIA:
        return (int64_t)EEHerdHandle(reinterpret_cast<EEAnimal*>((uintptr_t)inA)->mMedia);
    case EE_CP_HERD_FLEE:
        theUnit->mUnitClass->Flee(theUnit);
        return 0;
    case EE_CP_HERD_DAMAGE_FACTOR:
        io->f[0] = EEDamageFactor((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_HERD_CLEAR_ATTACKER: {
        EEComplexUnit*& theAttacker = theUnit->mLastAttacker;
        if (theAttacker)
            theAttacker->Dereference();
        theAttacker = NULL;
        return 0;
    }
    case EE_CP_HERD_RELEASE_HOLDER:
        theUnit->mHolder->ReleaseHeld(theUnit);
        if (theUnit->mHolder)
            theUnit->mHolder->Dereference();
        theUnit->mHolder = NULL;
        return 0;
    case EE_CP_HERD_HELD:
        return (int64_t)EEHerdHandle(theUnit->mHeldUnits[(size_t)v[0]]);
    case EE_CP_HERD_WAYPOINT: {
        const GE3DPoint& theLocation = theUnit->mWaypoints[(size_t)v[0]].mLocation;
        io->f[0] = theLocation.mX;
        io->f[1] = theLocation.mY;
        io->f[2] = theLocation.mZ;
        return 0;
    }
    case EE_CP_HERD_POSITION: {
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        io->f[0] = thePosition.mX;
        io->f[1] = thePosition.mY;
        io->f[2] = thePosition.mZ;
        return 0;
    }
    case EE_CP_HERD_DISTANCE:
        io->f[0] = theUnit->DistanceSquaredTo(EEHerdUnit(inB));
        return 0;
    case EE_CP_HERD_MAP_Z:
        io->f[0] = gWorld.mMap->mTerrainMesh.GetZ((long)v[0], (long)v[1]);
        return 0;
    case EE_CP_HERD_MAP_SIZE:
        io->v[0] = (int64_t)gMapWidth;
        io->v[1] = (int64_t)gMapHeight;
        io->v[2] = gWorld.GetMap()->GetSize();
        return 0;
    case EE_CP_HERD_BIRTH: {   // GiveBirth 0xa3d9c's newborn
        EEAnimal* theAnimal = reinterpret_cast<EEAnimal*>((uintptr_t)inB);
        EEUnitClass* theClass = theAnimal->GetUnitClass();
        EEUnit* theNew = theClass->CreateUnit(theUnit->GetTileX(), theUnit->GetTileY(), theUnit->mPlayer->GetPlayerID(), 0, NULL);
        EEComplexUnit* theYoung = dynamic_cast<EEComplexUnit*>(theNew);
        theYoung->SetGrowth(theAnimal->GetNewbornGrowth());
        theYoung->SetGrowTime(gPhysics->mGameTime + 4000);
        theYoung->SetTargetSearchTimer(theClass->mTargetSearchInterval);
        theUnit->HoldUnit(theYoung);
        return 0;
    }
    case EE_CP_HERD_FIELDS_OF:
        return (int64_t)EEHerdHandle(static_cast<EeHerdFields*>(reinterpret_cast<EEHerd*>((uintptr_t)inA)));
    case EE_CP_HERD_GOALS:
        return (int64_t)theUnit->mGoalQueues[0].mGoals.size();
    case EE_CP_HERD_GOAL_LOCATION: {
        EEUGRust* theGoal = EELocationGoalCast(theUnit->mGoalQueues[0].mGoals[(size_t)v[0]]);
        if (theGoal == NULL)
            return 0;
        io->f[0] = theGoal->GetLocation().GetX();
        io->f[1] = theGoal->GetLocation().GetY();
        return 1;
    }
    case EE_CP_HERD_FLAGS:
        return (gLoadingSavedGame ? 1 : 0) | (gWorldClearing ? 2 : 0);
    case EE_CP_HERD_GOAL_ATTACK:
        return EEHerdGiveGoal(theUnit, EEUGRust::NewAttackUnit(theUnit, static_cast<EEUnit*>(EEHerdUnit(inB)), true, false), 0,
                              kActionInitialAttackState) ? 1 : 0;
    case EE_CP_HERD_GOAL_MOVE: {
        GE3DPoint theLocation(io->f[0], io->f[1], io->f[2]);
        EEUGRust* theGoal = v[4] != 0
            ? EEUGRust::NewLocation(io->f[0], io->f[1], io->f[2], v[0] != 0, v[1] != 0, v[2] != 0, io->f[3], false, false)
            : EEUGRust::NewLocation(&theLocation, v[0] != 0, v[1] != 0, v[2] != 0, io->f[3], false, false);
        return EEHerdGiveGoal(theUnit, theGoal, v[3] != 0, kActionPrepareToMove) ? 1 : 0;
    }
    case EE_CP_HERD_PLAYER: {
        EEPlayer* thePlayer = gWorld.GetPlayer((unsigned long)v[0]);
        return (int64_t)EEHerdHandle(thePlayer);
    }
    case EE_CP_HERD_UNIT_BY_ID:
        return (int64_t)EEHerdHandle(dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)v[0]))));
    case EE_CP_HERD_SAVE_FLAGS:
        return (EESaveHasLiveState((unsigned long)v[0], (unsigned long)v[1]) ? 1 : 0)
            | (EESaveHasContinentRegions((unsigned long)v[0], (unsigned long)v[1]) ? 2 : 0);
    case EE_CP_HERD_ANIMAL_ON_MAP:
        return EEHerd::IsAnimalOnMap(theUnit) ? 1 : 0;
    }
    return ee_cp_area_host(static_cast<EEAIArea*>(me), inOp, io);
}
