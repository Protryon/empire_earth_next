// C:\EEX\Empire Earth\AI Thread\EECPCivilizationManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPCivilizationManager, vtable 0x43c068; its key "EECPCivilizationManager-Data" and ESDebugOut strings name the
// class). A computer player's civilization manager: 0x6e8d4..0x71c7f (the class-static key's guarded chain first;
// EECPBuildManager.cpp's code ends at 0x6e8d4; the map/sort COMDATs close the unit, and 0x71c7f, the first function of
// the next range, is a member of a small flag class called only from 0x76b9f).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
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
#include "lle/USingleton.h"


#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLoadContainer.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEWheel.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPBehavior.h"
#include "ee/EECPTrace.h"
#include "ee/EESaveGameFiles.h"

const UString EECPCivilizationManager::sData("EECPCivilizationManager-Data");
// SYNTHETIC: EXE 0x46e8d4   (wrapper)
// SYNTHETIC: EXE 0x46e8de   (ctor)
// SYNTHETIC: EXE 0x46e8ef   (registrar)
// SYNTHETIC: EXE 0x46e8fb   (dtor)

// RepairBuildings's worker test (16 bytes passed by value to find_if; its operator() 0x71796 closes the unit among the sort
// COMDATs, after its first user find_if 0x7176f) (provisional name)
struct EECPCMSameContinent {
    EECPCMSameContinent(const EETileCoordinate& inTile, long inFlags, bool inConnected)
        : mTile(inTile), mFlags(inFlags), mConnected(inConnected) {}
    bool operator()(EEComplexUnit* inUnit)
    {
        if (this->mConnected)
            return gContinentManager.AreConnected(gContinentManager.GetContinentAt(inUnit),
                gContinentManager.GetContinentAt(this->mTile), this->mFlags);
        EEAIContinent* theContinent = gContinentManager.GetContinentAt(this->mTile);
        return gContinentManager.GetContinentAt(inUnit) == theContinent;
    }

    EETileCoordinate mTile;                 // +0x00
    long mFlags;                            // +0x08
    bool mConnected;                        // +0x0c
};

// FUNCTION: EXE 0x46e917
// Creates the civilization manager with its resource, build, unit, research and wall managers; building counts
// start at zero, units-per-building and maximum counts come from the player's AI settings
EECPCivilizationManager::EECPCivilizationManager(EEComputerPlayer* inPlayer)
    : mNextSpaceDockSiteTime(0)
{
    this->mPlayer = inPlayer;
    this->mResourceManager = new EECPResourceManager(this);
    this->mBuildManager = new EECPBuildManager(this);
    this->mUnitManager = new EECPUnitManager(this);
    this->mResearchManager = new EECPResearchManager(this);
    this->mWallManager = new EECPWallManager(this);
    this->mUnitCounts[kUnitTypeBarracks] = 0;
    this->mUnitCounts[kUnitTypeArcheryRange] = 0;
    this->mUnitCounts[kUnitTypeStable] = 0;
    this->mUnitCounts[kUnitTypeDock] = 0;
    this->mUnitCounts[kUnitTypeSiegeFactory] = 0;
    this->mUnitCounts[kUnitTypeNavyYard] = 0;
    this->mUnitCounts[kUnitTypeAirport] = 0;
    this->mUnitCounts[kUnitTypeCyberFactory] = 0;
    this->mUnitCounts[kUnitTypeUniversity] = 0;
    this->mUnitCounts[kUnitTypeTemple] = 0;
    this->mUnitCounts[kUnitTypeHospital] = 0;
    this->mUnitCounts[kUnitTypeFarm] = 0;
    this->mUnitCounts[kUnitTypeCyberLaboratory] = 0;
    this->mUnitCounts[kUnitTypeGuardTowerPaleo] = 0;
    this->mUnitCounts[kUnitTypeTankFactory] = 0;
    this->mUnitCounts[kUnitTypeAA10Gun88mm] = 0;
    this->mUnitCounts[kUnitTypeSpaceDock] = 0;
    this->mUnitCounts[kUnitTypeSpaceTurret] = 0;
    this->mUnitCounts[kUnitTypePriestTower] = 0;
    this->mUnitsPer[kUnitTypeBarracks] = this->GetPlayer()->GetUnitsPerBarracks();
    this->mUnitsPer[kUnitTypeArcheryRange] = this->GetPlayer()->GetUnitsPerArcheryRange();
    this->mUnitsPer[kUnitTypeStable] = this->GetPlayer()->GetUnitsPerStable();
    this->mUnitsPer[kUnitTypeDock] = this->GetPlayer()->GetUnitsPerDock();
    this->mUnitsPer[kUnitTypeSiegeFactory] = this->GetPlayer()->GetUnitsPerSiegeFactory();
    this->mUnitsPer[kUnitTypeNavyYard] = this->GetPlayer()->GetUnitsPerNavalYard();
    this->mUnitsPer[kUnitTypeAirport] = this->GetPlayer()->GetUnitsPerAirport();
    this->mUnitsPer[kUnitTypeCyberFactory] = this->GetPlayer()->GetUnitsPerCyberFactory();
    this->mUnitsPer[kUnitTypeUniversity] = this->GetPlayer()->GetUnitsPerUniversity();
    this->mUnitsPer[kUnitTypeTemple] = this->GetPlayer()->GetUnitsPerTemple();
    this->mUnitsPer[kUnitTypeHospital] = this->GetPlayer()->GetUnitsPerHospital();
    this->mUnitsPer[kUnitTypeFarm] = this->GetPlayer()->GetUnitsPerGranary();
    this->mUnitsPer[kUnitTypeCyberLaboratory] = this->GetPlayer()->GetUnitsPerCyberLab();
    this->mUnitsPer[kUnitTypeGuardTowerPaleo] = this->GetPlayer()->GetUnitsPerTower();
    this->mUnitsPer[kUnitTypeTankFactory] = this->GetPlayer()->GetUnitsPerTankFactory();
    this->mUnitsPer[kUnitTypeAA10Gun88mm] = this->GetPlayer()->GetUnitsPerAntiAirBuilding();
    this->mUnitsPer[kUnitTypeSpaceDock] = this->GetPlayer()->GetUnitsPerSpaceDock();
    this->mUnitsPer[kUnitTypeSpaceTurret] = this->GetPlayer()->GetUnitsPerTower();
    this->mUnitsPer[kUnitTypePriestTower] = 0xc;
    this->mMaxCounts[kUnitTypeBarracks] = this->GetPlayer()->GetBarracksMax();
    this->mMaxCounts[kUnitTypeArcheryRange] = this->GetPlayer()->GetArcheryRangeMax();
    this->mMaxCounts[kUnitTypeStable] = this->GetPlayer()->GetStableMax();
    this->mMaxCounts[kUnitTypeDock] = this->GetPlayer()->GetDockMax();
    this->mMaxCounts[kUnitTypeSiegeFactory] = this->GetPlayer()->GetSiegeFactoryMax();
    this->mMaxCounts[kUnitTypeNavyYard] = this->GetPlayer()->GetNavalYardMax();
    this->mMaxCounts[kUnitTypeAirport] = this->GetPlayer()->GetAirportMax();
    this->mMaxCounts[kUnitTypeCyberFactory] = this->GetPlayer()->GetCyberFactoryMax();
    this->mMaxCounts[kUnitTypeUniversity] = this->GetPlayer()->GetUniversityMax();
    this->mMaxCounts[kUnitTypeTemple] = this->GetPlayer()->GetTempleMax();
    this->mMaxCounts[kUnitTypeHospital] = this->GetPlayer()->GetHospitalMax();
    this->mMaxCounts[kUnitTypeFarm] = this->GetPlayer()->GetGranaryMax();
    this->mMaxCounts[kUnitTypeCyberLaboratory] = this->GetPlayer()->GetCyberLabMax();
    this->mMaxCounts[kUnitTypeGuardTowerPaleo] = this->GetPlayer()->GetTowerMax();
    this->mMaxCounts[kUnitTypeTankFactory] = this->GetPlayer()->GetTankFactoryMax();
    this->mMaxCounts[kUnitTypeAA10Gun88mm] = this->GetPlayer()->GetAntiAirMax();
    this->mMaxCounts[kUnitTypeSpaceDock] = this->GetPlayer()->GetSpaceDockMax();
    this->mMaxCounts[kUnitTypeSpaceTurret] = this->GetPlayer()->GetTowerMax();
    this->mMaxCounts[kUnitTypePriestTower] = 0x10;
    this->mFirstUpdateDone = false;
    this->mEpochResearchRequested = false;
    this->mB7B = false;
    this->mAdvancingEpoch = false;
    this->mNextEpochAdvanceTime = 0;
    this->mBuildingCountsChecked = false;
    this->mLastCountCheck = 0;
    this->mNextRepairTime = 0;
    this->mBuildingCountsEnabled = false;
    this->mEpochAdvanceEnabled = false;
    this->mGarrison = NULL;
    this->mFortress = NULL;
    this->mNextGarrisonTime = 0;
    this->mNextFortressTime = 0;
    this->mNextMoraleBuildingTime = 0;
    this->mNextAirportSiteTime = 0;
    this->mScheduleEpochAdvance = true;
}

// SYNTHETIC: EXE 0x46f0e3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x46f0ff
// Destroys the manager and its five sub-managers, releasing the garrison and fortress town centres
EECPCivilizationManager::~EECPCivilizationManager()
{
    if (this->mResourceManager != NULL)
        delete this->mResourceManager;
    if (this->mBuildManager != NULL)
        delete this->mBuildManager;
    if (this->mResearchManager != NULL)
        delete this->mResearchManager;
    if (this->mUnitManager != NULL)
        delete this->mUnitManager;
    if (this->mWallManager != NULL)
        delete this->mWallManager;
    if (this->mGarrison != NULL)
        this->mGarrison->Dereference();
    if (this->mFortress != NULL)
        this->mFortress->Dereference();
}

// FUNCTION: EXE 0x46f18d
// Adds inDelta to the building counts a unit feeds: a non-building raises the population-based buildings and the
// building that trains it, a building (not a farm or anti-air) the anti-air count; returns true if any changed
bool EECPCivilizationManager::AdjustUnitCount(long inUnitType, long inDelta)
{
    bool theResult = false;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    EETechTreeNode* theNode = theTree->Get(gUnitClassTable[inUnitType] ? gUnitClassTable[inUnitType]->mTechnology : 100000);
    if (theNode != NULL) {
        EEUnitClass* theClass = gUnitClassTable[inUnitType];
        if (!theClass->IsLandBuilding()) {
            this->mUnitCounts[kUnitTypeFarm] += inDelta;
            this->mUnitCounts[kUnitTypeUniversity] += inDelta;
            this->mUnitCounts[kUnitTypeTemple] += inDelta;
            this->mUnitCounts[kUnitTypeHospital] += inDelta;
            this->mUnitCounts[kUnitTypeGuardTowerPaleo] += inDelta;
            this->mUnitCounts[kUnitTypeSpaceTurret] += inDelta;
            this->mUnitCounts[kUnitTypePriestTower] += inDelta;
            theResult = true;
        }
        if (inUnitType != 0x20 && inUnitType != 0x1e0) {
            if (!theClass->IsLandBuilding()) {
                for (unsigned long i = 0, n = theNode->mData->mUnitClasses.size(); i < n; i++) {
                    long theType = theNode->GetUnitClass(i);
                    EEUnitClass* theOther = gUnitClassTable[theType];
                    EETechTreeNode* theOtherNode = theTree->Get(theOther ? theOther->mTechnology : 100000);
                    if (theOtherNode == NULL || !theOtherNode->mData->mMakesUnit || theOther == NULL || !theOther->IsLandBuilding())
                        continue;
                    std::map<long, long>::iterator theEntry = this->mUnitCounts.find(theType);
                    if (theEntry != this->mUnitCounts.end()) {
                        theEntry->second += inDelta;
                        theResult = true;
                    }
                }
            } else {
                if (inUnitType != 0x2a && inUnitType != 0x33a && inUnitType != 0x91 && inUnitType != 0x93
                    && inUnitType != 0x300) {
                    this->mUnitCounts[kUnitTypeAA10Gun88mm] += inDelta;
                    theResult = true;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46f3bc
// Tries to advance an epoch once the advance time is reached: researches the cheapest technologies for missing SOL,
// else drops obsolete requests and starts the epoch research (or waits 60 s when ahead of every human)
void EECPCivilizationManager::AttemptEpochAdvance(unsigned long inTime)
{
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theEpoch = theTree->mCurrentEpoch;
    if (!(theEpoch < gWorld.mGameOptions.mEpochEnd))
        return;
    long theNext = theEpoch + 1;
    EETechTreeEpoch* theNode = (theNext >= 0 && theNext < theTree->mEpochs.size()) ? &theTree->mEpochs[theNext] : NULL;
    unsigned long theDue = this->mNextEpochAdvanceTime;
    if (!EECPCheatOn(this->mPlayer, kAICheatEpochs)) {
        // New (docs/AI_RESEARCH.md §8.8, owner: epoch pressure rises with time): a paid advance is asked for once the
        // epoch pressure passes the personality's epoch_pressure start (aipersonalities.yaml); the pending epoch then
        // reserves a growing share of its cost (EEComputerPlayer::CanAfford) while the gatherers fill it
        unsigned long theEarly = (unsigned long)UMFloatToLong((1.0f - EECPPersonality(this->mPlayer).mEpochPressureStart)
            * (float)EECPEpochTimerSpan(this->mPlayer));
        theDue = theDue > theEarly ? theDue - theEarly : 0;
        // the experiments "stone22" / "stone30" (EECPEpochPressure): the Stone Age is asked for at the personality's
        // start share of a 22 (30) minute span from the game's start
        // (and the experiments span20/span25/span30 for the later epochs: EECPScheduleSpan)
        float theSchedule = EECPScheduleSpan(this->mPlayer);
        if (theSchedule > 0.0f)
            theDue = EECPScheduleStart(this->mPlayer, inTime) + (unsigned long)UMFloatToLong(
                EECPPersonality(this->mPlayer).mEpochPressureStart * theSchedule * 60000.0f);
        // the experiment "safedash": the pressure's safe-town bonus brings the request forward by as much
        float theBonus = EECPPressureBonus(this->mPlayer);
        if (theBonus > 0.0f) {
            unsigned long theSooner = (unsigned long)UMFloatToLong(theBonus * (theSchedule > 0.0f ? theSchedule * 60000.0f
                : (float)EECPEpochTimerSpan(this->mPlayer)));
            theDue = theDue > theSooner ? theDue - theSooner : 0;
        }
    }
    if (!(inTime >= theDue))
        return;
    long theMissing = theNode->GetRequiredSOL() - theNode->GetCurrentSOL();
    if (theMissing > 0) {
        if (!this->mEpochResearchRequested && this->mEpochAdvanceEnabled) {
            ESDebugOut("EECPCivilizationManager::AttemptEpochAdvance meeting SOL requirements(1)\n");
            this->mResearchManager->ResearchCheapest(theMissing);
            this->mEpochResearchRequested = true;
        }
    } else {
        this->mEpochResearchRequested = false;
        this->mUnitManager->DropObsoleteRequests(theNext);
        this->mBuildManager->DropObsoleteRequests(theNext);
        theMissing = theNode->GetRequiredSOL() - theNode->GetCurrentSOL();
        if (theMissing > 0 && this->mEpochAdvanceEnabled) {
            ESDebugOut("EECPCivilizationManager::AttemptEpochAdvance meeting SOL requirements(2)\n");
            this->mResearchManager->ResearchCheapest(theMissing);
            this->mEpochResearchRequested = true;
        } else if (!this->mPlayer->mIgnoreOthers && theEpoch > this->GetMaxEpoch() && EECPCheatOn(this->mPlayer, kAICheatCatchUp)) {
            // (held back while ahead of every human: the epoch rubber band, with CatchUpEpoch the catchup cheat)
            this->SetNextEpochAdvanceTime(inTime + 60000, true);
        } else {
            this->mResearchManager->Research(theNode->mData->mID);
            ESDebugOut("EECPCivilizationManager::AttemptEpochAdvance advancing epoch at time %ld\n", inTime);
            EE_CP_TRACE(this->mPlayer, "epoch.advance", "from=%ld to=%ld cost=%ld/%ld/%ld/%ld/%ld", theEpoch, theNext,
                (long)theNode->GetCost(kResourceFood, kUnitTypeTownCenter, 1.0f), (long)theNode->GetCost(kResourceWood, kUnitTypeTownCenter, 1.0f),
                (long)theNode->GetCost(kResourceStone, kUnitTypeTownCenter, 1.0f), (long)theNode->GetCost(kResourceGold, kUnitTypeTownCenter, 1.0f),
                (long)theNode->GetCost(kResourceIron, kUnitTypeTownCenter, 1.0f));
            this->mAdvancingEpoch = true;
        }
    }
}

// FUNCTION: EXE 0x46f4d4
// Fills a centre's garrison: on island/planet maps a town centre hires citizens for the free slots, otherwise idle
// connected citizens are sent in and any slots still free are added to the garrison count outright
void EECPCivilizationManager::FillGarrison(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*> theGarrison;
    std::vector<EEComplexUnit*> theUnits;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    EEMap* theMap = gWorld.mMap;
    unsigned short theContinent = gContinentManager.GetContinentID(inUnit);
    bool theFlag;
    if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
        theFlag = true;
    else
        theFlag = false;
    long theRoom = inUnit->GetUnitClass()->mGarrisonCapacity - inUnit->mGarrisonCount;
    if (inUnit->GetUnitClass()->mUnitType == kUnitTypeTownCenter && theFlag) {
        if (theRoom > 0) {
            EETechTreeNode* theNode;
            if (theTree != NULL)
                theNode = theTree->Get(gUnitClassTable[kUnitTypeCitizen] ? gUnitClassTable[kUnitTypeCitizen]->mTechnology : 100000);
            else
                theNode = NULL;
            long theType = 0x20;
            if (theNode != NULL && this->mPlayer->CanAfford(theNode, 0x23, theRoom)) {
                long theMade;
                EECPUMRequest theRequest;
                theRequest.mTownID = 0;
                theRequest.mContinent = theContinent;
                theRequest.mUnitClass = theType;
                theRequest.mCount = theRoom;
                this->mUnitManager->StartProduction(theRequest, theMade);
                theRoom -= theMade;
            }
            if (theRoom > 0)
                this->mUnitManager->RequestUnits(theType, theRoom, 0, theContinent);
        }
    } else {
        this->mUnitManager->GetCitizens(theUnits);
        for (std::vector<EEComplexUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
            if (theRoom <= 0)
                break;
            EEComplexUnit* theUnit = *it;
            if ((theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionIdle
                && (theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionWaitForOpenPosition
                && (!theFlag || !this->mUnitManager->IsCitizenAvailable(theUnit)))
                continue;
            long theFlags = this->mPlayer->GetCitizenPassability();
            unsigned short theID = gContinentManager.GetContinentID(theUnit);
            if (!gContinentManager.AreConnected(gContinentManager.GetContinentByID(theID),
                    gContinentManager.mContinents[theContinent], theFlags)
                || theUnit->mContainer != NULL)
                continue;
            EEUGLoadGarrison* theGoal = dynamic_cast<EEUGLoadGarrison*>(inUnit->GetLoadGoal());
            if (theGoal == NULL) {
                theGarrison.clear();
                theGarrison.push_back(theUnit);
                theGoal = new EEUGLoadGarrison(inUnit, theGarrison, true);
                inUnit->SetGoal(theGoal, 0, 0);
                inUnit->StartAction(kActionWaitForGarrisonCost);
                inUnit->GetUnitClass()->ActionChanged(inUnit);
            } else
                theGoal->AddUnit(theUnit);
            theUnit->ClearGoalQueue(0);
            theUnit->SetGoal(theGoal->Clone(), 0, 0);
            theUnit->StartAction(kActionPrepareToMove);
            theUnit->GetUnitClass()->ActionChanged(theUnit);
            theRoom--;
        }
        if (theRoom > 0)
            inUnit->AddGarrison(inUnit->mGarrisonCount + theRoom, inUnit->mGarrisonPopulation);
    }
}

// FUNCTION: EXE 0x46f7b8
// Queues the buildings the unit counts call for (one per units-per share up to the maximum; military buildings
// by the difficulty's minimum), skipping docks on waterless maps, space docks off space maps and early airports
// NEAR (99.3): the deque end() in the unit loop adds _Myoff + _Mysize in the other order (Dinkum deque::end)
void EECPCivilizationManager::CheckBuildingCounts()
{
    std::deque<EEComplexUnit*>::iterator theUnit;
    std::map<long, long>::iterator theEntry;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    EEMap* theMap = gWorld.GetMap();
    long theHave = 0;
    long theOther = 0;
    bool theFlag2;
    if (theMap != NULL && (theMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap)
        theFlag2 = true;
    else
        theFlag2 = false;
    bool theFlag1;
    if (theMap != NULL && theMap->mTerrainMesh.GetIsSpaceMap())
        theFlag1 = true;
    else
        theFlag1 = false;
    if (!this->mPlayer->mCanBuild || !this->mBuildingCountsEnabled)
        return;
    for (theEntry = this->mUnitCounts.begin(); theEntry != this->mUnitCounts.end(); theEntry++) {
        long theKey = theEntry->first;
        long theCount = theEntry->second;
        unsigned long theType;
        if (theKey == 0x2f)
            theType = this->GetTowerClass();
        else if (theKey == 0x91)
            theType = this->GetAntiAirClass();
        else if (theKey == 0x2a)
            theType = EEGetFarmType(this->mPlayer);
        else
            theType = theKey;
        EETechTreeNode* theNode = theTree->Get(gUnitClassTable[theType] ? gUnitClassTable[theType]->mTechnology : 100000);
        if (theNode == NULL || theNode->GetIsObsolete() || !(theTree->mCurrentEpoch >= theNode->mEpoch) || !theNode->GetIsAvailable())
            continue;
        long thePer = this->mUnitsPer[theKey];
        long theMax = this->mMaxCounts[theKey];
        long theWanted = theCount / thePer;
        if (theCount % thePer != 0)
            theWanted++;
        if (theMax > 0 && theWanted > theMax)
            theWanted = theMax;
        if (theType == kUnitTypeDock && theWanted > 2 && EECPCoastFishingOn(this->mPlayer))
            theWanted = 2;   // economy.coast_fishing (docs/AI_RESEARCH.md §8.26): one or two docks, as the owner builds (it built six)
        if (theKey == 0x2a) {
            if (this->mPlayer->mUsesFishing)
                theHave = theWanted;
            else {
                std::deque<EEComplexUnit*>* theUnits = this->mUnitManager->GetUnits(theType);
                if (theUnits != NULL) {
                    theHave = 0;
                    theOther = 0;
                    for (theUnit = theUnits->begin(); theUnit != theUnits->end(); theUnit++) {
                        if ((*theUnit)->mComplexUnit->mIsBuilt)
                            theHave++;
                        else
                            theOther++;
                    }
                }
            }
        } else
            theHave = this->mUnitManager->GetCount(theType);
        unsigned long theBuild = 0;
        switch (theType) {
        case kUnitTypeBarracks:
        case kUnitTypeDock:
        case kUnitTypeArcheryRange:
        case kUnitTypeSiegeFactory:
        case kUnitTypeStable:
        case kUnitTypeNavyYard:
        case kUnitTypeAirport:
        case kUnitTypeTankFactory:
        case kUnitTypeCyberFactory:
        case kUnitTypeCyberLaboratory:
        case kUnitTypeSpaceDock: {
            unsigned long theFirst;
            if (this->mPlayer->IsDeathmatch()) {
                if (this->mPlayer->mDifficulty == kDifficultyEasy)
                    theFirst = this->mPlayer->mMinMilitaryBuildingsEasyDM;
                else if (this->mPlayer->mDifficulty == kDifficultyHard)
                    theFirst = this->mPlayer->mMinMilitaryBuildingsHardDM;
                else
                    theFirst = this->mPlayer->mMinMilitaryBuildingsMediumDM;
            } else {
                if (this->mPlayer->mDifficulty == kDifficultyEasy)
                    theFirst = this->mPlayer->mMinMilitaryBuildingsEasy;
                else if (this->mPlayer->mDifficulty == kDifficultyHard)
                    theFirst = this->mPlayer->mMinMilitaryBuildingsHard;
                else
                    theFirst = this->mPlayer->mMinMilitaryBuildingsMedium;
            }
            if (theHave == 0)
                theBuild = theFirst;
            else if (theHave < theWanted) {
                if (this->mPlayer->IsDeathmatch())
                    theBuild = theWanted - theHave;
                else
                    theBuild = 1;
            }
            if (theFlag2 && (theType == kUnitTypeDock || theType == kUnitTypeNavyYard))
                theBuild = 0;
            if (!theFlag1 && theType == kUnitTypeSpaceDock)
                theBuild = 0;
            if (theType == kUnitTypeAirport && !this->mPlayer->mAttackStarted)
                theBuild = 0;
            break;
        }
        default:
            if (theWanted > 0 && theHave < theWanted)
                theBuild = 1;
            if (!theFlag1 && theType == kUnitTypeSpaceTurret)
                theBuild = 0;
            break;
        }
        if (theBuild <= 0)
            continue;
        if (this->mBuildManager->IsQueued(theType))
            continue;
        if (theKey == 0x2a && theOther == 0) {
            if (!this->mBuildManager->IsQueued(0x34))
                this->mBuildManager->QueueBuilding(0x34, 1, 0);
        } else {
            for (unsigned long i = 0; i < theBuild; i++)
                this->mBuildManager->QueueBuilding(theType, 1, 0);
        }
    }
    this->mBuildingCountsChecked = true;
}

// FUNCTION: EXE 0x46fb5e
// Queues a house at the first resource centre whose morale is not full and that has a house site, unless one is
// already queued
void EECPCivilizationManager::QueueMoraleBuilding()
{
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    if (this->mBuildManager->IsQueued(0x25))
        return;
    for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters(); i++) {
        EECPResourceCenter* theCenter = this->mPlayer->GetResourceCenter(i);
        if (theCenter->mUnit != NULL && theCenter->GetMoraleFill() < 1.0f && theCenter->FindBuildSite(kUnitTypeHouse, theTile)) {
            this->mBuildManager->QueueBuildingAt(0x25, theTile.mX, theTile.mY);
            return;
        }
    }
}

// FUNCTION: EXE 0x46fbe2
// Returns the transport class for the docks, airports and space docks owned; with none, looks for an airport or
// space dock site (at most every 2 minutes) and otherwise falls back to a sea transport
long EECPCivilizationManager::ChooseTransportClass()
{
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    unsigned long theTime = gPhysics->mGameTime;
    bool theHave24 = this->mUnitManager->GetCount(0x24) != 0;
    bool theHave42;
    if (this->mUnitManager->GetCount(0x42) != 0 && !gWorld.mMap->mTerrainMesh.GetIsSpaceMap())
        theHave42 = true;
    else
        theHave42 = false;
    bool theHave2D8 = this->mUnitManager->GetCount(0x2d8) != 0;
    if (!theHave42 && !theHave24 && !theHave2D8 && theTree != NULL) {
        if (gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
            EETechTreeNode* theNode = theTree->Get(gUnitClassTable[kUnitTypeSpaceDock] ? gUnitClassTable[kUnitTypeSpaceDock]->mTechnology : 100000);
            if (theNode != NULL && theTree->mCurrentEpoch >= theNode->mEpoch && (this->mNextSpaceDockSiteTime == 0 || theTime > this->mNextSpaceDockSiteTime)) {
                theHave2D8 = this->mBuildManager->FindSite(kUnitTypeSpaceDock, theTile);
                if (this->mNextSpaceDockSiteTime == 0)
                    this->mNextSpaceDockSiteTime = theTime + this->mPlayer->mPlayerID * 17000 + 120000;
                else
                    this->mNextSpaceDockSiteTime = theTime + 120000;
            }
            theHave24 = false;
        } else {
            EETechTreeNode* theNode = theTree->Get(gUnitClassTable[kUnitTypeAirport] ? gUnitClassTable[kUnitTypeAirport]->mTechnology : 100000);
            if (theNode != NULL && theTree->mCurrentEpoch >= theNode->mEpoch && (this->mNextAirportSiteTime == 0 || theTime > this->mNextAirportSiteTime)) {
                theHave42 = this->mBuildManager->FindSite(kUnitTypeAirport, theTile);
                if (this->mNextAirportSiteTime == 0)
                    this->mNextAirportSiteTime = theTime + this->mPlayer->mPlayerID * 17000 + 120000;
                else
                    this->mNextAirportSiteTime = theTime + 120000;
            }
            theHave24 = !theHave42;
        }
    }
    return this->GetTransportClass(theHave24, theHave42, theHave2D8);
}

// FUNCTION: EXE 0x46fd9c
// Returns the anti-air building class for the current epoch (0 before the WW1 epoch)
long EECPCivilizationManager::GetAntiAirClass()
{
    long theResult = 0;
    long theEpoch = this->mPlayer->mTechnologies->mCurrentEpoch;
    if (theEpoch >= kTechEpochAtomicWW1) {
        if (theEpoch > kTechEpochAtomicModern) {
            if (theEpoch > kTechEpochNano) {
                if (theEpoch == kTechEpochSpace)
                    theResult = 0x300;
            } else
                theResult = 0x93;
        } else
            theResult = 0x91;
    }
    return theResult;
}

// FUNCTION: EXE 0x46fdd3
// Returns the fishing boat class for the current epoch (stone, bronze or modern boat)
long EECPCivilizationManager::GetFishingBoatClass()
{
    long theResult = 0;
    long theEpoch = this->mPlayer->mTechnologies->mCurrentEpoch;
    if (theEpoch >= kTechEpochPrehistoric) {
        if (theEpoch > kTechEpochCopper) {
            if (theEpoch > kTechEpochIndustrial) {
                if (theEpoch <= kTechEpochSpace)
                    theResult = 0xd3;
            } else
                theResult = 0x6d;
        } else
            theResult = 0x6c;
    }
    return theResult;
}

// FUNCTION: EXE 0x46fe05
// Returns the gate class for the current epoch
long EECPCivilizationManager::GetGateClass()
{
    long theResult = 0;
    switch (this->mPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
        theResult = 0x2e;
        break;
    case kTechEpochCopper:
        theResult = 0x1d9;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1c6;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1bc;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1bb;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1ba;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x126;
        break;
    case kTechEpochSpace:
        theResult = 0x2ff;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x46fe86
// Returns the guard tower class for the current epoch
long EECPCivilizationManager::GetTowerClass()
{
    long theResult = 0;
    switch (this->mPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
        theResult = 0x2f;
        break;
    case kTechEpochCopper:
        theResult = 0x1cd;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1ce;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1cf;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1d0;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1d1;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x1d2;
        break;
    case kTechEpochSpace:
        theResult = 0x2fd;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x46ff07
// Returns the transport class for the current epoch, preferring space, then air, then sea as available; 0 if none
long EECPCivilizationManager::GetTransportClass(bool inSea, bool inAir, bool inSpace)
{
    long theResult = 0;
    long theLate;
    switch (this->mPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochPrehistoric:
    case kTechEpochStone:
        if (inSea)
            theResult = 0x139;
        return theResult;
    case kTechEpochCopper:
        if (inSea)
            theResult = 0x6e;
        return theResult;
    case kTechEpochBronze:
    case kTechEpochDark:
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        if (inSea)
            theResult = 0x6f;
        return theResult;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        if (inSea)
            theResult = 0x70;
        return theResult;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
        if (inSea)
            theResult = 0x71;
        return theResult;
    case kTechEpochAtomicModern:
        if (inSea)
            theResult = 0x71;
        else if (inAir)
            theResult = 0x178;
        return theResult;
    case kTechEpochDigital:
        theLate = 0x71;
        break;
    case kTechEpochNano:
        theLate = 0x72;
        break;
    case kTechEpochSpace:
        if (inSpace)
            theResult = 0x2f4;
        else if (inAir)
            theResult = 3;
        else if (inSea)
            theResult = 0x72;
        return theResult;
    default:
        return theResult;
    }
    if (inAir)
        theResult = 3;
    else if (inSea)
        theResult = theLate;
    return theResult;
}

// FUNCTION: EXE 0x46ffdc
// Returns the wall class for the current epoch (0 before the Copper epoch)
long EECPCivilizationManager::GetWallClass()
{
    long theResult = 0;
    switch (this->mPlayer->mTechnologies->mCurrentEpoch) {
    case kTechEpochCopper:
        theResult = 0x1d3;
        break;
    case kTechEpochBronze:
    case kTechEpochDark:
        theResult = 0x1d4;
        break;
    case kTechEpochMiddle:
    case kTechEpochRenaissance:
        theResult = 0x1d5;
        break;
    case kTechEpochImperial:
    case kTechEpochIndustrial:
        theResult = 0x1d6;
        break;
    case kTechEpochAtomicWW1:
    case kTechEpochAtomicWW2:
    case kTechEpochAtomicModern:
        theResult = 0x1d7;
        break;
    case kTechEpochDigital:
    case kTechEpochNano:
        theResult = 0x1d8;
        break;
    case kTechEpochSpace:
        theResult = 0x2fe;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x47005a
// Returns the latest current epoch among the human players (0 with none)
long EECPCivilizationManager::GetMaxEpoch()
{
    long theMax = 0;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (!thePlayer->IsHuman())
            continue;
        EETechTree* theTree = thePlayer->mTechnologies;
        if (theTree == NULL)
            continue;
        long theEpoch = theTree->mCurrentEpoch;
        if (theMax > theEpoch)
            theEpoch = theMax;
        theMax = theEpoch;
    }
    return theMax;
}

// FUNCTION: EXE 0x4700a9
// Returns true when some human player is in a later epoch than this player
bool EECPCivilizationManager::IsBehindInEpoch()
{
    EETechTree* theTree = this->mPlayer->mTechnologies;
    bool theResult = false;
    if (theTree != NULL) {
        long theEpoch = theTree->mCurrentEpoch;
        theResult = this->GetMaxEpoch() > theEpoch;
    }
    return theResult;
}

// FUNCTION: EXE 0x4700cb
// Reads the sub-managers and the epoch, building-count and garrison state from a saved game (garrison and fortress
// as unit ids until Restore); returns the first read error
long EECPCivilizationManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    std::vector<EETileCoordinate> theUnused;
    unsigned long theID;
    long theResult = this->mResourceManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = this->mBuildManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = this->mResearchManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = this->mUnitManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mFirstUpdateDone);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mEpochResearchRequested);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mB7B);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAdvancingEpoch);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextEpochAdvanceTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mScheduleEpochAdvance);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mLastCountCheck);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mBuildingCountsChecked);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mEpochAdvanceEnabled);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mBuildingCountsEnabled);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mGarrison = EEUnitHandle<EEComplexUnit>::FromSavedID(theID);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextGarrisonTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextMoraleBuildingTime);
    if (theResult == kErrorOK)
        theResult = this->mWallManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mFortress = EEUnitHandle<EEComplexUnit>::FromSavedID(theID);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextFortressTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMoraleBuildingsEnabled);
    // port (0.267): the repair, airport site and space dock site timers (game ms; the original did not save them, so
    // a loaded player repaired at its first update and every 31 s from there: seed 4's 1350 s save sent a citizen to
    // repair at once, seeds 2's and 5's 600 s saves 50 and 20 s later than the continued game)
    if (theResult == kErrorOK && EESaveHasCivilizationTimers(inMajorVersion, inMinorVersion)) {
        theResult = inFile.Read(this->mNextRepairTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAirportSiteTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextSpaceDockSiteTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x4702b6
// Returns true when inAmount is below the player's cap for the resource
bool EECPCivilizationManager::IsBelowResourceCap(long inResource, long inAmount)
{
    return inAmount < this->mPlayer->mResourceCaps[inResource];
}

// FUNCTION: EXE 0x4702d4
// Releases a swapped garrison or fortress (re-requesting the garrison's citizens on island maps), then passes the
// swaps to the resource and wall managers
void EECPCivilizationManager::UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps)
{
    if (this->mGarrison != NULL || this->mFortress != NULL) {
        EEMap* theMap = gWorld.mMap;
        bool theFlag;
        if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
            theFlag = true;
        else
            theFlag = false;
        for (std::vector<EEPUnitSwap>::iterator it = inSwaps.begin(); it != inSwaps.end(); it++) {
            if (this->mGarrison == it->mOld) {
                this->ReleaseGarrison();
                if (theFlag) {
                    long theCount = it->mOld->mUnitClass->mGarrisonCapacity;
                    this->mUnitManager->RequestUnits(0x20, theCount, 0, gContinentManager.GetContinentID(it->mOld));
                }
                break;
            }
            if (this->mFortress == it->mOld) {
                this->ReleaseFortress();
                break;
            }
        }
    }
    this->mResourceManager->UnitsSwapped(inSwaps);
    this->mWallManager->UnitsSwapped(inSwaps);
}

// FUNCTION: EXE 0x47037c
// Resets the build, resource, unit and wall managers, replaces unaffordable research and clears the epoch flags
void EECPCivilizationManager::Reset()
{
    this->mBuildManager->Reset();
    this->mResearchManager->ReplaceUnaffordable();
    this->mResourceManager->Reset();
    this->mUnitManager->Reset();
    this->mWallManager->Reset();
    this->mEpochResearchRequested = this->mB7B = this->mAdvancingEpoch = false;
}

static const float kCMRepairHealthRatio = 0.8f;   // 0x43c060 (a unit-local const: not the pooled 0.8f)

// FUNCTION: EXE 0x4703b4
// Sends idle citizens to repair buildings below 80% hit points (not farms or volcanoes), nearest connected worker
// first; a gate chain is repaired from its nearer end
void EECPCivilizationManager::RepairBuildings()
{
    std::vector<EEComplexUnit*> theWorkers;
    std::vector<EEComplexUnit*> theDamaged;
    EEUnit* theUnit = this->mPlayer->mFirstUnit;
    theDamaged.assign(this->mRepairing.begin(), this->mRepairing.end());
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEUnitClass* theClass = theUnit->GetUnitClass();
        if (!theClass->IsLandBuilding() || theUnit->mTileNode == NULL)
            continue;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if ((theComplex->mIsBuilt || theComplex->mNumBuilders == 0)
            && (float)theUnit->mHitPoints / theUnit->mStats->mMaxHitPoints < kCMRepairHealthRatio && theComplex->mPreviousPart == NULL
            && theClass->mUnitType != kUnitTypeVolcano && theClass->mUnitType != kUnitTypeFarm && theClass->mUnitType != kUnitTypeRoboticFarm
            && (!theClass->mIsWall || theUnit->mHitPoints > 1)
            && std::find(theDamaged.begin(), theDamaged.end(), theComplex) == theDamaged.end())
            theDamaged.push_back(theComplex);
    }
    unsigned long theCount = theDamaged.size();
    if (theCount == 0)
        return;
    this->mUnitManager->GetIdleCitizens(theWorkers);
    if (theWorkers.size() <= 0)
        return;
    for (unsigned long i = 0; i < theCount; i++) {
        if (theWorkers.size() <= 0)
            break;
        EEComplexUnit* theTarget = theDamaged[i];
        std::sort(theWorkers.begin(), theWorkers.end(), EECPUnitDistanceLess(theTarget->mTileX, theTarget->GetTileY()));
        std::vector<EEComplexUnit*>::iterator theWorker = theWorkers.begin();
        if (theTarget->GetUnitClass()->mUnitType != kUnitTypeDock && theTarget->GetUnitClass()->mUnitType != kUnitTypeNavyYard)
            theWorker = std::find_if(theWorkers.begin(), theWorkers.end(),
                EECPCMSameContinent(theTarget->GetTile(), this->mPlayer->GetCitizenPassability(), true));
        if (theWorker == theWorkers.end())
            continue;
        EEComplexUnit* theRepairer = *theWorker;
        theWorkers.erase(theWorker);
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(theTarget->GetUnitClass());
        if (theGate != NULL && theTarget->mPreviousPart == NULL && theTarget->mNextPart != NULL) {
            float theDistance = theRepairer->DistanceSquaredTo(theGate->GetLastLinkedCarrier(theTarget));
            if (theRepairer->DistanceSquaredTo(theGate->GetLastLinked(theTarget)) > theDistance)
                theTarget = theGate->GetLastLinkedCarrier(theTarget);
            else
                theTarget = theGate->GetLastLinked(theTarget);
        }
        theRepairer->SetGoal(new EEUGBuildRepair(this->mPlayer, theTarget, true), 1, 0);
        theRepairer->GetUnitClass()->ActionChanged(theRepairer);
        std::vector<EEUnitHandle<EEComplexUnit> >::iterator theFound = std::find(this->mRepairing.begin(), this->mRepairing.end(), theTarget);
        if (theFound != this->mRepairing.end())
            this->mRepairing.erase(theFound);
    }
}

// FUNCTION: EXE 0x470765
// Restores the sub-managers after loading, recounts the player's units, turns saved delays back into game times and
// the garrison/fortress ids into referenced units; returns the first error
long EECPCivilizationManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = this->mBuildManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mUnitManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mResourceManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mResearchManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mWallManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK) {
        for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (!theUnit->IsPathMarker())
                this->AdjustUnitCount(theUnit->mUnitClass->mUnitType, 1);
        }
        if (this->mNextEpochAdvanceTime > 4000000000) {
            this->mAdvancingEpoch = false;
            this->mScheduleEpochAdvance = true;
        } else
            this->mNextEpochAdvanceTime += gPhysics->mGameTime;
        unsigned long theID = this->mGarrison.GetSavedID();
        this->mGarrison = NULL;
        if (!UIsNone32(theID)) {
            EEComplexUnit* theGarrison = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
            if (theGarrison != NULL) {
                this->mGarrison = theGarrison;
                theGarrison->Reference();
            }
        }
        theID = this->mFortress.GetSavedID();
        this->mFortress = NULL;
        if (!UIsNone32(theID)) {
            EEComplexUnit* theFortress = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
            if (theFortress != NULL) {
                this->mFortress = theFortress;
                theFortress->Reference();
            }
        }
        this->mNextGarrisonTime += gPhysics->mGameTime;
        this->mNextMoraleBuildingTime += gPhysics->mGameTime;
        this->mNextFortressTime += gPhysics->mGameTime;
    }
    return theResult;
}

// FUNCTION: EXE 0x4708c9
// Returns true for waypoint and path point units
bool EEUnit::IsPathMarker()
{
    return this->mUnitClass->mUnitType == kUnitTypeWaypoint || this->mUnitClass->mUnitType == kUnitTypePathPoint;
}

// FUNCTION: EXE 0x4708e7
// Writes the sub-managers and the manager's state (times relative to now, garrison/fortress as unit ids or -1) to
// the saved game, marking the building counts checked; returns the first write error
long EECPCivilizationManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mResourceManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mBuildManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mResearchManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mUnitManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mFirstUpdateDone);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mEpochResearchRequested);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mB7B);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mAdvancingEpoch);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNextEpochAdvanceTime - gPhysics->mGameTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mScheduleEpochAdvance);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mLastCountCheck);
    if (theResult != kErrorOK)
        return theResult;
    // port: the original set mBuildingCountsChecked here before writing it, so saving changed the game (the next
    // Update skipped the building-count check and moved its timer; PORT_LEARNINGS "The headless simulation"). The
    // file gets the running value (from save version 0.254; older saves have true): the loaded game checks its
    // building counts when the game that was saved does (R5a.2, "load equals continue").
    theResult = outFile.Write(this->mBuildingCountsChecked);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mEpochAdvanceEnabled);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mBuildingCountsEnabled);
    if (theResult != kErrorOK)
        return theResult;
    unsigned long theID = this->mGarrison != NULL ? this->mGarrison->GetUnitID() : (unsigned long)-1;
    theResult = outFile.Write(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNextGarrisonTime - gPhysics->mGameTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNextMoraleBuildingTime - gPhysics->mGameTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mWallManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theID = this->mFortress != NULL ? this->mFortress->GetUnitID() : (unsigned long)-1;
    theResult = outFile.Write(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNextFortressTime - gPhysics->mGameTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMoraleBuildingsEnabled);
    if (theResult == kErrorOK && EESaveHasCivilizationTimers(inMajorVersion, inMinorVersion)) {   // port (0.267, Load)
        theResult = outFile.Write(this->mNextRepairTime);
        if (theResult == kErrorOK)
            theResult = outFile.Write(this->mNextAirportSiteTime);
        if (theResult == kErrorOK)
            theResult = outFile.Write(this->mNextSpaceDockSiteTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x470b04
// Sets the next epoch advance time: inTime itself when inActual, else a random delay from the player's epoch
// frequency, lengthened when ahead of the humans and shortened when behind (a quarter when 3+ epochs behind)
void EECPCivilizationManager::SetNextEpochAdvanceTime(unsigned long inTime, bool inActual)
{
    // behaviour record 94 (0x4e6118 + 94 * 0x38 + 0x30 = 0x4e75d8: dbcpbehavior.dat's epoch advance scale; the
    // same absolute-address read as WorkOutPopulation's records 92/93 at 0x4e7568/0x4e75a0)
    float theScale = gCPBehaviors[kCPBehaviorEpochAdvanceTimeScale].mFloatValue;
    long theEpoch = this->mPlayer->mTechnologies->mCurrentEpoch;
    if (inActual) {
        this->mNextEpochAdvanceTime = inTime;
        ESDebugOut("EECPCivilizationManager::SetNextEpochAdvanceTime  setting actual next epoch advance time to %ld\n",
            inTime);
    } else {
        float theMinimum = this->mPlayer->GetEpochFrequencyMin();
        float theMaximum = this->mPlayer->GetEpochFrequencyMax();
        float theMinutes = gPhysics->GetRandom(theMinimum, theMaximum);
        theMinutes = this->mPlayer->GetEpochFrequencyModifier() * theMinutes;
        if (!this->mPlayer->mIgnoreOthers && EECPCheatOn(this->mPlayer, kAICheatCatchUp)) {   // the epoch rubber band (catchup)
            long theMax = this->GetMaxEpoch();
            if (theEpoch > theMax)
                theMinutes = theMinutes * theScale;
            else if (theEpoch < theMax)
                theMinutes = theMinutes / theScale;
            if (theMax - theEpoch >= 3)
                theMinutes = theMinutes * 0.25f;
        }
        long theDelay = UMFloatToLong(theMinutes * 60000.0f);
        this->mNextEpochAdvanceTime = inTime + theDelay;
        ESDebugOut("EECPCivilizationManager::SetNextEpochAdvanceTime  setting next epoch advance time to %ld (%f)\n",
            this->mNextEpochAdvanceTime, (double)theMinutes);
    }
}

// FUNCTION: EXE 0x470bf6
// Sends every citizen bound for the garrison centre back to idle and drops the garrison
void EECPCivilizationManager::ReleaseGarrison()
{
    unsigned long i = 0;
    std::vector<EEComplexUnit*> theUnits;
    this->mUnitManager->GetCitizens(theUnits);
    unsigned long theCount = theUnits.size();
    for (; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i];
        EEUGLoadGarrison* theGoal = dynamic_cast<EEUGLoadGarrison*>(theUnit->GetLoadGoal());
        if (theGoal == NULL || theGoal->mContainer != this->mGarrison)
            continue;
        theUnit->ClearGoalQueue(0);
        theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
    }
    this->mGarrison->Dereference();
    this->mGarrison = NULL;
}

// FUNCTION: EXE 0x470c9d
// Sends every citizen bound for the fortress town centre back to idle and drops the fortress
void EECPCivilizationManager::ReleaseFortress()
{
    unsigned long i = 0;
    std::vector<EEComplexUnit*> theUnits;
    this->mUnitManager->GetCitizens(theUnits);
    unsigned long theCount = theUnits.size();
    for (; i < theCount; i++) {
        EEComplexUnit* theUnit = theUnits[i];
        EEUGLoadGarrison* theGoal = dynamic_cast<EEUGLoadGarrison*>(theUnit->GetLoadGoal());
        if (theGoal == NULL || theGoal->mContainer != this->mFortress)
            continue;
        theUnit->ClearGoalQueue(0);
        theUnit->StartAction(kActionIdle);
        theUnit->mUnitClass->ActionChanged(theUnit);
    }
    this->mFortress->Dereference();
    this->mFortress = NULL;
}

// FUNCTION: EXE 0x470d44
// Registers a new unit with the unit manager and building counts; a new citizen heads for the garrison or fortress
// (a random one when both exist), and buildings and walls go to the wall manager
void EECPCivilizationManager::UnitAdded(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*> theGarrison;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (inUnit->mHitPoints > 0)
        this->mUnitManager->UnitAdded(inUnit, 0);
    if (!inUnit->IsPathMarker())
        this->AdjustUnitCount(inUnit->mUnitClass->mUnitType, 1);
    if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && (this->mGarrison != NULL || this->mFortress != NULL)) {
        EEComplexUnit* theContainer;
        if (this->mGarrison != NULL && this->mFortress != NULL)
            theContainer = gPhysics->mRandom.Generate() > 0.5f ? this->mFortress : this->mGarrison;
        else
            theContainer = this->mGarrison != NULL ? this->mGarrison : this->mFortress;
        EEUGLoadGarrison* theGoal = dynamic_cast<EEUGLoadGarrison*>(theContainer->GetLoadGoal());
        if (theGoal == NULL) {
            theGarrison.push_back(theUnit);
            theGoal = new EEUGLoadGarrison(theContainer, theGarrison, true);
            theContainer->SetGoal(theGoal, 0, 0);
            theContainer->StartAction(kActionWaitForGarrisonCost);
            theContainer->mUnitClass->ActionChanged(theContainer);
        } else
            theGoal->AddUnit(theUnit);
        theUnit->ClearGoalQueue(0);
        theUnit->SetGoal(theGoal->Clone(), 0, 0);
        theUnit->StartAction(kActionPrepareToMove);
        theUnit->mUnitClass->ActionChanged(theUnit);
    }
    if (inUnit->mUnitClass->IsLandBuilding() || (inUnit->mUnitClass->mIsWall && !inUnit->mUnitClass->mIsTower))
        this->mWallManager->UnitAdded(inUnit);
}

// FUNCTION: EXE 0x470ed1
// Removes a lost unit from the unit manager, the resource manager (citizens), the building counts and the wall
// manager (buildings and walls)
void EECPCivilizationManager::UnitLost(EEComplexUnit* inUnit, bool inSwapped)
{
    this->mUnitManager->UnitLost(inUnit);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeCitizen || inUnit->mUnitClass->mUnitType == kUnitTypeFemaleCitizen)
        this->mResourceManager->CitizenLost(inUnit->mComplexUnit);
    if (!inUnit->IsPathMarker())
        this->AdjustUnitCount(inUnit->mUnitClass->mUnitType, -1);
    if (inUnit->mUnitClass->IsLandBuilding() || (inUnit->mUnitClass->mIsWall && !inUnit->mUnitClass->mIsTower))
        this->mWallManager->UnitLost(inUnit, inSwapped);
}

// FUNCTION: EXE 0x470f52
// Runs the civilization each tick: first-update requests, the sub-managers, epoch advance, building counts every
// 30 s, repairs every 31 s, garrison and fortress choice, and a morale house every 59 s
void EECPCivilizationManager::Update(unsigned long inTime)
{
    UWideString theString1;
    UWideString theString2;
    EEGameOptions theOptions;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    if (!this->mFirstUpdateDone) {
        EETechTreeNode* theNode = theTree->Get(kTechFutureResearchSentinel);
        if (theNode != NULL) {
            if (theNode->mData->mMakesUnit) {
                EEUnitClass* theClass = gUnitClassTable[theNode->mData->mUnitClass];
                if (theClass != NULL) {
                    if (theClass->IsLandBuilding())
                        this->mBuildManager->QueueBuilding(theNode->mData->mUnitClass, 0, 0);
                    else
                        this->mUnitManager->RequestUnits(theNode->mData->mUnitClass, 1, 0, 0);
                }
            } else
                this->mResearchManager->ResearchUnitClass(kTechFutureResearchSentinel);
        }
        if (theTree != NULL) {
            EETechTreeNode* theCenter = theTree->Get(gUnitClassTable[kUnitTypeCitizen] ? gUnitClassTable[kUnitTypeCitizen]->mTechnology : 100000);
            this->mResourceManager->AddCosts(theCenter, this->mUnitManager->GetWantedCitizens());
        }
        this->mFirstUpdateDone = true;
    }
    if (this->mScheduleEpochAdvance) {
        this->SetNextEpochAdvanceTime(inTime, false);
        this->mScheduleEpochAdvance = false;
    }
    {
        EECPTraceScope theScope("unit");
        this->mUnitManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("resource");
        this->mResourceManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("build");
        this->mBuildManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("research");
        this->mResearchManager->Update(inTime);
    }
    {
        EECPTraceScope theScope("wall");
        this->mWallManager->Update(inTime);
    }
    if (!this->mAdvancingEpoch && (this->mBuildManager->HasLaterEpochRequests() || this->mResearchManager->HasLaterEpochIDs() || this->mUnitManager->HasLaterEpochRequests()
            || this->IsBehindInEpoch()))
        this->AttemptEpochAdvance(inTime);
    if (this->mBuildingCountsChecked) {
        this->mBuildingCountsChecked = false;
        this->mLastCountCheck = inTime;
    } else if (this->mLastCountCheck == 0 || inTime - this->mLastCountCheck > 30000) {
        ESDebugOut("EECPCivilizationManager::Update:  checking building counts at time %ld\n", inTime);
        this->CheckBuildingCounts();
    }
    if (this->mPlayer->mCanBuild && inTime >= this->mNextRepairTime) {
        this->RepairBuildings();
        this->mNextRepairTime = inTime + 31000;
    }
    if (this->mPlayer->mCanBuild) {
        if (inTime >= this->mNextGarrisonTime) {
            if (this->mNextGarrisonTime > 0)
                this->ChooseGarrison();
            this->mNextGarrisonTime = inTime + this->mPlayer->mTownCenterUpgradeTimer * 60000;
        }
        if (this->mPlayer->mCanBuild && inTime >= this->mNextFortressTime) {
            if (this->mNextFortressTime > 0)
                this->ChooseFortress();
            this->mNextFortressTime = inTime + this->mPlayer->mCapitolUpgradeTimer * 60000;
        }
    }
    if (!this->mMoraleBuildingsEnabled || !(inTime >= this->mNextMoraleBuildingTime))
        return;
    if (this->mNextMoraleBuildingTime > 0)
        this->QueueMoraleBuilding();
    this->mNextMoraleBuildingTime = inTime + 59000;
}

// FUNCTION: EXE 0x4711ac
// Picks a random connected settlement to garrison (on island maps one with enough available citizens) when none is
// held and no citizen is requested, then fills it if there are enough workers or it can afford the citizens
void EECPCivilizationManager::ChooseGarrison()
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEComplexUnit*> theCandidates;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    U2DPoint<long> theTile = this->mBuildManager->mTownCenterTile;
    EEMap* theMap = gWorld.mMap;
    if (theTile.mX == -10000 || theTile.mY == -10000)
        return;
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTile.mX, theTile.mY);
    EETechTreeNode* theNode = theTree->Get(gUnitClassTable[kUnitTypeCitizen] ? gUnitClassTable[kUnitTypeCitizen]->mTechnology : 100000);
    if (this->mGarrison != NULL && this->mGarrison->mHitPoints <= 0)
        this->ReleaseGarrison();
    if (this->mGarrison != NULL || this->mUnitManager->IsRequested(0x20) || this->mUnitManager->IsRequested(0x1e0))
        return;
    // economy.early_town_centers (docs/AI_RESEARCH.md §8.29; the owner: "upgrading a settlement to a capitol early game
    // is mostly a waste of food"; the playtest AI garrisoned its first settlement into a town centre at minute 5, and
    // had 4 by 71 minutes; "one or two town centers are okay"): before the Bronze Age a settlement becomes a town
    // centre only while the player has fewer than that many
    long theEarly = EECPEarlyTownCenters(this->mPlayer);
    if (theEarly > 0 && theTree->mCurrentEpoch < kTechEpochBronze) {
        std::vector<EEUnit*> theCenters;
        EEGetPlayerUnitsOfType(kUnitTypeTownCenter, this->mPlayer->mPlayerID, theCenters);
        EEGetPlayerUnitsOfType(kUnitTypeCapitol, this->mPlayer->mPlayerID, theCenters);
        if ((long)theCenters.size() >= theEarly)
            return;
    }
    if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)) {
        for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters() && this->mGarrison == NULL; i++) {
            EEComplexUnit* theCenter = this->mPlayer->GetResourceCenter(i)->mUnit;
            if (theCenter == NULL || theCenter->mUnitClass->mUnitType != kUnitTypeSettlement || theCenter->HasEffect(7))
                continue;
            this->mUnitManager->GetCitizens(theUnits);
            std::vector<EEComplexUnit*>::iterator it = theUnits.begin();
            while (it != theUnits.end()) {
                if (this->mUnitManager->IsCitizenAvailable(*it)
                    && gContinentManager.AreConnected(gContinentManager.GetContinentAt(*it),
                        gContinentManager.GetContinentAt(theCenter), this->mPlayer->GetCitizenPassability()))
                    it++;
                else
                    it = theUnits.erase(it);
            }
            if (theUnits.size() >= (unsigned long)(theCenter->mUnitClass->mGarrisonCapacity - theCenter->mGarrisonCount))
                theCandidates.push_back(theCenter);
        }
    } else {
        for (unsigned long j = 0; j < this->mPlayer->GetNumResourceCenters() && this->mGarrison == NULL; j++) {
            EEComplexUnit* theCenter = this->mPlayer->GetResourceCenter(j)->mUnit;
            if (theCenter != NULL && theCenter->mUnitClass->mUnitType == kUnitTypeSettlement && !theCenter->HasEffect(7)
                && gContinentManager.AreConnected(gContinentManager.GetContinentAt(theCenter), theContinent,
                    this->mPlayer->GetCitizenPassability()))
                theCandidates.push_back(theCenter);
        }
    }
    if (theCandidates.size() <= 0)
        return;
    EEComplexUnit* theCenter = theCandidates[gPhysics->GetRandomElementIndex(theCandidates)];
    if (theCenter == NULL)
        return;
    unsigned long theRoom = theCenter->mUnitClass->mGarrisonCapacity - theCenter->mGarrisonCount;
    unsigned long theNeeded = UMFloatToLong(theRoom * 2.5f);
    if (!(this->mUnitManager->GetNumWorkers() >= theNeeded) && !this->mPlayer->CanAfford(theNode, 0x29, theRoom))
        return;
    this->mGarrison = theCenter;
    theCenter->Reference();
    this->FillGarrison(this->mGarrison);
}

// FUNCTION: EXE 0x4714c2
// Picks a random town centre as the fortress when none is held and no citizen is requested, then fills it if there
// are enough workers or it can afford the citizens
void EECPCivilizationManager::ChooseFortress()
{
    std::vector<EEComplexUnit*> theCandidates;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    U2DPoint<long> theTile = this->mBuildManager->mTownCenterTile;
    if (theTile.mX == -10000 || theTile.mY == -10000)
        return;
    EETechTreeNode* theNode = theTree->Get(gUnitClassTable[kUnitTypeCitizen] ? gUnitClassTable[kUnitTypeCitizen]->mTechnology : 100000);
    if (this->mFortress != NULL && this->mFortress->mHitPoints <= 0)
        this->ReleaseFortress();
    if (this->mFortress != NULL || this->mUnitManager->IsRequested(0x20) || this->mUnitManager->IsRequested(0x1e0))
        return;
    // economy.early_town_centers: no town centre becomes a capitol before the Bronze Age
    if (EECPEarlyTownCenters(this->mPlayer) > 0 && theTree->mCurrentEpoch < kTechEpochBronze)
        return;
    for (unsigned long i = 0; i < this->mPlayer->GetNumResourceCenters() && this->mFortress == NULL; i++) {
        EEComplexUnit* theCenter = this->mPlayer->GetResourceCenter(i)->mUnit;
        if (theCenter != NULL && theCenter->mUnitClass->mUnitType == kUnitTypeTownCenter)
            theCandidates.push_back(theCenter);
    }
    if (theCandidates.size() <= 0)
        return;
    EEComplexUnit* theCenter = theCandidates[gPhysics->GetRandomElementIndex(theCandidates)];
    if (theCenter == NULL)
        return;
    unsigned long theRoom = theCenter->mUnitClass->mGarrisonCapacity - theCenter->mGarrisonCount;
    unsigned long theNeeded = UMFloatToLong(theRoom * 2.5f);
    if (!(this->mUnitManager->GetNumWorkers() >= theNeeded) && !this->mPlayer->CanAfford(theNode, 0x23, theRoom))
        return;
    this->mFortress = theCenter;
    theCenter->Reference();
    this->FillGarrison(this->mFortress);
}

// TEMPLATE: EXE 0x471663
// TEMPLATE: EXE 0x47168b
// TEMPLATE: EXE 0x47176f
// TEMPLATE: EXE 0x471796
// TEMPLATE: EXE 0x4717f5
// TEMPLATE: EXE 0x471828
// TEMPLATE: EXE 0x471930
// TEMPLATE: EXE 0x4719e1
// TEMPLATE: EXE 0x471a25
// TEMPLATE: EXE 0x471a72
// TEMPLATE: EXE 0x471acf
// TEMPLATE: EXE 0x471b02
// TEMPLATE: EXE 0x471b7c
// TEMPLATE: EXE 0x471bec
// TEMPLATE: EXE 0x471c3b
