// New (port): the scenario logic's host (ee_scenario, docs/RUST_PLAN.md R5b.5; ee/EEScenarioHost.h). The Rust
// conditions, player choices and triggers read the world through one callback by operation code
// (EE_SCENARIO_READ_*, rust/crates/ee_sim/src/scenario/host.rs); each case is the original's expression at its place.
// The players' and the unit classes' plain members and the context unit's fields are the world views' (ee_scenario
// unit_views.rs: they never reach this host).
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMArea.h"
#include "ee/EESMEffect.h"
#include "ee/EEUnit.h"
#include "lle/U2DSparseArray.h"
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
    case EE_SCENARIO_READ_CONTEXT_UNIT: return (int64_t)(uintptr_t)theContext->mUnit;
    case EE_SCENARIO_READ_WORLD: return (int64_t)(uintptr_t)&gWorld;
    case EE_SCENARIO_READ_PLAYER_HUMAN: return thePlayer != NULL && thePlayer->mHumanPlayer != NULL;
    case EE_SCENARIO_READ_PLAYER_COMPUTER: return thePlayer != NULL && thePlayer->mComputerPlayer != NULL;
    case EE_SCENARIO_READ_RANDOM: return gPhysics->GetRandom((long)inA, (long)inB);
    case EE_SCENARIO_READ_POPULATION: return thePlayer->GetTotalPopulation(false, false);
    case EE_SCENARIO_READ_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_SCENARIO_READ_COMPUTER_DIFFICULTY:
        return thePlayer->mComputerPlayer != NULL ? (int64_t)thePlayer->mComputerPlayer->mDifficulty : -1;
    case EE_SCENARIO_READ_STATISTIC: return thePlayer->mSavedGroups.GetLastValue(inB);
    case EE_SCENARIO_READ_EPOCH_COUNT: return EEEpochCount();
    case EE_SCENARIO_READ_EPOCH: return thePlayer->GetTechTree()->mCurrentEpoch;
    case EE_SCENARIO_READ_TIME: return FloatBits(theManager->mTime);
    case EE_SCENARIO_READ_TIME_OF_DAY: return FloatBits(gWorld.GetTimeOfDay());
    case EE_SCENARIO_READ_DIFFICULTY: return gDifficultyLevel;
    case EE_SCENARIO_READ_CAPTURE_SEED: return EESimCaptureRecord::Active() ? (int64_t)EESMCaptureSeed() : -1;
    case EE_SCENARIO_READ_CONTINENT: return gContinentManager.GetContinentID(inA, inB);
    case EE_SCENARIO_READ_TOWN_COUNT: return gWorld.GetPlayer(inA)->GetNumTowns();
    case EE_SCENARIO_READ_TOWN_CONTAINS: return gWorld.GetPlayer(inA)->mTowns[inB]->HullContains(U2DPoint<long>(inC, inD));
    case EE_SCENARIO_READ_CATEGORY_IN_SET: return gUnitSetManager.Contains((long)inA, (unsigned long)inB);
    case EE_SCENARIO_READ_UNIT_SET_COUNT: return gUnitSetManager.GetSize();
    case EE_SCENARIO_READ_OBJECT_EXISTS: return theManager->GetScenarioObject((unsigned long)inA) != NULL;
    case EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP: return theManager->GetScenarioObject((unsigned long)inA)->GetIsSelectOnMap();
    case EE_SCENARIO_READ_OBJECT_PICK: return theManager->GetScenarioObject((unsigned long)inA)->PickUnitType();
    case EE_SCENARIO_READ_WORLD_POPULATION_LIMIT: return gWorld.mPlayerPopulationLimit;
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
    case EE_SCENARIO_WRITE_DIFFICULTY: thePlayer->mComputerPlayer->mDifficulty = (long)inB; break;
    case EE_SCENARIO_WRITE_VARIABLE: thePlayer->mScenarioVariables[inB] = (unsigned long)inC; break;
    case EE_SCENARIO_WRITE_TRIBUTE: thePlayer->mTributes[inB] = (unsigned long)inC; break;
    case EE_SCENARIO_WRITE_ADD_TRIBUTE: thePlayer->AddTribute((long)inB, (long)inC, (long)inD); break;
    case EE_SCENARIO_WRITE_STATISTIC_ADD: thePlayer->mSavedGroups.Reset((long)inB, (long)inC); break;
    case EE_SCENARIO_WRITE_STATISTIC_SET: thePlayer->mSavedGroups.Set((long)inB, (long)inC); break;
    case EE_SCENARIO_WRITE_MINING_LIMIT: thePlayer->mMiningLimit = (long)inB; break;
    case EE_SCENARIO_WRITE_TILE: theContext->mTiles->push_back(U2DPoint<long>((long)inB, (long)inC)); break;
    default: EESMObjectWrite(theContext, inOp, inB); break;   // the object's list
    }
}

// the tile lists: every list is the Rust's (object_reads.rs tiles: ee_sim units' and the EE_SCENARIO_WZ_TILES_* statements)
void Tiles(void*, uint32_t, int32_t, int32_t, int32_t, EeScenarioTiles*) {}

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
    // (into the Rust's byte: a byte other than 0 or 1 is read as true there, stream.rs read_bool)
    case EE_SCENARIO_IO_READ_BOOL: return (int32_t)theFile.Read(*static_cast<bool*>(ioValue));
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
    case EE_SCENARIO_IO_TRIGGER: *static_cast<EESMTrigger**>(ioValue) = theStream->mTrigger;
        return 0;
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

// the reads' and the pending population write's single statements (ee_scenario read_compose.rs EE_SCENARIO_RX_*:
// the Rust composes the sequences), on handles; reached through the world channel (EESMEffectWorld)
std::vector<unsigned long> sSMReadIDs;   // GetUnits' list (the original's local)
template<class T> T* SMReadOf(int64_t inHandle) { return (T*)(intptr_t)inHandle; }
int64_t SMReadPtr(const void* inObject) { return (int64_t)(intptr_t)inObject; }
int64_t SMEReadWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    EEScenarioManager* m = gWorld.mScenarioManager;
    switch (inOp) {
    case EE_SCENARIO_RX_TECH_NODE: return SMReadPtr(Player((int32_t)a[0])->GetTechTree()->Get((long)a[1]));
    case EE_SCENARIO_RX_NODE_MAKES_UNIT: return SMReadOf<EETechTreeNode>(a[0])->mData->mMakesUnit;
    case EE_SCENARIO_RX_NODE_RESEARCHED: return SMReadOf<EETechTreeNode>(a[0])->mIsResearched;
    case EE_SCENARIO_RX_NODE_SET_RESEARCHED: return SMReadOf<EETechTreeNode>(a[0])->mIsResearched = a[1] != 0;
    case EE_SCENARIO_RX_NODE_AVAILABLE: return SMReadOf<EETechTreeNode>(a[0])->GetIsAvailable();
    case EE_SCENARIO_RX_OBJECT: return SMReadPtr(m->GetScenarioObject((unsigned long)a[0]));
    case EE_SCENARIO_RX_FIND_OBJECT: return SMReadPtr(m->FindObjectByID((long)a[0]));
    case EE_SCENARIO_RX_OBJECT_EVALUATE: return SMReadOf<EESMObject>(a[0])->Evaluate(0);
    case EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK: return SMReadOf<EESMObject>(a[0])->mHasAttributeCheck;
    case EE_SCENARIO_RX_OBJECT_UNIT_COUNT: return (int64_t)SMReadOf<EESMObject>(a[0])->mUnits.size();
    case EE_SCENARIO_RX_OBJECT_UNIT_AT: return (int64_t)SMReadOf<EESMObject>(a[0])->GetUnitAt((unsigned long)a[1]);
    case EE_SCENARIO_RX_OBJECT_ATTRIBUTE: return (int64_t)SMReadOf<EESMObject>(a[0])->GetAttribute();
    case EE_SCENARIO_RX_OBJECT_ATTRIBUTE_PERCENT: return SMReadOf<EESMObject>(a[0])->GetIsAttributePercent();
    case EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE:
        return SMReadOf<EESMObject>(a[0])->GetUnitAttribute((unsigned long)a[1], a[2] != 0, SMReadOf<EEUnit>(a[3]), 0, 0);
    case EE_SCENARIO_RX_OBJECT_GET_UNITS:
        SMReadOf<EESMObject>(a[0])->GetUnits(sSMReadIDs);
        return 0;
    case EE_SCENARIO_RX_IDS_CLEAR:
        sSMReadIDs.clear();
        return 0;
    case EE_SCENARIO_RX_IDS_COUNT: return (int64_t)sSMReadIDs.size();
    case EE_SCENARIO_RX_IDS_AT: return (int64_t)sSMReadIDs[(size_t)a[0]];
    case EE_SCENARIO_RX_UNIT: return SMReadPtr(gWorld.GetUnit((unsigned long)a[0]));
    case EE_SCENARIO_RX_UNIT_TYPE: return SMReadOf<EEUnit>(a[0])->mUnitClass->GetUnitType();
    case EE_SCENARIO_RX_TRIGGER: return SMReadPtr(m->GetTrigger((unsigned long)a[0]));
    case EE_SCENARIO_RX_TRIGGER_FIRED: return SMReadOf<EESMTrigger>(a[0])->mHasFired;
    case EE_SCENARIO_RX_TRIGGER_ON: return SMReadOf<EESMTrigger>(a[0])->mIsOn;
    case EE_SCENARIO_RX_UNIT_SET: return SMReadPtr(gUnitSetManager.GetSet((unsigned long)a[0]));
    case EE_SCENARIO_RX_SET_CONTAINS: return SMReadOf<EEUnitSet>(a[0])->Contains((unsigned long)a[1]);
    case EE_SCENARIO_RX_CONDITION: return SMReadPtr(m->GetCondition((unsigned long)a[0]));
    case EE_SCENARIO_RX_FIND_CONDITION: return SMReadPtr(m->FindConditionByID((long)a[0]));
    case EE_SCENARIO_RX_CONDITION_EVALUATE: return SMReadOf<EESMCondition>(a[0])->Evaluate();
    case EE_SCENARIO_RX_CONDITION_TYPE: return (int64_t)SMReadOf<EESMCondition>(a[0])->mType;
    case EE_SCENARIO_RX_CONDITION_PLAYER: return (int64_t)SMReadOf<EESMCondition>(a[0])->mPlayer;
    case EE_SCENARIO_RX_CONDITION_ATTRIBUTE: return (int64_t)SMReadOf<EESMCondition>(a[0])->mAttribute;
    case EE_SCENARIO_RX_EFFECT_COPY:
        SMReadOf<EESMEffect>(a[0])->Copy(*inContext->mEffect);
        return 0;
    }
    return 0;
}

// the object reads' statements on the continents and towns (object_reads.rs EE_SCENARIO_WZ_*; SMEObjectWorld's rest)
U2DPoint<long> sWZPoint;   // the centres' point (the original's local)
std::vector<U2DPoint<long> > sWZTiles;   // the host's tile list (the original's local)
int64_t SMEHostWorld(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_WZ_CONTINENT_CENTER: gContinentManager.GetContinentCenter((unsigned short)a[0], sWZPoint);
        return 0;
    case EE_SCENARIO_WZ_TOWN_CENTER: sWZPoint = gWorld.GetPlayer((unsigned long)a[0])->mTowns[(size_t)a[1]]->GetHullCenter();
        return 0;
    case EE_SCENARIO_WZ_TILES_CONTINENT: gContinentManager.GetContinentTiles((unsigned short)a[0], sWZTiles);
        return 0;
    case EE_SCENARIO_WZ_TILES_TOWN: gWorld.GetPlayer((unsigned long)a[0])->mTowns[(size_t)a[1]]->GetTiles(sWZTiles);
        return 0;
    }
    return 0;
}

EeScenarioStream EESMStreamTable(EESMStream* inStream)
{
    EeScenarioStream theTable = {inStream, StreamIO};
    return theTable;
}

// an object's numbers (NULL for none)
static const EeScenarioObjectFields* Object(void*, int32_t inIndex)
{
    return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inIndex);
}

EeScenarioHost EESMHost(EESMHostContext* inContext)
{
    EeScenarioHost theHost = {inContext, Read, Players, Write, Tiles, EESMEffectWorld, Object};
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
