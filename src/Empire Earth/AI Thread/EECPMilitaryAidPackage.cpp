// C:\EEX\Empire Earth\AI Thread\EECPMilitaryAidPackage.cpp (file name inferred: no __FILE__; RTTI class
// EECPMilitaryAidPackage, vtable 0x43c0c8). 0x84d8d..0x85ace: EECPIntelligence.cpp's COMDATs end at 0x84d8d (the
// package's ctor, called from EECPDefenseCommander 0x77d8e/0x783f6/...), EECPPersonnelRequest's ctor (EECPPersonnel.cpp)
// starts at 0x85ace.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/LowLevelEngineCore.h"
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
#include "lle/FSRestorable.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGGather.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EECPIArmy.h"
#include "ee/EECPILocation.h"
#include "ee/EECPMilitaryAidPackage.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPDefenseCommander.h"
// FUNCTION: EXE 0x484d8d
// Creates a recalled aid package with no sub groups; the first send waits the town-guard delay for the difficulty
EECPMilitaryAidPackage::EECPMilitaryAidPackage(EEComputerPlayer* inPlayer)
    : mThreat(0.0f), mPlayer(inPlayer), mTarget(NULL), mTownID(0), mState(kAidStateRecalled), mPlayerID(0)
{
    if (gPhysics != NULL && inPlayer != NULL) {
        long theDelay;
        if (!inPlayer->IsDeathmatch()) {
            switch (inPlayer->mDifficulty) {
            case kDifficultyEasy:
                theDelay = inPlayer->mTownGuardUpdateEasy;
                break;
            case kDifficultyMedium:
                theDelay = inPlayer->mTownGuardUpdateMedium;
                break;
            default:
                theDelay = inPlayer->mTownGuardUpdateHard;
                break;
            }
        } else {
            switch (inPlayer->mDifficulty) {
            case kDifficultyEasy:
                theDelay = inPlayer->mDMTownGuardUpdateEasy;
                break;
            case kDifficultyMedium:
                theDelay = inPlayer->mDMTownGuardUpdateMedium;
                break;
            default:
                theDelay = inPlayer->mDMTownGuardUpdateHard;
                break;
            }
        }
        this->mNextSendTime = gPhysics->mGameTime + theDelay;
    } else
        this->mNextSendTime = 0;
    this->mNextUpdateTime = 0;
}

// FUNCTION: EXE 0x484e42
// Requests more sub groups when the town's worst threats outnumber them (send delay permitting), deploys them
// to the threats and restarts the send delay (ally-assist delay when helping a human)
void EECPMilitaryAidPackage::Gather()
{
    long theDelay;
    std::map<U2DPoint<long>, EECPDCTownThreat> theThreats;
    std::map<U2DPoint<long>, EECPDCTownThreat>::iterator it;   // after the map: its _Ptr = 0 follows the map ctor
    std::vector<EECPDCTownThreat> theWorst;
    long theTotal = 0;
    bool theSent = false;
    this->mPlayer->mCombatManager->mDefenseCommander->GetWorstThreats(this->mTarget, this->mTownID, theThreats, theWorst);
    if (theWorst.size() > 0) {
        if (this->mSubGroupIDs.size() < theWorst.size()) {
            if (this->mNextSendTime <= gPhysics->mGameTime) {
                this->mPlayer->mCombatManager->mDefenseCommander->AssignSubGroups(this, false);
                theSent = true;
            }
        } else if (this->mTarget == this->mPlayer && this->mSubGroupIDs.size() < theWorst.size() * 2) {
            for (it = theThreats.begin(); it != theThreats.end(); ++it)
                theTotal += it->second.mEnemy - it->second.mOwn;
            if (theTotal > 0 && this->mNextSendTime <= gPhysics->mGameTime) {
                this->mPlayer->mCombatManager->mDefenseCommander->AssignSubGroups(this, false);
                theSent = true;
            }
        }
        this->Deploy(theWorst);
        if (theSent) {
            if (this->mTarget != NULL && this->mTarget->mComputerPlayer == NULL) {
                switch (this->mPlayer->mDifficulty) {
                case kDifficultyEasy:
                    theDelay = this->mPlayer->mAssistAlliesDelayEasy;
                    break;
                case kDifficultyMedium:
                    theDelay = this->mPlayer->mAssistAlliesDelayMedium;
                    break;
                default:
                    theDelay = this->mPlayer->mAssistAlliesDelayHard;
                    break;
                }
            } else if (!this->mPlayer->IsDeathmatch()) {
                switch (this->mPlayer->mDifficulty) {
                case kDifficultyEasy:
                    theDelay = this->mPlayer->mTownGuardUpdateEasy;
                    break;
                case kDifficultyMedium:
                    theDelay = this->mPlayer->mTownGuardUpdateMedium;
                    break;
                default:
                    theDelay = this->mPlayer->mTownGuardUpdateHard;
                    break;
                }
            } else {
                switch (this->mPlayer->mDifficulty) {
                case kDifficultyEasy:
                    theDelay = this->mPlayer->mDMTownGuardUpdateEasy;
                    break;
                case kDifficultyMedium:
                    theDelay = this->mPlayer->mDMTownGuardUpdateMedium;
                    break;
                default:
                    theDelay = this->mPlayer->mDMTownGuardUpdateHard;
                    break;
                }
            }
            this->mNextSendTime = gPhysics->mGameTime + theDelay;
        }
    }
    this->mPlayer->IsDeathmatch();
}
// FUNCTION: EXE 0x485006
// Puts the package in state 3
void EECPMilitaryAidPackage::Fn85006()
{
    this->mState = 3;
}

// FUNCTION: EXE 0x48500e
// Returns the first tile of the target town ((0, 0) if the town is gone or empty) and stores it as the location
U2DPoint<long> EECPMilitaryAidPackage::GetLocation()
{
    std::vector<U2DPoint<long> > thePoints;
    long theX = 0;
    long theY = 0;
    EETown* theTown = this->mTarget->GetTownByID(this->mTownID);
    if (theTown != NULL && theTown->mTiles.Size() > 0) {
        theTown->mTiles.BuildPointList(thePoints, 1);
        if (thePoints.size() > 0) {
            theX = thePoints[0].mX;
            theY = thePoints[0].mY;
        }
    }
    this->mLocation.mX = theX;
    this->mLocation.mY = theY;
    return U2DPoint<long>(theX, theY);
}

// NEAR: the original indexes the ids through an out-of-line begin() + i
// FUNCTION: EXE 0x485084
// Calls off every live sub group: those on mission 3 switch to combat mode, the others drop their task
void EECPMilitaryAidPackage::Recall()
{
    for (long i = this->mSubGroupIDs.size() - 1; i >= 0; i--) {
        EECPAttackSubGroup* theGroup = this->mPlayer->mCombatManager->mAttackCommander->GetSubGroup(this->mSubGroupIDs[i]);
        if (theGroup == NULL || theGroup->GetNumUnits() == 0)
            continue;
        if (theGroup->mMission == 3)
            theGroup->mTask.mMode = kAGModeCombat;
        else
            theGroup->mTask.mIsActive = false;
    }
}

// FUNCTION: EXE 0x485105
// Returns true once the threat is at or below the aid threshold or the package has no sub groups
bool EECPMilitaryAidPackage::IsFinished()
{
    return !(this->mPlayer->mAidThreatThreshold < this->mThreat && this->mSubGroupIDs.size() != 0);
}

// FUNCTION: EXE 0x485131
// Changes the package's state and runs its entry action (recall, gather, or just the state change for 2 and 3)
void EECPMilitaryAidPackage::SetState(long inState)
{
    this->mState = inState;
    switch (inState) {
    case kAidStateRecalled:
        this->Recall();
        break;
    case kAidStateGathering:
        this->Gather();
        break;
    case 2:
        this->Fn8588A();
        break;
    case 3:
        this->Fn85006();
        break;
    }
}

// FUNCTION: EXE 0x485163
// Drops dead sub groups and runs the current state's update (releasing everything if the town is gone);
// returns the delay until the next update and schedules it
unsigned long EECPMilitaryAidPackage::Update(unsigned long inTime)
{
    unsigned long theDelay = 35000;
    this->DropDeadSubGroups();
    if (this->mSubGroupIDs.size() > 0) {
        EETown* theTown = this->mTarget->GetTownByID(this->mTownID);
        if (theTown != NULL) {
            switch (this->mState) {
            case kAidStateRecalled:
                theDelay = this->UpdateRecalled(theTown);
                break;
            case kAidStateGathering:
                theDelay = this->UpdateGathering(theTown);
                break;
            case 2:
                theDelay = this->Fn859D7(theTown);
                break;
            case 3:
                theDelay = this->Fn85922(theTown);
                break;
            }
        } else {
            this->ReleaseSubGroups();
            this->mNextSendTime = 0;
            this->mThreat = 0.0f;
        }
    }
    this->mNextUpdateTime = inTime + theDelay;
    return theDelay;
}

// FUNCTION: EXE 0x4851ec
// Reads the package from a saved game (target as a player id until Restore; the send time only from
// version 0.212 on); returns the first read error
long EECPMilitaryAidPackage::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    if (inMajorVersion == 0 && inMinorVersion < 0xd4) {
        unsigned long theCount = 0;
        unsigned long theValue;
        theResult = inFile.Read(theCount);
        if (theResult == 0) {
            this->mSubGroupIDs.resize(theCount, 0);
            for (unsigned long i = 0; i < theCount; i++) {
                theValue = 0;
                if (theResult == 0)
                    theResult = inFile.Read(theValue);
                this->mSubGroupIDs[i] = theValue;
            }
            if (theResult == 0)
                theResult = this->mLocation.Load(inFile);
        }
        theValue = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theValue);
            this->mTarget = reinterpret_cast<EEPlayer*>(theValue);
            if (theResult == 0)
                theResult = inFile.Read(this->mThreat);
            if (theResult == 0)
                theResult = inFile.Read(this->mTownID);
            if (theResult == 0)
                theResult = inFile.Read(this->mState);
            if (theResult == 0)
                theResult = inFile.Read(this->mNextUpdateTime);
            if (theResult == 0)
                theResult = inFile.Read(this->mPlayerID);
        }
    } else {
        unsigned long theCount = 0;
        unsigned long theValue;
        theResult = inFile.Read(theCount);
        if (theResult == 0) {
            this->mSubGroupIDs.resize(theCount, 0);
            for (unsigned long i = 0; i < theCount; i++) {
                theValue = 0;
                if (theResult == 0)
                    theResult = inFile.Read(theValue);
                this->mSubGroupIDs[i] = theValue;
            }
            if (theResult == 0)
                theResult = this->mLocation.Load(inFile);
        }
        theValue = 0;
        if (theResult == 0) {
            theResult = inFile.Read(theValue);
            this->mTarget = reinterpret_cast<EEPlayer*>(theValue);
            if (theResult == 0)
                theResult = inFile.Read(this->mThreat);
            if (theResult == 0)
                theResult = inFile.Read(this->mTownID);
            if (theResult == 0)
                theResult = inFile.Read(this->mState);
            if (theResult == 0)
                theResult = inFile.Read(this->mNextUpdateTime);
            if (theResult == 0)
                theResult = inFile.Read(this->mPlayerID);
            if (theResult == 0)
                theResult = inFile.Read(this->mNextSendTime);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4853e3
// Sends each idle sub group near the town (or inside its hull) and not already at a threat to the threats in
// turn in combat mode, moving it there unless it is already close or busy loading, unloading or attacking
void EECPMilitaryAidPackage::Deploy(std::vector<EECPDCTownThreat>& inThreats)
{
    EETown* theTown = this->mTarget->GetTownByID(this->mTownID);
    EECPILocation theLocation;
    U2DPoint<long> theClosest(0, 0);
    EETileLocation theTile(0, 0);
    unsigned long theSent = 0;
    std::vector<EECPDCTownThreat>::iterator it;
    std::vector<EECPDCTownThreat>::iterator theEnd;
    if (theTown == NULL || inThreats.size() == 0)
        return;
    for (long i = this->mSubGroupIDs.size() - 1; i >= 0; i--) {
        EECPAttackSubGroup* theSubGroup =
            this->mPlayer->mCombatManager->mAttackCommander->GetSubGroup(this->mSubGroupIDs[i]);
        if (theSubGroup == NULL || theSubGroup->mUnits.size() == 0 || theSubGroup->mPath.size() != 0)
            continue;
        theSubGroup->FindPosition(theTile, true, false);
        theClosest = theTown->mHull.GetClosestPoint(theTile);
        long theDistance = theClosest.DistanceToSquared(theTile);
        if (theDistance > 0x4b0 && !theTown->mHull.BoundedContains(theTile))
            continue;
        if (theDistance <= 0x90) {
            for (it = inThreats.begin(), theEnd = inThreats.end(); it != theEnd; ++it) {
                if (it->mLocation == theClosest)
                    break;
            }
            if (it != theEnd)
                continue;
        }
        if (theSubGroup->mTask.mIsActive) {
            for (it = inThreats.begin(), theEnd = inThreats.end(); it != theEnd; ++it) {
                if (it->mLocation == theSubGroup->mTask.mLocation.mTile)
                    break;
            }
            if (it != theEnd)
                continue;
        }
        theLocation = theSubGroup->mTask.mLocation;
        theLocation.Clear();
        theLocation.mTile = (*(inThreats.begin() + theSent % inThreats.size())).mLocation;
        theLocation.mPlayer = this->mTarget->mPlayerID;
        theSubGroup->mTask.mMode = kAGModeCombat;
        theSubGroup->mTask.mLocation = theLocation;
        long theThreatDistance = theLocation.mTile.DistanceToSquared(theTile);
        switch (theSubGroup->mState) {
        case kASGStateWaitForUnits:
        case kASGStateClearSubSector:
        case kASGStateBoardTransport:
        case kASGStateInTransit:
        case kASGStateUnload:
        case kASGStateAttack:
        case kASGStateAttackWhileBoarding:
        case kASGStateLoad:
        case kASGStateCarryCargo:
            break;
        default:
            if (theThreatDistance > 0x90)
                theSubGroup->SetDestination(theLocation);
            break;
        }
        theSent++;
    }
}

// FUNCTION: EXE 0x48566a
// Removes the ids of sub groups that no longer exist
void EECPMilitaryAidPackage::DropDeadSubGroups()
{
    EECPAttackCommander* theCommander = this->mPlayer->mCombatManager->mAttackCommander;
    for (long i = this->mSubGroupIDs.size() - 1; i >= 0; i--) {
        if (theCommander->GetSubGroup(this->mSubGroupIDs[i]) == NULL)
            this->mSubGroupIDs.erase(this->mSubGroupIDs.begin() + i);
    }
}

// FUNCTION: EXE 0x4856e4
// Turns the loaded player id back into the target player, finds the town nearest the saved location and
// drops dead sub groups; returns OK
long EECPMilitaryAidPackage::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    U2DPoint<long> theTile;
    this->mTarget = gWorld.mPlayers[(int32_t)USavedID(this->mTarget)];
    EETown* theTown = this->mTarget->GetNearestTown(this->mLocation, theTile);
    if (theTown != NULL)
        this->mTownID = theTown->mTownID;
    else
        this->mTownID = 0;
    this->DropDeadSubGroups();
    return 0;
}

// FUNCTION: EXE 0x48572c
// Clears the task of every live sub group and forgets all of them
void EECPMilitaryAidPackage::ReleaseSubGroups()
{
    EECPAttackCommander* theCommander = this->mPlayer->mCombatManager->mAttackCommander;
    for (long i = this->mSubGroupIDs.size() - 1; i >= 0; i--) {
        EECPAttackSubGroup* theGroup = theCommander->GetSubGroup(this->mSubGroupIDs[i]);
        if (theGroup != NULL) {
            theGroup->mTask.mMode = kAGModeNone;
            theGroup->mTask.mIsActive = false;
        }
    }
    this->mSubGroupIDs.clear();
}

// FUNCTION: EXE 0x485789
// Refreshes the location and writes the package to the saved game (target by player id); returns the first
// write error
long EECPMilitaryAidPackage::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = 0;
    unsigned long thePlayerID = 0;
    this->GetLocation();
    unsigned long theCount = this->mSubGroupIDs.size();
    theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (unsigned long i = 0; i < theCount; i++)
            if (theResult == 0)
                theResult = outFile.Write(this->mSubGroupIDs[i]);
        if (theResult == 0)
            theResult = this->mLocation.Save(outFile);
    }
    thePlayerID = this->mTarget->mPlayerID;
    if (theResult == 0)
        theResult = outFile.Write(thePlayerID);
    if (theResult == 0)
        theResult = outFile.Write(this->mThreat);
    if (theResult == 0)
        theResult = outFile.Write(this->mTownID);
    if (theResult == 0)
        theResult = outFile.Write(this->mState);
    if (theResult == 0)
        theResult = outFile.Write(this->mNextUpdateTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mPlayerID);
    if (theResult == 0)
        theResult = outFile.Write(this->mNextSendTime);
    return theResult;
}

// FUNCTION: EXE 0x48588a
// Puts the package in state 2
void EECPMilitaryAidPackage::Fn8588A()
{
    this->mState = 2;
}

// FUNCTION: EXE 0x485892
// Recomputes the town's threat; recalls the package if it fell to the aid threshold, otherwise gathers again;
// returns the next update delay
unsigned long EECPMilitaryAidPackage::UpdateGathering(EETown* inTown)
{
    EECPDefenseCommander* theCommander = this->mPlayer->mCombatManager->mDefenseCommander;
    long thePlayerID = 0;
    unsigned long theDelay = 20000;
    float theThreat;
    {
        std::map<U2DPoint<long>, EECPDCTownThreat> theThreats;
        theThreat = theCommander->ComputeTownThreat(inTown, thePlayerID, &theThreats, false);
    }
    this->mThreat = theThreat;
    if (theThreat <= this->mPlayer->GetAidThreatThreshold()) {
        this->SetState(kAidStateRecalled);
        theDelay = 35000;
    } else
        this->Gather();
    return theDelay;
}
// FUNCTION: EXE 0x485922
// State 3 update: reads the town's threat; recalls the package if it fell to the aid threshold, otherwise
// gathers again; returns the next update delay
unsigned long EECPMilitaryAidPackage::Fn85922(EETown* inTown)
{
    EECPDefenseCommander* theCommander = this->mPlayer->mCombatManager->mDefenseCommander;
    long thePlayerID = 0;
    unsigned long theDelay = 30000;
    float theThreat = theCommander->GetTownThreat(inTown, thePlayerID);
    this->mThreat = theThreat;
    if (theThreat <= this->mPlayer->GetAidThreatThreshold()) {
        this->SetState(kAidStateRecalled);
        theDelay = 35000;
    } else
        this->Gather();
    return theDelay;
}

// FUNCTION: EXE 0x485981
// Reads the town's threat and starts gathering once it exceeds the aid threshold; returns the next update delay
unsigned long EECPMilitaryAidPackage::UpdateRecalled(EETown* inTown)
{
    EECPDefenseCommander* theCommander = this->mPlayer->mCombatManager->mDefenseCommander;
    long thePlayerID = 0;
    unsigned long theDelay = 35000;
    float theThreat = theCommander->GetTownThreat(inTown, thePlayerID);
    this->mThreat = theThreat;
    if (theThreat > this->mPlayer->GetAidThreatThreshold()) {
        this->SetState(kAidStateGathering);
        theDelay = 20000;
    }
    return theDelay;
}

// FUNCTION: EXE 0x4859d7
// State 2 update: reads the town's threat; recalls the package if it fell to the aid threshold, otherwise
// gathers again; returns the next update delay
unsigned long EECPMilitaryAidPackage::Fn859D7(EETown* inTown)
{
    EECPDefenseCommander* theCommander = this->mPlayer->mCombatManager->mDefenseCommander;
    long thePlayerID = 0;
    unsigned long theDelay = 25000;
    float theThreat = theCommander->GetTownThreat(inTown, thePlayerID);
    this->mThreat = theThreat;
    if (theThreat <= this->mPlayer->GetAidThreatThreshold()) {
        this->SetState(kAidStateRecalled);
        theDelay = 35000;
    } else
        this->Gather();
    return theDelay;
}

// FUNCTION: EXE 0x485a36
// Copies another package (EECPDefenseCommander's package copies), all but its location
// Member order inferred from the stores
EECPMilitaryAidPackage& EECPMilitaryAidPackage::operator=(const EECPMilitaryAidPackage& inOther)
{
    this->mPlayer = inOther.mPlayer;
    this->mThreat = inOther.mThreat;
    this->mSubGroupIDs = inOther.mSubGroupIDs;
    this->mTarget = inOther.mTarget;
    this->mTownID = inOther.mTownID;
    this->mNextUpdateTime = inOther.mNextUpdateTime;
    this->mPlayerID = inOther.mPlayerID;
    this->mState = inOther.mState;
    this->mNextSendTime = inOther.mNextSendTime;
    return *this;
}
