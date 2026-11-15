// New (port), TEST ONLY: the scenario manager's capture records (ee/EEScenarioCapture.h; tools/sim_capture.py
// --profile scenario). Reads only: nothing here changes the game.
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "lle/GERectangle.h"
#include "lle/U2DPoint.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMEffect.h"
#include "ee/EETechCapture.h"
#include "lle/FSMemoryFile.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EECPTrace.h"
#include "ee/EECameraControl.h"
#include "ee/EEEnvironmentManager.h"

namespace {

thread_local int sObjectDepth = 0;

unsigned long long Fnv(unsigned long long inHash, const void* inData, size_t inSize)
{
    const unsigned char* theBytes = (const unsigned char*)inData;
    for (size_t i = 0; i < inSize; i++)
        inHash = (inHash ^ theBytes[i]) * 0x100000001b3ULL;
    return inHash;
}

const unsigned long long kFnvStart = 0xcbf29ce484222325ULL;

unsigned long long TextHash(const UWideString& inText)
{
    const char* theText = (const char*)inText;
    return Fnv(kFnvStart, theText, theText ? strlen(theText) : 0);
}

void U64(EESimCaptureRecord& ioRecord, unsigned long long inValue)
{
    ioRecord.Bytes(&inValue, 8);
}

void Units(EESimCaptureRecord& ioRecord, const std::vector<unsigned long>& inUnits)
{
    unsigned long long theHash = kFnvStart;
    for (unsigned long u : inUnits) {
        unsigned int theID = (unsigned int)u;
        theHash = Fnv(theHash, &theID, 4);
    }
    ioRecord.U32((unsigned int)inUnits.size());
    U64(ioRecord, theHash);
}

void TimedEffects(EESimCaptureRecord& ioRecord, EEScenarioManager* inManager)
{
    ioRecord.U32((unsigned int)inManager->GetNumTimedEffects());
    for (unsigned long i = 0; i < inManager->GetNumTimedEffects(); i++) {
        EeScenarioTimedEntry e = inManager->GetTimedEntry(i);
        ioRecord.U32((unsigned int)e.mEffectIndex);
        ioRecord.U32((unsigned int)e.mTime);
        ioRecord.U32((e.mIsInCinematic ? 1u : 0u) | (e.mHasRun ? 2u : 0u));
    }
}

long IndexOf(EESMObject* inObject)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    for (unsigned long i = 0; theManager && i < theManager->mObjects.size(); i++)
        if (theManager->mObjects[i] == inObject)
            return (long)i;
    return -1;
}

long IndexOf(EESMArea* inArea)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    for (unsigned long i = 0; theManager && i < theManager->mAreas.size(); i++)
        if (theManager->mAreas[i] == inArea)
            return (long)i;
    return -1;
}

}   // namespace

unsigned int EESMCaptureSeed()
{
    return gPhysics != NULL ? (unsigned int)gPhysics->mRandom.GetSeed() : 0u;
}

void EESMCaptureCondition(EESMCondition* inCondition, bool inResult, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMCE"));
    r.U32((unsigned int)inCondition->mID);
    r.U32((unsigned int)inCondition->mType);
    r.U32((unsigned int)inCondition->mOperator);
    r.U32((unsigned int)inCondition->mDifficulty);
    r.U32((unsigned int)inCondition->mDiplomacy);
    r.U32((unsigned int)inCondition->mAttribute);
    r.U32((unsigned int)inCondition->mOtherAttribute);
    r.I32((int)inCondition->mTechnology);
    r.U32((unsigned int)inCondition->mAmount);
    r.U32((unsigned int)inCondition->mObjectIndex);
    r.U32((unsigned int)inCondition->mPlayer);
    r.U32((unsigned int)inCondition->mOtherPlayer);
    r.U32((unsigned int)inCondition->mMinTime);
    r.U32((unsigned int)inCondition->mMaxTime);
    r.U32((unsigned int)inCondition->mHour);
    r.U32((unsigned int)inCondition->mTrigger);
    r.U32((inCondition->mIsTriggerOn ? 1u : 0u) | (inCondition->mHasTriggerFired ? 2u : 0u) |
          (inCondition->mUsesDifficulty ? 4u : 0u) | (inCondition->mUsesElapsedTime ? 8u : 0u) |
          (inCondition->mUsesHour ? 16u : 0u) | (inCondition->mUsesPlayerAttribute ? 32u : 0u));
    U64(r, TextHash(inCondition->mChatText));
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    U64(r, theManager ? TextHash(theManager->mLastChatText) : 0);
    r.F32(theManager ? theManager->mTime : 0.0f);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    r.U32(inResult ? 1u : 0u);
    r.Commit();
}

EESMCaptureTriggerState EESMCaptureTriggerBefore(EESMTrigger* inTrigger)
{
    EESMCaptureTriggerState theState;
    theState.mIsOn = inTrigger->mIsOn;
    theState.mHasFired = inTrigger->mHasFired;
    theState.mStart = inTrigger->mConditionStartTime;
    return theState;
}

void EESMCaptureTrigger(EESMTrigger* inTrigger, const EESMCaptureTriggerState& inBefore, float inTime, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMTE"));
    r.U32((unsigned int)inTrigger->mID);
    r.U32((inBefore.mIsOn ? 1u : 0u) | (inBefore.mHasFired ? 2u : 0u));
    r.U32((unsigned int)inBefore.mStart);
    r.F32(inTime);
    r.U32((unsigned int)inTrigger->mConditionDuration);
    r.U32((inTrigger->mIsLooping ? 1u : 0u) | (inTrigger->mIsInCinematic ? 2u : 0u) | (gWorld.mInCinematic ? 4u : 0u));
    r.U32((unsigned int)inTrigger->mConditions.size());
    r.U32((unsigned int)inTrigger->mEffects.size());
    r.U32((inTrigger->mIsOn ? 1u : 0u) | (inTrigger->mHasFired ? 2u : 0u));
    r.U32((unsigned int)inTrigger->mConditionStartTime);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    if (gWorld.mScenarioManager)
        TimedEffects(r, gWorld.mScenarioManager);
    r.Commit();
}

EESMCaptureObjectState EESMCaptureObjectBegin(EESMObject* inObject)
{
    EESMCaptureObjectState theState;
    theState.mCount = (unsigned int)inObject->mUnits.size();
    theState.mHash = kFnvStart;
    for (unsigned long u : inObject->mUnits) {
        unsigned int theID = (unsigned int)u;
        theState.mHash = Fnv(theState.mHash, &theID, 4);
    }
    theState.mSeed = EESMCaptureSeed();
    sObjectDepth++;
    return theState;
}

void EESMCaptureObjectEnd(EESMObject* inObject, bool inIncludeDead, bool inResult, const EESMCaptureObjectState& inBefore)
{
    sObjectDepth--;
    EESimCaptureRecord r(EECaptureTag("SMOB"));
    r.I32((int)IndexOf(inObject));
    r.U32((unsigned int)inObject->mL64);
    r.U32((unsigned int)inObject->mOwner);
    r.U32((unsigned int)inObject->mFamily);
    r.U32((unsigned int)inObject->mClass);
    r.U32((unsigned int)inObject->mMinCount);
    r.U32((unsigned int)inObject->mMaxCount);
    r.U32((unsigned int)inObject->mArea);
    r.U32((unsigned int)inObject->mNearMode);
    r.U32((unsigned int)inObject->mNearObject);
    r.U32((unsigned int)inObject->mParent);
    r.U32((unsigned int)inObject->mAttribute);
    r.U32((unsigned int)inObject->mAttributeMin);
    r.U32((unsigned int)inObject->mAttributeMax);
    r.U32((unsigned int)inObject->mVisibleByPlayer);
    r.U32((unsigned int)inObject->mState);
    r.U32((unsigned int)inObject->mSelectedByPlayer);
    r.U32((unsigned int)inObject->mInAreaCheck);
    r.U32((unsigned int)inObject->mNearCheck);
    r.U32((unsigned int)inObject->mHasAttributeCheck);
    r.U32((unsigned int)inObject->mVisibleByCheck);
    r.U32((unsigned int)inObject->mHasStateCheck);
    r.U32((unsigned int)inObject->mSelectedByCheck);
    r.U32((inObject->mIsAttributePercent ? 1u : 0u) | (inIncludeDead ? 2u : 0u));
    r.U32(inBefore.mCount);
    U64(r, inBefore.mHash);
    r.U32(inBefore.mSeed);
    r.U32(inResult ? 1u : 0u);
    Units(r, inObject->mUnits);
    r.U32((inObject->mIsSelectOnMap ? 1u : 0u) | (inObject->mIsDynamic ? 2u : 0u));
    r.U32(EESMCaptureSeed());
    r.Commit();
}

void EESMCapturePlayers(long inType, bool inSingle, long inResult, const void* inPlayers, unsigned int inSeed)
{
    if (sObjectDepth > 0)
        return;
    const std::vector<long>* thePlayers = (const std::vector<long>*)inPlayers;
    EESimCaptureRecord r(EECaptureTag("SMCP"));
    r.I32((int)inType);
    r.U32(inSingle ? 1u : 0u);
    r.I32((int)inResult);
    r.U32(thePlayers ? (unsigned int)thePlayers->size() : 0xffffffffu);
    if (thePlayers)
        for (long p : *thePlayers)
            r.I32((int)p);
    r.U32((unsigned int)gWorld.mNumPlayers);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    r.Commit();
}

void EESMCaptureArea(EESMArea* inArea, long inKind, long inA, long inB, long inX, long inY, unsigned long long inHash)
{
    if (sObjectDepth > 0)
        return;
    EESimCaptureRecord r(EECaptureTag("SMAR"));
    r.I32((int)inKind);
    r.I32((int)IndexOf(inArea));
    r.U32((unsigned int)inArea->mID);
    r.U32((unsigned int)inArea->mSelectMode);
    r.I32((int)inArea->mAnchor.mX);
    r.I32((int)inArea->mAnchor.mY);
    r.F32(inArea->mRect.GetLeft());
    r.F32(inArea->mRect.GetTop());
    r.F32(inArea->mRect.GetRight());
    r.F32(inArea->mRect.GetBottom());
    r.I32((int)inA);
    r.I32((int)inB);
    r.I32((int)inX);
    r.I32((int)inY);
    U64(r, inHash);
    r.Commit();
}

void EESMCaptureEffect(EESMEffect* inEffect, bool inIsCinematic, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMEX"));
    r.U32((unsigned int)inEffect->mID);
    r.U32((unsigned int)inEffect->mType);
    r.U32((unsigned int)inEffect->mAIAction);
    r.U32((unsigned int)inEffect->mMediaAction);
    r.U32((unsigned int)inEffect->mPlayerAction);
    r.U32((unsigned int)inEffect->mObjectAction);
    r.U32((unsigned int)inEffect->mGameAction);
    r.U32((unsigned int)inEffect->mObject);
    r.U32((unsigned int)inEffect->mPlayer);
    r.U32(inIsCinematic ? 1u : 0u);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    r.I32(theManager ? (int)theManager->mGameResult : 0);
    r.U32(gWorld.mInCinematic ? 1u : 0u);
    if (theManager)
        TimedEffects(r, theManager);
    r.Commit();
    EESMCaptureEffectWorld(inEffect);
}

namespace {

// the FNV-1a of a memory file's bytes
unsigned long long FileHash(FSMemoryFile& inFile)
{
    unsigned long theLength = 0;
    inFile.GetLength(theLength);
    return Fnv(kFnvStart, inFile.GetData(), theLength);
}

// a unit of an effect's objects as SMEW reads it: its checksum and the state the effects write
void EffectUnit(EESimCaptureRecord& ioRecord, unsigned long inID)
{
    EEUnit* theUnit = gWorld.GetUnit(inID);
    ioRecord.U32((unsigned int)inID);
    ioRecord.U32(theUnit != NULL ? 1u : 0u);
    if (theUnit == NULL)
        return;
    ioRecord.I32((int)theUnit->GetChecksum(3));
    ioRecord.I32(theUnit->mPlayer != NULL ? (int)theUnit->mPlayer->mPlayerID : -1);
    ioRecord.I32((int)theUnit->GetHitPoints());
    ioRecord.I32((int)theUnit->mTileX);
    ioRecord.I32((int)theUnit->mTileY);
    ioRecord.U32(theUnit->mTileNode != NULL ? 1u : 0u);
    U64(ioRecord, TextHash(theUnit->GetStats()->mName));
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    ioRecord.U32(theComplex != NULL ? 1u : 0u);
    if (theComplex == NULL)
        return;
    ioRecord.U32((unsigned int)theComplex->GetCommandType());
    EEUnitGoal* theGoal = theComplex->GetGoal();
    ioRecord.I32(theGoal != NULL ? (int)theGoal->mKind : -1);
    ioRecord.U32((unsigned int)theComplex->mGoalQueues.size());
    for (size_t q = 0; q < theComplex->mGoalQueues.size(); q++) {
        ioRecord.U32((unsigned int)theComplex->mGoalQueues[q].mGoals.size());
        for (size_t g = 0; g < theComplex->mGoalQueues[q].mGoals.size(); g++)
            ioRecord.I32(theComplex->mGoalQueues[q].mGoals[g] != NULL ? (int)theComplex->mGoalQueues[q].mGoals[g]->mKind : -1);
    }
    ioRecord.U32((theComplex->mIsDisabled ? 1u : 0u) | (theComplex->mIsCommandable ? 2u : 0u) |
                 (theComplex->mIsDeletable ? 4u : 0u) | (theComplex->mIsLocked ? 8u : 0u) |
                 (theComplex->mIsDetachedFromAI ? 16u : 0u));
    long theBehavior = -1;
    for (size_t b = 0; b < EEUnitBehavior::sTable.size(); b++)
        if (EEUnitBehavior::sTable[b] == theComplex->mBehavior)
            theBehavior = (long)b;
    ioRecord.I32((int)theBehavior);
    ioRecord.I32(theComplex->mContainer != NULL ? (int)theComplex->mContainer->mUnitID : -1);
    ioRecord.I32((int)theComplex->mFlightTime);
    ioRecord.I32((int)theComplex->mAmmunition);
    ioRecord.U32((unsigned int)theComplex->mCargo.size());
    const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
    ioRecord.Bytes(&theLocation.mMatrix, sizeof(theLocation.mMatrix));
}

void EffectObject(EESimCaptureRecord& ioRecord, unsigned long inIndex)
{
    EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject(inIndex);
    ioRecord.U32(theObject != NULL ? (unsigned int)theObject->mUnits.size() : 0xffffffffu);
    if (theObject != NULL)
        for (unsigned long u : theObject->mUnits)
            EffectUnit(ioRecord, u);
}

}   // namespace

void EESMCaptureEffectWorld(EESMEffect* inEffect)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    if (theManager == NULL)
        return;
    EESimCaptureRecord r(EECaptureTag("SMEW"));
    r.U32((unsigned int)inEffect->mID);
    {
        FSMemoryFile theFile;
        r.I32((int)inEffect->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile));
        U64(r, FileHash(theFile));
    }
    // the world's out-of-synch sums as EE-SIM's --checksum-trace reads them (reads only)
    unsigned long theSaved = gPhysics->mLastOutOfSynchChecksum;
    r.U32((unsigned int)gPhysics->GetOutOfSynchChecksum(false, 0x1 | 0x2 | 0x10 | 0x40 | 0x100));
    gPhysics->mLastOutOfSynchChecksum = theSaved;
    unsigned long theUnits = 0;
    unsigned long theCount = 0;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext) {
        theUnits += (unsigned long)static_cast<EEUnit*>(theUnit)->GetChecksum(3);
        theCount++;
    }
    r.U32((unsigned int)theCount);
    r.U32((unsigned int)theUnits);
    r.U32((unsigned int)EECPOutOfSynchChecksum());
    // the units of the objects an effect names
    const unsigned long theObjects[] = {inEffect->mObject, inEffect->mTaskObject, inEffect->mPlaceObject,
                                        inEffect->mTargetObject, inEffect->mLocationObject};
    for (unsigned long theIndex : theObjects)
        EffectObject(r, theIndex);
    // the players
    r.U32((unsigned int)gWorld.mNumPlayers);
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        r.U32(thePlayer != NULL ? 1u : 0u);
        if (thePlayer == NULL)
            continue;
        r.U32((unsigned int)thePlayer->mGameResult);
        r.U32((thePlayer->mSeesAll ? 1u : 0u) | (thePlayer->mObserverMode ? 2u : 0u));
        for (unsigned long q = 0; q < gWorld.mNumPlayers; q++)
            r.I32((int)thePlayer->mEnemies[q]);
        r.U32((unsigned int)thePlayer->mPendingGroups.size());
        r.U32((unsigned int)thePlayer->mRunningGroups.size());
        r.U32((unsigned int)thePlayer->mPatrolGroups.size());
        r.U32((unsigned int)thePlayer->mCivilization.mAvailablePoints);
        r.U32((unsigned int)thePlayer->mCivilization.mMaxPoints);
        EEComputerPlayer* theComputer = thePlayer->mComputerPlayer;
        r.U32(theComputer != NULL ? 1u : 0u);
        if (theComputer != NULL) {
            r.U32((unsigned int)theComputer->mFavoriteLandType);
            r.U32((unsigned int)theComputer->mFavoriteSeaType);
            r.U32((unsigned int)theComputer->mFavoriteAirType);
        }
    }
    // the manager, the world and the camera
    U64(r, TextHash(theManager->mMediaStrings.GetSection(0)));
    U64(r, TextHash(theManager->mMediaStrings.GetSection(3)));
    U64(r, TextHash(theManager->mMediaStrings.GetSection(2)));
    r.U32((unsigned int)theManager->mMessages.size());
    r.U32((unsigned int)theManager->mPendingEffects.size());
    r.U32((unsigned int)theManager->mTriggers.size());
    for (EESMTrigger* t : theManager->mTriggers)
        r.U32(t ? ((t->mIsOn ? 1u : 0u) | (t->mHasFired ? 2u : 0u)) | ((unsigned int)t->mConditionStartTime << 2) : 0xffffffffu);
    r.F32(gWorld.GetTimeOfDay());
    r.F32(gWorld.mGameSpeed);
    r.U32((gPhysics->mTimeOfDayRuns ? 1u : 0u) | (gWorld.mInCinematic ? 2u : 0u) |
          (gWorld.mEnvironmentManager != NULL && gWorld.mEnvironmentManager->mIsEnabled ? 4u : 0u));
    r.U32((unsigned int)gWorld.mCameraControl.mKeys.size());
    r.U32((unsigned int)gWorld.mCameraControl.mPathIndex);
    r.Commit();
}

namespace {

// a list's count, the first error and the hash of its elements' Saves
template<class T> void SavedList(EESimCaptureRecord& ioRecord, const std::vector<T*>& inList, unsigned long inMajor,
                                 unsigned long inMinor)
{
    FSMemoryFile theFile;
    long theResult = kErrorOK;
    for (T* theElement : inList)
        if (theElement != NULL && theResult == kErrorOK)
            theResult = theElement->Save(inMajor, inMinor, theFile);
    ioRecord.U32((unsigned int)inList.size());
    ioRecord.I32((int)theResult);
    U64(ioRecord, FileHash(theFile));
}

}   // namespace

void EESMCaptureSaved(EEScenarioManager* inManager, long inKind, long inResult, unsigned long inMajorVersion,
                      unsigned long inMinorVersion)
{
    EESimCaptureRecord r(EECaptureTag("SMSV"));
    r.I32((int)inKind);
    r.I32((int)inResult);
    SavedList(r, inManager->mTriggers, inMajorVersion, inMinorVersion);
    SavedList(r, inManager->mConditions, inMajorVersion, inMinorVersion);
    SavedList(r, inManager->mEffects, inMajorVersion, inMinorVersion);
    SavedList(r, inManager->mObjects, inMajorVersion, inMinorVersion);
    SavedList(r, inManager->mAreas, inMajorVersion, inMinorVersion);
    SavedList(r, inManager->mPendingEffects, inMajorVersion, inMinorVersion);
    FSMemoryFile theFile;
    r.I32((int)inManager->Save(inMajorVersion, inMinorVersion, theFile));
    U64(r, FileHash(theFile));
    r.Commit();
}

void EESMCaptureUpdate(EEScenarioManager* inManager, bool inHumanPlaying)
{
    static unsigned long sRounds = 0;
    if (++sRounds % 120 == 0)
        EESMCaptureSaved(inManager, 1, 0, kSaveGameMajorVersion, kSaveGameMinorVersion);
    EESimCaptureRecord r(EECaptureTag("SMUP"));
    r.F32(inManager->mTime);
    r.F32(inManager->mTriggerTimer);
    r.U32((inHumanPlaying ? 1u : 0u) | (gWorld.mInCinematic ? 2u : 0u) | (inManager->mIsMovingCamera ? 4u : 0u) |
          (inManager->mIsShowingDialog ? 8u : 0u) | (inManager->mIsEndingCinematic ? 16u : 0u));
    r.U32((unsigned int)inManager->mTriggers.size());
    for (EESMTrigger* t : inManager->mTriggers)
        r.U32(t ? ((t->mIsOn ? 1u : 0u) | (t->mHasFired ? 2u : 0u)) : 0xffffffffu);
    TimedEffects(r, inManager);
    r.U32((unsigned int)inManager->mMessages.size());
    r.I32((int)inManager->mGameResult);
    r.U32(EESMCaptureSeed());
    r.Commit();
}

void EESMCapturePlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMPA"));
    r.I32((int)inPlayer);
    r.U32((unsigned int)inAttribute);
    r.I32((int)inMode);
    r.I32((int)inAmount);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    EEPlayer* thePlayer = inPlayer >= 0 && inPlayer < (long)(sizeof(gWorld.mPlayers) / sizeof(gWorld.mPlayers[0]))
                              ? gWorld.mPlayers[inPlayer] : NULL;
    r.U32(thePlayer != NULL ? 1u : 0u);
    if (thePlayer != NULL) {
        for (int i = 0; i < 7; i++)
            r.I32((int)thePlayer->mResourceCaps[i]);
        r.I32((int)thePlayer->mShareBonus);
        r.I32(thePlayer->mComputerPlayer != NULL ? (int)thePlayer->mComputerPlayer->mDifficulty : -1);
        for (int i = 0; i < 10; i++)
            r.U32((unsigned int)thePlayer->mScenarioVariables[i]);
        for (int i = 0; i < 10; i++)
            r.U32((unsigned int)thePlayer->mTributes[i]);
        r.I32((int)thePlayer->mMiningLimit);
        FSMemoryFile theFile;
        r.I32((int)thePlayer->mSavedGroups.Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile));
        U64(r, FileHash(theFile));
    }
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    if (theManager != NULL)
        SavedList(r, theManager->mPendingEffects, kSaveGameMajorVersion, kSaveGameMinorVersion);
    r.Commit();
}

void EESMCaptureTokens(const UWideString& inText, const UWideString& outText, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMTK"));
    U64(r, TextHash(inText));
    U64(r, TextHash(outText));
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    r.Commit();
}

void EESMCapturePlacement(EESMObject* inObject, unsigned long inUnitType, bool inTryRandom, bool inResult, float inX,
                          float inY, unsigned int inSeed)
{
    EESimCaptureRecord r(EECaptureTag("SMFP"));
    r.I32((int)IndexOf(inObject));
    r.U32((unsigned int)inUnitType);
    r.U32((inTryRandom ? 1u : 0u) | (inResult ? 2u : 0u));
    r.F32(inX);
    r.F32(inY);
    r.U32(inSeed);
    r.U32(EESMCaptureSeed());
    r.Commit();
}
