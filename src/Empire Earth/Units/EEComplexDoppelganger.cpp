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
#include "ee/EEUGRust.h"
#include "ee/EEUGRust.h"

// port (save version 0.256): each loaded unit's contact (collision) list as saved, by unit id, head first. The load
// makes its own contacts while it places the units, and EEPhysicsModel reads the list before it rebuilds it (a unit
// without contacts skips its collision step), so the loaded units collided differently in their first tick. Put back
// at the first tick in two passes over every unit (clear, then add): ClearContacts also removes the unit from its
// contacts' lists, so a unit-by-unit rebuild would undo its neighbours'.
static std::vector<std::pair<std::pair<EEComplexUnit*, unsigned long>, std::vector<unsigned long> > > sLoadedContacts;

static EEComplexUnit* LoadedComplexUnit(unsigned long inID)
{
    return EEUnitCast<EEComplexUnit>(static_cast<EEUnit*>(gWorld.GetUnit(inID)));
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

// Creates an empty complex doppelganger with no model, hit points or tile
EEComplexDoppelganger::EEComplexDoppelganger()
{
    this->Reset();
}

// Returns the remembered unit's animation state
long EEComplexDoppelganger::GetAnimation()
{
    return this->mAnimation;
}

// Returns itself: this doppelganger is a complex one
EEComplexDoppelganger* EEComplexDoppelganger::GetComplexDoppelganger()
{
    return this;
}

// Returns whether the remembered unit was fully built (the top bit of the packed hit points)
bool EEComplexDoppelganger::IsBuilt()
{
    return (this->mPackedHitPoints & 0x80000000) > 0;
}

// Returns the remembered animation frame
long EEComplexDoppelganger::GetFrame()
{
    return this->mFrame;
}

// Returns the remembered model
EEModel* EEComplexDoppelganger::GetModel()
{
    return this->mModel;
}

// Returns whether the remembered unit was a building foundation
bool EEComplexDoppelganger::IsFoundation()
{
    return this->mIsFoundation;
}

// Copies the remembered position and orientation into the caller's transformation
void EEComplexDoppelganger::GetTransformation(GETransformation& outTransformation)
{
    outTransformation = this->mTransformation;
}

// Returns the remembered scale
float EEComplexDoppelganger::GetScale()
{
    return this->mScale;
}

// Returns the remembered hit points (a resource's amount) without the built bit
long EEComplexDoppelganger::GetHitPoints()
{
    return this->mPackedHitPoints & 0x7fffffff;
}

// Returns the tile X the unit was seen on
long EEComplexDoppelganger::GetTileX()
{
    return this->mTileX;
}

// Returns the tile Y the unit was seen on
long EEComplexDoppelganger::GetTileY()
{
    return this->mTileY;
}

// Destroys the complex doppelganger, releasing its unit and model media
EEComplexDoppelganger::~EEComplexDoppelganger()
{
    this->Clear();
}

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

// Releases the remembered unit and the model's media, then resets
void EEComplexDoppelganger::Clear()
{
    EEDoppelganger::Clear();
    if (this->mModel != NULL)
        this->mModel->UnloadMedia();
    this->Reset();
}

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

// The sections of EEComplexUnit::Load (ee_units unit_load.rs EE_UNITS_UNIT_LOAD_SECTION_*): the containers with the
// original's loops (io.i carries the original's theValue, or theState, in and out; io.b = 1 when the held units' loop
// left for LoadLateVersions) and the port's live state; the result
static int32_t EEComplexUnitLoadSection(EEUnitSaveContext& inContext, uint32_t inIndex, EeUnitsUnitSaveIo* io)
{
    FSFile& inFile = *inContext.mFile;
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inContext.mUnit);
    unsigned long inMajorVersion = inContext.mMajorVersion;
    unsigned long inMinorVersion = inContext.mMinorVersion;
    unsigned long theValue = (unsigned long)io->i;
    long theCount = 0;
    long theResult = 0;
    switch (inIndex) {
    case EE_UNITS_UNIT_LOAD_SECTION_BUILD_QUEUE: {
        long theFirst;
        unsigned long theSecond;
        theUnit->mBuildQueue.clear();
        theResult = inFile.Read(theCount);
        for (long i = 0; i < theCount && theResult == 0; i++) {
            theResult = inFile.Read(theFirst);
            if (theResult == 0) {
                if (inMinorVersion < 0xf8)
                    theSecond = 0;
                else
                    theResult = inFile.Read(theSecond);
                if (theResult == 0) {
                    EEProductionEntry thePair = {theFirst, theSecond};
                    theUnit->mBuildQueue.push_back(thePair);
                }
            }
        }
        return theResult;
    }
    case EE_UNITS_UNIT_LOAD_SECTION_CITIZEN_QUEUE:
        theUnit->mCitizenBuildQueue.clear();
        theResult = inFile.Read(theCount);
        for (long j = 0; j < theCount && theResult == 0; j++) {
            theResult = inFile.Read(theValue);
            if (theResult == 0)
                theUnit->mCitizenBuildQueue.push_back(reinterpret_cast<EEUnit*>(theValue));
        }
        io->i = (int64_t)theValue;
        return theResult;
    case EE_UNITS_UNIT_LOAD_SECTION_CARGO:
        theUnit->mCargo.clear();
        theResult = inFile.Read(theCount);
        for (long k = 0; k < theCount && theResult == 0; k++) {
            theResult = inFile.Read(theValue);
            if (theResult == 0)
                theUnit->mCargo.push_back(reinterpret_cast<EEComplexUnit*>(theValue));
        }
        io->i = (int64_t)theValue;
        return theResult;
    case EE_UNITS_UNIT_LOAD_SECTION_WAYPOINTS: {
        EEComplexUnitWaypoint theWaypoint;
        theUnit->mWaypoints.clear();
        theResult = inFile.Read(theCount);
        for (long p = 0; p < theCount && theResult == 0; p++) {
            theResult = theWaypoint.mLocation.Load(inFile);
            if (theResult == 0 && (theResult = inFile.Read(theWaypoint.mDistance)) == 0
                && (theResult = inFile.Read(theWaypoint.mStartDistance)) == 0)
                theUnit->mWaypoints.push_back(theWaypoint);
        }
        return theResult;
    }
    case EE_UNITS_UNIT_LOAD_SECTION_GOAL_QUEUES: {
        unsigned long theGoalCount;
        theResult = inFile.Read(theGoalCount);
        for (unsigned long goal = 0; goal < theGoalCount; goal++)
            if (theResult == 0)
                theResult = (goal < theUnit->mGoalQueues.size() ? theUnit->mGoalQueues[goal] : *inContext.mGoals)
                                .Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == 0 && (theResult = inFile.Read(theUnit->mConversionEffect.mValue)) == 0)
            theResult = inFile.Read(theUnit->mHealEffect.mValue);
        return theResult;
    }
    case EE_UNITS_UNIT_LOAD_SECTION_HELD_UNITS:
        io->b = 0;
        theResult = inFile.Read(theCount);
        theUnit->mHeldUnits.resize(theCount, NULL);
        for (long n = 0; n < theCount; n++) {
            if (theResult != 0) {
                io->b = 1;   // goto LoadLateVersions
                break;
            }
            theResult = inFile.Read(theValue);
            theUnit->mHeldUnits[n] = reinterpret_cast<EEComplexUnit*>(theValue);
        }
        io->i = (int64_t)theValue;
        return theResult;
    case EE_UNITS_UNIT_LOAD_SECTION_UNIT_VARIABLES: {
        long theState = (long)io->i;
        theResult = inFile.Read(theCount);
        for (long r = 0; r < theCount && theResult == 0; r++) {
            theResult = inFile.Read(theState);
            if (theResult == 0)
                theUnit->mUnitVariables[r] = theState;
        }
        io->i = (int64_t)theState;
        return theResult;
    }
    case EE_UNITS_UNIT_LOAD_SECTION_LIVE_STATE:
        break;
    default:
        return 0;
    }
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
        // the turret angles as read above: the load's LoadMedia makes the array again, all zero, after Load (a
        // frigate's turrets then had to turn to their target again: space game, seed 2's 1900 s save, where the
        // continued frigate fired at its first tick)
        std::vector<float> mTurretAngles;
    } theLive;
    for (long q = 0; q < theUnit->mNumTurretAngles && theUnit->mTurretAngles != NULL; q++)
        theLive.mTurretAngles.push_back(theUnit->mTurretAngles[q]);
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
    unsigned long theUnitID = theUnit->mUnitID;
    EEComplexUnit* theSelf = theUnit;   // only while it is the world's unit of that id (not a doppelganger, not freed)
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
            unsigned long theParts = theUnit->GetModel() != NULL ? theUnit->GetModel()->GetNumParts() : 0;
            for (unsigned long q = 0; q < theLive.mTurretAngles.size() && q < theParts && theUnit->mTurretAngles != NULL; q++)
                theUnit->mTurretAngles[q] = theLive.mTurretAngles[q];
        });
    return theResult;
}

// Reads the complex unit's state from the saved game, keeping units and groups as ids for RepairPointers and queueing
// its pending path; fields newer than the file get defaults; returns the read status. port (R5b.4, round 26): the
// body is Rust (ee_units unit_load.rs, with RTSUnit's and EEUnit's parts) over the records this class derives from,
// EEUnitSaveOperation's parts and EEComplexUnitLoadSection's sections
long EEComplexUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEUnitVectorItem theGoals(this);
    EEUnitSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, &theGoals};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                               (uint8_t)(EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0),
                               (uint8_t)(EESaveHasRunningState(inMajorVersion, inMinorVersion) ? 1 : 0)};
    return ee_units_unit_load(&theContext, &EEUnitSaveOperation, this, this, this, &theIn);
}

// Load stores the saved fractions as floats in the longs.
// Turns the saved hit point, flight, garrison and power fractions back into amounts (at least 1 if nonzero; port,
// R5b.4 round 27: ee_units unit_load.rs finish_fractions over the record); marks an old save's (<= 200) unbuilt land
// building at 1 hit point as a foundation, reloading its media
long EEComplexUnit::FinishLoad(long inMajorVersion, long inMinorVersion)
{
    EeUnitsFinishLoadIn theMaxima = {this->mStats->mMaxHitPoints, this->mStats->mFlightTime,
                                     this->GetUnitClass()->mGarrisonCapacity, this->mStats->mPower};
    ee_units_unit_finish_fractions(this, &theMaxima);
    if (inMajorVersion == 0 && (unsigned long)inMinorVersion <= 200) {
        this->mIsFoundation = this->GetUnitClass()->IsLandBuilding() && this->GetUnitClass()->GetUnitType() != kUnitTypeFarm && !this->mIsBuilt && this->GetHitPoints() == 1;
        if (this->mIsFoundation == true) {
            this->GetPlayer()->UnloadUnitClassMedia(this->GetUnitClass()->GetUnitType());
            gDataPreLoading->PostLoadUnitClass(this->GetPlayer()->mPlayerID, this->GetUnitClass()->GetUnitType());
        }
    }
    return EEUnit::FinishLoad(inMajorVersion, inMinorVersion);
}

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
                    EEUGRust* theAttack = static_cast<EEUGRust*>(theGoal);
                    if (theAttack->GetAttacker() != this)
                        theAttack->SetAttacker(this);
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
        EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mCargo[cargo])));
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
                    EEUGRust* theAttack = static_cast<EEUGRust*>(theGoal);
                    if (theAttack->GetAttacker() != this)
                        theAttack->SetAttacker(this);
                }
            } else if (theGoal->mKind == kGoalPatrol)
                this->GetPlayer()->AddUnitToRepatrol(this);
            if (!theFailed)
                theFailed = theResult != 0;
            if (theGoal->mKind == kGoalLocation) {
                EEUGRust* theLocation = EELocationGoalCast(theGoal);
                theAutoWaypoint = theLocation->GetIsAutoWaypoint() == true;
            } else
                theAutoWaypoint = false;
            if (theResult == 0 && item == 0 && theGoal->mKind == kGoalBuildRepair)
                this->mCurrentResearch = static_cast<EEUGRust*>(theGoal)->GetBuildTechTreeID();
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
    this->mPreviousPart = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mPreviousPart)));
    if (this->mPreviousPart != NULL)
        this->mPreviousPart->Reference();
    this->mNextPart = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mNextPart)));
    if (this->mNextPart != NULL)
        this->mNextPart->Reference();
    this->mHomeBase = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mHomeBase)));
    if (this->mHomeBase != NULL)
        this->mHomeBase->Reference();
    this->mContainer = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mContainer)));
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
        this->mLastAttacker = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(theAttackerID));
        if (this->mLastAttacker == NULL && EESaveHasGhosts(inMajorVersion, inMinorVersion))
            this->mLastAttacker = EEFindLoadedGhost(theAttackerID);   // port (0.262): an attacker that died, as the saved game kept it
    }
    if (this->mLastAttacker != NULL)
        this->mLastAttacker->Reference();
    if (this->mOwner == (EEComplexUnit*)-1 || this->mOwner == NULL)
        this->mOwner = NULL;
    else {
        this->mOwner = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mOwner)));
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
        this->mHeldUnits[linked] = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mHeldUnits[linked])));
        if (this->mHeldUnits[linked] != NULL)
            this->mHeldUnits[linked]->Reference();
        else
            this->mHeldUnits.erase(this->mHeldUnits.begin() + linked);
    }
    if (this->mHolder == (EEComplexUnit*)-1)
        this->mHolder = NULL;
    else {
        this->mHolder = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(this->mHolder)));
        if (this->mHolder != NULL)
            this->mHolder->Reference();
    }
    if (theLostContainer == true)
        this->GetPlayer()->AddStrandedUnit(this);
    unsigned long theBehavior = USavedID(this->mBehavior);
    if (theBehavior < 8) {
        // port (saves from 0.254 on, no format change): the harass and cautious behaviours (3, 4) stay as saved. The
        // original made them the class's: the computer players give their explorers harass on Hard
        // (EECPExplorationManager), which can flee, and a loaded explorer stood and fought where the saved one ran
        // (seed 7's 600 s save: ShouldIRun, 9 ticks after the load)
        if ((theBehavior == 3 || theBehavior == 4) && !EESaveHasLiveState(inMajorVersion, inMinorVersion))
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
            if (theGoal == NULL || EELocationGoalCast(theGoal) == NULL
                || !EELocationGoalCast(theGoal)->GetIsAutoWaypoint())
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
// Shows the model of the unit's current animation state at the given frame
void EEComplexUnit::SetAnimationFrame(unsigned long inFrame)
{
    unsigned long theState = this->mAnimation;
    this->mPhysicalModel.SetModel(this->GetModel()->mModels[theState]);
    this->mPhysicalModel.SetFrame(inFrame);
}

// Writes the complex unit's state to the saved game, units and groups as ids and hit points, flight time, garrison
// and power as fractions of their maximum; returns the write status.
// port (R5b.4, round 26): the body is Rust (ee_units unit_save.rs, with RTSUnit's and EEUnit's parts) over the records
// this class derives from and EEUnitSaveOperation's parts
long EEComplexUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEUnitSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                               (uint8_t)(EESaveHasLiveState(inMajorVersion, inMinorVersion) ? 1 : 0),
                               (uint8_t)(EESaveHasRunningState(inMajorVersion, inMinorVersion) ? 1 : 0)};
    return ee_units_unit_save(&theContext, &EEUnitSaveOperation, this, this, this, &theIn);
}

// The C++ parts of the units' saves (ee_units unit_save.rs): the file's primitives with the overload the original
// called, the containers' sections with the original's loops, the composite members' Save, the ids it wrote for
// pointers and the maxima of the fractions
int32_t EEUnitSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsUnitSaveIo* io)
{
    EEUnitSaveContext& theContext = *static_cast<EEUnitSaveContext*>(inContext);
    FSFile& outFile = *theContext.mFile;
    FSFile& inFile = outFile;
    EEUnit* theUnit = static_cast<EEUnit*>(theContext.mUnit);   // (the unit operations only)
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);   // (the complex operations only)
    long theResult = 0;
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(outFile, inOp, io);
    switch (inOp) {
    case EE_UNITS_UNIT_SAVE_MAX:
        io->i = inIndex == EE_UNITS_UNIT_SAVE_MAX_HIT_POINTS ? (int64_t)theUnit->mStats->mMaxHitPoints
              : inIndex == EE_UNITS_UNIT_SAVE_MAX_FLIGHT_TIME ? (int64_t)theUnit->mStats->mFlightTime
              : inIndex == EE_UNITS_UNIT_SAVE_MAX_POWER ? (int64_t)theUnit->mStats->mPower
              : (int64_t)theUnit->GetUnitClass()->mGarrisonCapacity;
        return 0;
    case EE_UNITS_UNIT_SAVE_FLOAT:
        io->f = inIndex == EE_UNITS_UNIT_SAVE_FLOAT_SCALE ? theComplex->mPhysicalModel.GetScale()
              : inIndex == EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_X ? theComplex->mInitialContactLocation.GetX()
              : inIndex == EE_UNITS_UNIT_SAVE_FLOAT_CONTACT_Y ? theComplex->mInitialContactLocation.GetY()
              : theComplex->mInitialContactLocation.GetZ();
        return 0;
    case EE_UNITS_UNIT_SAVE_COLLISION_TILE:
        io->i = (int64_t)(inIndex == 0 ? theComplex->mCollisionTile.mX : theComplex->mCollisionTile.mY);
        return 0;
    case EE_UNITS_UNIT_SAVE_ID: {
        unsigned long theID = (unsigned long)-1;
        switch (inIndex) {
        case EE_UNITS_UNIT_SAVE_ID_FRAME: theID = theComplex->mPhysicalModel.GetFrame(); break;
        case EE_UNITS_UNIT_SAVE_ID_LAUNCHER_CLASS:
            theID = theComplex->mLauncherClass == NULL ? 0 : theComplex->mLauncherClass->GetUnitType();
            break;
        case EE_UNITS_UNIT_SAVE_ID_PATROL_GROUP:
            if (theComplex->mPatrolGroup != NULL) theID = theComplex->mPatrolGroup->mID;
            break;
        case EE_UNITS_UNIT_SAVE_ID_PREVIOUS_PART:
            if (theComplex->mPreviousPart != NULL) theID = theComplex->mPreviousPart->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_NEXT_PART:
            if (theComplex->mNextPart != NULL) theID = theComplex->mNextPart->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_HOME_BASE:
            if (theComplex->mHomeBase != NULL) theID = theComplex->mHomeBase->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_CONTAINER:
            if (theComplex->mContainer != NULL && theComplex->mContainer->mUnitClass->mTransportCapacity != 0)
                theID = theComplex->mContainer->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_COMMAND: theID = theComplex->mCommand == NULL ? 0 : theComplex->mCommand->mType; break;
        case EE_UNITS_UNIT_SAVE_ID_GATHERED_RESOURCE:
            if (theComplex->mGatheredResource != NULL) theID = theComplex->mGatheredResource->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_LAST_ATTACKER:
            if (theComplex->mLastAttacker != NULL) theID = theComplex->mLastAttacker->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_OWNER:
            if (theComplex->mOwner != NULL) theID = theComplex->mOwner->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_MINE:
            if (theComplex->mMine != NULL) theID = theComplex->mMine->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_HERD: theID = theComplex->mHerd == NULL ? 0 : theComplex->mHerd->mID; break;
        case EE_UNITS_UNIT_SAVE_ID_HOLDER:
            if (theComplex->mHolder != NULL) theID = theComplex->mHolder->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_BEHAVIOR:
            for (theID = 0; theID < 8; theID++)
                if (EEUnitBehavior::sTable[theID] == theComplex->mBehavior)
                    break;
            break;
        case EE_UNITS_UNIT_SAVE_ID_LAST_RESOURCE:
            if (theComplex->mLastResource != NULL) theID = theComplex->mLastResource->GetUnitID();
            break;
        case EE_UNITS_UNIT_SAVE_ID_CONVERSION_EFFECT: io->i = (int64_t)(long)theComplex->mConversionEffect.mValue; return 0;
        case EE_UNITS_UNIT_SAVE_ID_HEAL_EFFECT: io->i = (int64_t)(long)theComplex->mHealEffect.mValue; return 0;
        default: break;
        }
        io->i = (int64_t)theID;
        return 0;
    }
    case EE_UNITS_UNIT_SAVE_PART:
        switch (inIndex) {
        case EE_UNITS_UNIT_SAVE_PART_PROJECTILE_START: return theComplex->mProjectileStart.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_LOCATION: return theComplex->mPhysicalModel.GetLocation().Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_ANGULAR_VELOCITY: return theComplex->mAngularVelocity.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_ORIENTATION: return theComplex->mOrientation.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_U0DC: return theComplex->mU0DC.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_COLOR: return theComplex->mColor.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_FIRE_EFFECT: return outFile.Write(theComplex->mFireEffect.mValue);
        case EE_UNITS_UNIT_SAVE_PART_PATH: {
            U2DPoint<long> theGoalTile;
            bool theHasPath = gPathManager.GetPathGoal(theComplex, theGoalTile);
            if ((theResult = outFile.Write(theHasPath)) == 0)
                theResult = theGoalTile.Save(outFile);
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_PART_LAST_MAP_POSITION: return theComplex->mLastMapPosition.Save(outFile);
        case EE_UNITS_UNIT_SAVE_PART_HERD_HAS_UNIT:
            if (theComplex->mHerd != NULL)
                theComplex->mHerd->HasUnit(theComplex);
            return 0;
        default: return 0;
        }
    case EE_UNITS_UNIT_SAVE_SECTION:
        switch (inIndex) {
        case EE_UNITS_UNIT_SAVE_SECTION_BUILD_QUEUE: {
            long theLength = theComplex->mBuildQueue.size();
            theResult = outFile.Write(theLength);
            for (long i = 0; i < theLength && theResult == 0; i++) {
                long theFirst = theComplex->mBuildQueue[i].mTechnology;
                theResult = outFile.Write(theFirst);
                if (theResult == 0) {
                    unsigned long theSecond = theComplex->mBuildQueue[i].mFlag;
                    theResult = outFile.Write(theSecond);
                }
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_CITIZEN_QUEUE: {
            long theLength = 0;
            long theQueued = theComplex->mCitizenBuildQueue.size();
            for (long j = 0; j < theQueued; j++)
                if (theComplex->mCitizenBuildQueue[j]->mIsValid)
                    theLength++;
            theResult = outFile.Write(theLength);
            for (long j = 0; j < theQueued && theResult == 0; j++) {
                if (theComplex->mCitizenBuildQueue[j]->mIsValid) {
                    unsigned long theID = theComplex->mCitizenBuildQueue[j]->mUnitID;
                    theResult = outFile.Write(theID);
                }
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_CARGO: {
            long theLength = theComplex->mCargo.size();
            theResult = outFile.Write(theLength);
            for (long k = 0; k < theLength && theResult == 0; k++) {
                unsigned long theID = theComplex->mCargo[k]->mUnitID;
                theResult = outFile.Write(theID);
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_WAYPOINTS: {
            long theLength = theComplex->mWaypoints.size();
            theResult = outFile.Write(theLength);
            for (long p = 0; p < theLength && theResult == 0; p++) {
                EEComplexUnitWaypoint theWaypoint = theComplex->mWaypoints[p];
                theResult = theWaypoint.mLocation.Save(outFile);
                if (theResult == 0) {
                    theResult = outFile.Write(theWaypoint.mDistance);
                    if (theResult == 0)
                        theResult = outFile.Write(theWaypoint.mStartDistance);
                }
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_TURRET_ANGLES: {
            long theLength = theComplex->GetModel()->GetNumParts();
            theResult = outFile.Write(theLength);
            for (long q = 0; q < theLength && theResult == 0; q++)
                theResult = outFile.Write(theComplex->mTurretAngles[q]);
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_GOAL_QUEUES: {
            unsigned long theCount = theComplex->mGoalQueues.size();
            theResult = outFile.Write(theCount);
            for (unsigned long item = 0; item < theCount; item++)
                if (theResult == 0)
                    theResult = theComplex->mGoalQueues[item].Save(theContext.mMajorVersion, theContext.mMinorVersion, outFile);
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_HELD_UNITS: {
            long theLength = theComplex->mHeldUnits.size();
            theResult = outFile.Write(theLength);
            for (long n = 0; n < theLength && theResult == 0; n++) {
                unsigned long theID = theComplex->mHeldUnits[n]->mUnitID;
                theResult = outFile.Write(theID);
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_UNIT_VARIABLES: {
            long theLength = theComplex->mUnitVariables.size();
            theResult = outFile.Write(theLength);
            for (long r = 0; r < theLength && theResult == 0; r++) {
                long theValue = theComplex->mUnitVariables[r];
                theResult = outFile.Write(theValue);
            }
            return theResult;
        }
        case EE_UNITS_UNIT_SAVE_SECTION_IGNORED_TARGETS:
            theResult = outFile.Write((unsigned long)theComplex->mIgnoredTargets.size());
            for (size_t k = 0; k < theComplex->mIgnoredTargets.size() && theResult == 0; k++) {
                const EEComplexUnitAttacker& theAttacker = theComplex->mIgnoredTargets[k];
                if ((theResult = outFile.Write(theAttacker.mTileX)) == 0 && (theResult = outFile.Write(theAttacker.mTileY)) == 0
                    && (theResult = outFile.Write(theAttacker.mFromX)) == 0 && (theResult = outFile.Write(theAttacker.mFromY)) == 0
                    && (theResult = outFile.Write(theAttacker.mUnitID)) == 0)
                    theResult = outFile.Write(theAttacker.mTime);
            }
            return theResult;
        case EE_UNITS_UNIT_SAVE_SECTION_BOUNDED_PATH_TILES:
            theResult = outFile.Write((unsigned long)theComplex->mBoundedPathTiles.size());
            for (size_t k = 0; k < theComplex->mBoundedPathTiles.size() && theResult == 0; k++) {
                if ((theResult = outFile.Write(theComplex->mBoundedPathTiles[k].mX)) == 0)
                    theResult = outFile.Write(theComplex->mBoundedPathTiles[k].mY);
            }
            return theResult;
        case EE_UNITS_UNIT_SAVE_SECTION_CONTACTS:
            theResult = outFile.Write((unsigned long)theComplex->mContacts.mCount);
            for (UListNode<EEComplexUnit*>* theNode = theComplex->mContacts.mHead; theNode != NULL && theResult == 0; theNode = theNode->mNext)
                theResult = outFile.Write(theNode->mData->mUnitID);
            return theResult;
        default: return 0;
        }
    // ---- the loads (ee_units unit_load.rs)
    case EE_UNITS_UNIT_LOAD_TURRET_ANGLE:
        return inFile.Read(theComplex->mTurretAngles[inIndex]);
    case EE_UNITS_UNIT_LOAD_PART:
        switch (inIndex) {
        case EE_UNITS_UNIT_LOAD_PART_PROJECTILE_START: return theComplex->mProjectileStart.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_LOCATION: {
            GETransformation theLocation(false);
            theResult = theLocation.Load(inFile);
            if (theResult == 0)
                theComplex->mPhysicalModel.SetLocation(theLocation);
            return theResult;
        }
        case EE_UNITS_UNIT_LOAD_PART_ANGULAR_VELOCITY: return theComplex->mAngularVelocity.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_ORIENTATION: return theComplex->mOrientation.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_U0DC: return theComplex->mU0DC.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_COLOR: return theComplex->mColor.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_FIRE_EFFECT: return inFile.Read(theComplex->mFireEffect.mValue);
        case EE_UNITS_UNIT_LOAD_PART_PATH: {
            bool theHasPath = false;
            U2DPoint<long> theGoalTile;
            theResult = inFile.Read(theHasPath);
            if (theResult == 0)
                theResult = theGoalTile.Load(inFile);
            if (!theHasPath) {
                EEPathManagerPending thePending;
                thePending.mUnitID = theComplex->mUnitID;
                thePending.mTile.mX = theGoalTile.mX;
                thePending.mTile.mY = theGoalTile.mY;
                gPathManager.mPending.push_back(thePending);
            }
            return theResult;
        }
        case EE_UNITS_UNIT_LOAD_PART_LAST_MAP_POSITION: return theComplex->mLastMapPosition.Load(inFile);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_X: return inFile.Read(theComplex->mInitialContactLocation.mX);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_Y: return inFile.Read(theComplex->mInitialContactLocation.mY);
        case EE_UNITS_UNIT_LOAD_PART_CONTACT_Z: return inFile.Read(theComplex->mInitialContactLocation.mZ);
        default: return 0;
        }
    case EE_UNITS_UNIT_LOAD_SET: {
        unsigned long theValue = (unsigned long)io->i;
        switch (inIndex) {
        case EE_UNITS_UNIT_LOAD_SET_DEFAULT_BEHAVIOR: theComplex->mBehavior = EEUnitBehavior::sTable[5]; break;
        case EE_UNITS_UNIT_LOAD_SET_SCALE: {
            float theScale = io->f;
            if (GetIsLoadingScenario() || theScale == 0.0f) {
                if (theComplex->GetUnitClass() == NULL) {
                    if (theScale == 0.0f)
                        theScale = 1.0f;
                } else
                    theScale = theComplex->GetUnitClass()->GetScale();
            }
            theComplex->mPhysicalModel.SetScale(theScale);
            break;
        }
        case EE_UNITS_UNIT_LOAD_SET_FRAME: theComplex->mPhysicalModel.SetFrame(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_X: theComplex->mCollisionTile.mX = (long)io->i; break;
        case EE_UNITS_UNIT_LOAD_SET_COLLISION_TILE_Y: theComplex->mCollisionTile.mY = (long)io->i; break;
        case EE_UNITS_UNIT_LOAD_SET_LAUNCHER_CLASS:
            theComplex->mLauncherClass = theValue == 0 || UIsNone32(theValue) ? NULL : gUnitClassTable[theValue];
            break;
        case EE_UNITS_UNIT_LOAD_SET_PATROL_GROUP: theComplex->mPatrolGroup = USavedPointer<EEPPatrolGroup>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_PREVIOUS_PART: theComplex->mPreviousPart = reinterpret_cast<EEComplexUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_NEXT_PART: theComplex->mNextPart = reinterpret_cast<EEComplexUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_HOME_BASE: theComplex->mHomeBase = reinterpret_cast<EEComplexUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_CONTAINER: theComplex->mContainer = reinterpret_cast<EEComplexUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_ACTION:
            if (theComplex->GetUnitClass() != NULL && theComplex->GetUnitClass()->FindAction(theValue) != NULL) {
                if (theValue == 0)
                    theValue = 5000;
                theComplex->StartAction(theValue);
            }
            break;
        case EE_UNITS_UNIT_LOAD_SET_TURRETS: theComplex->ResizeTurretAngles((long)io->i); break;
        case EE_UNITS_UNIT_LOAD_SET_GATHERED_RESOURCE: theComplex->mGatheredResource = reinterpret_cast<EEUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER_CLEAR:
            if (theComplex->mLastAttacker != NULL)
                theComplex->mLastAttacker->Dereference();
            theComplex->mLastAttacker = NULL;
            break;
        case EE_UNITS_UNIT_LOAD_SET_LAST_ATTACKER: theComplex->mLastAttacker = reinterpret_cast<EEComplexUnit*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_OWNER_CLEAR:
            if (theComplex->mOwner != NULL)
                theComplex->mOwner->Dereference();
            theComplex->mOwner = NULL;
            break;
        case EE_UNITS_UNIT_LOAD_SET_OWNER: theComplex->mOwner = USavedPointer<EEComplexUnit>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_MINE: theComplex->mMine = USavedPointer<EEUnit>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_MINE_UNSET: theComplex->mMine = (EEUnit*)-1; break;
        case EE_UNITS_UNIT_LOAD_SET_HERD: theComplex->mHerd = reinterpret_cast<EEHerd*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_HOLDER: theComplex->mHolder = USavedPointer<EEComplexUnit>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_BEHAVIOR: theComplex->mBehavior = reinterpret_cast<EEUnitBehavior*>(theValue); break;
        case EE_UNITS_UNIT_LOAD_SET_LAST_RESOURCE: theComplex->mLastResource = reinterpret_cast<EEUnit*>(theValue); break;
        default: break;
        }
        return 0;
    }
    case EE_UNITS_UNIT_LOAD_SECTION:
        return EEComplexUnitLoadSection(theContext, inIndex, io);
    default:
        return 0;
    }
}

bool EEComplexDoppelganger::IsComplex()
{
    return true;
}
