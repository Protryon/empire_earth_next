// C:\EEX\Empire Earth\Player\EEPlayer.cpp (__FILE__ 0x165cfa). The player: 0x161825..0x16d346 (the class static keys'
// init chains first, then EEWUnitsOffMapInfo, EEPlayer in definition order, the selected-group and functor helpers it
// defines, and its template COMDATs). Unit flags: the exe's module set. Data: the vtable 0x4429e0.
#include <windows.h>
#include "platform/PlatSim.h"
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEUnitKind.h"
#include "ee/EETileLocation.h"
#include "lle/FSFile.h"
#include "lle/FSFileSpec.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEGameLog.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEWUnitsOffMapInfo.h"
#include "ee/EEButtonObject.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/UResourceFile.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitClassImprovement.h"
#include "lle/NEMessage.h"
#include "ee/EESavedGroup.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEHerd.h"
#include "ee/EEVectorIO.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EETeam.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEGameVariant.h"           // the wonder powers' hit point factors (EEPlayerHost)
#include "ee/EEUnitUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "lle/SESoundManager.h"
#include "ee/EEUGRust.h"                // the towns' patrol goals (EEPlayerHost)
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // ee_screens_hot_key_unit_selection: the docks' hot keys (ee_screens hot_keys.rs)
#include "lle/U2DRectangle.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAnimal.h"
#include "ee/EEStringIDs.h"
#include "lle/ISKeyCode.h"
// EEUnitGoal.cpp's goal counter (0x964480; Save and Load write and read it: it is not file-static in the original)
// (sGoalCounter: ee/EmpireEarth.h)
// the main game form's blinking buttons (0x27fcf0, cdecl; EEPlayer::RepairPointers keeps its result, so not void as
// EEPhysics.cpp declares it; inferred)
// (EESetCivBuilderButtonBlinking: ee/EmpireEarth.h)

const UString EEPlayer::sClassObjectType("EEPlayer-ClassObjectType");
const UString EEPlayer::sUnitIsComplex("EEPlayer-UnitIsComplex");
const UString EEPlayer::sNumResources("EEPlayer-NumResources");
const UString EEPlayer::sResource("EEPlayer-Resource");
const UString EEPlayer::sPopLimit("EEPlayer-PopLimit");
const UString EEPlayer::sPatrolGroups("EEPlayer-PatrolGroups");
const UString EEPlayer::sHaveTechTree("EEPlayer-HaveTechTree");
const UString EEPlayer::sDiplomaticStance("EEPlayer-DiplomaticStance");
const UString EEPlayer::sStatistics("EEPlayer-Statistics");
const UString EEPlayer::sPopulationCount("EEPlayer-PopulationCount");
const UString EEPlayer::sEndOfGameCount("EEPlayer-EndOfGameCount");
const UString EEPlayer::sObserverMode("EEPlayer-ObserverMode");
const UString EEPlayer::sGeneric("EEPlayer-Generic");
const UString EEPlayer::sSharedLOSFlag("EEPlayer-SharedLOSFlag");
const UString EEPlayer::sSendChatMessage("EEPlayer-SendChatMessage");
const UString EEPlayer::sSendChatMessageType("EEPlayer-SendChatMessageType");

// Deletes each element of a vector of pointers, then clears it; returns the count (`mov eax, ebx` before ret)
// (cdecl template COMDATs 0x16d269.. after the template tail, first used by the dtor) (provisional name)
template <class V> long EEDeletePointers(V& ioVector)
{
    long i = 0;
    for (V::iterator theIterator = ioVector.begin(); theIterator != ioVector.end(); i++, theIterator++)
        delete *theIterator;
    ioVector.clear();
    return i;
}

// std::for_each's functor in the dtor: dereferences each patrol group of mPatrolGroups (0x16d2f9) (provisional name)
struct EEPDereferencer {
    void operator()(EEPPatrolGroup* inGroup) { inGroup->Dereference(); }
};

// the interval of the healing units' hit-point bonus (0x4429b8, just before kTributeFee; ComputeLoadDistance)
// (provisional name)
static const float kPlayerHealInterval = EE_UNITS_PLAYER_HEAL_INTERVAL;   // (ee_units player_tick.rs)

// the effects every player loads (0x4429c0) (provisional name)
static const unsigned long sPlayerEffects[3] = {271, 274, 275};

// TEST ONLY: the player capture (tools/sim_capture.py; the player lane, round 1). PLST: each player's state at every
// UpdatePlayer (the statistics but series 0x14 and 0x15, which the local player's reports set by the wall clock, the
// events, the queued improvements and the plain members as FNV-1a 64 digests, then the resources and counts); PLEC: the economy and improvement decisions (the outermost call on the thread that runs
// UpdatePlayer: its kind, player, arguments, result, the reason's length, the resources and population after)
#include "ee/EESimCapture.h"
#include "ee/EETechCapture.h"
#include "ee/EEStatisticsHistory.h"
#include "ee/EEStatisticsRecord.h"
#include "platform/PlatThread.h"
#include <cstring>
#include <initializer_list>

static uint32_t sPlayerCaptureThread = 0;
static thread_local int sPlayerCaptureDepth = 0;

struct EEPlayerDigest {
    uint64_t mHash = 0xcbf29ce484222325ull;
    void Bytes(const void* inData, size_t inSize)
    {
        const unsigned char* theBytes = (const unsigned char*)inData;
        for (size_t i = 0; i < inSize; i++)
            this->mHash = (this->mHash ^ theBytes[i]) * 0x100000001b3ull;
    }
    void U32(uint32_t inValue) { this->Bytes(&inValue, 4); }
    void F32(float inValue) { this->Bytes(&inValue, 4); }
    void Into(EESimCaptureRecord& ioRecord) { ioRecord.U32((uint32_t)this->mHash); ioRecord.U32((uint32_t)(this->mHash >> 32)); }
};

static int32_t EEPlayerFloatBits(float inValue)
{
    int32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static int32_t EEPlayerNodeID(EETechTreeNode* inNode)
{
    return inNode != NULL && inNode->mData != NULL ? (int32_t)inNode->mData->mID : -1;
}

static void EEPlayerCaptureState(EEPlayer* inPlayer)
{
    if (!EESimCaptureRecord::Active())
        return;
    sPlayerCaptureThread = PlatCurrentThreadId();
    EESimCaptureRecord theRecord(EECaptureTag("PLST"));
    theRecord.I32((int32_t)inPlayer->mPlayerID);
    theRecord.U32(gPhysics != NULL ? (uint32_t)gPhysics->mGameTime : 0);
    EEPlayerDigest theStats;
    theStats.U32((uint32_t)inPlayer->mSavedGroups.mEndTime);
    for (unsigned long s = 0; s < 31; s++) {
        if (s == 0x14 || s == 0x15)
            continue;   // the local player's reports (EEPhysics::ReportStatistics: by the wall clock)
        EEStatisticsSeries theSeries = inPlayer->mSavedGroups.GetSeries(s);
        theStats.U32((uint32_t)theSeries.mMaxValue);
        theStats.U32(theSeries.mKeepHistory ? 1 : 0);
        theStats.U32((uint32_t)theSeries.mPoints.size());
        for (size_t p = 0; p < theSeries.mPoints.size(); p++) {
            theStats.U32((uint32_t)theSeries.mPoints[p].mValue);
            theStats.U32((uint32_t)theSeries.mPoints[p].mTime);
        }
    }
    theStats.Into(theRecord);
    EEPlayerDigest theEvents;
    theEvents.mHash = ee_units_events_digest(inPlayer->mEvents.mList);
    theEvents.Into(theRecord);
    EEPlayerDigest theQueued;
    theQueued.mHash = ee_units_queued_digest(inPlayer->mQueued);
    theQueued.Into(theRecord);
    EEPlayerDigest theFields;
    theFields.U32((uint32_t)inPlayer->mHackUnitClass);
    theFields.F32(inPlayer->mConversionFactor);
    theFields.F32(inPlayer->mTributeFee);
    theFields.U32((uint32_t)inPlayer->mMaxStartingCitizens);
    for (long d = 0; d < 17; d++)
        theFields.U32((uint32_t)inPlayer->mDamageFrom[d]);
    long theLongs[] = {inPlayer->mNumAttacks, inPlayer->mHeroLimit, inPlayer->mMiningLimit, inPlayer->mLAF0, inPlayer->mShareBonus,
                       inPlayer->mPopulationLimitBonus, inPlayer->mNumHeroes, inPlayer->mNumHeroesInProduction, inPlayer->mNumTeleporters};
    for (long l = 0; l < 9; l++)
        theFields.U32((uint32_t)theLongs[l]);
    bool theBools[] = {inPlayer->mAreaConversion, inPlayer->mBB42, inPlayer->mBB44, inPlayer->mInstantBuild, inPlayer->mHealsUnits,
                       inPlayer->mBehindFirewall, inPlayer->mEarthquakeShaken, inPlayer->mShiftMode, inPlayer->mJITManufacturing};
    for (long b = 0; b < 9; b++)
        theFields.U32(theBools[b] ? 1 : 0);
    theFields.Into(theRecord);
    for (long r = 2; r < 7; r++)   // (0 and 1 are never set but by Reset, to 0: kept out as recorded)
        theRecord.I32((int32_t)inPlayer->mResourceCaps[r]);
    theRecord.I32((int32_t)inPlayer->mPopulationCount);
    theRecord.I32((int32_t)inPlayer->mEndOfGameCount);
    theRecord.I32((int32_t)inPlayer->mNumVitalBuildings);
    theRecord.I32((int32_t)inPlayer->mNumCapitols);
    theRecord.I32((int32_t)inPlayer->mPopLimit);
    theRecord.Commit();
}

// one decision's scope: the outermost on the simulation thread records it
struct EEPlayerEconScope {
    bool mOuter;
    EEPlayerEconScope() : mOuter(sPlayerCaptureDepth++ == 0) {}
    ~EEPlayerEconScope() { sPlayerCaptureDepth--; }
    template <class R> R Record(const EEPlayer* inPlayer, int32_t inKind, R inResult, std::initializer_list<int64_t> inArgs,
                                const UWideString* inText = NULL)
    {
        if (!this->mOuter || !EESimCaptureRecord::Active() || sPlayerCaptureThread == 0
            || PlatCurrentThreadId() != sPlayerCaptureThread)
            return inResult;
        EESimCaptureRecord theRecord(EECaptureTag("PLEC"));
        theRecord.I32(inKind);
        theRecord.I32((int32_t)inPlayer->mPlayerID);
        for (std::initializer_list<int64_t>::const_iterator a = inArgs.begin(); a != inArgs.end(); ++a)
            theRecord.I32((int32_t)*a);
        theRecord.I32((int32_t)inResult);
        theRecord.I32(inText != NULL ? (int32_t)inText->GetLength() : -1);
        for (long r = 2; r < 7; r++)
            theRecord.I32((int32_t)inPlayer->mResourceCaps[r]);
        theRecord.I32((int32_t)inPlayer->mPopulationCount);
        theRecord.Commit();
        return inResult;
    }
};

// Creates an off-map record with no unit, time or reason
EEWUnitsOffMapInfo::EEWUnitsOffMapInfo()
{
    this->mUnit = NULL;
    this->mTime = 0;
    this->mReason = 0;
}

// Destroys the off-map record, releasing its unit
EEWUnitsOffMapInfo::~EEWUnitsOffMapInfo()
{
    this->ReleaseUnit();
}

// Reads the off-map unit's ID, location, time and reason from the saved game; returns the first read error
long EEWUnitsOffMapInfo::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID = (unsigned long)-1;
    long theResult = inFile.Read(theUnitID);
    this->mUnit = reinterpret_cast<RTSUnit*>(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mLocation.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mReason);
    return theResult;
}

// Releases the reference to the off-map unit and clears it
void EEWUnitsOffMapInfo::ReleaseUnit()
{
    if (this->mUnit != NULL) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
}

// Turns the loaded unit ID back into a referenced unit (NULL when it no longer exists); always succeeds
long EEWUnitsOffMapInfo::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    RTSUnit* theUnit = gWorld.GetUnit(USavedID(this->mUnit));
    if (theUnit != NULL) {
        this->mUnit = theUnit;
        theUnit->Reference();
    } else
        this->mUnit = NULL;
    return kErrorOK;
}

// Writes the off-map unit's ID (-1 for none), location, time and reason to the saved game;
// returns the first write error
long EEWUnitsOffMapInfo::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theUnitID = this->mUnit != NULL ? this->mUnit->GetUnitID() : (unsigned long)-1;
    long theResult = outFile.Write(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mLocation.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mReason);
    return theResult;
}

// Replaces the off-map unit with a new one, referencing it and releasing the old one
void EEWUnitsOffMapInfo::SetUnit(RTSUnit* inUnit)
{
    if (inUnit == this->mUnit)
        return;
    this->ReleaseUnit();
    if (inUnit != NULL) {
        this->mUnit = inUnit;
        inUnit->Reference();
    }
}

// Creates a player with a name and ID: resets all state, binds its tech tree to it and creates
// its wonder and construction-site lists
EEPlayer::EEPlayer(const UWideString& inName, unsigned long inPlayerID)
    : RTSPlayer(inName, inPlayerID)
{
    this->mViewedUnit = NULL;
    this->mSelectedViewedUnit = NULL;
    this->mQueued = ee_units_queued_new();
    this->mWonderState = ee_units_wonders_new();
    this->Reset();
    this->mTechnologies->SetPlayerIndex(inPlayerID);
    this->mTechnologies->SetPlayer(this);
    this->mComputerPlayer = NULL;
    this->mHumanPlayer = NULL;
    this->mWonders = new UReferenceContainer<EEComplexUnit*>;
    this->mConstructionSites = new UReferenceContainer<EEComplexUnit*>;
}

// Creates an unnamed player: resets all state and creates its wonder and construction-site lists
EEPlayer::EEPlayer()
{
    this->mViewedUnit = NULL;
    this->mSelectedViewedUnit = NULL;
    this->mQueued = ee_units_queued_new();
    this->mWonderState = ee_units_wonders_new();
    this->Reset();
    this->mComputerPlayer = NULL;
    this->mHumanPlayer = NULL;
    this->mWonders = new UReferenceContainer<EEComplexUnit*>;
    this->mConstructionSites = new UReferenceContainer<EEComplexUnit*>;
}

// Destroys the player: clears and releases every unit group, its media, tech tree, air traffic, upgrades,
// towns, herds, off-map records, patrol groups and wonder/construction-site lists
EEPlayer::~EEPlayer()
{
    unsigned long theCount = this->mRunningGroups.size();
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        this->mRunningGroups[i]->Clear();
        this->mRunningGroups[i]->Dereference();
    }
    this->mEvents.Clear(NULL);
    theCount = this->mFinishedGroups.size();
    for (i = 0; i < theCount; i++) {
        this->mFinishedGroups[i]->Clear();
        this->mFinishedGroups[i]->Dereference();
    }
    this->mActiveGroup->Clear();
    this->mActiveGroup->Dereference();
    this->mSelectedGroup->Clear();
    this->mSelectedGroup->Dereference();
    this->mL9B4->Clear();
    this->mL9B4->Dereference();
    for (long theGroup = 0; theGroup < 20; theGroup++) {
        this->mGroups[theGroup]->Clear();
        this->mGroups[theGroup]->Dereference();
    }
    UReferenceContainer<EEComplexUnit*>* theContainer = this->mConstructionSites;
    theContainer->Clear();
    theContainer->mItems.clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    if (this->mSelectedViewedUnit != NULL) {
        this->mSelectedViewedUnit->Dereference();
        this->mSelectedViewedUnit = NULL;
    }
    gDoppelgangerManager.UnregisterPlayer(this);
    this->UnloadAllMedia();
    delete this->mTechnologies;
    this->mTechnologies = NULL;
    for (long theAirport = 0; theAirport < 4; theAirport++) {
        delete mAirTraffic[theAirport];
        mAirTraffic[theAirport] = NULL;
    }
    for (unsigned long theClass = 0; theClass < EEUnitClassCount(); theClass++)
        delete this->mUnitClassUpgrades.begin()[theClass];
    this->mUnitClassUpgrades.clear();
    EEDeletePointers(this->mTowns);
    this->mTownsByID.clear();
    this->ClearUnitsToRepatrol();
    EEDeletePointers(this->mHerds);
    EEDeletePointers(this->mUnitsOffMap);
    std::for_each(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), EEPDereferencer());
    this->mPatrolGroups.clear();
    delete this->mWonders;
    delete this->mConstructionSites;
    this->UnloadPlayerEffects();
    ee_units_queued_free(this->mQueued);
    ee_units_wonders_free(this->mWonderState);
}

// Destroys the civilization (members only)
EECivilization::~EECivilization()
{
}

// a unit as the Rust player's operations take it: its RTSUnit address (0: none)
static int64_t EEPlayerUnitArg(RTSUnit* inUnit)
{
    return (int64_t)reinterpret_cast<uintptr_t>(inUnit);
}

static EEComplexUnit* EEPlayerUnitAt(int64_t inUnit)
{
    return static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(inUnit));
}

// port (the player lane, round 1): the economy and the improvements are Rust (ee_units player.rs) over this host; the
// player's members after the saved record are the Rust record EeUnitsPlayerFields, the queued improvements its
// queue (mQueued)
static int64_t EEPlayerHost(void* inPlayer, int32_t inOp, EeUnitsPlayerIo* io)
{
    EEPlayer* thePlayer = static_cast<EEPlayer*>(inPlayer);
    int64_t* a = io->i;
    EETechTreeNode* theNode = reinterpret_cast<EETechTreeNode*>(a[0]);
    EEUnitClass* theClass = (uint64_t)a[0] < gUnitClassTable.size() ? gUnitClassTable[(size_t)a[0]] : NULL;
    EEUnit* theUnit = static_cast<EEUnit*>(reinterpret_cast<RTSUnit*>(a[0]));   // the units' operations: an RTSUnit address
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(a[0]));
    EEPlayer* theOther = a[0] != 0 ? reinterpret_cast<EEPlayer*>(a[0]) : thePlayer;   // the diplomacy's: 0 this player
    switch (inOp) {
    case EE_UNITS_PLAYER_NODE: return (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mTechnologies->Get((long)a[0]));
    case EE_UNITS_PLAYER_NODE_INFO: {
        RTSTechTreeNodeData* d = theNode->mData;
        int64_t theInfo[8] = {d->mMakesUnit, d->mUnitClass, d->mCostResource, d->mRequiredStatistic, d->mRequiredStatisticValue,
                              d->mKind, (int64_t)theNode->GetNumUnitClasses(), theNode->mIsResearched};
        memcpy(a, theInfo, sizeof theInfo);
        return 0;
    }
    case EE_UNITS_PLAYER_NODE_AVAILABLE: return theNode->GetIsAvailable();
    case EE_UNITS_PLAYER_NODE_UNIT_CLASS: return theNode->GetUnitClass((unsigned long)a[1]);
    case EE_UNITS_PLAYER_NODE_COST: return theNode->GetCost((long)a[1], (long)a[2], 1.0f);
    case EE_UNITS_PLAYER_SCALE_COST: return thePlayer->ScaleCost((long)a[0]);
    case EE_UNITS_PLAYER_METALLURGY: return gCivPowersManager.HasPower(thePlayer, kCivPowerMetallurgy);
    case EE_UNITS_PLAYER_TAKE: thePlayer->TakeResource((long)a[0], (long)a[1]); return 0;
    case EE_UNITS_PLAYER_ADD: thePlayer->AddResource((long)a[0], (long)a[1]); return 0;
    case EE_UNITS_PLAYER_CLASS: {
        if (theClass == NULL)
            return 0;
        int64_t theInfo[6] = {theClass->mTechnology, theClass->mWeight, theClass->GetPopulationCost(), theClass->mUnitType == kUnitTypeTeleporter,
                              theClass->mAIKind == kAIKindHero, theClass->mImprovements != NULL};
        memcpy(a, theInfo, sizeof theInfo);
        return 1;
    }
    case EE_UNITS_PLAYER_EXTRA_POPULATION: return thePlayer->GetExtraPopulation((long)a[0]);
    case EE_UNITS_PLAYER_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_UNITS_PLAYER_HERO_COUNT: return thePlayer->GetHeroCount(a[0] != 0, a[1] != 0);
    case EE_UNITS_PLAYER_SCENARIO: return gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_PLAYER_CAN_AFFORD: return thePlayer->CanAfford(theNode, (long)a[1], (long)a[2]);
    case EE_UNITS_PLAYER_REASON: {
        static const long kReasons[7] = {0, IDS_YOU_HAVE_ALREADY_BUILT_MAXIMUM, IDS_YOU_HAVE_REACHED_YOUR_POP_LIMIT, IDS_UNIT_EXCEEDS_POP_LIMIT,
                                         IDS_NOT_ENOUGH_RESOURCES_FOR_CONSTRUCTION, IDS_NOT_ENOUGH_RESOURCES_TO_RESEARCH_THIS_TECHNOLOGY,
                                         IDS_MAXIMUM_NUMBER_OF_IMPROVEMENTS_HAVE_BEEN};
        gResource->GetString(kReasons[a[1]], *reinterpret_cast<UWideString*>(a[0]));
        return 0;
    }
    case EE_UNITS_PLAYER_STATS_LOWER: thePlayer->mSavedGroups.Lower((long)a[0], (long)a[1]); return 0;
    case EE_UNITS_PLAYER_IMPROVEMENT:
    case EE_UNITS_PLAYER_IMPROVEMENT_COST:
    case EE_UNITS_PLAYER_LEVEL: {
        EEUnitClassImprovementTable* theTable = theClass != NULL ? theClass->mImprovements : NULL;
        EEUnitClassUpgrade* theUpgrade = theClass != NULL ? thePlayer->mUnitClassUpgrades[(size_t)a[0]] : NULL;
        long theImprovement = (long)a[1];
        if (inOp == EE_UNITS_PLAYER_IMPROVEMENT) {
            io->f[0] = theImprovement == 12 || theTable == NULL ? 0.0f : theTable->mEntries[theImprovement].mStepFactor;
            io->f[1] = theTable != NULL ? theTable->mEntries[theImprovement].mCostFactor : 0.0f;
        }
        if (theUpgrade == NULL)
            return 0;
        EEUnitClassImprovement& theUp = theUpgrade->mImprovement;
        if (inOp == EE_UNITS_PLAYER_IMPROVEMENT_COST)
            return theUp.GetCost(theImprovement);
        a[0] = theUp.GetLevel(theImprovement);
        if (inOp == EE_UNITS_PLAYER_LEVEL)
            return 1;
        int64_t theInfo[6] = {theUp.CanUpgrade(theUpgrade->mUnitClass, theImprovement) == true, theUp.GetCost(theImprovement), theUp.mTotalCost,
                              theUp.GetLevel(theImprovement), theTable != NULL ? theTable->GetMaxLevel() : 0,
                              gWorldTable.GetLongValue(kWorldUnitImprovementPointsMax)};
        memcpy(a, theInfo, sizeof theInfo);
        return 1;
    }
    case EE_UNITS_PLAYER_IMPROVEMENT_TIME:
        return a[1] ? (long)gWorldTable.GetLongValue2(EEWorldImprovementTime(a[0])) : (long)gWorldTable.GetLongValue(EEWorldImprovementTime(a[0]));
    case EE_UNITS_PLAYER_IS_LOCAL: return thePlayer == gWorld.GetCurrentPlayer();
    case EE_UNITS_PLAYER_UPDATE_JIT: gUserInterface->UpdateJITManufacturing(); return 0;
    case EE_UNITS_PLAYER_PAY_IMPROVEMENT: thePlayer->PayImprovement(theClass, (long)a[1]); return 0;
    // the unit bookkeeping and the wonders (ee_units player_units.rs): the units as RTSUnit addresses
    case EE_UNITS_PLAYER_UNIT_INFO: {
        EEUnitClass* c = theUnit->mUnitClass;
        EEComplexUnit* x = theUnit->GetComplexUnit();
        EeUnitsPlayerUnit& u = *reinterpret_cast<EeUnitsPlayerUnit*>(a[1]);
        u.complex = EEPlayerUnitArg(x);
        u.unit_type = c->mUnitType;
        u.technology = c->mTechnology;
        u.population = c->GetPopulationCost();
        u.category = c->mCategory;
        u.ai_kind = c->mAIKind;
        u.wonder_bonus = c->mWonderBonus;
        u.wonder_unit_set = c->mWonderUnitSet;
        u.land_building = c->IsLandBuilding();
        u.vital = c->mIsVital;
        u.inactive = c->mIsInactive;
        u.blocks_tile = c->mBlocksTile;
        u.wall = c->mIsWall;
        u.tree = c->mIsTree;
        u.on_tile = theUnit->mTileNode != NULL;
        u.built = x != NULL && x->mIsBuilt;
        u.first_part = x != NULL && x->mPreviousPart == NULL;
        u.hit_points = theUnit->mHitPoints;
        u.tile_x = theUnit->mTileX;
        u.resource_amount = theUnit->mResourceAmount;
        u.container_category = x != NULL && x->mContainer != NULL ? (long)x->mContainer->mUnitClass->mCategory : -1;
        u.current_research = x != NULL ? x->mCurrentResearch : 0;
        u.build_queue = x != NULL ? (long)x->GetBuildQueueSize() : 0;
        return 0;
    }
    case EE_UNITS_PLAYER_NEXT_UNIT: return EEPlayerUnitArg(theUnit == NULL ? thePlayer->mFirstUnit : static_cast<EEUnit*>(theUnit->mNext));
    case EE_UNITS_PLAYER_NEXT_COMPLEX: return EEPlayerUnitArg(thePlayer->GetNextComplexUnit(theUnit));
    case EE_UNITS_PLAYER_STATE:
        return (gLoadingSavedGame ? 1 : 0) | (gGameScreenActive ? 2 : 0) | (gWorldClearing ? 4 : 0) | (UShutdownInProgress() ? 8 : 0)
               | (gWorld.mVictoryEnabled ? 16 : 0) | (gUserInterface != NULL ? 32 : 0);
    case EE_UNITS_PLAYER_RTS_ADD_UNIT: thePlayer->RTSPlayer::AddUnit(theUnit, a[1] != 0); return 0;
    case EE_UNITS_PLAYER_ACTIVATE: gWorld.ActivateUnit(theComplex); return 0;
    case EE_UNITS_PLAYER_NODE_IS_AVAILABLE: return theNode->mIsAvailable;
    case EE_UNITS_PLAYER_SET_NODE_AVAILABLE: thePlayer->mTechnologies->SetNodeAvailable((long)a[0], a[1] != 0); return 0;
    case EE_UNITS_PLAYER_RESEARCH: thePlayer->mTechnologies->Research((long)a[0], NULL); return 0;
    case EE_UNITS_PLAYER_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_UNITS_PLAYER_ID: return thePlayer->mPlayerID;
    case EE_UNITS_PLAYER_UNIT_PLACED: thePlayer->UnitPlaced(theComplex); return 0;
    case EE_UNITS_PLAYER_CITIZEN_CONTAINS: return thePlayer->GetTechTree()->Get(kTechCitizen)->Contains((long)a[0]);
    case EE_UNITS_PLAYER_TECH_OBSOLETE: return thePlayer->mTechnologies->Get((long)a[0]) != NULL && thePlayer->mTechnologies->Get((long)a[0])->GetIsObsolete();
    case EE_UNITS_PLAYER_TECH_ALLOW:
        if (thePlayer->mTechnologies->Get((long)a[0]) != NULL)
            thePlayer->mTechnologies->Get((long)a[0])->mIsAllowed = a[1] != 0;
        return 0;
    case EE_UNITS_PLAYER_SWAP_KIND: return gPhysics->GetUnitSwapKind(theComplex);
    case EE_UNITS_PLAYER_DEAD_STATISTIC: return EEAddDeadUnitStatistic(theComplex);
    case EE_UNITS_PLAYER_STATS_RESET: thePlayer->mSavedGroups.Reset((long)a[0], (long)a[1]); return 0;
    case EE_UNITS_PLAYER_FOUNDATION: return theComplex->IsFoundation();
    case EE_UNITS_PLAYER_ADD_TO_TOWN: return thePlayer->AddUnitToTown(theComplex);
    case EE_UNITS_PLAYER_ADD_TO_HERD: return thePlayer->AddUnitToHerd(theComplex);
    case EE_UNITS_PLAYER_CIV_POWER: return gCivPowersManager.HasPower(thePlayer, (long)a[0]);
    case EE_UNITS_PLAYER_PULSE_EXPLORATION: {
        EECivPower thePower;
        gCivPowersManager.GetPower(kCivPowerExploration, thePower);
        thePlayer->PulseSight(true, thePower.mAmount, theUnit->mUnitID);
        return 0;
    }
    case EE_UNITS_PLAYER_UPDATE_WALL: gContinentManager.UpdateWallUnit(theComplex, a[1] != 0); return 0;
    case EE_UNITS_PLAYER_LOG_LOST: EEGameLogUnitLost(thePlayer, theComplex); return 0;   // port: the game log (ee/EEGameLog.h)
    case EE_UNITS_PLAYER_UNIT_NODE: return (int64_t)reinterpret_cast<uintptr_t>(theUnit->GetTechTreeNode());
    case EE_UNITS_PLAYER_REMOVE_FROM_TOWN: thePlayer->RemoveUnitFromTown(theComplex); return 0;
    case EE_UNITS_PLAYER_REMOVE_FROM_HERD: thePlayer->RemoveUnitFromHerd(theComplex); return 0;
    case EE_UNITS_PLAYER_REMOVE_SITE: thePlayer->RemoveConstructionSite(theComplex); return 0;
    case EE_UNITS_PLAYER_RECOMPUTE_AREAS: thePlayer->SuggestRecomputeAllAreaEffects(); return 0;
    case EE_UNITS_PLAYER_REMOVE_FOREST:
        if (theUnit->mResourceAmount != 0)
            theUnit->mResourceAmount = 0;
        gContinentManager.RemoveForestTile(theUnit);
        return 0;
    case EE_UNITS_PLAYER_CAMOUFLAGE:
        if (a[0] != 0)
            thePlayer->EnableCamouflagePower();
        else
            thePlayer->DisableCamouflagePower();
        return 0;
    case EE_UNITS_PLAYER_OWNER_ADD_WONDER: theComplex->mPlayer->AddWonder(theComplex, false); return 0;
    case EE_UNITS_PLAYER_WONDER_STARTED:
        if (gWorld.mPlayers[a[0]] != thePlayer)
            gWorld.mPlayers[a[0]]->WonderStarted(static_cast<EEComplexUnit*>(reinterpret_cast<RTSUnit*>(a[1])));
        return 0;
    case EE_UNITS_PLAYER_CLEAR_FOOTPRINT: thePlayer->ClearFootprint(theComplex); return 0;
    case EE_UNITS_PLAYER_BUILD_QUEUE_TECH: return theComplex->mBuildQueue[(size_t)a[1]].mTechnology;
    case EE_UNITS_PLAYER_PENDING_COUNT: return thePlayer->mPendingUnits.size();
    case EE_UNITS_PLAYER_PENDING_POPULATION: return dynamic_cast<EEUnit*>(thePlayer->mPendingUnits[(size_t)a[0]])->GetUnitClass()->GetPopulationCost();
    case EE_UNITS_PLAYER_WONDER_LIST:
        if (a[1] != 0) {
            thePlayer->mWonders->Add(theComplex);
            theComplex->mCloaked = gPhysics->mGameTime;
            thePlayer->mSavedGroups.Reset(0x10, 1);
        } else {
            thePlayer->mWonders->Remove(theComplex);
            theComplex->mCloaked = 0;
            thePlayer->mSavedGroups.Lower(0x10, 1);
        }
        return 0;
    case EE_UNITS_PLAYER_UPGRADE_TYPE:
        if ((uint64_t)a[0] >= thePlayer->mUnitClassUpgrades.size())
            return -2;
        return thePlayer->mUnitClassUpgrades[(size_t)a[0]] != NULL ? (int64_t)thePlayer->mUnitClassUpgrades[(size_t)a[0]]->mUnitClass->GetUnitType() : -1;
    case EE_UNITS_PLAYER_SET_CONTAINS: return gUnitSetManager.GetSet((long)a[0]) != NULL && gUnitSetManager.GetSet((long)a[0])->Contains((long)a[1]);
    case EE_UNITS_PLAYER_WONDER_FACTORS:
        io->f[0] = gWorld.GetGameVariant()->GetFactor((long)a[0]);
        io->f[1] = gWorldTable.GetFloatValue(kWorldWonderHitPointBonus);
        return 0;
    case EE_UNITS_PLAYER_UPGRADE_HIT_POINTS: EEUpgradeHitPoints(thePlayer->mUnitClassUpgrades[(size_t)a[0]], thePlayer->mUnitClassUpgrades[(size_t)a[0]]->mUnitClass->GetUnitType(), io->f[0], thePlayer); return 0;
    case EE_UNITS_PLAYER_SHARE_BONUS:
        if (a[0] != 0)
            gWorld.RemoveShareBonus();
        else
            gWorld.ApplyShareBonus();
        return 0;
    case EE_UNITS_PLAYER_SIGHT_SHARER:
        if (a[1] != 0)
            theOther->AddSightSharer(thePlayer);
        else
            theOther->RemoveSightSharer(thePlayer);
        return 0;
    // ApplyImprovement's and UpdateFreeUnitPower's (ee_units player_units.rs)
    case EE_UNITS_PLAYER_UPGRADE_VALUES: {
        EEUnitClassUpgrade* theUpgrade = theClass != NULL ? thePlayer->mUnitClassUpgrades[(size_t)a[0]] : NULL;
        if (theUpgrade == NULL)
            return 0;
        int64_t theValues[4] = {theClass->mFlightTime, theUpgrade->mFlightTime, theUpgrade->mPower, theUpgrade->mMaxHitPoints};
        memcpy(a, theValues, sizeof theValues);
        return 1;
    }
    case EE_UNITS_PLAYER_CHANGE_LEVEL: thePlayer->mUnitClassUpgrades[(size_t)a[0]]->ChangeLevel((long)a[1], (long)a[2]); return 0;
    case EE_UNITS_PLAYER_COMPLEX_SIGHT:
        if (a[1] != 0)
            theUnit->GetComplexUnit()->SightOnHere();
        else
            theUnit->GetComplexUnit()->SightOffHere();
        return 0;
    case EE_UNITS_PLAYER_COMPLEX_VALUES:
        a[0] = theComplex->mFlightTime;
        a[1] = theComplex->mPower;
        return 0;
    case EE_UNITS_PLAYER_SET_VALUE:
        if (a[2] == 0)
            theComplex->SetFlightTime((long)a[1]);
        else if (a[2] == 1)
            theComplex->SetPower((long)a[1]);
        else
            theUnit->SetHitPoints((long)a[1]);
        return 0;
    case EE_UNITS_PLAYER_GENERATE: io->f[0] = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_PLAYER_PRODUCE: return theUnit->GetUnitClass()->ProduceUnit(theUnit->GetComplexUnit(), gUnitClassTable[(size_t)a[1]], false);
    case EE_UNITS_PLAYER_SLAVERY_SOUND:
        gSoundManager->Play(EESoundObject::Get(kSoundSlavery)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_PLAYER_HEALED_SET: thePlayer->mHealedUnitSet = a[1] != 0 ? gUnitSetManager.GetSet((long)a[0]) : NULL; return 0;
    case EE_UNITS_PLAYER_AT: return (int64_t)reinterpret_cast<uintptr_t>((uint64_t)a[0] < gWorld.mNumPlayers ? gWorld.mPlayers[a[0]] : NULL);
    // the diplomacy and the shared sight (ee_units player_diplomacy.rs): the players as EEPlayer addresses, 0 this one
    case EE_UNITS_PLAYER_VIEW:
        a[2] = theOther->mEnemies[a[1]];
        a[0] = theOther->mPlayerID;
        a[1] = theOther->mSaveVisMap;
        return 0;
    case EE_UNITS_PLAYER_UNITS_SIGHT:
        for (RTSUnit* u = theOther->mFirstUnit; u != NULL; u = u->mNext)
            if (a[2] == 0 || static_cast<EEUnit*>(u)->mUnitClass->IsLandBuilding()) {
                if (a[1] != 0)
                    u->SightOnHere();
                else
                    u->SightOffHere();
            }
        return 0;
    case EE_UNITS_PLAYER_EVENTS_SIGHT:
        if (a[1] != 0)
            theOther->mEvents.ShowEvents(theOther);
        else
            theOther->mEvents.HideEvents(theOther);
        return 0;
    case EE_UNITS_PLAYER_ALLY: {
        EEPlayer* theAlly = a[1] != 0 ? reinterpret_cast<EEPlayer*>(a[1]) : thePlayer;
        if (a[2] != 0)
            theOther->mAllies.push_back(theAlly);
        else
            theOther->RemoveAlly(theAlly);
        return 0;
    }
    case EE_UNITS_PLAYER_STORE_TABLE: EEUnitStorePlayerTable(thePlayer); return 0;   // New (R5b.4): the store's diplomacy table
    case EE_UNITS_PLAYER_RESET_WONDER_TIMERS: thePlayer->ResetWonderTimers(theOther); return 0;
    case EE_UNITS_PLAYER_SHARERS:
        if (a[1] != 0)
            thePlayer->mSightSharers.push_back(theOther);
        else if (std::find(thePlayer->mSightSharers.begin(), thePlayer->mSightSharers.end(), theOther) != thePlayer->mSightSharers.end())
            thePlayer->mSightSharers.erase(std::find(thePlayer->mSightSharers.begin(), thePlayer->mSightSharers.end(), theOther));
        return 0;
    case EE_UNITS_PLAYER_VIEWED_OF: return thePlayer->mViewedUnit != NULL && thePlayer->mViewedUnit->mPlayer == theOther;
    case EE_UNITS_PLAYER_UNVIEW: thePlayer->UpdateSelection(NULL, true); return 0;
    case EE_UNITS_PLAYER_IS_CURRENT: return theOther == gWorld.GetCurrentPlayer();
    case EE_UNITS_PLAYER_REFRESH_TERRAIN: gWorld.GetCurrentPlayer()->RefreshTerrainVisibility(); return 0;
    case EE_UNITS_PLAYER_MINIMAP_REDRAW:
        if (gMiniMap != NULL)
            gMiniMap->Redraw();
        return 0;
    // the towns and herds (ee_units player_areas.rs): list a[0] (0 the towns, 1 the herds), the areas and units by address
    case EE_UNITS_PLAYER_AREA_COUNT: return a[0] == 0 ? thePlayer->mTowns.size() : thePlayer->mHerds.size();
    case EE_UNITS_PLAYER_AREA_AT:
        return a[0] == 0 ? (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mTowns[(size_t)a[1]])
                         : (int64_t)reinterpret_cast<uintptr_t>(thePlayer->mHerds[(size_t)a[1]]);
    case EE_UNITS_PLAYER_TOWN_UNIT: return EETown::IsTownUnit(theUnit);
    case EE_UNITS_PLAYER_HERD_ANIMAL: return EEHerd::IsAnimalOnMap(theUnit);
    case EE_UNITS_PLAYER_AREA_CAN_TAKE:
        return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->CanTake(EEPlayerUnitAt(a[2])) : reinterpret_cast<EEHerd*>(a[1])->CanTake(EEPlayerUnitAt(a[2]), true);
    case EE_UNITS_PLAYER_HERD_HAS: return reinterpret_cast<EEHerd*>(a[0])->HasUnit(EEPlayerUnitAt(a[1]));
    case EE_UNITS_PLAYER_AREA_ADD:
        return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->AddUnit(EEPlayerUnitAt(a[2])) : reinterpret_cast<EEHerd*>(a[1])->AddUnit(EEPlayerUnitAt(a[2]));
    case EE_UNITS_PLAYER_AREA_NEW: {
        EEComplexUnit* theNew = EEPlayerUnitAt(a[1]);
        bool theResult;
        if (a[0] == 0) {
            EETown* theTown = new EETown(thePlayer, ++gWorld.mTownIDCounter);
            theTown->SetSerial(++gContinentManager.mLastSerial);
            gContinentManager.AddArea(theTown);
            theResult = theTown->AddUnit(theNew);
            if (theResult) {
                thePlayer->mTownsByID.insert(std::pair<long, EETown*>(theTown->mTownID, theTown));
                thePlayer->mTowns.push_back(theTown);
            } else
                delete theTown;
        } else {
            EEHerd* theHerd = new EEHerd(thePlayer, theNew->mUnitClass->mAnimalID, ++gWorld.mHerdIDCounter);
            theHerd->SetSerial(++gContinentManager.mLastSerial);
            gContinentManager.AddArea(theHerd);
            theResult = theHerd->AddUnit(theNew);
            if (theResult)
                thePlayer->mHerds.push_back(theHerd);
            else
                delete theHerd;
        }
        return theResult;
    }
    case EE_UNITS_PLAYER_HOLDER: return EEPlayerUnitArg(theComplex->mHolder);
    case EE_UNITS_PLAYER_RANDOM: return gPhysics->GetRandom((long)a[0], (long)a[1]);
    case EE_UNITS_PLAYER_AREA_TILES: return a[0] == 0 ? reinterpret_cast<EETown*>(a[1])->GetNumTiles() : reinterpret_cast<EEHerd*>(a[1])->GetNumTiles();
    case EE_UNITS_PLAYER_AREA_REMOVE_UNIT:
        if (a[0] == 0)
            reinterpret_cast<EETown*>(a[1])->RemoveUnit(EEPlayerUnitAt(a[2]));
        else
            reinterpret_cast<EEHerd*>(a[1])->RemoveUnit(EEPlayerUnitAt(a[2]));
        return 0;
    case EE_UNITS_PLAYER_AREA_DROP:
        if (a[0] == 0) {
            EETown* theTown = thePlayer->mTowns[(size_t)a[1]];
            gContinentManager.RemoveArea(theTown);
            thePlayer->mTowns.erase(thePlayer->mTowns.begin() + (size_t)a[1]);
            delete theTown;
        } else {
            EEHerd* theHerd = thePlayer->mHerds[(size_t)a[1]];
            gContinentManager.RemoveArea(theHerd);
            thePlayer->mHerds.erase(thePlayer->mHerds.begin() + (size_t)a[1]);
            delete theHerd;
        }
        return 0;
    case EE_UNITS_PLAYER_TOWN_MERGE: {
        EETown* theTown = reinterpret_cast<EETown*>(a[0]);
        EETown* theOld = reinterpret_cast<EETown*>(a[1]);
        theTown->Merge(theOld);
        gContinentManager.RemoveArea(theOld);
        std::vector<EETown*>::iterator theIterator = std::find(thePlayer->mTowns.begin(), thePlayer->mTowns.end(), theOld);
        if (theIterator == thePlayer->mTowns.end())
            return 0;
        thePlayer->RemoveTownID(theOld->mTownID);
        thePlayer->mTowns.erase(theIterator);
        EEPPatrolGroup* theGroup = theOld->mPatrolGroup;
        if (theGroup != NULL) {
            EEComplexUnit* thePoint;
            while ((thePoint = theGroup->GetLast()) != NULL) {
                theGroup->RemoveUnit(thePoint);
                thePoint->SetPatrolGroup(NULL);
                thePoint->Kill(true, false, false);
                thePoint->mPlayer->KillUnit(thePoint, false);
            }
            theGroup->Reference();
        }
        delete theOld;
        return (int64_t)reinterpret_cast<uintptr_t>(theGroup);
    }
    case EE_UNITS_PLAYER_TOWN_REPATROL: {
        EETown* theTown = reinterpret_cast<EETown*>(a[0]);
        EEPPatrolGroup* const* theOld = reinterpret_cast<EEPPatrolGroup* const*>(a[1]);   // (addresses as int64_t: 64-bit pointers)
        if (theTown->mPatrolGroup == NULL)
            return 0;
        for (EEUnit* u = thePlayer->mFirstUnit; u != NULL; u = static_cast<EEUnit*>(u->mNext)) {
            EEUGRust* theGoal = u->GetComplexUnit() != NULL ? EEPatrolGoalCast(u->GetComplexUnit()->mGoalState) : NULL;
            if (theGoal != NULL && theGoal->GetPatrolGroup() != NULL && std::find(theOld, theOld + a[2], theGoal->GetPatrolGroup()) != theOld + a[2])
                theGoal->SetPatrolGroup(theTown->mPatrolGroup);
        }
        std::for_each(theOld, theOld + a[2], EEPDereferencer());
        return 0;
    }
    case EE_UNITS_PLAYER_TOWN_CONTAINS: return reinterpret_cast<EETown*>(a[0])->Contains(U2DPoint<long>((long)a[1], (long)a[2]));
    case EE_UNITS_PLAYER_AREA_TILE: {
        U2DPoint<long> theTile = theComplex->FindAreaTile(true, 1);
        a[0] = theTile.mX;
        a[1] = theTile.mY;
        return 0;
    }
    case EE_UNITS_PLAYER_ASSIGN_TILE: {
        EEComplexUnit* theBuilding = theUnit->GetComplexUnit();
        a[0] = theBuilding->mTileX != -10000 ? theUnit->mTileX : UMFloor(theBuilding->mLastMapPosition.GetX());
        a[1] = theBuilding->mTileX != -10000 ? theUnit->mTileY : UMFloor(theBuilding->mLastMapPosition.GetY());
        return 0;
    }
    case EE_UNITS_PLAYER_TOWN_PATROL:
        if (reinterpret_cast<EETown*>(a[0])->mPatrolGroup == NULL)
            reinterpret_cast<EETown*>(a[0])->RefreshPatrolGroup();
        return 0;
    }
    return 0;
}

// the Rust player's view of a player (ee_units player.rs)
struct EEPlayerRefs : EeUnitsPlayerRefs {
    EEPlayerRefs(const EEPlayer* inPlayer)
    {
        EEPlayer* thePlayer = const_cast<EEPlayer*>(inPlayer);
        this->player = thePlayer;
        this->host = &EEPlayerHost;
        this->saved = thePlayer;
        this->fields = thePlayer;
        this->stats = thePlayer->mSavedGroups.mStats;
    }
    operator const EeUnitsPlayerRefs*() const { return this; }
};

// the unit bookkeeping and the wonders (ee_units player_units.rs ee_units_player_units)
static long EEPlayerUnits(const EEPlayer* inPlayer, uint32_t inWhat, RTSUnit* inUnit = NULL, bool inFlag = false, bool inFlag2 = false)
{
    return ee_units_player_units(EEPlayerRefs(inPlayer), inWhat, inUnit, inFlag, inFlag2);
}

// EEWorld::ApplyShareBonus (inRemove false) and RemoveShareBonus over every player's records (ee_units player_units.rs)
void EEPlayerShareBonus(bool inRemove)
{
    std::vector<EeUnitsPlayerRefs> theRefs(gWorld.mNumPlayers);   // (no player: zeros)
    for (unsigned long i = 0; i < gWorld.mNumPlayers; i++)
        if (gWorld.mPlayers[i] != NULL)
            theRefs[i] = EEPlayerRefs(gWorld.mPlayers[i]);
    if (!theRefs.empty())
        ee_units_player_share_bonus(&theRefs[0], (uint32_t)theRefs.size(), inRemove, gWorldTable.GetFloatValue(kWorldWonderPopulationBonus),
                                    gWorldTable.GetFloatValue2(kWorldWonderPopulationBonus), gWorld.mPlayerPopulationLimit);
}

// the improvement decisions (ee_units player.rs ee_units_player_improvement)
static long EEPlayerImprovement(const EEPlayer* inPlayer, uint32_t inWhat, unsigned long inUnitType, long inImprovement, bool inFlag = false,
                                bool inFlag2 = false, float inFraction = 0.0f, UWideString* outReason = NULL)
{
    return ee_units_player_improvement(EEPlayerRefs(inPlayer), inWhat, inUnitType, inImprovement, inFlag, inFlag2, inFraction, outReason);
}

// Queues an improvement (front or back) when it can be bought, paying unless told not to (ee_units player.rs)
void EEPlayer::QueueImprovement(unsigned long inUnitType, unsigned long inImprovement, bool inFront, bool inAlreadyPaid)
{
    EEPlayerEconScope theScope;
    EEPlayerImprovement(this, 5, inUnitType, (long)inImprovement, inFront, inAlreadyPaid);
    theScope.Record(this, 16, 0, {(int32_t)inUnitType, (int32_t)inImprovement, inFront, inAlreadyPaid});
}

// Records a media item as loaded by the player (ignores NULL)
void EEPlayer::AddLoadedMedia(EEMedia* inMedia)
{
    if (inMedia != NULL)
        this->mLoadedMedia.push_back(inMedia);
}

// Starts every pending command group running, then adds the pending units as the base player does
void EEPlayer::AddPendingUnits(bool inKeepID, bool inReleasePending)
{
    for (unsigned long i = 0; i < this->mPendingGroups.size(); i++)
        this->mRunningGroups.push_back(this->mPendingGroups[i]);
    RTSPlayer::AddPendingUnits(inKeepID, inReleasePending);
}

// Adds a patrol group to the player, keeping a reference to it
void EEPlayer::AddPatrolGroup(EEPPatrolGroup* inGroup)
{
    inGroup->Reference();
    this->mPatrolGroups.push_back(inGroup);
}

// Starts sharing the player's sight with another player, refreshing its land buildings' sight around the change
void EEPlayer::AddSightSharer(EEPlayer* inPlayer)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 1, inPlayer, 0, false);
}

// Toggles units in the selection: adds built units not yet selected and removes the rest;
// plays the last added unit's selection sound during local selection
void EEPlayer::AddAndRemoveUnitsFromSelectedGroup(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    for (unsigned long i = 0; i < inUnitIDs.size(); i++) {
        long& theID = inUnitIDs[i];
        RTSUnit* theUnit = this->GetUnit(theID);
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (!theGroup->Contains(theComplexUnit) && theComplexUnit->mIsBuilt != false
            && theComplexUnit->mUnitClass->mUnitType != kUnitTypePathPoint) {
            theGroup->Add(theComplexUnit);
            theLastUnit = theComplexUnit;
        } else
            theGroup->RemoveUnit(theComplexUnit);
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// Refunds a fraction of a technology's cost in each resource (ee_units player.rs)
void EEPlayer::RefundTechnology(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    EEPlayerEconScope theScope;
    ee_units_player_cost(EEPlayerRefs(this), 5, inTechnology, inUnitType, 0, inFraction, NULL);
    theScope.Record(this, 8, 0, {EEPlayerNodeID(inTechnology), inUnitType, EEPlayerFloatBits(inFraction)});
}

// Refunds a fraction of a unit class improvement's cost at its queued level (ee_units player.rs)
void EEPlayer::RefundImprovement(EEUnitClass* inClass, long inImprovement, float inFraction)
{
    EEPlayerEconScope theScope;
    if (inClass != NULL)
        EEPlayerImprovement(this, 9, inClass->mUnitType, inImprovement, false, false, inFraction);
    theScope.Record(this, 9, 0, {inClass != NULL ? (int32_t)inClass->mUnitType : -1, inImprovement, EEPlayerFloatBits(inFraction)});
}

// Selects a unit by ID, starting a new selection when adding a unit of a player other than the selected one
void EEPlayer::AddUnitToSelectedGroup(unsigned long inUnitID, bool inFlag)
{
    RTSUnit* theUnit = ::gWorld.GetUnit(inUnitID);
    if (theUnit == NULL)
        return;
    unsigned long thePlayerID = theUnit->mPlayer->mPlayerID;
    bool theNewGroup;
    if (inFlag && (thePlayerID != this->mPlayerID || this->mActiveGroup->GetFirstUnit() == NULL
                   || this->mActiveGroup->GetFirstUnit()->mPlayer->mPlayerID != thePlayerID))
        theNewGroup = true;
    else
        theNewGroup = false;
    this->SelectUnit(inUnitID, inFlag, theNewGroup);
}

// Adds the built units of a list of IDs to the selection; plays the last added unit's selection
// sound during local selection
void EEPlayer::AddUnitsToSelectedGroup(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    for (unsigned long i = 0; i < inUnitIDs.size(); i++) {
        long& theID = inUnitIDs[i];
        RTSUnit* theUnit = this->GetUnit(theID);
        if (theUnit == NULL)
            continue;
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        if (!theGroup->Contains(theComplexUnit) && theComplexUnit->mIsBuilt != false
            && theComplexUnit->mUnitClass->mUnitType != kUnitTypePathPoint) {
            theGroup->Add(theComplexUnit);
            theLastUnit = theComplexUnit;
        }
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// Pushes back the start time of every wonder's timer by an amount
void EEPlayer::DelayWonderTimers(long inAmount)
{
    for (std::vector<EEComplexUnit*>::iterator it = this->mWonders->mItems.begin(); it != this->mWonders->mItems.end(); ++it)
        (*it)->mCloaked += inAmount;
}

// Adds one of the player's own units to the repatrol list once, keeping a reference to it
void EEPlayer::AddUnitToRepatrol(EEComplexUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mPlayer != this)
        return;
    if (std::find(this->mUnitsToRepatrol.begin(), this->mUnitsToRepatrol.end(), inUnit) == this->mUnitsToRepatrol.end()) {
        this->mUnitsToRepatrol.push_back(inUnit);
        inUnit->Reference();
    }
}

// (gNumPlayers 0x530e0c, which EECPAttackCommander.cpp and EECPAttackSubGroup.cpp also declare, is
// gWorld.mNumPlayers, unsigned: `jbe`)

// Adds a unit to the player: counts its population, activates it in the world, marks its tech node (researched when
// built), counts vital units, unlocks the capitol technologies on the first capitol and records statistics
void EEPlayer::AddUnit(RTSUnit* inUnit, bool inKeepID)
{
    EEPlayerUnits(this, 0, inUnit, inKeepID);
}

// Records a unit leaving the map with its tile and the current game time
void EEPlayer::AddOffMapUnit(RTSUnit* inUnit)
{
    EEWUnitsOffMapInfo* theInfo = NULL;
    if (inUnit == NULL)
        return;
    theInfo = new EEWUnitsOffMapInfo;
    theInfo->SetUnit(inUnit);
    theInfo->mLocation = EETileLocation(inUnit->mTileX, inUnit->mTileY);
    theInfo->mTime = gPhysics != NULL ? gPhysics->mGameTime : 0;
    this->mUnitsOffMap.push_back(theInfo);
}

// Puts an animal into a herd it may join (a new herd if none, its holder's herd or a random one if
// several); returns false when it is no herd animal or no herd took it
bool EEPlayer::AddUnitToHerd(EEComplexUnit* inUnit)
{
    return ee_units_player_areas(EEPlayerRefs(this), 1, static_cast<RTSUnit*>(inUnit));
}

// Adds a unit to the stranded list once, keeping a reference to it
void EEPlayer::AddStrandedUnit(EEUnit* inUnit)
{
    std::vector<EEUnit*>::iterator theIterator = std::find(this->mStrandedUnits.begin(), this->mStrandedUnits.end(), inUnit);
    if (theIterator == this->mStrandedUnits.end()) {
        inUnit->Reference();
        this->mStrandedUnits.push_back(inUnit);
    }
}

// Selects a unit into the active group or as the single viewed unit; returns false for a foreign unit
// while own units are selected
bool EEPlayer::TrySelectUnit(EEUnit* inUnit, bool inFlag)
{
    bool theResult = true;
    if (inFlag) {
        if (inUnit->mPlayer == this) {
            if (inUnit->mTileNode != NULL) {
                this->UpdateSelection(NULL, true);
                if (!this->mActiveGroup->Contains(inUnit->GetComplexUnit()))
                    this->mActiveGroup->Add(inUnit->GetComplexUnit());
            }
        } else if (this->mActiveGroup->GetFirstUnit() != NULL)
            theResult = false;
        else
            this->UpdateSelection(inUnit, true);
    } else if (inUnit->mTileNode != NULL) {
        this->UpdateSelection(NULL, true);
        if (!this->mActiveGroup->Contains(inUnit->GetComplexUnit()))
            this->mActiveGroup->Add(inUnit->GetComplexUnit());
    } else if (this->mActiveGroup->GetNumUnits() == 0)
        this->UpdateSelection(inUnit, true);
    return theResult;
}

// Town merge is defined after its existing patrol-goal include below.

// Removes a town ID from the player's town lookup
void EEPlayer::RemoveTownID(long inID)
{
    std::map<long, EETown*>::iterator theIterator = this->mTownsByID.find(inID);
    if (theIterator != this->mTownsByID.end())
        this->mTownsByID.erase(theIterator);
}

// Returns the last unit of the patrol route, or NULL when it is empty
EEComplexUnit* EEPPatrolGroup::GetLast()
{
    if (this->mPatrolPoints.size() > 0)
        return this->mPatrolPoints.back();
    return NULL;
}

// (0x563659: EEUGPatrol::SetPatrolGroup, defined after ee/EEUGPatrol.h's include below)

// Detaches a unit from the computer player's control
void EEPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
}

// Adds a unit to the player's construction sites unless it is already there
void EEPlayer::AddConstructionSite(EEComplexUnit* inUnit)
{
    if (!this->mConstructionSites->Contains(inUnit))
        this->mConstructionSites->Add(inUnit);
}

// Records a wonder (starting its timer unless told not to); the first wonder of a type grants its wonder power
void EEPlayer::AddWonder(EEComplexUnit* inUnit, bool inIsLoading)
{
    EEPlayerUnits(this, 7, inUnit, inIsLoading);
}

// Recounts the population, the vital units that keep the player alive and the vital buildings
void EEPlayer::RecountPopulation()
{
    EEPlayerUnits(this, 4);
}

// Whether an improvement can be bought counting the queue (ee_units player.rs can_buy); else the reason text
bool EEPlayer::CanBuy(long inUnitType, long inImprovement, UWideString* outText)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 10, EEPlayerImprovement(this, 3, inUnitType, inImprovement, false, false, 0.0f, outText) != 0, {inUnitType, inImprovement, outText != NULL}, outText);
}

// Returns whether the player can produce a unit type (cost and wonder limits included); sets the reason
bool EEPlayer::CanProduceUnitType(long inUnitType, long inArg, UWideString* outReason)
{
    EEPlayerEconScope theScope;
    bool theCaptured = [&]() -> bool {
        long theTechnology = gUnitClassTable[inUnitType] ? gUnitClassTable[inUnitType]->mTechnology : 100000;
        if (theTechnology != kTechNone)
            return this->CanProduce(theTechnology, inArg, 1, true, 3, outReason);
        return false;
    }();
    return theScope.Record(this, 20, theCaptured, {inUnitType, inArg, outReason != NULL}, outReason);
}

// Whether a technology or unit can be produced (ee_units player.rs can_produce); sets the reason text when not
bool EEPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 1, ee_units_player_can_produce(EEPlayerRefs(this), inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, outReason), {inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, outReason != NULL}, outReason);
}

// Whether an improvement is allowed and within the limits, ignoring the queue (ee_units player.rs)
bool EEPlayer::CanImprove(long inUnitType, long inImprovement, UWideString* outReason)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 11, EEPlayerImprovement(this, 2, inUnitType, inImprovement) != 0, {inUnitType, inImprovement, outReason != NULL}, outReason);
}

// Whether the player can afford a technology at a cost scale (ee_units player.rs); sets the reason text when not
bool EEPlayer::CanAffordTechnology(long inTechnology, float inScale, UWideString* outReason)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 5, ee_units_player_cost(EEPlayerRefs(this), 3, NULL, inTechnology, 0, inScale, outReason), {inTechnology, EEPlayerFloatBits(inScale), outReason != NULL}, outReason);
}


// Does nothing (the base player ignores being targeted by another player)
// (ICF-kept empty `ret 8`, shared by the slots below; FlareReceived below (slot +0x98) is the same empty body,
// folded into it: the base player ignores flares)
void EEPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
}

void EEPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
}

// (slots sharing the empty body: EECPAttackFormation+0x14; EEHumanPlayer+0x58, +0x98; EECalamityClass+0x28;
// EECCConscription, EECCLineOfSight, EECCSpecialAttack, EECCSpecialLocation +0x28; EEE3DFarm+0x20;
// EEPlayer+0x58, +0x98; EESETSCTAlwaysTrue+0x4, +0xc; EESETSETNoEffect, EESETSETRemoveObject, EESETSETKillObject,
// EESETSETFreezeObject, EESETSETObjectCreate +0x8, +0x10; EESETSETPVisibility, EESETSETPCameraFadeOut,
// EESETSETPCameraFadeIn, EESETSETPlayerVictory +0x8; EEUnit+0x3c, +0x44, +0x68)

// the class forwarders' upgrade of a unit class: NULL past the classes or without one
static EEUnitStats* EEPlayerClassStats(EEPlayer* inPlayer, unsigned long inUnitClass)
{
    return inUnitClass < EEUnitClassCount() ? inPlayer->mUnitClassUpgrades[inUnitClass] : NULL;
}

// For a human player whose unit class has its button loaded, releases one button's media
// and acquires the other's
void EEPlayer::SwapClassButtonMedia(unsigned long inUnitClass, EEButtonObject* inFirst, EEButtonObject* inSecond)
{
    EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass);
    if (theStats == NULL || !(theStats->mButtonLoadCount > 0) || this->IsHuman() != true)
        return;
    if (inFirst != NULL)
        inFirst->Release();
    if (inSecond != NULL)
        inSecond->Acquire();
}

// Sets one of a unit class's acknowledgement sounds for the player
void EEPlayer::SetClassAcknowledgeSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetAcknowledgeSound(inSound, inIndex);
}

// Sets a unit class's creation sound for the player
void EEPlayer::SetClassCreationSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetCreationSound(inSound);
}

// Sets a unit class's death sound for the player
void EEPlayer::SetClassDeathSound(unsigned long inUnitClass, EESoundObject* inSound)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetDeathSound(inSound);
}

// Sets a unit class's model for the player
void EEPlayer::SetClassModel(unsigned long inUnitClass, EEModel* inModel)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetModel(inModel);
}

// Sets a unit class's button for the player
void EEPlayer::SetClassButton(unsigned long inUnitClass, EEButtonObject* inButton)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetButton(inButton);
}

// Sets one of a unit class's selection sounds for the player
void EEPlayer::SetClassSelectionSound(unsigned long inUnitClass, EESoundObject* inSound, long inIndex)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->SetSelectionSound(inSound, inIndex);
}

// Deletes all of the player's herds
void EEPlayer::DeleteHerds()
{
    for (long i = this->mHerds.size() - 1; i >= 0; i--)
        delete this->mHerds[i];
    this->mHerds.clear();
}

// Deletes all of the player's towns and empties the town lookup
void EEPlayer::ClearTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--)
        delete this->mTowns[i];
    this->mTowns.clear();
    this->mTownsByID.clear();
}

// Drops every queued improvement of a unit type
void EEPlayer::EraseQueuedImprovements(unsigned long inUnitType)
{
    ee_units_queued_op(this->mQueued, 0, inUnitType, 0);
}

// Drops the pending command groups, then releases the pending units as the base player does
void EEPlayer::ReleasePendingUnits()
{
    this->mPendingGroups.clear();
    RTSPlayer::ReleasePendingUnits();
}

// Empties a numbered control group and tells the user interface
void EEPlayer::ClearSavedGroup(long inGroup)
{
    this->mGroups[inGroup]->Clear();
    if (gUserInterface != NULL)
        gUserInterface->PostRefreshSavedGroup(inGroup + 1);
}

// Releases every unit waiting to repatrol and empties the list
void EEPlayer::ClearUnitsToRepatrol()
{
    for (long i = this->mUnitsToRepatrol.size() - 1; i >= 0; i--) {
        if (this->mUnitsToRepatrol[i] != NULL)
            this->mUnitsToRepatrol[i]->Dereference();
    }
    this->mUnitsToRepatrol.clear();
}

// Handles a finished unit: registers wonders (telling the others when victory is on), counts vital
// buildings, applies the civ-power sight pulse, and updates area effects, construction sites, towns and walls
void EEPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 3, inUnit);
}

// Moves the improvement queue of one unit type to another
void EEPlayer::MoveQueuedImprovements(unsigned long inFrom, unsigned long inTo)
{
    ee_units_queued_op(this->mQueued, 1, inFrom, inTo);
}

// Returns the number of living heroes and/or heroes in production
long EEPlayer::GetNumHeroes(bool inFirst, bool inSecond)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 21, EEPlayerUnits(this, 11, NULL, inFirst, inSecond), {inFirst, inSecond});
}

// Clears every goal queue of a complex unit leaving the player
void EEPlayer::UnitRemoved(RTSUnit* inUnit)
{
    EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(inUnit);
    if (theUnit != NULL) {
        for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
            theUnit->ClearGoalQueue(i);
    }
}

// Removes the player's sight from a tile, and its allies' sight when line of sight is shared
void EEPlayer::RemoveSharedSight(RTSTile* inTile)
{
    if (inTile == NULL)
        return;
    inTile->RemoveSight(this);
    gDoppelgangerManager.TileHidden(inTile, this);
    if (this->mSharedLOS != true)
        return;
    for (long i = this->mAllies.size() - 1; i >= 0; i--) {
        if (this->mAllies[i]->mSharedLOS == true) {
            inTile->RemoveSight(this->mAllies[i]);
            gDoppelgangerManager.TileHidden(inTile, this->mAllies[i]);
        }
    }
}

// Removes the player's and its sight-sharing allies' sight from the tile at a location
void EEPlayer::RemoveSharedSightAt(const EETileLocation& inLocation)
{
    this->RemoveSharedSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// Removes only the player's own sight from a tile
void EEPlayer::RemoveOwnSight(RTSTile* inTile)
{
    if (inTile != NULL) {
        inTile->RemoveSight(this);
        gDoppelgangerManager.TileHidden(inTile, this);
    }
}

// Removes only the player's own sight from the tile at a location
void EEPlayer::RemoveOwnSightAt(const EETileLocation& inLocation)
{
    this->RemoveOwnSight(gWorld.mMap->GetTileAtLocation(inLocation));
}

// Writes the player's per-class media counts and media load warnings to a text file
void EEPlayer::DumpMedia(FSFileSpec& inFile)
{
    UWideString theString;
    fprintf(inFile, "Media Information For Player %ld, The Players Name is %s\n\n", this->mPlayerID, (const char*)this->mName);
    for (unsigned long i = 0; i < this->mUnitClassUpgrades.size(); i++) {
        if (this->mUnitClassUpgrades[i] != NULL)
            this->mUnitClassUpgrades[i]->DumpMediaCount(inFile);
    }
    fprintf(inFile, "\n\n\t\tMedia that might not have been loaded correctly:\n");
    for (unsigned long j = 0; j < this->mMediaWarnings.size(); j++)
        fprintf(inFile, "\t\t%s", (const char*)this->mMediaWarnings[j]);
}

// Marks the player as having met another player when it first sees a unit of a non-gaia player
void EEPlayer::NoticeForeignUnit(EEUnit* inUnit)
{
    if (this->mCombatState == 0 && inUnit->mPlayer->mPlayerID != 0)
        this->mCombatState = 1;
}

// Puts every herd animal of the player that is in no herd into one
void EEPlayer::AssignHerds()
{
    ee_units_player_areas(EEPlayerRefs(this), 5, NULL);
}

// Adds every land building outside the player's towns to a town, then gives each town without
// a patrol group one
void EEPlayer::AssignTowns()
{
    ee_units_player_areas(EEPlayerRefs(this), 4, NULL);
}

// Executes a selection message against the player's network selection, keeping its local
// active selection and viewed unit
void EEPlayer::ExecuteSelectionMessage(NEMessage* inMessage)
{
    RTSUnit* theUnit = this->mViewedUnit;
    EESelectedGroup* theGroup = this->mActiveGroup;
    if (theUnit != NULL)
        theUnit->Reference();
    else
        theUnit = NULL;
    this->mActiveGroup = this->mSelectedGroup;
    gWorld.mExecutingLocalSelection = true;
    inMessage->Execute();
    gWorld.mExecutingLocalSelection = false;
    this->mSelectedGroup = this->mActiveGroup;
    this->mActiveGroup = theGroup;
    this->SetSelectedViewedUnit(this->mViewedUnit);
    this->UpdateSelection(static_cast<EEUnit*>(theUnit), false);
    if (theUnit != NULL)
        theUnit->Dereference();
}

// Returns the town nearest to a tile, optionally only on the tile's continent; NULL when none
EEAIArea* EEPlayer::GetNearestArea(const EETileCoordinate& inTile, bool inSameContinent)
{
    EETileDistanceLess theClosest(0, 0);
    unsigned short theContinent = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
    long theBest = 0;
    EEAIArea* theArea = NULL;
    for (long i = this->GetNumTowns() - 1; i >= 0; i--) {
        EETown* theTown = this->mTowns[i];
        if (theTown == NULL || (inSameContinent && theTown->mContinent != theContinent))
            continue;
        theClosest.mCenter = theTown->GetClosestPoint(inTile);
        long theDistance = theClosest.DistanceSquared(inTile);
        if (theArea == NULL || theDistance < theBest) {
            theBest = theDistance;
            theArea = theTown;
        }
    }
    return theArea;
}

// Returns the construction site nearest to a unit within a squared distance of 100, or NULL
EEUnit* EEPlayer::GetNearestConstructionSite(EEComplexUnit* inUnit)
{
    EEUnit* theResult = NULL;
    long theBestDistance = 100;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETileDistanceLess theCenter(inUnit->mTileX, inUnit->mTileY);
    for (std::vector<EEComplexUnit*>::iterator theIterator = this->mConstructionSites->mItems.begin();
         theIterator != this->mConstructionSites->mItems.end(); ++theIterator) {
        theTile.mX = (*theIterator)->mTileX;
        theTile.mY = (*theIterator)->mTileY;
        long theDistance = theCenter.DistanceSquared(theTile);
        if (theDistance < theBestDistance) {
            theResult = *theIterator;
            theBestDistance = theDistance;
        }
    }
    return theResult;
}

// Grants a wonder's power by its bonus kind: area conversion, a hit-point bonus for a unit set, the
// share bonus, sight of enemies, or healing of a unit set
void EEPlayer::AddWonderPower(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 9, inUnit);
}

// Returns the running, or else pending, command group holding a unit; NULL when none
EEGroup* EEPlayer::GetActiveGroupOf(EEComplexUnit* inUnit)
{
    EEGroup* theResult = NULL;
    for (long i = this->mRunningGroups.size() - 1; i >= 0; i--) {
        if (this->mRunningGroups[i]->Contains(inUnit)) {
            theResult = this->mRunningGroups[i];
            break;
        }
    }
    if (theResult != NULL)
        return theResult;
    for (long j = this->mPendingGroups.size() - 1; j >= 0; j--) {
        if (this->mPendingGroups[j]->Contains(inUnit)) {
            theResult = this->mPendingGroups[j];
            break;
        }
    }
    return theResult;
}

// Returns whether the player can afford to repair a number of a unit's hit points (cost relative to its maximum)
bool EEPlayer::CanAffordRepair(EEUnit* inUnit, EEUnit* inOther, unsigned long inValue)
{
    EEPlayerEconScope theScope;
    bool theCaptured = [&]() -> bool {
        unsigned long theMaximum = inUnit->mStats->mMaxHitPoints;
        EETechTreeNode* theNode = this->GetTechTree()->Get(inUnit->mUnitClass->GetTechnology());
        return this->CanAffordScaled(theNode, inOther->mUnitClass->GetUnitType(), (float)inValue / theMaximum, 1);
    }();
    return theScope.Record(this, 18, theCaptured, {(int32_t)inUnit->mUnitClass->mUnitType, (int32_t)inOther->mUnitClass->mUnitType, (int32_t)inValue});
}

// Adds the player's resources, population and tech tree to the sync checksum
void EEPlayer::GetChecksum(ESChecksum& ioChecksum)
{
    unsigned long theSum = 0;
    RTSPlayer::GetChecksum(ioChecksum);
    for (unsigned char i = 0; i < 7; i++)
        theSum += this->mResourceCaps[i];
    theSum += this->mPopulationCount;
    this->mTechnologies->GetChecksum(ioChecksum);
    UString theName("EEPlayer");
    ioChecksum.AddChecksum(theName, theSum);
}

// Returns the town containing a tile, or else the nearest town and its closest point to the tile
EETown* EEPlayer::GetNearestTown(const EETileLocation& inTile, EETileLocation& outTile)
{
    EETown* theTown = NULL;
    EETileLocation theCandidate;
    long theDistance = 0;
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        EETown* theCurrent = this->mTowns[i];
        if (!(!theCurrent->HullContains(inTile))) {
            theTown = this->mTowns[i];
            outTile = inTile;
            break;
        } else {
            if (theTown != NULL) {
                theCandidate = this->mTowns[i]->GetClosestPoint(inTile);
                if (theCandidate.DistanceToSquared(inTile) < theDistance) {
                    outTile = theCandidate;
                    theTown = this->mTowns[i];   // the closer town too (was its tile only)
                    theDistance = theCandidate.DistanceToSquared(inTile);   // was (theCandidate): always 0
                }
            } else {
                outTile = this->mTowns[i]->GetClosestPoint(inTile);
                theTown = this->mTowns[i];
                theDistance = outTile.DistanceToSquared(inTile);
            }
        }
    }
    return theTown;
}

// Returns whether the player still plays: not an observer, or an observer whose game has ended
bool EEPlayer::IsActive()
{
    return !this->mObserverMode || this->mGameResult != 0;
}

// Returns the town whose area contains a unit's tile, or NULL
EETown* EEPlayer::GetTownOf(EEUnit* inUnit)
{
    U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
    EETown* theTown = NULL;
    for (long i = this->GetNumTowns() - 1; i >= 0; i--) {
        EETown* theCandidate = this->mTowns[i];
        if (theCandidate != NULL && theCandidate->HullContains(theTile)) {
            theTown = this->mTowns[i];
            break;
        }
    }
    return theTown;
}

// Appends the player's herds of a unit type's animal to a list
void EEPlayer::GetHerdsOfType(std::vector<EEHerd*>& outHerds, long inUnitType)
{
    EEAnimal* theAnimal = EEAnimal::Find(inUnitType);
    if (theAnimal == NULL)
        return;
    for (long i = this->mHerds.size() - 1; i >= 0; i--) {
        EEHerd*& theHerd = this->mHerds[i];
        if (theHerd->mAnimalID == theAnimal->mAnimalID)
            outHerds.push_back(theHerd);
    }
}

// A unit type's level in an improvement counting the queued ones; -1 without an upgrade (ee_units player.rs)
long EEPlayer::GetImprovementLevel(unsigned long inUnitType, long inImprovement) const
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 13, EEPlayerImprovement(this, 0, inUnitType, inImprovement), {(int32_t)inUnitType, inImprovement});
}

// A unit type's queued improvements of a kind past the first (ee_units player.rs)
long EEPlayer::CountQueuedImprovements(unsigned long inUnitType, long inImprovement) const
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 14, EEPlayerImprovement(this, 1, inUnitType, inImprovement), {(int32_t)inUnitType, inImprovement});
}

// Whether an improvement has reached its limits counting the queue (ee_units player.rs)
bool EEPlayer::IsImprovementMaxed(unsigned long inUnitType, long inImprovement)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 12, EEPlayerImprovement(this, 4, inUnitType, inImprovement) != 0, {(int32_t)inUnitType, inImprovement});
}

// Returns the terrain kinds the player's citizens can cross, as passability flags 1/2/4/8
long EEPlayer::GetCitizenPassability()
{
    EEUnitStats* theStats = this->mUnitClassUpgrades[kUnitTypeCitizen];
    long theResult = 0;
    if (theStats != NULL) {
        long theFlags = theStats->mTerrainMask;
        if (theFlags & 1)
            theResult = 4;
        if (theFlags & 0x200)
            theResult |= 1;
        if (theFlags & 2)
            theResult |= 2;
        if (theFlags & 0x1000)
            theResult |= 8;
    }
    return theResult;
}

// Registers every herd of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterHerds()
{
    for (long i = this->mHerds.size() - 1; i >= 0; i--) {
        if (this->mHerds[i] == NULL)
            continue;
        ++gContinentManager.mLastSerial;
        this->mHerds[i]->SetSerial(gContinentManager.mLastSerial);
        gContinentManager.AddArea(this->mHerds[i]);
    }
}

// Registers every town of the player as an area with the continent manager under a new serial
void EEPlayer::RegisterTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (this->mTowns[i] == NULL)
            continue;
        ++gContinentManager.mLastSerial;
        this->mTowns[i]->SetSerial(gContinentManager.mLastSerial);
        gContinentManager.AddArea(this->mTowns[i]);
    }
}

// Returns the player's patrol group with an ID, or NULL
EEPPatrolGroup* EEPlayer::GetPatrolGroup(unsigned long inID)
{
    EEPPatrolGroup* theResult = NULL;
    for (unsigned long i = 0; i < this->mPatrolGroups.size(); i++) {
        if (this->mPatrolGroups[i]->mID == inID) {
            theResult = this->mPatrolGroups[i];
            break;
        }
    }
    return theResult;
}

// Returns the population, optionally adding the pending units and the units inside category-0x1d containers
long EEPlayer::GetTotalPopulation(bool inFirst, bool inSecond)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 19, EEPlayerUnits(this, 6, NULL, inFirst, inSecond), {inFirst, inSecond});
}

// Returns the player's population limit: the game's limit plus the player's bonuses
long EEPlayer::GetPopulationLimit(long inArg)
{
    return this->mPopulationLimitBonus + this->mShareBonus + gWorld.mPlayerPopulationLimit;
}

// Whether one resource covers the cost and the required statistic is reached (ee_units player.rs)
bool EEPlayer::CanAffordSingleResource(EETechTreeNode* inNode, long inUnitClass, long inCount)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 4, ee_units_player_cost(EEPlayerRefs(this), 1, inNode, inUnitClass, inCount, 0.0f, NULL), {EEPlayerNodeID(inNode), inUnitClass, inCount});
}

// Whether every resource covers a scaled cost, metallurgy pooling iron and gold (ee_units player.rs)
bool EEPlayer::CanAffordScaled(EETechTreeNode* inNode, long inUnitClass, float inScale, long inCount)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 3, ee_units_player_cost(EEPlayerRefs(this), 0, inNode, inUnitClass, inCount, inScale, NULL), {EEPlayerNodeID(inNode), inUnitClass, EEPlayerFloatBits(inScale), inCount});
}

// Returns the index of the team holding the player (0 when none)
unsigned long EEPlayer::GetTeamIndex()
{
    unsigned long thePlayerID = this->mPlayerID;
    unsigned long theResult = 0;
    for (unsigned long i = 0; i < gWorld.GetNumTeams(); i++) {
        EETeam& theTeam = gWorld.mTeams[i];
        if (theTeam.ContainsPlayer(thePlayerID)) {
            theResult = i;
            break;
        }
    }
    return theResult;
}

// Returns the player's town with an ID, or NULL
EETown* EEPlayer::GetTownByID(long inID)
{
    EETown* theTown = NULL;
    for (unsigned long i = 0; i < this->mTowns.size() && theTown == NULL; i++) {
        if (this->mTowns[i]->mTownID == inID)
            theTown = this->mTowns[i];
    }
    return theTown;
}

// Returns the off-map record of a unit, or NULL
EEWUnitsOffMapInfo* EEPlayer::GetOffMapInfo(EEComplexUnit* inUnit)
{
    EEWUnitsOffMapInfo* theResult = NULL;
    if (inUnit != NULL) {
        for (long i = this->mUnitsOffMap.size() - 1; i >= 0; i--) {
            if (inUnit == this->mUnitsOffMap[i]->mUnit) {
                theResult = this->mUnitsOffMap[i];
                break;
            }
        }
    }
    return theResult;
}

// Returns the timer start time of a wonder by index
long EEPlayer::GetWonderTime(long inIndex) const
{
    const std::vector<EEComplexUnit*>& theItems = this->mWonders->mItems;
    return theItems[inIndex]->mCloaked;
}

// Returns the number of wonders the player holds
unsigned long EEPlayer::GetNumWonderUnits() const
{
    return this->mWonders->GetSize();
}

// Whether the player can afford a technology in all resources or else from a single one (ee_units player.rs)
bool EEPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 2, ee_units_player_cost(EEPlayerRefs(this), 2, inTechnology, inUnitType, inCount, 1.0f, NULL), {EEPlayerNodeID(inTechnology), inUnitType, inCount});
}

// Adds damage taken from a player to its tally and enters combat when that player is an enemy
void EEPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    this->mDamageFrom[inPlayer] += inAmount;
    if (inPlayer == 0)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (thePlayer != this && this->mEnemies[thePlayer->mPlayerID] != 0)
        this->mCombatState = 2;
}


// Under civ power 0x27, periodically makes a free unit (class 0x20 or 0x1e0) at a town center or capitol,
// playing a sound for the local player and lengthening the interval each time
void EEPlayer::UpdateFreeUnitPower()
{
    EEPlayerUnits(this, 12);
}

// Advances the player to the next epoch
void EEPlayer::IncrementEpic()
{
    this->mTechnologies->AdvanceEpoch();
}

// Resets the player to its starting state: new selection and control groups, upgrades, tech tree and
// air traffic, zeroed counters and default settings, then loads the player effects
void EEPlayer::Reset()
{
    this->mActiveGroup = new EESelectedGroup(this);
    this->mSelectedGroup = new EESelectedGroup(this);
    this->mL9B4 = new EESelectedGroup(this);
    for (unsigned long i = 0; i < 20; i++)
        this->mGroups[i] = new EESavedGroup(this, i + 1);
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long j = 0; j < theCount; j++) {
        this->mRunningGroups[j]->Clear();
        this->mRunningGroups[j]->Dereference();
    }
    this->mRunningGroups.clear();
    this->mActiveGroup->Clear();
    for (unsigned long k = 0; k < 20; k++)
        this->mGroups[k]->Clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    // port (R5b.4): the record's fields (EeUnitsPlayerSaved: the caps, enemies, counters, flags, timers) are set by
    // ee_units player_tick.rs `reset`: those the original set before the events' clear here (no call between reads
    // them), the trade, refunds and vital buildings after it
    ee_units_player_reset(static_cast<EeUnitsPlayerSaved*>(this), (uint32_t)this->mPlayerID, 0);
    this->mSavedGroups.Clear();
    this->mSavedGroups.SetName(this->GetName());
    EEUnitStorePlayerTable(this);   // New (R5b.4): the store's diplomacy table
    this->mUnitClassUpgrades.resize(EEUnitClassCount());
    for (long theClass = 0; theClass < EEUnitClassCount(); theClass++) {
        if (gUnitClassTable[theClass] != NULL)
            this->mUnitClassUpgrades[theClass] = new EEUnitClassUpgrade(gUnitClassTable[theClass]);
        else
            this->mUnitClassUpgrades[theClass] = NULL;
    }
    this->mTechnologies = new EETechTree;
    *this->mTechnologies = gTechTree;
    for (long theAirport = 0; theAirport < 4; theAirport++)
        mAirTraffic[theAirport] = new EEAirTrafficControl(this);
    ee_units_player_fields_reset(this, gWorldTable.GetLongValue(kWorldStartingCitizensDefault),
                                 gWorldTable.GetLongValue(kWorldGatherersPerResourceMax));   // (ee_units player.rs)
    this->mHealedUnitSet = NULL;
    this->mEvents.Clear(this);
    ee_units_player_reset(static_cast<EeUnitsPlayerSaved*>(this), (uint32_t)this->mPlayerID, 1);
    this->LoadPlayerEffects();
}

// Returns the group's unit at an index
EEComplexUnit* EEGroup::GetUnit(long inIndex)
{
    const std::vector<EEComplexUnit*>& theUnits = this->mUnits;
    return theUnits[inIndex];
}

// Returns the number of units in the group
unsigned long EEGroup::GetNumUnits()
{
    return this->mUnits.size();
}

// Kills every unit of a herd
void EEPlayer::KillHerdUnits(EEHerd* inHerd, bool inKilled)
{
    std::vector<EEComplexUnit*> theUnits(inHerd->mUnits.begin(), inHerd->mUnits.end());
    while (theUnits.size() != 0) {
        theUnits.back()->Kill(inKilled, 0, 0);
        theUnits.pop_back();
    }
}

// Kills every stranded unit, removes it from the player and empties the stranded list
void EEPlayer::KillStrandedUnits()
{
    for (long i = this->mStrandedUnits.size() - 1; i >= 0; i--) {
        if (this->mStrandedUnits[i] == NULL)
            continue;
        this->mStrandedUnits[i]->Kill(0, 0, 0);
        this->KillUnit(this->mStrandedUnits[i], false);
        this->mStrandedUnits[i]->Dereference();
    }
    this->mStrandedUnits.clear();
}

// The context of EEPlayer::Load and Save's operations (EEPlayerSaveOperation)
struct EEPlayerSaveContext {
    EEPlayer* mPlayer;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
    EEUnitClassUpgrade* mDummyUpgrade;      // the load's stand-in upgrade (UPGRADE_DUMMY_NEW .. DELETE)
};

static int32_t EEPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);

// Loads the player's state from a saved game (the mirror of Save); returns the read status, some reads' errors ignored.
// port (R5b.4): the body is Rust (ee_units player_save.rs `load`: the original's four bodies, one per saved-game era,
// as one sequence with the eras' returns and assertion lines) over the record this class derives from
// (EeUnitsPlayerSaved) and EEPlayer::SaveOperation's parts
long EEPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEPlayerSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL};
    return ee_units_player_load(&theContext, &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// The C++ parts of the player's Load and Save (ee_units player_save.rs): the file's primitives with the overload the
// original called (io holds the old value in, the read value out; a bool as 0 or 1) and the player's containers and
// sub-objects by operation
// The Rust saves' file primitives (ee/EEPlayer.h): the overload the original called; a read's io holds the old value
// in and the read value out (a bool as 0 or 1)
int32_t EESavePrimitive(FSFile& theFile, int32_t inOp, EeUnitsPlayerSaveIo* io)
{
    switch (inOp) {
    case EE_UNITS_PLAYER_SAVE_READ_ULONG: {
        unsigned long theValue = (unsigned long)io->i;
        long theResult = theFile.Read(theValue);
        io->i = (int64_t)theValue;
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_READ_LONG: {
        long theValue = (long)io->i;
        long theResult = theFile.Read(theValue);
        io->i = (int64_t)theValue;
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_READ_FLOAT: {
        float theValue = io->f;
        long theResult = theFile.Read(theValue);
        io->f = theValue;
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_READ_BOOL: {
        bool theValue = io->b != 0;
        long theResult = theFile.Read(theValue);
        io->b = theValue ? 1 : 0;
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_WRITE_ULONG: {
        unsigned long theValue = (unsigned long)io->i;
        return theFile.Write(theValue);
    }
    case EE_UNITS_PLAYER_SAVE_WRITE_LONG: {
        long theValue = (long)io->i;
        return theFile.Write(theValue);
    }
    case EE_UNITS_PLAYER_SAVE_WRITE_FLOAT: {
        float theValue = io->f;
        return theFile.Write(theValue);
    }
    case EE_UNITS_PLAYER_SAVE_WRITE_BOOL: {
        bool theValue = io->b != 0;
        return theFile.Write(theValue);
    }
    case EE_UNITS_PLAYER_SAVE_READ_USHORT: {
        unsigned short theValue = (unsigned short)io->i;
        long theResult = theFile.Read(theValue);
        io->i = (int64_t)theValue;
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_WRITE_USHORT: {
        unsigned short theValue = (unsigned short)io->i;
        return theFile.Write(theValue);
    }
    }
    return -1;
}

int32_t EESaveFilePrimitive(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    return EESavePrimitive(*static_cast<FSFile*>(inFile), inOp, io);
}

static int32_t EEPlayerSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerSaveContext& theContext = *static_cast<EEPlayerSaveContext*>(inContext);
    EEPlayer* thePlayer = theContext.mPlayer;
    FSFile& theFile = *theContext.mFile;
    unsigned long theMajor = theContext.mMajorVersion;
    unsigned long theMinor = theContext.mMinorVersion;
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(theFile, inOp, io);
    switch (inOp) {
    // the load
    case EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW: {
        theContext.mDummyUpgrade = NULL;
        for (unsigned long i = 0; i < EEUnitClassCount(); i++) {
            if (gUnitClassTable[i] != NULL) {
                theContext.mDummyUpgrade = new EEUnitClassUpgrade(gUnitClassTable[i]);
                break;
            }
        }
        ESVERIFY_AT(theContext.mDummyUpgrade != NULL, inIndex);
        return kErrorOK;
    }
    case EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD: {
        unsigned long theID = (unsigned long)io->i;
        if (theID < EEUnitClassCount() && thePlayer->mUnitClassUpgrades[theID] != NULL) {
            thePlayer->mUnitClassUpgrades[theID]->Load(theMajor, theMinor, theFile);   // result ignored
            return kErrorOK;
        }
        return theContext.mDummyUpgrade->Load(theMajor, theMinor, theFile);
    }
    case EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_DELETE:
        delete theContext.mDummyUpgrade;
        theContext.mDummyUpgrade = NULL;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_RTS_LOAD:
        thePlayer->RTSPlayer::Load(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_SET_GOAL_COUNTER:
        sGoalCounter = (unsigned long)io->i;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_TECH_DELETE:
        delete thePlayer->mTechnologies;
        thePlayer->mTechnologies = NULL;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_TECH_LOAD:
        thePlayer->mTechnologies = new EETechTree;
        *thePlayer->mTechnologies = gTechTree;
        thePlayer->mTechnologies->SetPlayerIndex(thePlayer->mPlayerID);
        thePlayer->mTechnologies->SetPlayer(thePlayer);
        return thePlayer->mTechnologies->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_PATROL_CLEAR:
        thePlayer->mPatrolGroups.clear();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_LOAD: {
        EEPPatrolGroup* theGroup = new EEPPatrolGroup;
        long theResult = theGroup->Load(theMajor, theMinor, &theFile);
        if (theResult == kErrorOK)
            thePlayer->AddPatrolGroup(theGroup);
        theGroup->Dereference();
        return theResult;
    }
    case EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD:
        thePlayer->mSavedGroups.Load(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE:
        EEUnitStorePlayerTable(thePlayer);   // New (R5b.4): the store's diplomacy table
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_AIR_LOAD:
        return thePlayer->mAirTraffic[inIndex]->Load(theMajor, theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_ACTIVE_LOAD:
        return thePlayer->mActiveGroup->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIV_LOAD:
        return thePlayer->mCivilization.Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_GROUP_LOAD:
        return thePlayer->mGroups[inIndex]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIVFILE_LOAD:
        return thePlayer->mCivilizationFile.Load(theFile);
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH:
        thePlayer->mConstructionSiteIDs.push_back((long)io->i);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERDS_RESIZE:
        thePlayer->mHerds.resize((unsigned long)io->i, NULL);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERD_LOAD:
        thePlayer->mHerds[inIndex] = new EEHerd;
        return thePlayer->mHerds[inIndex]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_LOAD:
        return thePlayer->mEvents.Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE:
        thePlayer->mUnitsOffMap.resize((unsigned long)io->i, NULL);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_OFFMAP_LOAD:
        thePlayer->mUnitsOffMap[inIndex] = new EEWUnitsOffMapInfo;
        return thePlayer->mUnitsOffMap[inIndex]->Load(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_WONDER_PUSH:
        thePlayer->mWonderIDs.push_back((long)io->i);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR:
        thePlayer->mWonders->Clear();
        thePlayer->mWonders->mItems.clear();
        return kErrorOK;
    // the save
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS:
        io->i = (int64_t)thePlayer->mUnitClassUpgrades.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT:
        return thePlayer->mUnitClassUpgrades[inIndex] != NULL;
    case EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE:
        return thePlayer->mUnitClassUpgrades[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_RTS_SAVE:
        thePlayer->RTSPlayer::Save(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_GOAL_COUNTER:
        io->i = (int64_t)sGoalCounter;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_TECH_PRESENT:
        return thePlayer->mTechnologies != NULL;
    case EE_UNITS_PLAYER_SAVE_TECH_SAVE:
        if (thePlayer->mTechnologies != NULL)
            return thePlayer->mTechnologies->Save(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_SLOTS:
        io->i = (int64_t)thePlayer->mPatrolGroups.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_PATROL_KIND: {
        EEPPatrolGroup* theGroup = thePlayer->mPatrolGroups[inIndex];
        return theGroup == NULL ? 0 : theGroup->mIsTownPatrol ? 1 : 2;
    }
    case EE_UNITS_PLAYER_SAVE_PATROL_SAVE:
        return thePlayer->mPatrolGroups[inIndex]->Save(theMajor, theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_SAVE:
        thePlayer->mSavedGroups.Save(theMajor, theMinor, theFile);
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_AIR_SAVE:
        return thePlayer->mAirTraffic[inIndex]->Save(theMajor, theMinor, &theFile);
    case EE_UNITS_PLAYER_SAVE_ACTIVE_SAVE:
        return thePlayer->mActiveGroup->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIV_SAVE:
        return thePlayer->mCivilization.Save(theFile);
    case EE_UNITS_PLAYER_SAVE_GROUP_SAVE:
        return thePlayer->mGroups[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_CIVFILE_SAVE:
        return thePlayer->mCivilizationFile.Save(theFile);
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_COUNT:
        io->i = (int64_t)thePlayer->mConstructionSites->GetSize();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_CONSTRUCTION_ID:
        io->i = (int64_t)(unsigned long)thePlayer->mConstructionSites->mItems[inIndex]->mUnitID;
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERD_COUNT:
        io->i = (int64_t)thePlayer->mHerds.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_HERD_SAVE:
        return thePlayer->mHerds[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_EVENTS_SAVE:
        return thePlayer->mEvents.Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_OFFMAP_COUNT:
        io->i = (int64_t)thePlayer->mUnitsOffMap.size();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_OFFMAP_SAVE:
        return thePlayer->mUnitsOffMap[inIndex]->Save(theMajor, theMinor, theFile);
    case EE_UNITS_PLAYER_SAVE_WONDER_COUNT:
        io->i = (int64_t)thePlayer->mWonders->GetSize();
        return kErrorOK;
    case EE_UNITS_PLAYER_SAVE_WONDER_ID:
        io->i = (int64_t)(unsigned long)thePlayer->mWonders->mItems[inIndex]->mUnitID;
        return kErrorOK;
    }
    ESVERIFY_AT(false, inOp);
    return kErrorOK;
}

// Loads the class media of every unit class that has media and records those classes for a later unload
void EEPlayer::LoadAllClassMedia()
{
    for (long i = 0; i < EEUnitClassCount(); i++) {
        EEUnitStats* theStats = this->mUnitClassUpgrades[i];
        if (theStats != NULL && theStats->GetMediaCount() > 0) {
            theStats->mUnitClass->LoadClassMedia();
            this->mSoundLoadedClasses.push_back(this->mUnitClassUpgrades[i]->mUnitClass->mUnitType);
        }
    }
}

// Loads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::LoadUnitClassButtons(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->LoadButtons(this);
}

// Loads the media of one unit class for the player, if the class index is valid
void EEPlayer::SetUnitClassMedia(long inUnitClass, long inMediaFlags)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->LoadMedia(this, inMediaFlags);
}

// LoadUnit's and SaveUnit's C++ parts (ee_units player_save.rs load_unit, save_unit): the file's primitives, the
// class's fit, the loads, the saved unit's kind and its Save
struct EEPlayerUnitContext {
    EEPlayer* mPlayer;
    FSFile* mFile;
    RTSUnit* mUnit;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
};

static int32_t EEPlayerUnitOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerUnitContext& c = *static_cast<EEPlayerUnitContext*>(inContext);
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(*c.mFile, inOp, io);
    EEUnitClass* theClass = (uint64_t)io->i < gUnitClassTable.size() ? gUnitClassTable[(size_t)io->i] : NULL;
    bool theComplex = io->b != 0;
    EEUnit* theUnit = static_cast<EEUnit*>(c.mUnit);
    long theResult;
    switch (inOp) {
    case EE_UNITS_PLAYER_SAVE_UNIT_FITS:
        return theClass != NULL && ((theClass->mUnitType != kUnitTypePathPoint && theClass->mIsComplex == 0 && !theComplex)
                                    || ((theClass->mUnitType == kUnitTypePathPoint || theClass->mIsComplex == 1) && theComplex));
    case EE_UNITS_PLAYER_SAVE_UNIT_LOAD:
        theUnit = theClass->CreateUnitAtTile(c.mPlayer, -10000, -10000, NULL, 3.1415927f);
        theResult = theUnit->Load(c.mMajorVersion, c.mMinorVersion, *c.mFile);
        if (theUnit->mIsQueuedForDelete)
            c.mPlayer->KillUnit(theUnit, true);
        return theResult;
    case EE_UNITS_PLAYER_SAVE_UNIT_STANDIN:   // (a unit of no class of this kind: its id kept, the unit dropped)
        theUnit = theComplex ? EEComplexUnit::Create() : EEUnit::Create();
        theResult = theUnit->Load(c.mMajorVersion, c.mMinorVersion, *c.mFile);
        c.mPlayer->mLoadedUnitIDs.push_back(theUnit->GetUnitID());
        theUnit->RepairPointers(c.mMajorVersion, c.mMinorVersion);
        theUnit->Clear();
        theUnit->Dereference();
        return theResult;
    case EE_UNITS_PLAYER_SAVE_UNIT_KIND:
        io->i = theUnit->mUnitClass->mUnitType;
        io->b = theUnit->mUnitClass->mUnitType == kUnitTypePathPoint;
        return theUnit->mComplexUnit != NULL;
    case EE_UNITS_PLAYER_SAVE_UNIT_SAVE:
        return theUnit->Save(c.mMajorVersion, c.mMinorVersion, *c.mFile);
    }
    return 0;
}

// Reads one unit from a saved game and creates it on the player (ee_units player_save.rs load_unit)
long EEPlayer::LoadUnit(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEPlayerUnitContext theContext = {this, &inFile, NULL, inMajorVersion, inMinorVersion};
    return ee_units_player_unit(&theContext, &EEPlayerUnitOperation, false, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Returns the player's name
const UWideString& EEPlayer::GetName()
{
    return this->mName;
}

// Returns the player's next complex unit after inUnit (the first one for NULL), or NULL at the end
EEComplexUnit* EEPlayer::GetNextComplexUnit(EEUnit* inUnit)
{
    EEComplexUnit* theResult = NULL;
    EEUnit* theUnit = inUnit;
    if (this->mLastUnit != NULL) {
        if (theUnit == NULL) {
            theUnit = this->mFirstUnit;
            theResult = theUnit->GetComplexUnit();
        }
        while (theUnit != NULL && theResult == NULL) {
            theUnit = static_cast<EEUnit*>(theUnit->mNext);
            if (theUnit != NULL)
                theResult = theUnit->GetComplexUnit();
        }
    }
    return theResult;
}

// Plays a unit's acknowledgement sound, at most once per delay for the same selection
// NEAR (71.9): registers only (the original caches TMGetTime in edi and the delay in ebx)
void EEPlayer::PlayAcknowledgement(EEComplexUnit* inUnit)
{
    if (this->mLastAcknowledgedGroup == this->mActiveGroup->mChangeTime) {
        unsigned long theDelay = gWorldTable.GetLongValue(kWorldAcknowledgementDelay);
        if (TMGetTime() - this->mLastAcknowledgementTime <= theDelay)
            return;
    } else
        this->mLastAcknowledgedGroup = this->mActiveGroup->mChangeTime;
    this->mLastAcknowledgementTime = TMGetTime();
    EEPlayForLocalPlayer(this, inUnit->GetStats()->GetAcknowledgeSound(-1));
}

// Replaces each swapped old unit by its new one in all units, air traffic, the selection, control groups, wonders
// Called when the physics replaced units (EEPhysics 0x1534d1); a new unit of another player is not added to
// the groups, and an unbuilt wall becomes a construction site
void EEPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    EEComplexUnit* theOld = NULL;
    EEComplexUnit* theNew = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    for (EEComplexUnit* theUnit = this->GetNextComplexUnit(NULL); theUnit != NULL; theUnit = this->GetNextComplexUnit(theUnit))
        theUnit->ReplaceUnits(ioSwaps);
    for (long a = 0; a < 4; a++)
        mAirTraffic[a]->ReplaceUnits(ioSwaps);
    for (std::vector<EEPUnitSwap>::iterator it = ioSwaps.begin(); it != ioSwaps.end(); it++) {
        theOld = it->mOld;
        theNew = it->mNew;
        if (theOld == NULL || theNew == NULL)
            continue;
        if (theGroup->Contains(theOld)) {
            theGroup->RemoveUnit(theOld);
            if (theOld->mPlayer == theNew->mPlayer)
                theGroup->Add(theNew);
            this->mSelectedGroup->CopySelection(this->mActiveGroup);
        }
        for (long g = 0; g < 20; g++) {
            if (!this->mGroups[g]->Contains(theOld))
                continue;
            this->mGroups[g]->RemoveUnit(theOld);
            if (theOld->mPlayer == theNew->mPlayer)
                this->mGroups[g]->Add(theNew);
        }
        if (this == gWorld.GetCurrentPlayer())
            gUserInterface->PostMessage424Once();
        if (theNew->mUnitClass->mIsWall && !theNew->mIsBuilt)
            this->AddConstructionSite(theNew);
        std::vector<EEComplexUnit*>::iterator theIterator;
        if (this->mWonders->Contains(theOld, theIterator))
            this->mWonders->Replace(theOld, theNew);
    }
}

// Makes every town of the player recheck whether it is near water
void EEPlayer::RestartTowns()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        if (this->mTowns[i] != NULL)
            this->mTowns[i]->UpdateNearWater();
    }
}

// Reveals a tile to the player and, with shared line of sight, to every ally that shares it too
void EEPlayer::AddSharedSight(EETile* inTile, long inX, long inY)
{
    if (inTile == NULL)
        return;
    if (inTile->GetVisibility(this->mPlayerID) == 0)
        this->mSavedGroups.Reset(0x18, 1);
    inTile->AddSight(this);
    gDoppelgangerManager.TileShown(inTile, inX, inY, this);
    if (this->mSharedLOS != true)
        return;
    for (long i = this->mAllies.size() - 1; i >= 0; i--) {
        if (this->mAllies[i]->mSharedLOS) {
            inTile->AddSight(this->mAllies[i]);
            gDoppelgangerManager.TileShown(inTile, inX, inY, this->mAllies[i]);
        }
    }
}

// Reveals the tile at a map location to the player and its sight-sharing allies
void EEPlayer::AddSharedSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddSharedSight(theTile, inLocation.GetX(), inLocation.GetY());
}

// Reveals a tile to the player only (a first sighting adds to statistics series 0x18)
void EEPlayer::AddOwnSight(EETile* inTile, long inX, long inY)
{
    if (inTile == NULL)
        return;
    if (inTile->GetVisibility(this->mPlayerID) == 0)
        this->mSavedGroups.Reset(0x18, 1);
    inTile->AddSight(this);
    gDoppelgangerManager.TileShown(inTile, inX, inY, this);
}

// Reveals the tile at a map location to the player only
void EEPlayer::AddOwnSightAt(const EETileLocation& inLocation)
{
    EETile* theTile = gWorld.mMap->GetTileAtLocation(inLocation);
    this->AddOwnSight(theTile, inLocation.GetX(), inLocation.GetY());
}

// Empties the player: running and control groups, viewed unit, events, towns, herds, off-map units, patrols
void EEPlayer::Clear()
{
    gDoppelgangerManager.UnregisterPlayer(this);
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long i = 0; i < theCount; i++) {
        this->mRunningGroups[i]->Clear();
        this->mRunningGroups[i]->Dereference();
    }
    this->mRunningGroups.clear();
    this->mActiveGroup->Clear();
    for (long j = 0; j < 20; j++)
        this->mGroups[j]->Clear();
    if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
    this->mEvents.Clear(this);
    EEDeletePointers(this->mTowns);
    this->mTownsByID.clear();
    EEDeletePointers(this->mHerds);
    this->ClearUnitsToRepatrol();
    EEDeletePointers(this->mUnitsOffMap);
    std::for_each(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), EEPDereferencer());
    this->mPatrolGroups.clear();
    RTSPlayer::Clear();
}

// Removes a player from the ally list
void EEPlayer::RemoveAlly(EEPlayer* inPlayer)
{
    for (unsigned long i = 0; i < this->mAllies.size(); i++) {
        if (this->mAllies[i] == inPlayer) {
            this->mAllies.erase(this->mAllies.begin() + i);
            break;
        }
    }
}

// Removes a media item from the player's loaded-media list; returns whether it was there
bool EEPlayer::RemoveLoadedMedia(EEMedia* inMedia)
{
    std::vector<EEMedia*>::iterator theIterator = std::find(this->mLoadedMedia.begin(), this->mLoadedMedia.end(), inMedia);
    bool theResult = false;
    if (theIterator != this->mLoadedMedia.end()) {
        this->mLoadedMedia.erase(theIterator);
        theResult = true;
    }
    return theResult;
}

// Stops sharing the player's building sight with another player, refreshing every land building's sight
void EEPlayer::RemoveSightSharer(EEPlayer* inPlayer)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 2, inPlayer, 0, false);
}

// Finishes running groups left without units, then releases and drops every finished group
void EEPlayer::RemovalsDone()
{
    EEGroup* theGroup;
    unsigned long theCount = this->mRunningGroups.size();
    for (unsigned long i = 0; i < theCount; i++) {
        theGroup = this->mRunningGroups[i];
        if (theGroup->GetNumUnits() == 0 && std::find(this->mFinishedGroups.begin(), this->mFinishedGroups.end(), theGroup) == this->mFinishedGroups.end()) {
            // Virtual04 copies these entries from mPendingGroups; these are EEActiveGroup objects.
            static_cast<EEActiveGroup*>(theGroup)->Finish();
        }
    }
    theCount = this->mFinishedGroups.size();
    for (unsigned long j = 0; j < theCount; j++) {
        this->mFinishedGroups[j]->Clear();
        this->mFinishedGroups[j]->Dereference();
        std::vector<EEGroup*>::iterator theIterator = std::find(this->mRunningGroups.begin(), this->mRunningGroups.end(), this->mFinishedGroups[j]);
        if (theIterator != this->mRunningGroups.end()) {
            (*theIterator)->Dereference();
            this->mRunningGroups.erase(theIterator);
        }
    }
    this->mFinishedGroups.clear();
}

// Cancels and refunds a queued improvement (inFlag: never the running one) (ee_units player.rs)
void EEPlayer::RemoveQueuedImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    EEPlayerEconScope theScope;
    EEPlayerImprovement(this, 6, inUnitType, inImprovement, inFlag);
    theScope.Record(this, 17, 0, {inUnitType, inImprovement, inFlag});
}

// Removes a patrol group from the player and releases it
void EEPlayer::RemovePatrolGroup(EEPPatrolGroup* inGroup)
{
    std::vector<EEPPatrolGroup*>::iterator theIterator = std::find(this->mPatrolGroups.begin(), this->mPatrolGroups.end(), inGroup);
    this->mPatrolGroups.erase(theIterator);
    inGroup->Dereference();
}

// Deselects the unit with the given ID, if it exists
void EEPlayer::RemoveUnitFromSelectedGroup(long inUnitID)
{
    RTSUnit* theUnit = ::gWorld.GetUnit(inUnitID);
    if (theUnit != NULL)
        this->DeselectUnit(static_cast<EEUnit*>(theUnit));
}

// Takes away the power of a lost wonder (conversion, hit-point bonus, shared bonus, enemy sight, healing ...)
void EEPlayer::RemoveWonderPower(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 10, inUnit);
}

// Removes an animal from its herds, deleting any herd left with no tiles
void EEPlayer::RemoveUnitFromHerd(EEComplexUnit* inUnit)
{
    ee_units_player_areas(EEPlayerRefs(this), 3, static_cast<RTSUnit*>(inUnit));
}

// Takes a unit being removed off its tile, its region and the influence map
void EEPlayer::RemovingUnit(RTSUnit* inUnit)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    theUnit->mUnitClass->RemoveFromTile(theUnit);
    if (theUnit->mTileNode == NULL)
        return;
    theUnit->mUnitClass->RemoveFromRegion(theUnit);
    if (theUnit->mUnitClass->mInfluence != 0)
        gWorld.mInfluenceTable->RemoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY);
}

// Deletes the off-map record of a unit, if it has one
void EEPlayer::RemoveOffMapUnit(RTSUnit* inUnit)
{
    if (inUnit == NULL)
        return;
    for (long i = this->mUnitsOffMap.size() - 1; i >= 0; i--) {
        if (inUnit != this->mUnitsOffMap[i]->mUnit)
            continue;
        EEWUnitsOffMapInfo* theInfo = this->mUnitsOffMap[i];
        this->mUnitsOffMap.erase(this->mUnitsOffMap.begin() + i);
        delete theInfo;
        break;
    }
}

// Removes a unit from the selection, or clears the viewed unit when that is the unit
void EEPlayer::DeselectUnit(EEUnit* inUnit)
{
    if (inUnit != this->mViewedUnit && inUnit->GetComplexUnit() != NULL) {
        this->mActiveGroup->RemoveUnit(inUnit->GetComplexUnit());
        this->mSelectedGroup->RemoveUnit(inUnit->GetComplexUnit());
    } else if (this->mViewedUnit != NULL) {
        this->mViewedUnit->Dereference();
        this->mViewedUnit = NULL;
    }
}

// Removes a town building from the towns containing it, deleting any town left with no tiles
void EEPlayer::RemoveUnitFromTown(EEComplexUnit* inUnit)
{
    ee_units_player_areas(EEPlayerRefs(this), 2, static_cast<RTSUnit*>(inUnit));
}

// Removes a unit from the player's construction sites
void EEPlayer::RemoveConstructionSite(EEComplexUnit* inUnit)
{
    this->mConstructionSites->Remove(inUnit);
}

// Removes a wonder from the player; when the last of its type goes, its wonder power is taken away
void EEPlayer::RemoveWonder(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 8, inUnit);
}

// Lets every unit of the player finish loading; returns 0
long EEPlayer::FinishUnitsLoad(long inMajorVersion, long inMinorVersion)
{
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
        theUnit->FinishLoad(inMajorVersion, inMinorVersion);
    return 0;
}

// port (save version 0.260): the towns' ids and order. A load rebuilds every town from its units (RepairPointers),
// numbering them on from the saved counter (++mTownIDCounter) in the load's order, so the towns of the loaded game had
// other ids than the saved game's, which the saved AI state names its towns by (unit requests, defense packages,
// attack sub groups: GetTownByID), and stood in another order in mTowns, which the AI walks. Saved: the id counter
// and per player each town's id, hull points, counters and flags in mTowns' order; a rebuilt town whose hull equals a
// saved one takes its id and its counters, and when every town is matched they take the saved order. From 0.268 the
// towns are made from their saved areas instead. The save, the load, the pending towns and the matching are Rust
// (ee_units town_save.rs) over EETownSaveHost.

// the counters a town's units summed as they came and went (the rebuild counts them once each, where the running
// game counted a farm at its placement and again at its completion)
static long* EETownCounter(EETown* inTown, long inIndex)
{
    long* theCounters[8] = {(long*)&inTown->mValue, &inTown->mNumWalls, &inTown->mL80, &inTown->mNumWonders,
                            &inTown->mNumDocks, &inTown->mNumAirports, &inTown->mNumUnits, &inTown->mReach};
    return theCounters[inIndex];
}

struct EETownSaveContext {
    FSFile* mFile;                      // the save's and the load's
    EEPlayer* mPlayer;                  // RepairPointers' player
    std::vector<unsigned char> mArea;   // EE_UNITS_TOWN_AREA's bytes
    std::vector<EETown*> mMade;         // EE_UNITS_TOWN_MAKE's towns
};

static int32_t EETownSaveHost(void* inContext, int32_t inOp, uint32_t inPlayer, uint32_t inIndex, EeUnitsTownIo* io)
{
    EETownSaveContext& theContext = *static_cast<EETownSaveContext*>(inContext);
    if (EESaveIsPrimitive(inOp))
        return EESavePrimitive(*theContext.mFile, inOp, &io->prim);
    EEPlayer* thePlayer = theContext.mPlayer != NULL ? theContext.mPlayer
                          : inPlayer < gWorld.mNumPlayers ? gWorld.mPlayers[inPlayer] : NULL;
    EETown* theTown = thePlayer != NULL && inIndex < thePlayer->mTowns.size() ? thePlayer->mTowns[inIndex] : NULL;
    switch (inOp) {
    case EE_UNITS_TOWN_COUNTER: io->prim.i = gWorld.mTownIDCounter; return 0;
    case EE_UNITS_TOWN_SET_COUNTER: gWorld.mTownIDCounter = (long)io->prim.i; return 0;
    case EE_UNITS_TOWN_PLAYERS: io->prim.i = gWorld.mNumPlayers; return 0;
    case EE_UNITS_TOWN_SLOTS: return thePlayer != NULL ? (int32_t)thePlayer->mTowns.size() : 0;
    case EE_UNITS_TOWN_GET:
        if (theTown == NULL)
            return 0;
        io->id = theTown->mTownID;
        io->points = theTown->GetHullPointCount();
        for (long c = 0; c < 8; c++)
            io->counters[c] = *EETownCounter(theTown, c);
        io->near_water = theTown->mNearWater;
        io->near_map_edge = theTown->mNearMapEdge;
        io->continent = theTown->mContinent;
        return 1;
    case EE_UNITS_TOWN_HULL_POINT:
        io->x = theTown->GetHullPoint((unsigned long)io->prim.i).GetX();
        io->y = theTown->GetHullPoint((unsigned long)io->prim.i).GetY();
        return 0;
    case EE_UNITS_TOWN_AREA:
        theContext.mArea.clear();
        EEWriteAreaBytes(*theTown, theContext.mArea);
        io->bytes = theContext.mArea.empty() ? NULL : &theContext.mArea[0];
        io->len = theContext.mArea.size();
        return 0;
    case EE_UNITS_TOWN_READ_BYTES: {
        unsigned long theRead = 0;
        long theResult = theContext.mFile->Read(io->bytes, (unsigned long)io->len, theRead);
        io->prim.i = theRead;
        return theResult;
    }
    case EE_UNITS_TOWN_WRITE_BYTES: {
        unsigned long theWritten = 0;
        return theContext.mFile->Write(io->bytes, (unsigned long)io->len, theWritten);
    }
    case EE_UNITS_TOWN_MAKE: {   // port (0.268): one town made from its saved area (new serials: AddUnitToTown's)
        EETown* theMade = new EETown(thePlayer, io->id);
        if (!EEReadAreaBytes(std::vector<unsigned char>(io->bytes, io->bytes + io->len), *theMade)) {
            delete theMade;
            return 0;
        }
        theMade->mContinent = (unsigned short)io->continent;
        for (long c = 0; c < 8; c++)
            *EETownCounter(theMade, c) = io->counters[c];
        theMade->mNearWater = io->near_water != 0;
        theMade->mNearMapEdge = io->near_map_edge != 0;
        theContext.mMade.push_back(theMade);
        return 1;
    }
    case EE_UNITS_TOWN_MAKE_DISCARD:
        for (size_t m = 0; m < theContext.mMade.size(); m++)
            delete theContext.mMade[m];
        theContext.mMade.clear();
        return 0;
    case EE_UNITS_TOWN_MAKE_COMMIT:
        for (size_t m = 0; m < theContext.mMade.size(); m++) {
            theContext.mMade[m]->SetSerial(++gContinentManager.mLastSerial);
            gContinentManager.AddArea(theContext.mMade[m]);
            thePlayer->mTownsByID.insert(std::pair<long, EETown*>(theContext.mMade[m]->mTownID, theContext.mMade[m]));
            thePlayer->mTowns.push_back(theContext.mMade[m]);
        }
        theContext.mMade.clear();
        return 0;
    case EE_UNITS_TOWN_SET_SAVED:
        theTown->mTownID = io->id;
        for (long c = 0; c < 8; c++)
            *EETownCounter(theTown, c) = io->counters[c];
        theTown->mNearWater = io->near_water != 0;
        theTown->mNearMapEdge = io->near_map_edge != 0;
        return 0;
    case EE_UNITS_TOWN_REINDEX:
        thePlayer->mTownsByID.clear();
        for (size_t t = 0; t < thePlayer->mTowns.size(); t++)
            if (thePlayer->mTowns[t] != NULL)
                thePlayer->mTownsByID.insert(std::pair<long, EETown*>(thePlayer->mTowns[t]->mTownID, thePlayer->mTowns[t]));
        return 0;
    case EE_UNITS_TOWN_REORDER: {
        const int32_t* theSlots = reinterpret_cast<const int32_t*>(io->bytes);
        std::vector<EETown*> theOrder;
        for (size_t s = 0; s < io->len; s++)
            theOrder.push_back(thePlayer->mTowns[theSlots[s]]);
        thePlayer->mTowns.assign(theOrder.begin(), theOrder.end());
        return 0;
    }
    case EE_UNITS_TOWN_DEBUG:
        ESDebugOut("%.*s", (int)io->len, reinterpret_cast<const char*>(io->bytes));
        return 0;
    }
    return 0;
}

long EESaveTowns(FSFile& outFile)
{
    EETownSaveContext theContext = {&outFile, NULL};
    return ee_units_towns_save(&theContext, &EETownSaveHost);
}

long EELoadTowns(FSFile& inFile, bool inAreas)
{
    EETownSaveContext theContext = {&inFile, NULL};
    uint8_t thePending = 0;
    long theResult = ee_units_towns_load(&theContext, &EETownSaveHost, inAreas, &thePending);
    if (thePending != 0)
        EEAfterLoad([]() { ee_units_towns_clear(); });   // the loaded game's own towns from here on
    return theResult;
}

// port (0.268): the player's towns made from the save, else false (rebuilt from the buildings); port (0.260): the
// rebuilt towns take their saved ids and order (ee_units town_save.rs make, apply)
static bool EEMakeSavedTowns(EEPlayer* inPlayer)
{
    EETownSaveContext theContext = {NULL, inPlayer};
    return ee_units_towns_repair(&theContext, &EETownSaveHost, (uint32_t)inPlayer->mPlayerID, false);
}

static void EEApplySavedTowns(EEPlayer* inPlayer)
{
    EETownSaveContext theContext = {NULL, inPlayer};
    ee_units_towns_repair(&theContext, &EETownSaveHost, (uint32_t)inPlayer->mPlayerID, true);
}

// port: a town rebuilt by a load of a 0.254 save takes back its saved patrol group, the town patrol with a point on
// one of the town's hull vertices, as it was (the original made the points again from the hull: other units, other
// ids; FindPatrolGroup cannot find the saved one yet, its points are not on their tiles before the load ends);
// returns whether one was found
bool EEPlayer::AdoptTownPatrol(EETown* inTown)
{
    for (unsigned long g = 0; g < this->mPatrolGroups.size(); g++) {
        EEPPatrolGroup* theGroup = this->mPatrolGroups[g];
        if (theGroup == NULL || !theGroup->mIsTownPatrol)
            continue;
        bool theTaken = false;
        for (unsigned long t = 0; t < this->mTowns.size() && !theTaken; t++)
            theTaken = this->mTowns[t] != NULL && this->mTowns[t]->mPatrolGroup == theGroup;
        if (theTaken)
            continue;
        for (unsigned long i = 0; i < theGroup->mPatrolPoints.size(); i++) {
            EEComplexUnit* thePoint = theGroup->mPatrolPoints[i];
            for (unsigned long v = 0; v < inTown->GetHullPointCount(); v++) {
                if (thePoint->GetTileX() == inTown->GetHullPoint(v).GetX()
                    && thePoint->GetTileY() == inTown->GetHullPoint(v).GetY()) {
                    inTown->mPatrolGroup = theGroup;
                    theGroup->Reference();
                    return true;
                }
            }
        }
    }
    return false;
}

// Relinks the player after a load: towns, construction sites, wonders, groups, herds, events, queued improvements
// and off-map units are rebuilt from saved IDs; returns the status of the last step
// NEAR: inline budget (the original calls begin() out of line for mPatrolGroups[i] and mConstructionSiteIDs[j]), register allocation
// (the GetAvailablePoints test is != 0 here; the original compares unsigned, jbe)
long EEPlayer::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult;
    unsigned long i;
    this->mCivilization.Reapply(this, inFirst, inSecond);
    RTSPlayer::RepairPointers(inFirst, inSecond);
    for (i = 0; i < this->mPatrolGroups.size(); i++)
        this->mPatrolGroups[i]->RepairPointers(inFirst, inSecond);
    for (long theAirport = 0; theAirport < 4; theAirport++)
        theResult = mAirTraffic[theAirport]->RepairPointers(inFirst, inSecond);
    this->mLoadedUnitIDs.clear();
    // port (0.268): the towns as saved (EEMakeSavedTowns), else rebuilt from the buildings
    if (this->GetPlayerID() >= 1 && !EEMakeSavedTowns(this)) {
        // port (0.254 on, no format change): a building placed as a foundation joins its town when it is finished
        // (UnitPlaced skips a foundation, UnitCompleted adds it; a farm is never a foundation), so the rebuilt towns
        // leave the unfinished buildings out as the running game had them (seed 4's 600 s save: a town of the loaded
        // game took a building under construction, grew a hull and made patrol points the saved game did not have)
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetComplexUnit() != NULL
                && !(EESaveHasLiveState(inFirst, inSecond) && theUnit->mUnitClass->IsLandBuilding()
                     && !theUnit->GetComplexUnit()->mIsBuilt && theUnit->mUnitClass->mUnitType != kUnitTypeFarm))
                this->AddUnitToTown(theUnit->GetComplexUnit());
    }
    if (!this->mCivilization.GetNoPointsAdded() && this->mCivilization.GetAvailablePoints() != 0)
        theResult = EESetCivBuilderButtonBlinking(true);
    unsigned long j;
    for (j = 0; j < this->mConstructionSiteIDs.size(); j++) {
        RTSUnit* theUnit = gWorld.GetUnit(this->mConstructionSiteIDs[j]);
        if (theUnit != NULL && theUnit->mTileNode != NULL)
            this->mConstructionSites->Add(static_cast<EEUnit*>(theUnit)->mComplexUnit);
    }
    this->mConstructionSiteIDs.clear();
    for (j = 0; j < this->mWonderIDs.size(); j++) {
        RTSUnit* theUnit = gWorld.GetUnit(this->mWonderIDs[j]);
        if (theUnit != NULL && theUnit->mTileNode != NULL)
            this->mWonders->Add(static_cast<EEUnit*>(theUnit)->mComplexUnit);
    }
    this->mWonderIDs.clear();
    this->RecountPopulation();
    if (theResult == kErrorOK)
        this->mActiveGroup->RepairPointers(inFirst, inSecond);
    for (long theGroup = 0; theGroup < 20; theGroup++)
        this->mGroups[theGroup]->RepairPointers(inFirst, inSecond);
    for (unsigned long theHerd = 0; theHerd < this->mHerds.size(); theHerd++)
        this->mHerds[theHerd]->RepairPointers(inFirst, inSecond);
    this->RegisterHerds();
    theResult = this->mEvents.RepairEvents(this, inFirst, inSecond);
    EEPlayerImprovement(this, 10, 0, 0);   // the queued improvements queued again (ee_units player.rs requeue)
    this->mSelectedGroup->CopySelection(this->mActiveGroup);
    this->GetTechTree()->Get(kTechCapitol)->mIsAllowed = this->mNumCapitols == 0;
    if (this->GetTechTree()->Get(kTechSettlement)->GetIsObsolete())
        this->GetTechTree()->Get(kTechTownCenter)->mIsAllowed = this->mNumCapitols > 0;
    else {
        this->GetTechTree()->Get(kTechSettlement)->mIsAllowed = this->mNumCapitols > 0;
        this->GetTechTree()->Get(kTechTownCenter)->mIsAllowed = false;
    }
    EEApplySavedTowns(this);   // port (0.260): the rebuilt towns take their saved ids and order
    for (long theTown = this->mTowns.size() - 1; theTown >= 0; theTown--)
        if (this->mTowns[theTown] != NULL && this->mTowns[theTown]->mPatrolGroup == NULL
            && !(EESaveHasLiveState(inFirst, inSecond) && this->AdoptTownPatrol(this->mTowns[theTown])))
            this->mTowns[theTown]->RefreshPatrolGroup();
    if (inFirst == 0 && inSecond < 0xd7) {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->GetTileX() == -10000)
                this->AddOffMapUnit(theUnit);
    } else {
        for (long theInfo = this->mUnitsOffMap.size() - 1; theInfo >= 0; theInfo--) {
            this->mUnitsOffMap[theInfo]->RepairPointers(inFirst, inSecond);
            EEWUnitsOffMapInfo* theOffMap = this->mUnitsOffMap[theInfo];
            if (theOffMap->mUnit == NULL) {
                this->mUnitsOffMap.erase(this->mUnitsOffMap.begin() + theInfo);
                delete theOffMap;
            }
        }
    }
    this->RecountHeroesAndTeleporters();
    return theResult;
}

// When a human player reaches a valid epoch, lets every computer player catch up to it
void EEPlayer::EpochReached(long inEpoch)
{
    if (!this->IsHuman() || !EEIsEpochID(inEpoch))
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* theOther = gWorld.mPlayers[i];
        EEComputerPlayer* thePlayer = dynamic_cast<EEComputerPlayer*>(theOther);
        if (thePlayer != NULL)
            thePlayer->CatchUpEpoch(this);
    }
}

// Starts the next queued improvement's timer; true when the queue is empty (ee_units player.rs advance)
bool EEPlayer::AdvanceImprovement(unsigned long inUnitType)
{
    EEPlayerEconScope theScope;
    return theScope.Record(this, 15, EEPlayerImprovement(this, 7, inUnitType, 0) != 0, {(int32_t)inUnitType});
}

// Resets the player's tech tree to the global one and clears the population bonus and tribute fee
void EEPlayer::ResetTechTree()
{
    *this->mTechnologies = gTechTree;
    this->mTechnologies->SetPlayerIndex(this->mPlayerID);
    this->mTechnologies->SetPlayer(this);
    this->mBB44 = false;
    this->mPopulationLimitBonus = 0;
    this->mTributeFee = EE_UNITS_PLAYER_TRIBUTE_FEE;
}

#include "lle/FSMemoryFile.h"
#include "ee/EESaveGameFiles.h"

// Rebuilds the player's tech tree and upgrades from scratch, keeping its state and epoch range, and unloads the
// unit media its nodes had loaded
void EEPlayer::ReloadTechTree(bool inFlag)
{
    std::vector<long> theMedia;
    std::vector<long> theIDs;
    long theStartEpoch = this->GetTechTree()->mStartEpoch;
    long theEndEpoch = this->GetTechTree()->mEndEpoch;
    FSMemoryFile theFile(1000, true, 1);
    this->GetTechTree()->GetNodeIDs(theIDs);
    for (unsigned long i = 0; i < theIDs.size(); i++) {
        EETechTreeNode* theNode = this->GetTechTree()->Get(theIDs[i]);
        if (theNode != NULL && theNode->mIsUnitMediaLoaded == true) {
            long theType = theNode->mData->mUnitClass;
            theMedia.push_back(theType);
        }
    }
    this->GetTechTree()->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
    gWorld.RemoveShareBonus();
    this->ResetTechTree();
    this->RefreshUpgrades();
    theFile.SetPosition(0, kFileOriginBegin, false);
    if (this->GetTechTree()->Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile) == kErrorOK) {
        this->GetTechTree()->ResetEpochRange(theStartEpoch, theEndEpoch);
        this->GetTechTree()->ApplySavedState(true, !inFlag);
    }
    for (unsigned long j = 0; j < theMedia.size(); j++)
        this->UnloadUnitClassMedia(theMedia[j]);
}

// Recomputes every unit class upgrade and rescales the hit points, flight time and area effects of existing units
void EEPlayer::RefreshUpgrades()
{
    for (unsigned long i = 0; i < EEUnitClassCount(); i++) {
        EEUnitClassUpgrade* theUpgrade = this->GetUpgrade(i);
        if (theUpgrade == NULL)
            continue;
        long theOldValue = theUpgrade->mFlightTime;
        long theOldHitPoints = theUpgrade->GetMaxHitPoints();
        long theOldKind = theUpgrade->mAreaEffectTemplates[0];
        theUpgrade->Init();
        if (theOldHitPoints == theUpgrade->mMaxHitPoints
            && theOldValue == theUpgrade->mFlightTime && theOldKind == theUpgrade->mAreaEffectTemplates[0])
            continue;
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mUnitClass->GetUnitType() != theUpgrade->mUnitClass->GetUnitType())
                continue;
            EEComplexUnit* theComplex = theUnit->GetComplexUnit();
            long theNewHitPoints = theUpgrade->mMaxHitPoints;
            if (theOldHitPoints != theNewHitPoints && (theComplex == NULL || theComplex->GetIsBuilt() == true)) {
                float theScaled = ((float)theUnit->GetHitPoints() / theOldHitPoints) * theNewHitPoints;
                float theHitPoints = (float)UMCeil(theScaled);
                if ((long)theHitPoints > 0)
                    theUnit->SetHitPoints((long)theHitPoints);
            }
            if (theComplex == NULL)
                continue;
            long theNewValue = theUpgrade->mFlightTime;
            if (theOldValue != theNewValue) {
                float theScaled = ((float)theComplex->mFlightTime / theOldValue) * theNewValue;
                float theValue = (float)UMCeil(theScaled);
                if ((long)theValue > 0)
                    theComplex->SetFlightTime((long)theValue);
            }
            if (theOldKind != theUpgrade->mAreaEffectTemplates[0])
                theComplex->MakeAreaEffects();
        }
    }
    this->mCivilization.AddUpgrades(this->mCivilization.GetPurchasedUpgrades(), this->GetPlayerID(), false);
}

// Briefly reveals the area around one unit (by ID) or all units of a type, with the given sight range
void EEPlayer::PulseSight(bool inByID, float inSight, long inID)
{
    std::vector<EEUnit*> theUnits;
    EEUnit* theUnit;
    if (inByID) {
        if (gWorld.GetUnit(inID) != NULL) {
            EEUnit* theUnit = gWorld.GetUnit(inID);
            theUnits.push_back(theUnit);
        }
    } else {
        for (theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mTileNode != NULL && theUnit->mUnitClass->mUnitType == inID)
                theUnits.push_back(theUnit);
    }
    for (unsigned long i = 0; i < theUnits.size(); i++) {
        theUnit = theUnits[i];
        float theLineOfSight = theUnit->GetStats()->GetLineOfSight();
        float theSight = inSight - theLineOfSight;
        theUnit->GetComplexUnit()->SightOffHere();
        theUnit->GetComplexUnit()->SetSight(theSight);
        theUnit->SightOnHere();
        theUnit->GetComplexUnit()->SightOffHere();
        theUnit->GetComplexUnit()->mSight = 0;
        theUnit->SightOnHere();
    }
}

// Writes the player's state to a saved game; returns the write status (some writes' errors are ignored).
// port (R5b.4): the body is Rust (ee_units player_save.rs `save`) over the record this class derives from and
// EEPlayerSaveOperation's parts
long EEPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEPlayerSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    return ee_units_player_save(&theContext, &EEPlayerSaveOperation, static_cast<EeUnitsPlayerSaved*>(this), this->mQueued,
                                (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Writes one unit's type, kind and data to a saved game (ee_units player_save.rs save_unit; path points saved as units
// from 0.254)
long EEPlayer::SaveUnit(RTSUnit* inUnit, unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEPlayerUnitContext theContext = {this, &outFile, inUnit, inMajorVersion, inMinorVersion};
    return ee_units_player_unit(&theContext, &EEPlayerUnitOperation, true, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// Selects the unit with the given ID (optionally clearing the selection), playing its sound for own units
void EEPlayer::SelectUnit(unsigned long inUnitID, bool inArg1, bool inNewGroup)
{
    EEUnit* theUnit = static_cast<EEUnit*>(::gWorld.GetUnit(inUnitID));
    this->UpdateSelection(NULL, true);
    if (inNewGroup)
        this->mActiveGroup->Clear();
    if (theUnit == NULL || !theUnit->mUnitClass->IsSelectable(theUnit))
        return;
    unsigned long theType = theUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypePathPoint)
        this->TrySelectUnit(theUnit, inArg1);
    else if (theType != kUnitTypeVolcano) {
        if (theUnit->mHitPoints > 0
            && (!inArg1 || this->mSeesAll || theUnit->mPlayer == this
                || !gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, this->GetPlayerID()))
            && this->TrySelectUnit(theUnit, inArg1)) {
            EEPlayer* thePlayer = theUnit->mPlayer;
            if (thePlayer == this || thePlayer->mPlayerID == 0) {
                if (gWorld.mExecutingLocalSelection && thePlayer == this)
                    EEPlayForLocalPlayer(this, theUnit->mStats->GetSelectionSound(-1));
                return;
            }
        }
        if (theUnit->mResourceAmount > 0 && theUnit->mUnitClass->mUnitType != kUnitTypeFarm
            && !gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, this->GetPlayerID()))
            this->TrySelectUnit(theUnit, inArg1);
    }
}

// Replaces the selection with the built units of the given IDs and plays the last one's selection sound
void EEPlayer::SelectUnits(std::vector<long>& inUnitIDs)
{
    EEComplexUnit* theLastUnit = NULL;
    EESelectedGroup* theGroup = this->mActiveGroup;
    this->UpdateSelection(NULL, true);
    theGroup->Clear();
    // New (R5b.4): the pick (ee/EEUnitStore.h EESelectPick); the group takes each picked unit (Add skips an invalid
    // one), the last one picked plays its sound. The ids are the selection's 32-bit values (the original's long)
    std::vector<uint32_t> theIDs(inUnitIDs.size() + 1);
    for (size_t i = 0; i < inUnitIDs.size(); i++)
        theIDs[i] = static_cast<uint32_t>(inUnitIDs[i]);
    std::vector<uint32_t> thePicked(inUnitIDs.size() + 1);
    size_t thePickedCount = EESelectPick(this, &theIDs[0], inUnitIDs.size(), &thePicked[0]);
    EESelectPickCapture(100, this, &theIDs[0], inUnitIDs.size(), &thePicked[0], thePickedCount);   // TEST ONLY (off by default)
    for (size_t i = 0; i < thePickedCount; i++) {
        EEComplexUnit* theComplexUnit = static_cast<EEUnit*>(this->GetUnit(thePicked[i]))->mComplexUnit;
        theGroup->Add(theComplexUnit);
        theLastUnit = theComplexUnit;
    }
    if (theLastUnit != NULL && gWorld.mExecutingLocalSelection)
        EEPlayForLocalPlayer(this, theLastUnit->mStats->GetSelectionSound(-1));
}

// Moves movable units out of a new building's footprint, onto a waypoint outside it or the nearest free tile
void EEPlayer::ClearFootprint(EEComplexUnit* inUnit)
{
    GE3DPoint theWaypoint;
    GERectangle theBounds;
    GE2DPoint thePoint;
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    long theSize = inUnit->mUnitClass->mTileSize;
    long theType = inUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypeRoboticFarm || gUnitSetManager.Contains(theType, 0x4c))
        return;
    long theRight = theSize - 1 + theX;
    theBounds.mBottom = (float)(theSize - 1 + theY);
    theBounds.mTop = (float)theY;
    theBounds.mLeft = (float)theX;
    theBounds.mRight = (float)theRight;
    EEGetUnitsInRectangle(theBounds, theUnits);
    unsigned long theCount = theUnits.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i]->mComplexUnit;
        if (theUnit == NULL || theUnit == inUnit || !EEIsUnitMovable(inUnit, theUnit))
            continue;
        bool theMoved = false;
        long thePathCount = theUnit->mWaypoints.size();
        if (thePathCount > 0) {
            while (theUnit->NextWaypoint() == 0) {
                float thePathArg;
                if (theUnit->GetPathStart(theWaypoint, thePathArg) != 0)
                    continue;
                thePoint.mX = theWaypoint.GetX();
                thePoint.mY = theWaypoint.GetY();
                if (theBounds.Contains(thePoint))
                    continue;
                GETransformation theLocation(false);
                theLocation = theUnit->mPhysicalModel.GetLocation();
                theLocation.SetPosition(theWaypoint.GetX(), theWaypoint.GetY(), theWaypoint.mZ);
                theUnit->SetTransformation(theLocation, true);
                theMoved = true;
                break;
            }
        }
        if (theMoved)
            continue;
        EEGetSquareBlockRing(0, theX, theY, theSize, theTiles, false);
        std::sort(theTiles.begin(), theTiles.end(), EETileDistanceLess(theRight, theY));
        unsigned long theTileCount = theTiles.size();
        for (unsigned long j = 0; j < theTileCount; j++) {
            EETile* theTile = gWorld.mMap->GetTile(theTiles[j].mX,
                                                 (*(theTiles.begin() + j)).mY);
            if (theTile == NULL || theTile->mUnits != NULL ||
                !EECanStandOn(theTiles[j], theUnit))
                continue;
            GETransformation theLocation(false);
            theLocation = theUnit->mPhysicalModel.GetLocation();
            float theZ = theLocation.GetZ();
            float theNewX = (float)(theTiles[j]).mX + 0.5f;
            float theNewY = (float)(theTiles[j]).mY + 0.5f;
            if (gWorld.mMap->mTerrainMesh.GetIsValidTile(theTiles[j].mX,
                                                      (*(theTiles.begin() + j)).mY))
                theZ = gWorld.mMap->mTerrainMesh.GetMeshZ(theNewX, theNewY);
            theLocation.SetPosition(theNewX, theNewY, theZ);
            theUnit->SetTransformation(theLocation, true);
            theUnit->DropPath();
            theUnit->StartAction(kActionIdle);
            theUnit->mUnitClass->ActionChanged(theUnit);
        }
    }
}

// Sets every resource amount of the player to the same value
void EEPlayer::SetAllResources(long inValue)
{
    for (long i = 2; i < 7; i++)
        this->mResourceCaps[i] = inValue;
}

// Sets the player's civilization points (current and maximum) for an epoch
void EEPlayer::SetCivilizationPoints(long inEpoch)
{
    this->mCivilization.SetPointsAndMax(inEpoch);
}

// Changes the player's stance toward another player, forming or breaking the alliance and its shared sight
void EEPlayer::SetDiplomaticStance(EEPlayer* inOther, long inStance, EEMap* inMap)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 0, inOther, (unsigned long)inStance, inMap != NULL);
}

// Sets the player's current epoch in its tech tree
void EEPlayer::SetEpoch(long inEpoch)
{
    this->mTechnologies->SetEpoch(inEpoch);
}

// Sets the viewed unit of the selection, moving the reference
void EEPlayer::SetSelectedViewedUnit(RTSUnit* inUnit)
{
    if (this->mSelectedViewedUnit != NULL)
        this->mSelectedViewedUnit->Dereference();
    this->mSelectedViewedUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// Sets the viewed unit, moving the reference; with a unit and inFlag the selection is cleared first
void EEPlayer::UpdateSelection(EEUnit* inUnit, bool inFlag)
{
    if (inUnit != NULL && inFlag == true)
        this->mActiveGroup->Clear();
    if (this->mViewedUnit != NULL)
        this->mViewedUnit->Dereference();
    this->mViewedUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// Sets the player's override unit class
void EEPlayer::SetHackUnitClass(long inUnitClass)
{
    this->mHackUnitClass = inUnitClass;
}

// Turns the player's shared line of sight on or off, recomputing all its units' sight and events
void EEPlayer::SetSharedLOS(bool inFlag, EEMap* inArg)
{
    ee_units_player_diplomacy(EEPlayerRefs(this), 3, NULL, 0, inFlag);
}

// Copies the player's visibility of every tile into the terrain mesh
void EEPlayer::RefreshTerrainVisibility()
{
    EEMap* theMap = gWorld.mMap;
    unsigned long theSize = theMap->mSize;
    for (unsigned long x = 0; x < theSize; x++) {
        for (unsigned long y = 0; y < theSize; y++) {
            RTSTile* theTile = theMap->GetTile(x, y);
            if (theTile)
                theMap->mTerrainMesh.SetTileReferenceCount(x, y, theTile->GetVisibility(this->mPlayerID));
        }
    }
}

// Sets the player's game result; a new victory or defeat of another player is announced in the chat log
void EEPlayer::SetGameResult(long inState)
{
    // TEST ONLY: the headless simulation never defeats the idle local human (platform/PlatSim.h)
    if (gPlatSimNoHumanDefeat && inState == 2 && gWorld.mPlayers[gWorld.mCurrentPlayer] == this) {
        gPlatSimNoHumanDefeatHits++;
        return;
    }
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theText;
    UWideString theReason;
    UWideString theTitle;
    UWideString thePlayerName;
    if (inState == 1) {
        if (this->mGameResult == 1)
            goto SaveState;
        gResource->GetString(IDS_PLAYER_IS_VICTORIOUS, theReason);
    } else {
        if (inState != 2 || this->mGameResult == 2)
            goto SaveState;
        gResource->GetString(IDS_PLAYER_HAS_BEEN_DEFEATED, theReason);
    }
    gResource->GetString(IDS_MSG_GAME, theTitle);
    if (this != gWorld.GetCurrentPlayer()) {
        if (this->IsHuman())
            sprintf(theText, theReason, (const char*)this->mName);
        else {
            sprintf(thePlayerName, "%s - (%s)", (const char*)this->mCivilization.mName, (const char*)this->mName);
            sprintf(theText, theReason, (const char*)thePlayerName);
        }
        gUserInterface->AddChatLogEntry(theColor, theTitle, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
    }
SaveState:
    this->mGameResult = inState;
}

// Subtracts an amount from one of the player's resources
void EEPlayer::TakeResource(long inIndex, long inAmount)
{
    this->mResourceCaps[inIndex] -= inAmount;
}

// Pays a technology's cost scaled by inFraction (ee_units player.rs pay)
void EEPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    EEPlayerEconScope theScope;
    ee_units_player_cost(EEPlayerRefs(this), 4, inTechnology, inUnitType, 0, inFraction, NULL);
    theScope.Record(this, 6, 0, {EEPlayerNodeID(inTechnology), inUnitType, EEPlayerFloatBits(inFraction)});
}

// Pays a unit class improvement at its queued level (ee_units player.rs pay_improvement)
void EEPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{
    EEPlayerEconScope theScope;
    if (inClass != NULL)
        EEPlayerImprovement(this, 8, inClass->mUnitType, inImprovement);
    theScope.Record(this, 7, 0, {inClass != NULL ? (int32_t)inClass->mUnitType : -1, inImprovement});
}

// VirtualBC's functor (RTTI EEPSuggestRecomputeAllAreaEffects; vtable 0x442b08: the shared ??_G 0x16b555, its
// Virtual04 0x16b543, the base's empty Virtual08): every area effect of each complex unit recomputed
class EEPSuggestRecomputeAllAreaEffects : public EEUUUnitFunctor {
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAreaEffects.MarkDirty(); }
};

// Marks the area effects of every unit of the player for recomputation
void EEPlayer::SuggestRecomputeAllAreaEffects()
{
    EEPSuggestRecomputeAllAreaEffects theFunctor;
    EEForEachUnitOf(this, &theFunctor, false);
}

// Unloads every media item the player loaded and empties the list
void EEPlayer::UnloadAllMedia()
{
    for (long i = this->mLoadedMedia.size() - 1; i >= 0; i--)
        this->mLoadedMedia[i]->UnloadMedia();
    this->mLoadedMedia.clear();
}

// Unloads the class media of every unit class recorded as loaded and empties the record
void EEPlayer::UnloadAllClassMedia()
{
    for (unsigned long i = 0; i < this->mSoundLoadedClasses.size(); i++)
        gUnitClassTable[this->mSoundLoadedClasses[i]]->UnloadClassMedia();
    this->mSoundLoadedClasses.clear();
}

// Unloads the command buttons of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassButtons(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->UnloadButtons(this);
}

// Unloads the media of one unit class for the player, if the class index is valid
void EEPlayer::UnloadUnitClassMedia(long inUnitClass)
{
    if (EEUnitStats* theStats = EEPlayerClassStats(this, inUnitClass))
        theStats->UnloadMedia(this);
}

// Updates the player's four air traffic controllers
void EEPlayer::UpdateAirTraffic()
{
    for (long i = 0; i < 4; i++)
        mAirTraffic[i]->Update();
}

// Raises (or with inFlag lowers) a unit class improvement and rescales existing units' power, hit points or sight
void EEPlayer::ApplyImprovement(long inUnitType, long inImprovement, bool inFlag)
{
    EEPlayerEconScope theScope;
    ee_units_player_apply_improvement(EEPlayerRefs(this), inUnitType, inImprovement, inFlag);
    theScope.Record(this, 22, 0, {inUnitType, inImprovement, inFlag});
}

// Registers a unit placed in the game: wonder/teleporter counts, town, herd, sight power, walls
void EEPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EEPlayerUnits(this, 1, inUnit);
}

// Unregisters a unit leaving the player: counts, wonders, town, herd, construction site, heroes, capitol, walls/forest
// EEUnit::DepleteResource passes (this, 0); inArg is not read. Losing the last capitol re-enables the
// capitol technology and blocks the JIT-manufacturing ones
void EEPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    EEPlayerUnits(this, 2, inUnit, inSwapped);
}

#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"

// Reattaches a unit to the AI and drops it from its group when it has no further goals or builds
void EEPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI)
        inUnit->mIsDetachedFromAI = false;
    if (inUnit->mGoalQueues[0].mGoals.size() <= 1 && inUnit->mCitizenBuildQueue.size() == 0 && inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->RemoveUnit(inUnit);
}

// Returns whether a unit ID is in the list of IDs loaded into discarded units
bool EEPlayer::HasLoadedUnitID(unsigned long inUnitID)
{
    std::vector<unsigned long>::iterator theIterator = std::find(this->mLoadedUnitIDs.begin(), this->mLoadedUnitIDs.end(), inUnitID);
    return theIterator != this->mLoadedUnitIDs.end();
}

// Clears the viewed unit and the selection
void EEPlayer::UnselectAllUnits()
{
    this->UpdateSelection(NULL, true);
    this->mActiveGroup->Clear();
}

#include "ee/EEActiveGroup.h"             // EEActiveGroup::ClearGoal (inline)

// Adds a town building to a town: a new one, the only fitting one, or the largest fitting one after merging
// the others into it (their patrols move over); returns whether it was added
bool EEPlayer::AddUnitToTown(EEComplexUnit* inUnit)
{
    return ee_units_player_areas(EEPlayerRefs(this), 0, static_cast<RTSUnit*>(inUnit));
}

// Sends each enabled unit waiting to repatrol to the nearest point of its nearest town's patrol, then clears the list
void EEPlayer::RestorePatrols()
{
    for (long i = this->mUnitsToRepatrol.size() - 1; i >= 0; i--) {
        if (this->mUnitsToRepatrol[i]->mIsDisabled)
            continue;
        EETown* theTown = this->mUnitsToRepatrol[i]->FindNearestTown();
        if (theTown == NULL || theTown->mPatrolGroup == NULL)
            continue;
        EEComplexUnit* thePoint = theTown->mPatrolGroup->GetNearest(this->mUnitsToRepatrol[i]->mTileX, (*(this->mUnitsToRepatrol.begin() + i))->mTileY);
        if (thePoint == NULL || thePoint->mPatrolGroup == NULL)
            continue;
        EEPPatrolGroup* thePatrol = thePoint->mPatrolGroup;
        EEActiveGroup* theGroup = new EEActiveGroup(this);
        theGroup->Add(*(this->mUnitsToRepatrol.begin() + i));
        EEGroup* theBase = theGroup;
        this->mPendingGroups.push_back(theBase);
        theGroup->KeepMobileUnits();
        EEUGRust* theGoal = EEUGRust::NewPatrol(thePatrol, thePoint, true);
        theGroup->ClearGoal();
        theGroup->SetGoal(theGoal, false);
        theGroup->mAction = kActionMoveToNextPatrolPoint;
        theGroup->Plan();
    }
    this->ClearUnitsToRepatrol();
}

// Gives every town without a patrol group a new one
void EEPlayer::RefreshTownGroups()
{
    for (long i = this->mTowns.size() - 1; i >= 0; i--) {
        EETown* theTown = this->mTowns[i];
        if (theTown != NULL && theTown->mPatrolGroup == NULL)
            theTown->RefreshPatrolGroup();
    }
}

// The context of UpdatePlayer's operations: the player
struct EEPlayerTickContext {
    EEPlayer* mPlayer;
};

// The C++ parts of the player's tick (ee_units player_tick.rs): the groups, herds, air traffic, events, the queued
// improvements' map and the units, by operation
static int32_t EEPlayerTickOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EEPlayerTickContext& theContext = *static_cast<EEPlayerTickContext*>(inContext);
    EEPlayer* thePlayer = theContext.mPlayer;
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(static_cast<uintptr_t>(io->i));
    switch (inOp) {
    case EE_UNITS_PLAYER_TICK_VALIDATE:
        if (thePlayer->mViewedUnit != NULL && thePlayer->mViewedUnit->mReferenceCount == 1)
            thePlayer->UpdateSelection(NULL, true);
        if (thePlayer->mSelectedViewedUnit != NULL && thePlayer->mSelectedViewedUnit->mReferenceCount == 1)
            thePlayer->SetSelectedViewedUnit(NULL);
        thePlayer->mActiveGroup->Validate();
        thePlayer->mSelectedGroup->Validate();
        for (long theGroup = 0; theGroup < 20; theGroup++)
            thePlayer->mGroups[theGroup]->Validate();
        for (unsigned long i = 0, theCount = thePlayer->GetNumRunningGroups(); i < theCount; i++)
            thePlayer->mRunningGroups[i]->Validate();
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_COUNT:
        return (int32_t)thePlayer->mHerds.size();
    case EE_UNITS_PLAYER_TICK_GAME_TIME:
        io->i = (int64_t)gPhysics->mGameTime;
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_DUE:
        io->i = (int64_t)thePlayer->mHerds[inIndex]->mNextUpdateTime;
        io->b = thePlayer->mHerds[inIndex]->mIsActive ? 1 : 0;
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_UPDATE:
        thePlayer->mHerds[inIndex]->Update((unsigned long)io->i);
        return 0;
    case EE_UNITS_PLAYER_TICK_HERD_STATE:
        return (int32_t)thePlayer->mHerds[inIndex]->mState;
    case EE_UNITS_PLAYER_TICK_HERD_KILL:
        thePlayer->KillHerdUnits(thePlayer->mHerds[inIndex], 0);
        return 0;
    case EE_UNITS_PLAYER_TICK_AIR_AND_EVENTS:
        thePlayer->UpdateAirTraffic();
        thePlayer->mEvents.TickEvents(thePlayer, gPhysics->mTickTime);
        return 0;
    case EE_UNITS_PLAYER_TICK_TICK_TIME:
        io->f = gPhysics->GetTickTime();
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_APPLY:
        thePlayer->ApplyImprovement((long)io->i, (long)inIndex, false);
        return 0;
    case EE_UNITS_PLAYER_TICK_QUEUED_ADVANCE:
        return thePlayer->AdvanceImprovement((unsigned long)io->i) ? 1 : 0;
    case EE_UNITS_PLAYER_TICK_TICK_MS:
        io->i = (int64_t)gPhysics->mTickTimeMs;
        return 0;
    case EE_UNITS_PLAYER_TICK_HEAL_VALUE:
        if (inIndex == 0)
            io->i = gWorldTable.GetLongValue(kWorldWonderHealAmount);
        else if (inIndex == 1)
            io->i = thePlayer->mTechnologies->mCurrentEpoch;
        else if (inIndex == 2)
            io->i = gWorldTable.GetLongValue(kWorldWonderHealEpochStep);
        else
            io->f = gWorldTable.GetFloatValue(kWorldWonderHealEpochFactor);
        return 0;
    case EE_UNITS_PLAYER_TICK_UNIT_NEXT:
        theUnit = theUnit == NULL ? thePlayer->mFirstUnit : static_cast<EEUnit*>(theUnit->mNext);
        io->i = (int64_t)reinterpret_cast<uintptr_t>(theUnit);
        return theUnit != NULL && thePlayer->mHealedUnitSet->Contains(theUnit->mUnitClass->mUnitType) ? 1 : 0;
    case EE_UNITS_PLAYER_TICK_UNIT_HIT_POINTS:
        return inIndex == 0 ? (int32_t)theUnit->mHitPoints : (int32_t)theUnit->mStats->mMaxHitPoints;
    case EE_UNITS_PLAYER_TICK_UNIT_SET_HIT_POINTS:
        theUnit->SetHitPoints((long)(int32_t)inIndex);
        return 0;
    }
    return 0;
}

// Updates the player each tick: validates groups, herds, air traffic, events, improvement timers and wonder healing
// port (R5b.4): the body is Rust (ee_units player_tick.rs: the herds' and timers' decisions, the heal) over the
// record this class derives from and EEPlayerTickOperation's parts
void EEPlayer::UpdatePlayer()
{
    EEPlayerCaptureState(this);   // TEST ONLY (PLST)
    EEPlayerTickContext theContext = {this};
    ee_units_player_update(&theContext, &EEPlayerTickOperation, static_cast<EeUnitsPlayerSaved*>(this), this);
}

// Restarts the timers of both players' wonders at the current game time
void EEPlayer::ResetWonderTimers(EEPlayer* inPlayer)
{
    std::vector<EEComplexUnit*>::iterator theIterator;
    for (theIterator = inPlayer->mWonders->mItems.begin(); theIterator != inPlayer->mWonders->mItems.end(); ++theIterator)
        (*theIterator)->mCloaked = gPhysics->mGameTime;
    for (theIterator = this->mWonders->mItems.begin(); theIterator != this->mWonders->mItems.end(); ++theIterator)
        (*theIterator)->mCloaked = gPhysics->mGameTime;
}

// Disallows the technologies a map forbids (by map flag; space units off space maps) and sets the selection hotkey
void EEPlayer::ApplyMapRestrictions()
{
    if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->RemoveHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, 0);
    if ((gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap) {
        this->GetTechTree()->Get(kTechDock)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechNavyYard)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechLighthouseOfAlexandria)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechHurricane)->mIsAllowed = false;
    } else if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeDock, false, false, false)),
                                             NULL, NULL, NULL, 0, false);
    if (!gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
        this->GetTechTree()->Get(kTechSpaceDock)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechSpaceTurret)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechMeteorStorm)->mIsAllowed = false;
        this->GetTechTree()->Get(kTechOrbitalSpaceStation)->mIsAllowed = false;
    } else if (gWorld.mPlayers[gWorld.mCurrentPlayer] == this)
        gUIManager->mMainGameForm->AddHotKey(kKeyD, kKeyControl, kKeyNone, kKeyNone, EECreateScreenEvent(ee_screens_hot_key_unit_selection(kUnitTypeSpaceDock, false, false, false)),
                                             NULL, NULL, NULL, 0, false);
}

// Recounts the player's living heroes, heroes being researched or queued, and teleporters
void EEPlayer::RecountHeroesAndTeleporters()
{
    EEPlayerUnits(this, 5);
}

// With the Camouflage civ power, sets the cloak timer of each idle unit (command 5000)
void EEPlayer::EnableCamouflagePower()
{
    if (!gCivPowersManager.HasPower(this, kCivPowerCamouflage)
        || gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) == NULL)
        return;
    EEUnit* theUnit = this->mFirstUnit;
    EEUnitSet* theSet = gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplex = theUnit->GetComplexUnit();
        if (theComplex == NULL || !theSet->Contains(theUnit->mUnitClass->mUnitType))
            continue;
        unsigned long theCommand = theComplex->mCommand ? theComplex->mCommand->mType : 0;
        if (theCommand == kActionIdle)
            theComplex->mCamouflageTimer = gWorldTable.GetLongValue(kWorldCamouflageTiming);
    }
}

// With the Camouflage civ power, uncloaks every unit and makes it uncloakable, clearing its cloak timers
void EEPlayer::DisableCamouflagePower()
{
    if (!gCivPowersManager.HasPower(this, kCivPowerCamouflage)
        || gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) == NULL)
        return;
    EEUnit* theUnit = this->mFirstUnit;
    EEUnitSet* theSet = gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet);
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
        if (theComplexUnit == NULL || !theSet->Contains(theUnit->mUnitClass->mUnitType))   // membership (was GetSet(bool))
            continue;
        theComplexUnit->SetCloakableChain(false);
        theComplexUnit->SetCloakedChain(false);
        theComplexUnit->mCamouflageTimer = 0;
        theComplexUnit->mU32C = 0;
    }
}

// Sets whether the unit and its following parts can cloak; turning it off also uncloaks them
void EEComplexUnit::SetCloakableChain(bool inValue)
{
    if (this->mIsCloakable != inValue) {
        this->mIsCloakable = inValue;
        if (this->mNextPart != NULL)
            this->mNextPart->SetCloakableChain(inValue);
    }
    if (!inValue)
        this->SetCloakedChain(false);
}

// Cloaks or uncloaks the unit and its following parts
void EEComplexUnit::SetCloakedChain(bool inValue)
{
    if (this->mIsCloaked == inValue)
        return;
    this->mIsCloaked = inValue;
    if (this->mNextPart != NULL)
        this->mNextPart->SetCloakedChain(inValue);
}

// Records a media warning when a unit class's buttons were not preloaded for the player
void EEPlayer::CheckButtonsPreloaded(long inUnitClass)
{
    if (this->mUnitClassUpgrades[inUnitClass]->mButtonLoadCount != 0)
        return;
    UString* theMessage = new UString;
    sprintf(*theMessage, "The unit %s BUTTONS might not have been preloaded for player %ld - have a programmer verify this.\n",
            (const char*)gUnitClassTable[inUnitClass]->mName, this->mPlayerID);
    this->mMediaWarnings.push_back(*theMessage);
    delete theMessage;
}

// Records a media warning when a unit class's media was not preloaded for the player (loads nothing)
void EEPlayer::LoadUnitClassMedia(long inUnitClass)
{
    if (this->mUnitClassUpgrades[inUnitClass]->mMediaCount != 0)
        return;
    UString* theMessage = new UString;
    sprintf(*theMessage, "The unit %s might not have been preloaded for player %ld - have a programmer verify this.\n",
            (const char*)gUnitClassTable[inUnitClass]->mName, this->mPlayerID);
    this->mMediaWarnings.push_back(*theMessage);
    delete theMessage;
}

// Loads the three player graphic effects and adds them to the loaded media
void EEPlayer::LoadPlayerEffects()
{
    for (unsigned char i = 0; i < 3; i++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(sPlayerEffects[i]);
        if (theEffect == NULL)
            continue;
        EEMedia* theMedia = theEffect;
        theMedia->LoadMedia();
        this->AddLoadedMedia(theMedia);
    }
}

// Unloads the three player graphic effects that are in the loaded media
void EEPlayer::UnloadPlayerEffects()
{
    for (unsigned char i = 0; i < 3; i++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(sPlayerEffects[i]);
        if (theEffect == NULL)
            continue;
        EEMedia* theMedia = theEffect;
        if (this->RemoveLoadedMedia(theMedia))
            theMedia->UnloadMedia();
    }
}

// EETechTreeNode 0x1cf989 / 0x1cfcc9 call these through slots +0x88 / +0xcc; both slots hold the shared empty
// `ret 4` 0x557269 (ICF), so each has an empty body (this also removes two unresolved virtuals)
void EEPlayer::EpochMediaLoaded(long inEpoch)
{
}

void EEPlayer::EpochMediaUnloaded(long inEpoch)
{
}

bool EEPlayer::AvoidsEnemies(class EEComplexUnit *)
{
    return false;
}

// Returns the number of living heroes and/or heroes in production
long EEPlayer::GetHeroCount(bool inFirst, bool inSecond)
{
    return this->GetNumHeroes(inFirst, inSecond);
}

bool EEPlayer::IsHuman()
{
    return false;
}

void EEPlayer::UnitStranded(class EEComplexUnit *)
{
}

void EEPlayer::Virtual60()
{
}

void EEPlayer::WonderStarted(class EEComplexUnit *)
{
}
