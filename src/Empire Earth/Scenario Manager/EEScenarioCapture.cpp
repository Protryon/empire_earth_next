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
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMEffect.h"
#include "ee/EETechCapture.h"
#include "ee/EEScenarioCapture.h"

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
    ioRecord.U32((unsigned int)inManager->mTimedEffects.size());
    for (const EESMTimedEffect& e : inManager->mTimedEffects) {
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

bool EESMCaptureInObject()
{
    return sObjectDepth > 0;
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
}

void EESMCaptureUpdate(EEScenarioManager* inManager, bool inHumanPlaying)
{
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
