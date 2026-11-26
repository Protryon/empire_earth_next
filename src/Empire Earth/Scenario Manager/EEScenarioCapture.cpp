// New (port), TEST ONLY: the scenario manager's capture records (ee/EEScenarioCapture.h; tools/sim_capture.py
// --profile scenario). The records are built in Rust (ee_sim scenario sm_capture.rs, moved whole from here in the
// scenario editor lane's round 3); this unit keeps the world's reads, one statement each.
// Reads only: nothing here changes the game.
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
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
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EECPTrace.h"
#include "ee/EEEnvironmentManager.h"
#include "rust/game/gamecpp.h"   // the camera's keys (camera_io.rs)

namespace {

EEScenarioManager* SM(const void* p) { return (EEScenarioManager*)p; }
EESMTrigger* ST(const void* p) { return (EESMTrigger*)p; }
EESMCondition* SC(const void* p) { return (EESMCondition*)p; }
EESMEffect* SE(const void* p) { return (EESMEffect*)p; }
EESMObject* SO(const void* p) { return (EESMObject*)p; }
EESMArea* SA(const void* p) { return (EESMArea*)p; }
EEUnit* SU(const void* p) { return (EEUnit*)p; }
EEComplexUnit* SX(const void* p) { return (EEComplexUnit*)p; }
EEPlayer* SP(const void* p) { return (EEPlayer*)p; }
EEComputerPlayer* SCP(const void* p) { return (EEComputerPlayer*)p; }
FSMemoryFile& SF(int64_t inFile) { return *(FSMemoryFile*)(intptr_t)inFile; }
const std::vector<long>* SV(const void* p) { return (const std::vector<long>*)p; }
int64_t P(const void* p) { return (int64_t)(intptr_t)p; }
unsigned long Major(int64_t inVersion) { return (unsigned long)(inVersion >> 32); }
unsigned long Minor(int64_t inVersion) { return (unsigned long)(uint32_t)inVersion; }

}   // namespace

// the records' reads (sm_capture.rs EE_SCENARIO_CAP_*): inP an object, inA and inB numbers
extern "C" int64_t ee_scenariocpp_capture(uint32_t inOp, const void* inP, int64_t inA, int64_t inB)
{
    switch (inOp) {
    case EE_SCENARIO_CAP_SEED: return gPhysics != NULL ? (unsigned int)gPhysics->mRandom.GetSeed() : 0u;
    case EE_SCENARIO_CAP_MANAGER: return P(gWorld.mScenarioManager);
    case EE_SCENARIO_CAP_TRIGGERS: return (int64_t)SM(inP)->mTriggers.size();
    case EE_SCENARIO_CAP_TRIGGER_AT: return P(SM(inP)->GetTrigger((unsigned long)inA));
    case EE_SCENARIO_CAP_CONDITIONS: return (int64_t)SM(inP)->mConditions.size();
    case EE_SCENARIO_CAP_CONDITION_AT: return P(SM(inP)->GetCondition((unsigned long)inA));
    case EE_SCENARIO_CAP_EFFECTS: return (int64_t)SM(inP)->mEffects.size();
    case EE_SCENARIO_CAP_EFFECT_AT: return P(SM(inP)->GetEffect((unsigned long)inA));
    case EE_SCENARIO_CAP_OBJECTS: return (int64_t)SM(inP)->mObjects.size();
    case EE_SCENARIO_CAP_OBJECT_AT: return P(SM(inP)->GetScenarioObject((unsigned long)inA));
    case EE_SCENARIO_CAP_AREAS: return (int64_t)SM(inP)->mAreas.size();
    case EE_SCENARIO_CAP_AREA_AT: return P(SM(inP)->GetArea((unsigned long)inA));
    case EE_SCENARIO_CAP_PENDING: return (int64_t)SM(inP)->mPendingEffects.size();
    case EE_SCENARIO_CAP_PENDING_AT: return P(SM(inP)->mPendingEffects[(size_t)inA]);
    case EE_SCENARIO_CAP_SAVE_TRIGGER: return ST(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_CONDITION: return SC(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_EFFECT: return SE(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_OBJECT: return SO(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_AREA: return SA(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_MANAGER: return SM(inP)->Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_SAVE_GROUPS: return SP(inP)->mSavedGroups.Save(Major(inB), Minor(inB), SF(inA));
    case EE_SCENARIO_CAP_FILE_NEW: return P(new FSMemoryFile((EeFsMemFile*)inP, false));
    case EE_SCENARIO_CAP_FILE_FREE: delete (FSMemoryFile*)inP;
        return 0;
    case EE_SCENARIO_CAP_LAST_CHAT: return P((const char*)SM(inP)->mLastChatText);
    case EE_SCENARIO_CAP_MEDIA_SECTION: return P((const char*)SM(inP)->mMediaStrings.GetSection((unsigned long)inA));
    case EE_SCENARIO_CAP_MESSAGES: return (int64_t)SM(inP)->mMessages.size();
    case EE_SCENARIO_CAP_TIMED: return P(SM(inP)->mTimedEffects);
    case EE_SCENARIO_CAP_CAMERA_KEYS: return (int64_t)ee_gamecpp_camera_key_count();
    case EE_SCENARIO_CAP_CAMERA_PATH: return (int64_t)gWorld.mCameraControl.mPathIndex;
    case EE_SCENARIO_CAP_CONDITION_CHAT: return P((const char*)SC(inP)->mChatText);
    case EE_SCENARIO_CAP_TRIGGER_CONDITIONS: return (int64_t)ST(inP)->mConditions.size();
    case EE_SCENARIO_CAP_TRIGGER_EFFECTS: return (int64_t)ST(inP)->mEffects.size();
    case EE_SCENARIO_CAP_OBJECT_UNITS: return (int64_t)SO(inP)->mUnits.size();
    case EE_SCENARIO_CAP_OBJECT_UNIT: return (int64_t)(uint32_t)SO(inP)->mUnits[(size_t)inA];
    case EE_SCENARIO_CAP_WEATHER: return gWorld.mEnvironmentManager != NULL && gWorld.mEnvironmentManager->mIsEnabled;
    case EE_SCENARIO_CAP_LAST_SUM: return (int64_t)gPhysics->mLastOutOfSynchChecksum;
    case EE_SCENARIO_CAP_SUM: return (int64_t)gPhysics->GetOutOfSynchChecksum(false, (unsigned long)inA);
    case EE_SCENARIO_CAP_SET_LAST_SUM: return (int64_t)(gPhysics->mLastOutOfSynchChecksum = (unsigned long)inA);
    case EE_SCENARIO_CAP_CP_SUM: return (int64_t)EECPOutOfSynchChecksum();
    case EE_SCENARIO_CAP_FIRST_UNIT: return P(static_cast<EEUnit*>(gWorld.mUnits.mFirst));
    case EE_SCENARIO_CAP_NEXT_UNIT: return P(static_cast<EEUnit*>(SU(inP)->mTileNode->mNext));
    case EE_SCENARIO_CAP_PLAYER_PENDING_GROUPS: return (int64_t)SP(inP)->GetNumPendingGroups();
    case EE_SCENARIO_CAP_PLAYER_RUNNING_GROUPS: return (int64_t)SP(inP)->GetNumRunningGroups();
    case EE_SCENARIO_CAP_PLAYER_CIV_AVAILABLE: return (int64_t)SP(inP)->mCivilization.mAvailablePoints;
    case EE_SCENARIO_CAP_PLAYER_CIV_MAX: return (int64_t)SP(inP)->mCivilization.mMaxPoints;
    case EE_SCENARIO_CAP_PLAYER_COMPUTER: return P(SP(inP)->mComputerPlayer);
    case EE_SCENARIO_CAP_COMPUTER_LAND: return (int64_t)SCP(inP)->mFavoriteLandType;
    case EE_SCENARIO_CAP_COMPUTER_SEA: return (int64_t)SCP(inP)->mFavoriteSeaType;
    case EE_SCENARIO_CAP_COMPUTER_AIR: return (int64_t)SCP(inP)->mFavoriteAirType;
    case EE_SCENARIO_CAP_COMPUTER_DIFFICULTY: return (int64_t)SCP(inP)->mDifficulty;
    case EE_SCENARIO_CAP_UNIT_NAME: return P((const char*)SU(inP)->GetStats()->mName);
    case EE_SCENARIO_CAP_UNIT_CHECKSUM: return (int64_t)SU(inP)->GetChecksum(3);
    case EE_SCENARIO_CAP_GOAL_KIND: return (int64_t)((EEUnitGoal*)inP)->mKind;
    case EE_SCENARIO_CAP_QUEUE_LEN: return (int64_t)SX(inP)->mGoalQueues[(size_t)inA].mGoals.size();
    case EE_SCENARIO_CAP_QUEUE_GOAL: return P(SX(inP)->mGoalQueues[(size_t)inA].mGoals[(size_t)inB]);
    case EE_SCENARIO_CAP_BEHAVIORS: return (int64_t)EEUnitBehavior::sTable.size();
    case EE_SCENARIO_CAP_BEHAVIOR_AT: return P(EEUnitBehavior::sTable[(size_t)inA]);
    case EE_SCENARIO_CAP_LONGS: return (int64_t)SV(inP)->size();
    case EE_SCENARIO_CAP_LONGS_AT: return (int64_t)(*SV(inP))[(size_t)inA];
    }
    return 0;
}

// the records' float reads
extern "C" double ee_scenariocpp_capture_f(uint32_t inOp, const void* inP)
{
    switch (inOp) {
    case EE_SCENARIO_CAP_TIME_OF_DAY: return gWorld.GetTimeOfDay();
    }
    return 0.0;
}
