// C:\EEX\Empire Earth\AI Thread\EEArtificialIntelligence.h (RECONSTRUCTION; file name inferred). The
// AI thread's scheduler of the computer players (RTTI EEArtificialIntelligence, no base; vtable 0x44218c: the destructor
// only; sizeof 0x20; the game holds it in gArtificialIntelligence 0x5319d0, assert "gArtificialIntelligence != NULL").
// Its ctor 0x14260b and ??_G 0x142639 are inline (emitted by Empire Earth.cpp, their first user); EEArtificialIntelligence.cpp
// 0x2cc87..0x2ce2e holds two free helpers and the members. Names are provisional. Requires <vector>.
#ifndef EE_EEARTIFICIALINTELLIGENCE_H
#define EE_EEARTIFICIALINTELLIGENCE_H

class EEComputerPlayer;
class EEPlayer;
class EEUnit;

class EEArtificialIntelligence {
public:
    EEArtificialIntelligence() : mNextUpdateTime(0), mNextPlayer(0) { this->mUpdateInterval = 1000; }
    virtual ~EEArtificialIntelligence() {}
    void CollectComputerPlayers();          // 0x2cd0e (after a game is set up: 0x139a26, 0x276e53)
    void Update();                          // 0x2cd99 (the physics thread: one computer player per interval)

    std::vector<EEComputerPlayer*> mComputerPlayers;   // +0x04
    unsigned long mUpdateInterval;          // +0x14 (1000 / the number of computer players)
    unsigned long mNextUpdateTime;          // +0x18
    unsigned long mNextPlayer;              // +0x1c
};

extern EEArtificialIntelligence* gArtificialIntelligence;

#endif
