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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileLocation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEEvent.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/GESurface.h"
#include "lle/GEViewport.h"
#include "lle/GEScreen.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameVariant.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EERules.h"
#include "ee/EEAIPathSector.h"
#include "ee/AStarGraphableObjectBase.h"
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
#include "ee/EEUnitKind.h"
#include "rust/sim/units.h"

// the location goal's host hands the unit's transformation to ee_ge's GeTransformation (the same 4x4 floats)
static_assert(sizeof(GETransformation) == 64, "GETransformation is ee_ge's GeTransformation");

long gSpaceUnloadSoundID = 498;             // 0x4c8fa8 (.data = 0x1f2; the teleporter's unload sound)

// ---- the build/repair goal's world work (EEUGBuildRepair.cpp's helpers; the goal itself is build.rs)

// Orders units farthest from the reference tile first, so a sort leaves the nearest unit last
inline long EETileDistanceLess::DistanceSquared(EEUnit* inUnit) const
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->DistanceSquared(theTile);
}

bool EETileDistanceLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    long theLeft = this->DistanceSquared(inFirst);
    long theRight = this->DistanceSquared(inSecond);
    bool theResult = theLeft > theRight;
    return theResult;
}

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
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    GE3DPoint theLocation;
    U2DPoint<long> theTile;
    U2DPoint<long> theTarget;
    GE2DPoint theCenter;
    EEUnit* theFound = NULL;
    EEUnitClass* theCitizen = EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(inUnit->GetUnitClass());
    if (inUnit->mResourceAmount > 0 && inUnit->mResourceType != inResource->mResourceType)
        inUnit->mResourceAmount = 0;
    if (inResource->mResourceType == kResourceWood && !ee_units_uu_spot(1, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inResource, (float(*)[3])&theLocation.mX)) {
        unsigned short theForestID =
            gContinentManager.GetForestIDAt(U2DPoint<long>(inResource->GetTileX(), inResource->GetTileY()));
        EEAIForest* theForest = gContinentManager.GetForests()[theForestID];
        if (theForest != NULL && theForest->GetNumTiles() > 1) {
            theCenter.SetX(inResource->GetTileX());
            theCenter.SetY(inResource->GetTileY());
            ee_units_uu_units_in_radius(theCenter.mX, theCenter.mY, inUnit->GetLineOfSight(), &theUnits);
            EEComplexUnit* theOther = NULL;
            for (long i = theUnits.size() - 1; i >= 0; i--) {
                EEComplexUnit* theUnit = (EEComplexUnit*)*(theUnits.begin() + i);
                if (theUnit->GetPlayer() == inUnit->GetPlayer() && theUnit->GetUnitClass()->IsDropSite()
                    && theUnit->CanTake(inUnit, 3)) {
                    theOther = theUnit;
                    break;
                }
            }
            if (theOther != NULL)
                theTile.SetXY(theOther->GetTileX(), theOther->GetTileY());
            else
                theTile.SetXY(inUnit->GetTileX(), inUnit->GetTileY());
            theTarget = U2DPoint<long>(inResource->GetTileX(), inResource->GetTileY());
            if (theTile != theTarget) {
                theTile.GetLineTiles(theTarget, 1.0f, theTiles);
                for (unsigned long j = 0; j < theTiles.size() && theFound == NULL; j++) {
                    long theX = theTiles[j].mX;
                    long theY = (theTiles.begin() + j)->mY;
                    EEMapTile* theMapTile = ::gWorld.GetMap()->GetTile(theX, theY);
                    if (theMapTile == NULL)
                        continue;
                    for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode != NULL && theFound == NULL;
                         theNode = theNode->mNext) {
                        EEUnit* theTree = theNode->mUnit;
                        if (theTree != NULL && theTree->mResourceType == kResourceWood && inUnit->CanGather(theTree, theX, theY, false, false)
                            && theCitizen->HasGatherPoint(inUnit, theTree)
                            && gContinentManager.GetForestIDAt(U2DPoint<long>(theX, theY)) == theForestID)
                            theFound = theTree;
                    }
                }
            }
        }
    } else if (inResource->GetUnitClass()->mFamily != kUnitFamilyAnimal)
        inUnit->SetGatheredResource(inResource);
    return theFound;
}

}  // namespace

namespace {

// where a body's file calls and text go
struct EEGoalContext {
    FSFile* mFile;
    UWideString* mText;
    FILE* mDump;
};

uint64_t Handle(const void* inObject)
{
    return reinterpret_cast<uintptr_t>(inObject);
}

template <class T> T* ObjectOf(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

void PointOut(EeUnitsGoalIo* ioArgs, const GE3DPoint& inPoint)
{
    ioArgs->f[0] = inPoint.GetX();
    ioArgs->f[1] = inPoint.GetY();
    ioArgs->f[2] = inPoint.GetZ();
}

void PointIn(const EeUnitsGoalIo& inArgs, GE3DPoint& outLocation)
{
    outLocation.SetX(inArgs.f[0]);
    outLocation.SetY(inArgs.f[1]);
    outLocation.SetZ(inArgs.f[2]);
}

// the tiles of an argument block (i[2k], i[2k + 1]) in and out
void TilesIn(const EeUnitsGoalIo* inArgs, U2DPoint<long>* outTiles, int inCount)
{
    for (int k = 0; k < inCount; k++)
        outTiles[k].SetXY((long)inArgs->i[2 * k], (long)inArgs->i[2 * k + 1]);
}

void TilesOut(EeUnitsGoalIo* ioArgs, const U2DPoint<long>* inTiles, int inCount)
{
    for (int k = 0; k < inCount; k++) {
        ioArgs->i[2 * k] = inTiles[k].mX;
        ioArgs->i[2 * k + 1] = inTiles[k].mY;
    }
}

// a goal body's float argument from its bits
float FloatOf(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, sizeof(theValue));
    return theValue;
}

// FSFile::Read of a T into an argument (its bits for a float)
template <class T> long ReadInto(FSFile* inFile, int64_t& ioValue)
{
    T theValue = (T)ioValue;
    long theResult = inFile->Read(theValue);
    ioValue = (int64_t)theValue;
    return theResult;
}

long ReadFloatInto(FSFile* inFile, int64_t& ioValue)
{
    float theValue = FloatOf(ioValue);
    long theResult = inFile->Read(theValue);
    uint32_t theBits;
    memcpy(&theBits, &theValue, sizeof(theBits));
    ioValue = theBits;
    return theResult;
}

// the tiles a split operation hands in place (goal_split.rs: `io.text` names its buffer of U2DPoint<long>)
static_assert(sizeof(U2DPoint<long>) == 2 * sizeof(long), "a tile is two longs (goal_split.rs's c_long pairs)");
U2DPoint<long>* TilesAt(EeUnitsGoalIo* ioArgs)
{
    return reinterpret_cast<U2DPoint<long>*>(const_cast<uint8_t*>(ioArgs->text));
}

// the units of an argument block's handle list
std::vector<EEUnit*> UnitsIn(const EeUnitsGoalIo* inArgs)
{
    std::vector<EEUnit*> theUnits;
    for (size_t k = 0; k < inArgs->list_len; k++)
        theUnits.push_back(ObjectOf<EEUnit>(inArgs->list[k]));
    return theUnits;
}

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
    EEGoalContext* theContext = static_cast<EEGoalContext*>(inContext);
    EEUnit* theUnit = ObjectOf<EEUnit>(inA);
    EEComplexUnit* theComplex = ObjectOf<EEComplexUnit>(inA);
    EECalamityClass* theClass = ObjectOf<EECalamityClass>(inA);
    GE3DPoint thePoint(ioArgs->f[0], ioArgs->f[1], ioArgs->f[2]);
    switch (inOp) {
    case EE_UNITS_GOAL_HOST_REFERENCE:   // (a patrol group's: rust_goal.rs, a Rust object)
        theUnit->Reference();
        return 0;
    case EE_UNITS_GOAL_HOST_DEREFERENCE:
        theUnit->Dereference();
        return 0;
    case EE_UNITS_GOAL_HOST_X_START_ACTION:
        theComplex->StartAction((unsigned long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_COMPLEX: return Handle(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_X_FIND_PLAIN: return Handle(static_cast<EEUnit*>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_X_TELL_GROUP:
        theComplex->mActiveGroup->TellGroup(theComplex, thePoint);
        return 0;
    case EE_UNITS_GOAL_HOST_DROP_PATH:
        theComplex->DropPath();
        return 0;
    case EE_UNITS_GOAL_HOST_FILE_OPEN:
        return theContext->mFile->GetIsOpen() ? 1 : 0;
    case EE_UNITS_GOAL_HOST_X_WRITE_U32: return theContext->mFile->Write((uint32_t)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_WRITE_I32: return theContext->mFile->Write((int32_t)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_WRITE_F32: return theContext->mFile->Write(FloatOf(ioArgs->i[1]));
    case EE_UNITS_GOAL_HOST_X_WRITE_BOOL: return theContext->mFile->Write(ioArgs->i[1] != 0);
    case EE_UNITS_GOAL_HOST_X_READ_U32: return ReadInto<uint32_t>(theContext->mFile, ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_READ_I32: return ReadInto<int32_t>(theContext->mFile, ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_READ_F32: return ReadFloatInto(theContext->mFile, ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_READ_BOOL: return ReadInto<bool>(theContext->mFile, ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_HAS_TEXT: return theContext->mText != NULL;
    case EE_UNITS_GOAL_HOST_X_APPEND_TEXT:
        *theContext->mText += std::string((const char*)ioArgs->text, ioArgs->len).c_str();
        return 0;
    case EE_UNITS_GOAL_HOST_X_APPEND_DUMP:
        fputs(std::string((const char*)ioArgs->text, ioArgs->len).c_str(), theContext->mDump);
        return 0;
    case EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME:
        *theContext->mText += (const char*)theUnit->mUnitClass->mName;
        return 0;

    // the patrol's (the groups' own calls are Rust's: rust_goal.rs)
    case EE_UNITS_GOAL_HOST_PLAYER_GROUP:
        return Handle(theComplex->GetPlayer()->GetPatrolGroup((unsigned long)inB));

    // the cast calamity's
    case EE_UNITS_GOAL_HOST_X_PLAYER_TECH: return Handle(theComplex->mPlayer->mTechnologies->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_NODE_CALAMITY: return ObjectOf<EETechTreeNode>(inA)->mData->mCalamity;
    case EE_UNITS_GOAL_HOST_X_CALAMITY_CLASS_OF:
        return Handle(EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_CALAMITY_PLACE:
        theClass->Place(ObjectOf<EEComplexUnit>(inB), (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_CALAMITY_TARGET_KIND: return theClass->GetTargetKind();
    case EE_UNITS_GOAL_HOST_X_CALAMITY_COST: return theClass->GetCost();
    case EE_UNITS_GOAL_HOST_X_CALAMITY_RANGE: return theClass->mRange;
    case EE_UNITS_GOAL_HOST_X_UNIT_PLAYER_ID: return theComplex->mPlayer->GetPlayerID();
    case EE_UNITS_GOAL_HOST_X_CREATE_UNIT_CALAMITY:
        return EECalamityManager::GetInstance()->CreateUnitCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1],
                                                                     (unsigned long)ioArgs->i[2], (unsigned long)ioArgs->i[3]);
    case EE_UNITS_GOAL_HOST_X_CREATE_LOCATION_CALAMITY:
        return EECalamityManager::GetInstance()->CreateLocationCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1],
                                                                         (long)ioArgs->i[2], (long)ioArgs->i[3],
                                                                         (unsigned long)ioArgs->i[4]);
    case EE_UNITS_GOAL_HOST_X_CREATE_GLOBAL_CALAMITY:
        return EECalamityManager::GetInstance()->CreateGlobalCalamity((unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1],
                                                                       (unsigned long)ioArgs->i[3]);
    case EE_UNITS_GOAL_HOST_RESET_SAVED_GROUP:
        theComplex->mPlayer->mSavedGroups.Reset(6, 1);
        return 0;
    case EE_UNITS_GOAL_HOST_CAN_CAST_AT: {
            GE3DPoint theLocation;
            return theClass->EECalamityClass::CanCastAt(ObjectOf<EEComplexUnit>(inB), theLocation,
                                                        (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1],
                                                        true, NULL) == 0;
        }
    case EE_UNITS_GOAL_HOST_FACES_TARGET:
        return theComplex->GetUnitClass()->FacesTarget(theComplex, ObjectOf<EEUnit>(inB), false);
    case EE_UNITS_GOAL_HOST_FACES_POINT:
        return theComplex->GetUnitClass()->FacesPoint(theComplex, thePoint, false);
    case EE_UNITS_GOAL_HOST_X_IN_CONTACT_RANGE: return EEIsInContactRange(theComplex, ObjectOf<EEUnit>(inB), ioArgs->f[0]);
    // the explore goal's
    case EE_UNITS_GOAL_HOST_X_PATH_AREA_AT: return Handle(gContinentManager.GetPathAreaAt(theComplex));
    case EE_UNITS_GOAL_HOST_X_AREA_TOP: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mTop;
    case EE_UNITS_GOAL_HOST_X_AREA_BOTTOM: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mBottom;
    case EE_UNITS_GOAL_HOST_X_AREA_LEFT: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mLeft;
    case EE_UNITS_GOAL_HOST_X_AREA_RIGHT: return ObjectOf<EEAIPathArea>(inA)->GetHullBounds().mRight;
    case EE_UNITS_GOAL_HOST_X_MAP_TILE: return Handle(::gWorld.mMap->GetTile((long)ioArgs->i[0], (long)ioArgs->i[1]));
    case EE_UNITS_GOAL_HOST_X_EDGE_TILE: return ::gWorld.mMap->IsEdgeTile((long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TILE_VISIBLE: return ObjectOf<EEMapTile>(inB)->IsVisibleTo(theComplex->mPlayer);
    case EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS: return theComplex->mUnitClass->mDomains;
    case EE_UNITS_GOAL_HOST_X_SPACE_MAP: return ::gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    case EE_UNITS_GOAL_HOST_X_CAN_MOVE_ON_TERRAIN: return theComplex->mUnitClass->CanMoveOnTerrain(theComplex, (long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_PATH_REACHABLE: {
            EECPASGFlags theFlags;
            theComplex->mUnitClass->GetTerrainFlags(theComplex, theFlags);
            EETileCoordinate theFrom;
            theFrom.mX = (long)ioArgs->i[0];
            theFrom.mY = (long)ioArgs->i[1];
            EETileCoordinate theTo;
            theTo.mX = (long)ioArgs->i[2];
            theTo.mY = (long)ioArgs->i[3];
            EEAutoPathRequest* thePath =
                gPathManager.RequestPath(theComplex->mPlayer->GetPlayerID(), theFrom, theTo, &theFlags, 1, 10, NULL, 1);
            bool theReachable = false;
            if (thePath->mPath.size() > 0) {
                EETileCoordinate theLast = thePath->mPath.back();
                EEAIPathSubSector* theFirst = gContinentManager.GetPathSubSectorAt(theLast.GetX(), theLast.mY);
                EEAIPathSubSector* theSecond = gContinentManager.GetPathSubSectorAt(theTo.GetX(), theTo.mY);
                theReachable = theFirst == theSecond;
                if (!theReachable && theFirst != NULL && theSecond != NULL && theFirst->IsNeighbor(theSecond))
                    theReachable = theFirst->AreLinksBlocked(theSecond, theFlags);
            }
            thePath->Dereference();
            return theReachable;
        }
    case EE_UNITS_GOAL_HOST_X_MAP_WIDTH: return gMapWidth;
    case EE_UNITS_GOAL_HOST_X_MAP_HEIGHT: return gMapHeight;
    case EE_UNITS_GOAL_HOST_X_MAP_L195614: return ::gWorld.mMap->mL195614;
    case EE_UNITS_GOAL_HOST_X_MAP_L195610: return ::gWorld.mMap->mL195610;
    case EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE: return theComplex->mUnitClass->mUnitType;
    case EE_UNITS_GOAL_HOST_X_CONTINENT_AT: return Handle(static_cast<EEAIArea*>(gContinentManager.GetContinentAt(theComplex)));
    case EE_UNITS_GOAL_HOST_X_RANDOM_CONTINENT:
        return Handle(static_cast<EEAIArea*>(static_cast<EEAIContinent*>(
            gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom))));
    case EE_UNITS_GOAL_HOST_X_AREA_RANDOM_POINT:
        LongPairOut(ioArgs, ObjectOf<EEAIArea>(inA)->GetRandomPoint());
        return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_NEARBY_TARGET: return Handle(EEFindNearbyTarget(theComplex));
    case EE_UNITS_GOAL_HOST_X_NOTICE_FOREIGN_UNIT:
        theComplex->mPlayer->NoticeForeignUnit(ObjectOf<EEUnit>(inB));
        return 0;
    // the unload transport's
    case EE_UNITS_GOAL_HOST_X_IS_TRANSPORT:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass()) != NULL;
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_UNLOAD_AT:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
            ->CanUnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TILE_FREE: return EEIsTileFree(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_UNLOAD_TILES:
        EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
            ->FindUnloadTiles(theComplex, &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2], &TilesAt(ioArgs)[3]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_AIRPLANE_UNLOAD_TILES:
        EEUnitClassOfKind<EE_CLASS_KIND_AIRPLANE>(theComplex->GetUnitClass())
            ->FindUnloadTiles(theComplex, &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2], &TilesAt(ioArgs)[3]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_CAN_UNLOAD_NEAR:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
            ->CanUnloadNear(theComplex, theComplex->GetCargo(0), &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1], &TilesAt(ioArgs)[2]);
    case EE_UNITS_GOAL_HOST_FIRE_CARGO:
        theComplex->mUnitClass->FireShot(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_UNLOAD_AT:
        EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
            ->UnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND: return gSpaceUnloadSoundID;
    case EE_UNITS_GOAL_HOST_X_SOUND_GET: return Handle(EESoundObject::Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS:
        theComplex->mUnitClass->GetSoundParameters(theComplex, ObjectOf<EESoundObject>(inB), &ioArgs->f[0], &ioArgs->f[1], &ioArgs->f[2]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SOUND_PLAY:
        ObjectOf<EESoundObject>(inA)->Play(ioArgs->f[0], ioArgs->f[1], ioArgs->f[2]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SOUND_PLAY_PLAIN:
        ObjectOf<EESoundObject>(inA)->Play();
        return 0;

    // the load goals' (the unit and class reads give zeros for no unit; the class's are Rust's views)
    case EE_UNITS_GOAL_HOST_X_NO_WAYPOINTS: return theComplex->mWaypoints.empty();
    case EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_LOAD:
        return EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())->CanLoad(theComplex, ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_BOARDING: {
            EEComplexUnit* theContainer = ObjectOf<EEComplexUnit>(inB);
            GE3DPoint theOut(ioArgs->f[3], ioArgs->f[4], ioArgs->f[5]);
            EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theContainer->GetUnitClass())
                ->FindBoardingLocation(theComplex, theContainer, thePoint, theOut);
            ioArgs->f[3] = theOut.GetX();
            ioArgs->f[4] = theOut.GetY();
            ioArgs->f[5] = theOut.GetZ();
            return 0;
        }
    case EE_UNITS_GOAL_HOST_X_FIND_RENDEZVOUS: {
            std::vector<EEComplexUnit*> theUnits;
            for (size_t k = 0; k < ioArgs->list_len; k++)
                theUnits.push_back(ObjectOf<EEComplexUnit>(ioArgs->list[k]));
            EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
                ->FindRendezvousTiles(&theUnits, theComplex, &TilesAt(ioArgs)[0], &TilesAt(ioArgs)[1]);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_X_TECH_TREE_GET: return Handle(theComplex->mPlayer->GetTechTree()->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS:
        return EEEvent::GetEventEffect(EEEvent::Get(ObjectOf<EETechTreeNode>(inA)->mData->mResearchedEvent)->GetEffect(0))->mNewUnitClass;
    case EE_UNITS_GOAL_HOST_X_CLASS_TABLE: return Handle(gUnitClassTable[(long)ioArgs->i[0]]);
    case EE_UNITS_GOAL_HOST_X_CLASS_TECHNOLOGY: return ObjectOf<EEUnitClass>(inA)->GetTechnology();
    case EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE: return ObjectOf<EETechTreeNode>(inA)->GetIsAvailable();
    case EE_UNITS_GOAL_HOST_CLEAR_GOALS:
        theComplex->ClearGoals((long)inB);
        return 0;
    case EE_UNITS_GOAL_HOST_X_CAN_TAKE: return ObjectOf<EEComplexUnit>(inB)->CanTake(theComplex, (unsigned long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_GARRISON_UNIT:
        return theComplex->GarrisonUnit(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_LOAD_CARGO:
        return theComplex->LoadCargo(ObjectOf<EEComplexUnit>(inB), 0, 0);
    case EE_UNITS_GOAL_HOST_HAS_CARGO:
        return theComplex->HasCargo(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_X_REPLACE_UNIT:
        EEReplaceUnit(theComplex, theComplex->mPlayer->mPlayerID, ObjectOf<EEUnitClass>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_GENERATE:
        ioArgs->f[0] = gPhysics->mRandom.Generate();
        return 0;
    // the build/repair goal's
    // (EE_UNITS_GOAL_HOST_BUILD_UNIT's reads are Rust's views: rust_goal.rs build_unit_fields)
    case EE_UNITS_GOAL_HOST_X_NODE_ID: return ObjectOf<EETechTreeNode>(inA)->mData->mID;
    case EE_UNITS_GOAL_HOST_X_NODE_KIND: return ObjectOf<EETechTreeNode>(inA)->mData->mKind;
    case EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME: return ObjectOf<EETechTreeNode>(inA)->GetBuildTime();
    case EE_UNITS_GOAL_HOST_IN_WORK_REACH:
        return EEIsInWorkReach(theComplex, ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_SURROUNDING_TILES: {
            std::vector<EETileCoordinate> theTiles;
            theComplex->mUnitClass->GetSurroundingTiles(theComplex, theTiles, 1, 0);
            for (size_t i = 0; i < theTiles.size(); i++)
                ee_units_goal_push_tile(ioArgs->sink, (int32_t)theTiles[i].GetX(), (int32_t)theTiles[i].GetY());
            return 0;
        }
    case EE_UNITS_GOAL_HOST_X_UNIT_PLAYER: return Handle(theComplex->mPlayer);
    case EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH: return ObjectOf<EEPlayer>(inA)->mTechnologies != NULL;
    case EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS:
        ObjectOf<EEPlayer>(inA)->mTechnologies->Research(ObjectOf<EEComplexUnit>(inB)->mUnitClass->GetTechnology(), NULL);
        return 0;
    case EE_UNITS_GOAL_HOST_X_CHECK_BUTTONS:
        ObjectOf<EEPlayer>(inA)->CheckButtonsPreloaded((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SET_BUILT:
        theComplex->SetBuilt(1, 1, 1);
        return 0;
    case EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE:
        return ObjectOf<EEPlayer>(inA)->mTechnologies->Get((long)ioArgs->i[0])->GetIsAvailable();
    case EE_UNITS_GOAL_HOST_X_SPACE_WALL:
        EECreateSpaceWallEnergyWall(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER: return Handle(::gWorld.GetCurrentPlayer());
    case EE_UNITS_GOAL_HOST_X_CREATION_SOUND: return Handle(theComplex->mStats->mCreationSound);
    case EE_UNITS_GOAL_HOST_X_PLAY_SAMPLE:
        gSoundManager->Play(ObjectOf<EESoundObject>(inA)->mSampleID, ObjectOf<EESoundObject>(inA)->mVolume, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM: return Handle(gUIManager->GetMainGameForm());
    case EE_UNITS_GOAL_HOST_X_REFRESH_GROUP:
        gUIManager->GetMainGameForm()->mRefreshGroup = true;
        return 0;
    case EE_UNITS_GOAL_HOST_PLAY_EFFECT:
        theComplex->PlayEffect((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_FACE_TARGET:
        theComplex->FaceTarget(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_X_NEXT_PART: return Handle(theComplex->mNextPart);
    case EE_UNITS_GOAL_HOST_X_FULL_HIT_POINTS:
        theComplex->SetHitPoints(theComplex->mStats->mMaxHitPoints);
        return 0;
    case EE_UNITS_GOAL_HOST_X_KILL:
        theComplex->Kill(0, 0, 0);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SCALED_COST:
        return theComplex->GetPlayer()->ScaleCost(ObjectOf<EETechTreeNode>(inB)->GetCost((unsigned long)ioArgs->i[0], 0x20, 1.0f));
    case EE_UNITS_GOAL_HOST_X_ADD_RESOURCE:
        theComplex->GetPlayer()->AddResource((unsigned long)ioArgs->i[0], (long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_GATE_SIDE_PART:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theComplex->mUnitClass)->IsSidePart(theComplex);
    case EE_UNITS_GOAL_HOST_X_ADD_EVENT:
        ObjectOf<EEPlayer>(inA)->mEvents.AddEvent(U2DPoint<long>(ObjectOf<EEUnit>(inB)->mTileX, ObjectOf<EEUnit>(inB)->mTileY), 2,
                                                  ObjectOf<EEPlayer>(inA), (long)ioArgs->i[0], false);
        return 0;
    case EE_UNITS_GOAL_HOST_NEAREST_SITE:
        return Handle(theComplex->mPlayer->GetNearestConstructionSite(theComplex));
    case EE_UNITS_GOAL_HOST_CAN_PRODUCE:
        return theUnit->mPlayer->CanProduce(ObjectOf<EEUnit>(inB)->mUnitClass->GetTechnology(), 0x20, 1, true, 3, NULL);
    case EE_UNITS_GOAL_HOST_CAN_AFFORD_REPAIR:
        return theComplex->mPlayer->CanAffordRepair(ObjectOf<EEUnit>(inB), theComplex, (unsigned long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_PAY:
        ObjectOf<EEPlayer>(inA)->Pay(ObjectOf<EETechTreeNode>(inB), 0x20, ioArgs->f[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_HIT_POINTS:
        theComplex->SetHitPoints((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_X_WONDER_CHAT:
        WonderStartedChat(theComplex, ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA:
        theComplex->mPlayer->LoadUnitClassMedia(ObjectOf<EEUnit>(inB)->mUnitClass->GetUnitType());
        return 0;
    case EE_UNITS_GOAL_HOST_X_LOAD_MEDIA:
        theComplex->LoadMedia(false);
        return 0;
    case EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS:
        gDataPreLoading->PostUnloadUnitClass(theComplex->mPlayer->mPlayerID, theComplex->mUnitClass->GetUnitType());
        return 0;
    case EE_UNITS_GOAL_HOST_X_PART_FINISHED:
        theComplex->PartFinished();
        return 0;
    // the gather goal's
    case EE_UNITS_GOAL_HOST_NEXT_RESOURCE: {
            std::vector<EEUnit*> thePassed = UnitsIn(ioArgs);
            return Handle(theComplex->mUnitClass->FindNextResource(theComplex, ObjectOf<EEUnit>(inB), (long)ioArgs->i[0], &thePassed));
        }
    case EE_UNITS_GOAL_HOST_FIND_STORAGE:
        return Handle(theComplex->mUnitClass->FindStorage(theComplex, ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_X_IS_HUMAN: return theComplex->mPlayer->IsHuman();
    case EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER: return Handle(theComplex->mPlayer->mComputerPlayer);
    case EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP: return Handle(ObjectOf<EEComputerPlayer>(inA)->mActiveGroup);
    case EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS: return ObjectOf<EESelectedGroup>(inA)->GetNumUnits();
    case EE_UNITS_GOAL_HOST_X_GROUP_CLEAR:
        ObjectOf<EESelectedGroup>(inA)->Clear();
        return 0;
    case EE_UNITS_GOAL_HOST_X_CITIZEN_FIND_STORAGE:
        return Handle(EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(theComplex->GetUnitClass())->FindStorage(theComplex, ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_NEXT_TREE:
        return Handle(EEGatherNextTree(theComplex, ObjectOf<EEUnit>(inB)));
    case EE_UNITS_GOAL_HOST_X_FIND_ACTION: return Handle(theComplex->mUnitClass->FindAction((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_X_SET_ATTACK_GOAL:
        theComplex->SetGoal(EEUGRust::NewAttackUnit(theComplex, ObjectOf<EEUnit>(inB), ioArgs->i[0] != 0, false), 1, 0);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_GATHERED:
        theComplex->SetGatheredResource(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_LAST_WAYPOINT:
        PointOut(ioArgs, GE3DPoint(theComplex->mWaypoints.back().mLocation));
        return 0;
    case EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST: return Handle(EEFindLoadedGhost((unsigned long)inA));
    case EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST_UNIT: return Handle(EEFindLoadedGhostUnit((unsigned long)inA));
    case EE_UNITS_GOAL_HOST_LINE_TO_UNIT_BLOCKED: {
            EEUnit* theBlocker;
            long theArg4;
            long theArg5;
            return EEIsLineToUnitBlocked(theComplex, ObjectOf<EEUnit>(inB), &theBlocker, &theArg4, &theArg5);
        }
    // the gathering's (world.rs: BeginGathering, Work, SearchNavalResource)
    case EE_UNITS_GOAL_HOST_TAKE_HIT:
        theUnit->TakeHit(ObjectOf<EEComplexUnit>(inB), UMFloatToLong(ObjectOf<EEComplexUnit>(inB)->mStats->mAttack), 0, 1);
        return 0;
    case EE_UNITS_GOAL_HOST_X_DEPLETE_ONE:
        theUnit->DepleteResource(1);
        return 0;
    case EE_UNITS_GOAL_HOST_X_ADD_RESOURCE_AMOUNT:
        theComplex->AddResourceAmount(1);
        return 0;
    case EE_UNITS_GOAL_HOST_X_SET_LAST_RESOURCE:
        theComplex->SetLastResource(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_REGROW:
        theUnit->RegrowForest(ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_X_GATHER_TIME: return theComplex->mStats->mGatherTimes[ioArgs->i[0]];
    case EE_UNITS_GOAL_HOST_X_WORK_TIME_FACTOR:
        ioArgs->f[0] = EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->mWorkTimeFactor;
        return 0;
    case EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER:
        theComplex->SetWorkTimer((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_HIGH_TECH_TOOLS:
        return EEGetRules().Epoch(theComplex->mPlayer->mTechnologies->mCurrentEpoch).mHighTechTools;
    case EE_UNITS_GOAL_HOST_RELEASE_FIRE:
        theComplex->ReleaseFireEffect();
        return 0;
    case EE_UNITS_GOAL_HOST_MINING_BEAM: {
            GE3DPoint thePosition;
            ObjectOf<EEUnit>(inB)->GetPosition(thePosition);
            EEEffectHandle theHandle;
            ::gWorld.GetEffectManager()->CreateEffect(theComplex, thePosition, EEGraphicEffect::Get(kGraphicEffectSpaceCitizenMiningBeam), &theHandle, 0);
            theComplex->mFireEffect = theHandle;
            return 0;
        }
    case EE_UNITS_GOAL_HOST_RESOURCE_OF_KIND: {   // the class's search (its UCBD note): the gather world's, gather_world.rs
            std::vector<EEUnit*> theTaken = UnitsIn(ioArgs);
            return Handle(theComplex->mUnitClass->FindResourceOfKind(theComplex, ObjectOf<EEUnit>(inB), ObjectOf<EEUnit>((uint64_t)ioArgs->i[3]),
                (long)ioArgs->i[0], (unsigned long)ioArgs->i[1], &theTaken, ioArgs->i[2] != 0));
        }
    // the location goal's
    // the world searches' queries (world.rs): unit lists into the sink, tiles as ee_units_goal_push_tile
    case EE_UNITS_GOAL_HOST_ACTION_CHANGED:
        theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    default:   // the combat goals' operations (EEUGRustCombat.cpp)
        return EECombatGoalHostOp(inOp, inA, inB, ioArgs);
    }
    return 0;
}

// one body (EE_UNITS_GOAL_*) of inGoal; inA, inB, ioArgs as it takes them; the file or the text it writes
int64_t Goal(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0,
             EeUnitsGoalIo* ioArgs = NULL, FSFile* inFile = NULL, UWideString* inText = NULL, FILE* inDump = NULL)
{
    EEGoalContext theContext = {inFile, inText, inDump};
    EeUnitsGoalHost theHost = {&theContext, GoalOp};
    EeUnitsGoalIo theArgs = {};
    return ee_units_rust_goal(inOperation, inGoal->mKind, Handle(inGoal), inA, inB,
                              ioArgs != NULL ? ioArgs : &theArgs, &theHost);
}

}  // namespace

int64_t EEUGRustGoal(int inOperation, const EEUGRust* inGoal, uint64_t inA, uint64_t inB, EeUnitsGoalIo* ioArgs,
                     FSFile* inFile)
{
    return Goal(inOperation, inGoal, inA, inB, ioArgs, inFile);
}

// the goal's members are the Rust object's head (ee_units goal_obj.rs)
static_assert(sizeof(EEUnitGoal) == sizeof(EeUnitsGoalBase) && offsetof(EEUnitGoal, mKind) == offsetof(EeUnitsGoalBase, kind)
                  && offsetof(EEUnitGoal, mAction) == offsetof(EeUnitsGoalBase, action)
                  && offsetof(EEUnitGoal, mStartTime) == offsetof(EeUnitsGoalBase, start_time)
                  && offsetof(EEUnitGoal, mCreationTime) == offsetof(EeUnitsGoalBase, creation_time)
                  && offsetof(EEUnitGoal, mID) == offsetof(EeUnitsGoalBase, id)
                  && offsetof(EEUnitGoal, mPathAttempts) == offsetof(EeUnitsGoalBase, path_attempts)
                  && offsetof(EEUnitGoal, mB1C) == offsetof(EeUnitsGoalBase, b1c)
                  && offsetof(EEUnitGoal, mB1D) == offsetof(EeUnitsGoalBase, b1d),
              "EEUnitGoal is EeUnitsGoalBase");

// the world host of the goals' context-free calls (EEUnitGoal::Call)
static const int sGoalHostRegistered = (ee_units_goal_register_host(GoalOp), 0);

// 0x23c3e6: an empty goal of a kind for loading, giving the drawn goal id back (NULL for an unknown kind)
EEUnitGoal* EEUnitGoal::CreateGoal(unsigned char inKind)
{
    EEUnitGoal* theGoal = NULL;
    switch (inKind) {
    case kGoalAttackGround: theGoal = NewAttackGround(GE3DPoint(), false); break;
    case kGoalAttackUnit: theGoal = NewAttackUnitEmpty(false); break;
    case kGoalBuildRepair: theGoal = NewBuildRepair(false); break;
    case kGoalCastCalamity: theGoal = NewCastCalamity(false); break;
    case kGoalExplore: theGoal = NewExplore(false); break;
    case kGoalGather: theGoal = NewGather(false); break;
    case kGoalLoadGarrison:
    case kGoalLoadTransport: theGoal = NewLoad(inKind, false); break;
    case kGoalLocation: theGoal = NewLocation(false, false, false, false); break;
    case kGoalPatrol: theGoal = NewPatrol(false); break;
    case kGoalUnloadTransport: theGoal = NewUnloadTransport(false); break;
    case kGoalHealUnit: theGoal = NewHealUnit(false); break;
    }
    if (theGoal != NULL)
        theGoal->mID = ee_units_goal_counter_back();
    return theGoal;
}

// the location goal (location.rs); its accessors read and write the record directly
namespace {

EEUGRust* NewLocationGoal(const GE3DPoint& inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace, float inYaw,
                          bool inIsAutoWaypoint, unsigned char inGroupPathState)
{
    EEUGRust* theGoal = EEUGRust::Make(kGoalLocation, inArg);
    EeUnitsGoalIo theArgs = {{inIsForcedMove, inShouldFace, inIsAutoWaypoint, inGroupPathState}};
    PointOut(&theArgs, inLocation);
    theArgs.f[3] = inYaw;
    Goal(EE_UNITS_GOAL_NEW_LOCATION, theGoal, 0, 0, &theArgs);
    return theGoal;
}

EeUnitsLocationGoal LocationOf(const EEUGRust* inGoal)
{
    EeUnitsLocationGoal theRecord;
    ee_units_location_goal_get(Handle(inGoal), &theRecord);
    return theRecord;
}

void LocationSet(EEUGRust* inGoal, const EeUnitsLocationGoal& inRecord)
{
    ee_units_location_goal_set(Handle(inGoal), &inRecord);
}

}  // namespace

EEUGRust* EEUGRust::NewLocation(float inX, float inY, float inZ, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
{
    return NewLocationGoal(GE3DPoint(inX, inY, inZ), inArg, inIsForcedMove, inShouldFace, inYaw, inIsAutoWaypoint,
                           inGroupPathState);
}

EEUGRust* EEUGRust::NewLocation(const GE2DPoint& inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
{
    return NewLocationGoal(GE3DPoint(inLocation.GetX(), inLocation.GetY(), 0.0f), inArg, inIsForcedMove, inShouldFace,
                           inYaw, inIsAutoWaypoint, inGroupPathState);
}

EEUGRust* EEUGRust::NewLocation(const GE3DPoint* inLocation, bool inArg, bool inIsForcedMove, bool inShouldFace,
                                float inYaw, bool inIsAutoWaypoint, unsigned char inGroupPathState)
{
    return NewLocationGoal(*inLocation, inArg, inIsForcedMove, inShouldFace, inYaw, inIsAutoWaypoint,
                           inGroupPathState);
}

EEUGRust* EEUGRust::NewLocation(bool inArg, bool inIsForcedMove, bool inIsAutoWaypoint, unsigned char inGroupPathState)
{
    return NewLocationGoal(GE3DPoint(), inArg, inIsForcedMove, false, 0.0f, inIsAutoWaypoint, inGroupPathState);
}

GE3DPoint EEUGRust::GetLocation() const
{
    EeUnitsLocationGoal theRecord = LocationOf(this);
    return GE3DPoint(theRecord.location[0], theRecord.location[1], theRecord.location[2]);
}

void EEUGRust::SetLocation(const GE3DPoint& inLocation)
{
    EeUnitsLocationGoal theRecord = LocationOf(this);
    theRecord.location[0] = inLocation.GetX();
    theRecord.location[1] = inLocation.GetY();
    theRecord.location[2] = inLocation.GetZ();
    LocationSet(this, theRecord);
}

void EEUGRust::GetCalamityTargetLocation(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_CALAMITY_TARGET_LOCATION, this, 0, 0, &theArgs);
    PointIn(theArgs, outLocation);
}

void EEUGRust::GetExploreTileCenter(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_EXPLORE_TILE, this, 0, 0, &theArgs);
    outLocation.SetX((float)(long)theArgs.i[0] + 0.5f);
    outLocation.SetY((float)(long)theArgs.i[1] + 0.5f);
}

EEUGRust* EEUGRust::NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                            bool inArg)
{
    std::sort(inUnits.begin(), inUnits.end(),
              [](EEComplexUnit* inFirst, EEComplexUnit* inSecond) { return inFirst->mUnitID < inSecond->mUnitID; });
    inUnits.erase(std::unique(inUnits.begin(), inUnits.end()), inUnits.end());
    EEUGRust* theGoal = EEUGRust::Make(inKind, inArg);
    std::vector<uint64_t> theUnits;
    for (size_t i = 0; i < inUnits.size(); i++)
        theUnits.push_back(Handle(inUnits[i]));
    EeUnitsGoalIo theArgs = {};
    theArgs.list = theUnits.data();
    theArgs.list_len = theUnits.size();
    Goal(EE_UNITS_GOAL_NEW_LOAD, theGoal, Handle(inContainer), 0, &theArgs);
    return theGoal;
}

void EEUGRust::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    PointOut(&theArgs, outLocation);
    Goal(EE_UNITS_GOAL_DESTINATION, this, Handle(inUnit), 0, &theArgs);
    PointIn(theArgs, outLocation);
}

void EEUGRust::DumpOutOfSynch(FILE* inFile)
{
    Goal(EE_UNITS_GOAL_DUMP, this, 0, 0, NULL, NULL, NULL, inFile);
}

void EEUGRust::GetDebugLine(UWideString& ioText)
{
    Goal(EE_UNITS_GOAL_DEBUG_LINE, this, 0, 0, NULL, NULL, &ioText);
}

void EEUGRust::GetGoalLocation(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    PointOut(&theArgs, outLocation);
    Goal(EE_UNITS_GOAL_LOCATION, this, 0, 0, &theArgs);
    PointIn(theArgs, outLocation);
}

long EEUGRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // the base goal's record first (but the heal goal's), then the kind's: one Rust sequence (rust_goal.rs)
    return (long)Goal(EE_UNITS_GOAL_LOAD, this, inMajorVersion, inMinorVersion, NULL, &inFile);
}

long EEUGRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return (long)Goal(EE_UNITS_GOAL_SAVE, this, inMajorVersion, inMinorVersion, NULL, &outFile);
}
