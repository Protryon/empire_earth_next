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

// The order FindGatherSpot sorts the tiles around the resource in: the farther from the gatherer first (0x18 bytes:
// the gatherer's position, the resource's tile, its path sector's id, the gatherer) (provisional name)
struct EEGatherTileFarther {
    EEGatherTileFarther(EEComplexUnit* inUnit, const EETileCoordinate& inTile) : mTile(inTile), mUnit(inUnit)
    {
        EEAIPathSector* theSector = gContinentManager.GetPathSector(inTile);
        this->mSector = theSector ? theSector->GetID() : (unsigned short)0;
        this->mPosition.SetX(inUnit->mPhysicalModel.GetLocation().GetX());
        this->mPosition.SetY(inUnit->mPhysicalModel.GetLocation().GetY());
    }
    // 0x621ddb: whether the first tile is farther from the gatherer than the second (sorting puts the nearest last)
    bool operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const
    {
        float theFirst = this->mPosition.DistanceToSquared(U2DPoint<float>((float)inFirst.GetX(), (float)inFirst.GetY()));
        float theSecond = this->mPosition.DistanceToSquared(U2DPoint<float>((float)inSecond.GetX(), (float)inSecond.GetY()));
        return theFirst > theSecond;
    }

    U2DPoint<float> mPosition;              // +0x00
    EETileCoordinate mTile;                 // +0x08
    unsigned short mSector;                 // +0x10
    EEComplexUnit* mUnit;                   // +0x14
};

// 0x6226cf FindGatherSpot's search: the resource's nearest spot in its path subsector, under the gatherer or in a
// neighbouring subsector the gatherer can enter, else any spot with room (the waiting area, at the tile's height)
bool EEGatherFindSpot(EEComplexUnit* inUnit, EEUnit* inResource, GE3DPoint& outWaitingArea)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOthers;
    EECPASGFlags theFlags;
    bool theFound = false;
    bool theHaveFlags = false;
    GE3DPoint theLocation;
    ee_units_uu_unit_tiles(1, (uint64_t)(uintptr_t)inResource, &theTiles);
    std::sort(theTiles.begin(), theTiles.end(), EEGatherTileFarther(inUnit, inResource->GetTile()));
    EEAIPathSubSector* theStart = gContinentManager.GetPathSubSectorAt(inResource);
    while (!theFound && theTiles.size() != 0) {
        EEAIPathSubSector* theSubSector =
            gContinentManager.GetPathSubSectorAt(theTiles.back().GetX(), theTiles.back().GetY());
        if (theSubSector == theStart || U2DPoint<long>(inUnit->mTileX, inUnit->GetTileY()) == theTiles.back())
            theFound = true;
        else if (theStart->IsNeighbor(theSubSector)) {
            if (!theHaveFlags) {
                inUnit->mUnitClass->GetTerrainFlags(inUnit, theFlags);
                theHaveFlags = true;
            }
            if (theFlags.mFlags[theSubSector->GetKind()])
                theFound = true;
        }
        if (theFound == true) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles.back().GetX(), theTiles.back().GetY());
            if (theTile != NULL
                && EEFindPlaceOnTile(inUnit, inUnit->GetUnitClass(), theTiles.back().GetX(), theTiles.back().GetY(), 1,
                              theTiles.back().GetX(), theTiles.back().GetY(), 1, &theLocation)) {
                outWaitingArea = theLocation;
                outWaitingArea.mZ = theTile->GetHeight();
            } else
                theFound = false;
        } else
            theOthers.push_back(theTiles.back());
        theTiles.pop_back();
    }
    if (!theFound) {
        for (unsigned long i = 0; i < theOthers.size() && !theFound; i++) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theOthers[i].mX, theOthers[i].mY);
            if (theTile == NULL
                || !EEFindPlaceOnTile(inUnit, inUnit->GetUnitClass(), theOthers[i].mX, theOthers[i].mY, 1,
                              theOthers[i].mX, theOthers[i].mY, 1, &theLocation))
                continue;
            outWaitingArea = theLocation;
            theFound = true;
            outWaitingArea.mZ = theTile->GetHeight();
        }
    }
    return theFound;
}

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
            EEGetUnitsInRadius(theCenter, inUnit->GetLineOfSight(), theUnits);
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

// the resource-accessible trigger: a land gatherer's resource on land, a naval one's on deep water, in its continent
// with a gather point
bool EEGatherAccessible(EEComplexUnit* inUnit, EEUnit* inResource)
{
    EEUnitClass* theClass = EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(inUnit->GetUnitClass());
    if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen) {
        if (inResource != NULL && inResource->mTile->mTerrainType != kTerrainDeepWater
            && gContinentManager.InSameContinent(inUnit, inResource))
            return theClass->HasGatherPoint(inUnit, inResource);
    } else if (inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen) {
        if (inResource != NULL && inResource->mTile->mTerrainType == kTerrainDeepWater
            && gContinentManager.InSameContinent(inUnit, inResource))
            return theClass->HasGatherPoint(inUnit, inResource);
    }
    return false;
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

// one operation (EE_UNITS_GOAL_HOST_*) of a body: inA, inB the handles it names, ioArgs the rest
// (the plain reads of a unit, its class, stats and player are Rust's views: rust_goal.rs read_unit)
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
    case EE_UNITS_GOAL_HOST_START_ACTION:
        theComplex->StartAction((unsigned long)ioArgs->i[0]);
        if (ioArgs->i[1] == 0)
            theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_FIND_UNIT:
        if (inB != 0)
            return Handle(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((unsigned long)inA)));
        return Handle(static_cast<EEUnit*>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_TELL_GROUP:
        if (theComplex->mActiveGroup != NULL)
            theComplex->mActiveGroup->TellGroup(theComplex, thePoint);
        return 0;
    case EE_UNITS_GOAL_HOST_DROP_PATH:
        theComplex->DropPath();
        return 0;
    case EE_UNITS_GOAL_HOST_FILE_OPEN:
        return theContext->mFile->GetIsOpen() ? 1 : 0;
    case EE_UNITS_GOAL_HOST_WRITE:
        if (ioArgs->i[0] == 0)
            return theContext->mFile->Write((uint32_t)ioArgs->i[1]);
        if (ioArgs->i[0] == 1)
            return theContext->mFile->Write((int32_t)ioArgs->i[1]);
        if (ioArgs->i[0] == 3) {
            uint32_t theBits = (uint32_t)ioArgs->i[1];
            float theValue;
            memcpy(&theValue, &theBits, sizeof(theValue));
            return theContext->mFile->Write(theValue);
        }
        return theContext->mFile->Write(ioArgs->i[1] != 0);
    case EE_UNITS_GOAL_HOST_READ: {
            long theResult;
            if (ioArgs->i[0] == 0) {
                uint32_t theValue = (uint32_t)ioArgs->i[1];
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            } else if (ioArgs->i[0] == 1) {
                int32_t theValue = (int32_t)ioArgs->i[1];
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            } else if (ioArgs->i[0] == 3) {
                uint32_t theBits = (uint32_t)ioArgs->i[1];
                float theValue;
                memcpy(&theValue, &theBits, sizeof(theValue));
                theResult = theContext->mFile->Read(theValue);
                memcpy(&theBits, &theValue, sizeof(theBits));
                ioArgs->i[1] = theBits;
            } else {
                bool theValue = ioArgs->i[1] != 0;
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            }
            return theResult;
        }
    case EE_UNITS_GOAL_HOST_APPEND: {
            std::string theText((const char*)ioArgs->text, ioArgs->len);
            if (theContext->mText != NULL)
                *theContext->mText += theText.c_str();
            else
                fputs(theText.c_str(), theContext->mDump);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME:
        *theContext->mText += (const char*)theUnit->mUnitClass->mName;
        return 0;

    // the patrol's (the groups' own calls are Rust's: rust_goal.rs)
    case EE_UNITS_GOAL_HOST_PLAYER_GROUP:
        return Handle(theComplex->GetPlayer()->GetPatrolGroup((unsigned long)inB));

    // the cast calamity's
    case EE_UNITS_GOAL_HOST_CALAMITY_CLASS: {
            EETechTreeNode* theNode = theComplex->mPlayer->mTechnologies->Get((long)ioArgs->i[0]);
            if (theNode == NULL)
                return 0;
            ioArgs->i[1] = ioArgs->i[2] = theNode->mData->mCalamity;
            return Handle(
                EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity));
        }
    case EE_UNITS_GOAL_HOST_CALAMITY_PLACE:
        theClass->Place(ObjectOf<EEComplexUnit>(inB), (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_CALAMITY_INFO:
        ioArgs->i[0] = theClass->GetTargetKind();
        ioArgs->i[1] = theClass->GetCost();
        ioArgs->i[2] = theClass->mRange;
        return 0;
    case EE_UNITS_GOAL_HOST_CREATE_CALAMITY: {
            EECalamityManager* theManager = EECalamityManager::GetInstance();
            unsigned long theCalamity = (unsigned long)ioArgs->i[1];
            unsigned long theSource = theComplex->GetUnitID();
            unsigned long thePlayer = ObjectOf<EEComplexUnit>((uint64_t)ioArgs->i[4])->mPlayer->GetPlayerID();
            if (ioArgs->i[0] == 3)
                return theManager->CreateUnitCalamity(theCalamity, theSource, ObjectOf<EEUnit>(inB)->GetUnitID(),
                                                      thePlayer);
            if (ioArgs->i[0] == 2)
                return theManager->CreateLocationCalamity(theCalamity, theSource, (long)ioArgs->i[2],
                                                          (long)ioArgs->i[3], thePlayer);
            return theManager->CreateGlobalCalamity(theCalamity, theSource, thePlayer);
        }
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
    case EE_UNITS_GOAL_HOST_UNIT_IN_SET: {
            EEUnitSet* theSet = gUnitSetManager.GetSet((long)ioArgs->i[0]);
            return theSet != NULL && theSet->Contains(theComplex->mUnitClass->mUnitType);
        }
    case EE_UNITS_GOAL_HOST_CONTACT_RANGE: {
            GE3DPoint theTarget;
            ObjectOf<EEUnit>(inB)->GetPosition(theTarget);
            theComplex->GetPosition(thePoint);
            return EEIsInContactRange(theComplex, ObjectOf<EEUnit>(inB), thePoint.DistanceToSquared(theTarget));
        }
    // the explore goal's
    case EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS: {
            EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theComplex);
            if (theArea == NULL)
                return 0;
            const U2DRectangle<long>& theBounds = theArea->GetHullBounds();
            ioArgs->i[0] = theBounds.mTop;
            ioArgs->i[1] = theBounds.mBottom;
            ioArgs->i[2] = theBounds.mLeft;
            ioArgs->i[3] = theBounds.mRight;
            return 1;
        }
    case EE_UNITS_GOAL_HOST_TILE_SCREEN: {
            EEMap* theMap = ::gWorld.mMap;
            long theX = (long)ioArgs->i[0];
            long theY = (long)ioArgs->i[1];
            EEMapTile* theTile = theMap->GetTile(theX, theY);
            if (theTile == NULL || theMap->IsEdgeTile(theX, theY) || theTile->IsVisibleTo(theComplex->mPlayer))
                return 1;
            return (theComplex->mUnitClass->mDomains & kMovementAir) != 0
                && (!theMap->mTerrainMesh.GetIsSpaceMap() || theComplex->mUnitClass->CanMoveOnTerrain(theComplex, 12));
        }
    case EE_UNITS_GOAL_HOST_CELL_TAKEN: {
            GE3DPoint theCell((float)(long)ioArgs->i[0] + 0.5f, (float)(long)ioArgs->i[1] + 0.5f, 0.0f);
            return theComplex->mActiveGroup != NULL && theComplex->mActiveGroup->IsCellTaken(theCell);
        }
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
    case EE_UNITS_GOAL_HOST_MAP_DIMENSIONS:
        ioArgs->i[0] = gMapWidth;
        ioArgs->i[1] = gMapHeight;
        ioArgs->i[2] = ::gWorld.mMap->mL195614;
        ioArgs->i[3] = ::gWorld.mMap->mL195610;
        return 0;
    case EE_UNITS_GOAL_HOST_CONTINENT_POINT: {
            EEAIArea* theArea;
            if (((theComplex->mUnitClass->mDomains & kMovementAir) == 0
                 || (::gWorld.mMap->mTerrainMesh.GetIsSpaceMap() && !theComplex->mUnitClass->CanMoveOnTerrain(theComplex, 12)))
                && theComplex->mUnitClass->mUnitType != kUnitTypeSp15SpaceFighter)
                theArea = gContinentManager.GetContinentAt(theComplex);
            else
                theArea = static_cast<EEAIContinent*>(
                    gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom));
            if (theArea == NULL)
                return 0;
            U2DPoint<long> thePoint = theArea->GetRandomPoint();
            ioArgs->i[0] = thePoint.mX;
            ioArgs->i[1] = thePoint.mY;
            return 1;
        }
    case EE_UNITS_GOAL_HOST_NOTICE_FOREIGN: {
            EEUnit* theTarget = EEFindNearbyTarget(theComplex);
            if (theTarget != NULL)
                theComplex->mPlayer->NoticeForeignUnit(theTarget);
            return 0;
        }
    // the unload transport's
    case EE_UNITS_GOAL_HOST_CAN_UNLOAD_AT: {
            EEUnitClass* theTransport = EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass());
            if (theTransport != NULL)
                return theTransport->CanUnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
            return EEIsTileFree(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
        }
    case EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES: {
            U2DPoint<long> theTiles[4];
            TilesIn(ioArgs, theTiles, 4);
            EEUnitClass* theTransport = EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass());
            if (theTransport != NULL)
                theTransport->FindUnloadTiles(theComplex, &theTiles[0], &theTiles[1], &theTiles[2], &theTiles[3]);
            else
                EEUnitClassOfKind<EE_CLASS_KIND_AIRPLANE>(theComplex->GetUnitClass())
                    ->FindUnloadTiles(theComplex, &theTiles[0], &theTiles[1], &theTiles[2], &theTiles[3]);
            TilesOut(ioArgs, theTiles, 4);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_CAN_UNLOAD_NEAR: {
            U2DPoint<long> theTiles[3];
            TilesIn(ioArgs, theTiles, 3);
            bool theResult = EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
                                 ->CanUnloadNear(theComplex, theComplex->GetCargo(0), &theTiles[0], &theTiles[1],
                                                 &theTiles[2]);
            TilesOut(ioArgs, theTiles, 3);
            return theResult;
        }
    case EE_UNITS_GOAL_HOST_FIRE_CARGO:
        theComplex->mUnitClass->FireShot(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_UNLOAD_AT:
        EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass())
            ->UnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_UNLOAD_SOUND:
        ioArgs->i[0] = ioArgs->i[0] != 0 ? gSpaceUnloadSoundID : 0x184;
        // fall through
    case EE_UNITS_GOAL_HOST_PLAY_SOUND: {
            EESoundObject* theSound = EESoundObject::Get((long)ioArgs->i[0]);
            if (theSound != NULL) {
                float theVolume;
                float thePan;
                float thePitch;
                theComplex->mUnitClass->GetSoundParameters(theComplex, theSound, &theVolume, &thePan, &thePitch);
                theSound->Play(theVolume, thePan, thePitch);
            }
            return 0;
        }

    // the load goals' (the unit and class reads give zeros for no unit; the class's are Rust's views)
    case EE_UNITS_GOAL_HOST_LOAD_UNIT:
        if (theComplex != NULL) {
            int64_t theFields[6] = {theComplex->mGarrisonCount, theComplex->mCargoRoom, theComplex->mHitPoints,
                                    theComplex->mIsDisabled, theComplex->mWaypoints.empty(), theComplex->mAnimation};
            memcpy(ioArgs->i, theFields, sizeof(theFields));
        }
        return 0;
    case EE_UNITS_GOAL_HOST_CAN_LOAD: {
            EEUnitClass* theTransport = EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass());
            return theTransport != NULL && theTransport->CanLoad(theComplex, ObjectOf<EEComplexUnit>(inB));
        }
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
    case EE_UNITS_GOAL_HOST_RENDEZVOUS: {
            std::vector<EEComplexUnit*> theUnits;
            for (size_t k = 0; k < ioArgs->list_len; k++)
                theUnits.push_back(ObjectOf<EEComplexUnit>(ioArgs->list[k]));
            U2DPoint<long> theTiles[2];
            TilesIn(ioArgs, theTiles, 2);
            EEUnitClass* theClass = EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(theComplex->GetUnitClass());
            if (ioArgs->i[4] == 0)
                theClass->FindRendezvousTiles(&theUnits, theComplex, &theTiles[0], &theTiles[1]);
            else   // the transport base's rendezvous: its own tile for both
                theTiles[0] = theTiles[1] = U2DPoint<long>(theComplex->GetTileX(), theComplex->GetTileY());
            TilesOut(ioArgs, theTiles, 2);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_GARRISON_UPGRADE: {
            EETechTreeNode* theNode = theComplex->mPlayer->GetTechTree()->Get((long)ioArgs->i[0]);
            ioArgs->i[0] = theNode != NULL;
            if (theNode == NULL)
                return 0;
            long theUpgrade = EEEvent::GetEventEffect(EEEvent::Get(theNode->mData->mResearchedEvent)->GetEffect(0))->mNewUnitClass;
            EEUnitClass* theClass = gUnitClassTable[theUpgrade];
            EETechTreeNode* theAvailable = theClass != NULL ? theComplex->mPlayer->GetTechTree()->Get(theClass->GetTechnology()) : NULL;
            ioArgs->i[1] = theUpgrade;
            ioArgs->i[2] = theAvailable != NULL && theAvailable->GetIsAvailable();
            return 0;
        }
    case EE_UNITS_GOAL_HOST_CLEAR_GOALS:
        theComplex->ClearGoals((long)inB);
        return 0;
    case EE_UNITS_GOAL_HOST_DEPOSIT:
        if (theComplex->mResourceAmount > 0 && ObjectOf<EEComplexUnit>(inB)->CanTake(theComplex, theComplex->mResourceType))
            ee_units_uu_deposit_load(inA, inB);   // uu_world.rs
        return 0;
    case EE_UNITS_GOAL_HOST_GARRISON_UNIT:
        return theComplex->GarrisonUnit(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_LOAD_CARGO:
        return theComplex->LoadCargo(ObjectOf<EEComplexUnit>(inB), 0, 0);
    case EE_UNITS_GOAL_HOST_HAS_CARGO:
        return theComplex->HasCargo(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_REPLACE_CLASS:
        if (gUnitClassTable[(long)ioArgs->i[0]] != NULL)
            EEReplaceUnit(theComplex, theComplex->mPlayer->mPlayerID, gUnitClassTable[(long)ioArgs->i[0]]);
        return 0;
    case EE_UNITS_GOAL_HOST_GENERATE:
        ioArgs->f[0] = gPhysics->mRandom.Generate();
        return 0;
    // the build/repair goal's
    // (EE_UNITS_GOAL_HOST_BUILD_UNIT's reads are Rust's views: rust_goal.rs build_unit_fields)
    case EE_UNITS_GOAL_HOST_TECH_NODE:
        return Handle(ObjectOf<EEPlayer>(inA)->GetTechTree()->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_NODE_INFO: {
            EETechTreeNode* theNode = ObjectOf<EETechTreeNode>(inA);
            if (theNode != NULL) {
                ioArgs->i[0] = theNode->mData->mID;
                ioArgs->i[1] = theNode->mData->mKind;
                ioArgs->i[2] = theNode->GetBuildTime();
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_IN_WORK_REACH:
        return EEIsInWorkReach(theComplex, ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_SURROUNDING_TILES: {
            std::vector<EETileCoordinate> theTiles;
            theComplex->mUnitClass->GetSurroundingTiles(theComplex, theTiles, 1, 0);
            for (size_t i = 0; i < theTiles.size(); i++)
                ee_units_goal_push_tile(ioArgs->sink, (int32_t)theTiles[i].GetX(), (int32_t)theTiles[i].GetY());
            return 0;
        }
    case EE_UNITS_GOAL_HOST_COMPLETE_BUILDING: {
            // FinishBuilding's completion of an unbuilt building by the worker's player
            EEPlayer* thePlayer = theComplex->mPlayer;
            EEComplexUnit* theTarget = ObjectOf<EEComplexUnit>(inB);
            if (thePlayer != NULL && thePlayer->mTechnologies != NULL)
                thePlayer->mTechnologies->Research(theTarget->mUnitClass->GetTechnology(), NULL);
            thePlayer->CheckButtonsPreloaded((long)ioArgs->i[0]);
            theTarget->SetBuilt(1, 1, 1);
            if (theTarget->mUnitClass->GetUnitType() == kUnitTypeWallSpace
                && thePlayer->mTechnologies->Get(kTechWallSpace)->GetIsAvailable())
                EECreateSpaceWallEnergyWall(theTarget);
            if (thePlayer == ::gWorld.GetCurrentPlayer()) {
                EESoundObject* theSound = theTarget->mStats->mCreationSound;
                if (theSound != NULL)
                    gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
            }
            if (gUIManager->GetMainGameForm() != NULL)
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            return 0;
        }
    case EE_UNITS_GOAL_HOST_PLAY_EFFECT:
        theComplex->PlayEffect((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_FACE_TARGET:
        theComplex->FaceTarget(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_FILL_PARTS:
        for (EEComplexUnit* thePart = theComplex->mNextPart; thePart != NULL; thePart = thePart->mNextPart)
            thePart->SetHitPoints(thePart->mStats->mMaxHitPoints);
        return 0;
    case EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION: {
            // CancelFoundation: the foundation killed, its cost refunded to the worker's player
            EEComplexUnit* theWorker = ObjectOf<EEComplexUnit>(inB);
            EETechTreeNode* theNode = ObjectOf<EETechTreeNode>((uint64_t)ioArgs->i[0]);
            theComplex->Kill(0, 0, 0);
            for (unsigned long i = 2; i < 7; i++) {
                long theValue = theWorker->GetPlayer()->ScaleCost(theNode->GetCost(i, 0x20, 1.0f));
                theWorker->GetPlayer()->AddResource(i, theValue);
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_GATE_SIDE_PART:
        return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theComplex->mUnitClass)->IsSidePart(theComplex);
    case EE_UNITS_GOAL_HOST_BUILD_EVENT: {
            EEPlayer* thePlayer = ObjectOf<EEPlayer>(inA);
            EEUnit* theTarget = ObjectOf<EEUnit>(inB);
            U2DPoint<long> theTile(theTarget->mTileX, theTarget->mTileY);
            thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, ioArgs->i[0] != 0 ? 0x7c42 : 0x273a, false);
            return 0;
        }
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
    case EE_UNITS_GOAL_HOST_FOUNDATION_STARTED: {
            // Work's foundation become a building site: the wonder's notice and clock, the media, the parts
            EEUnit* theTarget = ObjectOf<EEUnit>(inB);
            if (ioArgs->i[0] != 0) {
                if (theComplex->mPlayer != ::gWorld.GetCurrentPlayer()) {
                    UWideString theFormat;
                    UWideString theName;
                    UWideString theText;
                    gResource->GetString(IDS_HAS_STARTED_CONSTRUCTING_WONDER, theFormat);
                    gResource->GetString(theTarget->mUnitClass->mNameStringID, theName);
                    sprintf(theText, (const char*)theFormat, (const char*)theComplex->mPlayer->mName,
                            (const char*)theName);
                    gUserInterface->AddChatLogEntry(theText, 1);
                }
                EESoundObject* theSound = EESoundObject::Get(kSoundWonderClock);
                if (theSound != NULL)
                    theSound->Play();
            }
            theComplex->mPlayer->LoadUnitClassMedia(theTarget->mUnitClass->GetUnitType());
            theComplex->LoadMedia(false);
            gDataPreLoading->PostUnloadUnitClass(theComplex->mPlayer->mPlayerID, theComplex->mUnitClass->GetUnitType());
            for (EEComplexUnit* thePart = theComplex->mNextPart; thePart != NULL; thePart = thePart->mNextPart) {
                thePart->SetHitPoints(2);
                thePart->mPlayer->LoadUnitClassMedia(thePart->mUnitClass->GetUnitType());
                thePart->LoadMedia(false);
                gDataPreLoading->PostUnloadUnitClass(thePart->mPlayer->mPlayerID, thePart->mUnitClass->GetUnitType());
                thePart->PartFinished();
            }
            theComplex->PartFinished();
            return 0;
        }
    // the gather goal's
    case EE_UNITS_GOAL_HOST_NEXT_RESOURCE: {
            std::vector<EEUnit*> thePassed;
            for (size_t k = 0; k < ioArgs->list_len; k++)
                thePassed.push_back(ObjectOf<EEUnit>(ioArgs->list[k]));
            return Handle(theComplex->mUnitClass
                              ->FindNextResource(theComplex, ObjectOf<EEUnit>(inB), (long)ioArgs->i[0], &thePassed));
        }
    case EE_UNITS_GOAL_HOST_FIND_STORAGE:
        return Handle(theComplex->mUnitClass->FindStorage(theComplex, ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_STORAGE_SEARCH: {
            // FindStorage's search (an AI may order a settlement for the gatherer instead: -1)
            unsigned long theCommand = theComplex->mCommand ? theComplex->mCommand->mType : 0;
            theComplex->ReleaseFireEffect();
            if ((theCommand == kActionFindStorage || theCommand == kActionFindNewStorage) && !theComplex->mPlayer->IsHuman()) {
                // (the resource manager's BuildSettlementFor: it only cleared the computer player's selection; main's
                // stage 6)
                EEComputerPlayer* theCP = theComplex->mPlayer->mComputerPlayer;
                EESelectedGroup* theGroup = theCP != NULL ? theCP->mActiveGroup : NULL;
                if (theGroup != NULL && theGroup->GetNumUnits() > 0)
                    theGroup->Clear();
            }
            EEUnitClass* theClass = EEUnitClassOfKind<EE_CLASS_KIND_BASIC_CITIZEN>(theComplex->GetUnitClass());
            return Handle(theClass->FindStorage(theComplex, (theComplex->mCommand ? theComplex->mCommand->mType : 0) == kActionFindNewStorage));
        }
    case EE_UNITS_GOAL_HOST_GATHER_SPOT: {
            bool theFound = EEGatherFindSpot(theComplex, ObjectOf<EEUnit>(inB), thePoint);
            PointOut(ioArgs, thePoint);
            return theFound;
        }
    case EE_UNITS_GOAL_HOST_NEXT_TREE:
        return Handle(EEGatherNextTree(theComplex, ObjectOf<EEUnit>(inB)));
    case EE_UNITS_GOAL_HOST_HUNT: {
            EEUnit* theResource = ObjectOf<EEUnit>(inB);
            long theAction = 0x7e0;
            if (!(theResource->mHitPoints > 0 && theComplex->mUnitClass->FindAction(theAction) != NULL))
                return 0;
            theComplex->SetGoal(EEUGRust::NewAttackUnit(theComplex, theResource, ioArgs->i[0] != 0, false), 1, 0);
            theComplex->StartAction(theAction);
            theComplex->mUnitClass->ActionChanged(theComplex);
            return 1;
        }
    case EE_UNITS_GOAL_HOST_SET_GATHERED:
        theComplex->SetGatheredResource(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_LAST_WAYPOINT:
        PointOut(ioArgs, GE3DPoint(theComplex->mWaypoints.back().mLocation));
        return 0;
    case EE_UNITS_GOAL_HOST_FIND_GHOST:
        if (inB != 0)
            return Handle(EEFindLoadedGhost((unsigned long)inA));
        return Handle(EEFindLoadedGhostUnit((unsigned long)inA));
    case EE_UNITS_GOAL_HOST_LINE_TO_UNIT_BLOCKED: {
            EEUnit* theBlocker;
            long theArg4;
            long theArg5;
            return EEIsLineToUnitBlocked(theComplex, ObjectOf<EEUnit>(inB), &theBlocker, &theArg4, &theArg5);
        }
    case EE_UNITS_GOAL_HOST_ACCESSIBLE:
        return EEGatherAccessible(theComplex, ObjectOf<EEUnit>(inB));
    // the gathering's (world.rs: BeginGathering, Work, SearchNavalResource)
    case EE_UNITS_GOAL_HOST_TAKE_HIT:
        theUnit->TakeHit(ObjectOf<EEComplexUnit>(inB), UMFloatToLong(ObjectOf<EEComplexUnit>(inB)->mStats->mAttack), 0, 1);
        return 0;
    case EE_UNITS_GOAL_HOST_TAKE_ONE:
        theUnit->DepleteResource(1);
        ObjectOf<EEComplexUnit>(inB)->AddResourceAmount(1);
        ObjectOf<EEComplexUnit>(inB)->SetLastResource(theUnit);
        return 0;
    case EE_UNITS_GOAL_HOST_REGROW:
        theUnit->RegrowForest(ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_WORK_TIMER: {   // the gather time of the resource (variant-scaled except stone)
            EEUnit* theResource = ObjectOf<EEUnit>(inB);
            long theRate = theComplex->mStats->mGatherTimes[ee_units_uu_resource_index((uint64_t)(uintptr_t)theResource)];
            if (theResource->mResourceType != kResourceStone)
                theRate = UMFloatToLong(EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->mWorkTimeFactor * theRate);
            theComplex->SetWorkTimer(theRate);
            return 0;
        }
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
            std::vector<EEUnit*> theTaken;
            for (size_t k = 0; k < ioArgs->list_len; k++)
                theTaken.push_back(ObjectOf<EEUnit>(ioArgs->list[k]));
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
