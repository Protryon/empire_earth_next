// New (port): the game log. Every game writes <data root>/Logs/game-<date>-<time>.log (config.yaml debug:
// "Game Log", 1 by default; 0 for none) so a played game can be dug into afterwards, for the human and the computer
// players alike (tools/game_log.py). One line per record, "EE-GAME <game ms> p<player> <event> key=value ...":
//   start    the map, difficulty and players (human or computer, colour, civilization, the AI's personality and cheats)
//   civ      a player's civilization once chosen (the screen opens after the start; checked every sample)
//   sample   every 30 game s per player: epoch, population, citizens (idle, gathering per resource, building),
//            army, buildings, towers, stock, what came in and what was lost since the last sample
//   epoch    a player reached an epoch
//   chat     a chat line, the text last ("text=" to the end of the line): notes typed while playing
//   end      the game's last sample (the log closes with the next game or the program)
//   order    TEST ONLY (training data from playtests): a human player's order as it executes in the lockstep
//            (EEPhysics::DrainInput, so the same in multiplayer and replays): verb= move, attack, convert, gather,
//            build, repair, stop, garrison, load, unload, patrol, patrol_point, formation, train, research, explore,
//            attack_ground, heal, rally, return, dropoff, stance, wall, gate, delete, improve, pack, unpack, fanatic,
//            replant, calamity; for the selected group units=N classes=<class>:<n>,... ids=<id>,... (24 at most);
//            then target=<id>:<class>:p<owner>@<x>,<y> (a unit), at=<x>,<y> (a tile), class=/tech= (what is
//            built, trained or researched), queued=1, forced=1, times=5, kind=<n> (formation, stance)
//   select   TEST ONLY: a human player's selection after a selection message, when it changed (units=, classes=, ids=)
// With the log on, the computer players' decision trace (ee/EECPTrace.h, the "EE-AI" lines) goes into it too.
// The log only reads the game; it never changes it (lockstep stays intact).
// Requires lle/LowLevelEngine.h.
#ifndef EE_EEGAMELOG_H
#define EE_EEGAMELOG_H

class EEPlayer;
class EEUnit;

bool EEGameLogOn();                                         // a game log is open
void EEGameLogTick(unsigned long inGameTime);               // every physics tick (EEArtificialIntelligence::Update)
void EEGameLogLine(const char* inText);                     // a whole line, newline included (the AI trace)
void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount);   // EEPlayer::AddResource
void EEGameLogUnitLost(EEPlayer* inPlayer, EEUnit* inUnit);  // EEPlayer::UnitTaken (not swapped)
void EEGameLogChat(unsigned long inPlayer, const char* inText);   // EEMInGameChat::Execute: a chat line (notes while playing)

#endif
