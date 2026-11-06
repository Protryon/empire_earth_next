// New (port): the game log. Every game writes <data root>/Logs/game-<date>-<time>.log (config.ini [Game]
// "Game Log", 1 by default; 0 for none) so a played game can be dug into afterwards, for the human and the computer
// players alike (tools/game_log.py). One line per record, "EE-GAME <game ms> p<player> <event> key=value ...":
//   start    the map, difficulty and players (human or computer, civilization, the AI's personality and cheats)
//   sample   every 30 game s per player: epoch, population, citizens (idle, gathering per resource, building),
//            army, buildings, towers, stock, what came in and what was lost since the last sample
//   epoch    a player reached an epoch
//   end      the game's last sample (the log closes with the next game or the program)
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

#endif
