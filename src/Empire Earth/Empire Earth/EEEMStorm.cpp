// C:\EEX\Empire Earth\Empire Earth\EEEMStorm.cpp (name from RTTI EEEMStorm; no __FILE__). 0x12dba8..0x12ed4e: the
// storm keys and colours ($E chains), the storm route planners (Snow/Rain +4), GetTypeName, EEEMStorm (ctor .. Update)
// and EEAIBPGModifierStormShaper. EECommandLine.cpp ends at 0x12dba8, EEEnvironmentManager.cpp starts at 0x12ed4e.
// Unit flags: the exe's module set. (provisional names)
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <deque>
#include <algorithm>

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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "lle/LowLevelEngineCore.h"
#include "lle/FSRestorable.h"
#include "lle/U2DRectangle.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETileCoordinate.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIHill.h"
#include "ee/EETown.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEPathManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect.h"
#include "rust/sim/app.h"
#include "rust/sim/astar.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEStormModifiers.h"
#include "ee/EEStormType.h"
#include "ee/EEAppCapture.h"
#include "rust/sim/app.h"
#include "rust/sim/astar.h"

// Constructs the storm's "Storm-Data" key at startup and destroys it at exit (wrapper, constructor, registrar,
// destructor)
UString EEEMStorm::sStormData("Storm-Data");
// Constructs the storm's "Environment-Data" key at startup and destroys it at exit (wrapper, constructor,
// registrar, destructor)
UString EEEMStorm::sEnvironmentData("Environment-Data");
// Constructs the black storm colour at startup and registers its destructor
GEColor gStormColor;                        // (read by Step: the environment manager's colour when a rain storm ends)
// Constructs the light storm colour at startup and registers its destructor
GEColor gStormColorLight(0.679f, 0.726f, 0.8f);
// Constructs the dark storm colour at startup and registers its destructor
GEColor gStormColorDark(0.273f, 0.343f, 0.457f);

// whether a route step's sectors' centres share an axis (the planners' straight step)
static bool EEStormStraight(EEAIPathSubSector* inFrom, EEAIPathSubSector* inTo)
{
    return inFrom->GetSector()->GetCenter().mX == inTo->GetSector()->GetCenter().mX ||
           inFrom->GetSector()->GetCenter().mY == inTo->GetSector()->GetCenter().mY;
}

// Returns the cost of a snow storm's route step: cheap over cliffs and hills, dear over forest, shallows and the
// map edge (ee_app storm.rs)
long EEAIGPGModifierStormRoutePlannerSnow::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theKind = inTo->GetKind();
    bool theHill = (theKind == kAITileLand || theKind == kAITileWall || theKind == kAITileGate) &&
                   gContinentManager.GetHillAt(inTo->GetLocation()) != NULL;
    return ee_app_storm_route_cost(true, theFrom->GetSector() != inTo->GetSector(), EEStormStraight(theFrom, inTo), theKind,
                                   theHill);
}

// Returns the cost of a rain storm's route step: cheap along shores, dear over water, cliffs and the map edge
// (ee_app storm.rs)
long EEAIGPGModifierStormRoutePlannerRain::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    return ee_app_storm_route_cost(false, theFrom->GetSector() != inTo->GetSector(), EEStormStraight(theFrom, inTo), inTo->GetKind(),
                                   false);
}

// Returns the name of a storm type ("Rain", "Snow", "Hurricane", ...), or "Type Not defined" (ee_app storm.rs)
UWideString EEEMStorm::GetTypeName(long inType)
{
    return UWideString(ee_app_storm_type_name(inType));
}

// Creates a storm of a type for the environment manager at full intensity; any known type starts at once
EEEMStorm::EEEMStorm(long inType, EEEnvironmentManager* inManager)
    : mManager(inManager)
{
    ee_app_storm_init(this, inType);
    this->mStorm = NULL;
    this->mRequest = NULL;
    this->mName = EEEMStorm::GetTypeName(inType);
    if (inType != kStormUnknown)
        this->Begin();
}

// Destroys the storm, cancelling its route request and ending its weather effect
EEEMStorm::~EEEMStorm()
{
    this->Release();
}

// Starts the storm: picks a goal in a random player's town, a start over water (rain) or cliffs and hills (snow),
// creates the weather effect around the start and requests its route
void EEEMStorm::Begin()
{
    std::vector<U2DPoint<long> > thePath;
    std::vector<U2DPoint<long> > theVisited;
    EEAIAStarBounds theBounds;
    EEGraphicEffect* theEffect = NULL;
    EEAIBPGModifierStormShaper* theShaper = new EEAIBPGModifierStormShaper;
    EEEffectHandle theHandle;
    theHandle.mValue = 0;
    EEAIArea* theArea;
    EEPlayer* thePlayer = gWorld.mPlayers[gPhysics->GetRandom(1L, gWorld.mNumPlayers - 1)];
    if (thePlayer == NULL || thePlayer->mTowns.size() == 0
        || (theArea = thePlayer->mTowns[gPhysics->GetRandom(0L, (long)thePlayer->mTowns.size() - 1)]) == NULL) {
        theArea = gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryLand)];
        if (theArea == NULL)
            theArea = gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone)];
    }
    this->mGoal = theArea->GetRandomPoint();
    theArea = NULL;
    if (this->mType == kStormRain || this->mType == kStormHurricane) {
        theArea = gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryWater)];
    } else if (this->mType == kStormSnow || this->mType == kStormBlizzard) {
        if (gPhysics->GetRandom() <= 0.5f)
            theArea = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryCliff, gPhysics->mRandom));
        else
            theArea = dynamic_cast<EEAIHill*>(gContinentManager.GetRandomArea(kAIAreaHill, gPhysics->mRandom, 0));
    }
    if (theArea == NULL)
        theArea = gContinentManager.GetContinents()[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone)];
    this->mStart = theArea->GetRandomPoint();
    theShaper->mInner.Set(this->mStart);



    theShaper->mInner.mTop = max(theShaper->mInner.mTop - 4, 0);
    theShaper->mInner.mLeft = max(theShaper->mInner.mLeft - 4, 0);
    theShaper->mInner.mRight = min(theShaper->mInner.mRight + 4, gWorld.GetMap()->mSize);
    theShaper->mInner.mBottom = min(theShaper->mInner.mBottom + 4, gWorld.GetMap()->mSize);
    theShaper->mOuter = theShaper->mInner;



    theShaper->mOuter.mTop = max(theShaper->mOuter.mTop - 4, 0);
    theShaper->mOuter.mLeft = max(theShaper->mOuter.mLeft - 4, 0);
    theShaper->mOuter.mRight = min(theShaper->mOuter.mRight + 4, gWorld.GetMap()->mSize);
    theShaper->mOuter.mBottom = min(theShaper->mOuter.mBottom + 4, gWorld.GetMap()->mSize);
    theShaper->mMaxSteps = gPhysics->GetRandom(0x2dL, 0x87L);
    theShaper->mRain = this->mType == kStormRain || this->mType == kStormHurricane;
    theShaper->mSnow = this->mType == kStormSnow || this->mType == kStormBlizzard;
    theBounds.Set(this->mStart);
    theBounds.mTop = max(theBounds.mTop - 30, 0);
    theBounds.mLeft = max(theBounds.mLeft - 30, 0);
    theBounds.mRight = min(theBounds.mRight + 30, gWorld.GetMap()->mSize);
    theBounds.mBottom = min(theBounds.mBottom + 30, gWorld.GetMap()->mSize);
    gPathManager.FindPath(NULL, gWorld.GetMap(), this->mStart, this->mGoal, theBounds, thePath, 4, 0x6e, theShaper, &theVisited);
    if (this->mType == kStormRain || this->mType == kStormHurricane)
        theEffect = EEGraphicEffect::Get(kGraphicEffectRain);
    else if (this->mType == kStormSnow || this->mType == kStormBlizzard)
        theEffect = EEGraphicEffect::Get(kGraphicEffectSnow);
    gEffectManager->CreateWeatherEffect(theVisited, theEffect, &theHandle);
    this->mStorm = dynamic_cast<EEE3DRust*>(gEffectManager->GetEffect(theHandle));
    this->mStorm->Reference();
    this->mStorm->GetBounds(this->mArea);
    this->mStorm->mRecord->storm.sustained = true;
    theShaper->Dereference();
    this->RequestRoute();
    this->mState = 2;
}

// Creates the storm shaper with no steps and a near radius of 8 tiles
EEAIBPGModifierStormShaper::EEAIBPGModifierStormShaper()
{
    this->mSteps = 0;
    this->mMaxSteps = 0;
    this->mDistanceSquared = 0x40;
    this->mRain = false;
    this->mSnow = false;
}

// Returns whether the shaper's search should stop (no node, or the step limit reached)
bool EEAIBPGModifierStormShaper::CheckNode(EEAIAStarNode* inNode)
{
    return inNode == NULL || this->mSteps >= this->mMaxSteps;
}

// New (port): the shaper's state as ee_astar's modifiers read it
EeAStarStorm EEAIBPGModifierStormShaper::StormState() const
{
    EeAStarStorm theStorm;
    theStorm.inner = EEAIAStarRectOf(this->mInner);
    theStorm.outer = EEAIAStarRectOf(this->mOuter);
    theStorm.distance_squared = this->mDistanceSquared;
    theStorm.snow = this->mSnow;
    theStorm.rain = this->mRain;
    theStorm.start_x = this->mBoundedGoal->mStartTile.mX;
    theStorm.start_y = this->mBoundedGoal->mStartTile.mY;
    return theStorm;
}

// Returns the cost of a tile for shaping the storm: cheap on cliffs and hills (snow) or water (rain)
long EEAIBPGModifierStormShaper::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                           long inParentY)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    EeAStarStorm theStorm = this->StormState();
    return ee_astar_mod_storm_cost(&theCallbacks, &theStorm, inX, inY);
}

// the shaper's acceptance chances (unit-local .rdata after its vtable): ee_astar modifiers::STORM_*_CHANCE

// Returns whether the storm may spread onto a tile: always inside the inner box, else by a chance that falls with
// distance
bool EEAIBPGModifierStormShaper::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    EeAStarStorm theStorm = this->StormState();
    return ee_astar_mod_storm_can_enter(&theCallbacks, &theStorm, inX, inY);
}

// Returns whether the storm is over (no weather effect, or finished)
bool EEEMStorm::IsOver()
{
    return this->mStorm == NULL || this->mState == 6;
}

// Loads the storm's route, areas, weather effect handle, intensities, type, state and times; always returns 0
long EEEMStorm::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffectHandle theHandle = kNoEffectHandle;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return 0;
    this->mPath.resize(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult == kErrorOK)
            theResult = this->mPath[i].Load(inFile);
    }
    if (theResult != kErrorOK)
        return 0;
    this->mArea.Load(inFile);
    this->mStart.Load(inFile);
    this->mGoal.Load(inFile);
    this->mArea.Load(inFile);
    theResult = inFile.Read(theHandle.mValue);
    this->mStorm = reinterpret_cast<EEE3DRust*>(theHandle.mValue);
    if (theResult == kErrorOK && (theResult = inFile.Read(this->mTargetIntensity)) == 0
        && (theResult = inFile.Read(this->mIntensity)) == 0 && (theResult = inFile.Read(this->mStep)) == 0
        && (theResult = inFile.Read(this->mType)) == 0 && (theResult = inFile.Read(this->mState)) == 0
        && (theResult = inFile.Read(this->mNextMoveTime)) == 0 && (theResult = inFile.Read(this->mIsSevere)) == 0)
        inFile.Read(this->mEndTime);
    return 0;
}

// Reconnects the loaded storm to its weather effect and renames it; returns 9 when the effect is missing
long EEEMStorm::Restore()
{
    long theResult = 0;
    EEEffectHandle theHandle;
    theHandle.mValue = USavedID(this->mStorm);
    this->mStorm = dynamic_cast<EEE3DRust*>(gEffectManager->GetEffect(theHandle));
    if (this->mStorm != NULL)
        this->mStorm->Reference();
    else {
        this->mStorm = NULL;
        theResult = 9;
    }
    if (this->mPath.size() == 0 && this->mStorm != NULL)
        this->mStorm->mRecord->storm.sustained = false;
    this->mName = EEEMStorm::GetTypeName(this->mType);
    if (this->mState == 2)
        this->mState = 8;
    return theResult;
}

// Requests the storm's route from its start to its goal with the planner for its type
void EEEMStorm::RequestRoute()
{
    EECPASGFlags theFlags;
    memset(theFlags.mFlags, 1, sizeof(theFlags.mFlags));
    if (this->mRequest != NULL) {
        gPathManager.CancelAutoPathRequest(this->mRequest);
        this->mRequest->Dereference();
        this->mRequest = NULL;
    }
    EEAIGPGModifierStormRoutePlannerBase* thePlanner;
    switch (this->mType) {
    case kStormRain:
    case kStormHurricane:
        thePlanner = new EEAIGPGModifierStormRoutePlannerRain;
        break;
    default:
        thePlanner = new EEAIGPGModifierStormRoutePlannerSnow;
        break;
    }
    this->mRequest = gPathManager.RequestPath(0, this->mStart, this->mGoal, &theFlags, 1, 500, thePlanner, 0);
    if (thePlanner != NULL)
        thePlanner->Dereference();
}

// Takes the finished route (reversed, to pop from the back); the step then moves (state 3)
void EEEMStorm::TakePath()
{
    this->mPath = this->mRequest->mPath;
    std::reverse(this->mPath.begin(), this->mPath.end());
    this->mRequest->Dereference();
    this->mRequest = NULL;
}

// Writes the storm's route, areas, weather effect handle, intensities, type, state and times; always returns 0
long EEEMStorm::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mPath.size();
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return 0;
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult == 0)
            theResult = this->mPath[i].Save(outFile);
    }
    if (theResult != 0)
        return 0;
    this->mArea.Save(outFile);
    this->mStart.Save(outFile);
    this->mGoal.Save(outFile);
    this->mArea.Save(outFile);
    unsigned long theHandle = this->mStorm->mHandle.mValue;
    if ((theResult = outFile.Write(theHandle)) == 0 && (theResult = outFile.Write(this->mTargetIntensity)) == 0
        && (theResult = outFile.Write(this->mIntensity)) == 0 && (theResult = outFile.Write(this->mStep)) == 0
        && (theResult = outFile.Write(this->mType)) == 0 && (theResult = outFile.Write(this->mState)) == 0
        && (theResult = outFile.Write(this->mNextMoveTime)) == 0 && (theResult = outFile.Write(this->mIsSevere)) == 0)
        outFile.Write(this->mEndTime);
    return 0;
}

// Cancels the storm's route request and ends its weather effect
void EEEMStorm::Release()
{
    if (this->mRequest != NULL) {
        gPathManager.CancelAutoPathRequest(this->mRequest);
        this->mRequest->Dereference();
        this->mRequest = NULL;
    }
    if (this->mStorm == NULL)
        return;
    this->mStorm->mRecord->storm.sustained = false;
    this->mStorm->mRecord->storm.elapsed = this->mStorm->mRecord->storm.duration;
    this->mStorm->Release();
    this->mStorm = NULL;
}

// New (port): the step's host (ee_app storm.rs; no operation touches the storm's EeAppStormFields)
static void EEEMStormHostOp(void* inContext, int32_t inOp, EeAppStormIo* io)
{
    EEEMStorm* theStorm = static_cast<EEEMStorm*>(inContext);
    switch (inOp) {
    case EE_APP_STORM_OP_REQUEST:
        io->flag = theStorm->mRequest != NULL;
        io->done = io->flag && theStorm->mRequest->mIsDone;
        break;
    case EE_APP_STORM_OP_TAKE_PATH:
        theStorm->TakePath();
        break;
    case EE_APP_STORM_OP_REQUEST_ROUTE:
        theStorm->RequestRoute();
        break;
    case EE_APP_STORM_OP_PATH_LEN:
        io->n = (long)theStorm->mPath.size();
        break;
    case EE_APP_STORM_OP_DISTANCE:
        io->n = theStorm->mStorm->GetCenterPoint().DistanceToSquared(io->flag ? theStorm->mGoal : theStorm->mPath.back());
        break;
    case EE_APP_STORM_OP_POP_PATH:
        theStorm->mPath.pop_back();
        break;
    case EE_APP_STORM_OP_MOVE:
        theStorm->mStorm->Move(io->flag ? theStorm->mGoal : theStorm->mPath.back(), theStorm->mArea);
        break;
    case EE_APP_STORM_OP_TILE_COUNT:
        io->n = (long)theStorm->mStorm->GetTileCount();
        break;
    case EE_APP_STORM_OP_END_EFFECT:
        theStorm->mStorm->mRecord->storm.elapsed = theStorm->mStorm->mRecord->storm.duration;
        break;
    case EE_APP_STORM_OP_SET_INTENSITY:
        if (theStorm->mStorm != NULL)
            theStorm->mStorm->mRecord->storm.intensity = io->f;
        break;
    case EE_APP_STORM_OP_GAME_SPEED:
        io->f = gWorld.GetGameSpeed();
        break;
    case EE_APP_STORM_OP_RANDOM_RANGE:
        io->f = gPhysics->GetRandom(io->f, io->g);
        break;
    case EE_APP_STORM_OP_RANDOM:
        io->f = gPhysics->mRandom.Generate();
        break;
    case EE_APP_STORM_OP_RESET_COLOR:
        theStorm->mManager->mC18 = gStormColor;
        break;
    }
}

// Advances the storm: follows its route to the goal, lingers, then dies away (also at its end time) and fades its
// intensity; the target is always 0.4 (the hour is fixed at 10) (ee_app storm.rs, over the host above)
void EEEMStorm::Step(unsigned long inTime)
{
    ee_app_storm_step(this, &EEEMStormHostOp, this, inTime);
    EEAppCaptureStorm(*this, 1, inTime);   // STRM (TEST ONLY)
}
