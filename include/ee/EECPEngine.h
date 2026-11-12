// The computer player's engine, chosen per computer seat (docs/AI_LUA_PLAN.md §5 S0, "AI manifest"). New in this
// port (no original address).
//
// The lobby's choices for a computer slot (EEGameOptionsPlayer::mEngine, an index; the EEMPlayerInformation field
// kPlayerInfoEngine carries it to every peer): 0 classic, 1 random, 2 + k the AI manifest's entry k
// (Data/ai/ais.json, in file order). The manifest is read once per process (EE_AI_SCRIPTS's folder first, TEST
// ONLY); a missing or unreadable file leaves just classic and random, a bad entry is skipped with a log line.
// EEGameState::Create resolves random at the game's start from the game's random seed and the slot (the same on every
// peer) and gives the computer player its engine: EEComputerPlayer::mEngine classic or next, and for next the AI's id
// (EECPNextState::mAI), whose manifest entry names the Lua script. Saved games keep the engine (EEPPlayerHelper,
// from save version 0.251) and the AI's id (from 0.253; older saves' next seats are the manifest's "boom").
// TEST ONLY: EE_AI_ENGINE in the environment overrides the lobby: one choice for every computer player, or a comma
// list taken in turn by player id from player 1 (the human slot), as EE_AI_PERSONALITY; its names are classic,
// random, an AI's id, or next (the manifest's first entry).
//
// classic: the original computer player, unchanged. next: the new engine (EECPNextUpdate) running the AI's script.
#ifndef EE_EECPENGINE_H
#define EE_EECPENGINE_H

#include <cstdint>
#include <string>
#include <vector>

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
const unsigned long kEECPMaxAIs = 64;   // the manifest's entries past this are skipped

// an AI of the manifest (Data/ai/ais.json "ais")
struct EECPAIEntry {
    std::string mID;                // [a-z0-9_]+, unique; not classic, random or next
    std::string mName;              // the lobby's text (UTF-8, a malformed byte as U+FFFD)
    std::string mScript;            // the Lua module (require's name: dots are folders under Data/ai)
    std::string mDescription;
    bool mRandom;                   // the lobby's Random may pick it
    bool mSeesAll;                  // "sees_all": its queries may read through the fog (ai.enemy_near; the classic's
                                    // information cheat, docs/AI_LUA_UNITS.md §11); false for every other AI
    bool mFillsGarrison;            // "fills_garrison": ai.garrison_fill may top a building's garrison up with no
                                    // citizens (the classic's FillGarrison cheat, docs/AI_LUA_UNITS.md §11)
};

const std::vector<EECPAIEntry>& EECPAIManifest();   // read at the first call
unsigned long EECPEngineChoiceCount();              // 2 + the manifest's entries
const EECPAIEntry* EECPAIByChoice(unsigned long inChoice);   // NULL: classic, random or out of range
const EECPAIEntry* EECPAIByID(const std::string& inID);      // NULL: not in the manifest
long EECPChoiceByName(const std::string& inName);    // classic, random, next (the first AI) or an id -> -1: none
// a hash of the AI folder's content (ais.json and every .lua by relative path; FNV-1a 32): part of the checksum a
// joiner must match (EEInitializeNetwork), and logged at the start
uint32_t EECPAIFolderHash();

// a log line "EE-AI engine <text>" (the debug output, the game log when open, EE-SIM's stderr)
void EECPEngineLog(const char* inFormat, ...);

// the computer player's engine: kEECPEngineClassic or kEECPEngineNext
long EECPEngine(const EEComputerPlayer* inPlayer);
// what it plays: "classic" or its AI's id (the trace and the game log's engine=)
std::string EECPEngineName(const EEComputerPlayer* inPlayer);
// the next engine's AI id of a computer player ("" none)
std::string EECPEngineAI(const EEComputerPlayer* inPlayer);
// whether the seat's AI declares "sees_all" in the manifest (false for the classic and every other seat)
bool EECPEngineSeesAll(const EEComputerPlayer* inPlayer);
// whether the seat's AI declares "fills_garrison" in the manifest (false for the classic and every other seat)
bool EECPEngineFillsGarrison(const EEComputerPlayer* inPlayer);
// gives a computer player its engine from a resolved choice (classic or 2 + k)
void EECPEngineSet(EEComputerPlayer* inPlayer, unsigned long inChoice);

// the choice a computer player of the lobby slot inSlot (0..15) plays with: the lobby's choice after EE_AI_ENGINE's
// override for the player id, random resolved from the game's random seed and the slot (identical on every peer,
// and independent of the game's other draws) uniformly among classic and the AIs marked random -> classic or 2 + k
unsigned long EECPEngineResolve(unsigned long inChoice, unsigned long inSeed, unsigned long inSlot,
                                unsigned long inPlayerID);

// the next engine's tick (EEComputerPlayer::Update calls it instead of the managers' decisions)
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime);
// the next engine's start (EEComputerPlayer::Update, the first tick): resolves the script and loads it -> false
// when it cannot (logged), and the seat then plays classic
bool EECPNextPrepare(EEComputerPlayer* inPlayer);

#endif
