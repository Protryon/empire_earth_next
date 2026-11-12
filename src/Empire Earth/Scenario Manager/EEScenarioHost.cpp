// New (port): the scenario logic's host (ee_scenario, docs/RUST_PLAN.md R5b.5; ee/EEScenarioHost.h). The Rust
// conditions, player choices and triggers read the world through one callback by operation code
// (EE_SCENARIO_READ_*, rust/crates/ee_scenario/src/host.rs); each case is the original's expression at its place.
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETown.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEScenarioCapture.h"
#include "ee/EEScenarioHost.h"

namespace {

// the world's player slot inA (NULL outside the array, as no original index reached)
EEPlayer* Player(int32_t inA)
{
    return inA >= 0 && inA < (int32_t)(sizeof(gWorld.mPlayers) / sizeof(gWorld.mPlayers[0])) ? gWorld.mPlayers[inA] : NULL;
}

// two longs as one read: x in the low 32 bits, y in the high (ee_scenario area.rs unpack)
int64_t Pack(long inX, long inY)
{
    return (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32);
}

// the unit class inA (NULL past the table)
EEUnitClass* Class(int32_t inA)
{
    return inA >= 0 && (unsigned long)inA < EEUnitClassCount() ? gUnitClassTable[inA] : NULL;
}

int64_t FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

int64_t Read(void* inContext, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, int32_t inD)
{
    EESMHostContext* theContext = static_cast<EESMHostContext*>(inContext);
    EESMCondition* theCondition = theContext->mCondition;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EEPlayer* thePlayer = Player(inA);
    switch (inOp) {
    case EE_SCENARIO_READ_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_SCENARIO_READ_PLAYER_EXISTS: return thePlayer != NULL;
    case EE_SCENARIO_READ_PLAYER_HUMAN: return thePlayer != NULL && thePlayer->mHumanPlayer != NULL;
    case EE_SCENARIO_READ_PLAYER_COMPUTER: return thePlayer != NULL && thePlayer->mComputerPlayer != NULL;
    case EE_SCENARIO_READ_RANDOM: return gPhysics->GetRandom((long)inA, (long)inB);
    case EE_SCENARIO_READ_ENEMY: return thePlayer->mEnemies[inB];
    case EE_SCENARIO_READ_RESOURCE: return thePlayer->mResourceCaps[inB];
    case EE_SCENARIO_READ_POPULATION: return thePlayer->GetTotalPopulation(false, false);
    case EE_SCENARIO_READ_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_SCENARIO_READ_UNIT_COUNT: return thePlayer->mUnitCount;
    case EE_SCENARIO_READ_COMPUTER_DIFFICULTY:
        return thePlayer->mComputerPlayer != NULL ? (int64_t)thePlayer->mComputerPlayer->mDifficulty : -1;
    case EE_SCENARIO_READ_VARIABLE: return thePlayer->mScenarioVariables[inB];
    case EE_SCENARIO_READ_TRIBUTE: return thePlayer->mTributes[inB];
    case EE_SCENARIO_READ_STATISTIC: return thePlayer->mSavedGroups.GetLastValue(inB);
    case EE_SCENARIO_READ_MINING_LIMIT: return thePlayer->mMiningLimit;
    case EE_SCENARIO_READ_EPOCH_COUNT: return EEEpochCount();
    case EE_SCENARIO_READ_EPOCH: return thePlayer->GetTechTree()->mCurrentEpoch;
    case EE_SCENARIO_READ_TECH_NODE: {
        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(inB);
        return theNode == NULL ? -1 : (theNode->mData->mMakesUnit ? 1 : 0) | (theNode->mIsResearched ? 2 : 0);
    }
    case EE_SCENARIO_READ_TECH_AVAILABLE: {
        EETechTreeNode* theNode = thePlayer->GetTechTree()->Get(inB);
        bool theResearched = theNode->mIsResearched;
        theNode->mIsResearched = false;
        bool theResult = theNode->GetIsAvailable();
        theNode->mIsResearched = theResearched;
        return theResult;
    }
    case EE_SCENARIO_READ_OBJECT: {
        EESMObject* theObject = theManager->GetScenarioObject((unsigned long)inA);
        return theObject == NULL ? -1 : theObject->Evaluate(0);
    }
    case EE_SCENARIO_READ_TRIGGER: {
        EESMTrigger* theTrigger = theManager->GetTrigger((unsigned long)inA);
        return theTrigger == NULL ? -1 : (theTrigger->mHasFired ? 1 : 0) | (theTrigger->mIsOn ? 2 : 0);
    }
    case EE_SCENARIO_READ_TIME: return FloatBits(theManager->mTime);
    case EE_SCENARIO_READ_TIME_OF_DAY: return FloatBits(gWorld.GetTimeOfDay());
    case EE_SCENARIO_READ_DIFFICULTY: return gDifficultyLevel;
    case EE_SCENARIO_READ_CHAT_CONTAINS: {
        UWideString theText(theCondition->mChatText);
        UWideString theChat(theManager->mLastChatText);
        PlatMbsLwr((unsigned char*)(const char*)theText);
        PlatMbsLwr((unsigned char*)(const char*)theChat);
        return theChat.Find(theText, 0) != theChat.GetLength();
    }
    case EE_SCENARIO_READ_CAPTURE_SEED: return EESimCaptureRecord::Active() ? (int64_t)EESMCaptureSeed() : -1;
    case EE_SCENARIO_READ_CONTINENT: return gContinentManager.GetContinentID(inA, inB);
    case EE_SCENARIO_READ_CONTINENT_CENTER: {
        U2DPoint<long> theTile;
        gContinentManager.GetContinentCenter((unsigned short)inA, theTile);
        return Pack(theTile.mX, theTile.mY);
    }
    case EE_SCENARIO_READ_TOWN_COUNT: return gWorld.GetPlayer(inA)->GetNumTowns();
    case EE_SCENARIO_READ_TOWN_CONTAINS: return gWorld.GetPlayer(inA)->mTowns[inB]->mHull.BoundedContains(U2DPoint<long>(inC, inD));
    case EE_SCENARIO_READ_TOWN_CENTER: {
        U2DPoint<long> theCenter = gWorld.GetPlayer(inA)->mTowns[inB]->mHull.GetCenter();
        return Pack(theCenter.mX, theCenter.mY);
    }
    case EE_SCENARIO_READ_UNIT_TILE: return Pack(theContext->mUnit->GetTileX(), theContext->mUnit->GetTileY());
    case EE_SCENARIO_READ_UNIT_HAS_COMPLEX: return theContext->mUnit->GetComplexUnit() != NULL;
    case EE_SCENARIO_READ_UNIT_PREVIOUS_TILE: {
        EEComplexUnit* theUnit = theContext->mUnit->GetComplexUnit();
        return Pack(theUnit->mU0DC.GetX(), theUnit->mU0DC.GetY());
    }
    case EE_SCENARIO_READ_CLASS_COUNT: return EEUnitClassCount();
    case EE_SCENARIO_READ_CLASS_EXISTS: return Class(inA) != NULL;
    case EE_SCENARIO_READ_CLASS_SELECTABLE: return Class(inA) != NULL && Class(inA)->mIsScenarioSelectable;
    case EE_SCENARIO_READ_CLASS_CATEGORY: return Class(inA)->mCategory;
    case EE_SCENARIO_READ_CLASS_EPOCH: return Class(inA)->mEpoch;
    case EE_SCENARIO_READ_CLASS_FAMILY: return Class(inA)->mFamily;
    case EE_SCENARIO_READ_CLASS_AI_KIND: return Class(inA)->mAIKind;
    case EE_SCENARIO_READ_CLASS_TYPE: return Class(inA)->GetUnitType();
    case EE_SCENARIO_READ_CATEGORY_IN_SET: return gUnitSetManager.Contains((long)inA, (unsigned long)inB);
    case EE_SCENARIO_READ_UNIT_SET_COUNT: return gUnitSetManager.GetSize();
    case EE_SCENARIO_READ_UNIT_SET_HAS: {
        EEUnitSet* theSet = gUnitSetManager.GetSet((unsigned long)inA);
        return theSet == NULL ? -1 : theSet->Contains((unsigned long)inB);
    }
    case EE_SCENARIO_READ_OBJECT_EXISTS: return theManager->GetScenarioObject((unsigned long)inA) != NULL;
    case EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP: return theManager->GetScenarioObject((unsigned long)inA)->GetIsSelectOnMap();
    case EE_SCENARIO_READ_OBJECT_FIRST_UNIT_TYPE: {
        std::vector<unsigned long> theUnits;
        theManager->GetScenarioObject((unsigned long)inA)->GetUnits(theUnits);
        EEUnit* theUnit = theUnits.size() > 0 ? gWorld.GetUnit(theUnits[0]) : NULL;
        return theUnit != NULL ? theUnit->mUnitClass->GetUnitType() : -1;
    }
    case EE_SCENARIO_READ_OBJECT_PICK: return theManager->GetScenarioObject((unsigned long)inA)->PickUnitType();
    case EE_SCENARIO_READ_CONDITION: {
        EESMCondition* theOther = theManager->GetCondition((unsigned long)inA);
        return theOther == NULL ? -1 : theOther->Evaluate();
    }
    }
    return 0;
}

// TEST ONLY: a player choice's SMCP record (ee/EEScenarioCapture.h)
void Players(void*, int32_t inKind, bool inSingle, int32_t inResult, bool inListed, const int32_t* inPlayers,
             size_t inCount, uint32_t inSeed)
{
    std::vector<long> thePlayers(inPlayers, inPlayers + inCount);
    EESMCapturePlayers(inKind, inSingle, inResult, inListed ? &thePlayers : NULL, inSeed);
}

}   // namespace

EeScenarioHost EESMHost(EESMHostContext* inContext)
{
    EeScenarioHost theHost = {inContext, Read, Players};
    return theHost;
}

// EESMObject's filter as the Rust record
EeScenarioObjectFilter EESMObjectFilter(const EESMObject& inObject)
{
    EeScenarioObjectFilter theFilter = {(uint32_t)inObject.mClass, (uint32_t)inObject.mFamily, (uint32_t)inObject.mParent};
    return theFilter;
}

// EESMArea's members as the Rust record
EeScenarioAreaRecord EESMAreaRecord(const EESMArea& inArea)
{
    EeScenarioAreaRecord theRecord = {(uint32_t)inArea.mSelectMode, (int32_t)inArea.mAnchor.mX, (int32_t)inArea.mAnchor.mY,
                                inArea.mRect.GetLeft(), inArea.mRect.GetTop(), inArea.mRect.GetRight(),
                                inArea.mRect.GetBottom(), (int32_t)inArea.mCenterTile.mX, (int32_t)inArea.mCenterTile.mY};
    return theRecord;
}

// EESMCondition's members as the Rust record
EeScenarioCondition EESMConditionRecord(const EESMCondition& inCondition)
{
    EeScenarioCondition theRecord;
    theRecord.kind = (uint32_t)inCondition.mType;
    theRecord.op = (uint32_t)inCondition.mOperator;
    theRecord.difficulty = (uint32_t)inCondition.mDifficulty;
    theRecord.diplomacy = (uint32_t)inCondition.mDiplomacy;
    theRecord.attribute = (uint32_t)inCondition.mAttribute;
    theRecord.other_attribute = (uint32_t)inCondition.mOtherAttribute;
    theRecord.technology = (int32_t)inCondition.mTechnology;
    theRecord.amount = (uint32_t)inCondition.mAmount;
    theRecord.object_index = (uint32_t)inCondition.mObjectIndex;
    theRecord.player = (uint32_t)inCondition.mPlayer;
    theRecord.other_player = (uint32_t)inCondition.mOtherPlayer;
    theRecord.min_time = (uint32_t)inCondition.mMinTime;
    theRecord.max_time = (uint32_t)inCondition.mMaxTime;
    theRecord.hour = (uint32_t)inCondition.mHour;
    theRecord.trigger = (uint32_t)inCondition.mTrigger;
    theRecord.is_trigger_on = inCondition.mIsTriggerOn;
    theRecord.has_trigger_fired = inCondition.mHasTriggerFired;
    theRecord.uses_difficulty = inCondition.mUsesDifficulty;
    theRecord.uses_elapsed_time = inCondition.mUsesElapsedTime;
    theRecord.uses_hour = inCondition.mUsesHour;
    theRecord.uses_player_attribute = inCondition.mUsesPlayerAttribute;
    return theRecord;
}
