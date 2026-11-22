// C:\EEX\Empire Earth\AI Thread\EEArtificialIntelligence.h (RECONSTRUCTION; file name inferred). The
// AI thread's scheduler of the computer players (RTTI EEArtificialIntelligence, no base; vtable 0x44218c: the destructor
// only; sizeof 0x20; the game holds it in gArtificialIntelligence 0x5319d0, assert "gArtificialIntelligence != NULL").
// Its ctor 0x14260b and ??_G 0x142639 are inline (emitted by Empire Earth.cpp, their first user); EEArtificialIntelligence.cpp
// 0x2cc87..0x2ce2e holds two free helpers and the members. Names are provisional. Requires <vector>.
#ifndef EE_EEARTIFICIALINTELLIGENCE_H
#define EE_EEARTIFICIALINTELLIGENCE_H

#include "rust/sim/ai.h"   // ee_ai_collect_computer_players, ee_ai_update (ee_sim::ai next_schedule.rs)

// The computer players' schedule (0x2cc87..0x2ce2e; its members and bodies are Rust: ee_sim::ai next_schedule.rs)
class EEArtificialIntelligence {
public:
    virtual ~EEArtificialIntelligence() {}
    void CollectComputerPlayers() { ee_ai_collect_computer_players(); }   // 0x2cd0e (after a game is set up)
    void Update() { ee_ai_update(); }   // 0x2cd99 (the physics thread: one computer player per interval)
};

extern EEArtificialIntelligence* gArtificialIntelligence;

#endif
