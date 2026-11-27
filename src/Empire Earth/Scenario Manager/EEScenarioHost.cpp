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
#include "ee/EESaveGameFiles.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "rust/sim/app.h"
#include "rust/sim/scenario.h"

namespace {

// the world's player slot inA (NULL outside the array, as no original index reached)
EEPlayer* Player(int32_t inA) { return inA >= 0 && inA < (int32_t)(sizeof(gWorld.mPlayers) / sizeof(gWorld.mPlayers[0])) ? gWorld.mPlayers[inA] : NULL; }

}   // namespace

// the host's callbacks (ee_scenario host.rs EeScenarioHost; entry.rs builds the table in Rust)
extern "C" int64_t ee_scenariocpp_host_read(EESMHostContext* theContext, uint32_t inOp, int32_t inA, int32_t inB, int32_t inC, int32_t inD)
{
    EEPlayer* thePlayer = Player(inA);
    switch (inOp) {
    case EE_SCENARIO_READ_CONTEXT_UNIT: return (int64_t)(uintptr_t)theContext->mUnit;
    case EE_SCENARIO_READ_WORLD: return (int64_t)(uintptr_t)&gWorld;
    case EE_SCENARIO_READ_PLAYER_HUMAN: return thePlayer != NULL && thePlayer->mHumanPlayer != NULL;
    case EE_SCENARIO_READ_PLAYER_COMPUTER: return thePlayer != NULL && thePlayer->mComputerPlayer != NULL;
    case EE_SCENARIO_READ_RANDOM: return gPhysics->GetRandom((long)inA, (long)inB);
    case EE_SCENARIO_READ_POPULATION: return thePlayer->GetTotalPopulation(false, false);
    case EE_SCENARIO_READ_POPULATION_LIMIT: return thePlayer->GetPopulationLimit(0);
    case EE_SCENARIO_READ_COMPUTER_DIFFICULTY: return thePlayer->mComputerPlayer != NULL ? (int64_t)thePlayer->mComputerPlayer->mDifficulty : -1;
    case EE_SCENARIO_READ_STATISTIC: return thePlayer->mSavedGroups.GetLastValue(inB);
    case EE_SCENARIO_READ_EPOCH_COUNT: return EEEpochCount();
    case EE_SCENARIO_READ_TIME: return EESMBits(gWorld.mScenarioManager->mTime);
    case EE_SCENARIO_READ_TIME_OF_DAY: return EESMBits(gWorld.GetTimeOfDay());
    case EE_SCENARIO_READ_DIFFICULTY: return gDifficultyLevel;
    case EE_SCENARIO_READ_CAPTURE_SEED: return ee_app_capture_active() ? ee_scenariocpp_capture(EE_SCENARIO_CAP_SEED, NULL, 0, 0) : -1;
    case EE_SCENARIO_READ_CONTINENT: return gContinentManager.GetContinentID(inA, inB);
    case EE_SCENARIO_READ_TOWN_COUNT: return gWorld.GetPlayer(inA)->GetNumTowns();
    case EE_SCENARIO_READ_TOWN_CONTAINS: return gWorld.GetPlayer(inA)->GetTown(inB)->HullContains(U2DPoint<long>(inC, inD));
    case EE_SCENARIO_READ_CATEGORY_IN_SET: return gUnitSetManager.Contains((long)inA, (unsigned long)inB);
    case EE_SCENARIO_READ_UNIT_SET_COUNT: return gUnitSetManager.GetSize();
    case EE_SCENARIO_READ_OBJECT_EXISTS: return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA) != NULL;
    case EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP: return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA)->GetIsSelectOnMap();
    case EE_SCENARIO_READ_OBJECT_PICK: return ee_scenario_en_object_pick_unit_type(gWorld.mScenarioManager->GetScenarioObject((unsigned long)inA));
    case EE_SCENARIO_READ_WORLD_POPULATION_LIMIT: return gWorld.mPlayerPopulationLimit;
    }
    return EESMObjectRead(theContext, inOp, inA, inB, inC, inD);   // the objects' and units' reads
}

// a plan's write (ee_scenario player.rs): each the original's statement on player inPlayer
extern "C" void ee_scenariocpp_host_write(EESMHostContext* theContext, uint32_t inOp, long inPlayer, int64_t inB, int64_t inC, int64_t inD)
{
    EEPlayer* thePlayer = Player((int32_t)inPlayer);
    switch (inOp) {
    case EE_SCENARIO_WRITE_SET_ATTRIBUTE: theContext->mEffect->SetPlayerAttribute(inPlayer, (unsigned long)inB, (long)inC, (long)inD); break;
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
extern "C" void ee_scenariocpp_host_tiles(void*, uint32_t, int32_t, int32_t, int32_t, EeScenarioTiles*) {}

// TEST ONLY: a player choice's SMCP record (ee/EEScenarioCapture.h)
extern "C" void ee_scenariocpp_host_players(void*, int32_t inKind, bool inSingle, int32_t inResult, bool inListed, const int32_t* inPlayers,
             size_t inCount, uint32_t inSeed)
{
    std::vector<long> thePlayers(inPlayers, inPlayers + inCount);
    ee_scenario_cap_players(inKind, inSingle, inResult, inListed ? &thePlayers : NULL, inSeed);
}

// an element's save (ee_scenario stream.rs): each operation the original's FSFile call on the value the Rust hands
// over (the 4-byte long adapters, float, bool), or the element's string or id list by slot
extern "C" int32_t ee_scenariocpp_stream_io(EESMStream* theStream, uint32_t inOp, uint32_t inSlot, unsigned long inIndex, void* ioValue)
{
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
    }
    // the manager's lists, media strings and dialog messages (EEScenarioManager.cpp)
    return inOp >= EE_SCENARIO_IO_ELEMENT_SAVE ? EESMManagerIO(theStream, inOp, inSlot, inIndex, ioValue) : 0;
}

// the reads' and the pending population write's single statements (ee_scenario read_compose.rs EE_SCENARIO_RX_*:
// the Rust composes the sequences), on handles; reached through the world channel (entry.rs routes it here)
std::vector<unsigned long> sSMReadIDs;   // GetUnits' list (the original's local)
template<class T> T* SMReadOf(int64_t inHandle) { return (T*)(intptr_t)inHandle; }
int64_t SMReadPtr(const void* inObject) { return (int64_t)(intptr_t)inObject; }
extern "C" int64_t ee_scenariocpp_world_read(EESMHostContext* inContext, uint32_t inOp, const int64_t* a)
{
    switch (inOp) {
    case EE_SCENARIO_RX_OBJECT: return SMReadPtr(gWorld.mScenarioManager->GetScenarioObject((unsigned long)a[0]));
    case EE_SCENARIO_RX_FIND_OBJECT: return SMReadPtr(gWorld.mScenarioManager->FindObjectByID((long)a[0]));
    case EE_SCENARIO_RX_OBJECT_EVALUATE: return SMReadOf<EESMObject>(a[0])->Evaluate(0);
    case EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK: return SMReadOf<EESMObject>(a[0])->mHasAttributeCheck;
    case EE_SCENARIO_RX_OBJECT_UNIT_COUNT: return (int64_t)SMReadOf<EESMObject>(a[0])->mUnits.size();
    case EE_SCENARIO_RX_OBJECT_UNIT_AT: return (int64_t)SMReadOf<EESMObject>(a[0])->GetUnitAt((unsigned long)a[1]);
    case EE_SCENARIO_RX_OBJECT_ATTRIBUTE: return (int64_t)SMReadOf<EESMObject>(a[0])->GetAttribute();
    case EE_SCENARIO_RX_OBJECT_ATTRIBUTE_PERCENT: return SMReadOf<EESMObject>(a[0])->GetIsAttributePercent();
    case EE_SCENARIO_RX_OBJECT_UNIT_ATTRIBUTE:
        return SMReadOf<EESMObject>(a[0])->GetUnitAttribute((unsigned long)a[1], a[2] != 0, SMReadOf<EEUnit>(a[3]), 0, 0);
    case EE_SCENARIO_RX_OBJECT_GET_UNITS: SMReadOf<EESMObject>(a[0])->GetUnits(sSMReadIDs); return 0;
    case EE_SCENARIO_RX_IDS_CLEAR: sSMReadIDs.clear(); return 0;
    case EE_SCENARIO_RX_IDS_COUNT: return (int64_t)sSMReadIDs.size();
    case EE_SCENARIO_RX_IDS_AT: return (int64_t)sSMReadIDs[(size_t)a[0]];
    case EE_SCENARIO_RX_UNIT: return SMReadPtr(gWorld.GetUnit((unsigned long)a[0]));
    case EE_SCENARIO_RX_UNIT_TYPE: return SMReadOf<EEUnit>(a[0])->mUnitClass->GetUnitType();
    case EE_SCENARIO_RX_TRIGGER: return SMReadPtr(gWorld.mScenarioManager->GetTrigger((unsigned long)a[0]));
    case EE_SCENARIO_RX_TRIGGER_FIRED: return SMReadOf<EESMTrigger>(a[0])->mHasFired;
    case EE_SCENARIO_RX_TRIGGER_ON: return SMReadOf<EESMTrigger>(a[0])->mIsOn;
    case EE_SCENARIO_RX_UNIT_SET: return SMReadPtr(gUnitSetManager.GetSet((unsigned long)a[0]));
    case EE_SCENARIO_RX_SET_CONTAINS: return SMReadOf<EEUnitSet>(a[0])->Contains((unsigned long)a[1]);
    case EE_SCENARIO_RX_CONDITION: return SMReadPtr(gWorld.mScenarioManager->GetCondition((unsigned long)a[0]));
    case EE_SCENARIO_RX_FIND_CONDITION: return SMReadPtr(gWorld.mScenarioManager->FindConditionByID((long)a[0]));
    case EE_SCENARIO_RX_CONDITION_EVALUATE: return SMReadOf<EESMCondition>(a[0])->Evaluate();
    case EE_SCENARIO_RX_CONDITION_TYPE: return (int64_t)SMReadOf<EESMCondition>(a[0])->mType;
    case EE_SCENARIO_RX_CONDITION_PLAYER: return (int64_t)SMReadOf<EESMCondition>(a[0])->mPlayer;
    case EE_SCENARIO_RX_CONDITION_ATTRIBUTE: return (int64_t)SMReadOf<EESMCondition>(a[0])->mAttribute;
    case EE_SCENARIO_RX_EFFECT_COPY: SMReadOf<EESMEffect>(a[0])->Copy(*inContext->mEffect); return 0;
    }
    return 0;
}

// an object's numbers (NULL for none)
extern "C" const EeScenarioObjectFields* ee_scenariocpp_host_object(void*, int32_t inIndex)
{
    return gWorld.mScenarioManager->GetScenarioObject((unsigned long)inIndex);
}

// The members and single statements of the scenario elements' methods whole in Rust (ee_scenario entry.rs
// EE_SCENARIO_EN_*): p the object, a a string's slot or a number, b a number
static_assert(sizeof(EESMHostContext) == sizeof(EeScenarioHostContext) && sizeof(EESMStream) == sizeof(EeScenarioStreamContext),
              "entry.rs's context records");
extern UWideString EESMEffect::* const kSMETexts[EE_SCENARIO_EC_TEXTS];   // EESMEffect.cpp
template<class T> T* EN(void* p) { return static_cast<T*>(p); }
int64_t ENP(const void* p) { return (int64_t)(intptr_t)p; }
extern "C" int64_t ee_scenariocpp_entry(uint32_t inOp, void* p, int64_t a, int64_t b)
{
    switch (inOp) {
    case EE_SCENARIO_EN_EFFECT_FIELDS: return ENP(static_cast<EeScenarioEffectFields*>(EN<EESMEffect>(p)));
    case EE_SCENARIO_EN_EFFECT_STRING: return ENP(&(EN<EESMEffect>(p)->*kSMETexts[a]));
    case EE_SCENARIO_EN_EFFECT_OBJECTS: return ENP(&EN<EESMEffect>(p)->mObjects);
    case EE_SCENARIO_EN_CONDITION_FIELDS: return ENP(static_cast<EeScenarioConditionFields*>(EN<EESMCondition>(p)));
    case EE_SCENARIO_EN_CONDITION_STRING: return ENP(a == 0 ? &EN<EESMCondition>(p)->mName : a == 1 ? &EN<EESMCondition>(p)->mText : &EN<EESMCondition>(p)->mChatText);
    case EE_SCENARIO_EN_OBJECT_FIELDS: return ENP(static_cast<EeScenarioObjectFields*>(EN<EESMObject>(p)));
    case EE_SCENARIO_EN_OBJECT_NAME: return ENP(&EN<EESMObject>(p)->mName);
    case EE_SCENARIO_EN_OBJECT_UNITS: return ENP(&EN<EESMObject>(p)->mUnits);
    case EE_SCENARIO_EN_TRIGGER_FIELDS: return ENP(static_cast<EeScenarioTriggerFields*>(EN<EESMTrigger>(p)));
    case EE_SCENARIO_EN_TRIGGER_STRING: return ENP(a == 0 ? &EN<EESMTrigger>(p)->mName : &EN<EESMTrigger>(p)->mText);
    case EE_SCENARIO_EN_MEDIA_STRING: return ENP(EN<EESMMediaStrings>(p)->GetString((unsigned long)a));
    case EE_SCENARIO_EN_MANAGER_FIELDS: return ENP(static_cast<EeScenarioManagerFields*>(EN<EEScenarioManager>(p)));
    case EE_SCENARIO_EN_MANAGER_TIMED: return ENP(EN<EEScenarioManager>(p)->mTimedEffects);
    case EE_SCENARIO_EN_MANAGER_STRING: return ENP(a == 0 ? &EN<EEScenarioManager>(p)->mExportFileName : &EN<EEScenarioManager>(p)->mLastChatText);
    case EE_SCENARIO_EN_MANAGER_MEDIA: return ENP(&EN<EEScenarioManager>(p)->mMediaStrings);
    case EE_SCENARIO_EN_MESSAGE_FIELDS: return ENP(static_cast<EeScenarioMessageFields*>(EN<EESMDialogMessage>(p)));
    case EE_SCENARIO_EN_MESSAGE_STRING: return ENP(a == 0 ? &EN<EESMDialogMessage>(p)->mText : &EN<EESMDialogMessage>(p)->mSound);
    case EE_SCENARIO_EN_AREA_FIELDS: return ENP(static_cast<EeScenarioAreaFields*>(EN<EESMArea>(p)));
    case EE_SCENARIO_EN_AREA_STRING: return ENP(a == 0 ? &EN<EESMArea>(p)->mName : &EN<EESMArea>(p)->mText);
    case EE_SCENARIO_EN_AREA_TILES: return ENP(&EN<EESMArea>(p)->mTiles);
    case EE_SCENARIO_EN_MANAGER: return ENP(gWorld.mScenarioManager);
    case EE_SCENARIO_EN_MANAGER_RESET: EN<EEScenarioManager>(p)->Reset();
        return 0;
    case EE_SCENARIO_EN_SAVE_VERSION: return ((int64_t)kSaveGameMajorVersion << 32) | kSaveGameMinorVersion;
    case EE_SCENARIO_EN_UNIT_ID: return (int64_t)EN<EEUnit>(p)->mUnitID;
    case EE_SCENARIO_EN_UNIT_BY_ID: return ENP(gWorld.GetUnit((unsigned long)a));
    case EE_SCENARIO_EN_AREA_AT: return ENP(gWorld.mScenarioManager->GetArea((unsigned long)a));
    case EE_SCENARIO_EN_AREA_COUNT: return (int64_t)EN<EEScenarioManager>(p)->mAreas.size();
    case EE_SCENARIO_EN_AREA_INDEX: return ENP(EN<EEScenarioManager>(p)->mAreas[(size_t)a]);
    case EE_SCENARIO_EN_STRING_TEXT: return ENP((const char*)*EN<UWideString>(p));
    case EE_SCENARIO_EN_STRING_SET_TEXT: *EN<UWideString>(p) = (const char*)(intptr_t)a;
        return 0;
    case EE_SCENARIO_EN_STRING_ASSIGN: *EN<UWideString>(p) = *(const UWideString*)(intptr_t)a;
        return 0;
    case EE_SCENARIO_EN_UNITS_COUNT: return (int64_t)EN<std::vector<unsigned long> >(p)->size();
    case EE_SCENARIO_EN_UNITS_AT: return (int64_t)(*EN<std::vector<unsigned long> >(p))[(size_t)a];
    case EE_SCENARIO_EN_UNITS_SET: (*EN<std::vector<unsigned long> >(p))[(size_t)a] = (unsigned long)b;
        return 0;
    case EE_SCENARIO_EN_UNITS_ASSIGN: *EN<std::vector<unsigned long> >(p) = *(const std::vector<unsigned long>*)(intptr_t)a;
        return 0;
    case EE_SCENARIO_EN_PHYSICS_TICK: return (int64_t)gPhysics->mCurrentTick;
    case EE_SCENARIO_EN_TILES_COUNT: return (int64_t)EN<std::vector<U2DPoint<long> > >(p)->size();
    case EE_SCENARIO_EN_TILES_CLEAR: EN<std::vector<U2DPoint<long> > >(p)->clear();
        return 0;
    case EE_SCENARIO_EN_TILES_COPY_TO: *(std::vector<U2DPoint<long> >*)(intptr_t)a = *EN<std::vector<U2DPoint<long> > >(p);
        return 0;
    case EE_SCENARIO_EN_TILES_DATA: return ENP(EN<std::vector<U2DPoint<long> > >(p)->data());
    case EE_SCENARIO_EN_PENDING_CLEAR: EN<EEScenarioManager>(p)->mPendingEffects.clear();
        return 0;
    case EE_SCENARIO_EN_CAPTURE_SEED: return ee_scenariocpp_capture(EE_SCENARIO_CAP_SEED, NULL, 0, 0);
    case EE_SCENARIO_EN_DATA_FIND: return gDataPath->FindFile(UWideString((const char*)(intptr_t)a));
    case EE_SCENARIO_EN_CAMPAIGN_PREFIX: return ENP((const char*)gU52A280);
    case EE_SCENARIO_EN_STRINGS_PUSH_TEXT: EN<std::vector<UWideString> >(p)->push_back(UWideString((const char*)(intptr_t)a));
        return 0;
    case EE_SCENARIO_EN_STRING_APPEND_TEXT: *EN<UWideString>(p) += (const char*)(intptr_t)a;
        return 0;
    case EE_SCENARIO_EN_VERIFY: ESVerify((const char*)p, (unsigned long)a, (const char*)(intptr_t)b);
        return 0;
    }
    return 0;
}
