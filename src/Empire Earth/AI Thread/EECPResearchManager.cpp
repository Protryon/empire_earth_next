// C:\EEX\Empire Earth\AI Thread\EECPResearchManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPResearchManager, vtable 0x43c27c; its ESDebugOut strings name the class and two members). A computer player's
// research manager: 0x89fc4..0x8bbbf (the class-static key's guarded chain first; EECPPlayerInteractionManager.cpp ends at
// 0x89fc4, EECPResourceCenter's code starts at 0x8bbbf). The vector/sort COMDATs close the unit.
// Known differences (inline budget): ResearchCheapest (the original refuses theIDs[j]'s begin() and make_pair's pair
// ctor (0xaa817) in its first loop), TryResearch (the original expands GetNumUnitClasses and refuses its size()),
// Research (the original keeps 15 in ebx), IsWanted (the return-false block placement).
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
#include "ee/EEEvent.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPResearchManager.h"
#include "ee/EEAIRandom.h"
#include "LostDebug.h"   // NOT original source: EE_LOST_DEBUG stand-in (ChooseResearch; docs/LOST_DEBUG.md)
const UString EECPResearchManager::sData("EECPResearchManager-Data");
// SYNTHETIC: EXE 0x489fc4   (wrapper)
// SYNTHETIC: EXE 0x489fce   (ctor)
// SYNTHETIC: EXE 0x489fdf   (registrar)
// SYNTHETIC: EXE 0x489feb   (dtor)

static long sHistoryCounter;                // 0x4e5d50 (AddHistory)

// Orders ResearchCheapest's {cost, id} entries by cost (an empty predicate: the sort COMDATs 0x8b582.. compare .first only,
// and pass a dword of garbage for it) (provisional name)
struct EECPRMEntryLess {
    bool operator()(const std::pair<long, long>& inFirst, const std::pair<long, long>& inSecond) const
    {
        return inFirst.first < inSecond.first;
    }
};

// FUNCTION: EXE 0x48a007
// Creates the research manager for the civilization manager's player, disabled and with no epoch seen yet
EECPResearchManager::EECPResearchManager(EECPCivilizationManager* inManager)
    : mManager(inManager)
{
    this->mPlayer = inManager->mPlayer;
    this->mTechTree = this->mPlayer->mTechnologies;
    this->mEpoch = -1;
    this->mNextTime = 0;
    this->mRecordsHistory = false;
    this->mStarted = false;
    this->mEnabled = false;
}

// SYNTHETIC: EXE 0x48a09c
// (the scalar deleting destructor)

// FUNCTION: EXE 0x48a0b8
// Destroys the research manager (members only)
EECPResearchManager::~EECPResearchManager()
{
}

// FUNCTION: EXE 0x48a0f0
// Picks a random research: a producible candidate that upgrades citizen attributes, else a shuffled one whose
// researching building the player has; starts it if its costs stay below the resource caps and records it
void EECPResearchManager::ChooseResearch()
{
    EETechTreeNode* theNode = NULL;
    bool theFound = false;
    long theID = 100000;
    bool theAffordable = true;
    if (!this->mEnabled)
        return;
    unsigned long i;
    unsigned long theCount;
    for (i = 0, theCount = this->mCandidates.size(); i < theCount && !theFound; i++) {
        theID = this->mCandidates[i];
        theNode = this->mTechTree->Get(theID);
        if (theNode == NULL || !this->mPlayer->CanProduce(theID, theNode->GetUnitClass(0), 1, true, 3, NULL))
            continue;
        EEEvent* theEvent = EEEvent::Get(theNode->mData->mResearchedEvent);
        if (theEvent == NULL)
            continue;
        for (unsigned long e = 0; e < theEvent->GetNumEffects() && !theFound; e++) {
            EEEventEffect* theEffect = EEEvent::GetEventEffect(theEvent->GetEffect(e));
            if (theEffect->mKind == 2 && (theEffect->mUnitClass == 0x20 || theEffect->mUnitClass == 0x1e0)
                && (theEffect->mAttribute == 0x1a || theEffect->mAttribute == 0x1b || theEffect->mAttribute == 0x1c
                    || theEffect->mAttribute == 0x1d || theEffect->mAttribute == 0x20 || theEffect->mAttribute == 0x21
                    || theEffect->mAttribute == 0x1f))
                theFound = true;
            else
                theFound = false;
        }
    }
    if (!theFound) {
        EEAIRandomShuffle(this->mCandidates.begin(), this->mCandidates.end());
        // the second loop reuses theCount (the original's one [ebp-0x14] slot for both sizes)
        unsigned long j;
        for (j = 0, theCount = this->mCandidates.size(); j < theCount && !theFound; j++) {
            theID = this->mCandidates[j];
            // LOST-DEBUG: none: exact only (the original refuses this mCandidates[j]'s _Ptrit::operator+; inline_sim)
            EE_LOST_DEBUG(2);
            theNode = this->mTechTree->Get(theID);
            if (theNode != NULL && this->mPlayer->CanProduce(theID, theNode->GetUnitClass(0), 1, true, 3, NULL)) {
                for (unsigned long k = 0; k < theNode->GetNumUnitClasses() && !theFound; k++)
                    theFound = this->mManager->GetUnitManager()->GetCount(theNode->GetUnitClass(k)) > 0;
            }
        }
        if (!theFound)
            return;
    }
    long theCosts[7];
    memset(theCosts, 0, sizeof(theCosts));
    this->mTechTree->SumCosts(theID, theNode->GetUnitClass(0), theCosts);
    for (unsigned long r = 2; r < 7 && theAffordable; r++)
        theAffordable = this->mManager->IsBelowResourceCap(r, this->mPlayer->ScaleCost(theCosts[r]));
    if (!theAffordable)
        return;
    this->Research(theID);
    this->AddHistory(theID);
    std::vector<long>::iterator theIterator = std::find(this->mCandidates.begin(), this->mCandidates.end(), theID);
    if (theIterator != this->mCandidates.end())
        this->mCandidates.erase(theIterator);
}

// SYNTHETIC: EXE 0x48a42e
// (EETechTreeNode::GetNumUnitClasses, inline in ee/EEPlayer.h: emitted after its first user)

// FUNCTION: EXE 0x48a447
// Collects the wanted, not yet researched techs of the current and earlier epochs as research candidates
void EECPResearchManager::BuildCandidates()
{
    this->mCandidates.clear();
    for (long i = this->mEpoch; i >= 0; i--)
        this->mTechTree->GetEpochNodeIDs(i, this->mCandidates);
    std::vector<long>::iterator theIterator = this->mCandidates.begin();
    while (theIterator != this->mCandidates.end()) {
        EETechTreeNode* theNode = this->mTechTree->Get(*theIterator);
        if (this->IsWanted(*theIterator) && (theNode == NULL || !theNode->mIsResearched))
            theIterator++;
        else
            theIterator = this->mCandidates.erase(theIterator);
    }
}

// FUNCTION: EXE 0x48a4bf
// Returns whether the player could produce the tech with its costs added to the resource caps; outTotal gets
// its total unscaled cost
bool EECPResearchManager::CanAfford(EETechTreeNode* inNode, long& outTotal)
{
    outTotal = 0;
    long i;
    for (i = 2; i < 7; i++) {
        long theCost = this->mPlayer->ScaleCost(inNode->GetCost(i, 0x20, 1.0f));
        this->mPlayer->mResourceCaps[i] += theCost;
        outTotal += inNode->GetCost(i, 0x20, 1.0f);
    }
    long theID = inNode->mData->mID;
    bool theResult = this->mPlayer->CanProduce(theID, inNode->GetUnitClass(0), 1, true, 3, NULL);
    for (i = 2; i < 7; i++) {
        long theCost = this->mPlayer->ScaleCost(inNode->GetCost(i, 0x20, 1.0f));
        this->mPlayer->mResourceCaps[i] -= theCost;
    }
    return theResult;
}

// FUNCTION: EXE 0x48a5c5
// Reads the candidates, the per-epoch queues (techs that make units are dropped; 14 epochs before version 0.234),
// the next research time, the epoch and the enabled flag; returns the first read error
long EECPResearchManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    this->mTechTree = this->mPlayer->mTechnologies;
    long theID;
    unsigned long theCount;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        this->mCandidates.clear();
        for (i = 0; i < theCount && theResult == kErrorOK; i++) {
            theResult = inFile.Read(theID);
            if (theResult != kErrorOK)
                continue;
            EETechTreeNode* theNode = this->mTechTree->Get(theID);
            if (theNode != NULL && !theNode->mData->mMakesUnit)
                this->mCandidates.push_back(theID);
        }
    }
    if (theResult == kErrorOK) {
        for (i = 0; i < 15 && theResult == kErrorOK; i++) {
            if (inMajorVersion == 0 && inMinorVersion < 0xea && i == 14)
                continue;
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mEpochIDs[i].clear();
                for (unsigned long j = 0; j < theCount && theResult == kErrorOK; j++) {
                    theResult = inFile.Read(theID);
                    if (theResult != kErrorOK)
                        continue;
                    EETechTreeNode* theNode = this->mTechTree->Get(theID);
                    if (theNode != NULL && !theNode->mData->mMakesUnit)
                        this->mEpochIDs[i].push_back(theID);
                }
            }
        }
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mEpoch);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mEnabled);
    return theResult;
}

// FUNCTION: EXE 0x48a73f
// Returns whether any epoch after the current one has queued techs
bool EECPResearchManager::HasLaterEpochIDs()
{
    bool theResult = false;
    for (long i = this->mEpoch + 1; i < 15 && !theResult; i++) {
        if (this->mEpochIDs[i].size() > 0)
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x48a77a
// Queues research of the tech that unlocks a unit class
void EECPResearchManager::ResearchUnitClass(long inUnitClass)
{
    this->Research(gUnitClassTable[inUnitClass] ? gUnitClassTable[inUnitClass]->mTechnology : 100000);
}

// FUNCTION: EXE 0x48a79f
// Queues a wanted tech in its epoch's list (epoch advances first), first ordering a building or unit that can
// research it if the player has none, and reserves its costs
void EECPResearchManager::Research(long inID)
{
    bool theDone = false;
    if (!this->IsWanted(inID))
        return;
    EETechTreeNode* theNode = this->mTechTree->Get(inID);
    unsigned long i;
    for (i = 0; i < theNode->GetNumUnitClasses() && !theDone; i++)
        theDone = this->mManager->mUnitManager->GetUnits(theNode->GetUnitClass(i)) > 0;
    if (!theDone) {
        for (i = 0; i < theNode->GetNumUnitClasses() && !theDone; i++) {
            long theClass = theNode->GetUnitClass(i);
            if (theClass == kUnitTypeTownCenter || theClass == kUnitTypeCapitol || gUnitClassTable[theClass] == NULL)
                continue;
            if (gUnitClassTable[theClass]->IsLandBuilding())
                this->mManager->mBuildManager->QueueBuilding(theClass, 0, 0);
            else
                this->mManager->mUnitManager->RequestUnits(theClass, 1, 0, 0);
            theDone = true;
        }
        if (!theDone)
            return;
    }
    long theEpoch = (inID >= kTechEpochPrehistoric && inID < kTechEpochAll) ? inID - 1 : inID / 1000 - 1;
    if (inID >= kTechEpochPrehistoric && inID < kTechEpochAll)
        this->mEpochIDs[theEpoch].insert(this->mEpochIDs[theEpoch].begin(), inID);
    else
        this->mEpochIDs[theEpoch].push_back(inID);
    if (inID < kTechEpochPrehistoric || inID >= kTechEpochAll)
        this->mManager->mResourceManager->AddCosts(theNode, 1);
}

// FUNCTION: EXE 0x48a91e
// Builds inCount unit-making techs that can be sited, cheapest affordable first (repeating the list if short);
// with none, adds inCount to the next epoch's SOL count (a null epoch at the last epoch)
void EECPResearchManager::ResearchCheapest(unsigned long inCount)
{
    std::vector<long> theIDs;
    std::vector<long> theChosen;
    std::vector<std::pair<long, long> > theEntries;
    bool theB = false;
    EETileCoordinate theArg;
    theArg.mX = 0;
    theArg.mY = 0;
    long theTotal;
    for (long i = this->mEpoch; i >= 0; i--)
        this->mTechTree->GetEpochNodeIDs(i, theIDs);
    unsigned long theCount;
    unsigned long j;
    for (j = 0, theCount = theIDs.size(); j < theCount; j++) {
        EETechTreeNode* theNode = this->mTechTree->Get(theIDs[j]);
        long theClass = theNode->mData->mUnitClass;
        switch (theClass) {
        case kUnitTypeTownCenter:
        case kUnitTypeCapitol:
        case kUnitTypeFarm:
        case kUnitTypeSettlement:
        case kUnitTypeGranary:
        case kUnitTypeRoboticFarm:
            continue;
        case kUnitTypeDock:
        case kUnitTypeNavyYard:
            if ((gWorld.GetMap()->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap)
                continue;
            break;
        }
        if (theClass != -1 && theNode->mData->mCountsTowardEpoch && theNode->mData->mMakesUnit && theNode->mData->mKind != 4
            && theNode->mData->mKind != 6 && this->CanAfford(theNode, theTotal)
            && this->mManager->GetBuildManager()->FindSite(theNode->mData->mUnitClass, theArg))
            theEntries.push_back(std::make_pair(theTotal, theIDs[j]));
    }
    if (theEntries.empty()) {
        unsigned long theSize;
        unsigned long k;
        for (k = 0, theSize = theIDs.size(); k < theSize; k++) {
            EETechTreeNode* theNode = this->mTechTree->Get(theIDs[k]);
            long theClass = theNode->mData->mUnitClass;
            switch (theClass) {
            case kUnitTypeTownCenter:
            case kUnitTypeCapitol:
            case kUnitTypeFarm:
            case kUnitTypeSettlement:
            case kUnitTypeGranary:
            case kUnitTypeRoboticFarm:
                continue;
            case kUnitTypeDock:
            case kUnitTypeNavyYard:
                if ((gWorld.GetMap()->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap)
                    continue;
                break;
            }
            if (theClass != -1 && theNode->mData->mCountsTowardEpoch && theNode->mData->mMakesUnit && theNode->mData->mKind != 4
                && theNode->mData->mKind != 6 && this->mManager->GetBuildManager()->FindSite(theClass, theArg))
                theEntries.push_back(std::make_pair(0L, theIDs[k]));
        }
    }
    if (theEntries.empty()) {
        EETechTreeEpoch* theEpoch = NULL;
        long theNext = this->mTechTree->mCurrentEpoch + 1;
        if (theNext >= 0 && theNext < 15)
            theEpoch = &this->mTechTree->mEpochs[theNext];
        // in the last epoch there is no next one to credit
        for (unsigned long n = 0; theEpoch != NULL && n < inCount; n++)
            theEpoch->AddSOL();
    } else {
        std::sort(theEntries.begin(), theEntries.end(), EECPRMEntryLess());
        theTotal = theEntries[0].first;
        unsigned long theNumEntries;
        unsigned long m;
        for (m = 0, theNumEntries = theEntries.size(); m < theNumEntries && theChosen.size() < inCount; m++) {
            EETechTreeNode* theNode = this->mTechTree->Get(theEntries[m].second);
            if (!theNode->mIsResearched && gUnitClassTable[theNode->mData->mUnitClass] != NULL)
                theChosen.push_back(theEntries[m].second);
        }
        for (m = 0; theChosen.size() < inCount; ) {
            theChosen.push_back(theEntries[m++].second);
            theB = true;
            if (m >= theEntries.size())
                m = 0;
        }
        for (j = 0, theCount = theChosen.size(); j < theCount; j++) {
            EETechTreeNode* theNode = this->mTechTree->Get(theChosen[j]);
            if (theNode->mData->mMakesUnit) {
                long theClass = theNode->mData->mUnitClass;
                if (gUnitClassTable[theClass]->IsLandBuilding())
                    this->mManager->GetBuildManager()->QueueBuilding(theClass, theB, 0);
                else
                    this->mManager->GetUnitManager()->RequestUnits(theClass, 1, 0, 0);
            } else
                this->Research(theChosen[j]);
        }
    }
}

// FUNCTION: EXE 0x48ade8
// Rebuilds the candidates, drops unaffordable unit-making techs from the epoch queues and builds as many of the
// cheapest instead
void EECPResearchManager::ReplaceUnaffordable()
{
    long theCount = 0;
    this->BuildCandidates();
    for (long i = 0; i <= this->mEpoch; i++) {
        for (std::vector<long>::iterator theIterator = this->mEpochIDs[i].begin(); theIterator != this->mEpochIDs[i].end();
             theIterator++) {
            EETechTreeNode* theNode = this->mTechTree->Get(*theIterator);
            if (!theNode->mData->mCountsTowardEpoch || !theNode->mData->mMakesUnit)
                continue;
            long theTotal;
            if (!this->CanAfford(theNode, theTotal)) {
                this->mEpochIDs[i].erase(theIterator--);
                theCount++;
            }
        }
    }
    if (theCount > 0)
        this->ResearchCheapest(theCount);
}

// FUNCTION: EXE 0x48ae93
// Re-fetches the player's tech tree after loading; returns OK
long EECPResearchManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->mTechTree = this->mPlayer->mTechnologies;
    return kErrorOK;
}

// FUNCTION: EXE 0x48aeaa
// Queues a tech at an idle built unit that can research it; if there is none, orders that building or unit (or
// schedules the epoch advance); returns whether the research was queued
bool EECPResearchManager::TryResearch(long inID)
{
    bool theResult = false;
    std::deque<EEComplexUnit*>::iterator theIterator;
    EETechTreeNode* theNode = this->mTechTree->Get(inID);
    if (theNode->GetIsAvailable()) {
        for (unsigned long i = 0; i < theNode->GetNumUnitClasses() && !theResult; i++) {
            long theUnitClass = theNode->GetUnitClass(i);
            EETechTreeNode* theOther = this->mTechTree->Get(gUnitClassTable[theUnitClass] ? gUnitClassTable[theUnitClass]->mTechnology
                                                                                    : 100000);
            if (theOther == NULL)
                continue;
            long theClass = theOther->mData->mUnitClass;
            if (theClass != kUnitTypeCitizen && theClass != kUnitTypeFemaleCitizen) {
                std::deque<EEComplexUnit*>* theUnits = this->mManager->mUnitManager->GetUnits(theClass);
                if (theUnits == NULL)
                    continue;
                long theCount = theUnits->size();
                if (theCount == 0) {
                    if (inID >= kTechEpochPrehistoric && inID < kTechEpochAll) {
                        this->mManager->mAdvancingEpoch = false;
                        this->mManager->mScheduleEpochAdvance = true;
                    } else if (gUnitClassTable[theClass]->IsLandBuilding())
                        this->mManager->mBuildManager->QueueBuilding(theClass, 0, 0);
                    else
                        this->mManager->mUnitManager->RequestUnits(theClass, 1, 0, 0);
                } else {
                    theIterator = theUnits->begin();
                    if (theCount > 1) {
                        while (theIterator != theUnits->end() && !theResult) {
                            EEComplexUnit* theUnit = *theIterator;
                            if (((theUnit->mCommand ? theUnit->mCommand->mType : 0) == kActionIdle
                                 || (theUnit->mCommand ? theUnit->mCommand->mType : 0) == kActionWaitForGarrisonCost)
                                && theUnit->mIsBuilt) {
                                theUnit->QueueResearch(inID, inID >= kTechEpochPrehistoric && inID < kTechEpochAll);
                                theResult = true;
                            }
                            ++theIterator;
                        }
                    } else {
                        EEComplexUnit* theUnit = *theIterator;
                        if (theUnit->mIsBuilt) {
                            theUnit->QueueResearch(inID, inID >= kTechEpochPrehistoric && inID < kTechEpochAll);
                            theResult = true;
                        }
                    }
                }
            } else if (theNode->mData->mMakesUnit) {
                this->mManager->mBuildManager->QueueBuilding(theNode->mData->mUnitClass, 0, 0);
                theResult = true;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x48b0df
// Writes the candidates, the 15 epoch queues, the next research time, the epoch and the enabled flag; returns
// the first write error
long EECPResearchManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mCandidates.size();
    long theResult = outFile.Write(theCount);
    unsigned long i;
    if (theResult == kErrorOK) {
        for (i = 0; i < theCount && theResult == kErrorOK; i++)
            theResult = outFile.Write(this->mCandidates[i]);
    }
    if (theResult == kErrorOK) {
        for (unsigned long j = 0; j < 15 && theResult == kErrorOK; j++) {
            theCount = this->mEpochIDs[j].size();
            theResult = outFile.Write(theCount);
            if (theResult == kErrorOK) {
                for (i = 0; i < theCount && theResult == kErrorOK; i++)
                    theResult = outFile.Write(this->mEpochIDs[j][i]);
            }
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNextTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEpoch);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnabled);
    return theResult;
}

// FUNCTION: EXE 0x48b1fb
// Schedules the next random research the player's research frequency (seconds, scaled by its modifier) after inTime
void EECPResearchManager::SetNextRandomResearchTime(unsigned long inTime)
{
    long theDelay = this->mPlayer->mResearchFrequency;
    theDelay = UMFloatToLong(this->mPlayer->GetResearchFrequencyModifier() * theDelay);
    this->mNextTime = theDelay * 1000 + inTime;
    ESDebugOut("EECPResearchManager::SetNextRandomResearchTime  setting next research time to %d (%f)\n", this->mNextTime,
               theDelay);
}

// FUNCTION: EXE 0x48b24e
// On reaching a valid epoch, schedules the next epoch advance (not while loading) and resets the current epoch's
// SOL count
void EECPResearchManager::EpochReached(long inEpoch)
{
    if (!(inEpoch >= kTechEpochPrehistoric) || !(inEpoch < kTechEpochAll))
        return;
    if (!gLoadingSavedGame) {
        EECPCivilizationManager* theManager = this->mManager;
        theManager->mAdvancingEpoch = false;
        theManager->mScheduleEpochAdvance = true;
    }
    this->mTechTree->mEpochs[this->mTechTree->mCurrentEpoch].mCurrentSOL = 0;
}

// FUNCTION: EXE 0x48b288
// Returns whether a tech is worth researching: not an epoch or unit tech and researched at a non-citizen unit
// that is not an epoch building
bool EECPResearchManager::IsWanted(long inID)
{
    EETechTree* theTree = this->mPlayer->mTechnologies;
    EETechTreeNode* theNode = theTree->Get(inID);
    if (theNode == NULL || theNode->mData->mID <= kTechEpochPrehistoric || theNode->mData->mMakesUnit)
        return false;
    if (theNode->mData->mID == 0x7eb)
        return false;
    bool theResult = false;
    for (unsigned long i = 0; i < theNode->GetNumUnitClasses() && !theResult; i++) {
        long theClass = theNode->GetUnitClass(i);
        EETechTreeNode* theOther = theTree->Get(gUnitClassTable[theClass] ? gUnitClassTable[theClass]->mTechnology : 100000);
        if (theOther != NULL && (theOther->mData->mID < kTechEpochPrehistoric || theOther->mData->mID >= kTechEpochAll) && theClass != kUnitTypeCitizen
            && theClass != kUnitTypeFemaleCitizen)
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x48b34b
// Starts the first queued tech it can (rebuilding candidates on a new epoch) and, when due, a random research;
// only the first computer player records history
void EECPResearchManager::Update(unsigned long inTime)
{
    if (!this->mStarted) {
        this->mRecordsHistory = false;
        for (unsigned long i = 0; i < gWorld.GetNumPlayers(); i++) {
            EEPlayer* thePlayer = gWorld.mPlayers[i];
            if (thePlayer->mComputerPlayer == NULL)
                continue;
            if (thePlayer->mPlayerID == this->mPlayer->mPlayerID)
                this->mRecordsHistory = true;
            break;
        }
        this->SetNextRandomResearchTime(inTime);
        this->mStarted = true;
    }
    if (this->mEpoch != this->mTechTree->mCurrentEpoch) {
        this->mEpoch = this->mTechTree->mCurrentEpoch;
        this->BuildCandidates();
    }
    long j;
    bool theDone;
    for (j = 0, theDone = false; j <= this->mEpoch && !theDone; j++) {
        for (std::vector<long>::iterator theIterator = this->mEpochIDs[j].begin();
             theIterator != this->mEpochIDs[j].end() && !theDone; theIterator++) {
            if (this->TryResearch(*theIterator)) {
                this->mEpochIDs[j].erase(theIterator);
                theDone = true;
            }
        }
    }
    if (!(inTime > this->mNextTime))
        return;
    ESDebugOut("EECPResearchManager::Update  attempting random research at %d\n", inTime);
    this->ChooseResearch();
    this->SetNextRandomResearchTime(inTime);
}

// FUNCTION: EXE 0x48b467
// Records a researched tech at the front of the last-5 history (when this player records history)
void EECPResearchManager::AddHistory(long inID)
{
    UWideString theName;
    UWideString theText;
    if (!this->mRecordsHistory)
        return;
    sHistoryCounter++;
    if (this->mHistory.size() == 5)
        this->mHistory.erase(this->mHistory.end() - 1);
    this->mHistory.insert(this->mHistory.begin(), EECPRMHistoryEntry(sHistoryCounter, inID));
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x48b51f
// TEMPLATE: EXE 0x48b52f
// TEMPLATE: EXE 0x48b55b
// TEMPLATE: EXE 0x48b582
// TEMPLATE: EXE 0x48b642
// TEMPLATE: EXE 0x48b664
// TEMPLATE: EXE 0x48b696
// TEMPLATE: EXE 0x48b6bf
// TEMPLATE: EXE 0x48b797
// TEMPLATE: EXE 0x48b7cb
// TEMPLATE: EXE 0x48b866
// TEMPLATE: EXE 0x48b8a0
// TEMPLATE: EXE 0x48b8e1
// TEMPLATE: EXE 0x48b9c6
// TEMPLATE: EXE 0x48b9f5
// TEMPLATE: EXE 0x48ba40
// TEMPLATE: EXE 0x48ba69
// TEMPLATE: EXE 0x48bae3
// TEMPLATE: EXE 0x48bb39
// TEMPLATE: EXE 0x48bb82
