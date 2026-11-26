// C:\EEX\Empire Earth\Units\EEUnitAction.cpp (file name inferred: no __FILE__; it sorts between EEUnit.cpp and
// EEUnitAI.cpp). The unit AI scripts, 0x22a509..0x22d95f. New (port, R5b.4 round 20): the scripts are Rust's
// (ee_units script.rs: the token reader and comment strip 0x22a69e/0x22a70f, both name tables' RegisterAll
// 0x22a8e5/0x22bd05, GetTriggerID/GetActionID and the names, the parser 0x22c8bd, AddTrigger 0x22bbae, the links
// 0x22b854/0x22c777/0x22c7a4, GetScript 0x22c7e0, DeleteScripts 0x22cdb6, both Evaluates 0x22b9a0/0x22cd8b in ai.rs);
// here the script file's reader, the static calls over them and the unit's AI host (ee_units ai.rs).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include <string>

#include "ee/EEUnit.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
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

void* EEUnitAction::GetScript(const UString& inName)
{
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ee_units_script_get((const uint8_t*)(const char*)inName, inName.GetLength())));   // script.rs
}

// IsActive's action (the gather test's local)
static unsigned long sAIAction;

// The C++ of EEComplexUnit::AIStep's Rust body (ee_units ai.rs; EE_UNITS_AI_H_*): one statement of the original's calls
// each; inA an object, inB a value
extern "C" int64_t ee_units_ai_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB)
{
    switch (inOp) {
    case EE_UNITS_AI_H_GOAL_ACTION_RESET: sAIAction = 0; return 0;
    case EE_UNITS_AI_H_GOAL_ACTIVE: return (static_cast<EEComplexUnit*>(inUnit))->mGoalState->IsActive(&sAIAction);
    case EE_UNITS_AI_H_GOAL_ACTION: return (int64_t)sAIAction;
    case EE_UNITS_AI_H_CONDITION: return (static_cast<EEComplexUnit*>(inUnit))->mUnitClass->EvaluateCondition((static_cast<EEComplexUnit*>(inUnit)), (long)inB);
    case EE_UNITS_AI_H_START_ACTION: (static_cast<EEComplexUnit*>(inUnit))->StartAction((long)inB); return 0;
    case EE_UNITS_AI_H_ACTION_CHANGED: (static_cast<EEComplexUnit*>(inUnit))->GetUnitClass()->ActionChanged((static_cast<EEComplexUnit*>(inUnit))); return 0;
    case EE_UNITS_AI_H_DROP_FINISHED_GOALS: (static_cast<EEComplexUnit*>(inUnit))->DropFinishedGoals(false); return 0;
    case EE_UNITS_AI_H_UPDATE_CAMOUFLAGE: (static_cast<EEComplexUnit*>(inUnit))->UpdateCamouflage((long)inB); return 0;
    default: return 0;
    }
}
