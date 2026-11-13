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
#include "rust/ee_units.h"

extern FSPath* gDataPath;                   // 0x531908

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

// the host calls (ee_units ai.rs EeUnitsAIHost); the context is the unit
static EEComplexUnit* AIUnit(void* inContext)
{
    return static_cast<EEComplexUnit*>(inContext);
}

static void AIMembers(void* inContext, EeUnitsAIState* outState)
{
    EEComplexUnit* theUnit = AIUnit(inContext);
    outState->command = theUnit->mCommand != NULL ? (int64_t)theUnit->mCommand->mType : 0;
    outState->next_trigger_time = (int64_t)theUnit->mNextTriggerTime.Get();
    outState->goal_kind = theUnit->mGoalState != NULL ? (int32_t)theUnit->mGoalState->mKind : -1;
}

static uint64_t AICommand(void* inContext)
{
    return reinterpret_cast<uintptr_t>(AIUnit(inContext)->mCommand.Get());
}

static bool AIGatherInactive(void* inContext, uint32_t* outAction)
{
    EEComplexUnit* theUnit = AIUnit(inContext);
    unsigned long theAction = 0;
    if (theUnit->mGoalState == NULL || theUnit->mGoalState->IsActive(&theAction))
        return false;
    *outAction = (uint32_t)theAction;
    return true;
}

static int64_t AICondition(void* inContext, int64_t inTrigger)
{
    EEComplexUnit* theUnit = AIUnit(inContext);
    long theEvent = theUnit->mUnitClass->EvaluateCondition(theUnit, (long)inTrigger);
    if (EEUnitStepNoting()) {   // TEST ONLY (USST)
        uint32_t theWords[2] = {(uint32_t)inTrigger, (uint32_t)theEvent};
        EEUnitStateNote(kUnitStateCondition, theUnit->mUnitID, theWords, 2);
    }
    return theEvent;
}

static uint32_t AIActionType(void* inContext, uint64_t inAction)
{
    return (uint32_t)reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>(inAction))->mType;
}

static void AIStartAction(void* inContext, uint32_t inAction)
{
    EEComplexUnit* theUnit = AIUnit(inContext);
    theUnit->StartAction(inAction);
    theUnit->GetUnitClass()->ActionChanged(theUnit);
}

static void AIDropFinishedGoals(void* inContext)
{
    AIUnit(inContext)->DropFinishedGoals(false);
}

static void AISetCommand(void* inContext, uint64_t inAction, uint32_t inType)
{
    EEComplexUnit* theUnit = AIUnit(inContext);
    theUnit->UpdateCamouflage(inType);
    theUnit->mCommand = reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>(inAction));
    theUnit->GetUnitClass()->ActionChanged(theUnit);
}

static void AISetNextTriggerTime(void* inContext, uint32_t inValue)
{
    AIUnit(inContext)->mNextTriggerTime = (unsigned long)inValue;
}

void EEUnitAIHostFor(EEComplexUnit* inUnit, EeUnitsAIHost& outHost)
{
    outHost.ctx = inUnit;
    outHost.members = AIMembers;
    outHost.command = AICommand;
    outHost.gather_inactive = AIGatherInactive;
    outHost.condition = AICondition;
    outHost.action_type = AIActionType;
    outHost.start_action = AIStartAction;
    outHost.drop_finished_goals = AIDropFinishedGoals;
    outHost.set_command = AISetCommand;
    outHost.set_next_trigger_time = AISetNextTriggerTime;
}
