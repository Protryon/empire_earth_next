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
//   SMEW  after every SMEX (the effect's world): the effect's saved bytes' FNV-1a, the world's out-of-synch checksum,
//         the units' checksum sum and the computer players' (as EE-SIM's --checksum-trace), every unit of the five
//         objects the effect names (checksum, owner, hit points, tile, name, command, goals, flags, behaviour,
//         container, flight time, ammunition, cargo, location), each player's result, visibility, enemies, groups'
//         counts, civilization and favourite units, the media texts, messages, pending effects, triggers, time of
//         day, game speed, cinematic and weather flags and the camera path
//   SMUP  each trigger round of EEScenarioManager::Update (every 500 ms): the clocks, the human flag, the triggers'
//         state and the timed effects after
//   SMSV  the manager's saved bytes (kind 0: after EEScenarioManager::Load, with its result; kind 1: every 120th
//         trigger round): each list's count, the first error and the FNV-1a of its elements' Saves into a memory
//         file (triggers, conditions, effects, objects, areas, pending effects, the built-ins included), then the
//         whole manager's Save
//   SMPA  every EESMEffect::SetPlayerAttribute (the any/all resources' recursions too): the player, attribute, mode
//         and amount, the seeds, the player's numbers after (resources, share bonus, AI difficulty, scenario
//         variables, tributes, mining limit, the statistics' saved bytes' FNV-1a) and the pending effects after
//   SMTK  every EESMEffect::ExpandTokens: the text's and the result's FNV-1a, the seeds
//   SMFP  every EESMObject::FindPlacement (the retry too): the object's index, the unit type, the random flag, the
//         result and location, the seeds
//   SMCU  every EEChangeUnit (kind 0) and EEModifyUnitAttribute (kind 1): the arguments, the seeds, the unit's values
//         after (checksum, hit points, resource, flight time, garrison, ammunition, scale, reload, power, rotting,
//         flags, variables, command and goal queues) or the player's upgrade after, and the pending effects
// Off (EE_SIM_CAPTURE unset) each hook is one cached test. The records are built in Rust (ee_sim scenario
// sm_capture.rs). The hooks the game calls are pointers that EEScenarioCapture.cpp sets before main, so no game unit names a symbol of that TEST ONLY unit (PORT_LEARNINGS
// "Cycle cuts").
#ifndef EE_EESCENARIOCAPTURE_H
#define EE_EESCENARIOCAPTURE_H

#include "ee/EESimCapture.h"
#include "rust/sim/scenario.h"   // the records' states before (ee_sim scenario sm_capture.rs)

class EESMCondition;
class EESMTrigger;
class EESMObject;
class EESMArea;
class EESMEffect;
class EEScenarioManager;
class EEUnit;
class EEPlayer;
class EEUnitClassUpgrade;
class UWideString;

// the physics random stream's seed (0 without physics)
inline unsigned int (*EESMCaptureSeed)() = nullptr;
inline void (*EESMCaptureCondition)(EESMCondition* inCondition, bool inResult, unsigned int inSeed) = nullptr;
// the trigger's state before: on, fired, start time
typedef EeScenarioCapTriggerBefore EESMCaptureTriggerState;
inline EESMCaptureTriggerState (*EESMCaptureTriggerBefore)(EESMTrigger* inTrigger) = nullptr;
inline void (*EESMCaptureTrigger)(EESMTrigger* inTrigger, const EESMCaptureTriggerState& inBefore, float inTime,
                                  unsigned int inSeed) = nullptr;
// SMOB: Begin before the object's Evaluate (the units before; the depth that keeps SMCP/SMAR out), End after it
typedef EeScenarioCapObjectBefore EESMCaptureObjectState;
inline EESMCaptureObjectState (*EESMCaptureObjectBegin)(EESMObject* inObject) = nullptr;
inline void (*EESMCaptureObjectEnd)(EESMObject* inObject, bool inIncludeDead, bool inResult,
                                    const EESMCaptureObjectState& inBefore) = nullptr;
inline void (*EESMCapturePlayers)(long inType, bool inSingle, long inResult, const void* inPlayers,
                                  unsigned int inSeed) = nullptr;
// kind 0 ContainsUnit (unit id), 1 ContainsTile (x, y), 2 GetCenterTile, 3 GetTiles; the result's x, y (or bool in x;
// GetTiles: count and hash)
inline void (*EESMCaptureArea)(EESMArea* inArea, long inKind, long inA, long inB, long inX, long inY,
                               unsigned long long inHash) = nullptr;
inline void (*EESMCaptureEffect)(EESMEffect* inEffect, bool inIsCinematic, unsigned int inSeed) = nullptr;
inline void (*EESMCapturePlayerAttribute)(long inPlayer, unsigned long inAttribute, long inMode, long inAmount,
                                          unsigned int inSeed) = nullptr;
inline void (*EESMCaptureTokens)(const UWideString& inText, const UWideString& outText, unsigned int inSeed) = nullptr;
inline void (*EESMCapturePlacement)(EESMObject* inObject, unsigned long inUnitType, bool inTryRandom, bool inResult,
                                    float inX,
                                    float inY, unsigned int inSeed) = nullptr;
// SMCU: kind 0 EEChangeUnit (inArgs: attribute, percent, unit id, minimum, maximum, random, mode), kind 1
// EEModifyUnitAttribute (attribute, percent, player id, unit type, amount, mode, 0)
inline void (*EESMCaptureUnitChange)(long inKind, const long* inArgs, EEUnit* inUnit, EEPlayer* inPlayer,
                                     EEUnitClassUpgrade* inUpgrade, unsigned int inSeed) = nullptr;
inline void (*EESMCaptureUpdate)(EEScenarioManager* inManager, bool inHumanPlaying) = nullptr;
inline void (*EESMCaptureSaved)(EEScenarioManager* inManager, long inKind, long inResult, unsigned long inMajorVersion,
                                unsigned long inMinorVersion) = nullptr;

#endif
