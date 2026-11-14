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
#include "ee/EESMEffect.h"
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
    case EE_SCENARIO_READ_SHARE_BONUS: return thePlayer->mShareBonus;
    case EE_SCENARIO_READ_WORLD_POPULATION_LIMIT: return gWorld.mPlayerPopulationLimit;
    case EE_SCENARIO_READ_PLAYER_ID: return thePlayer->mPlayerID;
    case EE_SCENARIO_READ_UNIT_FIELD: return EESMUnitField(theContext->mUnit, inA);
    case EE_SCENARIO_READ_TOKEN_OBJECT: {
        EESMObject* theObject = theManager->FindObjectByID(inA);
        if (theObject == NULL || (inB != 0 && theObject->mHasAttributeCheck == 0))
            return -1;
        theObject->Evaluate(0);
        return (int64_t)theObject->mUnits.size();
    }
    case EE_SCENARIO_READ_TOKEN_VALUE: {
        EESMObject* theObject = theManager->FindObjectByID(inA);
        return theObject->GetUnitAttribute(theObject->GetAttribute(), theObject->GetIsAttributePercent(),
                                           gWorld.GetUnit(theObject->GetUnitAt(0)), 0, 0);
    }
    case EE_SCENARIO_READ_TOKEN_CONDITION: {
        EESMCondition* theToken = theManager->FindConditionByID(inA);
        if (theToken == NULL || theToken->mType != kSMConditionPlayerAttribute)
            return -1;
        return inB == 0 ? (int64_t)theToken->mPlayer : (int64_t)theToken->mAttribute;
    }
    case EE_SCENARIO_READ_CONDITION: {
        EESMCondition* theOther = theManager->GetCondition((unsigned long)inA);
        return theOther == NULL ? -1 : theOther->Evaluate();
    }
    }
    return EESMObjectRead(theContext, inOp, inA, inB, inC, inD);   // the objects' and units' reads
}

// a plan's write (ee_scenario player.rs): each the original's statement on player inPlayer
void Write(void* inContext, uint32_t inOp, long inPlayer, int64_t inB, int64_t inC, int64_t inD)
{
    EESMHostContext* theContext = static_cast<EESMHostContext*>(inContext);
    EEPlayer* thePlayer = Player((int32_t)inPlayer);
    switch (inOp) {
    case EE_SCENARIO_WRITE_SET_ATTRIBUTE:
        theContext->mEffect->SetPlayerAttribute(inPlayer, (unsigned long)inB, (long)inC, (long)inD);
        break;
    case EE_SCENARIO_WRITE_RESOURCE: thePlayer->mResourceCaps[inB] = (long)inC; break;
    case EE_SCENARIO_WRITE_SHARE_BONUS: thePlayer->mShareBonus = (long)inB; break;
    case EE_SCENARIO_WRITE_PENDING_POPULATION: {
        EESMEffect theEffect;
        theEffect.Copy(*theContext->mEffect);
        theEffect.mPlayer = (unsigned long)inB;
        theEffect.mPlayerAttributeMode = 0;
        theEffect.mPlayerAttributeAmount = (unsigned long)inC;
        gWorld.mScenarioManager->AddV134Effect(theEffect);
        break;
    }
    case EE_SCENARIO_WRITE_DIFFICULTY: thePlayer->mComputerPlayer->mDifficulty = (long)inB; break;
    case EE_SCENARIO_WRITE_VARIABLE: thePlayer->mScenarioVariables[inB] = (unsigned long)inC; break;
    case EE_SCENARIO_WRITE_TRIBUTE: thePlayer->mTributes[inB] = (unsigned long)inC; break;
    case EE_SCENARIO_WRITE_ADD_TRIBUTE: thePlayer->AddTribute((long)inB, (long)inC, (long)inD); break;
    case EE_SCENARIO_WRITE_STATISTIC_ADD: thePlayer->mSavedGroups.Reset((long)inB, (long)inC); break;
    case EE_SCENARIO_WRITE_STATISTIC_SET: thePlayer->mSavedGroups.Set((long)inB, (long)inC); break;
    case EE_SCENARIO_WRITE_MINING_LIMIT: thePlayer->mMiningLimit = (long)inB; break;
    case EE_SCENARIO_WRITE_CREATE_UNIT: EESMCreateUnit(theContext, inPlayer, (long)inB, inC != 0, (unsigned long)inD); break;
    case EE_SCENARIO_WRITE_TILE: theContext->mTiles->push_back(U2DPoint<long>((long)inB, (long)inC)); break;
    default: EESMObjectWrite(theContext, inOp, inB); break;   // the object's list
    }
}

// a tile list (ee_scenario placement.rs EE_SCENARIO_TILES_*), handed over tile by tile
void Tiles(void*, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, EeScenarioTiles* outList)
{
    std::vector<U2DPoint<long> > theTiles;
    switch (inOp) {
    case EE_SCENARIO_TILES_AREA: gWorld.mScenarioManager->GetArea((unsigned long)inA)->GetTiles(theTiles); break;
    case EE_SCENARIO_TILES_CONTINENT: gContinentManager.GetContinentTiles((unsigned short)inA, theTiles); break;
    case EE_SCENARIO_TILES_TOWN: gWorld.GetPlayer(inA)->mTowns[inB]->GetTiles(theTiles); break;
    default: EESMObjectTiles(inOp, inA, inB, inC, theTiles); break;
    }
    for (const U2DPoint<long>& theTile : theTiles)
        ee_scenario_tiles_push(outList, (int32_t)theTile.mX, (int32_t)theTile.mY);
}

// TEST ONLY: a player choice's SMCP record (ee/EEScenarioCapture.h)
void Players(void*, int32_t inKind, bool inSingle, int32_t inResult, bool inListed, const int32_t* inPlayers,
             size_t inCount, uint32_t inSeed)
{
    std::vector<long> thePlayers(inPlayers, inPlayers + inCount);
    EESMCapturePlayers(inKind, inSingle, inResult, inListed ? &thePlayers : NULL, inSeed);
}

// an element's save (ee_scenario stream.rs): each operation the original's FSFile call on the value the Rust hands
// over (the 4-byte long adapters, float, bool), or the element's string or id list by slot
int32_t StreamIO(void* inContext, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue)
{
    EESMStream* theStream = static_cast<EESMStream*>(inContext);
    FSFile& theFile = *theStream->mFile;
    switch (inOp) {
    case EE_SCENARIO_IO_READ_ULONG: return (int32_t)theFile.Read(*static_cast<unsigned long*>(ioValue));
    case EE_SCENARIO_IO_READ_LONG: return (int32_t)theFile.Read(*static_cast<long*>(ioValue));
    case EE_SCENARIO_IO_READ_FLOAT: return (int32_t)theFile.Read(*static_cast<float*>(ioValue));
    case EE_SCENARIO_IO_READ_BOOL: {
        // a byte other than 0 or 1 is read as true (the Rust field is a bool)
        bool theValue = *static_cast<bool*>(ioValue);
        long theResult = theFile.Read(theValue);
        *static_cast<bool*>(ioValue) = *reinterpret_cast<unsigned char*>(&theValue) != 0;
        return (int32_t)theResult;
    }
    case EE_SCENARIO_IO_WRITE_ULONG: return (int32_t)theFile.Write(*static_cast<const unsigned long*>(ioValue));
    case EE_SCENARIO_IO_WRITE_LONG: return (int32_t)theFile.Write(*static_cast<const long*>(ioValue));
    case EE_SCENARIO_IO_WRITE_FLOAT: return (int32_t)theFile.Write(*static_cast<const float*>(ioValue));
    case EE_SCENARIO_IO_WRITE_BOOL: return (int32_t)theFile.Write(*static_cast<const bool*>(ioValue));
    case EE_SCENARIO_IO_LOAD_STRING: return (int32_t)theStream->mStrings[inSlot]->Load(theFile);
    case EE_SCENARIO_IO_SAVE_STRING: return (int32_t)theStream->mStrings[inSlot]->Save(theFile);
    case EE_SCENARIO_IO_LIST_CLEAR: theStream->mList->clear(); return 0;
    case EE_SCENARIO_IO_LIST_PUSH: theStream->mList->push_back(*static_cast<unsigned long*>(ioValue)); return 0;
    case EE_SCENARIO_IO_LIST_COUNT: *static_cast<unsigned long*>(ioValue) = theStream->mList->size(); return 0;
    case EE_SCENARIO_IO_LIST_AT: *static_cast<unsigned long*>(ioValue) = (*theStream->mList)[inIndex]; return 0;
    case EE_SCENARIO_IO_ENTRY_COUNT:
        *static_cast<unsigned long*>(ioValue) = inSlot == EE_SCENARIO_ENTRIES_CONDITIONS
                                                    ? theStream->mTrigger->GetConditionCount()
                                                    : theStream->mTrigger->GetEffectCount();
        return 0;
    case EE_SCENARIO_IO_ENTRY_AT: {
        EeScenarioEntry& theEntry = *static_cast<EeScenarioEntry*>(ioValue);
        if (inSlot == EE_SCENARIO_ENTRIES_CONDITIONS) {
            EESMTConditionWrapper* theWrapper = theStream->mTrigger->GetConditionAt(inIndex);
            theEntry = {(unsigned long)theWrapper->mIsNot, theWrapper->mOperator, theWrapper->mValue};
        } else {
            EESMTEffectWrapper* theWrapper = theStream->mTrigger->GetEffectAt(inIndex);
            theEntry = {theWrapper->mDelay, theWrapper->mOperator, theWrapper->mEffectIndex};
        }
        return 0;
    }
    case EE_SCENARIO_IO_ENTRY_SET: {
        const EeScenarioEntry& theEntry = *static_cast<const EeScenarioEntry*>(ioValue);
        if (inSlot == EE_SCENARIO_ENTRIES_CONDITIONS) {
            theStream->mTrigger->SetCondition(inIndex, theEntry.first != 0, theEntry.value);
            theStream->mTrigger->SetConditionOperator(inIndex, theEntry.op);
        } else {
            theStream->mTrigger->SetEffect(inIndex, (long)theEntry.first, theEntry.value);
            theStream->mTrigger->SetEffectOperator(inIndex, theEntry.op);
        }
        return 0;
    }
    case EE_SCENARIO_IO_ELEMENT_LOAD:
    case EE_SCENARIO_IO_ELEMENT_SAVE:
    case EE_SCENARIO_IO_ELEMENT_COUNT:
    case EE_SCENARIO_IO_MEDIA:
    case EE_SCENARIO_IO_MESSAGES:
        return EESMManagerIO(theStream, inOp, inSlot, inIndex, ioValue);
    }
    return 0;
}

}   // namespace

EeScenarioStream EESMStreamTable(EESMStream* inStream)
{
    EeScenarioStream theTable = {inStream, StreamIO};
    return theTable;
}

EeScenarioHost EESMHost(EESMHostContext* inContext)
{
    EeScenarioHost theHost = {inContext, Read, Players, Write, Tiles};
    return theHost;
}

// EESMArea's members as the Rust record
EeScenarioAreaRecord EESMAreaRecord(const EESMArea& inArea)
{
    EeScenarioAreaRecord theRecord = {(uint32_t)inArea.mSelectMode, (int32_t)inArea.mAnchor.mX, (int32_t)inArea.mAnchor.mY,
                                inArea.mRect.GetLeft(), inArea.mRect.GetTop(), inArea.mRect.GetRight(),
                                inArea.mRect.GetBottom(), (int32_t)inArea.mCenterTile.mX, (int32_t)inArea.mCenterTile.mY};
    return theRecord;
}
