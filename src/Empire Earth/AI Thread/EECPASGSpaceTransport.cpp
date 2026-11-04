// C:\EEX\Empire Earth\AI Thread\EECPASGSpaceTransport.cpp (file name inferred: no __FILE__). 0x5096f..0x513b7 (unit map
// edges, not verified yet).
#include <windows.h>
#include <vector>
#include <set>

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
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPASGSea.h"
#include "ee/EECPASGSpaceship.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGTransport.h"
#include "ee/EECPASGSpaceTransport.h"
#include "ee/EECPASGStats.h"

#include "ee/EEComputerPlayer.h"
#include "ee/EEUnitUtilities.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEMathInline.h"
#include "ee/EEUnitUtilities.h"

// FUNCTION: EXE 0x45096f
// Creates a space transport sub-group for the attack group, with no land group to carry yet
EECPASGSpaceTransport::EECPASGSpaceTransport(EECPAttackGroup* inGroup, long inKind)
    : EECPASGSpaceship(inGroup, inKind), EECPASGTransport()
{
    this->mLand = NULL;
    this->mOwner = this;
}

// SYNTHETIC: EXE 0x4509ad
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4509c9
// Destroys the space transport sub-group (base members only)
EECPASGSpaceTransport::~EECPASGSpaceTransport()
{
}

// FUNCTION: EXE 0x4509f2
// Returns the share of transports in danger, at least 0.75 if an endangered one carries cargo; 0 while redirecting
float EECPASGSpaceTransport::GetDangerRatio(long inArg1)
{
    float theResult = 0.0f;
    long theCount = 0;
    long theCargo = 0;
    long theTotalCargo = 0;
    if (this->mState != kASGStateRedirectCargo) {
        for (long i = this->mUnits.size() - 1; i >= 0; i--) {
            // The original (0x450a3f, 0x450a44) also sums the total cargo, never used, before testing each unit
            theTotalCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            if (this->mUnits[i]->IsInDanger()) {
                theCount++;
                theCargo += this->mUnits[i]->GetUnit()->GetNumCargoUnits();
            }
        }
        if (theCount > 0) {
            theResult = (float)theCount / this->mUnits.size();
            if (theCargo > 0 && 0.75f > theResult)
                theResult = 0.75f;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x450ab8
// Returns the total transport capacity of the sub-group's units
long EECPASGSpaceTransport::GetCargoCapacity()
{
    long theSum = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        theSum += this->mUnits[i]->GetUnit()->GetUnitClass()->mTransportCapacity;
    return theSum;
}

// FUNCTION: EXE 0x450af9
// Moves the rally point to where a ship would dock at one of the player's docks (picked after a shuffle);
// returns the rally point
EETileLocation EECPASGSpaceTransport::GetRallyPoint(bool inIncludeAllies)
{
    std::vector<EEUnit*> theUnits;
    if (this->mUnits.empty())
        return this->mRallyPoint.mTile;
    EEGetPlayerUnitsOfType(kUnitTypeDock, this->mPlayer2->mPlayerID, theUnits);
    if (theUnits.size() > 0) {
        GETransformation theTransformation;
        GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
        Fn4EEF9(theUnits, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
        if (EEGetDockShipFacing(&theLocation, this->mUnits.front()->GetUnit()->GetUnitClass(),
                      theUnits.front(), false, &theTransformation)) {
            this->mRallyPoint.mTile = EETileLocation(UMFloor(theTransformation.GetX()), UMFloor(theTransformation.GetY()));
        }
    }
    return this->mRallyPoint.mTile;
}

// FUNCTION: EXE 0x450c4a
// Switches state: idle gets a task, remove/move/load/scatter run their handlers, carrying puts the land group in
// transit, release drops the land group and makes a return task to a fresh rally point; others go to the base
void EECPASGSpaceTransport::SetState(long inState, long inTime)
{
    unsigned long theOldState = this->mState;
    this->mState = inState;
    switch (this->mState) {
    case kASGStateIdle:
        if (this->mUnits.size() > 0 && !this->mTask.mIsActive)
            this->mGroup->AssignTask(this);
        break;
    case kASGStateRemove:
        this->RequestRemoval();
        break;
    case kASGStateMove:
        this->mNextWaypoint = this->mTask.mLocation;
        this->MoveToDestination();
        break;
    case kASGStateLoad:
        this->LoadCargo();
        break;
    case kASGStateCarryCargo:
        if (this->mLand != NULL && this->mLand->mState != kASGStateInTransit)
            this->mLand->SetState(kASGStateInTransit, inTime);
        break;
    case kASGStateReleaseCargo:
        if (this->mLand != NULL) {
            if (this->mLand->mTransport == this)
                this->mLand->ReleaseTransport();
            EECPASGTransport::ReleaseLand();
        }
        this->GetRallyPoint(false);
        this->ReturnToRallyPoint();
        this->mTask.mLocation = this->mRallyPoint;
        this->mTask.mType = kAGTaskReturn;
        this->mTask.mIsActive = true;
        break;
    case kASGStateRedirectCargo:
        break;
    case kASGStateScatter:
        this->ScatterDestination();
        break;
    default:
        this->mState = theOldState;
        EECPASGSpaceship::SetState(inState, inTime);
        break;
    }
}

// FUNCTION: EXE 0x450ddc
// Loads the sub-group and then its transport data from a saved game; returns the first error
long EECPASGSpaceTransport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EECPASGSpaceship::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = EECPASGTransport::Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// FUNCTION: EXE 0x450e0d
// Moves the sub-group to a random tile within 5 tiles of its current position
void EECPAttackSubGroup::ScatterDestination()
{
    EEAIAStarBounds theBounds;
    EETileLocation theCenter(0, 0);
    this->FindPosition(theCenter, true, true);
    theBounds.Set(theCenter);
    theBounds.Grow(5, true);
    long theY = gPhysics->GetRandom(theBounds.mTop, theBounds.mBottom);
    long theX = gPhysics->GetRandom(theBounds.mLeft, theBounds.mRight);
    this->mNextWaypoint.mTile = EETileLocation(theX, theY);
    this->MoveToDestination();
}

// FUNCTION: EXE 0x450e95
// Places the formation at the next waypoint (the task location if unset, the rally point if off the map),
// snapped to the closest map tile
void EECPASGSpaceTransport::MoveToDestination()
{
    EECPILocation theLocation;
    GE3DPoint thePoint(0.0f, 0.0f, 0.0f);
    if (this->mNextWaypoint.mTile.mX == 0 && this->mNextWaypoint.mTile.mY == 0)
        this->mNextWaypoint = this->mTask.mLocation;
    if (gWorld.mMap->GetTileAtLocation(this->mNextWaypoint.mTile) == NULL)
        this->mNextWaypoint.mTile = this->mRallyPoint.mTile;
    thePoint.mX = this->mNextWaypoint.mTile.mX;
    thePoint.mY = this->mNextWaypoint.mTile.mY;
    if (gWorld.mMap->GetTile(this->mNextWaypoint.mTile.mX, this->mNextWaypoint.mTile.mY) == NULL) {
        long theX;
        long theY;
        gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY);
        if (gWorld.mMap->GetTile(theX, theY) != NULL) {
            thePoint.mX = theX;
            thePoint.mY = theY;
        }
    }
    theLocation.mTile.mX = (long)thePoint.mX;
    theLocation.mTile.mY = (long)thePoint.mY;
    this->PlaceFormation(theLocation);
}

// FUNCTION: EXE 0x450fa8
// Writes the sub-group and then its transport data to the saved game; returns the first error
long EECPASGSpaceTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EECPAttackSubGroup::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = EECPASGTransport::Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// FUNCTION: EXE 0x450fd9
// Tests a condition: request filled; task valid or has cargo = the land group has units; should return, all
// aboard, all unloaded, loading blocked; others go to the base
// (EECPASGSeaTransport's identical copy folds into this one: ICF)
bool EECPASGSpaceTransport::TestCondition(long inState, long inTime)
{
    bool theResult = false;
    switch ((unsigned long)inState) {   // unsigned compares (ja), as the base 0x4604cf
    case kASGConditionRequestFilled:
        theResult = this->IsRequestFilled();
        break;
    case kASGConditionTaskValid:
    case kASGConditionHasCargo:
        theResult = this->mLand != NULL && this->mLand->GetNumUnits() > 0;
        break;
    case kASGConditionTaskInvalid:
        theResult = this->mLand == NULL;
        break;
    case kASGConditionShouldReturn:
        theResult = this->mLand == NULL || this->mUnits.size() == 0 || (this->mTask.mIsActive && this->mTask.mType == kAGTaskReturn);
        break;
    case kASGConditionAllAboard:
        theResult = this->mLand != NULL && this->mLand->CountUnitsAboard() == this->mLand->GetNumUnits();
        break;
    case 4001:
    case kASGConditionAnyAboard:
        break;
    case kASGConditionAllUnloaded:
        theResult = this->mLand == NULL || (this->mLand->CountUnitsAboard() == 0 && this->mCargoStage == 2);
        break;
    case kASGConditionLoadingBlocked:
        theResult = this->IsLoadingBlocked();
        break;
    default:
        theResult = EECPASGSpaceship::TestCondition(inState, inTime);
        break;
    }
    return theResult;
}

// IsLoadingBlocked (ICF-folded into EECPASGSeaTransport's copy 0x44ed08): true if a ready transport waiting to
// load has an adjacent passenger that can stand on neither tile one turn left or right of it
bool EECPASGSpaceTransport::IsLoadingBlocked()
{
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    bool theResult = false;
    for (long i = this->mUnits.size() - 1; i >= 0 && !theResult; i--) {
        EEComplexUnit* theUnit = (*(this->mUnits.begin() + i))->GetUnit();
        if ((*(this->mUnits.begin() + i))->IsDone() || theUnit->mIsDisabled || theUnit->mHitPoints <= 0 || theUnit->mWaypoints.size() != 0)
            continue;
        EEUnitGoal* theGoal = theUnit->mActiveGoal;
        if (theGoal == NULL || theGoal->mKind != kGoalLoadTransport)
            continue;
        EEUGLoadContainer* theLoad = static_cast<EEUGLoadContainer*>(theGoal);
        for (long j = theLoad->mUnits.mItems.size() - 1; j >= 0 && !theResult; j--) {
            EEComplexUnit* theOther = *(theLoad->mUnits.mItems.begin() + j);
            if (theOther == NULL || theOther->mTileX == -10000 || theOther->mIsDisabled || theOther->mHitPoints <= 0)
                continue;
            if (UMAbs(theUnit->mTileX - theOther->mTileX) > 1 || UMAbs(theUnit->mTileY - theOther->mTileY) > 1)
                continue;
            long theDirection = EEGetDirectionTo(theUnit, theOther);
            EEGetNextTile(theUnit->GetTile(), theTile, EETurnDirection(theDirection, 1, true), 1);
            if (EECanStandOn(theTile, theOther))
                continue;
            EEGetNextTile(theUnit->GetTile(), theTile, EETurnDirection(theDirection, 1, false), 1);
            theResult = !EECanStandOn(theTile, theOther);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x45110d
// Removes the unit the base way; while loading, restarts loading once for every transport still in service (not
// while the world clears or a game loads); returns whether it was removed
bool EECPASGSpaceTransport::RemoveUnit(EEComplexUnit* inUnit)
{
    bool theResult = EECPAttackSubGroup::RemoveUnit(inUnit);
    if (theResult && this->mCargoStage == 1 && gPhysics != NULL) {
        for (unsigned long i = 0; i < this->mUnits.size(); i++) {
            if (!this->mUnits[i]->IsDone() && !gWorldClearing && !gLoadingSavedGame) {
                this->StartLoading();   // once, not once per remaining unit
                break;
            }
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x45117e
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x451189
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4511a6
// Resets every total and count to zero
void EECPASGStats::Clear()
{
    this->mTotalAttack = 0.0f;
    this->mNumAttacking = 0;
    this->mTotalHitPoints = 0;
    this->mTotalRange = 0.0f;
    this->mTotalMaxHitPoints = 0;
    this->mNumCitizens = 0;
    this->mNumPriests = 0;
    this->mTotalSpeed = 0.0f;
    this->mNumProphets = 0;
    this->mUnitCount = 0;
    this->mTotalInfluence = 0;
    this->mL24 = 0;
    this->mL34 = 0;
    this->mL38 = 0;
    this->mNumCalamityUnits = 0;
    this->mL40 = 0;
    this->mL44 = 0;
}

// FUNCTION: EXE 0x4511e2
// Recomputes the statistics from the given sub-group units
void EECPASGStats::Compute(std::vector<EECPAttackSubGroupUnit*>& inUnits)
{
    this->Clear();
    this->mUnitCount = inUnits.size();
    for (long i = inUnits.size() - 1; i >= 0; i--)
        this->Add(inUnits[i]->GetUnit());
}

// FUNCTION: EXE 0x451239
// Adds one unit: attackers, influence, attack, hit points, range and speed totals, calamity casters, priests
// (and emissaries), citizens and prophets
void EECPASGStats::Add(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEUnitGoal* theGoal = inUnit->GetGoal();
    if (theGoal != NULL && (theGoal->mKind == kGoalAttackUnit || theGoal->mKind == kGoalAttackGround || theGoal->mKind == kGoalCastCalamity))
        this->mNumAttacking++;
    this->mTotalInfluence += inUnit->GetUnitClass()->mInfluence;
    this->mTotalAttack += theStats->mAttack;
    this->mTotalHitPoints += inUnit->mHitPoints;
    this->mTotalMaxHitPoints += theStats->mMaxHitPoints;
    this->mTotalRange += theStats->mRange;
    this->mTotalSpeed += theStats->mMaxVelocity;
    if (inUnit->GetUnitClass()->mHasCalamityPowers)
        this->mNumCalamityUnits++;
    switch (inUnit->GetUnitClass()->mUnitType) {
    case kUnitTypePriest:
    case kUnitTypeEmissary:
        this->mNumPriests++;
        break;
    case kUnitTypeCitizen:
    case kUnitTypeFemaleCitizen:
        this->mNumCitizens++;
        break;
    case kUnitTypeProphet:
        this->mNumProphets++;
        break;
    }
}

// FUNCTION: EXE 0x4512e9
// Destroys a map<long, long> (std::_Tree destructor)

// FUNCTION: EXE 0x451314
// Erases a range of a map<long, long>, clearing the whole tree at once for begin..end (std::_Tree erase)

// FUNCTION: EXE 0x451385
// Frees a subtree of a map<long, long> (std::_Tree _Erase)

// ICF-FOLDED: EXE 0x4ac98e
// Does nothing: transports never attack
// (no address of its own: folded into the shared `ret 0xc` body the vtable slot points at)
void EECPASGSpaceTransport::AttackWith(EECPAttackSubGroupUnit*,EEComplexUnit*,EETileCoordinate*)
{
}

// ICF-FOLDED: EXE 0x444b05
// Sets the land group the transports carry
// (no address of its own: the same body as EECPASGAirTransport::SetLand, which ICF kept; [sib 12])
void EECPASGSpaceTransport::SetLand(EECPASGLand* inLand)
{
    EECPASGTransport::SetLand(inLand);
}

// ICF-FOLDED: EXE 0x444188
// Counts the units aboard all the sub-group's transports
// (no address of its own: the same body as EECPASGAirTransport::Virtual04, which ICF kept; [sib 14])
long EECPASGSpaceTransport::CountCargo()
{
    long theResult = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        theResult += this->mUnits[i]->mUnit->GetNumCargoUnits();
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x441a2e
// Sets the sub-group's task and flags every unit as needing new orders
// (no address of its own: the same body as EECPASGAir::Virtual14, which ICF kept; [sib 29])
void EECPASGSpaceTransport::SetTask(const EECPAGTask& inTask)
{
    EECPAttackSubGroup::SetTask(inTask);
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        this->mUnits[i]->mNeedsOrders = true;
    }
}

// ICF-FOLDED: EXE 0x444a58
// Switches to the load state if there are transports and the land group still has units not aboard
// (no address of its own: the same body as EECPASGAirTransport::Virtual14, which ICF kept; [sib 30])
void EECPASGSpaceTransport::StartLoading()
{
    std::vector<EEComplexUnit*> theUnits;
    if (this->mLand == NULL || this->mUnits.size() <= 0)
        return;
    this->mLand->GetUnitsNotAboard(theUnits);
    if (theUnits.size() > 0)
        this->SetState(kASGStateLoad, gPhysics->mGameTime);
}

// ICF-FOLDED: EXE 0x44e801
// Sends the transports back to the rally point, lets go of any land group, then asks for removal
// (no address of its own: the same body as EECPASGSeaTransport::RequestRemoval, which ICF kept; [sib 35])
void EECPASGSpaceTransport::RequestRemoval()
{
    this->ReturnToRallyPoint();
    if (this->mLand != NULL) {
        EECPASGTransport::ReleaseLand();
    }
    EECPAttackSubGroup::RequestRemoval();
}

// ICF-FOLDED: EXE 0x44e9bb
// Runs the transport state machine every 2 s (5 s after a scatter): wait, gather, move, load (scatter if
// blocked), carry until unloaded, release, then remove or idle; task-valid is tested twice when moving
// (no address of its own: the same body as EECPASGSeaTransport::Update, which ICF kept; [sib 37])
void EECPASGSpaceTransport::Update(long inTime)
{
    long theDelay = 2000;
    if (this->mNextScanTime <= inTime && this->mUnits.size() > 0)
        this->ScanTargets(1);
    this->UpdateUnits(inTime);
    bool theFailed = false;
    switch (this->mState) {
    case kASGStateIdle:
        theFailed = !this->TestCondition(kASGConditionHasUnits, inTime);
        if (theFailed)
            this->SetState(kASGStateWaitForUnits, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionShouldReturn, inTime))
                this->SetState(kASGStateRemove, inTime);
            else if (this->TestCondition(kASGConditionTaskValid, inTime))
                this->SetState(kASGStateMove, inTime);
            else
                this->SetState(kASGStateIdle, inTime);
        }
        break;
    case kASGStateWaitForUnits:
        if (this->TestCondition(kASGConditionRequestEmpty, inTime))
            this->SetState(kASGStateRemove, inTime);
        else if (this->TestCondition(kASGConditionRequestFilled, inTime))
            this->SetState(kASGStateGather, inTime);
        break;
    case kASGStateGather:
        if (!this->TestCondition(kASGConditionHasUnits, inTime))
            this->SetState(kASGStateReturnToRallyPoint, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateRemove:
    case kASGStateReturnToRallyPoint:
        break;
    case kASGStateMove:
        theFailed = !this->TestCondition(kASGConditionTaskValid, inTime);
        if (theFailed)
            this->SetState(kASGStateRemove, inTime);
        if (!theFailed && this->TestCondition(kASGConditionTaskValid, inTime))
            this->SetState(kASGStateLoad, inTime);
        break;
    case kASGStateLoad:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed) {
            if (this->TestCondition(kASGConditionAllAboard, inTime))
                this->SetState(kASGStateCarryCargo, inTime);
            else if (this->TestCondition(kASGConditionLoadingBlocked, inTime)) {
                this->SetState(kASGStateScatter, inTime);
                theDelay = 5000;
            }
        }
        break;
    case kASGStateCarryCargo:
    case kASGStateRedirectCargo:
        theFailed = !this->TestCondition(kASGConditionHasCargo, inTime);
        if (theFailed)
            this->SetState(kASGStateIdle, inTime);
        if (!theFailed && this->TestCondition(kASGConditionAllUnloaded, inTime))
            this->SetState(kASGStateReleaseCargo, inTime);
        break;
    case kASGStateReleaseCargo:
        if (this->TestCondition(kASGConditionShouldReturn, inTime))
            this->SetState(kASGStateRemove, inTime);
        else
            this->SetState(kASGStateIdle, inTime);
        break;
    case kASGStateScatter:
        if (!this->TestCondition(kASGConditionHasCargo, inTime))
            this->SetState(kASGStateIdle, inTime);
        else
            this->SetState(kASGStateLoad, inTime);
        break;
    default:
        this->SetState(kASGStateIdle, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + theDelay;
}

// ICF-FOLDED: EXE 0x44e76a
// Lets go of the land group it carries, then shuts the sub-group down
// (no address of its own: the same body as EECPASGSeaTransport::Shutdown, which ICF kept; [sib 47])
void EECPASGSpaceTransport::Shutdown()
{
    EECPASGTransport::ReleaseLand();
    EECPAttackSubGroup::Shutdown();
}
