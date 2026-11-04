// C:\EEX\Empire Earth\Units\EEUGGather.cpp (file name inferred: no __FILE__; the key "EEUGGather-Data").
// Gathering a resource: 0x221d98..0x22528d (the edges are the key chain and EEUGHealUnit.cpp's key chain; after
// Work 0x22410b come the std::sort COMDATs of the two tile orders). Unit flags: the exe's module set. Data: the key
// 0x563580 (guard byte 0x563578); vtable 0x4475a8.

#include <windows.h>
#include <stdio.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEGameVariant.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPhysics.h"
#include "ee/EEUCNavalCitizen.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EEAIContinentManager.h"

const UString EEUGGather::sData("EEUGGather-Data");
// SYNTHETIC: EXE 0x621d98   (wrapper)
// SYNTHETIC: EXE 0x621da2   (ctor)
// SYNTHETIC: EXE 0x621db3   (registrar)
// SYNTHETIC: EXE 0x621dbf   (dtor)

// FindGatherSpot's path subsectors and class flags. Included after the key: with U2DSparseArray.h above it the key's
// init chain is no longer the unit's first
#include "ee/EEAIPathSector.h"
#include "lle/U2DSparseArray.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"

// EEAIContinentManager's inline forest id (ee/EEAIContinentManager.h declares it; EEAIForest is complete here). Its
// out-of-line COMDAT is 0x625272 (GatherNextTree's first call; the second is expanded)
inline unsigned short EEAIContinentManager::GetForestIDAt(const EETileLocation& inTile)
{
    EEAIForest* theForest = this->GetForestAt(inTile);
    return theForest != NULL ? theForest->mID : 0;
}

// the path distance EEGetPathLengthTo reports for no path (0x447598, loaded through memory; EvaluateCondition)
// (provisional name)
static const float kNoPathDistance = 10000.0f;
// the squared distance within which a gatherer checks the resource's tiles (0x44759c, loaded through memory)
// (provisional name)
static const float kGatherRangeSquared = 16.0f;
// the squared distance from the resource's centre within which a gatherer stops waiting (0x4475a0; provisional name)
static const float kWaitRangeSquared = 36.0f;

// The order FindGatherSpot sorts the tiles around the resource in: the farther from the gatherer first (0x18 bytes: the
// gatherer's position, the resource's tile, its path sector's id, the gatherer; the ctor 0x222a5f and the implicit
// copy ctor 0x222ab5 are inline COMDATs after FindGatherSpot) (provisional name)
struct EEGatherTileFarther {
    EEGatherTileFarther(EEComplexUnit* inUnit, const EETileCoordinate& inTile) : mTile(inTile), mUnit(inUnit)
    {
        EEAIPathSector* theSector = gContinentManager.GetPathSector(inTile);
        this->mSector = theSector ? theSector->mID : (unsigned short)0;
        this->mPosition.SetX(inUnit->mPhysicalModel.GetLocation().GetX());
        this->mPosition.SetY(inUnit->mPhysicalModel.GetLocation().GetY());
    }
    bool operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const;

    U2DPoint<float> mPosition;              // +0x00
    EETileCoordinate mTile;                 // +0x08
    unsigned short mSector;                 // +0x10
    EEComplexUnit* mUnit;                   // +0x14
};

#pragma optimize("g", off)
// FUNCTION: EXE 0x621ddb
// Returns whether the first tile is farther from the gatherer than the second (sorting puts the nearest last)
// Built without /Og in the original (every local in the frame, the result through an int temporary): the
// optimize("g", off) pragma alone makes it exact.
bool EEGatherTileFarther::operator()(const U2DPoint<long>& inFirst, const U2DPoint<long>& inSecond) const
{
    float theFirst = this->mPosition.DistanceToSquared(U2DPoint<float>((float)inFirst.GetX(), (float)inFirst.GetY()));
    float theSecond = this->mPosition.DistanceToSquared(U2DPoint<float>((float)inSecond.GetX(), (float)inSecond.GetY()));
    return theFirst > theSecond;
}
#pragma optimize("g", on)

// TEMPLATE: EXE 0x621e6d
// (inline: EETileDistanceLess::operator(), ee/EEAreaUtilities.h; first emitted here)

// FUNCTION: EXE 0x621e95
// Creates a gather goal for a resource and a storage building, referencing both, aimed at the resource's tile (else
// the storage's)
EEUGGather::EEUGGather(EEUnit* inResource, EEUnit* inStorage, bool inArg3)
    : EEUnitGoal(kGoalGather, inArg3)
{
    this->mResource = inResource;
    if (inResource != NULL)
        inResource->Reference();
    this->mStorage = inStorage;
    if (inStorage != NULL)
        inStorage->Reference();
    if (inResource != NULL)
        ::gWorld.GetMap()->GetTileLocation(inResource->mTileX, inResource->GetTileY(), this->mDestination);
    else if (inStorage != NULL)
        ::gWorld.GetMap()->GetTileLocation(inStorage->mTileX, inStorage->GetTileY(), this->mDestination);
    this->mWaitingArea = this->mDestination;
    this->mGathering = false;
    this->mWaiting = false;
}

// TEMPLATE: EXE 0x621f29
// (inline: Virtual0C, emitted with the ctor)

// TEMPLATE: EXE 0x621f3f
// (inline: Virtual78, emitted with the ctor)

// TEMPLATE: EXE 0x621f43
// (inline: Virtual70, emitted with the ctor)

// SYNTHETIC: EXE 0x621f64
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x621f80
// Destroys the goal, releasing the resource and the storage
EEUGGather::~EEUGGather()
{
    if (this->mResource != NULL) {
        this->mResource->Dereference();
        this->mResource = NULL;
    }
    if (this->mStorage != NULL) {
        this->mStorage->Dereference();
        this->mStorage = NULL;
    }
    this->mGathering = true;
}

// FUNCTION: EXE 0x621fb5
// Picks the destination: the waiting area, the farm tile nearest the granary (or gatherer), a spot by the resource
// (wood and moving animals pass to the next one), a free tile by a storage (trying others); tells the active group
// NEAR: the EETileDistanceLess temporaries are built by an out-of-line ctor call in the original; frame and registers
void EEUGGather::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EEUnit*> theResources;
    std::vector<EEUnit*> theStorages;
    EEUCBasicCitizen* theClass = static_cast<EEUCBasicCitizen*>(inUnit->mUnitClass);
    if (this->mWaiting)
        outLocation = this->mWaitingArea;
    else if (this->mGathering && this->mResource != NULL) {
        if (this->mResource->mUnitClass->mUnitType == kUnitTypeFarm) {
            EEGetFootprintTiles(this->mResource, theTiles);
            if (this->mStorage != NULL && (this->mStorage->mUnitClass->mUnitType == kUnitTypeGranaryImproved || this->mStorage->mUnitClass->mUnitType == kUnitTypeGranary))
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(this->mStorage->GetTile()));
            else
                std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(inUnit->GetTile()));
            outLocation.mX = theTiles[0].GetX() + 0.5f;
            outLocation.mY = theTiles[0].GetY() + 0.5f;
            this->mDestination = outLocation;
        } else {
            theResources.clear();
            while (this->mResource != NULL) {
                if (EEFindResourceSpot(inUnit, static_cast<EEComplexUnit*>(this->mResource), outLocation))
                    break;
                if (this->mResource->GetResourceType() != kResourceWood) {
                    if (this->mResource->mComplexUnit == NULL || !(this->mResource->mComplexUnit->mStats->mMaxVelocity > 0.0f))
                        break;
                }
                theResources.push_back(this->mResource);
                this->SetResource(theClass->FindNextResource(inUnit, this->mResource, this->mResource->GetResourceType(), &theResources));
            }
            this->mDestination = outLocation;
        }
    } else if (this->mStorage != NULL) {
        theStorages.clear();
        while (this->mStorage != NULL) {
            if (EEFindNextFreeTile(inUnit, static_cast<EEComplexUnit*>(this->mStorage), outLocation))
                break;
            if (std::find(theStorages.begin(), theStorages.end(), this->mStorage) != theStorages.end())
                break;
            EEUnit* theStorage = this->mStorage;
            theStorages.push_back(theStorage);
            this->SetStorage(theClass->FindStorage(inUnit, true));
        }
        this->mDestination = outLocation;
    } else {
        outLocation.mX = (float)inUnit->mTileX;
        outLocation.mY = (float)inUnit->GetTileY();
        outLocation.mZ = inUnit->mTile->GetHeight();
    }
    if (inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
}

// FUNCTION: EXE 0x622271
// Returns whether the gatherer, within 4 tiles of the resource, stands on one of its gather spots with room to stand
bool EEUGGather::IsAtResourceSpot(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    std::vector<U2DPoint<long> > theTiles;
    bool theResult = false;
    if (this->mResource != NULL && !(inUnit->DistanceSquaredToUnit(this->mResource) > kGatherRangeSquared)) {
        EEGetResourceSpots(this->mResource, theTiles);
        U2DPoint<long> theTile(inUnit->mTileX, inUnit->GetTileY());
        if (std::find(theTiles.begin(), theTiles.end(), theTile) != theTiles.end())
            theResult = EEFindPlaceOnTile(inUnit, inUnit->GetUnitClass(), inUnit->mTileX, inUnit->GetTileY(), 1, inUnit->mTileX,
                                   inUnit->GetTileY(), 1, &theLocation);
    }
    return theResult;
}

// FUNCTION: EXE 0x62234a
// Returns a copy of the goal with the same resource, storage, flags, destination and waiting area
EEUnitGoal* EEUGGather::Clone()
{
    EEUGGather* theGoal = new EEUGGather(this->mResource, this->mStorage, this->GetB1C());
    theGoal->mGathering = this->mGathering;
    theGoal->mWaiting = this->mWaiting;
    theGoal->mDestination = this->mDestination;
    theGoal->mWaitingArea = this->mWaitingArea;
    return theGoal;
}

// FUNCTION: EXE 0x6223aa
// Writes the waiting area, destination, resource and storage ids and the gathering and waiting flags to the
// out-of-synch dump, then the base goal's
void EEUGGather::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Waiting Area X: %6.6f\n", this->mWaitingArea.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Waiting Area Y: %6.6f\n", this->mWaitingArea.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Waiting Area Z: %6.6f\n", this->mWaitingArea.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Actual Destination X: %6.6f\n", this->mDestination.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Actual Destination Y: %6.6f\n", this->mDestination.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Actual Destination Z: %6.6f\n", this->mDestination.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Resource Target ID: #%08x\n", this->mResource ? this->mResource->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Storage Target ID: #%08x\n", this->mStorage ? this->mStorage->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Gathering Flag %d\n", this->mGathering == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGGather - Waiting Flag %d\n", this->mWaiting == true);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x622489
// Returns the base goal's checksum plus the raw bits of the waiting area and destination, the resource and storage
// ids and the two flags
long EEUGGather::GetOutOfSynchChecksum()
{
    float theValue = this->mWaitingArea.GetX();
    long theChecksum = *reinterpret_cast<long*>(&theValue);
    theValue = this->mWaitingArea.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mWaitingArea.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDestination.GetX();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDestination.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDestination.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theChecksum += this->mResource ? this->mResource->mUnitID : 0;
    theChecksum += (this->mStorage ? this->mStorage->mUnitID : 0) + (this->mWaiting == true) + (this->mGathering == true);
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// FUNCTION: EXE 0x622500
// Switches to the next resource of the current resource's type (else the gatherer's carried type) near the gatherer
void EEUGGather::FindNewResource(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theUnused;
    EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    this->SetResource(theClass->FindNextResource(inUnit, this->mResource, this->mResource ? this->mResource->mResourceType : inUnit->mResourceType, &theUnits));
}

// FUNCTION: EXE 0x622593
// Sends the gatherer to work the first free farm around the drop sites of its farm
void EEUGGather::SendToNextFarm(EEComplexUnit* inUnit)
{
    EEComplexUnit* theTarget = NULL;
    std::vector<EEUnit*> theUnits;
    long theCount = 1;
    if (!EEGetFarmDropSites(this->mResource->GetComplexUnit(), theUnits))
        return;
    for (std::vector<EEUnit*>::iterator theIterator = theUnits.begin(); theIterator != theUnits.end();
         ++theIterator) {
        if (theTarget != NULL)
            break;
        theTarget = EEFindFreeFarm((*theIterator)->GetComplexUnit(), 1, &theCount);
    }
    if (theTarget != NULL)
        EEWorkFarm(theTarget, inUnit);
}

// FUNCTION: EXE 0x622612
// Picks the storage building to deliver to (a new one on find-new-storage); an AI player that orders a settlement
// for the gatherer instead stops there
// NEAR: the original keeps theFree in inUnit's argument slot and this in the frame (ebp frame)
void EEUGGather::FindStorage(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    inUnit->ReleaseFireEffect();
    if ((theCommand == kActionFindStorage || theCommand == kActionFindNewStorage) && !inUnit->mPlayer->IsHuman()) {
        bool theFree = !inUnit->mPlayer->mComputerPlayer->mCivilizationManager->mResourceManager->BuildSettlementFor(inUnit);
        if (!theFree)
            return;
    }
    EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    EEUnit* theStorage = theClass->FindStorage(inUnit, (inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionFindNewStorage);
    this->SetStorage(NULL);
    if (theStorage != NULL)
        this->SetStorage(theStorage);
}

// FUNCTION: EXE 0x6226cf
// Finds a waiting area by the resource: its nearest spot in the resource's path subsector, under the gatherer or in a
// neighbouring subsector the gatherer can enter, else any spot with room; returns whether one was found
bool EEUGGather::FindGatherSpot(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theOthers;
    EECPASGFlags theFlags;
    bool theFound = false;
    bool theHaveFlags = false;
    GE3DPoint theLocation;
    EEGetResourceSpots(this->mResource, theTiles);
    std::sort(theTiles.begin(), theTiles.end(), EEGatherTileFarther(inUnit, this->mResource->GetTile()));
    EEAIPathSubSector* theStart = gContinentManager.GetPathSubSectorAt(this->mResource);
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
            if (theFlags.mFlags[theSubSector->mKind])
                theFound = true;
        }
        if (theFound == true) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles.back().GetX(), theTiles.back().GetY());
            if (theTile != NULL
                && EEFindPlaceOnTile(inUnit, inUnit->GetUnitClass(), theTiles.back().GetX(), theTiles.back().GetY(), 1,
                              theTiles.back().GetX(), theTiles.back().GetY(), 1, &theLocation)) {
                this->mWaitingArea = theLocation;
                this->mWaitingArea.mZ = theTile->GetHeight();
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
            this->mWaitingArea = theLocation;
            theFound = true;
            this->mWaitingArea.mZ = theTile->GetHeight();
        }
    }
    return theFound;
}

// TEMPLATE: EXE 0x622a5f
// (inline: EEGatherTileFarther's ctor, after its first user FindGatherSpot)

// TEMPLATE: EXE 0x622ab5
// (inline: EEGatherTileFarther's implicit copy ctor)

// FUNCTION: EXE 0x622ae2
// Starts gathering: the animation for the resource (hunt, farm, chop, mine; a mining beam from the nano age), faces
// it, sets the work timer from the gather time (variant-scaled except stone) and stops the gatherer
// NEAR: register choice (this in ebx), the original loads gEffectManager before EEGraphicEffect::Get
void EEUGGather::BeginGathering(EEComplexUnit* inUnit)
{
    if (this->mResource == NULL)
        return;
    if (inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
        inUnit->SetAnimation(0xb, -1, 0);
    else {
        long theCategory = this->mResource->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryLand)
            inUnit->SetAnimation(this->mResource->mHitPoints > 0 ? 0xb : 0xa, -1, 0);
        else
            switch (this->mResource->mResourceType) {
            case kResourceFood:
                inUnit->SetAnimation(this->mResource->mUnitClass->mUnitType == kUnitTypeFarm ? 6 : 0xa, -1, 0);
                break;
            case kResourceWood:
                inUnit->SetAnimation(3, -1, 0);
                if (inUnit->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochNano)
                    inUnit->ReleaseFireEffect();
                break;
            case kResourceStone:
            case kResourceGold:
            case kResourceIron:
                inUnit->SetAnimation(9, -1, 0);
                if (inUnit->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochNano) {
                    inUnit->ReleaseFireEffect();
                    if (this->mResource != NULL) {
                        GE3DPoint thePosition;
                        this->mResource->GetPosition(thePosition);
                        EEEffectHandle theHandle;
                        ::gWorld.GetEffectManager()->CreateEffect(inUnit, thePosition, EEGraphicEffect::Get(kGraphicEffectSpaceCitizenMiningBeam), &theHandle, 0);
                        inUnit->mFireEffect = theHandle;
                    }
                }
                break;
            }
    }
    inUnit->SetResourceType(this->mResource->mResourceType);
    if (inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen)
        inUnit->FaceTarget(this->mResource);
    long theRate = inUnit->mStats->mGatherTimes[EEGetResourceIndex(this->mResource)];
    if (this->mResource->mResourceType != kResourceStone)
        inUnit->SetWorkTimer(UMFloatToLong(
            EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->mWorkTimeFactor * theRate));
    else
        inUnit->SetWorkTimer(theRate);
    inUnit->DropPath();
    inUnit->mActiveGoal->SetPathAttempts(0);
    inUnit->SetCurrentPhysicsAction(6);
    inUnit->SetVelocity(0.0f);
    inUnit->mAngularVelocity.mYaw = 0.0f;
    inUnit->mAngularVelocity.mPitch = 0.0f;
    inUnit->mAngularVelocity.mRoll = 0.0f;
}

// FUNCTION: EXE 0x622ce6
// Appends "gather" or "dropoff" and the resource's id and tile (plus type and amount when gathering) to the debug line
void EEUGGather::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    if (this->mGathering) {
        ioText += "gather";
        if (this->mResource != NULL) {
            sprintf(theText, "(uid %d, at %d,%d, type %d, amount %d)", this->mResource->mUnitID & 0xffffff,
                    this->mResource->mTileX, this->mResource->GetTileY(), this->mResource->mResourceType, this->mResource->mResourceAmount);
            ioText += theText;
        }
    } else {
        ioText += "dropoff";
        if (this->mResource != NULL) {
            sprintf(theText, "(uid %d, at %d,%d)", this->mResource->mUnitID & 0xffffff, this->mResource->mTileX,
                    this->mResource->GetTileY());
            ioText += theText;
        }
    }
    ioText += ":";
}

// FUNCTION: EXE 0x622d9b
// Appends "Gathering" or "Dropping Off" and the resource's name and tile (plus type and amount) to the debug text
void EEUGGather::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    if (this->mGathering) {
        ioText += "Gathering\n";
        if (this->mResource != NULL) {
            sprintf(theText, "Unit: %s at (%d,%d)\nType:%d\nAmount:%d\n", (const char*)this->mResource->mUnitClass->mName,
                    this->mResource->mTileX, this->mResource->GetTileY(), this->mResource->mResourceType, this->mResource->mResourceAmount);
            ioText += theText;
        }
    } else {
        ioText += "Dropping Off\n";
        if (this->mResource != NULL) {
            sprintf(theText, "Unit: %s at (%d,%d)\n", (const char*)this->mResource->mUnitClass->mName, this->mResource->mTileX,
                    this->mResource->GetTileY());
            ioText += theText;
        }
    }
}

// FUNCTION: EXE 0x622e3c
// Returns the waiting area while waiting with no current target, else the destination
void EEUGGather::GetGoalLocation(GE3DPoint& outLocation)
{
    if (!(this->mGathering && this->mResource != NULL || !this->mGathering && this->mStorage != NULL) && this->mWaiting)
        outLocation = this->mWaitingArea;
    else
        outLocation = this->mDestination;
}

// FUNCTION: EXE 0x622e84
// Returns move-to-waiting-area as the first action with a resource, else action 0x1f49 with a storage, else 0xfa6
long EEUGGather::GetStartAction()
{
    if (this->mResource != NULL)
        return kActionMoveToWaitingArea;
    return this->mStorage ? 0x1f49 : 0xfa6;
}

// FUNCTION: EXE 0x622ea2
// Runs the current gather action: find storage or resource, deposit, gather, arrive, next tree, unfinished buildings,
// next farm, naval search or finish
void EEUGGather::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionFindNewStorage:
    case kActionFindStorage:
        this->FindStorage(inUnit);
        break;
    case kActionFindNewResource:
        this->FindNewResource(inUnit);
        break;
    case kActionAddResourceToStorage:
        EEDepositLoad(inUnit, static_cast<EEComplexUnit*>(this->mStorage));
        this->SetPathAttempts(0);
        break;
    case kActionGatherResource:
        this->BeginGathering(inUnit);
        break;
    case kActionMoveToWaitingArea:
        this->ArriveAtResource(inUnit);
        break;
    case kActionMoveToStorage:
        this->BuildFarms(inUnit);
        break;
    case kActionMoveToResource:
        this->GatherNextTree(inUnit);
        break;
    case kActionFindFarm:
        this->SendToNextFarm(inUnit);
        break;
    case kActionWaitForOpenPosition:
        this->SearchNavalResource(inUnit);
        break;
    case kActionNotifyWaitingCitizen:
        this->FinishGathering(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x622f70
// Reads the base goal, resource and storage ids, gathering flag, waiting area, waiting flag and destination; returns
// the first read error
long EEUGGather::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    unsigned long theUnitID;
    theResult = inFile.Read(theUnitID);
    if (theResult != 0)
        return theResult;
    this->mResource = reinterpret_cast<EEUnit*>(theUnitID);
    theResult = inFile.Read(theUnitID);
    if (theResult != 0)
        return theResult;
    this->mStorage = reinterpret_cast<EEUnit*>(theUnitID);
    theResult = inFile.Read(this->mGathering);
    if (theResult != 0)
        return theResult;
    theResult = this->mWaitingArea.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mWaiting);
    if (theResult == 0)
        theResult = this->mDestination.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x622ff0
// Heads for the resource: a tree with no free spot gives way to the first gatherable tree of its forest on the line
// from a nearby drop site (or the gatherer) to it; drops a load of another type and sets gathering
void EEUGGather::GatherNextTree(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    GE3DPoint theLocation;
    U2DPoint<long> theTile;
    U2DPoint<long> theTarget;
    GE2DPoint theCenter;
    EEUCBasicCitizen* theCitizen = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    if (inUnit->mResourceAmount > 0 && inUnit->mResourceType != this->mResource->mResourceType)
        inUnit->mResourceAmount = 0;
    if (this->mResource->mResourceType == kResourceWood && !EEFindResourceSpot(inUnit, static_cast<EEComplexUnit*>(this->mResource), theLocation)) {
        unsigned short theForestID =
            gContinentManager.GetForestIDAt(U2DPoint<long>(this->mResource->GetTileX(), this->mResource->GetTileY()));
        EEAIForest* theForest = gContinentManager.mForests[theForestID];
        if (theForest != NULL && theForest->mTiles.Size() > 1) {
            theCenter.SetX(this->mResource->GetTileX());
            theCenter.SetY(this->mResource->GetTileY());
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
            theTarget = U2DPoint<long>(this->mResource->GetTileX(), this->mResource->GetTileY());
            if (theTile != theTarget) {
                theTile.GetLineTiles(theTarget, 1.0f, theTiles);
                EEUnit* theFound = NULL;
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
                if (theFound != NULL)
                    this->SetResource(theFound);
            }
        }
    } else if (this->mResource->GetUnitClass()->mFamily != kUnitFamilyAnimal)
        inUnit->SetGatheredResource(this->mResource);
    inUnit->SetCurrentPhysicsAction(0);
    this->mWaiting = false;
    this->mGathering = true;
}

// FUNCTION: EXE 0x62333b
// Unless it works a farm, gives the gatherer a build goal for each unfinished own settlement or town centre in sight
// (each becomes the storage); stops gathering
// NEAR: the original calls begin() and _Ptrit::operator+ out of line (8 more frame bytes); ours inlines them
void EEUGGather::BuildFarms(EEComplexUnit* inUnit)
{
    GE2DPoint theCenter;
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theOthers;
    inUnit->SetCurrentPhysicsAction(0);
    if (this->mResource == NULL || this->mResource->mUnitClass->mUnitType != kUnitTypeFarm) {
        theCenter.SetX(inUnit->mTileX);
        theCenter.SetY(inUnit->GetTileY());
        EEGetUnitsInRadius(theCenter, inUnit->GetLineOfSight(), theUnits);
        for (long theIndex = theUnits.size() - 1; theIndex >= 0; theIndex--) {
            EEUnit* theUnit = theUnits[theIndex];
            unsigned long theType = theUnit->mUnitClass->mUnitType;
            if ((theType != kUnitTypeSettlement && theType != kUnitTypeTownCenter) || theUnit->mPlayer != inUnit->mPlayer || !(theUnit->mHitPoints > 0)
                || static_cast<EEComplexUnit*>(theUnit)->mIsBuilt)
                continue;
            EEUGBuildRepair* theGoal = new EEUGBuildRepair(inUnit->GetPlayer(), theUnit, false);
            if (theGoal == NULL)
                continue;
            this->SetStorage(theUnit);
            inUnit->SetGoal(theGoal, 1, 0);
            inUnit->mUnitClass->ActionChanged(inUnit);
        }
    }
    this->mGathering = false;
    this->mWaiting = false;
}

// FUNCTION: EXE 0x6234b7
// At the waiting area (not for trees): hunts a living animal, else moves in when the resource has room and the gatherer
// is within 6 tiles or nearer than the waiting area; otherwise waits (idles with no gather spot)
// NEAR: this/inUnit register swap (edi/esi), the x coordinates go through an integer temporary in the original
void EEUGGather::ArriveAtResource(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theOthers;
    if (this->mResource->mUnitClass->mIsTree)
        return;
    inUnit->SetCurrentPhysicsAction(0);
    long theKind = this->mResource->mUnitClass->mFamily;
    if (theKind == kUnitFamilyAnimal || theKind == kUnitFamilyFish) {
        long theAction = 0x7e0;
        if (this->mResource->mHitPoints > 0 && inUnit->mUnitClass->FindAction(theAction) != NULL) {
            inUnit->SetGoal(new EEUGAttackUnit(inUnit, this->mResource, this->GetB1C(), false), 1, 0);
            inUnit->StartAction(theAction);
        } else
            inUnit->StartAction(kActionMoveToResource);
    } else if (this->FindGatherSpot(inUnit)) {
        float theHalf = this->mResource->mUnitClass->mTileSize * 0.5f;
        float theX = this->mResource->mTileX + theHalf;
        float theY = this->mResource->GetTileY() + theHalf;
        inUnit->SetGatheredResource(NULL);
        float theDY = inUnit->mPhysicalModel.GetLocation().GetY() - theY;
        float theDX = inUnit->mPhysicalModel.GetLocation().GetX() - theX;
        float theDistance = theDX * theDX + theDY * theDY;
        if (this->HasGatherRoom(inUnit, this->mResource)) {
            if (theDistance <= kWaitRangeSquared)
                goto start;
            theDY = this->mWaitingArea.GetY() - theY;
            theDX = this->mWaitingArea.GetX() - theX;
            if (theDistance < theDX * theDX + theDY * theDY)
                goto start;
        }
        this->mGathering = false;
        this->mWaiting = true;
        return;
    start:
        inUnit->StartAction(kActionMoveToResource);
    } else
        inUnit->StartAction(kActionIdle);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x623675
// Returns an unfinished living settlement within twice the gatherer's line of sight (0 if none)
// NEAR: schedule only (the second fild comes one store later in the original)
EEUnit* EEUGGather::FindUnfinishedFarm(EEComplexUnit* inUnit)
{
    GE2DPoint theCenter((float)inUnit->GetTileX(), (float)inUnit->GetTileY());
    std::vector<EEUnit*> theUnits;
    EEUnit* theResult = NULL;
    EEGetUnitsInRadius(theCenter, inUnit->GetLineOfSight() * 2, theUnits);
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        EEUnit* theUnit = theUnits[i];
        if (theUnit->mUnitClass->mUnitType == kUnitTypeSettlement && theUnit->mHitPoints > 0 && !static_cast<EEComplexUnit*>(theUnit)->mIsBuilt) {
            theResult = theUnit;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x62371b
// Stops gathering, leaving the resource (except trees and animals) and resetting the path attempts
void EEUGGather::FinishGathering(EEComplexUnit* inUnit)
{
    if (this->mResource->mResourceType != kResourceWood && this->mResource->mUnitClass->mFamily != kUnitFamilyAnimal)
        inUnit->SetGatheredResource(NULL);
    inUnit->mActiveGoal->SetPathAttempts(0);
    this->mGathering = false;
}

// FUNCTION: EXE 0x623757
// Returns whether the resource has a free gather position: a farm takes one gatherer, others its unblocked side
// tiles (counted twice) and corner tiles, capped by the player's mining limit
// NEAR: the original calls begin(), _Ptrit::operator+ and EEMap::GetTile out of line (inline budget); ours inlines them
bool EEUGGather::HasGatherRoom(EEComplexUnit* inUnit, EEUnit* inResource)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theEdges;
    EEMap* theMap = ::gWorld.GetMap();
    unsigned long theLimit;
    if (inResource->GetUnitClass()->GetUnitType() == kUnitTypeFarm)
        theLimit = 1;
    else {
        EEGetSideTiles(inResource->GetTileX(), inResource->GetTileY(), inResource->GetUnitClass()->mTileSize, theTiles);
        EEGetCornerTiles(0, inResource->GetTileX(), inResource->GetTileY(), inResource->GetUnitClass()->mTileSize, theEdges);
        unsigned long theCount = theTiles.size() * 2 + theEdges.size();
        unsigned long theIndex; for (theIndex = 0; theIndex < theTiles.size(); theIndex++) {
            EEMapTile* theTile = theMap->GetTile(theTiles[theIndex].mX, (theTiles.begin() + theIndex)->mY);
            if (theTile == NULL)
                theCount--;
            else
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if (theUnit == NULL)
                        continue;
                    long theCategory = theUnit->GetUnitClass()->GetCategory();
                    if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen && !theUnit->IsPassableBy(inUnit))
                        theCount--;
                }
        }
        for (theIndex = 0; theIndex < theEdges.size(); theIndex++) {
            EEMapTile* theTile = theMap->GetTile((theEdges.begin() + theIndex)->mX, (theEdges.begin() + theIndex)->mY);
            if (theTile == NULL)
                theCount--;
            else
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if (theUnit == NULL)
                        continue;
                    long theCategory = theUnit->GetUnitClass()->GetCategory();
                    if (theCategory != kUnitCategoryLandCitizen && theCategory != kUnitCategoryNavalCitizen && !theUnit->IsPassableBy(inUnit))
                        theCount--;
                }
        }
        theLimit = theCount < inUnit->GetPlayer()->mMiningLimit ? theCount : inUnit->GetPlayer()->mMiningLimit;
    }
    return ::gWorld.GetResourceUsers(inResource) < theLimit;
}

// FUNCTION: EXE 0x623966
// Marks the goal active when the path's last waypoint reaches the waiting area, the resource or the storage
// NEAR: the original schedules the mZ == 0 fcomp after the second mWaitingArea word copy, ours after the first
void EEUGGather::PathTaken(EEComplexUnit* inUnit)
{
    EEComplexUnitWaypoint theWaypoint = inUnit->mWaypoints.back();
    GE3DPoint theTarget;
    GE3DPoint theLocation;
    theLocation = theWaypoint.mLocation;
    if (this->mWaiting) {
        theTarget = this->mWaitingArea;
        if (theWaypoint.mLocation.GetZ() == 0.0f)
            theTarget.mZ = 0.0f;
        this->mIsActive = theLocation.DistanceToSquared(theTarget) < 1.0f;
    } else if (this->mGathering)
        this->mIsActive = this->IsResourceInReach(theLocation, inUnit);
    else
        this->mIsActive = this->IsStorageInReach(theLocation, inUnit);
}

// FUNCTION: EXE 0x623a18
// Turns the loaded ids back into resource and storage, referencing them; returns 3 when one is gone (a missing
// resource leaves the storage cleared)
long EEUGGather::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 0;
    if (USavedID(this->mResource) != -1) {
        this->mResource = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mResource)));
        if (this->mResource != NULL)
            this->mResource->Reference();
        else
            theResult = 3;
    } else {
        this->mResource = NULL;
        this->mWaiting = false;
        this->mGathering = false;
    }
    if (theResult == 0 && USavedID(this->mStorage) != -1) {
        this->mStorage = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(USavedID(this->mStorage)));
        if (this->mStorage != NULL)
            this->mStorage->Reference();
        else
            theResult = 3;
    } else
        this->mStorage = NULL;
    return theResult;
}

// FUNCTION: EXE 0x623a91
// Starts moving to the waiting area when there is a resource, else idles
void EEUGGather::Start(EEComplexUnit* inUnit)
{
    if (this->mResource != NULL)
        inUnit->StartAction(kActionMoveToWaitingArea);
    else
        inUnit->StartAction(kActionIdle);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x623abf
// Returns whether the gatherer stands on its farm or is within reach of another resource; a land gatherer then faces it
// NEAR: the original keeps theResult in inUnit's argument slot ([ebp+0xb]); ours in bl
bool EEUGGather::IsAtResource(EEComplexUnit* inUnit)
{
    bool theResult = ::gWorld.GetMap()->GetTile(inUnit->mTileX, inUnit->GetTileY())->HasUnit(this->mResource);
    if (this->mResource->mUnitClass->mUnitType != kUnitTypeFarm)
        theResult = EEIsWithinReach(inUnit, this->mResource);
    if (theResult && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen)
        inUnit->FaceTarget(this->mResource);
    return theResult;
}

// FUNCTION: EXE 0x623b44
// Returns whether the resource is within reach from a location
bool EEUGGather::IsResourceInReach(const GE3DPoint& inLocation, EEComplexUnit* inUnit)
{
    if (this->mResource != NULL && EEIsWithinReachOf(inLocation, inUnit, this->mResource))
        return true;
    return false;
}

// FUNCTION: EXE 0x623b69
// Writes the base goal, resource and storage ids (-1 when invalid), the flags (cleared without a valid resource),
// waiting area and destination; returns the first write error
long EEUGGather::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mResource && this->mResource->mIsValid ? this->mResource->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mStorage && this->mStorage->mIsValid ? this->mStorage->GetUnitID() : -1);
    if (theResult != 0)
        return theResult;
    if (this->mResource == NULL || !this->mResource->mIsValid)
        this->mGathering = false;
    theResult = outFile.Write(this->mGathering);
    if (theResult != 0)
        return theResult;
    theResult = this->mWaitingArea.Save(outFile);
    if (theResult != 0)
        return theResult;
    if (this->mResource == NULL || !this->mResource->mIsValid)
        this->mWaiting = false;
    theResult = outFile.Write(this->mWaiting);
    if (theResult == 0)
        theResult = this->mDestination.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x623c3d
// Sets the resource, moving the reference
void EEUGGather::SetResource(EEUnit* inResource)
{
    if (this->mResource != NULL)
        this->mResource->Dereference();
    this->mResource = inResource;
    if (inResource != NULL)
        inResource->Reference();
}

// FUNCTION: EXE 0x623c5e
// Sets the storage building, moving the reference
void EEUGGather::SetStorage(EEUnit* inStorage)
{
    if (this->mStorage != NULL)
        this->mStorage->Dereference();
    this->mStorage = inStorage;
    if (inStorage != NULL)
        inStorage->Reference();
}

// FUNCTION: EXE 0x623c7f
// Returns whether the gatherer is within the storage's inner radius (squared distance <= radius squared times sqrt(2))
bool EEUGGather::IsAtStorage(EEComplexUnit* inUnit)
{
    float theDistance = this->mStorage->mComplexUnit->DistanceSquaredTo(inUnit);
    float theRange = this->mStorage->mUnitClass->mInnerCollisionSphere;
    float theRangeSquared = theRange * theRange;
    if (theDistance <= theRangeSquared * 1.4142135f)
        return true;
    return false;
}

// FUNCTION: EXE 0x623cbe
// Returns whether a location is within sqrt(2) times the storage's inner radius (2D)
// the storage radius factor sqrt(2) is the pooled literal 0x441d58 (EEMiniMap::SetMap reads it too)
bool EEUGGather::IsStorageInReach(const GE3DPoint& inLocation, EEComplexUnit* inUnit)
{
    GE2DPoint thePoint;
    bool theResult = false;
    if (this->mStorage != NULL) {
        thePoint.SetX(inLocation.GetX());
        thePoint.SetY(inLocation.GetY());
        float theDistance = this->mStorage->mComplexUnit->DistanceSquared2D(thePoint);
        float theRange = this->mStorage->mUnitClass->mInnerCollisionSphere * 1.4142135f;
        theResult = theDistance <= theRange * theRange;
    }
    return theResult;
}

// FUNCTION: EXE 0x623d21
// Answers the gather triggers (wall in the way, load full or empty, resource or storage found, in range or gone,
// accessible, room to gather, farmer, waiting area reached, no path ...); others go to the common goal
// NEAR: the original switch compares unsigned (inEvent is unsigned there; the slot takes a long); frame, registers
bool EEUGGather::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    bool theResult = false;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    long theArg4;
    long theArg5;
    EEUnit* theBlocker;
    switch (inEvent) {
    case kTriggerBlockedByWall:
        if (this->mGathering && this->mResource != NULL)
            theResult = EEIsLineToUnitBlocked(inUnit, this->mResource, &theBlocker, &theArg4, &theArg5);
        else if (!this->mGathering && this->mStorage != NULL)
            theResult = EEIsLineToUnitBlocked(inUnit, this->mStorage, &theBlocker, &theArg4, &theArg5);
        break;
    case kTriggerResourceCapacityFilled:
        theResult = inUnit->mResourceAmount >= inUnit->mStats->mCarryAmount;
        break;
    case kTriggerResourceCapacityEmpty:
        theResult = inUnit->mResourceAmount == 0;
        break;
    case kTriggerNewResourceFound:
        theResult = this->mResource != NULL;
        break;
    case kTriggerResourceDepleted:
        if (this->mResource == NULL || !this->mResource->mIsValid || this->mResource->mResourceAmount == 0)
            theResult = true;
        break;
    case kTriggerResourceInRange:
        if (this->mResource != NULL && this->mResource->mIsValid && this->IsAtResource(inUnit))
            theResult = true;
        break;
    case kTriggerStorageInRange:
        if (this->mStorage != NULL && this->mStorage->mIsValid && this->IsAtStorage(inUnit))
            theResult = true;
        break;
    case kTriggerStorageFound:
        theResult = this->mStorage != NULL;
        break;
    case kTriggerStorageDestroyed:
        if (this->mStorage == NULL || !this->mStorage->mIsValid || this->mStorage->mHitPoints == 0 || static_cast<EEComplexUnit*>(this->mStorage)->mIsDisabled)
            theResult = true;
        break;
    case kTriggerResourceAccessible: {
        EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
        if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen) {
            if (this->mResource != NULL && this->mResource->mTile->mTerrainType != kTerrainDeepWater
                && gContinentManager.InSameContinent(inUnit, this->mResource))
                theResult = theClass->HasGatherPoint(inUnit, this->mResource);
        } else if (inUnit->mUnitClass->mCategory == kUnitCategoryNavalCitizen) {
            if (this->mResource != NULL && this->mResource->mTile->mTerrainType == kTerrainDeepWater
                && gContinentManager.InSameContinent(inUnit, this->mResource))
                theResult = theClass->HasGatherPoint(inUnit, this->mResource);
        }
        break;
    }
    case kTriggerWaitingAreaInRange:
        if (inUnit->GetTileX() == UMFloor(this->mWaitingArea.GetX()) && inUnit->GetTileY() == UMFloor(this->mWaitingArea.GetY()))
            theResult = true;
        break;
    case kTriggerResourceTargetIsTree:
        if (this->mResource != NULL && this->mResource->mResourceType == kResourceWood)
            theResult = true;
        break;
    case kTriggerOpenPositionAvailable:
        theResult = this->HasGatherRoom(inUnit, this->mResource);
        break;
    case kTriggerImAFarmer:
        if (this->mResource != NULL && this->mResource->mUnitClass->mUnitType == kUnitTypeFarm)
            theResult = true;
        break;
    case kTriggerFarmNotFound:
        theResult = true;
        break;
    case kTriggerCanUseThisTileAsWaitingArea:
        theResult = this->IsAtResourceSpot(inUnit);
        break;
    case kTriggerGoalIsResource:
        theResult = this->mGathering;
        break;
    case kTriggerGoalIsStorage:
        if (!this->mGathering && !this->mWaiting)
            theResult = true;
        break;
    case kTriggerGoalIsWaitingArea:
        theResult = this->mWaiting;
        break;
    case kTriggerCannotPathToTarget:
        if (this->mGathering && this->mResource != NULL) {
            theTile.mX = this->mResource->mTileX;
            theTile.mY = this->mResource->GetTileY();
        } else if (!this->mGathering && this->mStorage != NULL) {
            theTile.mX = this->mStorage->mTileX;
            theTile.mY = this->mStorage->GetTileY();
        } else if (this->mWaiting) {
            theTile.mX = UMFloor(this->mWaitingArea.GetX());
            theTile.mY = UMFloor(this->mWaitingArea.GetY());
        } else
            break;
        if (EEGetPathLengthTo(inUnit, theTile, 0x10, false) == kNoPathDistance)
            theResult = true;
        break;
    default:
        theResult = EEUnitGoal::EvaluateCondition(inUnit, inEvent);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x6240dc
// Replaces the storage with the new unit when the same player owns it, else drops it
void EEUGGather::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (this->mStorage != inOld)
        return;
    if (inNew->mPlayer == this->mStorage->mPlayer) {
        this->mStorage = inNew;
        inNew->Reference();
    } else
        this->mStorage = NULL;
    inOld->Dereference();
}

// FUNCTION: EXE 0x62410b
// Gathers once: hits a living non-farm resource with the gatherer's attack, else takes one unit of it; regrows trees
// and restarts the work timer
// NEAR: this/inUnit register swap (esi/edi)
void EEUGGather::Work(EEComplexUnit* inUnit)
{
    unsigned long theState = inUnit->mAnimation;
    if (this->mResource == NULL)
        return;
    if (this->mResource->mHitPoints != 0 && this->mResource->mUnitClass->mUnitType != kUnitTypeFarm) {
        this->mResource->TakeHit(inUnit, UMFloatToLong(inUnit->mStats->mAttack), 0, 1);
        long theCategory = this->mResource->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryLand) {
            unsigned long theNewState = (this->mResource->mHitPoints > 0) + 10;
            if (theNewState != theState)
                inUnit->SetAnimation(theNewState, -1, 0);
        }
    } else {
        long theCategory = this->mResource->mUnitClass->mCategory;
        if ((theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryLand) && theState != 10)
            inUnit->SetAnimation(10, -1, 0);
        this->mResource->DepleteResource(1);
        inUnit->AddResourceAmount(1);
        inUnit->SetLastResource(this->mResource);
    }
    if (this->mResource->mUnitClass->mIsTree)
        this->mResource->RegrowForest(inUnit);
    long theRate = inUnit->mStats->mGatherTimes[EEGetResourceIndex(this->mResource)];
    if (this->mResource->mResourceType != kResourceStone)
        inUnit->SetWorkTimer(UMFloatToLong(
            EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->mWorkTimeFactor * theRate));
    else
        inUnit->SetWorkTimer(theRate);
}

// FUNCTION: EXE 0x624250
// Idles without a resource (or an AI player's farm), moves a fishing boat on to other fish, else waits facing the
// resource (animation 7 or 8) and stops gathering
// NEAR: the original keeps the dynamic_cast result in inUnit's argument slot and reloads mUnitClass
void EEUGGather::SearchNavalResource(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    long theRange = UMFloatToLong(inUnit->GetLineOfSight() * 5.0f);
    EEUCNavalCitizen* theClass = dynamic_cast<EEUCNavalCitizen*>(inUnit->GetUnitClass());
    if (this->mResource == NULL || inUnit->mPlayer->mComputerPlayer != NULL && this->mResource->mUnitClass->mUnitType == kUnitTypeFarm)
        inUnit->StartAction(kActionIdle);
    else if (this->mResource->mUnitClass->mUnitType == kUnitTypeResFish) {
        EEUnit* theResource = theClass->FindResourceOfKind(inUnit, this->mResource, this->mResource, theRange, this->mResource->GetResourceType(), &theUnits, true);
        if (theResource == NULL || theResource == this->mResource)
            return;
        this->SetResource(theResource);
        inUnit->StartAction(kActionMoveToWaitingArea);
    } else {
        inUnit->SetCurrentPhysicsAction(4);
        inUnit->FaceTarget(this->mResource);
        if (::gPhysics->mRandom.Generate() < 0.5f || inUnit->mStats->mCanFireOverWalls)
            inUnit->SetAnimation(7, -1, 0);
        else
            inUnit->SetAnimation(8, -1, 0);
        this->mGathering = false;
        return;
    }
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x62437e
// Sorts tiles nearest a given tile first (std::sort with EETileDistanceLess)

// FUNCTION: EXE 0x624464
// Sorts tiles farthest from the gatherer first (std::sort with EEGatherTileFarther)

// FUNCTION: EXE 0x624556
// Insertion-sorts a tile range by distance to a tile (std::_Insertion_sort, EETileDistanceLess)

// FUNCTION: EXE 0x624589
// Quick-sorts a tile range by distance to a tile, heap-sorting deep ranges (std::_Sort, EETileDistanceLess)

// FUNCTION: EXE 0x624696
// Insertion-sorts a tile range without bounds checks (std::_Unguarded_insertion_sort, EETileDistanceLess)

// FUNCTION: EXE 0x6246e0
// Insertion-sorts a tile range farthest first (std::_Insertion_sort, EEGatherTileFarther)

// FUNCTION: EXE 0x624714
// Quick-sorts a tile range farthest first, heap-sorting deep ranges (std::_Sort, EEGatherTileFarther)

// FUNCTION: EXE 0x62481d
// Insertion-sorts a tile range without bounds checks (std::_Unguarded_insertion_sort, EEGatherTileFarther)

// FUNCTION: EXE 0x624866
// Insertion-sorts a tile range by distance to a tile (std::_Insertion_sort_1, EETileDistanceLess)

// FUNCTION: EXE 0x624928
// Makes a tile range a heap by distance to a tile (std::make_heap, EETileDistanceLess)

// FUNCTION: EXE 0x62496c
// Sorts a tile heap by distance to a tile (std::sort_heap, EETileDistanceLess)

// FUNCTION: EXE 0x6249b9
// Partitions a tile range around a pivot tile (std::_Unguarded_partition, EETileDistanceLess)

// FUNCTION: EXE 0x624ad0
// Moves a tile back to its sorted place (std::_Unguarded_insert, EETileDistanceLess)

// FUNCTION: EXE 0x624b0c
// Insertion-sorts a tile range farthest first (std::_Insertion_sort_1, EEGatherTileFarther)

// FUNCTION: EXE 0x624bcd
// Makes a tile range a heap, farthest first (std::make_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x624c12
// Sorts a tile heap farthest first (std::sort_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x624c5e
// Partitions a tile range around a pivot tile (std::_Unguarded_partition, EEGatherTileFarther)

// FUNCTION: EXE 0x624d75
// Moves a tile back to its sorted place (std::_Unguarded_insert, EEGatherTileFarther)

// FUNCTION: EXE 0x624db1
// Builds a tile heap by distance to a tile (std::_Make_heap, EETileDistanceLess)

// FUNCTION: EXE 0x624e16
// Pops the top tile off a heap by distance to a tile (std::pop_heap, EETileDistanceLess)

// FUNCTION: EXE 0x624e49
// Builds a tile heap, farthest first (std::_Make_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x624ead
// Pops the top tile off a farthest-first heap (std::pop_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x624ee1
// Sifts a tile down a heap by distance to a tile (std::_Adjust_heap, EETileDistanceLess)

// FUNCTION: EXE 0x624f7b
// Pops the top tile off a heap by distance to a tile (std::_Pop_heap_0, EETileDistanceLess)

// FUNCTION: EXE 0x624ff3
// Sifts a tile down a farthest-first heap (std::_Adjust_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x62508c
// Pops the top tile off a farthest-first heap (std::_Pop_heap_0, EEGatherTileFarther)

// FUNCTION: EXE 0x625105
// Sifts a tile up a heap by distance to a tile (std::_Push_heap, EETileDistanceLess)

// FUNCTION: EXE 0x625168
// Moves the top tile of a heap by distance to a tile to the end (std::_Pop_heap, EETileDistanceLess)

// FUNCTION: EXE 0x6251bc
// Sifts a tile up a farthest-first heap (std::_Push_heap, EEGatherTileFarther)

// FUNCTION: EXE 0x62521f
// Moves the top tile of a farthest-first heap to the end (std::_Pop_heap, EEGatherTileFarther)

// TEMPLATE: EXE 0x625272
// EEAIContinentManager::GetForestIDAt (inline, defined at the top of this unit)
