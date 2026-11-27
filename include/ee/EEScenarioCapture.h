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
// sm_capture.rs; the elements' records are called from Rust, ee_scenario entry.rs; the C++ hosts call the Rust
// records directly, sm_capture_game.rs and entry.rs's ee_scenario_en_cap_* with the save version).
#ifndef EE_EESCENARIOCAPTURE_H
#define EE_EESCENARIOCAPTURE_H

#include "ee/EESimCapture.h"

#endif
