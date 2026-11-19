// The computer player's engine, chosen per computer seat (docs/AI_LUA_PLAN.md §5 S0, "AI manifest"). New in this
// port (no original address). The manifest, the choices, the resolve, the folder's hash and the log are Rust's
// (ee_game cp_engine.rs, since the game's leaves round 1); the seats' functions are EECPNext.cpp's.
//
// The lobby's choices for a computer slot (EEGameOptionsPlayer::mEngine, an index; the EEMPlayerInformation field
// kPlayerInfoEngine carries it to every peer): 0 Classic (the manifest's "classic" AI), 1 random, 2 + k the AI
// manifest's entry k
// (Data/ai/ais.json, in file order). The manifest is read once per process (EE_AI_SCRIPTS's folder first, TEST
// ONLY); a missing or unreadable file leaves just classic and random, a bad entry is skipped with a log line.
// EEGameState::Create resolves random at the game's start from the game's random seed and the slot (the same on every
// peer) and gives the computer player its AI's id (EECPNextState::mAI), whose manifest entry names the Lua script.
// Every computer player runs a Lua AI (the C++ classic's decisions were deleted: stage 5, AI_RESEARCH §8.90). A player
// given no AI (a scenario's, a saved game's from before 0.251) plays the manifest's "classic" AI; a seat whose AI is
// missing or does not load stands idle (a log line says why). Saved games keep the engine (EEPPlayerHelper,
// from save version 0.251) and the AI's id (from 0.253; older saves' next seats are the manifest's "boom").
// TEST ONLY: EE_AI_ENGINE in the environment overrides the lobby: one choice for every computer player, or a comma
// list taken in turn by player id from player 1 (the human slot), as EE_AI_PERSONALITY; its names are classic,
// random, an AI's id, or next (the manifest's first entry).
//
#ifndef EE_EECPENGINE_H
#define EE_EECPENGINE_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#include "rust/game/cp_engine.h"

class EEComputerPlayer;

// EEComputerPlayer::mEngine: the engine a computer player plays with
enum EECPEngineKind {
    kEECPEngineClassic = 0,
    kEECPEngineNext = 1,
};

// the lobby's choices (EEGameOptionsPlayer::mEngine; the engine box's lines, in this order)
enum EECPEngineChoice {
    kEECPChoiceClassic = 0,
    kEECPChoiceRandom = 1,
    kEECPChoiceFirstAI = 2,         // + k: the manifest's entry k
};

// an AI of the manifest (Data/ai/ais.json "ais"; its texts live as long as the process): id ([a-z0-9_]+, unique),
// name (the lobby's text, UTF-8), script (the Lua module: dots are folders under Data/ai), random (the lobby's
// Random may pick it), sees_all, fills_garrison (the classic's cheats, docs/AI_LUA_UNITS.md §11), classic (the
// lobby's Classic plays it)
typedef EeCpAiEntry EECPAIEntry;
inline std::string EECPAIText(const uint8_t* inText, size_t inLength) { return std::string((const char*)inText, inLength); }
inline std::string EECPAIID(const EECPAIEntry* inAI) { return EECPAIText(inAI->id, inAI->id_len); }

inline unsigned long EECPEngineChoiceCount() { return (unsigned long)ee_cp_engine_choice_count(); }   // 2 + the AIs
// NULL: random or out of range (choice 0: the "classic" AI, NULL when the manifest marks none)
inline const EECPAIEntry* EECPAIByChoice(unsigned long inChoice) { return ee_cp_engine_by_choice(inChoice); }
inline const EECPAIEntry* EECPClassicAI() { return ee_cp_engine_classic(); }   // NULL: none
inline const EECPAIEntry* EECPAIByID(const std::string& inID)                  // NULL: not in the manifest
{
    return ee_cp_engine_by_id((const uint8_t*)inID.data(), inID.size());
}
// classic, random, next (the first AI) or an id -> -1
inline long EECPChoiceByName(const std::string& inName) { return (long)ee_cp_engine_choice_by_name(inName.c_str()); }
// a hash of the AI folder's content (ais.json and every .lua by relative path; FNV-1a 32): part of the checksum a
// joiner must match (EEInitializeNetwork), and logged at the start
inline uint32_t EECPAIFolderHash() { return ee_cp_engine_folder_hash(); }

// a log line "EE-AI engine <text>" (the debug output, the game log when open, EE-SIM's stderr)
inline void EECPEngineLog(const char* inFormat, ...)
{
    char theText[900];
    va_list theArguments;
    va_start(theArguments, inFormat);
    vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    ee_cp_engine_log(theText);
}

// the computer player's engine: kEECPEngineClassic or kEECPEngineNext
long EECPEngine(const EEComputerPlayer* inPlayer);
// what it plays: its AI's id ("none": a seat without one; the trace and the game log's engine=)
std::string EECPEngineName(const EEComputerPlayer* inPlayer);
// the next engine's AI id of a computer player ("" none)
std::string EECPEngineAI(const EEComputerPlayer* inPlayer);
// whether the seat's AI declares "sees_all" in the manifest (false for the classic and every other seat)
bool EECPEngineSeesAll(const EEComputerPlayer* inPlayer);
// whether the seat's AI declares "fills_garrison" in the manifest (false for the classic and every other seat)
bool EECPEngineFillsGarrison(const EEComputerPlayer* inPlayer);
// gives a computer player its AI from a resolved choice (classic or 2 + k; none: the seat stands idle)
void EECPEngineSet(EEComputerPlayer* inPlayer, unsigned long inChoice);

// the choice a computer player of the lobby slot inSlot (0..15) plays with: the lobby's choice after EE_AI_ENGINE's
// override for the player id, random resolved from the game's random seed and the slot (identical on every peer,
// and independent of the game's other draws) uniformly among classic and the AIs marked random -> classic or 2 + k
inline unsigned long EECPEngineResolve(unsigned long inChoice, unsigned long inSeed, unsigned long inSlot,
                                       unsigned long inPlayerID)
{
    return (unsigned long)ee_cp_engine_resolve(inChoice, inSeed, inSlot, inPlayerID);
}

// the next engine's tick (EEComputerPlayer::Update calls it instead of the managers' decisions)
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime);
// the next engine's start (EEComputerPlayer::Update, the first tick): resolves the script and loads it -> false
// when it cannot (logged), and the seat then stands idle
bool EECPNextPrepare(EEComputerPlayer* inPlayer);

#endif
