// The computer player's reflexes (ee_ai next_reflex.rs; docs/AI_LUA_VERBS.md §5): the citizen militia, the danger
// zones and the flight from them, the naval flight. The bodies are Rust; these are their C++ entry points
// (EECPNext.cpp). New in this port: no original address.
#ifndef EE_EECPREFLEXES_H
#define EE_EECPREFLEXES_H

class EEComputerPlayer;
class EEUnit;
class EEComplexUnit;

// a citizen hit by a player's unit: the militia (military.militia) or a danger zone and its defenders
void EECPRMNoteDanger(EEComputerPlayer* inPlayer, EEUnit* inCitizen, unsigned long inAttacker);
// a boat or land unit hit by a warship: the units round it leave (to the farthest dock, or the town centre)
void EECPRMNavalFlee(EEComputerPlayer* inPlayer, EEUnit* inVictim, EEComplexUnit* inAttacker);
// every tick: the zones expire; the gatherers in one go home
void EECPRMFleeRaids(EEComputerPlayer* inPlayer);

#endif
