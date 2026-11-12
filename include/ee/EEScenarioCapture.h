// New (port), TEST ONLY: the scenario manager's records of the simulation capture (ee/EESimCapture.h;
// tools/sim_capture.py --profile scenario; docs/RUST_PLAN.md R5b.5). With EE_SIM_CAPTURE set, the game appends:
//   SMCE  every EESMCondition::Evaluate: the condition's settings, the texts' hashes, the scenario clock, the physics
//         random seed before and after, the result
//   SMTE  every EESMTrigger::Evaluate: the trigger's id and state before, the time, the state after, the timed
//         effects' list after, the seeds
//   SMOB  every EESMObject::Evaluate: the object's index and settings, the dead flag, the units before (count, hash),
//         the result, the units and flags after, the seeds
//   SMCP  every EEScenarioManager::ChoosePlayer outside an object's Evaluate: the choice, the single flag, the result,
//         the players, the seeds
//   SMAR  the area queries outside an object's Evaluate (kind 0 ContainsUnit, 1 ContainsTile, 2 GetCenterTile,
//         3 GetTiles): the area's index and settings, the input, the result
//   SMEX  every EESMEffect::Execute: the effect's id, type and sub-kinds, the cinematic flag, the seeds, the game
//         result and the timed effects after
//   SMUP  each trigger round of EEScenarioManager::Update (every 500 ms): the clocks, the human flag, the triggers'
//         state and the timed effects after
// Off (EE_SIM_CAPTURE unset) each hook is one cached test.
#ifndef EE_EESCENARIOCAPTURE_H
#define EE_EESCENARIOCAPTURE_H

#include "ee/EESimCapture.h"

class EESMCondition;
class EESMTrigger;
class EESMObject;
class EESMArea;
class EESMEffect;
class EEScenarioManager;
class EEUnit;

// the physics random stream's seed (0 without physics)
unsigned int EESMCaptureSeed();
void EESMCaptureCondition(EESMCondition* inCondition, bool inResult, unsigned int inSeed);
// the trigger's state before: on, fired, start time
struct EESMCaptureTriggerState {
    bool mIsOn;
    bool mHasFired;
    unsigned long mStart;
};
EESMCaptureTriggerState EESMCaptureTriggerBefore(EESMTrigger* inTrigger);
void EESMCaptureTrigger(EESMTrigger* inTrigger, const EESMCaptureTriggerState& inBefore, float inTime, unsigned int inSeed);
// SMOB: Begin before the object's Evaluate (the units before; the depth that keeps SMCP/SMAR out), End after it
struct EESMCaptureObjectState {
    unsigned int mCount;
    unsigned long long mHash;
    unsigned int mSeed;
};
EESMCaptureObjectState EESMCaptureObjectBegin(EESMObject* inObject);
void EESMCaptureObjectEnd(EESMObject* inObject, bool inIncludeDead, bool inResult, const EESMCaptureObjectState& inBefore);
bool EESMCaptureInObject();               // inside an object's Evaluate on this thread
void EESMCapturePlayers(long inType, bool inSingle, long inResult, const void* inPlayers, unsigned int inSeed);
// kind 0 ContainsUnit (unit id), 1 ContainsTile (x, y), 2 GetCenterTile, 3 GetTiles; the result's x, y (or bool in x;
// GetTiles: count and hash)
void EESMCaptureArea(EESMArea* inArea, long inKind, long inA, long inB, long inX, long inY, unsigned long long inHash);
void EESMCaptureEffect(EESMEffect* inEffect, bool inIsCinematic, unsigned int inSeed);
void EESMCaptureUpdate(EEScenarioManager* inManager, bool inHumanPlaying);

#endif
