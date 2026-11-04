// C:\EEX\Empire Earth\AI Thread\EECPPersonnel.cpp (file name inferred: no __FILE__; RTTI class EECPPersonnel,
// vtable 0x43c258). A computer player's personnel (the units it may assign and the requests for them): 0x868b8..0x89574
// (the member functions first, the map/set COMDATs close the unit; EECPPlayerInteractionManager.cpp starts at 0x89574).
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
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EETown.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPUnitManager.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include <EEAIFunctionObjects.h>
#include "ee/EECPCombatManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEMathInline.h"

// RTTI EECPUGatherUnitsOfTypeAndTownAndContinent > EECPUMGatherUnitsScreenBase (vtable 0x43c244: CountRequest 0x85a97,
// the two empty `xor al, al` slots ICF-folded elsewhere): the town and continent of the last pending request of
// mUnitType (EECPPersonnelRequest::Cancel builds it on the stack) (provisional member names)
class EECPUGatherUnitsOfTypeAndTownAndContinent : public EECPUMGatherUnitsScreenBase {
public:
    EECPUGatherUnitsOfTypeAndTownAndContinent() : EECPUMGatherUnitsScreenBase(false, true, false)
    {
        this->mContinent = 0;
        this->mTownID = 0;
        this->mUnitType = 0;
    }
// TEMPLATE: EXE 0x485a97
// Counts a pending unit request of the wanted unit type and keeps its town and continent (the last match wins)
    // (emitted with the vtable just before this unit's first function 0x85ace)
    virtual bool CountRequest(EEUnitClass* inClass, EECPUMRequest* inRequest)   // +0x04 0x85a97
    {
        bool theResult = false;
        if (inClass != NULL) {
            theResult = this->mUnitType == (long)inClass->mUnitType;
            if (theResult) {
                this->mRequested += inRequest->mCount;
                this->mTownID = inRequest->mTownID;
                this->mContinent = inRequest->mContinent;
            }
        }
        return theResult;
    }
    virtual bool CountUnit(EEComplexUnit* inUnit) { return false; }   // +0x08
    virtual bool CountUnits(EEUnitClass* inClass, std::deque<EEComplexUnit*>* inUnits) { return false; }   // +0x0c
    void Clear();                           // 0x8600d

    unsigned short mContinent;              // +0x10
    long mTownID;                           // +0x14
    long mUnitType;                         // +0x18
};

extern long gNumPlayers;                    // 0x530e0c, the world's player count (as EECPAttackCommander.cpp declares it; provisional name)

static bool EECPMayDraft(EEUnitClass* inClass);   // 0x86fc1 (defined after its first user DraftUnit; provisional name)

// The personnel requests (EECPPersonnelRequest): 0x85ace..0x868b8

// FUNCTION: EXE 0x485ace
// Creates an empty personnel request with the given id for the personnel manager, with no deadline
EECPPersonnelRequest::EECPPersonnelRequest(EECPPersonnel* inPersonnel, long inID)
{
    this->mFill = 0.0f;
    this->mDeadline = 0xffffffff;
    this->mPersonnel = inPersonnel;
    this->mNumFilled = 0;
    this->mContinent = 0;
    this->mTownID = 0;
    this->mStartTime = 0;
    this->mID = inID;
}

// SYNTHETIC: EXE 0x485b29
// (the scalar deleting destructor)

// FUNCTION: EXE 0x485b45
// Destroys the request, releasing its references to the assigned units
EECPPersonnelRequest::~EECPPersonnelRequest()
{
    this->Clear();
}

// FUNCTION: EXE 0x485b64
// Assigns a unit to the slot waiting for its id, else to a free slot of its type; returns whether a slot took it
bool EECPPersonnelRequest::TryAssign(EEComplexUnit* inUnit)
{
    long theType = inUnit->mUnitClass->mUnitType;
    bool theResult = false;
    long i;
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i].mUnitID == 0xffffffff || this->mUnits[i].mUnit != NULL || this->mUnits[i].mUnitID != inUnit->mUnitID)
            continue;
        inUnit->Reference();
        this->mUnits[i].mUnit = inUnit;
        this->mUnits[i].mB0C = true;
        this->mNumFilled++;
        theResult = true;
        this->mFill = (float)this->mNumFilled / (float)this->mUnits.size();
        inUnit->SetAIState(0x46cd3);
        break;
    }
    if (!theResult) {
        for (i = this->mUnits.size() - 1; i >= 0; i--) {
            if (this->mUnits[i].mClass != theType || this->mUnits[i].mUnit != NULL)
                continue;
            inUnit->Reference();
            this->mUnits[i].mUnit = inUnit;
            this->mUnits[i].mB0C = true;
            this->mNumFilled++;
            theResult = true;
            this->mFill = (float)this->mNumFilled / (float)this->mUnits.size();
            inUnit->SetAIState(0x46cd3);
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x485d51
// Empties the request and hands its living units back to the personnel manager as new units
void EECPPersonnelRequest::ReleaseAll()
{
    std::vector<EEComplexUnit*> theUnits;
    long i;
    for (i = this->mUnits.size() - 1; i >= 0; i--) {
        if (this->mUnits[i].mUnit == NULL)
            continue;
        if (this->mUnits[i].mUnit->mHitPoints > 0) {
            this->mUnits[i].mUnit->Reference();
            theUnits.push_back(this->mUnits[i].mUnit);
        }
        this->mUnits[i].mUnit->Dereference();
    }
    this->mUnits.clear();
    for (i = theUnits.size() - 1; i >= 0; i--) {
        this->mPersonnel->AddNewUnit(theUnits[i]);
        (theUnits[i])->Dereference();
    }
}

// FUNCTION: EXE 0x485e94
// Drops the request's unfilled type slots and cancels one production order for each (at the request's town and
// continent, else where one of that type is pending); then counts the request as full
void EECPPersonnelRequest::Cancel()
{
    EECPUGatherUnitsOfTypeAndTownAndContinent theGatherer;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        if ((this->mUnits[i]).mUnit != NULL || (*(this->mUnits.begin() + i)).mClass == 0)
            continue;
        EECPUnitManager* theManager = this->mPersonnel->mPlayer->mCivilizationManager->mUnitManager;
        if (!theManager->CancelRequest((*(this->mUnits.begin() + i)).mClass, 1, this->mTownID, this->mContinent)) {
            theGatherer.Clear();
            theGatherer.mUnitType = this->mUnits[i].mClass;
            this->mPersonnel->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
            this->mPersonnel->mPlayer->mCivilizationManager->mUnitManager->CancelRequest(this->mUnits[i].mClass, 1,
                                                                             theGatherer.mTownID,
                                                                             theGatherer.mContinent);
        }
        this->mUnits.erase(this->mUnits.begin() + i);
    }
    this->mFill = 1.0f;
}

// FUNCTION: EXE 0x48600d
// Resets the gatherer's counts, town, continent and unit type
void EECPUGatherUnitsOfTypeAndTownAndContinent::Clear()
{
    this->mUnits = 0;
    this->mRequested = 0;
    this->mContinent = 0;
    this->mTownID = 0;
    this->mUnitType = 0;
}

// FUNCTION: EXE 0x486020
// Drops up to inCount unfilled slots of a unit type; returns how many were dropped
long EECPPersonnelRequest::ReleaseOfType(long inClass, long inCount)
{
    long theRemoved = 0;
    long theCount = inCount;
    for (long i = this->mUnits.size() - 1; i >= 0 && theCount > 0; i--) {
        if (this->mUnits[i].mClass != inClass || this->mUnits[i].mUnit != NULL)
            continue;
        this->mUnits.erase(this->mUnits.begin() + i);
        theRemoved++;
        theCount--;
    }
    this->UpdateFill();
    return theRemoved;
}

// FUNCTION: EXE 0x4860c0
// Recomputes and returns the request's filled fraction (0 when it has no slots)
float EECPPersonnelRequest::UpdateFill()
{
    this->mFill = this->mUnits.size() > 0 ? (float)this->mNumFilled / (float)this->mUnits.size() : 0.0f;
    return this->mFill;
}

// FUNCTION: EXE 0x4860fa
// Releases every assigned unit and empties the request's slots
void EECPPersonnelRequest::Clear()
{
    for (long i = this->mUnits.size() - 1; i >= 0; i--)
        if (this->mUnits[i].mUnit != NULL)
            this->mUnits[i].mUnit->Dereference();
    this->mUnits.clear();
}

// FUNCTION: EXE 0x486144
// Returns the population cost of the request's slots (the assigned unit's class, else the requested class)
long EECPPersonnelRequest::GetPopulation()
{
    long theTotal = 0;
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass;
        if (this->mUnits[i].mUnit != NULL)
            theClass = this->mUnits[i].mUnit->mUnitClass;
        else
            theClass = gUnitClassTable[this->mUnits[i].mClass];
        if (theClass != NULL)
            theTotal += theClass->GetPopulationCost();
    }
    return theTotal;
}

// FUNCTION: EXE 0x48619f
// Returns the request's town id, first set to the player's town nearest the request's tile when it has one
long EECPPersonnelRequest::GetTownID()
{
    U2DPoint<long> theTile;
    if (this->mTile.mX == 0 && this->mTile.mY == 0)
        return this->mTownID;
    EEPlayer* thePlayer = this->mPersonnel->mPlayer;
    EETown* theTown = thePlayer->GetNearestTown(this->mTile, theTile);
    if (theTown != NULL)
        this->mTownID = theTown->mTownID;
    return this->mTownID;
}

// FUNCTION: EXE 0x4861de
// Resets the request to one slot per requested unit type and one per named unit id, with a new id and start time
void EECPPersonnelRequest::Setup(long inID, std::vector<long>& inClasses, std::vector<long>& inUnitIDs)
{
    Unit theUnit;
    theUnit.mB0C = false;
    theUnit.mUnit = NULL;
    theUnit.mClass = 0;
    theUnit.mUnitID = 0;
    this->Clear();
    this->mStartTime = gPhysics->mGameTime;
    this->mID = inID;
    long i;
    for (i = inClasses.size() - 1; i >= 0; i--) {
        theUnit.mClass = inClasses[i];
        theUnit.mB0C = false;
        theUnit.mUnitID = 0xffffffff;
        theUnit.mUnit = NULL;
        this->mUnits.push_back(theUnit);
    }
    for (i = inUnitIDs.size() - 1; i >= 0; i--) {
        theUnit.mClass = 0;
        theUnit.mB0C = false;
        theUnit.mUnitID = inUnitIDs[i];
        theUnit.mUnit = NULL;
        this->mUnits.push_back(theUnit);
    }
    if (this->mUnits.size() > 0)
        this->mFill = (float)this->mNumFilled / (float)this->mUnits.size();
}

// FUNCTION: EXE 0x4862c9
// Reads the request's slots (unit ids, resolved by Restore) and settings from a saved game; saves before version
// 0.222 have no deadline; returns the first read error
long EECPPersonnelRequest::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long i;
    unsigned long theCount = 0;
    unsigned long theID = 0;
    long theResult;
    if (inMajorVersion == 0 && inMinorVersion < 0xde) {
        if ((theResult = inFile.Read(theCount)) != 0)
            return theResult;
        this->mUnits.resize(theCount);
        for (i = 0; i < theCount; i++) {
            if (theResult == 0 && (theResult = inFile.Read(theID)) == 0) {
                this->mUnits[i].mUnit = reinterpret_cast<EEComplexUnit*>(theID);
                if ((theResult = inFile.Read(this->mUnits[i].mClass)) == 0)
                    theResult = inFile.Read(this->mUnits[i].mUnitID);
            }
            if (this->mUnits[i].mUnitID == 0)
                this->mUnits[i].mUnitID = 0xffffffff;
            if (theResult == 0)
                theResult = inFile.Read(this->mUnits[i].mB0C);
        }
        if (theResult != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mFill)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mID)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mContinent)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mTownID)) != 0)
            return theResult;
        if ((theResult = this->mTile.Load(inFile)) != 0)
            return theResult;
        theResult = inFile.Read(this->mStartTime);
    } else {
        if ((theResult = inFile.Read(theCount)) != 0)
            return theResult;
        this->mUnits.resize(theCount);
        for (i = 0; i < theCount; i++) {
            if (theResult == 0 && (theResult = inFile.Read(theID)) == 0) {
                this->mUnits[i].mUnit = reinterpret_cast<EEComplexUnit*>(theID);
                if ((theResult = inFile.Read(this->mUnits[i].mClass)) == 0)
                    theResult = inFile.Read(this->mUnits[i].mUnitID);
            }
            if (this->mUnits[i].mUnitID == 0)
                this->mUnits[i].mUnitID = 0xffffffff;
            if (theResult == 0)
                theResult = inFile.Read(this->mUnits[i].mB0C);
        }
        if (theResult != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mFill)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mID)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mContinent)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mTownID)) != 0)
            return theResult;
        if ((theResult = this->mTile.Load(inFile)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mStartTime)) != 0)
            return theResult;
        theResult = inFile.Read(this->mDeadline);
    }
    return theResult;
}

// FUNCTION: EXE 0x48655c
// Removes the request's last slot, releasing its unit (a draftable unit also leaves its area groups)
void EECPPersonnelRequest::PopSlot()
{
    EEComplexUnit* theUnit = this->mUnits.back().mUnit;
    if (theUnit != NULL) {
        if (EECPMayDraft(theUnit->mUnitClass))
            this->mPersonnel->mPlayer->mCombatManager->ReleaseFromAreaGroups(this->mUnits.back().mUnit);
        this->mUnits.back().mUnit->Dereference();
    }
    this->mUnits.pop_back();
}

// FUNCTION: EXE 0x4865a2
// Removes a unit's slot from the request and updates the fill (1 when no slots remain); returns whether it was there
bool EECPPersonnelRequest::Remove(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (inUnit == NULL)
        return theResult;
    for (std::vector<Unit>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
        if (it->mUnit != inUnit)
            continue;
        it->mUnit->Dereference();
        it->mUnit = NULL;
        this->mNumFilled--;
        this->mUnits.erase(it);
        this->mFill = this->mUnits.size() > 0 ? (float)this->mNumFilled / this->mUnits.size() : 1.0f;
        theResult = true;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x486624
// After loading, turns saved unit ids into units, keeping only military units and military type slots; recounts the
// fill and town and gives saves before version 0.222 a deadline 600000 ms after the start; returns 0
long EECPPersonnelRequest::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->mNumFilled = 0;
    std::vector<Unit>::iterator it = this->mUnits.begin();
    while (it != this->mUnits.end()) {
        if ((unsigned long)it->mUnit != 0xffffffff) {
            EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((unsigned long)it->mUnit));
            if (theUnit != NULL && EECPCombatManager::IsMilitaryTypeUnit(theUnit)) {
                it->mUnit = theUnit;
                theUnit->Reference();
                theUnit->SetAIState(0x46cd2);
                it->mB0C = true;
                this->mNumFilled++;
                it++;
            } else {
                it->mUnit = NULL;
                it = this->mUnits.erase(it);
            }
        } else {
            bool theKeep = EECPCombatManager::IsMilitaryType(it->mClass);
            it->mUnit = NULL;
            if (theKeep)
                it++;
            else
                it = this->mUnits.erase(it);
        }
    }
    if (this->mUnits.size() > 0)
        this->mFill = (float)this->mNumFilled / (float)this->mUnits.size();
    else
        this->mFill = 1.0f;
    this->GetTownID();
    if (inMajorVersion == 0 && inMinorVersion < 0xde)
        this->mDeadline = this->mStartTime + 600000;
    return 0;
}

// FUNCTION: EXE 0x486742
// Writes the request's slots (unit ids) and settings to the saved game; returns the first write error
long EECPPersonnelRequest::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mUnits.size();
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < theCount; i++) {
        unsigned long theID = this->mUnits[i].mUnit != NULL ? this->mUnits[i].mUnit->GetUnitID() : 0xffffffff;
        if (theResult == 0 && (theResult = outFile.Write(theID)) == 0 &&
            (theResult = outFile.Write(this->mUnits[i].mClass)) == 0 &&
            (theResult = outFile.Write(this->mUnits[i].mUnitID)) == 0)
            theResult = outFile.Write(this->mUnits[i].mB0C);
    }
    if (theResult != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mFill)) == 0 && (theResult = outFile.Write(this->mID)) == 0 &&
        (theResult = outFile.Write(this->mContinent)) == 0 && (theResult = outFile.Write(this->mTownID)) == 0 &&
        (theResult = this->mTile.Save(outFile)) == 0 && (theResult = outFile.Write(this->mStartTime)) == 0)
        theResult = outFile.Write(this->mDeadline);
    return theResult;
}

// FUNCTION: EXE 0x486878
// Puts a new unit into the slot of an old one (the slot takes the new unit's type); returns whether it was found
bool EECPPersonnelRequest::Replace(EEComplexUnit* inOld, EEComplexUnit* inNew)
{
    bool theResult = false;
    for (std::vector<Unit>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
        if (it->mUnit != inOld)
            continue;
        theResult = true;
        it->mUnit->Dereference();
        it->mUnit = inNew;
        inNew->Reference();
        it->mClass = inNew->mUnitClass->mUnitType;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x4868b8
// Creates the computer player's personnel manager, with no units or requests
EECPPersonnel::EECPPersonnel(EEComputerPlayer* inPlayer)
    : mPlayer(inPlayer), mNextID(0), mNextUpdateTime(0), mDrafting(false)
{
}

// SYNTHETIC: EXE 0x486915
// (the scalar deleting destructor)

// FUNCTION: EXE 0x486931
// Destroys the personnel manager, releasing every free unit and request it holds
EECPPersonnel::~EECPPersonnel()
{
    this->ClearAll();
}

// FUNCTION: EXE 0x486959
// Hands a draftable unit to the attack commander and returns whether it was taken; when that is not possible now
// (player inactive, no game screen or continents, kept by combat, observer, drafting already) keeps its id for later
bool EECPPersonnel::DraftUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mPlayer->mActive && gGameScreenActive && gContinentManager.GetNumContinents() != 0 &&
        EECPMayDraft(inUnit->mUnitClass) && !this->mPlayer->mCombatManager->KeepsUnit(inUnit) && !this->mPlayer->mObserverMode && !this->mDrafting) {
        this->mDrafting = true;
        if (!gContinentManagerClearing) {
            inUnit->SetAIState(0x46cd5);
            theResult = this->mPlayer->mCombatManager->mAttackCommander->TakeUnit(inUnit);
        } else {
            inUnit->SetAIState(290000);
            this->mDeferredUnits.insert(inUnit->GetUnitID());
        }
        this->mDrafting = false;
    } else {
        inUnit->SetAIState(0x46cd2);
        this->mDeferredUnits.insert(inUnit->GetUnitID());
    }
    return theResult;
}

// FUNCTION: EXE 0x486a54
// On the game screen, retries drafting each deferred unit still owned by the player, under AI control and in no
// attack group; then forgets them all
void EECPPersonnel::DraftDeferred()
{
    if (!gGameScreenActive)
        return;
    std::set<unsigned long>::iterator it;
    for (it = this->mDeferredUnits.begin(); it != this->mDeferredUnits.end(); ++it) {
        unsigned long theID = *it;
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
        if (theUnit != NULL && theUnit->mPlayer == this->mPlayer && !theUnit->mIsDetachedFromAI &&
            this->mPlayer->mCombatManager->mAttackCommander->FindHolder(theUnit) == NULL)
            this->DraftUnit(theUnit);
    }
    this->mDeferredUnits.clear();
}

// FUNCTION: EXE 0x486aee
// Submits a new request: fills it from the free units at once
void EECPPersonnel::SubmitRequest(EECPPersonnelRequest* inRequest)
{
    this->FillRequest(inRequest);
}

// FUNCTION: EXE 0x486afa
// Drops up to inCount unfilled slots of a unit type across the requests, in request id order
void EECPPersonnel::ReleaseOfType(long inUnitClass, long inCount)
{
    std::map<long, EECPPersonnelRequest*>::iterator it;
    long theCount = inCount;
    for (it = this->mRequests.begin(); it != this->mRequests.end() && theCount > 0;
         ++it)
        theCount -= it->second->ReleaseOfType(inUnitClass, theCount);
}

// FUNCTION: EXE 0x486b40
// Fills a request's empty slots: named units from the free pool or other requests (dropping slots of vanished units),
// type slots from free units of that type, else by ordering one built at the request's town and continent
void EECPPersonnel::FillRequest(EECPPersonnelRequest* inRequest)
{
    std::multimap<long, EEUnit*>::iterator it;
    std::vector<long> theIDs;
    long theTownID = inRequest->GetTownID();
    for (long i = inRequest->mUnits.size() - 1; i >= 0; i--) {
        bool theIsClassSlot = (inRequest->mUnits[i]).mClass != 0 &&
                              (inRequest->mUnits[i]).mUnitID == -1;
        bool theDrop = false;
        bool theFound = false;
        bool theUseClass = true;
        if (!theIsClassSlot && (inRequest->mUnits[i]).mUnitID != -1) {
            for (it = this->mUnits.begin(); it != this->mUnits.end();) {
                if (it->second->mUnitID == (inRequest->mUnits[i]).mUnitID &&
                    (theFound = inRequest->TryAssign((EEComplexUnit*)it->second)) != false) {
                    it->second->Dereference();
                    it = this->mUnits.erase(it);
                    theUseClass = false;
                } else
                    ++it;
            }
            if (!theFound) {
                theUseClass = false;
                EEComplexUnit* theUnit =
                    dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((inRequest->mUnits[i]).mUnitID));
                if (theUnit != NULL) {
                    this->RemoveFromRequests(theUnit);
                    if (inRequest->TryAssign(theUnit))
                        this->DraftUnit(theUnit);
                    continue;
                }
                theDrop = true;
            }
        }
        if (theUseClass && (inRequest->mUnits[i]).mUnit == NULL) {
            it = this->mUnits.find((inRequest->mUnits[i]).mClass);
            if (it != this->mUnits.end()) {
                if (inRequest->TryAssign((EEComplexUnit*)it->second)) {
                    it->second->Dereference();
                    this->mUnits.erase(it);
                }
                continue;
            }
            if ((inRequest->mUnits[i]).mB0C)
                continue;
            this->RequestProduction((inRequest->mUnits[i]).mClass, theTownID, inRequest->mContinent);
            (inRequest->mUnits[i]).mB0C = true;
        }
        if (theDrop)
            inRequest->mUnits.erase(inRequest->mUnits.begin() + i);
    }
    inRequest->UpdateFill();
}

// FUNCTION: EXE 0x486e3c
// Ends the request with the given id, returning its living units to the free pool
void EECPPersonnel::ReleaseRequest(long inID)
{
    std::map<long, EECPPersonnelRequest*>::iterator theEntry = this->mRequests.find(inID);
    if (theEntry == this->mRequests.end())
        return;
    EECPPersonnelRequest* theRequest = theEntry->second;
    this->mRequests.erase(theEntry);
    theRequest->ReleaseAll();
    theRequest->Dereference();
}


// FUNCTION: EXE 0x486e81
// Periodic upkeep: culls units for the military, cancels the unfilled part of overdue requests (ending empty ones),
// unloads free units carrying cargo (dropping those then detached from the AI); next update in 30 s
void EECPPersonnel::Update(unsigned long inTime)
{
    std::map<long, EECPPersonnelRequest*>::iterator it;
    std::multimap<long, EEUnit*>::iterator theEntry;
    std::vector<EEComplexUnit*> theUnits;
    this->KillUnitsForMilitary();
    for (it = this->mRequests.begin(); it != this->mRequests.end();) {
        EECPPersonnelRequest* theRequest = it->second;
        if (inTime < theRequest->mDeadline)
            ++it;
        else {
            theRequest->Cancel();
            if (theRequest->GetNumSlots() != 0)
                ++it;
            else {
                it = this->mRequests.erase(it);
                theRequest->Dereference();
            }
        }
    }
    for (theEntry = this->mUnits.begin(); theEntry != this->mUnits.end();) {
        if (theEntry->second == NULL)
            theEntry = this->mUnits.erase(theEntry);
        else {
            if (((EEComplexUnit*)theEntry->second)->GetNumCargoUnits() != 0)
                theUnits.push_back((EEComplexUnit*)theEntry->second);
            ++theEntry;
        }
    }
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        this->UnloadTransport(theUnits[i]);
        if (theUnits[i]->mIsDetachedFromAI)
            this->RemoveUnit(theUnits[i]);
    }
    this->mNextUpdateTime = inTime + 30000;
}

// FUNCTION: EXE 0x486fae
// Returns the number of slots in a request (out-of-line copy of the inline GetNumSlots in ee/EECPPersonnel.h)

// FUNCTION: EXE 0x486fc1
// Returns whether units of a class may be drafted for attack: no transports, naval or space buildings,
// or anti-missile batteries
// (static: emitted after its first user DraftUnit)
static bool EECPMayDraft(EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass != NULL) {
        if (inClass->IsTransport() || inClass->mCategory == kUnitCategorySpaceBuilding || inClass->mCategory == kUnitCategoryNavalBuilding || inClass->mUnitType == kUnitType14AntiMissileBattery)
            theResult = false;
        else
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x486ffb
// Creates a referenced empty request with the next request id (not yet registered)
EECPPersonnelRequest* EECPPersonnel::NewRequest()
{
    EECPPersonnelRequest* theRequest = new EECPPersonnelRequest(this, this->mNextID++);
    theRequest->Reference();
    return theRequest;
}

// FUNCTION: EXE 0x487025
// Offers a unit to each request that is not yet full, in id order; returns whether one took it
bool EECPPersonnel::OfferUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (this->mRequests.size() != 0) {
        for (std::map<long, EECPPersonnelRequest*>::iterator it = this->mRequests.begin(); it != this->mRequests.end(); ++it) {
            if (it->second->mFill == 1.0f)
                continue;
            theResult = it->second->TryAssign(inUnit);
            if (theResult)
                break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x48707b
// From the Atomic - WW1 epoch on, kills the player's living obsolete units of 17 unit types that some human
// player cannot see
void EECPPersonnel::RetireObsoleteUnits()
{
    std::vector<EEPlayer*> thePlayers;
    std::vector<EEComplexUnit*> theUnits;
    long theEpoch = this->mPlayer->mTechnologies->mCurrentEpoch;
    long i;
    EEComplexUnit* theComplexUnit;
    for (i = gNumPlayers - 1; i > 0; i--) {
        if (gWorld.mPlayers[i] != NULL && gWorld.mPlayers[i]->IsHuman())
            thePlayers.push_back(gWorld.GetPlayer(i));
    }
    if (thePlayers.size() > 0) {
        for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            theComplexUnit = theUnit->mComplexUnit;
            if (theComplexUnit == NULL || theComplexUnit->GetTechTreeNode() == NULL ||
                !theComplexUnit->GetTechTreeNode()->GetIsObsolete() || theEpoch < kTechEpochAtomicWW1)
                continue;
            switch (theComplexUnit->mUnitClass->mUnitType) {
            case kUnitTypeArch06CavalryArcher:
            case kUnitTypeGunSpear04StoneThrower:
            case kUnitTypeArch05CrossBow:
            case kUnitTypeArch06LongBow:
            case kUnitTypeInf04Phalanx:
            case kUnitTypeArcheryRange:
            case kUnitTypeStable:
            case kUnitTypeInf05Pilum:
            case kUnitTypeGunSiege06Trebuchet:
            case kUnitTypeGunSiege09SerpentineCannon:
            case kUnitTypeCav06Knight:
            case kUnitTypeGunCannon09BronzeCannon:
            case kUnitTypeGunSpear06Balistae:
            case kUnitTypeS09RoyalGalleon:
            case kUnitTypeCav07CarabineersGun:
            case kUnitTypeCav09ImperialCuirassierSword:
            case kUnitTypeInf08Halberdier: {
                bool theFound = false;
                for (i = thePlayers.size() - 1; i >= 0; i--) {
                    if (theFound)
                        break;
                    theFound = theComplexUnit->mHitPoints > 0 && !EEIsSeenBy(theComplexUnit, thePlayers[i], true);
                }
                if (theFound)
                    theUnits.push_back(theComplexUnit);
                break;
            }
            }
        }
    }
    for (i = theUnits.size() - 1; i >= 0; i--)
        theUnits[i]->Kill(true, false, false);
}

// FUNCTION: EXE 0x487275
// Reads the free units (as saved ids, resolved by Restore), the requests, the next request id and the deferred
// unit ids from a saved game; returns the first read error
long EECPPersonnel::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theUnitID = 0;
    unsigned long theCount = 0;
    unsigned long theValue = 0;
    long theRequestID = 0;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (i = 0; i < theCount; i++) {
        if (theResult == kErrorOK && (theResult = inFile.Read(theUnitID)) == 0)
            theResult = inFile.Read(theValue);
        this->mUnits.insert(std::multimap<long, EEUnit*>::value_type(theUnitID, reinterpret_cast<EEUnit*>(theValue)));
    }
    if (theResult != kErrorOK)
        return theResult;
    theCount = 0;
    theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (i = 0; i < theCount; i++) {
        EECPPersonnelRequest* theRequest = NULL;
        if (theResult != kErrorOK || (theResult = inFile.Read(theRequestID)) != 0)
            continue;
        theRequest = new EECPPersonnelRequest(this, theRequestID);
        theResult = theRequest->Load(inMajorVersion, inMinorVersion, inFile);
        this->mRequests.insert(std::map<long, EECPPersonnelRequest*>::value_type(theRequestID, theRequest));
    }
    if (theResult != kErrorOK)
        return theResult;
    if ((theResult = inFile.Read(this->mNextID)) == 0 && (theResult = inFile.Read(theCount)) == 0) {
        for (i = 0; i < theCount; i++) {
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theValue);
                this->mDeferredUnits.insert(theValue);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4873f5
// Has the player kill units to free population for the military: the citizen surplus, plus 5 more citizens (or
// the units over the cap) when the population is at its limit and transports are short
void EECPPersonnel::KillUnitsForMilitary()
{
    EEAIFOUnitsToKillForMilitary theScorer(this->mPlayer, 0, 0, 0);
    float theRatio = 0.0f;
    long theCount = 0;
    theScorer.mCitizensToKill = EEAIFOUnitsToKillScoringBase::GetCitizenSurplus(this->mPlayer, theRatio, theCount);
    if (this->mPlayer->GetTotalPopulation(false, false) + 1 >= this->mPlayer->GetPopulationLimit(0)) {
        EECPUGatherTransportUnitsCount theTransports(this->mPlayer, true, true);
        this->mPlayer->mCivilizationManager->mUnitManager->GatherUnits(theTransports);
        if (theTransports.mUnits <= 5 && theTransports.mRequested > theScorer.mCitizensToKill) {
            if (theCount > 6)
                theScorer.mCitizensToKill = max(0L, theScorer.mCitizensToKill) + 5;
            else
                theScorer.mMilitaryToKill = theScorer.mMilitaryToKill - this->mPlayer->GetPopulationLimit(0) + this->mPlayer->GetTotalPopulation(false, false) + 5;
        }
    }
    theScorer.mCount = max(0L, theScorer.mCitizensToKill) + theScorer.mMilitaryToKill;
    if (theScorer.mCount > 0)
        this->mPlayer->ChooseUnitsToKill(theScorer, false);
}

// FUNCTION: EXE 0x4874db
// Creates, fills and registers a request for unit types and named units on a continent, due in 600000 ms; the
// named units leave attack duty first
EECPPersonnelRequest* EECPPersonnel::NewContinentRequest(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent)
{
    EECPPersonnelRequest* theRequest = this->NewRequest();
    EECPCombatManager* theManager = this->mPlayer->mCombatManager;
    for (long i = inUnitIDs.size() - 1; i >= 0; i--) {
        long theID = inUnitIDs[i];
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
        if (theUnit != NULL)
            theManager->TakeFromAttack(theUnit, false);
    }
    theRequest->mDeadline = gPhysics->mGameTime + 600000;
    theRequest->mContinent = inContinent;
    theRequest->Setup(theRequest->mID, inClasses, inUnitIDs);
    this->SubmitRequest(theRequest);
    this->mRequests.insert(std::make_pair(theRequest->mID, theRequest));
    return theRequest;
}

// FUNCTION: EXE 0x4875ab
// Creates, fills and registers a request for unit types and named units at a town (and its centre tile), due in
// 600000 ms; the named units leave attack duty first
EECPPersonnelRequest* EECPPersonnel::NewTownRequest(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inTownID,
                                             const U2DPoint<long>& inCenter)
{
    EECPPersonnelRequest* theRequest = this->NewRequest();
    EECPCombatManager* theManager = this->mPlayer->mCombatManager;
    for (long i = inUnitIDs.size() - 1; i >= 0; i--) {
        long theID = inUnitIDs[i];
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(theID));
        if (theUnit != NULL)
            theManager->TakeFromAttack(theUnit, false);
    }
    theRequest->mDeadline = gPhysics->mGameTime + 600000;
    theRequest->mTownID = inTownID;
    theRequest->mTile = inCenter;
    theRequest->Setup(theRequest->mID, inClasses, inUnitIDs);
    this->SubmitRequest(theRequest);
    this->mRequests.insert(std::map<long, EECPPersonnelRequest*>::value_type(theRequest->mID, theRequest));
    return theRequest;
}

// FUNCTION: EXE 0x487687
// Follows units replaced by others: rekeys free military units and request slots to the new unit, and takes in
// military units newly gained from another player unless combat claims them
void EECPPersonnel::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    std::multimap<long, EEUnit*>::iterator it;
    std::multimap<long, EEUnit*>::iterator theEnd;
    long theType = 0;
    long i;
    if (this->mUnits.size() > 0) {
        for (i = ioSwaps.size() - 1; i >= 0; i--) {
            if (!this->mPlayer->mCombatManager->IsMilitaryClass((ioSwaps[i]).mOld->GetUnitClass()))
                continue;
            if ((ioSwaps[i]).mOld->mPlayer != this->mPlayer && (ioSwaps[i]).mNew->mPlayer != this->mPlayer)
                continue;
            theType = (ioSwaps[i]).mNew->GetUnitClass()->mUnitType;
            std::pair<std::multimap<long, EEUnit*>::iterator, std::multimap<long, EEUnit*>::iterator> theRange =
                this->mUnits.equal_range(theType);
            for (it = theRange.first, theEnd = theRange.second; it != theEnd; ++it) {
                if (it->second != (ioSwaps[i]).mNew)
                    continue;
                it->second->Dereference();   // the erased entry's reference (no new one is taken)
                this->mUnits.erase(it);
                break;
            }
            theType = (ioSwaps[i]).mOld->GetUnitClass()->mUnitType;
            theRange = this->mUnits.equal_range(theType);
            for (it = theRange.first, theEnd = theRange.second; it != theEnd; ++it) {
                if (it->second != (ioSwaps[i]).mOld)
                    continue;
                it->second->Dereference();
                (ioSwaps[i]).mNew->Reference();
                this->mUnits.erase(it);
                long theNewType = (ioSwaps[i]).mNew->GetUnitClass()->mUnitType;
                this->mUnits.insert(std::make_pair(theNewType, (ioSwaps[i]).mNew));
                break;
            }
        }
    }
    std::map<long, EECPPersonnelRequest*>::iterator theRequest;
    for (theRequest = this->mRequests.begin(); theRequest != this->mRequests.end(); ++theRequest) {
        for (i = ioSwaps.size() - 1; i >= 0; i--) {
            if (!this->mPlayer->mCombatManager->IsMilitaryClass((ioSwaps[i]).mOld->GetUnitClass()))
                continue;
            if ((ioSwaps[i]).mOld->mPlayer != this->mPlayer && (ioSwaps[i]).mNew->mPlayer != this->mPlayer)
                continue;
            if (theRequest->second->Remove((ioSwaps[i]).mNew) &&
                EECPMayDraft((ioSwaps[i]).mNew->GetUnitClass())) {
                this->mPlayer->mCombatManager->ReleaseFromAreaGroups((ioSwaps[i]).mNew);
                (ioSwaps[i]).mNew->SetAIState(0x46cd6);
            }
            EEPUnitSwap& theSwap = ioSwaps[i];
            theRequest->second->Replace((ioSwaps[i]).mOld, theSwap.mNew);
        }
    }
    for (i = ioSwaps.size() - 1; i >= 0; i--) {
        long theCategory = (ioSwaps[i]).mOld->GetUnitClass()->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
            continue;
        if ((ioSwaps[i]).mNew->mPlayer != this->mPlayer)
            continue;
        EEPlayer* theOldPlayer = (ioSwaps[i]).mOld->mPlayer;
        if (theOldPlayer == (ioSwaps[i]).mNew->mPlayer)
            continue;
        if (!this->mPlayer->mCombatManager->IsMilitaryClass((ioSwaps[i]).mNew->GetUnitClass()))
            continue;
        EEPUnitSwap& theSwap = ioSwaps[i];
        if (this->mPlayer->mCombatManager->OfferUnit((ioSwaps[i]).mOld, theSwap.mNew))
            continue;
        this->AddNewUnit((ioSwaps[i]).mNew);
    }
}

// FUNCTION: EXE 0x487c5b
// Releases every free unit and request and empties both
void EECPPersonnel::ClearAll()
{
    std::multimap<long, EEUnit*>::iterator it;
    std::map<long, EECPPersonnelRequest*>::iterator theRequest;
    for (it = this->mUnits.begin(); it != this->mUnits.end(); ++it)
        it->second->Dereference();
    this->mUnits.clear();
    for (theRequest = this->mRequests.begin(); theRequest != this->mRequests.end(); ++theRequest)
        theRequest->second->Dereference();
    this->mRequests.clear();
}

// FUNCTION: EXE 0x487ccd
// Removes a unit from every request; returns whether any held it
bool EECPPersonnel::RemoveFromRequests(EEComplexUnit* inUnit)
{
    bool theResult = false;
    std::map<long, EECPPersonnelRequest*>::iterator it;
    for (it = this->mRequests.begin(); it != this->mRequests.end(); ++it)
        if (it->second->Remove(inUnit))
            theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x487d12
// After loading, resolves the free units' saved ids (dropping missing and duplicate units), restores each request
// and removes units held by more than one request; returns the first request error
long EECPPersonnel::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::multimap<long, EEUnit*>::iterator it;
    std::map<long, EECPPersonnelRequest*>::iterator theRequestIt;
    long theResult = 0;
    std::map<long, EEComplexUnit*> theUnits;
    for (it = this->mUnits.begin(); it != this->mUnits.end();) {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit((unsigned long)it->second));
        if (theUnit != NULL) {
            if (theUnits.end() == theUnits.find(theUnit->mUnitID)) {
                it->second = theUnit;
                theUnit->Reference();
                ++it;
            } else
                it = this->mUnits.erase(it);
            theUnits.insert(std::map<long, EEComplexUnit*>::value_type(theUnit->mUnitID, theUnit));
        } else
            it = this->mUnits.erase(it);
    }
    // restore every request, keeping the first error (each result used to overwrite the previous one)
    for (theRequestIt = this->mRequests.begin(); theRequestIt != this->mRequests.end(); ++theRequestIt) {
        long theRequestResult = theRequestIt->second->Restore(inMajorVersion, inMinorVersion);
        if (theResult == 0)
            theResult = theRequestResult;
    }
    std::map<long, EEComplexUnit*> theTaken;
    std::vector<long> theIDs;
    for (theRequestIt = this->mRequests.begin(); theRequestIt != this->mRequests.end(); ++theRequestIt) {
        EECPPersonnelRequest* theRequest = theRequestIt->second;
        bool theChanged = false;
        for (long i = theRequest->mUnits.size() - 1; i >= 0; i--) {
            if ((theRequest->mUnits[i]).mUnit == NULL)
                continue;
            if (theTaken.end() == theTaken.find((theRequest->mUnits[i]).mUnit->mUnitID))
                theTaken.insert(std::map<long, EEComplexUnit*>::value_type(
                    (theRequest->mUnits[i]).mUnit->mUnitID, (theRequest->mUnits[i]).mUnit));
            else {
                theRequest->mUnits.erase(theRequest->mUnits.begin() + i);
                theChanged = true;
            }
        }
        if (theChanged)
            theRequest->UpdateFill();
    }
    return theResult;
}

// FUNCTION: EXE 0x487f9e
// Asks the unit manager to build one unit of a type for a town and continent
void EECPPersonnel::RequestProduction(long inUnitClass, long inTownID, unsigned short inContinent)
{
    this->mPlayer->mCivilizationManager->mUnitManager->RequestUnits(inUnitClass, 1, inTownID, inContinent);
}

// FUNCTION: EXE 0x487fc0
// Takes back a unit (a loaded transport unloads first): offers it to the requests, else keeps it free, then tries
// to draft it; does nothing while loading, clearing the world or observing
void EECPPersonnel::ReturnUnit(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    if (gLoadingSavedGame || gWorldClearing || this->mPlayer->mObserverMode)
        return;
    if (theUnit->mUnitClass->IsTransport() && theUnit->GetNumCargoUnits() > 0)
        this->UnloadTransport(theUnit);
    if (theUnit->mIsDetachedFromAI || theUnit->mPlayer != this->mPlayer)
        return;
    if (!this->OfferUnit(theUnit)) {
        theUnit->Reference();
        this->mUnits.insert(std::multimap<long, EEUnit*>::value_type(theUnit->mUnitClass->mUnitType, theUnit));
    }
    this->DraftUnit(theUnit);
}

// FUNCTION: EXE 0x488057
// Writes the free units (type, id), the requests, the next request id and the deferred unit ids to the saved
// game; returns the first write error
// each block runs only while every earlier write succeeded (the original returns at the first failure)
long EECPPersonnel::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theType;
    unsigned long theUnitID;
    long theKey;
    std::multimap<long, EEUnit*>::iterator theUnit;
    std::map<long, EECPPersonnelRequest*>::iterator theRequest;
    std::set<unsigned long>::iterator theID;
    unsigned long theCount = this->mUnits.size();
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (theUnit = this->mUnits.begin(); theUnit != this->mUnits.end(); ++theUnit) {
        theType = theUnit->first;
        if (theResult == 0 && (theResult = outFile.Write(theType)) == 0) {
            theUnitID = theUnit->second->mUnitID;
            theResult = outFile.Write(theUnitID);
        }
    }
    if (theResult != 0)
        return theResult;
    theCount = this->mRequests.size();
    if ((theResult = outFile.Write(theCount)) != 0)
        return theResult;
    for (theRequest = this->mRequests.begin(); theRequest != this->mRequests.end(); ++theRequest) {
        theKey = theRequest->first;
        if (theResult == 0 && (theResult = outFile.Write(theKey)) == 0)
            theResult = theRequest->second->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult != 0 || (theResult = outFile.Write(this->mNextID)) != 0)
        return theResult;
    theCount = this->mDeferredUnits.size();
    if ((theResult = outFile.Write(theCount)) != 0)
        return theResult;
    for (theID = this->mDeferredUnits.begin(); theID != this->mDeferredUnits.end(); ++theID)
        if (theResult == 0)
            theResult = outFile.Write(*theID);
    return theResult;
}

// FUNCTION: EXE 0x4881cc
// Takes in a new unit: offers it to the requests, else keeps it free, then tries to draft it; if not drafted, sends
// a naval or space building next to a dock and an anti-missile battery next to a town centre
void EECPPersonnel::AddNewUnit(EEComplexUnit* inUnit)
{
    if (gWorldClearing || gLoadingSavedGame)
        return;
    if (!this->OfferUnit(inUnit)) {
        inUnit->Reference();
        inUnit->SetFlightTime(inUnit->mStats->mFlightTime);
        this->mUnits.insert(std::multimap<long, EEUnit*>::value_type(inUnit->mUnitClass->mUnitType, inUnit));
    }
    if (this->DraftUnit(inUnit))
        return;
    EEUnitGoal* theGoal;
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory == kUnitCategoryNavalBuilding || theCategory == kUnitCategorySpaceBuilding) {
        EETileCoordinate theTile;
        theTile.mX = 0;
        theTile.mY = 0;
        if (!this->mPlayer->mCivilizationManager->GetBuildManager()->FindSpotNearDock(inUnit, theTile))
            return;
        theGoal = new EEUGLocation((float)theTile.mX, (float)theTile.mY, 0.0f, false, true, false, 0.0f, false, 0);
    } else if (inUnit->mUnitClass->mUnitType == kUnitType14AntiMissileBattery) {
        EETileCoordinate theTile;
        theTile.mX = 0;
        theTile.mY = 0;
        if (!this->mPlayer->mCivilizationManager->GetBuildManager()->FindSpotNearTownCenter(inUnit, theTile))
            return;
        theGoal = new EEUGLocation((float)theTile.mX, (float)theTile.mY, 0.0f, false, true, false, 0.0f, false, 0);
    } else
        return;
    inUnit->SetGoal(theGoal, 0, 0);
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x488337
// Removes a unit from the free pool, else from the requests; a draftable unit found leaves its area groups;
// returns whether it was found
bool EECPPersonnel::RemoveUnit(EEComplexUnit* inUnit)
{
    std::multimap<long, EEUnit*>::iterator it = this->mUnits.find(inUnit->mUnitClass->mUnitType);
    long theType = inUnit->mUnitClass->mUnitType;
    bool theFound = it != this->mUnits.end();
    bool theRemoved = false;
    if (theFound) {
        while (it != this->mUnits.end() && it->second->mUnitClass->mUnitType == (unsigned long)theType) {
            if (it->second == inUnit) {
                inUnit->Dereference();
                this->mUnits.erase(it);
                theRemoved = true;
                break;
            }
            ++it;
        }
    }
    if (!theRemoved)
        theFound = this->RemoveFromRequests(inUnit);
    if (theFound && EECPMayDraft(inUnit->mUnitClass)) {
        this->mPlayer->mCombatManager->ReleaseFromAreaGroups(inUnit);
        inUnit->SetAIState(0x46cd6);
    }
    return theFound;
}

// FUNCTION: EXE 0x488403
// Sends a loaded transport with no other goal to unload: air transports at a random point of a random land
// continent, sea/space ones on a shore of a neighbouring land continent, up to 21 tries; then detaches it from the AI
void EECPPersonnel::UnloadTransport(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<unsigned short> theIDs;
    if (inUnit->mHitPoints <= 0 || inUnit->mIsDisabled || inUnit->GetTileX() == -10000 || !inUnit->GetUnitClass()->IsTransport() ||
        inUnit->mCargo.size() == 0)
        return;
    if (inUnit->mActiveGoal != NULL && inUnit->mActiveGoal->mKind != kGoalLocation)
        return;
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(inUnit);
    if (theContinent == NULL)
        return;
    EEUGUnloadTransport* theGoal = new EEUGUnloadTransport(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), true);
    bool theFound = false;
    long n;
    if (inUnit->GetUnitClass()->mDomains & kMovementAir) {
        EEAIContinent* theTarget = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryLand, gPhysics->mRandom));
        if (theTarget != NULL) {
            for (n = 0x14; n >= 0; n--) {
                theGoal->mDestination = theTarget->GetRandomPoint();
                theGoal->FindUnloadTiles();
                if (theGoal->CanUnloadAtDestination()) {
                    theFound = true;
                    break;
                }
            }
        }
    } else if (inUnit->GetUnitClass()->mDomains & kMovementSeaOrSpace) {
        EEAIContinent* theTarget = theContinent;
        if (theContinent->mCategory != kContinentCategoryLand) {
            gContinentManager.GetAdjacentIDs(theIDs, theContinent->mID);
            if (theIDs.size() != 0) {
                Fn4EEF9(theIDs, UMFloatToLong(gPhysics->mRandom.Generate()));
                while (theIDs.size() != 0) {
                    if (gContinentManager.GetContinentByID(theIDs.back()) != NULL &&
                        gContinentManager.GetContinentByID(theIDs.back())->mCategory == kContinentCategoryLand) {
                        theTarget = gContinentManager.GetContinentByID(theIDs.back());
                        break;
                    }
                    theIDs.pop_back();
                }
                if (theTarget == NULL)
                    theTarget = theContinent;
            }
        }
        if (inUnit->GetUnitClass()->mDomains & kMovementSea)
            theTarget->GetShoreInRegion(gContinentManager.GetContinent(inUnit, kAIPathWaterBody), theTiles, kAIPathWaterBody);
        else
            theTarget->GetShoreInRegion(gContinentManager.GetContinent(inUnit, kAIPathSpaceRegion), theTiles, kAIPathSpaceRegion);
        if (theTiles.size() != 0) {
            Fn4EEF9(theTiles, UMFloatToLong(gPhysics->mRandom.Generate()));
            for (n = 0x14; theTiles.size() != 0; ) {
                theGoal->mDestination = theTiles.back();
                theGoal->FindUnloadTiles();
                if (theGoal->CanUnloadAtDestination()) {
                    theFound = true;
                    break;
                }
                theTiles.pop_back();
                if (--n < 0)
                    break;
            }
        }
    }
    if (theFound) {
        inUnit->SetGoal(theGoal, 0, 0);
        inUnit->StartAction(kActionPrepareToMove);
        inUnit->GetUnitClass()->ActionChanged(inUnit);
        if (inUnit->mGoalState != NULL)
            this->mPlayer->EEPlayer::DetachUnitFromAI(inUnit);
    } else if (theGoal != NULL)
        delete theGoal;
}

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x488750
// TEMPLATE: EXE 0x48877b
// TEMPLATE: EXE 0x4887b6
// TEMPLATE: EXE 0x4887c6
// TEMPLATE: EXE 0x4887f2
// TEMPLATE: EXE 0x488826
// TEMPLATE: EXE 0x488887
// TEMPLATE: EXE 0x4888ed
// TEMPLATE: EXE 0x488901
// TEMPLATE: EXE 0x4889e2
// TEMPLATE: EXE 0x4889f6
// TEMPLATE: EXE 0x488a67
// TEMPLATE: EXE 0x488b12
// TEMPLATE: EXE 0x488b62
// TEMPLATE: EXE 0x488c78
// TEMPLATE: EXE 0x488ce9
// TEMPLATE: EXE 0x488d2e
// TEMPLATE: EXE 0x489034
// TEMPLATE: EXE 0x48909e
// TEMPLATE: EXE 0x489273
// TEMPLATE: EXE 0x48948b
// TEMPLATE: EXE 0x4894c1

// Template and inline COMDATs this unit already emits (compared by name)
// TEMPLATE: EXE 0x4888c2
// TEMPLATE: EXE 0x48892c
// TEMPLATE: EXE 0x488ba6
// TEMPLATE: EXE 0x488bdf
// TEMPLATE: EXE 0x488cfc
// TEMPLATE: EXE 0x488fd9
// TEMPLATE: EXE 0x489066
// TEMPLATE: EXE 0x4894f2
// TEMPLATE: EXE 0x48951a
// TEMPLATE: EXE 0x489551
