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

// New (R5b.4 round 20): the reader of a script's file for the Rust parser (ee_units script.rs): Unit AI
// Scripts\<name>.tai through the data path, its bytes pushed to the sink; false when it is missing or does not open
static bool EEReadScript(void*, const uint8_t* inName, size_t inLength, void* inSink)
{
    UString theName;
    theName.Copy((const char*)std::string((const char*)inName, inLength).c_str(), 0, (unsigned long)inLength, 0, true);
    FSFileSpec* theFile = NULL;
    bool theRead = false;
    long theResult = gDataPath->FindFile("Unit AI Scripts\\" + theName + ".tai", theFile);
    if (theResult == kErrorOK && theFile != NULL && theFile->Open(kFileAccessRead) == kErrorOK) {
        unsigned long theLength = 0;
        unsigned long theCount = 0;
        theFile->GetLength(theLength);
        std::vector<char> theBytes(theLength + 1);
        theFile->Read(&theBytes[0], theLength, theCount);
        ee_units_script_push(inSink, (const uint8_t*)&theBytes[0], theLength);
        theRead = true;
    }
    if (theFile != NULL) {
        theFile->Close();
        delete theFile;
    }
    return theRead;
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
    EeUnitsScriptReader theReader = {NULL, EEReadScript};
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        ee_units_script_get((const uint8_t*)(const char*)inName, inName.GetLength(), &theReader)));
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
