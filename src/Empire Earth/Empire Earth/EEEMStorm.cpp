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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
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
#include "ee/EEEffect3D.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEStormModifiers.h"
#include "ee/EEStormType.h"

// FUNCTION: EXE 0x52dba8
// FUNCTION: EXE 0x52dbb2
// FUNCTION: EXE 0x52dbc3
// FUNCTION: EXE 0x52dbcf
// Constructs the storm's "Storm-Data" key at startup and destroys it at exit (wrapper, constructor, registrar,
// destructor)
UString EEEMStorm::sStormData("Storm-Data");
// FUNCTION: EXE 0x52dbeb
// FUNCTION: EXE 0x52dbf5
// FUNCTION: EXE 0x52dc06
// FUNCTION: EXE 0x52dc12
// Constructs the storm's "Environment-Data" key at startup and destroys it at exit (wrapper, constructor,
// registrar, destructor)
UString EEEMStorm::sEnvironmentData("Environment-Data");
// FUNCTION: EXE 0x52dc2e
// FUNCTION: EXE 0x52dc38
// FUNCTION: EXE 0x52dc51
// FUNCTION: EXE 0x52dc5d
// Constructs the black storm colour at startup and registers its destructor
GEColor gStormColor;                        // (read by Step: the environment manager's colour when a rain storm ends)
// FUNCTION: EXE 0x52dc5e
// FUNCTION: EXE 0x52dc68
// FUNCTION: EXE 0x52dc87
// FUNCTION: EXE 0x52dc93
// Constructs the light storm colour at startup and registers its destructor
GEColor gStormColorLight(0.679f, 0.726f, 0.8f);
// FUNCTION: EXE 0x52dc94
// FUNCTION: EXE 0x52dc9e
// FUNCTION: EXE 0x52dcbd
// FUNCTION: EXE 0x52dcc9
// Constructs the dark storm colour at startup and registers its destructor
GEColor gStormColorDark(0.273f, 0.343f, 0.457f);

// FUNCTION: EXE 0x52dcca
// Returns the cost of a snow storm's route step: cheap over cliffs and hills, dear over forest, shallows and the
// map edge
long EEAIGPGModifierStormRoutePlannerSnow::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost;
    if (theFrom->mSector != inTo->mSector)
        theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
            10 : 14;
    else
        theCost = 1;
    switch (inTo->mKind) {
    case kAITileLand:
        theCost += 2;
        if (gContinentManager.GetHillAt(inTo->mLocation) != NULL)
            theCost = 1;
        break;
    case kAITileCliff:
        theCost = 1;
        break;
    case kAITileForest:
    case kAITileShallows:
        theCost += 20;
        break;
    case kAITileMapEdge:
        theCost += 500;
        break;
    case kAITileWall:
    case kAITileGate:
        theCost += 5;
        if (gContinentManager.GetHillAt(inTo->mLocation) != NULL)
            theCost = 1;
        break;
    default:
        theCost += 100;
        break;
    }
    return theCost;
}

// FUNCTION: EXE 0x52dd54
// Returns the cost of a rain storm's route step: cheap along shores, dear over water, cliffs and the map edge
long EEAIGPGModifierStormRoutePlannerRain::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost;
    if (theFrom->mSector != inTo->mSector)
        theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
            10 : 14;
    else
        theCost = 1;
    switch (inTo->mKind) {
    case kAITileWater:
    case kAITileShallows:
        theCost += 20;
        break;
    case kAITileCliff:
        theCost += 20;
        break;
    case kAITileMapEdge:
    case kAITileSpace:
        theCost += 500;
        break;
    case kAITileShallowsShore:
    case kAITileShallowsNearShore:
    case kAITileShore:
    case kAITileNearShore:
        theCost = 1;
        break;
    default:
        theCost += 100;
        break;
    }
    return theCost;
}

// FUNCTION: EXE 0x52ddbf
// Returns the name of a storm type ("Rain", "Snow", "Hurricane", ...), or "Type Not defined"
UWideString EEEMStorm::GetTypeName(long inType)
{
    UWideString theName;
    switch (inType) {
    case kStormUnknown:
        theName += "Unknown";
        break;
    case kStormRain:
        theName += "Rain";
        break;
    case kStormSnow:
        theName += "Snow";
        break;
    case kStormHurricane:
        theName += "Hurricane";
        break;
    case kStormBlizzard:
        theName += "Blizzard";
        break;
    case kStormPlaceHolder1:
        theName += "PlaceHolder1";
        break;
    case kStormPlaceHolder2:
        theName += "PlaceHolder2";
        break;
    case kStormPlaceHolder3:
        theName += "PlaceHolder3";
        break;
    case kStormPlaceHolder4:
        theName += "PlaceHolder4";
        break;
    default:
        theName = "Type Not defined";
        break;
    }
    return theName;
}

// FUNCTION: EXE 0x52de72
// Creates a storm of a type for the environment manager at full intensity; any known type starts at once
EEEMStorm::EEEMStorm(long inType, EEEnvironmentManager* inManager)
    : mManager(inManager), mTargetIntensity(1.0f), mIntensity(1.0f)
{
    this->mStorm = NULL;
    this->mRequest = NULL;
    this->mStep = 0.02f;
    this->mType = inType;
    this->mState = 1;
    this->mNextMoveTime = 0;
    this->mIsSevere = inType == kStormHurricane || inType == kStormBlizzard;
    this->mEndTime = 0xffffffff;
    this->mName = EEEMStorm::GetTypeName(inType);
    if (inType != kStormUnknown)
        this->Begin();
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x52df26

// FUNCTION: EXE 0x52df42
// Destroys the storm, cancelling its route request and ending its weather effect
EEEMStorm::~EEEMStorm()
{
    this->Release();
}

// FUNCTION: EXE 0x52df63
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
        theArea = gContinentManager.mContinents[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryLand)];
        if (theArea == NULL)
            theArea = gContinentManager.mContinents[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone)];
    }
    this->mGoal = theArea->GetRandomPoint();
    theArea = NULL;
    if (this->mType == kStormRain || this->mType == kStormHurricane) {
        theArea = gContinentManager.mContinents[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryWater)];
    } else if (this->mType == kStormSnow || this->mType == kStormBlizzard) {
        if (gPhysics->GetRandom() <= 0.5f)
            theArea = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryCliff, gPhysics->mRandom));
        else
            theArea = dynamic_cast<EEAIHill*>(gContinentManager.GetRandomArea(kAIAreaHill, gPhysics->mRandom, 0));
    }
    if (theArea == NULL)
        theArea = gContinentManager.mContinents[(unsigned short)gContinentManager.GetLargestContinent(kContinentCategoryNone)];
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
    this->mStorm = dynamic_cast<EEE3DStorm*>(gEffectManager->GetEffect(theHandle));
    this->mStorm->Reference();
    this->mStorm->GetBounds(this->mArea);
    this->mStorm->mIsSustained = true;
    theShaper->Dereference();
    this->RequestRoute();
}

// FUNCTION: EXE 0x52e395
// Creates the storm shaper with no steps and a near radius of 8 tiles
EEAIBPGModifierStormShaper::EEAIBPGModifierStormShaper()
{
    this->mSteps = 0;
    this->mMaxSteps = 0;
    this->mDistanceSquared = 0x40;
    this->mRain = false;
    this->mSnow = false;
}

// FUNCTION: EXE 0x52e3d8
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

// FUNCTION: EXE 0x52e3ee
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

// FUNCTION: EXE 0x52e48b
// Returns whether the storm may spread onto a tile: always inside the inner box, else by a chance that falls with
// distance
bool EEAIBPGModifierStormShaper::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    EeAStarStorm theStorm = this->StormState();
    return ee_astar_mod_storm_can_enter(&theCallbacks, &theStorm, inX, inY);
}

// FUNCTION: EXE 0x52e523
// Raises the storm's intensity one step toward the target (from nothing straight to 0.15)
void EEEMStorm::Grow()
{
    float theIntensity = (this->mIntensity != 0.0f ? this->mStep * this->mIntensity : 0.15f) + this->mIntensity;
    if (theIntensity > this->mTargetIntensity)
        theIntensity = this->mTargetIntensity;
    this->SetIntensity(theIntensity);
}

// FUNCTION: EXE 0x52e559
// Lowers the storm's intensity one step toward the target, ending it at 0.005 or below
void EEEMStorm::Shrink()
{
    float theIntensity = this->mIntensity - this->mStep * this->mIntensity;
    if (theIntensity < this->mTargetIntensity)
        theIntensity = this->mTargetIntensity;
    if (theIntensity <= 0.005f)
        theIntensity = 0.0f;
    this->SetIntensity(theIntensity);
}

// FUNCTION: EXE 0x52e588
// Returns whether the storm is over (no weather effect, or finished)
bool EEEMStorm::IsOver()
{
    return this->mStorm == NULL || this->mState == 6;
}

// FUNCTION: EXE 0x52e599
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
    this->mStorm = reinterpret_cast<EEE3DStorm*>(theHandle.mValue);
    if (theResult == kErrorOK && (theResult = inFile.Read(this->mTargetIntensity)) == 0
        && (theResult = inFile.Read(this->mIntensity)) == 0 && (theResult = inFile.Read(this->mStep)) == 0
        && (theResult = inFile.Read(this->mType)) == 0 && (theResult = inFile.Read(this->mState)) == 0
        && (theResult = inFile.Read(this->mNextMoveTime)) == 0 && (theResult = inFile.Read(this->mIsSevere)) == 0)
        inFile.Read(this->mEndTime);
    return 0;
}

// FUNCTION: EXE 0x52e6b5
// Reconnects the loaded storm to its weather effect and renames it; returns 9 when the effect is missing
long EEEMStorm::Restore()
{
    long theResult = 0;
    EEEffectHandle theHandle;
    theHandle.mValue = USavedID(this->mStorm);
    this->mStorm = dynamic_cast<EEE3DStorm*>(gEffectManager->GetEffect(theHandle));
    if (this->mStorm != NULL)
        this->mStorm->Reference();
    else {
        this->mStorm = NULL;
        theResult = 9;
    }
    if (this->mPath.size() == 0 && this->mStorm != NULL)
        this->mStorm->mIsSustained = false;
    this->mName = EEEMStorm::GetTypeName(this->mType);
    if (this->mState == 2)
        this->mState = 8;
    return theResult;
}

// FUNCTION: EXE 0x52e74e
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
    this->mState = 2;
    if (thePlanner != NULL)
        thePlanner->Dereference();
}

// FUNCTION: EXE 0x52e817
// Takes the finished route (reversed, to pop from the back) and starts moving
void EEEMStorm::TakePath()
{
    this->mPath = this->mRequest->mPath;
    this->mState = 3;
    std::reverse(this->mPath.begin(), this->mPath.end());
    this->mRequest->Dereference();
    this->mRequest = NULL;
}

// FUNCTION: EXE 0x52e876
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

// FUNCTION: EXE 0x52e9a2
// Sets the storm's intensity, also on its weather effect
void EEEMStorm::SetIntensity(float inIntensity)
{
    this->mIntensity = inIntensity;
    if (this->mStorm != NULL)
        this->mStorm->mIntensity = inIntensity;
}

// FUNCTION: EXE 0x52e9bd
// Sets the storm's target intensity, clamped to 0..2
void EEEMStorm::SetTargetIntensity(float inIntensity)
{
    if (inIntensity <= 2.0f) {
        if (inIntensity < 0.0f)
            inIntensity = 0.0f;
    } else
        inIntensity = 2.0f;
    this->mTargetIntensity = inIntensity;
}

// FUNCTION: EXE 0x52e9eb
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
    this->mStorm->mIsSustained = false;
    this->mStorm->mElapsedTime = this->mStorm->mDuration;
    this->mStorm->Release();
    this->mStorm = NULL;
}

// the storm's day (unit-local .rdata before EEEMStorm's vtable) and its longest stay (after the planners' vtables)
static const float kStormNoon = 12.0f;
static const float kStormDay = 24.0f;
static const float kStormMaximumStay = 45000.0f;

// FUNCTION: EXE 0x52ea48
// Advances the storm: follows its route to the goal, lingers, then dies away (also at its end time) and fades its
// intensity; the target is always 0.4 (the hour is fixed at 10)
void EEEMStorm::Step(unsigned long inTime)
{
    float theHour = 10.0f;
    float theIntensity = 0.4f;
    if (theHour > kStormNoon) {
        if (theHour < kStormDay) {
            float theFraction = theHour / kStormDay;
            theIntensity = (this->mType == kStormBlizzard || this->mType == kStormHurricane ? 2.0f : 1.0f) * theFraction;
        } else
            theIntensity = this->mType == kStormBlizzard || this->mType == kStormHurricane ? 2.0f : 1.0f;
    }
    switch (this->mState) {
    case 2:
        if (this->mRequest != NULL) {
            if (this->mRequest->mIsDone)
                this->TakePath();
        } else
            this->mState = 3;
        break;
    case 3:
        if (this->mNextMoveTime <= inTime) {
            if (this->mPath.size() != 0) {
                if (this->mStorm->GetCenterPoint().DistanceToSquared(this->mPath.back()) <= 9)
                    this->mPath.pop_back();
                if (this->mPath.size() != 0)
                    this->mStorm->Move(this->mPath.back(), this->mArea);
                else
                    this->mStorm->Move(this->mGoal, this->mArea);
            }
            if (this->mStorm->GetCenterPoint().DistanceToSquared(this->mGoal) <= 9) {
                this->mState = 7;
                this->mNextMoveTime = UMFloatToLong(gWorld.GetGameSpeed() * gPhysics->GetRandom(15000.0f, kStormMaximumStay))
                          + inTime;
            } else {
                if (gPhysics->mRandom.Generate() <= 0.25f)
                    this->mState = 4;
                this->mNextMoveTime = UMFloatToLong(gWorld.GetGameSpeed() * 15000.0f) + inTime;
            }
        }
        if (this->mStorm->GetTileCount() < 45)
            this->mState = 5;
        break;
    case 4:
        if (gPhysics->mRandom.Generate() <= 0.4f)
            this->mState = 3;
        this->mNextMoveTime = UMFloatToLong(gWorld.GetGameSpeed() * 15000.0f) + inTime;
        break;
    case 5:
        theIntensity = 0.0f;
        this->mState = 6;
        break;
    case 6:
        theIntensity = 0.0f;
        this->mStorm->mElapsedTime = this->mStorm->mDuration;
        break;
    case 7:
        if (this->mNextMoveTime <= inTime)
            this->mState = 5;
        break;
    case 8:
        this->RequestRoute();
        this->mState = 2;
        break;
    }
    if (this->mEndTime <= inTime) {
        if (this->mType == kStormRain || this->mType == kStormHurricane)
            this->mManager->mC18 = gStormColor;
        this->mState = 5;
    }
    this->SetTargetIntensity(theIntensity);
    if (this->mIntensity < this->mTargetIntensity)
        this->Grow();
    else if (this->mIntensity > this->mTargetIntensity)
        this->Shrink();
}
