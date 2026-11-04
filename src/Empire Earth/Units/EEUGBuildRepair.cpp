// C:\EEX\Empire Earth\Units\EEUGBuildRepair.cpp (file name inferred: no __FILE__; the key "EEUGBuildRepair-Data").
// Building or repairing a unit: 0x21e0c1..0x220986. Unit flags: the exe's module set. Data: the key 0x563538;
// vtable 0x44740c. The tail 0x220283..0x220986 is the std::sort instantiation for EETileDistanceLess (0x21e104).

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEMap.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "lle/SESoundManager.h"
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
#include "ee/EEComputerPlayer.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "lle/UResourceFile.h"
#include <algorithm>
#include "ee/EEStringIDs.h"
const UString EEUGBuildRepair::sData("EEUGBuildRepair-Data");
// SYNTHETIC: EXE 0x61e0c1   (wrapper)
// SYNTHETIC: EXE 0x61e0cb   (ctor)
// SYNTHETIC: EXE 0x61e0dc   (registrar)
// SYNTHETIC: EXE 0x61e0e8   (dtor)

// FUNCTION: EXE 0x61e104
// Orders units farthest from the reference tile first, so a sort leaves the nearest unit last
// Built without /Og: every local lives in the frame, `this` is spilled to [ebp-0x30] and the setg result is stored
// and reloaded (the /Og-off pragma alone does not make it exact, so it is not used)

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


// FUNCTION: EXE 0x61e164
// Creates a build/repair goal on a building (its first part), referencing it; caches its class, tech tree node, size
// and site, and counts it a repair when it is already built (never for a farm)
EEUGBuildRepair::EEUGBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg)
    : EEUnitGoal(kGoalBuildRepair, inArg)
{
    GE3DPoint thePosition;
    this->mPlayer = inPlayer;
    EEUnit* theTarget = inTarget;
    EEComplexUnit* theCarrier = inTarget->mComplexUnit;
    if (theCarrier != NULL) {
        do {
            theTarget = theCarrier;
            theCarrier = theCarrier->mPreviousPart;
        } while (theCarrier != NULL);
    }
    this->mTarget = theTarget;
    theTarget->Reference();
    this->mTargetClass = this->mTarget->mUnitClass;
    this->mTargetType = this->mTargetClass->mUnitType;
    this->mTechTreeID = gUnitClassTable[this->mTargetType] ? gUnitClassTable[this->mTargetType]->mTechnology : 100000;
    this->mTargetNode = this->mPlayer->GetTechTree()->Get(this->mTechTreeID);
    this->mTargetScale = this->mTarget->mUnitClass->mTileSize;
    if (theTarget->mComplexUnit != NULL)
        theTarget->mComplexUnit->GetPosition(thePosition);
    else {
        thePosition.SetX((float)theTarget->GetTileX() + 0.5f);
        thePosition.SetY((float)theTarget->GetTileY() + 0.5f);
    }
    this->SetTargetLocation(GE2DPoint(thePosition.GetX(), thePosition.GetY()));
    this->mFirstTickAmount = 0;
    this->mFirstTickAdjusted = false;
    this->mWorkerCountIncremented = false;
    this->mIsRepair = this->mTarget->mTileNode != NULL && this->mTarget->mComplexUnit->mIsBuilt && this->mTarget->mUnitClass->GetUnitType() != kUnitTypeFarm;
}

// FUNCTION: EXE 0x61e298
// Returns move-to-repair-site as the first action
long EEUGBuildRepair::GetStartAction()
{
    return kActionMoveToRepairSite;
}

// FUNCTION: EXE 0x61e29e
// Sets the build site's location (z 0) and recomputes its centre
void EEUGBuildRepair::SetTargetLocation(const GE2DPoint& inLocation)
{
    this->mTargetLocation.SetX(inLocation.GetX());
    this->mTargetLocation.SetY(inLocation.GetY());
    this->mTargetLocation.SetZ(0.0f);
    this->UpdateTargetCenter();
}

// FUNCTION: EXE 0x61e2ba
// Recomputes the build site's centre from its location and the building's tile size
void EEUGBuildRepair::UpdateTargetCenter()
{
    float theHalfScale = (float)this->mTargetScale * 0.5f;
    this->mTargetCenter.SetX(this->mTargetLocation.GetX() + theHalfScale);
    this->mTargetCenter.SetY(this->mTargetLocation.GetY() + theHalfScale);
}

// SYNTHETIC: EXE 0x61e2d6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x61e2f2
// Destroys the goal: takes the unit off the building's builder count if it was counted and releases the building
EEUGBuildRepair::~EEUGBuildRepair()
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(this->mTarget);
    if (this->mTarget != NULL) {
        if (this->mWorkerCountIncremented && theUnit->mNumBuilders > 0)
            theUnit->mNumBuilders--;
        this->mTarget->Dereference();
    }
}

// FUNCTION: EXE 0x61e342
// Returns whether the unit is within work reach of the building (a multi-part building's attack part) and, except
// at farms, stands on a tile next to it
bool EEUGBuildRepair::IsAtBuildSite(EEComplexUnit* inUnit)
{
    std::vector<EETileCoordinate> theTiles;
    bool theResult = false;
    bool theFound = false;
    if (this->mTarget != NULL) {
        EEComplexUnit* theTarget = this->mTarget->mComplexUnit;
        if (theTarget->mNextPart != NULL)
            theTarget = EEGetAttackPart(inUnit, theTarget);
        theResult = EEIsInWorkReach(inUnit, theTarget);
        if (theResult) {
            long theType = theTarget->mUnitClass->GetUnitType();
            if (theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm) {
                theTarget->mUnitClass->GetSurroundingTiles(theTarget, theTiles, 1, 0);
                unsigned long theCount = theTiles.size();
                for (unsigned long i = 0; i < theCount && !theFound; i++) {
                    EETileCoordinate& theTile = theTiles[i];
                    if (inUnit->GetTileX() == theTile.GetX() && inUnit->GetTileY() == theTile.GetY())
                        theFound = true;
                    else
                        theFound = false;
                }
                theResult = theFound;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61e410
// At full hit points completes an unbuilt building (technology, built flag, space energy wall, AI gates, local sound,
// panel refresh); if this is the unit's goal, also fills other parts, starts mines, uncounts the builder; returns that
bool EEUGBuildRepair::FinishBuilding(EEComplexUnit* inUnit)
{
    EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(this->mTarget);
    EEPlayer* thePlayer = inUnit->mPlayer;
    bool theResult = false;
    while (theTarget->mPreviousPart != NULL)
        theTarget = theTarget->mPreviousPart;
    if (theTarget != NULL && theTarget->mHitPoints >= theTarget->mStats->mMaxHitPoints) {
        if (!theTarget->mIsBuilt) {
            if (thePlayer != NULL && thePlayer->mTechnologies != NULL)
                thePlayer->mTechnologies->Research(theTarget->mUnitClass->GetTechnology(), NULL);
            thePlayer->CheckButtonsPreloaded(this->mTarget->mUnitClass->GetUnitType());
            theTarget->SetBuilt(1, 1, 1);
            if (theTarget->mUnitClass->GetUnitType() == kUnitTypeWallSpace
                && thePlayer->mTechnologies->Get(kTechWallSpace)->GetIsAvailable())
                EECreateSpaceWallEnergyWall(theTarget);
            if (thePlayer->mComputerPlayer != NULL)
                thePlayer->mComputerPlayer->mCivilizationManager->mWallManager->AddGates(theTarget);
            if (thePlayer == ::gWorld.GetCurrentPlayer()) {
                EESoundObject* theSound = theTarget->mStats->mCreationSound;
                if (theSound != NULL)
                    gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
            }
            if (gUIManager->GetMainGameForm() != NULL)
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
        }
        inUnit->PlayEffect(100000);
        inUnit->SetProductionTime(0);
        if (inUnit->mGoalState == this) {
            for (EEComplexUnit* theUnit = theTarget->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart)
                theUnit->SetHitPoints(theUnit->mStats->mMaxHitPoints);
            if (theTarget->mUnitClass->mCategory == kUnitCategoryMiningBuilding) {
                theTarget->StartAction(kActionGatherResource);
                theTarget->mUnitClass->ActionChanged(theTarget);
            }
            if (this->mWorkerCountIncremented) {
                if (theTarget->mNumBuilders > 0)
                    --theTarget->mNumBuilders;
                this->mWorkerCountIncremented = false;
            }
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61e5ea
// Heads for the next free tile around the building (kept as the site), else its position, and tells the unit's
// active group; without a building heads for the site or stays put
void EEUGBuildRepair::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    if (this->mTarget != NULL) {
        if (EEFindNextFreeTile(inUnit, this->mTarget->GetComplexUnit(), outLocation))
            this->SetTargetLocation(GE2DPoint(outLocation.GetX(), outLocation.GetY()));
        else
            this->mTarget->mComplexUnit->GetPosition(outLocation);
        if (inUnit->mActiveGroup != NULL)
            inUnit->mActiveGroup->TellGroup(inUnit, outLocation);
    } else if (this->mTargetLocation.GetX() > 0.0f)
        outLocation = this->mTargetLocation;
    else
        inUnit->GetPosition(outLocation);
}

// FUNCTION: EXE 0x61e680
// Returns the hit points one work tick adds (repair rate, or build time shared by the builders; all that is missing
// with instant build), at least 1; the first real tick counts the builder and the second is corrected by the first's
long EEUGBuildRepair::ComputeBuildAmount(EEComplexUnit* inWorker, bool inPreview)
{
    EEComplexUnit* theTarget = this->mTarget->mComplexUnit;
    while (theTarget->mPreviousPart != NULL)
        theTarget = theTarget->mPreviousPart;
    long theMaxHitPoints = theTarget->mStats->mMaxHitPoints;
    long theWorkers = theTarget->mNumBuilders;
    if (inPreview && theWorkers < 2)
        theWorkers = 1;
    long theAmount;
    if (!inWorker->mPlayer->mInstantBuild) {
        if (theTarget->mIsBuilt) {
            theAmount = UMCeil((inWorker->mStats->mRepairRate * 0.01f) * theMaxHitPoints);
            theAmount = UMCeil(3.0f / (theWorkers + 2.0f) * theAmount);
        } else {
            float theWorkerCount = (float)theWorkers;
            float theFactor = this->mTarget->mStats->mBuildTimeScale;
            float theMaximum = (float)theMaxHitPoints;
            float theRate = theMaximum / ((this->mTargetNode->GetBuildTime() * theFactor) * (3.0f / (theWorkerCount + 2.0f)));
            theAmount = min(UMCeil(inWorker->mStats->mWorkRate * theRate / theWorkerCount), theMaxHitPoints);
        }
    } else
        theAmount = theMaxHitPoints - theTarget->mHitPoints;
    theAmount = max(theAmount, 1);
    theAmount = min(theAmount, theMaxHitPoints - theTarget->mHitPoints);
    if (!this->mWorkerCountIncremented) {
        if (!inPreview) {
            this->mFirstTickAdjusted = false;
            this->mWorkerCountIncremented = true;
        }
        this->mFirstTickAmount = theAmount;
    } else if (!this->mFirstTickAdjusted && !inPreview) {
        theAmount = theAmount * 2 - this->mFirstTickAmount;
        this->mFirstTickAdjusted = true;
    }
    return theAmount;
}

// FUNCTION: EXE 0x61e81f
// Retargets a gate to the gate part the unit should work on
void EEUGBuildRepair::RetargetGatePart(EEComplexUnit* inUnit)
{
    if (this->mTarget->mUnitClass->mCategory == kUnitCategoryGate) {
        EEComplexUnit* theNew = EEGetAttackPart(inUnit, this->mTarget->mComplexUnit);
        this->ReplaceUnit(this->mTarget->mComplexUnit, theNew, NULL);
    }
}

// FUNCTION: EXE 0x61e853
// Kills the building if it is still a living foundation, refunding its cost to the unit's player, and gives the unit
// a goal to move to the site
void EEUGBuildRepair::CancelFoundation(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = NULL;
    EEComplexUnit* theTarget = this->mTarget->GetComplexUnit();
    while (theTarget->mPreviousPart != NULL)
        theTarget = theTarget->mPreviousPart;
    if (theTarget->IsFoundation() && theTarget->GetHitPoints() > 0) {
        theTarget->Kill(0, 0, 0);
        for (unsigned long i = 2; i < 7; i++) {
            long theValue = inUnit->GetPlayer()->ScaleCost(this->mTargetNode->GetCost(i, 0x20, 1.0f));
            inUnit->GetPlayer()->AddResource(i, theValue);
        }
    }
    if (this->mTargetLocation.GetX() <= 0.0f)
        theGoal = new EEUGLocation(&GE3DPoint(theTarget->mPhysicalModel.GetLocation()), false, true, false, 0.0f,
                                   false, false);
    else
        theGoal = new EEUGLocation(&this->mTargetLocation, false, true, false, 0.0f, false, false);
    inUnit->SetGoal(theGoal, 0, 0);
}

// FUNCTION: EXE 0x61e955
// Returns a copy of the goal for the same player and building
EEUnitGoal* EEUGBuildRepair::Clone()
{
    return new EEUGBuildRepair(this->mPlayer, this->mTarget, this->GetB1C());
}

// FUNCTION: EXE 0x61e985
// Writes the site location and centre, player, building, class and tech tree ids, size, first-tick amount and flags
// to the out-of-synch dump, then the base goal's
void EEUGBuildRepair::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Location (%6.6f, %6.6f, %6.6f)\n",
            this->mTargetLocation.GetX(), this->mTargetLocation.GetY(), this->mTargetLocation.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Center (%6.6f, %6.6f, %6.6f)\n",
            this->mTargetCenter.GetX(), this->mTargetCenter.GetY(), this->mTargetCenter.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Player Number %d\n", this->mPlayer->mPlayerID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Unit ID #%08x\n", this->mTarget ? this->mTarget->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Class %d\n", this->mTargetClass->mUnitType);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Tech Tree Node ID %d\n", this->mTargetNode->mData->mID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Tech Tree ID %d\n", this->mTechTreeID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Type %d\n", this->mTargetType);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Target Scale %d\n", this->mTargetScale);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Amount added to First Tick %d\n", this->mFirstTickAmount);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Construction Worker Count Incremented Flag Value %d\n",
            this->mWorkerCountIncremented == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair First Tick Adjustment Made Flag Value %d\n",
            this->mFirstTickAdjusted == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGBuildRepair Is Repair Flag Value %d\n", this->mIsRepair == true);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x61ea99
// Returns the base goal's checksum plus the site's raw coordinate bits, player, building id, class and tech ids, size,
// first-tick amount and flags
long EEUGBuildRepair::GetOutOfSynchChecksum()
{
    float theValue = this->mTargetLocation.GetX();
    long theChecksum = *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetLocation.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetLocation.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetCenter.GetX();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetCenter.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue) + this->mPlayer->mPlayerID;
    long theTargetID = this->mTarget ? this->mTarget->mUnitID : 0;
    theChecksum += theTargetID + this->mTechTreeID + this->mTargetType + this->mTargetScale + this->mFirstTickAmount + this->mTargetClass->mUnitType
        + (this->mWorkerCountIncremented == true) + (this->mFirstTickAdjusted == true) + (this->mIsRepair == true)
        + this->mTargetNode->mData->mID;
    return EEUnitGoal::GetOutOfSynchChecksum() + theChecksum;
}

// the build/repair search radius (0x447404; provisional name)
static const float kBuildRepairRadius = 4.0f;

// FUNCTION: EXE 0x61eb31
// Sends the unit to help build the first unbuilt, paid-for land building of its player within 4 tiles of a building;
// returns whether it found one
bool EEUGBuildRepair::HelpBuildNearby(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    std::vector<EEUnit*> theUnits;
    GE2DPoint theCenter;
    bool theResult = false;
    U2DPoint<long> theTile(inTarget->mTileX, inTarget->mTileY);
    theCenter = GE2DPoint((float)theTile.GetX(), (float)theTile.GetY());
    if (EEGetUnitsInRadius(theCenter, kBuildRepairRadius, theUnits) > 0) {
        std::vector<EEUnit*>::iterator i;
        for (i = theUnits.begin(); i != theUnits.end(); ++i) {
            EEComplexUnit* theUnit = (EEComplexUnit*)*i;
            if (!theUnit->mIsBuilt && theUnit != inTarget && theUnit->mHitPoints > 0
                && theUnit->mUnitClass->IsLandBuilding() && theUnit->mIsPaidFor && theUnit->mPlayer == inUnit->mPlayer)
                break;
        }
        if (i != theUnits.end()) {
            inUnit->SetGoal(new EEUGBuildRepair((*i)->mPlayer, *i, false), 0, 0);
            inUnit->StartAction(kActionMoveToRepairSite);
            inUnit->mUnitClass->ActionChanged(inUnit);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61ec58
// Sends the unit to work the nearest free farm around the farm's drop sites; returns whether one was found
bool EEUGBuildRepair::TendNextFarm(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EEUnit*> theFreeUnits;
    std::vector<EEUnit*> theUnits;
    bool theResult = false;
    long theCount = 1;
    if (EEGetFarmDropSites(inTarget, theUnits)) {
        for (std::vector<EEUnit*>::iterator i = theUnits.begin(); i != theUnits.end(); ++i) {
            while ((inTarget = EEFindFreeFarm((*i)->GetComplexUnit(), 1, &theCount)) != 0) {
                EEUnit* theFree = inTarget;
                theFreeUnits.push_back(theFree);
            }
        }
        if (theFreeUnits.size() > 0) {
            std::sort(theFreeUnits.begin(), theFreeUnits.end(), EETileDistanceLess(inUnit->GetTile()));
            EEWorkFarm(theFreeUnits.back()->mComplexUnit, inUnit);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61ed5e
// Sends the unit to gather a resource near a drop site, passing over mines at the player's worker limit (else a random
// one of those); returns whether a gather goal was set
// The original repeats the identical first search when it returns null; both calls and the second, unused vector
// are kept.
bool EEUGBuildRepair::GatherNearBuilding(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EEUnit*> theRejected;
    std::vector<EEUnit*> theUnused;
    bool theResult = false;
    EEUCBasicCitizen* theClass = dynamic_cast<EEUCBasicCitizen*>(inUnit->GetUnitClass());
    EEUnit* theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
    if (theResource == NULL)
        theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
    while (!theResult && theResource != NULL) {
        if (!theResource->mUnitClass->mIsTree && theResource->mUnitClass->mAnimalID == 1000
            && ::gWorld.GetResourceUsers(theResource) >= inUnit->mPlayer->mMiningLimit) {
            theRejected.push_back(theResource);
            theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
        } else {
            inUnit->SetGoal(new EEUGGather(theResource, inTarget, false), 0, 0);
            inUnit->StartAction(kActionMoveToWaitingArea);
            inUnit->mUnitClass->ActionChanged(inUnit);
            theResult = true;
        }
    }
    if (!theResult && theRejected.size() > 0) {
        EEAIRandomShuffle(theRejected.begin(), theRejected.end());
        inUnit->SetGoal(new EEUGGather(theRejected[0], inTarget, false), 0, 0);
        inUnit->StartAction(kActionMoveToWaitingArea);
        inUnit->mUnitClass->ActionChanged(inUnit);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x61ef11
// Appends the site tile, building type and builder-counted flag to the one-line debug text
void EEUGBuildRepair::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "build at %d,%d, obj %d (%s):", UMFloor(this->mTargetLocation.GetX()), UMFloor(this->mTargetLocation.GetY()),
            this->mTargetType, this->mWorkerCountIncremented ? "true" : "false");
    ioText += theText;
}

// FUNCTION: EXE 0x61ef9e
// Appends "BuildRepair", the site tile, the building's name and the builder-counted flag to the debug text
void EEUGBuildRepair::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "BuildRepair: at %d,%d\nObject:%s\nConstructionWorkerCountIncremented:%s\n",
            UMFloor(this->mTargetLocation.GetX()), UMFloor(this->mTargetLocation.GetY()),
            this->mTarget ? (const char*)this->mTarget->mUnitClass->mName : "None", this->mWorkerCountIncremented ? "true" : "false");
    ioText += theText;
}

// FUNCTION: EXE 0x61f03d
// Returns whether the target is a gate's middle part whose gate is not closed (despite the name)
bool EEUGBuildRepair::IsGateClosed(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mTarget->mUnitClass->mCategory == kUnitCategoryGate) {
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(this->mTarget->mUnitClass);
        EEComplexUnit* theUnit = this->mTarget->mComplexUnit;
        if (!theGate->IsSidePart(theUnit) && (theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionClosed)
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61f09d
// Runs the current action: walking, gate retargeting, an insufficient-resources event, next work (human players),
// cancelling to a location goal, waiting for the site to clear or starting a work cycle
void EEUGBuildRepair::Step(EEComplexUnit* inUnit)
{
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionMoveToRepairSite:
        inUnit->SetCurrentPhysicsAction(0);
        break;
    case kActionChangeTargetForSpecialRepair:
        this->RetargetGatePart(inUnit);
        break;
    case kActionInsufficientResources:
        this->AddBuildEvent();
        break;
    case kActionTaskSmartCitizen:
        if (inUnit->mPlayer->IsHuman())
            this->FindNextWork(inUnit);
        break;
    case kActionConvertToLocationGoal:
        this->CancelFoundation(inUnit);
        break;
    case kActionWaitForBuildSiteToClear:
        this->BeginRepair(inUnit);
        break;
    case kActionRepairBuilding:
        this->BeginBuilding(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x61f138
// Posts a player event at the building's tile (string 0x7c42 for a repair, else 0x273a)
void EEUGBuildRepair::AddBuildEvent()
{
    U2DPoint<long> theTile(this->mTarget->mTileX, this->mTarget->mTileY);
    EEPlayer* thePlayer = this->mPlayer;
    thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, this->mIsRepair ? 0x7c42 : 0x273a, false);
}

// FUNCTION: EXE 0x61f177
// Reads the building id (resolved by RepairPointers), builder-counted flag, first-tick state and repair flag from the
// saved game; returns the first read error
long EEUGBuildRepair::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    unsigned long theTargetID;
    theResult = inFile.Read(theTargetID);
    if (theResult != 0)
        return theResult;
    this->mTarget = reinterpret_cast<EEUnit*>(theTargetID);
    theResult = inFile.Read(this->mWorkerCountIncremented);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mFirstTickAdjusted);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mFirstTickAmount);
    if (theResult == 0)
        theResult = inFile.Read(this->mIsRepair);
    return theResult;
}

// FUNCTION: EXE 0x61f1e4
// Tells the computer player's build manager that its unbuilt building is abandoned
void EEUGBuildRepair::Abandon(EEComplexUnit* inUnit)
{
    EEComputerPlayer* theComputer = inUnit->mPlayer->mComputerPlayer;
    if (theComputer != NULL && this->mTarget != NULL && !this->mTarget->mComplexUnit->mIsBuilt)
        theComputer->mCivilizationManager->mBuildManager->BuildingAbandoned(this->mTarget->mComplexUnit);
}

// FUNCTION: EXE 0x61f21a
// Starts a work cycle: faces the building, plays the work effect and animation, drops off a load it takes, sets the
// work timer and remaining production time and stops the unit
void EEUGBuildRepair::BeginBuilding(EEComplexUnit* inUnit)
{
    EEComplexUnit* theTarget = this->mTarget->mComplexUnit;
    inUnit->PlayEffect(this->mTechTreeID);
    inUnit->FaceTarget(this->mTarget);
    inUnit->SetAnimation(1, -1, 0);
    if (inUnit->mResourceAmount > 0 && theTarget != NULL && theTarget->mHitPoints > 0 && theTarget->mIsBuilt
        && theTarget->CanTake(inUnit, inUnit->mResourceType))
        EEDepositLoad(inUnit, theTarget);
    if (theTarget->mIsBuilt)
        inUnit->SetWorkTimer(1000);
    else
        inUnit->SetWorkTimer(inUnit->mStats->mWorkRate);
    long theMaxHitPoints = this->mTarget->mStats->mMaxHitPoints;
    long theHitPoints = this->mTarget->mHitPoints;
    long theTime;
    if (!inUnit->mPlayer->mInstantBuild)
        theTime = UMFloatToLong(this->mTargetNode->GetBuildTime() * ((float)(theMaxHitPoints - theHitPoints) / theMaxHitPoints));
    else
        theTime = 1;
    inUnit->SetProductionTime(theTime);
    inUnit->DropPath();
    inUnit->SetCurrentPhysicsAction(6);
    inUnit->SetVelocity(0.0f);
    inUnit->mAngularVelocity = GEOrientation();
}

// FUNCTION: EXE 0x61f350
// Turns the loaded id back into the building and recomputes its player, class, tech data and site; returns 3 when the
// building or its technology is missing; saves up to minor version 0x4c clear the builder-counted flag
long EEUGBuildRepair::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    GE3DPoint thePosition;
    long theResult = 3;
    if (reinterpret_cast<unsigned long>(this->mTarget) != -1) {
        this->mTarget = static_cast<EEUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget)));
        if (this->mTarget != NULL) {
            this->mTarget->Reference();
            this->mPlayer = this->mTarget->mPlayer;
            this->mTargetClass = this->mTarget->mUnitClass;
            this->mTargetType = this->mTargetClass->mUnitType;
            this->mTechTreeID = gUnitClassTable[this->mTargetType] ? gUnitClassTable[this->mTargetType]->mTechnology : 100000;
            this->mTargetNode = this->mPlayer->GetTechTree()->Get(this->mTechTreeID);
            this->mTargetScale = this->mTargetClass->mTileSize;
            theResult = this->mTechTreeID != kTechNone ? 0 : 3;
            if (this->mTarget->mComplexUnit != NULL)
                this->mTarget->mComplexUnit->GetPosition(thePosition);
            else {
                thePosition.SetX((float)this->mTarget->GetTileX() + 0.5f);
                thePosition.SetY((float)this->mTarget->GetTileY() + 0.5f);
            }
            this->SetTargetLocation(GE2DPoint(thePosition.GetX(), thePosition.GetY()));
        }
    } else
        this->mTarget = NULL;
    if (inMinorVersion <= 0x4c)
        this->mWorkerCountIncremented = false;
    return theResult;
}

// FUNCTION: EXE 0x61f451
// Starts moving to the site, or looking for next work when there is no building
void EEUGBuildRepair::Start(EEComplexUnit* inUnit)
{
    if (this->mTarget == NULL)
        inUnit->StartAction(kActionTaskSmartCitizen);
    else
        inUnit->StartAction(kActionMoveToRepairSite);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x61f47f
// Writes the building id (-1 once the building is gone), builder-counted flag, first-tick state and repair flag; returns
// the first write error
long EEUGBuildRepair::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    // ReplaceUnit clears the building when an enemy takes it over: save -1, which RepairPointers reads as no
    // building, instead of reading the id through NULL
    unsigned long theTargetID = this->mTarget != NULL ? this->mTarget->GetUnitID() : (unsigned long)-1;
    theResult = outFile.Write(theTargetID);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mWorkerCountIncremented);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFirstTickAdjusted);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFirstTickAmount);
    if (theResult == 0)
        theResult = outFile.Write(this->mIsRepair);
    return theResult;
}

// FUNCTION: EXE 0x61f4f5
// Returns whether units other than the building block its footprint (only the last footprint tile decides)
bool EEUGBuildRepair::IsBuildSiteTaken()
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    bool theResult = false;
    EEGetFootprintTiles(this->mTarget, theTiles);
    for (unsigned long i = 0; i < theTiles.size(); ++i) {
        EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[i].GetX(), theTiles[i].GetY());
        EEGetBlockingUnits(this->mPlayer, this->mTargetType, theTile, 0, theUnits);
        if (theUnits.size() > 1 || (theUnits.size() == 1 && theUnits[0] != this->mTarget))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61f5fc
// Returns whether no blocking or movable units stand on the building's footprint (the building, a runway under type
// 0x42 and a gate's own parts excepted); farms always pass
bool EEUGBuildRepair::IsFootprintClear()
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theLinked;
    bool theResult = true;
    if (this->mTargetType != 0x2a && this->mTargetType != 0x33a) {
        if (this->mTarget->GetUnitClass()->GetCategory() == kUnitCategoryGate) {
            EEComplexUnit* theUnit = this->mTarget->GetComplexUnit();
            while (theUnit->mPreviousPart != NULL)
                theUnit = theUnit->mPreviousPart;
            theLinked.push_back(theUnit);
            for (theUnit = theUnit->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart)
                theLinked.push_back(theUnit);
        }
        EEGetFootprintTiles(this->mTarget, theTiles);
        for (unsigned long i = 0; i < theTiles.size() && theResult; ++i) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[i].GetX(), theTiles[i].GetY());
            EEGetBlockingUnits(this->mPlayer, this->mTargetType, theTile, 0, theUnits);
            if (theUnits.empty() || (theUnits.size() == 1 && theUnits[0] == this->mTarget)
                || (theUnits.size() == 1 && theUnits[0]->GetUnitClass()->GetUnitType() == kUnitTypeAirportRunway && this->mTargetType == 0x42))
                theResult = !EEGetMovableTileUnits(this->mPlayer, this->mTargetType, theTile, 0, theUnits);
            else
                theResult = false;
            if (!theResult && theLinked.size() > 0) {
                for (unsigned long j = 0; j < theUnits.size() && !theResult; ++j)
                    theResult = std::find(theLinked.begin(), theLinked.end(), theUnits[j]) != theLinked.end();
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x61f85e
// Returns whether no unit on the footprint stands in the way (standing 2) and no foundation above 1 hit point is
// there (the building, the player's runway under type 0x42 and a gate's own parts excepted)
bool EEUGBuildRepair::IsSiteFreeOfUnits()
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theLinked;
    bool theResult = true;
    EEGetFootprintTiles(this->mTarget, theTiles);
    EEGetUnitsOn(theTiles, theUnits);
    if (this->mTarget->mUnitClass->mCategory == kUnitCategoryGate) {
        EEComplexUnit* theUnit = this->mTarget->mComplexUnit;
        while (theUnit->mPreviousPart != NULL)
            theUnit = theUnit->mPreviousPart;
        theLinked.push_back(theUnit);
        for (theUnit = theUnit->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart)
            theLinked.push_back(theUnit);
    }
    unsigned long i = 0;
    unsigned long theCount = theUnits.size();
    for (; i < theCount && theResult; ++i) {
        if (theUnits[i] == this->mTarget
            || (theUnits[i]->mUnitClass->GetUnitType() == kUnitTypeAirportRunway && this->mTargetType == 0x42 && theUnits[i]->mPlayer == this->mPlayer)
            || ((theLinked.size() > 0) && std::find(theLinked.begin(), theLinked.end(), theUnits[i]) != theLinked.end()))
            continue;
        theResult = EEGetTileUnitStanding(theUnits[i], this->mPlayer, this->mTargetType, false) != 2;
        if (theUnits[i]->IsFoundation() && theUnits[i]->mHitPoints > 1)
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x61faa0
// Finds the unit's next job after the building: work the farm (or the next free one), drop off at a granary and tend
// a farm, build the nearest site after a wall, drop off and gather at a drop site, else help build nearby
void EEUGBuildRepair::FindNextWork(EEComplexUnit* inUnit)
{
    if (this->mTarget == NULL)
        return;
    EEUnitClass* theClass = this->mTarget->mUnitClass;
    long theType = theClass->GetUnitType();
    EEComplexUnit* theTarget = this->mTarget->mComplexUnit;
    if (theType == kUnitTypeFarm) {
        if (::gWorld.GetResourceUsers(this->mTarget) < 1) {
            inUnit->SetGoal(new EEUGGather(this->mTarget, 0, false), 0, 0);
            inUnit->StartAction(kActionMoveToResource);
            inUnit->mUnitClass->ActionChanged(inUnit);
        } else
            EEUGBuildRepair::TendNextFarm(inUnit, theTarget);
    } else if (this->mTarget->mUnitClass->mUnitType == kUnitTypeGranaryImproved || this->mTarget->mUnitClass->mUnitType == kUnitTypeGranary || theType == kUnitTypeRoboticFarm) {
        if (inUnit->mResourceAmount > 0 && theTarget->CanTake(inUnit, inUnit->mResourceType))
            EEDepositLoad(inUnit, theTarget);
        EEUGBuildRepair::TendNextFarm(inUnit, theTarget);
    } else if (theClass->mIsWall && !theClass->mIsTower) {
        EEUnit* theNext = inUnit->mPlayer->GetNearestConstructionSite(inUnit);
        if (theNext != NULL) {
            inUnit->SetGoal(new EEUGBuildRepair(theNext->GetPlayer(), theNext, false), 0, 0);
            inUnit->StartAction(kActionMoveToRepairSite);
            inUnit->mUnitClass->ActionChanged(inUnit);
        }
    } else if (theClass->IsDropSite() && theTarget != NULL) {
        if (inUnit->mResourceAmount > 0 && theTarget->CanTake(inUnit, inUnit->mResourceType))
            EEDepositLoad(inUnit, theTarget);
        if (this->mB1C)
            EEUGBuildRepair::GatherNearBuilding(inUnit, theTarget);
    } else
        EEUGBuildRepair::HelpBuildNearby(inUnit, this->mTarget);
}

// FUNCTION: EXE 0x61fc1e
// Moves units standing on the building's site aside
void EEUGBuildRepair::ClearBuildSite(EEComplexUnit* inUnit)
{
    EETileCoordinate theTile(this->mTarget->mTileX, this->mTarget->mTileY);
    EEStepAsideFromSite(this->mTarget, this->mTarget->mUnitClass, this->mTarget->GetPlayer(), theTile, 0);
}

// FUNCTION: EXE 0x61fc4b
// Answers the build/repair triggers (destroyed, finished: completes it, not paid for, repair unaffordable, site in
// range, clear: clears it, visible: within sight squared plus size, valid ...); others go to the common goal
bool EEUGBuildRepair::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition;
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(this->mTarget);
    bool theResult = false;
    switch (inEvent) {
    case kTriggerBuildingNotPaidFor:
        if (!theUnit->mIsPaidFor && this->mTarget != NULL) {
            EEComplexUnit* theOuter = theUnit;
            while (theOuter->mPreviousPart != NULL)
                theOuter = theOuter->mPreviousPart;
            if (!this->mTarget->mPlayer->CanProduce(theOuter->mUnitClass->GetTechnology(), 0x20, 1, true, 3, NULL)) {
                theResult = true;
                break;
            }
        }
        theResult = false;
        break;
    case kTriggerGoalIsBuild:
        if (!this->mIsRepair)
            theResult = true;
        else if (this->mTarget != NULL) {
            long theType = this->mTarget->mUnitClass->GetUnitType();
            if (theType == kUnitTypeGranaryImproved || theType == kUnitTypeGranary)
                theResult = true;
            else
                theResult = false;
        }
        break;
    case kTriggerCannotAffordRepair:
        if (this->mIsRepair) {
            EEComplexUnit* theOuter = theUnit;
            while (theOuter->mPreviousPart != NULL)
                theOuter = theOuter->mPreviousPart;
            long theAmount = this->ComputeBuildAmount(inUnit, true);
            theResult = !inUnit->mPlayer->CanAffordRepair(theOuter, inUnit, theAmount);
        }
        break;
    case kTriggerRepairIsSpecialUnit:
        theResult = this->IsGateClosed(inUnit);
        break;
    default:
        theResult = EEUnitGoal::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerBuildingDestroyed:
        if (this->mTarget == NULL || this->mTarget->mHitPoints == 0)
            theResult = true;
        else
            theResult = false;
        break;
    case kTriggerBuildingRepaired:
        if (this->mTarget == NULL || this->FinishBuilding(inUnit))
            theResult = true;
        break;
    case kTriggerRepairSiteInRange:
        theResult = this->IsAtBuildSite(inUnit);
        break;
    case kTriggerBuildSiteClear:
        theResult = this->IsFootprintClear();
        if (!theResult && this->mTarget != NULL) {
            long theType = this->mTarget->mUnitClass->GetUnitType();
            if (theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm)
                this->ClearBuildSite(this->mTarget->mComplexUnit);
        }
        break;
    case kTriggerBuildSiteVisible:
        if (this->mTarget != NULL) {
            const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
            thePosition.SetX(theTransformation.GetX());
            thePosition.SetY(theTransformation.GetY());
            thePosition.SetZ(theTransformation.GetZ());
            long theScale = this->mTarget->mUnitClass->mTileSize;
            float theRange = inUnit->GetLineOfSight() * inUnit->GetLineOfSight();
            theRange += theScale;
            if (thePosition.DistanceToSquared(this->mTargetCenter) <= theRange)
                theResult = true;
            else
                theResult = false;
        }
        break;
    case kTriggerBuildSiteValid:
        theResult = this->IsSiteFreeOfUnits();
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x61feb2
// Follows a building replaced by another unit, dropping the target when the new unit belongs to an enemy
void EEUGBuildRepair::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (this->mTarget == NULL || this->mTarget->mComplexUnit == NULL || this->mTarget->mComplexUnit != inOld)
        return;
    this->mTarget->mComplexUnit->Dereference();
    EEUnit* theTarget;
    if (inNew->mPlayer == this->mPlayer || this->mPlayer->mEnemies[inNew->mPlayer->mPlayerID] == 0) {
        theTarget = inNew;
        inNew->Reference();
    } else
        theTarget = NULL;
    this->mTarget = theTarget;
}

// FUNCTION: EXE 0x61fefe
// Adds a work tick of hit points (repairs only when affordable, paid per tick); a foundation becomes a building site
// (media loaded, others' wonders announced); sets the remaining production time and the next work timer
void EEUGBuildRepair::Work(EEComplexUnit* inUnit)
{
    if (this->mTarget == NULL)
        return;
    EEComplexUnit* theTarget = this->mTarget->mComplexUnit;
    while (theTarget->mPreviousPart != NULL)
        theTarget = theTarget->mPreviousPart;
    long theMaximum = theTarget->mStats->mMaxHitPoints;
    if (theTarget->mHitPoints < theMaximum && !::gWorld.mInCinematic) {
        if (!this->mWorkerCountIncremented)
            ++theTarget->mNumBuilders;
        long theAmount = this->ComputeBuildAmount(inUnit, false);
        if ((this->mTargetType == 0x2a || this->mTargetType == 0x33a) && theTarget->mHitPoints == 1 && !theTarget->mIsPaidFor) {
            theTarget->mIsPaidFor = true;
            inUnit->mPlayer->Pay(this->mTargetNode, 0x20, 1.0f);
        }
        if (!this->mIsRepair || inUnit->mPlayer->CanAffordRepair(theTarget, inUnit, theAmount)) {
            long theHitPoints = theTarget->mHitPoints + theAmount;
            if (theHitPoints > theMaximum)
                theHitPoints = theMaximum;
            if (theTarget->IsFoundation() && theHitPoints > 1) {
                theTarget->SetHitPoints(theHitPoints);
                if (this->mTargetNode->mData->mKind == 4) {
                    if (theTarget->mPlayer != ::gWorld.GetCurrentPlayer()) {
                        UWideString theFormat;
                        UWideString theName;
                        UWideString theText;
                        gResource->GetString(IDS_HAS_STARTED_CONSTRUCTING_WONDER, theFormat);
                        gResource->GetString(this->mTarget->mUnitClass->mNameStringID, theName);
                        sprintf(theText, (const char*)theFormat, (const char*)theTarget->mPlayer->mName,
                                (const char*)theName);
                        gUserInterface->AddChatLogEntry(theText, 1);
                    }
                    EESoundObject* theSound = EESoundObject::Get(kSoundWonderClock);
                    if (theSound != NULL)
                        theSound->Play();
                }
                theTarget->mPlayer->LoadUnitClassMedia(this->mTarget->mUnitClass->GetUnitType());
                theTarget->LoadMedia(false);
                gDataPreLoading->PostUnloadUnitClass(theTarget->mPlayer->mPlayerID, theTarget->mUnitClass->GetUnitType());
                for (EEComplexUnit* theUnit = theTarget->mNextPart; theUnit != NULL; theUnit = theUnit->mNextPart) {
                    theUnit->SetHitPoints(2);
                    theUnit->mPlayer->LoadUnitClassMedia(theUnit->mUnitClass->GetUnitType());
                    theUnit->LoadMedia(false);
                    gDataPreLoading->PostUnloadUnitClass(theUnit->mPlayer->mPlayerID, theUnit->mUnitClass->GetUnitType());
                    theUnit->PartFinished();
                }
                theTarget->PartFinished();
            } else
                theTarget->SetHitPoints(theHitPoints);
            if (this->mIsRepair)
                theTarget->mPlayer->Pay(this->mTargetNode, 0x20, (float)theAmount / (float)theMaximum);
            long theRemaining;
            theMaximum = theTarget->mStats->mMaxHitPoints;
            theHitPoints = theTarget->mHitPoints;
            if (!inUnit->mPlayer->mInstantBuild) {
                float theSpeed = theTarget->mStats->mBuildTimeScale;
                long theCost = this->mTargetNode->GetBuildTime();
                theRemaining = UMFloatToLong(((float)(theMaximum - theHitPoints) / (float)theMaximum) * ((float)theCost * theSpeed));
            } else
                theRemaining = 1;
            inUnit->SetProductionTime(theRemaining);
        }
    }
    if (theTarget->mIsBuilt)
        inUnit->SetWorkTimer(1000);
    else
        inUnit->SetWorkTimer(inUnit->mStats->mWorkRate);
}

// FUNCTION: EXE 0x620216
// Stops the unit to wait for the site (animation 7) and, except at farms, moves units off the site
void EEUGBuildRepair::BeginRepair(EEComplexUnit* inUnit)
{
    inUnit->SetCurrentPhysicsAction(3);
    inUnit->SetVelocity(0.0f);
    inUnit->mAngularVelocity = GEOrientation();
    inUnit->SetAnimation(7, -1, 0);
    long theType = this->mTarget->mUnitClass->GetUnitType();
    if (theType != kUnitTypeFarm && theType != kUnitTypeRoboticFarm)
        this->ClearBuildSite(this->mTarget->mComplexUnit);
}

// TEMPLATE: EXE 0x620283
// std::sort<_Ptrit<EEUnit*>, EETileDistanceLess> (TendNextFarm) and its helpers (identified by call graph)

// TEMPLATE: EXE 0x620367
// std::_Insertion_sort<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x62039a
// std::_Sort<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x6204a2
// std::_Unguarded_insertion_sort<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x6204e4
// std::_Insertion_sort_1<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620595
// std::make_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x6205d9
// std::sort_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620626
// std::_Unguarded_partition (the median of three inlined)<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620746
// std::_Unguarded_insert<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620779
// std::_Make_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x6207d6
// std::pop_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620809
// std::_Adjust_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620883
// std::_Pop_heap_0<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x6208f3
// std::_Push_heap<_Ptrit<EEUnit*>, EETileDistanceLess>

// TEMPLATE: EXE 0x620942
// std::_Pop_heap<_Ptrit<EEUnit*>, EETileDistanceLess>
