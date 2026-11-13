// C:\EEX\Empire Earth\AI Thread\EECPExplorationManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPExplorationManager, vtable 0x43c1b0). A computer player's exploration manager: 0x7ed38..0x8013d
// (EECPDiplomacyManager.cpp ends at 0x7ed38; the vector helpers and the FSLoadVector/FSSaveVector<unsigned long>
// COMDATs 0x7ffd0..0x8013d close the unit; EECPIArmy.cpp starts at 0x8013d).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
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

#include "ee/EmpireEarth.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEVectorIO.h"
#include "ee/EECPExplorationManager.h"
#include "ee/EEAIKindIDs.h"

extern long& gDCGameSpeed;                   // 0x530d24 (> 0: sea explorers; EECPDefenseCommander.cpp's name) (provisional name)
extern long& gNumSpaceTiles;                 // 0x530d2c (> 0: space explorers) (provisional name)

// FUNCTION: EXE 0x47ed38
// Creates the computer player's exploration manager, disabled and with no explorers
EECPExplorationManager::EECPExplorationManager(EEComputerPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
    this->mB2C = false;
    this->mNextTime = 0;
    this->mEnabled = false;
}

// SYNTHETIC: EXE 0x47ed7a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x47ed96
// Destroys the exploration manager, releasing each open explorer request back to the personnel manager
EECPExplorationManager::~EECPExplorationManager()
{
    for (unsigned long i = 0; i < this->mRequests.size(); i++) {
        EECPPersonnelRequest* theRequest = this->mRequests[i];
        if (theRequest != NULL) {
            this->mPlayer->mCombatManager->mPersonnel->ReleaseRequest(theRequest->mID);
            theRequest->Dereference();
        }
    }
}

// NEAR: inline budget: the original calls the screener base ctor 0x4783b8 and dtor 0x47f148, vector begin()/
// operator+ and mUnitClassUpgrades's operator[] out of line; this build expands them.
// FUNCTION: EXE 0x47edf7
// Asks the personnel manager for one explorer of the kind (land, sea, air, space): the fastest acceptable unit
// class of that domain (no fishing boats), on the largest sea or space continent for those kinds
void EECPExplorationManager::RequestExplorer(long inKind)
{
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;
    std::vector<long> theWanted;
    std::vector<long> theUnits;
    float theBest = 0.0f;
    float theValue;
    long theBestClass;
    long theContinent = 0;
    unsigned long i;

    this->GetPlayer()->GetScreenedTypesByLine(theClasses, EEAIFOAcceptableComputerPlayerExplorerUnitScreener(this->GetPlayer()));
    if (theClasses.size() > 0) {
        for (it = theClasses.begin(); it != theClasses.end(); ++it) {
            std::vector<long>* theList = it->second;
            if (theList->size() <= 0)
                continue;
            for (i = 0; i < theList->size(); i++) {
                EEUnitClass* theClass = gUnitClassTable[(*theList)[i]];
                if (theClass == NULL || theClass->GetCategory() == kUnitCategoryNavalCitizen)
                    continue;
                switch (inKind) {
                case kExplorerSpace:
                    if (theClass->GetDomains() & kMovementSpace) {
                        theValue = this->GetPlayer()->mUnitClassUpgrades[theClass->GetUnitType()]->GetMaxVelocity();
                        if (theValue > theBest) {
                            theBest = theValue;
                            theBestClass = (*theList)[i];
                        }
                    }
                    theContinent = gContinentManager.GetLargestContinent(kContinentCategorySpace);
                    break;
                case kExplorerAir:
                    if (theClass->GetDomains() & kMovementAir) {
                        theValue = this->GetPlayer()->mUnitClassUpgrades[theClass->GetUnitType()]->GetMaxVelocity();
                        if (theValue > theBest) {
                            theBest = theValue;
                            theBestClass = (*theList)[i];
                        }
                    }
                    break;
                case kExplorerSea:
                    if (theClass->GetDomains() & kMovementSea) {
                        theValue = this->GetPlayer()->mUnitClassUpgrades[theClass->GetUnitType()]->GetMaxVelocity();
                        if (theValue > theBest) {
                            theBest = theValue;
                            theBestClass = (*theList)[i];
                        }
                    }
                    theContinent = gContinentManager.GetLargestContinent(kContinentCategoryWater);
                    break;
                case kExplorerLand:
                    if ((theClass->GetDomains() & kMovementAirSeaOrSpace) == 0) {
                        theValue = this->GetPlayer()->mUnitClassUpgrades[theClass->GetUnitType()]->GetMaxVelocity();
                        if (theValue > theBest) {
                            theBest = theValue;
                            theBestClass = (*theList)[i];
                        }
                    }
                    break;
                }
            }
            i = 0;
        }
        if (theBest > 0.0f) {
            theWanted.clear();
            theWanted.push_back(theBestClass);
            this->mRequests.push_back(this->GetPlayer()->GetCombatManager()->GetPersonnel()->NewContinentRequest(theWanted, theUnits, theContinent));
        }
    }
    for (it = theClasses.begin(); it != theClasses.end(); ++it)
        delete it->second;
}

// FUNCTION: EXE 0x47f14f
// Returns true if the unit is one of the player's explorers
bool EECPExplorationManager::IsExplorer(unsigned long inUnitID)
{
    return std::find(this->mUnitIDs.begin(), this->mUnitIDs.end(), inUnitID) != this->mUnitIDs.end();
}

// FUNCTION: EXE 0x47f1a4
// Reads the explorer ids, the two flags and the open request ids (resolved later by Restore) from a saved game;
// skips an extra value in old (0.32 and earlier) saves; returns the last read's result
long EECPExplorationManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theOld;
    unsigned long theCount;
    unsigned long theID;
    long theResult = FSLoadVector(inFile, this->mUnitIDs, "AI");
    if (theResult == kErrorOK) {
        if (inMajorVersion <= 0 && inMinorVersion <= 0x20)
            theResult = inFile.Read(theOld);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mB2C);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mEnabled);
                if (theResult == kErrorOK) {
                    theCount = 0;
                    theResult = inFile.Read(theCount);
                    this->mRequests.resize(theCount, NULL);
                    for (unsigned long i = 0; i < theCount; i++) {
                        theID = 0;
                        theResult = inFile.Read(theID);
                        this->mRequests[i] = reinterpret_cast<EECPPersonnelRequest*>(theID);
                    }
                }
            }
        }
    }
    return theResult;
}

// NEAR: the last mNew read computes the element address first (lea; mov [ecx+4]); ours folds it.
// FUNCTION: EXE 0x47f26d
// Replaces swapped-out explorers' ids with their new units' ids, for swaps involving this player
void EECPExplorationManager::UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps)
{
    for (long i = inSwaps.size() - 1; i >= 0; i--) {
        if (inSwaps[i].mOld->mPlayer != this->mPlayer && inSwaps[i].mNew->mPlayer != this->mPlayer)
            continue;
        std::vector<unsigned long>::iterator it =
            std::find(this->mUnitIDs.begin(), this->mUnitIDs.end(), inSwaps[i].mOld->GetUnitID());
        if (it == this->mUnitIDs.end())
            continue;
        const std::vector<EEPUnitSwap>& theSwaps = inSwaps;
        const EEPUnitSwap& theSwap = theSwaps[i];
        *it = theSwap.mNew->mUnitID;
    }
}

// NEAR: inline budget: the original calls _Ptrit::operator+ (and begin()) out of line in the first loop.
// FUNCTION: EXE 0x47f352
// Turns the loaded request ids back into referenced requests (dropping missing ones) and puts every living
// explorer back into the exploring AI state; always succeeds
long EECPExplorationManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long i;
    for (i = this->mRequests.size() - 1; i >= 0; i--) {
        EECPPersonnelRequest* theRequest =
            this->GetPlayer()->GetCombatManager()->GetPersonnel()->GetRequest((int32_t)USavedID(this->mRequests[i]));
        if (theRequest != NULL) {
            theRequest->Reference();
            this->mRequests[i] = theRequest;
        } else
            this->mRequests.erase(this->mRequests.begin() + i);
    }
    for (long j = this->mUnitIDs.size() - 1; j >= 0; j--) {
        unsigned long theID = this->mUnitIDs[j];
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(theID)));
        if (theUnit != NULL)
            theUnit->SetAIState(0x46cd1);
    }
    return 0;
}

// FUNCTION: EXE 0x47f452
// Writes the explorer ids, the two flags and the open requests' ids to the saved game; stops at the first
// error before the requests, then returns the last write's result
long EECPExplorationManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = 0;
    long theID;
    long theResult = FSSaveVector(outFile, this->mUnitIDs, "AI");
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mB2C);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mEnabled);
    if (theResult != kErrorOK)
        return theResult;
    theCount = this->mRequests.size();
    theResult = outFile.Write(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        theID = this->mRequests[i]->mID;
        theResult = outFile.Write(theID);
    }
    return theResult;
}

// FUNCTION: EXE 0x47f4f4
// Forgets a removed explorer, unless it is only being swapped for another unit
void EECPExplorationManager::UnitRemoved(EEComplexUnit* inUnit, bool inSwapped)
{
    if (inUnit == NULL)
        return;
    if (inSwapped && gPhysics != NULL && gPhysics->GetNumUnitSwaps() != 0 && gPhysics->GetUnitSwapKind(inUnit) == 1)
        return;
    std::vector<unsigned long>::iterator it = std::find(this->mUnitIDs.begin(), this->mUnitIDs.end(), inUnit->GetUnitID());
    if (it != this->mUnitIDs.end())
        this->mUnitIDs.erase(it);
}

// NEAR: inline budget again (the requests' size()/begin()/operator+ out of line in the original); slots.
// FUNCTION: EXE 0x47f590
// Every 30 s: counts explorers and requests per domain and requests more where short (space uses the sea quota);
// takes filled requests' units, retiring an outdated explorer each, and sends idle explorers out to explore
void EECPExplorationManager::Update(unsigned long inTime)
{
    bool theResearched = false;
    bool theObsolete = false;
    bool theScenarioObsolete = false;
    unsigned long theLandCount = 0;
    unsigned long theSeaCount = 0;
    unsigned long theAirCount = 0;
    unsigned long theSpaceCount = 0;
    unsigned long theLandWanted;
    unsigned long theSeaWanted;
    unsigned long theSpaceWanted;
    unsigned long theAirWanted;
    long i;
    long j;
    EECPPersonnelRequest* theRequest;

    if (!this->GetPlayer()->GetCanExplore() || this->GetNextTime() > inTime || !this->GetEnabled() || this->GetPlayer()->GetSeesAll())
        return;

    // the land explorers
    theLandWanted = this->GetPlayer()->GetNumLandExplorers();
    if (theLandWanted > 0) {
        for (i = this->mUnitIDs.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(this->mUnitIDs[i]));
            if (theUnit != NULL) {
                EEUnitClass* theClass = theUnit->GetUnitClass();
                if ((theClass->GetDomains() & kMovementAirOrSea) == 0
                    && theClass->GetEpoch() >= this->GetPlayer()->GetTechTree()->GetCurrentEpoch() - 1)
                    theLandCount++;
            } else
                this->mUnitIDs.erase(this->mUnitIDs.begin() + i);
        }
        for (j = this->mRequests.size() - 1; j >= 0; j--) {
            theRequest = this->mRequests[j];
            if (theRequest != NULL) {
                if (theRequest->mUnits.size() != 0) {
                    if ((gUnitClassTable[theRequest->mUnits[0].mClass]->mDomains & kMovementSea) == 0
                        && (gUnitClassTable[theRequest->mUnits[0].mClass]->mDomains & kMovementAir) == 0)
                        theLandCount++;
                    continue;
                }
                theRequest->Dereference();
            }
            this->mRequests.erase(this->mRequests.begin() + j);
        }
        if (theLandCount < theLandWanted)
            this->RequestExplorer(kExplorerLand);
    }

    // the sea explorers
    theSeaWanted = this->GetPlayer()->GetNumSeaExplorers();
    this->GetPlayer()->GetTechTree()->GetTechIDState(kTechDock, theResearched, theObsolete, theScenarioObsolete);
    if (theSeaWanted > 0 && theResearched && !theObsolete && gDCGameSpeed > 0) {
        for (i = this->mUnitIDs.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(this->mUnitIDs[i]));
            if (theUnit != NULL) {
                EEUnitClass* theClass = theUnit->GetUnitClass();
                if ((theClass->GetDomains() & kMovementSea) != 0
                    && theClass->GetEpoch() >= this->GetPlayer()->GetTechTree()->GetCurrentEpoch() - 1)
                    theSeaCount++;
            } else
                this->mUnitIDs.erase(this->mUnitIDs.begin() + i);
        }
        for (j = this->mRequests.size() - 1; j >= 0; j--) {
            theRequest = this->mRequests[j];
            if (theRequest != NULL) {
                if (!theRequest->IsEmpty()) {
                    if ((gUnitClassTable[theRequest->mUnits[0].mClass]->mDomains & kMovementSea) != 0)
                        theSeaCount++;
                    continue;
                }
                theRequest->Dereference();
            }
            this->mRequests.erase(this->mRequests.begin() + j);
        }
        if (theSeaCount < theSeaWanted)
            this->RequestExplorer(kExplorerSea);
    }

    // the space explorers
    theSpaceWanted = this->GetPlayer()->GetNumSeaExplorers();
    this->GetPlayer()->GetTechTree()->GetTechIDState(kTechSpaceDock, theResearched, theObsolete, theScenarioObsolete);
    if (theSpaceWanted > 0 && theResearched && !theObsolete && gNumSpaceTiles > 0) {
        for (i = this->mUnitIDs.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(this->mUnitIDs[i]));
            if (theUnit != NULL) {
                EEUnitClass* theClass = theUnit->GetUnitClass();
                if ((theClass->GetDomains() & kMovementSpace) != 0
                    && theClass->GetEpoch() >= this->GetPlayer()->GetTechTree()->GetCurrentEpoch() - 1)
                    theSpaceCount++;
            } else
                this->mUnitIDs.erase(this->mUnitIDs.begin() + i);
        }
        for (j = this->mRequests.size() - 1; j >= 0; j--) {
            theRequest = this->mRequests[j];
            if (theRequest != NULL) {
                if (!theRequest->IsEmpty()) {
                    if ((gUnitClassTable[theRequest->mUnits[0].mClass]->mDomains & kMovementSpace) != 0)
                        theSpaceCount++;
                    continue;
                }
                theRequest->Dereference();
            }
            this->mRequests.erase(this->mRequests.begin() + j);
        }
        if (theSpaceCount < theSpaceWanted)
            this->RequestExplorer(kExplorerSpace);
    }

    // the air explorers
    theAirWanted = this->GetPlayer()->GetAttackStarted() ? this->GetPlayer()->GetNumAirExplorers() : 0;
    this->GetPlayer()->GetTechTree()->GetTechIDState(kTechAirport, theResearched, theObsolete, theScenarioObsolete);
    if (theAirWanted > 0 && theResearched && !theObsolete) {
        for (i = this->mUnitIDs.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(this->mUnitIDs[i]));
            if (theUnit == NULL)
                continue;
            EEUnitClass* theClass = theUnit->GetUnitClass();
            if ((theClass->GetDomains() & kMovementAir) != 0
                && theClass->GetEpoch() >= this->GetPlayer()->GetTechTree()->GetCurrentEpoch() - 1)
                theAirCount++;
        }
        for (j = this->mRequests.size() - 1; j >= 0; j--) {
            theRequest = this->mRequests[j];
            if (theRequest != NULL) {
                if (!theRequest->IsEmpty()) {
                    if ((gUnitClassTable[theRequest->mUnits[0].mClass]->mDomains & kMovementAir) != 0)
                        theAirCount++;
                    continue;
                }
                theRequest->Dereference();
            }
            this->mRequests.erase(this->mRequests.begin() + j);
        }
        if (theAirCount < theAirWanted)
            this->RequestExplorer(kExplorerAir);
    }

    // the filled requests: the new unit becomes an explorer, an outdated one goes back to the personnel
    for (j = this->mRequests.size() - 1; j >= 0; j--) {
        theRequest = this->mRequests[j];
        if (theRequest != NULL) {
            if (!theRequest->IsEmpty()) {
                if (theRequest->mFill == 1.0f) {
                    EEComplexUnit* theNew = theRequest->mUnits.back().mUnit;
                    if (theNew != NULL) {
                        for (unsigned long k = 0; k < this->mUnitIDs.size(); k++) {
                            EEComplexUnit* theOld = static_cast<EEComplexUnit*>(gWorld.GetUnit(this->mUnitIDs[k]));
                            if (theOld == NULL
                                || !(theOld->GetUnitClass()->GetEpoch() < this->GetPlayer()->GetTechTree()->GetCurrentEpoch() - 1))
                                continue;
                            theOld->ClearGoals(0);
                            if (theOld->GetUnitClass()->GetCategory() == kUnitCategoryAirplane || theOld->GetUnitClass()->GetCategory() == kUnitCategoryPlanetSpaceship)
                                theOld->StartAction(kActionCircle);
                            else
                                theOld->StartAction(kActionIdle);
                            theOld->GetUnitClass()->ActionChanged(theOld);
                            theOld->SetBehavior(theOld->GetUnitClass()->GetBehavior());
                            this->GetPlayer()->GetCombatManager()->GetPersonnel()->ReturnUnit(theOld);
                            this->mUnitIDs.erase(this->mUnitIDs.begin() + k);
                            break;
                        }
                    }
                    this->mUnitIDs.push_back(theNew->GetUnitID());
                    // Fix: held across PopSlot, which may release a dead unit's last reference (the original then wrote
                    // its AI state; EECPAttackSubGroup::TakeRequestedUnits)
                    theNew->Reference();
                    theRequest->PopSlot();
                    theNew->SetAIState(0x46cd1);
                    theNew->Dereference();
                    this->mPlayer->mCombatManager->mPersonnel->ReleaseRequest(theRequest->mID);
                    theRequest->Dereference();
                    this->mRequests.erase(this->mRequests.begin() + j);
                }
                continue;
            }
            theRequest->Dereference();
        }
        this->mRequests.erase(this->mRequests.begin() + j);
    }

    // send the idle explorers out
    for (j = this->mUnitIDs.size() - 1; j >= 0; j--) {
        if (this->mUnitIDs[j] != 0 && gWorld.GetUnit(this->mUnitIDs[j]) != NULL) {
            EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(gWorld.GetUnit(this->mUnitIDs[j]));
            if (theUnit->GetHitPoints() > 0 && theUnit->GetGoal() == NULL
                && (theUnit->GetContainer() == NULL || theUnit->GetUnitClass()->GetAirTrafficType() != 0)) {
                if (this->GetPlayer()->GetDifficulty() == kDifficultyEasy)
                    theUnit->SetBehavior(EEUnitBehavior::sTable[0]);
                else if (this->GetPlayer()->GetDifficulty() == kDifficultyHard)
                    theUnit->SetBehavior(EEUnitBehavior::sTable[3]);
                theUnit->ClearGoals(0);
                theUnit->SetGoal(EEUGRust::NewExplore(false), 0, 0);
                theUnit->StartAction(kActionFindUnexploredArea);
                theUnit->GetUnitClass()->ActionChanged(theUnit);
            }
        } else
            this->mUnitIDs.erase(this->mUnitIDs.begin() + j);
    }
    this->mNextTime = inTime + 30000;
}

// The inline and template COMDATs this unit emits (first users here): the screeners' base dtor 0x47f148 (after
// RequestExplorer), vector<EECPPersonnelRequest*>::push_back 0x47ffd0, vector<EECPPersonnelRequest::Unit>::back
// 0x47fffc, FSLoadVector/FSSaveVector<unsigned long> 0x480003/0x4800a1.
// Not emitted by this build: 0x47f148 and 0x47fffc, which it expands inline (see RequestExplorer and Update).
// TEMPLATE: EXE 0x47f148
// TEMPLATE: EXE 0x47ffd0
// TEMPLATE: EXE 0x47fffc
// TEMPLATE: EXE 0x480003
// TEMPLATE: EXE 0x4800a1
