// C:\EEX\Empire Earth\Units\EEUnitAction.cpp (file name inferred: no __FILE__; it sorts between EEUnit.cpp and
// EEUnitAI.cpp). The unit AI scripts, 0x22a509..0x22d95f. New (port, R5b.4 round 20): the scripts are Rust's
// (ee_units script.rs: the token reader and comment strip 0x22a69e/0x22a70f, both name tables' RegisterAll
// 0x22a8e5/0x22bd05, GetTriggerID/GetActionID and the names, the parser 0x22c8bd, AddTrigger 0x22bbae, the links
// 0x22b854/0x22c777/0x22c7a4, GetScript 0x22c7e0, DeleteScripts 0x22cdb6, both Evaluates 0x22b9a0/0x22cd8b in ai.rs);
// here the script file's reader, the static calls over them and the unit's AI host (ee_units ai.rs).
#include <windows.h>
#include <string.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include <map>
#include <string>
#include <vector>

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEPhysics.h"
#include "rust/sim/units.h"

extern FSPath* gDataPath;

// The script file's read (ee_units script.rs OpSource; EE_UNITS_SCRIPT_H_*): one statement of the original's each; its
// locals (the name, the file, the length and the count read) are these
static UString sScriptName;
static FSFileSpec* sScriptFile;
static unsigned long sScriptLength;
static unsigned long sScriptCount;

extern "C" int64_t ee_units_script_host(int32_t inOp, uint64_t inA, int64_t inB)
{
    switch (inOp) {
    case EE_UNITS_SCRIPT_H_NAME: sScriptName.Copy((const char*)std::string((const char*)(uintptr_t)inA, (size_t)inB).c_str(), 0, (unsigned long)inB, 0, true); return 0;
    case EE_UNITS_SCRIPT_H_FIND: return gDataPath->FindFile("Unit AI Scripts\\" + sScriptName + ".tai", sScriptFile = NULL);
    case EE_UNITS_SCRIPT_H_HAS_FILE: return sScriptFile != NULL;
    case EE_UNITS_SCRIPT_H_OPEN: return sScriptFile->Open(kFileAccessRead);
    case EE_UNITS_SCRIPT_H_GET_LENGTH: sScriptFile->GetLength(sScriptLength = 0); return 0;
    case EE_UNITS_SCRIPT_H_LENGTH: return (int64_t)sScriptLength;
    case EE_UNITS_SCRIPT_H_READ: sScriptFile->Read((char*)(uintptr_t)inA, (unsigned long)inB, sScriptCount = 0); return 0;
    case EE_UNITS_SCRIPT_H_CLOSE: sScriptFile->Close(); return 0;
    case EE_UNITS_SCRIPT_H_DELETE: delete sScriptFile; return 0;
    default: return 0;
    }
}

long EEUnitAction::GetActionID(const char* inName)
{
    return ee_units_script_action_id(inName);
}

const char* EEUnitAction::GetActionName(unsigned long inAction)
{
    return ee_units_script_action_name((uint32_t)inAction);
}

void* EEUnitAction::GetScript(const UString& inName)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ee_units_script_get((const uint8_t*)(const char*)inName, inName.GetLength())));   // script.rs
}

void EEUnitAction::DeleteScripts()
{
    ee_units_script_delete_all();
}

// IsActive's action (the gather test's local)
static unsigned long sAIAction;

// The C++ of EEComplexUnit::AIStep's Rust body (ee_units ai.rs; EE_UNITS_AI_H_*): one statement of the original's calls
// each; inA an object, inB a value
extern "C" int64_t ee_units_ai_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    switch (inOp) {
    case EE_UNITS_AI_H_COMMAND_TYPE: return theUnit->mCommand != NULL ? (int64_t)theUnit->mCommand->mType : 0;
    case EE_UNITS_AI_H_NEXT_TRIGGER_TIME: return (int64_t)theUnit->mNextTriggerTime;
    case EE_UNITS_AI_H_GOAL_KIND: return theUnit->mGoalState != NULL ? (int32_t)theUnit->mGoalState->mKind : -1;
    case EE_UNITS_AI_H_COMMAND: return (int64_t)reinterpret_cast<uintptr_t>(theUnit->mCommand);
    case EE_UNITS_AI_H_HAS_GOAL: return theUnit->mGoalState != NULL;
    case EE_UNITS_AI_H_GOAL_ACTION_RESET: sAIAction = 0; return 0;
    case EE_UNITS_AI_H_GOAL_ACTIVE: return theUnit->mGoalState->IsActive(&sAIAction);
    case EE_UNITS_AI_H_GOAL_ACTION: return (int64_t)sAIAction;
    case EE_UNITS_AI_H_CONDITION: return theUnit->mUnitClass->EvaluateCondition(theUnit, (long)inB);
    case EE_UNITS_AI_H_ACTION_TYPE: return reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>(inA))->mType;
    case EE_UNITS_AI_H_START_ACTION: theUnit->StartAction((long)inB); return 0;
    case EE_UNITS_AI_H_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_AI_H_DROP_FINISHED_GOALS: theUnit->DropFinishedGoals(false); return 0;
    case EE_UNITS_AI_H_UPDATE_CAMOUFLAGE: theUnit->UpdateCamouflage((long)inB); return 0;
    case EE_UNITS_AI_H_SET_COMMAND: theUnit->mCommand = reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>(inA)); return 0;
    case EE_UNITS_AI_H_SET_NEXT_TRIGGER_TIME: theUnit->mNextTriggerTime = (unsigned long)inB; return 0;
    case EE_UNITS_AI_H_UNIT_ID: return (int64_t)theUnit->mUnitID;
    case EE_UNITS_AI_H_IS_CITIZEN: return theUnit->mIsCitizen;
    case EE_UNITS_AI_H_GAME_TIME: return (int64_t)gPhysics->mGameTime;
    default: return 0;
    }
}
