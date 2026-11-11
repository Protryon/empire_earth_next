// C:\EEX\Empire Earth\Units\EEComplexDoppelganger.cpp (file name inferred: no __FILE__; the class name). A complex
// unit's doppelganger (what another player last saw of it), then EEComplexUnit's Load/FinishLoad/RepairPointers/Save
// (0x1f2452..0x1f5180; the std template COMDATs close the unit at 0x1f5180..0x1f57d3). 0x1f1f19..0x1f57d3 (after
// EEActiveGroup.cpp; EEComplexUnit.cpp follows).
#include <windows.h>
#include <stdio.h>

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
#include "lle/GEModel.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"

#include "ee/EEUnitKind.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEComplexDoppelganger.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEDataPreLoading.h"
#include <string.h>
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitAction.h"
#include "ee/EEPPatrolGroup.h"
#include <math.h>
#include <algorithm>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEHerd.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEPathManager.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGBuildRepair.h"

// port (save version 0.256): each loaded unit's contact (collision) list as saved, by unit id, head first. The load
// makes its own contacts while it places the units, and EEPhysicsModel reads the list before it rebuilds it (a unit
// without contacts skips its collision step), so the loaded units collided differently in their first tick. Put back
// at the first tick in two passes over every unit (clear, then add): ClearContacts also removes the unit from its
// contacts' lists, so a unit-by-unit rebuild would undo its neighbours'.
static std::vector<std::pair<std::pair<EEComplexUnit*, unsigned long>, std::vector<unsigned long> > > sLoadedContacts;

static EEComplexUnit* LoadedComplexUnit(unsigned long inID)
{
    return dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(inID)));
}

static void RestoreLoadedContacts()
{
    std::vector<std::pair<std::pair<EEComplexUnit*, unsigned long>, std::vector<unsigned long> > > theContacts;
    theContacts.swap(sLoadedContacts);
    for (size_t i = 0; i < theContacts.size(); i++) {
        EEComplexUnit* theUnit = theContacts[i].first.first;
        if (static_cast<RTSUnit*>(theUnit) == gWorld.GetUnit(theContacts[i].first.second))
            theUnit->ClearContacts();
    }
    for (size_t i = 0; i < theContacts.size(); i++) {
        EEComplexUnit* theUnit = theContacts[i].first.first;
        if (static_cast<RTSUnit*>(theUnit) != gWorld.GetUnit(theContacts[i].first.second))
            continue;
        const std::vector<unsigned long>& theIDs = theContacts[i].second;
        for (size_t k = theIDs.size(); k-- > 0;) {   // AddContact adds at the head: the last saved first
            EEComplexUnit* theContact = LoadedComplexUnit(theIDs[k]);
            if (theContact != NULL)
                theUnit->AddContact(theContact);
        }
    }
}

// FUNCTION: EXE 0x5f1f19
// Creates an empty complex doppelganger with no model, hit points or tile
EEComplexDoppelganger::EEComplexDoppelganger()
{
    this->Reset();
}

// TEMPLATE: EXE 0x5f1f3b
// (std::exception::what, emitted with the <stdexcept> vtables of the vector _Xlen paths)

// FUNCTION: EXE 0x5f1f3f
// Returns the remembered unit's animation state
long EEComplexDoppelganger::GetAnimation()
{
    return this->mAnimation;
}

// FUNCTION: EXE 0x5f1f43
// Returns itself: this doppelganger is a complex one
EEComplexDoppelganger* EEComplexDoppelganger::GetComplexDoppelganger()
{
    return this;
}

// FUNCTION: EXE 0x5f1f46
// Returns whether the remembered unit was fully built (the top bit of the packed hit points)
bool EEComplexDoppelganger::IsBuilt()
{
    return (this->mPackedHitPoints & 0x80000000) > 0;
}

// FUNCTION: EXE 0x5f1f57
// Returns the remembered animation frame
long EEComplexDoppelganger::GetFrame()
{
    return this->mFrame;
}

// FUNCTION: EXE 0x5f1f5b
// Returns the remembered model
EEModel* EEComplexDoppelganger::GetModel()
{
    return this->mModel;
}

// FUNCTION: EXE 0x5f1f5f
// Returns whether the remembered unit was a building foundation
bool EEComplexDoppelganger::IsFoundation()
{
    return this->mIsFoundation;
}

// FUNCTION: EXE 0x5f1f63
// Copies the remembered position and orientation into the caller's transformation
void EEComplexDoppelganger::GetTransformation(GETransformation& outTransformation)
{
    outTransformation = this->mTransformation;
}

// FUNCTION: EXE 0x5f1f74
// Returns the remembered scale
float EEComplexDoppelganger::GetScale()
{
    return this->mScale;
}

// FUNCTION: EXE 0x5f1f78
// Returns the remembered hit points (a resource's amount) without the built bit
long EEComplexDoppelganger::GetHitPoints()
{
    return this->mPackedHitPoints & 0x7fffffff;
}

// FUNCTION: EXE 0x5f1f81
// Returns the tile X the unit was seen on
long EEComplexDoppelganger::GetTileX()
{
    return this->mTileX;
}

// FUNCTION: EXE 0x5f1f85
// Returns the tile Y the unit was seen on
long EEComplexDoppelganger::GetTileY()
{
    return this->mTileY;
}

// FUNCTION: EXE 0x5f1f89
// Destroys the complex doppelganger, releasing its unit and model media
EEComplexDoppelganger::~EEComplexDoppelganger()
{
    this->Clear();
}

// FUNCTION: EXE 0x5f1fa0
// Forgets the model, hit points, frame, animation, scale and tile (-10000, off the map) without releasing anything
void EEComplexDoppelganger::Reset()
{
    this->mScale = 0.0f;
    this->mModel = NULL;
    this->mPackedHitPoints = 0;
    this->mFrame = 0;
    this->mAnimation = 0;
    this->mTileX = -10000;
    this->mTileY = -10000;
    this->mIsFoundation = false;
}

// FUNCTION: EXE 0x5f1fc2
// Releases the remembered unit and the model's media, then resets
void EEComplexDoppelganger::Clear()
{
    EEDoppelganger::Clear();
    if (this->mModel != NULL)
        this->mModel->UnloadMedia();
    this->Reset();
}

// FUNCTION: EXE 0x5f1fdf
// Remembers what a viewing player sees of a unit: model, frame, animation, hit points (a resource's amount),
// built flag, position and scale (a unit with no complex part stands built at its tile centre), then loads the media
void EEComplexDoppelganger::Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EEDoppelganger::Initialize(inUnit, inPlayer, inTile);
    if (theClass->mCategory == kUnitCategoryResource)
        this->mPackedHitPoints = inUnit->mResourceAmount | 0x80000000;
    else
        this->mPackedHitPoints = inUnit->mHitPoints;
    if (theUnit != NULL) {
        if (theUnit->IsFoundation() == true)
            this->mModel = EEGetPlayerClassGraphic(theUnit->GetPlayer(), theClass);
        else
            this->mModel = inUnit->GetModel();
        this->mFrame = theUnit->mPhysicalModel.GetFrame();
        this->mAnimation = theUnit->mAnimation;
        if (theUnit->mIsBuilt)
            this->mPackedHitPoints |= 0x80000000;
        this->mTransformation = theUnit->mPhysicalModel.GetLocation();
        this->mScale = theUnit->mPhysicalModel.GetScale();
        this->mIsFoundation = theUnit->IsFoundation();
    } else {
        this->mModel = inUnit->GetModel();
        this->mFrame = 0;
        this->mAnimation = 0;
        this->mScale = theClass->GetScale();
        this->mPackedHitPoints |= 0x80000000;
        this->mIsFoundation = false;
        this->mTransformation.MakeIdentity();
        this->mTransformation.MakeIdentity();
        float theHalf = inUnit->mUnitClass->mTileSize * 0.5f;
        GE3DPoint thePosition(inUnit->mTileX + theHalf, inUnit->mTileY + theHalf, 0.0f);
        thePosition.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()));
        this->mTransformation.SetPosition(thePosition);
    }
    this->mTileX = inUnit->mTileX;
    this->mTileY = inUnit->mTileY;
    this->mModel->LoadMedia();
}

// FUNCTION: EXE 0x5f212e
// Copies another complex doppelganger's remembered view for a new viewing player
void EEComplexDoppelganger::Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile)
{
    EEComplexDoppelganger* theOther = dynamic_cast<EEComplexDoppelganger*>(inOther);
    EEDoppelganger::Initialize(inOther, inPlayer, inTile);
    this->mModel = theOther->mModel;
    this->mFrame = theOther->mFrame;
    this->mAnimation = theOther->mAnimation;
    this->mPackedHitPoints = theOther->mPackedHitPoints;
    this->mScale = theOther->mScale;
    this->mTransformation = theOther->mTransformation;
    this->mTileX = theOther->mTileX;
    this->mTileY = theOther->mTileY;
    this->mIsFoundation = theOther->mIsFoundation;
}

// FUNCTION: EXE 0x5f21a3
// Reads the transformation, model index, hit points, frame, animation, scale and tile after the base data (the
// foundation flag only from minor version 0xe6 on); returns the first read error
long EEComplexDoppelganger::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    if (inMajorVersion == 0 && inMinorVersion < 0xe6) {
        theResult = EEDoppelganger::Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != kErrorOK)
            return theResult;
        this->mTransformation.Load(inFile);
        long theModel;
        theResult = inFile.Read(theModel);
        this->mModel = reinterpret_cast<EEModel*>(theModel);
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(this->mPackedHitPoints);
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(this->mFrame);
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(this->mAnimation);
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(this->mScale);
        if (theResult != kErrorOK)
            return theResult;
        theResult = inFile.Read(this->mTileX);
        if (theResult != kErrorOK)
            return theResult;
        return inFile.Read(this->mTileY);
    }
    theResult = EEDoppelganger::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != kErrorOK)
        return theResult;
    this->mTransformation.Load(inFile);
    {
        long theModel;
        theResult = inFile.Read(theModel);
        this->mModel = reinterpret_cast<EEModel*>(theModel);
    }
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mPackedHitPoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mFrame);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mAnimation);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mScale);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTileX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTileY);
    if (theResult != kErrorOK)
        return theResult;
    return inFile.Read(this->mIsFoundation);
}

// FUNCTION: EXE 0x5f230c
// Turns the loaded model index back into its media (clamping the frame) and puts the doppelganger on its tile; frees
// it if the unit or model is gone; returns 0
long EEComplexDoppelganger::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    unsigned long theModel = USavedID(this->mModel);
    this->mModel = NULL;
    if (EEDoppelganger::RepairPointers(inFirst, inSecond) == kErrorOK) {
        EEModel* theMedia = (EEModel*)EEMedia::Get(theModel);
        if (theMedia != NULL && theMedia->LoadMedia() == kErrorOK) {
            this->mModel = theMedia;
            this->mFrame = min(this->mFrame, theMedia->mModel->GetFrameCount() - 1);
            gDoppelgangerManager.AddToTiles(this, this->mTileX, this->mTileY, this->mUnit->mUnitClass->GetUnitType() == kUnitTypeAirport);
            return kErrorOK;
        }
    }
    gDoppelgangerManager.Free(this);
    return kErrorOK;
}

// FUNCTION: EXE 0x5f2392
// Writes the transformation, model index, hit points, frame, animation, scale, tile and foundation flag after the base
// data; returns the first write error
long EEComplexDoppelganger::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEDoppelganger::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    this->mTransformation.Save(outFile);
    long theModel = this->mModel->mMediaIndex;
    theResult = outFile.Write(theModel);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mPackedHitPoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mFrame);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mAnimation);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mScale);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTileX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTileY);
    if (theResult != kErrorOK)
        return theResult;
    return outFile.Write(this->mIsFoundation);
}

static const float kUnsetHeading = -10000.0f;   // 0x446db0 (Load's mInitialContactHeading; provisional name)
// FUNCTION: EXE 0x5f2452
// Reads the complex unit's state from the saved game, keeping units and groups as ids for RepairPointers and queueing
// its pending path; fields newer than the file get defaults; returns the read status
long EEComplexUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long i, j, k, p, q, n, r;
    EEUnitVectorItem theGoals(this);
    EEComplexUnitWaypoint theWaypoint;
    GETransformation theLocation(false);
    U2DPoint<long> theGoalTile;
    unsigned long theValue;
    unsigned long theSecond;
    unsigned long theGoalCount;
    long theCount;
    long theFirst;
    long theState;
    unsigned long theAction;
    long thePartsResult;
    float theFraction;
    float theScale;
    bool theFlag;
    bool theHasPath = false;
    long theResult = EEUnit::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        goto LoadFinalVersions;
    this->mInitialContactHeading = kUnsetHeading;
    this->mBehavior = EEUnitBehavior::sTable[5];
    this->mBuildQueue.clear();
    theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == 0; i++) {
        theResult = inFile.Read(theFirst);
        if (theResult == 0) {
            if (inMinorVersion < 0xf8)
                theSecond = 0;
            else
                theResult = inFile.Read(theSecond);
            if (theResult == 0) {
                EEProductionEntry thePair = {theFirst, theSecond};
                this->mBuildQueue.push_back(thePair);
            }
        }
    }
    this->mCitizenBuildQueue.clear();
    theResult = inFile.Read(theCount);
    for (j = 0; j < theCount && theResult == 0; j++) {
        theResult = inFile.Read(theValue);
        if (theResult == 0) {
            EEUnit* theUnit = reinterpret_cast<EEUnit*>(theValue);
            this->mCitizenBuildQueue.push_back(theUnit);
        }
    }
    this->mCargo.clear();
    theResult = inFile.Read(theCount);
    for (k = 0; k < theCount && theResult == 0; k++) {
        theResult = inFile.Read(theValue);
        if (theResult == 0) {
            EEComplexUnit* theUnit = reinterpret_cast<EEComplexUnit*>(theValue);
            this->mCargo.push_back(theUnit);
        }
    }
    this->mWaypoints.clear();
    theResult = inFile.Read(theCount);
    for (p = 0; p < theCount; p++) {
        if (theResult != 0)
            goto LoadFlags;
        theResult = theWaypoint.mLocation.Load(inFile);
        if (theResult == 0 && (theResult = inFile.Read(theWaypoint.mDistance)) == 0
            && (theResult = inFile.Read(theWaypoint.mStartDistance)) == 0)
            this->mWaypoints.push_back(theWaypoint);
    }
    if (theResult != 0 || (theResult = this->mProjectileStart.Load(inFile)) != 0)
        goto LoadFlags;
    theResult = theLocation.Load(inFile);
    if (theResult != 0)
        goto LoadFlags;
    this->mPhysicalModel.SetLocation(theLocation);
    theResult = inFile.Read(theFlag);
    this->mIsFiringHigh = theFlag;
    if (theResult != 0 || (theResult = inFile.Read(theScale)) != 0)
        goto LoadFlags;
    if (GetIsLoadingScenario() || theScale == 0.0f) {
        if (this->GetUnitClass() == NULL) {
            if (theScale == 0.0f)
                theScale = 1.0f;
        } else
            theScale = this->GetUnitClass()->GetScale();
    }
    this->mPhysicalModel.SetScale(theScale);
    theResult = inFile.Read(theValue);
    this->mPhysicalModel.SetFrame(theValue);
    if (theResult != 0 || (theResult = this->mAngularVelocity.Load(inFile)) != 0 || (theResult = this->mOrientation.Load(inFile)) != 0)
        goto LoadFlags;
    inFile.Read(theState);
    this->mCollisionTile.mX = theState;
    theResult = inFile.Read(theState);
    this->mCollisionTile.mY = theState;
    if (theResult != 0 || (theResult = inFile.Read(theValue)) != 0)
        goto LoadFlags;
    this->mLauncherClass = theValue == 0 || UIsNone32(theValue) ? NULL : gUnitClassTable[theValue];
    theResult = inFile.Read(theValue);
    this->mPatrolGroup = USavedPointer<EEPPatrolGroup>(theValue);
    if (theResult != 0)
        goto LoadFlags;
    theResult = inFile.Read(theValue);
    this->mPreviousPart = reinterpret_cast<EEComplexUnit*>(theValue);
    if (theResult != 0)
        goto LoadFlags;
    theResult = inFile.Read(theValue);
    this->mNextPart = reinterpret_cast<EEComplexUnit*>(theValue);
    if (theResult != 0)
        goto LoadFlags;
    theResult = inFile.Read(theValue);
    this->mHomeBase = reinterpret_cast<EEComplexUnit*>(theValue);
    if (theResult != 0)
        goto LoadFlags;
    theResult = inFile.Read(theValue);
    this->mContainer = reinterpret_cast<EEComplexUnit*>(theValue);
    if (theResult != 0)
        goto LoadFlags;
    theResult = inFile.Read(theAction);
    if (this->GetUnitClass() != NULL && this->GetUnitClass()->FindAction(theAction) != NULL) {
        if (theAction == 0)
            theAction = 5000;
        this->StartAction(theAction);
    }
    if (theResult != 0 || (thePartsResult = inFile.Read(theCount)) != 0)
        goto LoadFlags;
    this->ResizeTurretAngles(theCount);
    this->mNumTurretAngles = theCount;
    for (q = 0; q < theCount; q++) {
        if (thePartsResult != 0)
            goto LoadFlags;
        thePartsResult = inFile.Read(this->mTurretAngles[q]);
    }
    if (thePartsResult != 0)
        goto LoadFlags;
    if ((theResult = inFile.Read(this->mHeight)) != 0
        || (theResult = inFile.Read(this->mVelocity)) != 0
        || (theResult = inFile.Read(this->mF220)) != 0
        || (theResult = inFile.Read(this->mF21C)) != 0
        || (theResult = inFile.Read(this->mCurrentResearch)) != 0
        || (theResult = inFile.Read(this->mAnimation)) != 0
        || (theResult = inFile.Read(this->mCurrentPhysicsAction)) != 0
        || (theResult = inFile.Read(this->mCurrentPhysicsOrder)) != 0
        || (theResult = inFile.Read(this->mFacing)) != 0
        || (theResult = inFile.Read(this->mLaunchYaw)) != 0)
        goto LoadFlags;
    theResult = inFile.Read(theFraction);
    memcpy(&this->mLastHitPoints, &theFraction, sizeof(theFraction));
    if (theResult != 0)
        goto LoadFlags;
    if ((theResult = inFile.Read(this->mExitingUnitID)) != 0
        || (theResult = inFile.Read(this->mCrewHitPoints)) != 0
        || (theResult = inFile.Read(this->mProductionTime)) != 0)
        goto LoadFlags;
    theResult = inFile.Read(theFraction);
    memcpy(&this->mFlightTime, &theFraction, sizeof(theFraction));
    if (theResult != 0)
        goto LoadFlags;
    if ((theResult = inFile.Read(this->mReloadTimer)) != 0
        || (theResult = inFile.Read(this->mCargoRoom)) != 0
        || (theResult = inFile.Read(this->mRottingAwayTime)) != 0
        || (theResult = inFile.Read(this->mTargetSearchTimer)) != 0
        || (theResult = inFile.Read(this->mWorkTimer)) != 0
        || (theResult = inFile.Read(this->mRegenerationTimer)) != 0
        || (theResult = inFile.Read(this->mHealSearchTimer)) != 0
        || (theResult = inFile.Read(this->mCollisionResolutionCount)) != 0
        || (theResult = inFile.Read(this->mWakeTimer)) != 0
        || (theResult = inFile.Read(this->mTrailEffectTimer)) != 0
        || (theResult = inFile.Read(this->mAmmunition)) != 0
        || (theResult = inFile.Read(this->mAnimationTime)) != 0
        || (theResult = inFile.Read(theValue)) != 0
        || (theResult = inFile.Read(this->mWeaponTagPoint)) != 0)
        goto LoadFlags;
    inFile.Read(this->mTimesUpdated);

LoadFlags:
    if ((theResult = inFile.Read(this->mConversionStartTime)) != 0
        || (theResult = inFile.Read(this->mCollisionState)) != 0
        || (theResult = inFile.Read(this->mIsDisabled)) != 0
        || (theResult = inFile.Read(this->mIsDeletable)) != 0
        || (theResult = inFile.Read(this->mIsCommandable)) != 0
        || (theResult = inFile.Read(this->mIsAnimationPaused)) != 0
        || (theResult = inFile.Read(this->mHasArrived)) != 0
        || (theResult = inFile.Read(this->mIsPathDone)) != 0
        || (theResult = inFile.Read(this->mIsClearanceGranted)) != 0)
        goto LoadLegacy;
    theResult = inFile.Read(this->mIsBuilt);
LoadLegacy:
    if (inMinorVersion <= 0x1b) {
        if (theResult == 0)
            theResult = inFile.Read(theFlag);
        if (theResult == 0)
            theResult = inFile.Read(theFlag);
    } else {
        if (theResult == 0)
            theResult = inFile.Read(this->mOpacity);
        if (theResult == 0)
            theResult = inFile.Read(this->mIsCloaked);
    }
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mGatheredResource = reinterpret_cast<EEUnit*>(theValue);
        if (theResult == 0 && (theResult = inFile.Read(this->mNumAreaEffects)) == 0)
            theResult = inFile.Read(this->mU2FC);
    }
    if (this->mLastAttacker != NULL)
        this->mLastAttacker->Dereference();
    this->mLastAttacker = NULL;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mLastAttacker = reinterpret_cast<EEComplexUnit*>(theValue);
    }
    if (this->mOwner != NULL)
        this->mOwner->Dereference();
    this->mOwner = NULL;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mOwner = USavedPointer<EEComplexUnit>(theValue);
        if (theResult == 0) {
            theResult = inFile.Read(theFraction);
            memcpy(&this->mGarrisonCount, &theFraction, sizeof(theFraction));
            if (theResult == 0 && (theResult = inFile.Read(this->mGarrisonPopulation)) == 0
                && (theResult = inFile.Read(this->mIsPaidFor)) == 0)
                theResult = inFile.Read(this->mNumBuilders);
        }
    }
    theValue = (unsigned long)-1;
    this->mMine = (EEUnit*)-1;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mMine = USavedPointer<EEUnit>(theValue);
        if (theResult == 0 && (theResult = this->mU0DC.Load(inFile)) == 0
            && (theResult = inFile.Read(this->mIsDetachedFromAI)) == 0 && (theResult = this->mColor.Load(inFile)) == 0
            && (theResult = inFile.Read(this->mIsTinted)) == 0 && (theResult = inFile.Read(this->mIsLocked)) == 0
            && (theResult = inFile.Read(this->mGateDirection)) == 0 && (theResult = inFile.Read(this->mOpenFraction)) == 0) {
            theResult = inFile.Read(theGoalCount);
            for (unsigned long goal = 0; goal < theGoalCount; goal++)
                if (theResult == 0)
                    theResult = (goal < this->mGoalQueues.size() ? this->mGoalQueues[goal] : theGoals).Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == 0 && (theResult = inFile.Read(this->mConversionEffect.mValue)) == 0)
                theResult = inFile.Read(this->mHealEffect.mValue);
        }
    }
    theValue = 0;
    if (theResult == 0) {
        theResult = inFile.Read(theValue);
        this->mHerd = reinterpret_cast<EEHerd*>(theValue);
    }
    theValue = (unsigned long)-1;
    if (theResult == 0) {
        inFile.Read(theValue);
        this->mHolder = USavedPointer<EEComplexUnit>(theValue);
    }
    theCount = 0;
    theResult = inFile.Read(theCount);
    this->mHeldUnits.resize(theCount, NULL);
    for (n = 0; n < theCount; n++) {
        if (theResult != 0)
            goto LoadLateVersions;
        theResult = inFile.Read(theValue);
        this->mHeldUnits[n] = reinterpret_cast<EEComplexUnit*>(theValue);
    }
    if ((theResult = inFile.Read(this->mGrowth)) != 0
        || (theResult = inFile.Read(this->mB34F)) != 0
        || (theResult = inFile.Read(this->mIsRegrowing)) != 0
        || (theResult = inFile.Read(this->mNumAttackers)) != 0
        || (theResult = inFile.Read(this->mGrowTime)) != 0
        || (theResult = inFile.Read(this->mGrazeTime)) != 0
        || (theResult = inFile.Read(this->mFireEffect.mValue)) != 0)
        goto LoadLateVersions;
    theResult = inFile.Read(theHasPath);
    if (theResult == 0)
        theResult = theGoalTile.Load(inFile);
    if (!theHasPath) {
        EEPathManagerPending thePending;
        thePending.mUnitID = this->mUnitID;
        thePending.mTile.mX = theGoalTile.mX;
        thePending.mTile.mY = theGoalTile.mY;
        gPathManager.mPending.push_back(thePending);
    }
    if ((theResult = inFile.Read(this->mPositiveMorale)) != 0
        || (theResult = inFile.Read(this->mNegativeMorale)) != 0)
        goto LoadLateVersions;
    theResult = inFile.Read(theFraction);
    memcpy(&this->mPower, &theFraction, sizeof(theFraction));
    if (theResult != 0)
        goto LoadLateVersions;
    if ((theResult = inFile.Read(this->mPowerRegenTimer)) != 0
        || (theResult = inFile.Read(this->mIsCloakable)) != 0
        || (theResult = inFile.Read(this->mInitialContactLocation.mX)) != 0
        || (theResult = inFile.Read(this->mInitialContactLocation.mY)) != 0
        || (theResult = inFile.Read(this->mInitialContactLocation.mZ)) != 0
        || (theResult = inFile.Read(this->mInitialContactHeading)) != 0)
        goto LoadLateVersions;
    theResult = inFile.Read(theValue);
    this->mBehavior = reinterpret_cast<EEUnitBehavior*>(theValue);
    if (theResult == 0 && (theResult = inFile.Read(this->mExperience)) == 0) {
        theResult = inFile.Read(theValue);
        this->mLastResource = reinterpret_cast<EEUnit*>(theValue);
    }
LoadLateVersions:
    if (inMinorVersion >= 0xc9 && theResult == 0 && (theResult = inFile.Read(this->mIsFoundation)) == 0
        && (inMajorVersion != 0 || inMinorVersion >= 0xcd)) {
        theResult = inFile.Read(theCount);
        for (r = 0; r < theCount && theResult == 0; r++) {
            theResult = inFile.Read(theState);
            if (theResult == 0)
                this->mUnitVariables[r] = theState;
        }
    }
    if (inMinorVersion >= 0xd3 && theResult == 0 && (theResult = inFile.Read(this->mFormationRank)) == 0)
        theResult = inFile.Read(this->mWaterState);
    if (inMinorVersion >= 0xd5 && theResult == 0)
        theResult = this->mLastMapPosition.Load(inFile);
    if (inMinorVersion >= 0xe2 && theResult == 0)
        theResult = inFile.Read(this->mCloaked);
    if (inMinorVersion >= 0xe8 && theResult == 0)
        theResult = inFile.Read(this->mCamouflageTimer);
LoadFinalVersions:
    if (inMajorVersion == 0 && inMinorVersion <= 0xea) {
        this->mIsSpecialAttack = false;
        this->mDisabledReason = 0;
    } else if (theResult == 0 && (theResult = inFile.Read(this->mIsSpecialAttack)) == 0)
        theResult = inFile.Read(this->mDisabledReason);
    if (inMinorVersion < 0xf3)
        this->mPlanetID = 0;
    else if (theResult == 0)
        theResult = inFile.Read(this->mPlanetID);
    if (inMajorVersion == 0 && inMinorVersion < 0xf4)
        this->mSight = 0.0f;
    else if (theResult == 0)
        theResult = inFile.Read(this->mSight);
    if ((inMajorVersion != 0 || inMinorVersion >= 0xf5) && theResult == 0)
        theResult = inFile.Read(this->mU32C);
    // port (0.254): the running state the original did not save (Save); put back at the first tick, after the load's
    // own rebuilding has set some of it again (EEAfterLoad)
    if (theResult == 0 && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        struct LiveState {
            float mAnimationFraction, mCollisionRadius, mPushVelocityX, mPushVelocityY, mF24C;
            long mNextFrame, mAIState;
            unsigned long mNextTriggerTime;
            std::deque<EEComplexUnitAttacker> mIgnoredTargets;
            std::vector<U2DPoint<long> > mBoundedPathTiles;
            // port (0.256): the model's frame (RepairPointers restarts an animation at its last frame), the counters
            // the original saved as fractions (FinishLoad) or set at load (the rotting time, a builder's research)
            bool mRunning = false;
            unsigned long mFrame = 0;
            long mFlightTime = 0, mRottingAwayTime = 0, mCurrentResearch = 0, mLastHitPoints = 0, mGarrisonCount = 0, mPower = 0;
        } theLive;
        unsigned long theLiveCount = 0;
        if ((theResult = inFile.Read(theLive.mAnimationFraction)) == 0 && (theResult = inFile.Read(theLive.mNextFrame)) == 0
            && (theResult = inFile.Read(theLive.mCollisionRadius)) == 0 && (theResult = inFile.Read(theLive.mPushVelocityX)) == 0
            && (theResult = inFile.Read(theLive.mPushVelocityY)) == 0 && (theResult = inFile.Read(theLive.mF24C)) == 0
            && (theResult = inFile.Read(theLive.mAIState)) == 0 && (theResult = inFile.Read(theLive.mNextTriggerTime)) == 0)
            theResult = inFile.Read(theLiveCount);
        for (unsigned long k = 0; k < theLiveCount && theResult == 0; k++) {
            EEComplexUnitAttacker theAttacker;
            if ((theResult = inFile.Read(theAttacker.mTileX)) == 0 && (theResult = inFile.Read(theAttacker.mTileY)) == 0
                && (theResult = inFile.Read(theAttacker.mFromX)) == 0 && (theResult = inFile.Read(theAttacker.mFromY)) == 0
                && (theResult = inFile.Read(theAttacker.mUnitID)) == 0 && (theResult = inFile.Read(theAttacker.mTime)) == 0)
                theLive.mIgnoredTargets.push_back(theAttacker);
        }
        theLiveCount = 0;
        if (theResult == 0)
            theResult = inFile.Read(theLiveCount);
        for (unsigned long k = 0; k < theLiveCount && theResult == 0; k++) {
            U2DPoint<long> theTile;
            if ((theResult = inFile.Read(theTile.mX)) == 0 && (theResult = inFile.Read(theTile.mY)) == 0)
                theLive.mBoundedPathTiles.push_back(theTile);
        }
        unsigned long theUnitID = this->mUnitID;
        EEComplexUnit* theSelf = this;   // only while it is the world's unit of that id (not a doppelganger, not freed)
        if (theResult == 0 && EESaveHasRunningState(inMajorVersion, inMinorVersion)) {   // port (0.256), see LiveState
            theLive.mRunning = true;
            if ((theResult = inFile.Read(theLive.mFrame)) == 0 && (theResult = inFile.Read(theLive.mFlightTime)) == 0
                && (theResult = inFile.Read(theLive.mRottingAwayTime)) == 0 && (theResult = inFile.Read(theLive.mCurrentResearch)) == 0
                && (theResult = inFile.Read(theLive.mLastHitPoints)) == 0 && (theResult = inFile.Read(theLive.mGarrisonCount)) == 0
                && (theResult = inFile.Read(theLive.mPower)) == 0)
                theResult = inFile.Read(theLiveCount);
            std::vector<unsigned long> theContacts;
            for (unsigned long k = 0; k < theLiveCount && theResult == 0; k++) {
                unsigned long theID = 0;
                if ((theResult = inFile.Read(theID)) == 0)
                    theContacts.push_back(theID);
            }
            if (theResult == 0) {
                if (sLoadedContacts.empty())
                    EEAfterLoad(RestoreLoadedContacts);   // once per load (the first unit read registers it; it runs at the first tick, every unit read)
                sLoadedContacts.push_back(std::make_pair(std::make_pair(theSelf, theUnitID), theContacts));
            }
        }
        if (theResult == 0)
            EEAfterLoad([theUnitID, theSelf, theLive]() {
                EEComplexUnit* theUnit = theSelf;
                if (static_cast<RTSUnit*>(theUnit) != gWorld.GetUnit(theUnitID))
                    return;
                theUnit->mAnimationFraction = theLive.mAnimationFraction;
                theUnit->mNextFrame = theLive.mNextFrame;
                theUnit->mCollisionRadius = theLive.mCollisionRadius;
                theUnit->mPushVelocityX = theLive.mPushVelocityX;
                theUnit->mPushVelocityY = theLive.mPushVelocityY;
                theUnit->mF24C = theLive.mF24C;
                theUnit->mAIState = theLive.mAIState;
                theUnit->mNextTriggerTime = theLive.mNextTriggerTime;
                theUnit->mIgnoredTargets = theLive.mIgnoredTargets;
                theUnit->mBoundedPathTiles = theLive.mBoundedPathTiles;
                if (theLive.mRunning) {
                    theUnit->mPhysicalModel.SetFrame(theLive.mFrame);
                    theUnit->mFlightTime = theLive.mFlightTime;
                    theUnit->mRottingAwayTime = theLive.mRottingAwayTime;
                    theUnit->mCurrentResearch = theLive.mCurrentResearch;
                    theUnit->mLastHitPoints = theLive.mLastHitPoints;
                    theUnit->mGarrisonCount = theLive.mGarrisonCount;
                    theUnit->mPower = theLive.mPower;
                }
            });
    }
    return theResult;
}

// Load stores the saved fractions as floats in the longs.
// FUNCTION: EXE 0x5f3395
// Turns the saved hit point, flight, garrison and power fractions back into amounts (at least 1 if nonzero); marks an
// old save's (<= 200) unbuilt land building at 1 hit point as a foundation, reloading its media
long EEComplexUnit::FinishLoad(long inMajorVersion, long inMinorVersion)
{
    float theFraction;
    memcpy(&theFraction, &this->mLastHitPoints, 4);
    this->mLastHitPoints = (long)(this->mStats->mMaxHitPoints * theFraction + 0.00001f);
    if (theFraction != 0.0f && this->mLastHitPoints == 0)
        this->mLastHitPoints = 1;
    memcpy(&theFraction, &this->mFlightTime, 4);
    this->mFlightTime = (long)(theFraction * this->mStats->mFlightTime + 0.00001f);
    if (theFraction != 0.0f && this->mFlightTime == 0)
        this->mFlightTime = 1;
    memcpy(&theFraction, &this->mGarrisonCount, 4);
    this->mGarrisonCount = (long)(theFraction * this->GetUnitClass()->mGarrisonCapacity + 0.00001f);
    if (theFraction != 0.0f && this->mGarrisonCount == 0)
        this->mGarrisonCount = 1;
    memcpy(&theFraction, &this->mPower, 4);
    this->mPower = (long)(theFraction * this->mStats->mPower + 0.00001f);
    if (theFraction != 0.0f && this->mPower == 0)
        this->mPower = 1;
    if (inMajorVersion == 0 && (unsigned long)inMinorVersion <= 200) {
        this->mIsFoundation = this->GetUnitClass()->IsLandBuilding() && this->GetUnitClass()->GetUnitType() != kUnitTypeFarm && !this->mIsBuilt && this->GetHitPoints() == 1;
        if (this->mIsFoundation == true) {
            this->GetPlayer()->UnloadUnitClassMedia(this->GetUnitClass()->GetUnitType());
            gDataPreLoading->PostLoadUnitClass(this->GetPlayer()->mPlayerID, this->GetUnitClass()->GetUnitType());
        }
    }
    return EEUnit::FinishLoad(inMajorVersion, inMinorVersion);
}

// FUNCTION: EXE 0x5f353f
// Turns the loaded ids back into units, groups, research and behaviour, dropping the missing ones; strands a unit whose
// transport or owner vanished and clears goal queues that fail to repair; returns the last status
long EEComplexUnit::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<long> theUnused;
    std::vector<EEComplexUnit*> theCargo;
    std::vector<EEProductionEntry> theResearch;
    bool theFailed = false;
    bool theLostContainer = false;
    bool theAutoWaypoint = false;
    long theResult = EEUnit::RepairPointers(inMajorVersion, inMinorVersion);
    if (theResult != 0)
        return theResult;
    if (this->mCommand == NULL && this->GetUnitClass() != NULL)
        this->StartAction(kActionIdle);
    if (this->GetUnitClass() == NULL) {
        this->mPatrolGroup = NULL;
        this->mCurrentResearch = kTechNone;
        this->mBuildQueue.clear();
        this->mCitizenBuildQueue.clear();
        this->mCargo.clear();
        this->mPreviousPart = NULL;
        this->mNextPart = NULL;
        this->mHomeBase = NULL;
        this->mContainer = NULL;
        this->mGatheredResource = NULL;
        this->mLastResource = NULL;
        this->mLastAttacker = NULL;
        this->mOwner = NULL;
        this->mMine = NULL;
        this->mHolder = NULL;
        this->mBehavior = NULL;
        this->mHerd = NULL;
        for (long empty = this->mHeldUnits.size() - 1; empty >= 0; empty--)
            this->mHeldUnits[empty] = NULL;
        for (unsigned long item = 0; item < this->mGoalQueues.size(); item++) {
            theFailed = false;
            for (unsigned long goal = 0; goal < this->mGoalQueues[item].mGoals.size(); goal++) {
                EEUnitGoal* theGoal = this->mGoalQueues[item].mGoals[goal];
                theResult = theGoal->RepairPointers(inMajorVersion, inMinorVersion);
                if (theResult == 0 && theGoal->mKind == kGoalAttackUnit && inMajorVersion == 0 && inMinorVersion < 0xe0) {
                    EEUGAttackUnit* theAttack = static_cast<EEUGAttackUnit*>(theGoal);
                    if (theAttack->mAttacker != this)
                        theAttack->mAttacker = this;
                }
                if (!theFailed)
                    theFailed = theResult != 0;
            }
            if (theFailed)
                this->mGoalQueues[item].Clear();
        }
        return theResult;
    }
    long theCount = this->mCitizenBuildQueue.size();
    for (long unit = 0; unit < theCount; unit++) {
        this->mCitizenBuildQueue[unit] = static_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mCitizenBuildQueue[unit])));
        this->mCitizenBuildQueue[unit]->Reference();
    }
    theCount = this->mCargo.size();
    for (long cargo = 0; cargo < theCount; cargo++) {
        if (this->GetPlayer()->HasLoadedUnitID(USavedID(this->mCargo[cargo])))
            continue;
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mCargo[cargo])));
        theUnit->Reference();
        theCargo.push_back(theUnit);
    }
    this->mCargo.clear();
    this->mCargo = theCargo;
    for (unsigned long item = 0; item < this->mGoalQueues.size(); item++) {
        theFailed = false;
        for (unsigned long goal = 0; goal < this->mGoalQueues[item].mGoals.size(); goal++) {
            EEUnitGoal* theGoal = this->mGoalQueues[item].mGoals[goal];
            theResult = theGoal->RepairPointers(inMajorVersion, inMinorVersion);
            if (theResult == 0) {
                if (theGoal->mKind == kGoalAttackUnit && inMajorVersion == 0 && inMinorVersion < 0xe0) {
                    EEUGAttackUnit* theAttack = static_cast<EEUGAttackUnit*>(theGoal);
                    if (theAttack->mAttacker != this)
                        theAttack->mAttacker = this;
                }
            } else if (theGoal->mKind == kGoalPatrol)
                this->GetPlayer()->AddUnitToRepatrol(this);
            if (!theFailed)
                theFailed = theResult != 0;
            if (theGoal->mKind == kGoalLocation) {
                EEUGLocation* theLocation = EEGoalCast<EEUGLocation>(theGoal);
                theAutoWaypoint = theLocation->mIsAutoWaypoint == true;
            } else
                theAutoWaypoint = false;
            if (theResult == 0 && item == 0 && theGoal->mKind == kGoalBuildRepair)
                this->mCurrentResearch = static_cast<EEUGBuildRepair*>(theGoal)->mTechTreeID;
        }
        if (!theFailed)
            continue;
        this->mGoalQueues[item].Clear();
        if (item != 0 || this->GetUnitClass() == NULL || this->GetCommandType() == kActionDying)
            continue;
        this->StartAction(kActionIdle);
        if (this->mCurrentPhysicsAction != 3)
            this->mCurrentPhysicsAction = 3;
    }
    if (this->mPatrolGroup == (EEPPatrolGroup*)-1)
        this->mPatrolGroup = NULL;
    else {
        this->mPatrolGroup = this->GetPlayer()->GetPatrolGroup(USavedID(this->mPatrolGroup));
        if (this->mPatrolGroup != NULL)
            this->mPatrolGroup->Reference();
        else
            theResult = 3;
    }
    this->mPreviousPart = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mPreviousPart)));
    if (this->mPreviousPart != NULL)
        this->mPreviousPart->Reference();
    this->mNextPart = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mNextPart)));
    if (this->mNextPart != NULL)
        this->mNextPart->Reference();
    this->mHomeBase = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mHomeBase)));
    if (this->mHomeBase != NULL)
        this->mHomeBase->Reference();
    this->mContainer = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mContainer)));
    if (this->mContainer != NULL) {
        // A not-yet-repaired cargo vector still holds IDs; a repaired one holds actual unit pointers.
        if (this->mContainer->mUnitClass->mTransportCapacity < 1
            || (!this->mContainer->HasCargo(this) && !this->mContainer->HasCargo(reinterpret_cast<EEComplexUnit*>(this->mUnitID)))) {
            this->mContainer = NULL;
            theLostContainer = true;
        } else
            this->mContainer->Reference();
    }
    if (this->mCurrentResearch == kTechNone)
        this->mProductionTime = 0;
    else if (this->mCurrentResearch < 200000 && this->mPlayer->mTechnologies->Get(this->mCurrentResearch) == NULL) {
        this->mCurrentResearch = kTechNone;
        this->mProductionTime = 0;
    }
    if (this->GetUnitClass()->IsLandBuilding() && this->mCurrentResearch != kTechNone && this->mCurrentResearch < 200000) {
        EETechTreeNode* theNode = this->mPlayer->mTechnologies->Get(this->mCurrentResearch);
        if (theNode != NULL && (theNode->mData->mUnitClass < 0 || gUnitClassTable[theNode->mData->mUnitClass] == NULL))
            theNode->mIsAllowed = false;
    }
    for (unsigned long research = 0; research < this->mBuildQueue.size(); research++) {
        if (!(this->mBuildQueue[research].mTechnology < 200000))
            continue;
        EETechTreeNode* theNode = this->mPlayer->mTechnologies->Get(this->mBuildQueue[research].mTechnology);
        if (theNode == NULL)
            continue;
        if (theNode->mData->mUnitClass < 0 || gUnitClassTable[theNode->mData->mUnitClass] == NULL)
            theNode->mIsAllowed = false;
        theResearch.push_back(this->mBuildQueue[research]);
    }
    this->mBuildQueue.clear();
    this->mBuildQueue = theResearch;
    EEUnit* theLink = dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mGatheredResource)));
    this->mGatheredResource = NULL;
    if (theLink != NULL && (theLink->GetUnitID() != 0 || theLink->mResourceType != kResourceNone))
        this->SetGatheredResource(theLink);
    {
        unsigned long theAttackerID = USavedID(this->mLastAttacker);
        this->mLastAttacker = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theAttackerID));
        if (this->mLastAttacker == NULL && EESaveHasGhosts(inMajorVersion, inMinorVersion))
            this->mLastAttacker = EEFindLoadedGhost(theAttackerID);   // port (0.262): an attacker that died, as the saved game kept it
    }
    if (this->mLastAttacker != NULL)
        this->mLastAttacker->Reference();
    if (this->mOwner == (EEComplexUnit*)-1 || this->mOwner == NULL)
        this->mOwner = NULL;
    else {
        this->mOwner = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mOwner)));
        if (this->mOwner != NULL)
            this->mOwner->Reference();
        else
            theLostContainer = true;
    }
    if (this->mMine == (EEUnit*)-1 || this->GetUnitClass() == NULL)
        this->mMine = NULL;
    else {
        this->mMine = dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mMine)));
        if (this->mMine != NULL)
            this->mMine->Reference();
    }
    unsigned long theParts = this->GetModel()->GetNumParts();
    if (theParts != this->mNumTurretAngles) {
        float* theValues;
        if (theParts == 0)
            theValues = NULL;
        else {
            theValues = new float[theParts];
            for (unsigned long part = 0; part < theParts; part++)
                theValues[part] = part < (unsigned long)this->mNumTurretAngles ? this->mTurretAngles[part] : 0.0f;
        }
        delete[] this->mTurretAngles;
        this->mTurretAngles = theValues;
    }
    if (this->GetStats()->mModel->mLastFrames[this->mAnimation] <= this->mPhysicalModel.GetFrame())
        this->SetAnimationFrame(this->mStats->mModel->mStartFrames[this->mAnimation]);
    if (this->mCurrentResearch != kTechNone)
        gDataPreLoading->PostLoadTechnology(this->GetPlayer()->mPlayerID, this->mCurrentResearch);
    EETechTreeNode* theTechnology = this->GetTechTreeNode();
    if (theTechnology != NULL && theTechnology->mData->mKind == 4 && this->GetHitPoints() > 0 && this->mIsBuilt == true)
        this->GetPlayer()->AddWonder(this, !(inMajorVersion == 0 && inMinorVersion < 0xe2));
    this->mHerd = NULL;
    for (long linked = this->mHeldUnits.size() - 1; linked >= 0; linked--) {
        this->mHeldUnits[linked] = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mHeldUnits[linked])));
        if (this->mHeldUnits[linked] != NULL)
            this->mHeldUnits[linked]->Reference();
        else
            this->mHeldUnits.erase(this->mHeldUnits.begin() + linked);
    }
    if (this->mHolder == (EEComplexUnit*)-1)
        this->mHolder = NULL;
    else {
        this->mHolder = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(USavedID(this->mHolder)));
        if (this->mHolder != NULL)
            this->mHolder->Reference();
    }
    if (theLostContainer == true)
        this->GetPlayer()->AddStrandedUnit(this);
    unsigned long theBehavior = USavedID(this->mBehavior);
    if (theBehavior < 8) {
        if (theBehavior == 3 || theBehavior == 4)
            this->mBehavior = NULL;
        else
            this->mBehavior = EEUnitBehavior::Get(theBehavior);
    } else {
        unsigned long behavior;
        for (behavior = 0; behavior < 8; behavior++)
            if (this->mBehavior == EEUnitBehavior::Get(behavior))
                break;
        if (behavior == 8)
            this->mBehavior = NULL;
    }
    if (this->mBehavior == NULL) {
        if (this->GetUnitClass() != NULL)
            this->mBehavior = this->GetUnitClass()->mBehavior;
        if (this->mBehavior == NULL)
            this->mBehavior = EEUnitBehavior::sTable[5];
    }
    EETagPointList* theTags = this->mStats->mModel->mTagPoints[this->mAnimation];
    if (this->GetPlayer() != NULL && theTags != NULL && theTags->GetNumTagPoints() <= this->mWeaponTagPoint)
        this->mWeaponTagPoint = 0;
    theLink = dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mLastResource)));
    this->mLastResource = NULL;
    if (theLink != NULL && (theLink->GetUnitID() != 0 || theLink->mResourceType != kResourceNone))
        this->SetLastResource(theLink);
    if (this->GetHitPoints() < 1) {
        this->mAreaEffects.Clear();
        this->mAreaEffects.mItems.clear();
    } else {
        // port (0.256): the saved count stays (MakeAreaEffects counts the templates: a unit made by EEMakeUnitCopy,
        // whose CopyFrom zeroes it, had 0 in the game that was saved and its templates' count after a load)
        unsigned long theCount = this->mNumAreaEffects;
        this->MakeAreaEffects();
        if (EESaveHasRunningState(inMajorVersion, inMinorVersion))
            this->mNumAreaEffects = theCount;
    }
    this->RefreshGoals();
    this->RefreshActiveGoal();
    if (theAutoWaypoint) {
        for (long autoGoal = this->mGoalQueues[0].mGoals.size() - 1; autoGoal >= 0; autoGoal--) {
            EEUnitGoal* theGoal = this->mGoalQueues[0].mGoals.back();
            if (theGoal == NULL || EEGoalCast<EEUGLocation>(theGoal) == NULL
                || !EEGoalCast<EEUGLocation>(theGoal)->mIsAutoWaypoint)
                break;
            this->mGoalQueues[0].PopBack();
        }
    }
    if (this->GetUnitClass()->mRottingTime < this->mRottingAwayTime)
        this->mRottingAwayTime = this->GetUnitClass()->mRottingTime;
    if (inMajorVersion == 0 && inMinorVersion <= 0xca)
        this->mIsDeletable = this->GetUnitClass()->mIsDeletable;
    if ((this->GetUnitClass()->mCategory == kUnitCategoryLandCitizen || this->GetUnitClass()->mCategory == kUnitCategoryNavalCitizen) && this->mCommand->mType == kActionResourcesRotting && this->mResourceAmount != 0)
        this->mResourceAmount = 0;
    return theResult;
}

// RepairPointers's helper, out of line in this unit.
// FUNCTION: EXE 0x5f414d
// Shows the model of the unit's current animation state at the given frame
void EEComplexUnit::SetAnimationFrame(unsigned long inFrame)
{
    unsigned long theState = this->mAnimation;
    this->mPhysicalModel.SetModel(this->GetModel()->mModels[theState]);
    this->mPhysicalModel.SetFrame(inFrame);
}

// FUNCTION: EXE 0x5f417e
// Writes the complex unit's state to the saved game, units and groups as ids and hit points, flight time, garrison
// and power as fractions of their maximum; returns the write status
long EEComplexUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::vector<long> theUnused;
    EEComplexUnitWaypoint theWaypoint;
    U2DPoint<long> theGoalTile;
    unsigned long theID;
    unsigned long theCount;
    unsigned long theBehavior;
    unsigned long theZero = 0;
    long q;
    long theLength;
    long theState;
    float theFraction;
    float theScale;
    bool theHasPath;
    long theResult = EEUnit::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;

    theLength = this->mBuildQueue.size();
    theResult = outFile.Write(theLength);
    for (long i = 0; i < theLength && theResult == 0; i++) {
        long theFirst = this->mBuildQueue[i].mTechnology;
        theResult = outFile.Write(theFirst);
        if (theResult == 0) {
            unsigned long theSecond = this->mBuildQueue[i].mFlag;
            theResult = outFile.Write(theSecond);
        }
    }
    theLength = 0;
    long theQueued = this->mCitizenBuildQueue.size();
    long j; for (j = 0; j < theQueued; j++)
        if (this->mCitizenBuildQueue[j]->mIsValid)
            theLength++;
    theResult = outFile.Write(theLength);
    for (j = 0; j < theQueued && theResult == 0; j++) {
        if (this->mCitizenBuildQueue[j]->mIsValid) {
            theID = this->mCitizenBuildQueue[j]->mUnitID;
            theResult = outFile.Write(theID);
        }
    }
    theLength = this->mCargo.size();
    theResult = outFile.Write(theLength);
    for (long k = 0; k < theLength && theResult == 0; k++) {
        theID = this->mCargo[k]->mUnitID;
        theResult = outFile.Write(theID);
    }
    theLength = this->mWaypoints.size();
    theResult = outFile.Write(theLength);
    for (long p = 0; p < theLength; p++) {
        if (theResult != 0)
            goto SaveGoals;
        theWaypoint = this->mWaypoints[p];
        theResult = theWaypoint.mLocation.Save(outFile);
        if (theResult == 0) {
            theResult = outFile.Write(theWaypoint.mDistance);
            if (theResult == 0)
                theResult = outFile.Write(theWaypoint.mStartDistance);
        }
    }
    if (theResult != 0 || (theResult = this->mProjectileStart.Save(outFile)) != 0
        || (theResult = this->mPhysicalModel.GetLocation().Save(outFile)) != 0
        || (theResult = outFile.Write(this->mIsFiringHigh)) != 0)
        goto SaveGoals;
    theScale = this->mPhysicalModel.GetScale();
    if ((theResult = outFile.Write(theScale)) != 0)
        goto SaveGoals;
    theID = this->mPhysicalModel.GetFrame();
    if ((theResult = outFile.Write(theID)) != 0 || (theResult = this->mAngularVelocity.Save(outFile)) != 0
        || (theResult = this->mOrientation.Save(outFile)) != 0)
        goto SaveGoals;
    theState = this->mCollisionTile.mX;
    outFile.Write(theState);
    theState = this->mCollisionTile.mY;
    if ((theResult = outFile.Write(theState)) != 0)
        goto SaveGoals;
    theID = this->mLauncherClass == NULL ? 0 : this->mLauncherClass->GetUnitType();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mPatrolGroup == NULL ? (unsigned long)-1 : this->mPatrolGroup->mID;
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mPreviousPart == NULL ? (unsigned long)-1 : this->mPreviousPart->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mNextPart == NULL ? (unsigned long)-1 : this->mNextPart->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mHomeBase == NULL ? (unsigned long)-1 : this->mHomeBase->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mContainer == NULL || this->mContainer->mUnitClass->mTransportCapacity == 0
        ? (unsigned long)-1 : this->mContainer->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mCommand == NULL ? 0 : this->mCommand->mType;
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theLength = this->GetModel()->GetNumParts();
    theResult = outFile.Write(theLength);
    for (q = 0; q < theLength; q++) {
        if (theResult != 0)
            goto SaveGoals;
        theResult = outFile.Write(this->mTurretAngles[q]);
    }
    if (theResult != 0 || (theResult = outFile.Write(this->mHeight)) != 0
        || (theResult = outFile.Write(this->mVelocity)) != 0 || (theResult = outFile.Write(this->mF220)) != 0
        || (theResult = outFile.Write(this->mF21C)) != 0 || (theResult = outFile.Write(this->mCurrentResearch)) != 0
        || (theResult = outFile.Write(this->mAnimation)) != 0 || (theResult = outFile.Write(this->mCurrentPhysicsAction)) != 0
        || (theResult = outFile.Write(this->mCurrentPhysicsOrder)) != 0 || (theResult = outFile.Write(this->mFacing)) != 0
        || (theResult = outFile.Write(this->mLaunchYaw)) != 0)
        goto SaveGoals;
    theFraction = this->mLastHitPoints < 1 || this->mStats->mMaxHitPoints < 1 ? 0.0f : (float)this->mLastHitPoints / this->mStats->mMaxHitPoints;
    if ((theResult = outFile.Write(theFraction)) != 0 || (theResult = outFile.Write(this->mExitingUnitID)) != 0
        || (theResult = outFile.Write(this->mCrewHitPoints)) != 0 || (theResult = outFile.Write(this->mProductionTime)) != 0)
        goto SaveGoals;
    theFraction = this->mFlightTime < 1 || this->mStats->mFlightTime < 1 ? 0.0f : (float)this->mFlightTime / this->mStats->mFlightTime;
    if ((theResult = outFile.Write(theFraction)) != 0 || (theResult = outFile.Write(this->mReloadTimer)) != 0
        || (theResult = outFile.Write(this->mCargoRoom)) != 0 || (theResult = outFile.Write(this->mRottingAwayTime)) != 0
        || (theResult = outFile.Write(this->mTargetSearchTimer)) != 0 || (theResult = outFile.Write(this->mWorkTimer)) != 0
        || (theResult = outFile.Write(this->mRegenerationTimer)) != 0 || (theResult = outFile.Write(this->mHealSearchTimer)) != 0
        || (theResult = outFile.Write(this->mCollisionResolutionCount)) != 0 || (theResult = outFile.Write(this->mWakeTimer)) != 0
        || (theResult = outFile.Write(this->mTrailEffectTimer)) != 0 || (theResult = outFile.Write(this->mAmmunition)) != 0
        || (theResult = outFile.Write(this->mAnimationTime)) != 0 || (theResult = outFile.Write(theZero)) != 0
        || (theResult = outFile.Write(this->mWeaponTagPoint)) != 0 || (theResult = outFile.Write(this->mTimesUpdated)) != 0
        || (theResult = outFile.Write(this->mConversionStartTime)) != 0 || (theResult = outFile.Write(this->mCollisionState)) != 0
        || (theResult = outFile.Write(this->mIsDisabled)) != 0 || (theResult = outFile.Write(this->mIsDeletable)) != 0
        || (theResult = outFile.Write(this->mIsCommandable)) != 0 || (theResult = outFile.Write(this->mIsAnimationPaused)) != 0
        || (theResult = outFile.Write(this->mHasArrived)) != 0 || (theResult = outFile.Write(this->mIsPathDone)) != 0
        || (theResult = outFile.Write(this->mIsClearanceGranted)) != 0 || (theResult = outFile.Write(this->mIsBuilt)) != 0
        || (theResult = outFile.Write(this->mOpacity)) != 0 || (theResult = outFile.Write(this->mIsCloaked)) != 0)
        goto SaveGoals;
    theID = this->mGatheredResource == NULL ? (unsigned long)-1 : this->mGatheredResource->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theState = this->mNumAreaEffects;
    if ((theResult = outFile.Write(theState)) != 0 || (theResult = outFile.Write(this->mU2FC)) != 0)
        goto SaveGoals;
    theID = this->mLastAttacker == NULL ? (unsigned long)-1 : this->mLastAttacker->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theID = this->mOwner == NULL ? (unsigned long)-1 : this->mOwner->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0)
        goto SaveGoals;
    theFraction = this->mGarrisonCount < 1 || this->GetUnitClass()->mGarrisonCapacity < 1 ? 0.0f : (float)this->mGarrisonCount / this->GetUnitClass()->mGarrisonCapacity;
    if ((theResult = outFile.Write(theFraction)) != 0 || (theResult = outFile.Write(this->mGarrisonPopulation)) != 0
        || (theResult = outFile.Write(this->mIsPaidFor)) != 0 || (theResult = outFile.Write(this->mNumBuilders)) != 0)
        goto SaveGoals;
    theID = this->mMine == NULL ? (unsigned long)-1 : this->mMine->GetUnitID();
    if ((theResult = outFile.Write(theID)) != 0 || (theResult = this->mU0DC.Save(outFile)) != 0
        || (theResult = outFile.Write(this->mIsDetachedFromAI)) != 0 || (theResult = this->mColor.Save(outFile)) != 0
        || (theResult = outFile.Write(this->mIsTinted)) != 0 || (theResult = outFile.Write(this->mIsLocked)) != 0
        || (theResult = outFile.Write(this->mGateDirection)) != 0)
        goto SaveGoals;
    outFile.Write(this->mOpenFraction);

SaveGoals:
    theCount = this->mGoalQueues.size();
    theResult = outFile.Write(theCount);
    for (unsigned long item = 0; item < theCount; item++)
        if (theResult == 0)
            theResult = this->mGoalQueues[item].Save(inMajorVersion, inMinorVersion, outFile);
    theState = this->mConversionEffect.mValue;
    if (theResult == 0) {
        theResult = outFile.Write(theState);
        if (theResult == 0) {
            theState = this->mHealEffect.mValue;
            theResult = outFile.Write(theState);
        }
    }
    theID = this->mHerd == NULL ? 0 : this->mHerd->mID;
    if (theResult == 0)
        theResult = outFile.Write(theID);
    if (this->mHerd != NULL)
        this->mHerd->HasUnit(this);
    theID = this->mHolder == NULL ? (unsigned long)-1 : this->mHolder->GetUnitID();
    if (theResult == 0)
        outFile.Write(theID);
    theLength = this->mHeldUnits.size();
    theResult = outFile.Write(theLength);
    for (long n = 0; n < theLength; n++) {
        if (theResult != 0)
            goto SaveFinalFields;
        theID = this->mHeldUnits[n]->mUnitID;
        theResult = outFile.Write(theID);
    }
    if (theResult != 0 || (theResult = outFile.Write(this->mGrowth)) != 0
        || (theResult = outFile.Write(this->mB34F)) != 0 || (theResult = outFile.Write(this->mIsRegrowing)) != 0
        || (theResult = outFile.Write(this->mNumAttackers)) != 0 || (theResult = outFile.Write(this->mGrowTime)) != 0
        || (theResult = outFile.Write(this->mGrazeTime)) != 0 || (theResult = outFile.Write(this->mFireEffect.mValue)) != 0)
        goto SaveFinalFields;
    theHasPath = gPathManager.GetPathGoal(this, theGoalTile);
    if ((theResult = outFile.Write(theHasPath)) != 0 || (theResult = theGoalTile.Save(outFile)) != 0
        || (theResult = outFile.Write(this->mPositiveMorale)) != 0 || (theResult = outFile.Write(this->mNegativeMorale)) != 0)
        goto SaveFinalFields;
    theFraction = this->mStats->mPower < 1 ? 0.0f : (float)this->mPower / this->mStats->mPower;
    if ((theResult = outFile.Write(theFraction)) != 0 || (theResult = outFile.Write(this->mPowerRegenTimer)) != 0
        || (theResult = outFile.Write(this->mIsCloakable)) != 0)
        goto SaveFinalFields;
    theScale = this->mInitialContactLocation.GetX();
    if ((theResult = outFile.Write(theScale)) != 0)
        goto SaveFinalFields;
    theScale = this->mInitialContactLocation.GetY();
    if ((theResult = outFile.Write(theScale)) != 0)
        goto SaveFinalFields;
    theScale = this->mInitialContactLocation.GetZ();
    if ((theResult = outFile.Write(theScale)) != 0 || (theResult = outFile.Write(this->mInitialContactHeading)) != 0)
        goto SaveFinalFields;
    for (theBehavior = 0; theBehavior < 8; theBehavior++)
        if (EEUnitBehavior::sTable[theBehavior] == this->mBehavior)
            break;
    if ((theResult = outFile.Write(theBehavior)) != 0 || (theResult = outFile.Write(this->mExperience)) != 0)
        goto SaveFinalFields;
    theID = this->mLastResource == NULL ? (unsigned long)-1 : this->mLastResource->GetUnitID();
    if ((theResult = outFile.Write(theID)) == 0)
        outFile.Write(this->mIsFoundation);

SaveFinalFields:
    theLength = this->mUnitVariables.size();
    theResult = outFile.Write(theLength);
    for (long r = 0; r < theLength; r++) {
        if (theResult != 0)
            return theResult;
        long theValue = this->mUnitVariables[r];
        theResult = outFile.Write(theValue);
    }
    if (theResult == 0 && (theResult = outFile.Write(this->mFormationRank)) == 0
        && (theResult = outFile.Write(this->mWaterState)) == 0 && (theResult = this->mLastMapPosition.Save(outFile)) == 0
        && (theResult = outFile.Write(this->mCloaked)) == 0 && (theResult = outFile.Write(this->mCamouflageTimer)) == 0
        && (theResult = outFile.Write(this->mIsSpecialAttack)) == 0 && (theResult = outFile.Write(this->mDisabledReason)) == 0
        && (theResult = outFile.Write(this->mPlanetID)) == 0 && (theResult = outFile.Write(this->mSight)) == 0)
        theResult = outFile.Write(this->mU32C);
    // port (0.254): the running state the original did not save: the animation, collision and push, the AI state and
    // trigger time, the targets ignored for now and the tiles a bounded path avoids (Load)
    if (theResult == 0 && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        if ((theResult = outFile.Write(this->mAnimationFraction)) == 0 && (theResult = outFile.Write(this->mNextFrame)) == 0
            && (theResult = outFile.Write(this->mCollisionRadius)) == 0 && (theResult = outFile.Write(this->mPushVelocityX)) == 0
            && (theResult = outFile.Write(this->mPushVelocityY)) == 0 && (theResult = outFile.Write(this->mF24C)) == 0
            && (theResult = outFile.Write(this->mAIState)) == 0 && (theResult = outFile.Write(this->mNextTriggerTime)) == 0)
            theResult = outFile.Write((unsigned long)this->mIgnoredTargets.size());
        for (size_t k = 0; k < this->mIgnoredTargets.size() && theResult == 0; k++) {
            const EEComplexUnitAttacker& theAttacker = this->mIgnoredTargets[k];
            if ((theResult = outFile.Write(theAttacker.mTileX)) == 0 && (theResult = outFile.Write(theAttacker.mTileY)) == 0
                && (theResult = outFile.Write(theAttacker.mFromX)) == 0 && (theResult = outFile.Write(theAttacker.mFromY)) == 0
                && (theResult = outFile.Write(theAttacker.mUnitID)) == 0)
                theResult = outFile.Write(theAttacker.mTime);
        }
        if (theResult == 0)
            theResult = outFile.Write((unsigned long)this->mBoundedPathTiles.size());
        for (size_t k = 0; k < this->mBoundedPathTiles.size() && theResult == 0; k++) {
            if ((theResult = outFile.Write(this->mBoundedPathTiles[k].mX)) == 0)
                theResult = outFile.Write(this->mBoundedPathTiles[k].mY);
        }
        // port (0.256): the model's frame, the exact counters the original saved as fractions or set at load, and the
        // contact list's unit ids, head first (Load; sLoadedContacts)
        if (theResult == 0 && EESaveHasRunningState(inMajorVersion, inMinorVersion)) {
            if ((theResult = outFile.Write(this->mPhysicalModel.GetFrame())) == 0 && (theResult = outFile.Write(this->mFlightTime)) == 0
                && (theResult = outFile.Write(this->mRottingAwayTime)) == 0 && (theResult = outFile.Write(this->mCurrentResearch)) == 0
                && (theResult = outFile.Write(this->mLastHitPoints)) == 0 && (theResult = outFile.Write(this->mGarrisonCount)) == 0
                && (theResult = outFile.Write(this->mPower)) == 0)
                theResult = outFile.Write((unsigned long)this->mContacts.mCount);
            for (UListNode<EEComplexUnit*>* theNode = this->mContacts.mHead; theNode != NULL && theResult == 0; theNode = theNode->mNext)
                theResult = outFile.Write(theNode->mData->mUnitID);
        }
    }
    return theResult;
}

// TEMPLATE: EXE 0x5f5180
// std::vector<EETileCoordinate>::operator=

// TEMPLATE: EXE 0x5f52a4
// std::vector<EETileCoordinate>::push_back

// TEMPLATE: EXE 0x5f52d0
// std::deque<EEComplexUnitWaypoint>::push_back

// TEMPLATE: EXE 0x5f5364
// UReferenceContainer<EEAreaEffect*>::Clear

// TEMPLATE: EXE 0x5f5388
// std::vector<long>::end

// TEMPLATE: EXE 0x5f5394
// std::deque<EEComplexUnitWaypoint>::iterator::operator*

// TEMPLATE: EXE 0x5f53c8
// U2DPoint<float>::Load (mU0DC)

// TEMPLATE: EXE 0x5f53fc
// U2DPoint<float>::Save

// (0x5f5430 EEUnitBehavior::Get: our RepairPointers/ResetEEComplexUnit still expand it inline; no marker yet)
// TEMPLATE: EXE 0x5f543e
// std::vector<EEPathManagerPending>::push_back (Load's gPathManager.mPending.push_back)

// TEMPLATE: EXE 0x5f546a
// std::vector<EEPathManagerPending>::insert(iterator, const T&)

// TEMPLATE: EXE 0x5f54c4
// std::vector<EEPathManagerPending>::insert(iterator, size_type, const T&)

// TEMPLATE: EXE 0x5f5784
// std::vector<EEPathManagerPending>::_Ufill: NOT emitted by our compile (our insert(n) expands it; Dinkum _Ufill inline budget, depth)

// TEMPLATE: EXE 0x5f57ab
// std::vector<EEPathManagerPending>::_Ucopy

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEComplexDoppelganger::IsComplex()
{
    return true;
}
