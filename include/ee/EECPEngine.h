// The computer player's engine, chosen per computer seat (docs/AI_LUA_PLAN.md §5 S0). New in this port (no
// original address).
//
// The lobby holds the host's choice for each computer slot (EEGameOptionsPlayer::mEngine: classic, next or random;
// the EEMPlayerInformation field kPlayerInfoEngine carries it to every peer). EEGameState::Create resolves random
// at the game's start from the game's random seed and the slot, the same on every peer, and gives the computer
// player its engine (EEComputerPlayer::mEngine: classic or next). Saved games keep it (EEPPlayerHelper, from save
// version 0.251). TEST ONLY: EE_AI_ENGINE in the environment overrides the lobby: one engine for every computer
// player, or a comma list taken in turn by player id from player 1 (the human slot), as EE_AI_PERSONALITY.
//
// classic: the original computer player, unchanged. next: the new engine; for now a stub that makes no decisions
// (EECPNextUpdate), so its town stands idle.
#ifndef EE_EECPENGINE_H
#define EE_EECPENGINE_H

class EEComputerPlayer;

enum EECPEngineKind {
    kEECPEngineClassic = 0,
    kEECPEngineNext = 1,
    kEECPEngineRandom = 2,          // a lobby choice only (resolved at the game's start)
    kEECPEngineChoiceCount = 3,     // the lobby's choices (its combo lines, in this order)
};

// the computer player's engine: kEECPEngineClassic or kEECPEngineNext
long EECPEngine(const EEComputerPlayer* inPlayer);
const char* EECPEngineName(long inEngine);   // "classic", "next", "random" ("classic" for anything else)

// the engine a computer player of the lobby slot inSlot (0..15) plays with: the choice, random resolved from the
// game's random seed and the slot (identical on every peer, and independent of the game's other draws), then
// EE_AI_ENGINE's override for the player id
long EECPEngineResolve(unsigned long inChoice, unsigned long inSeed, unsigned long inSlot, unsigned long inPlayerID);

// the next engine's tick (EEComputerPlayer::Update calls it instead of the managers' decisions). S0: the stub; the
// later stages replace it with view -> Lua strategy -> tactics
void EECPNextUpdate(EEComputerPlayer* inPlayer, unsigned long inTime);

#endif
