// C:\EEX\Empire Earth\Actions\EEAResetAllPlayerInfo.h (RECONSTRUCTION; file name inferred). The scenario editor's
// reset-all-player-info action (RTTI EEAResetAllPlayerInfo > EEAction > UReferenceCounter; vtable 0x439050; sizeof
// 0x1c; EEAResetAllPlayerInfo.cpp 0x4444..0x4613): the inline ctor (COMDAT 0x2813be at its first user: the player
// list by value), the dtor 0x4444 (out of line: ??_G 0x45f7 calls it),
// Virtual4 0x4464 (per listed player: the civilization helper, then three posts to the editor's load thread).
// Names are provisional. Requires ee/EEAction.h, <vector>.
#ifndef EE_EEARESETALLPLAYERINFO_H
#define EE_EEARESETALLPLAYERINFO_H

#include <vector>
#include "ee/EEAction.h"

class EEPlayer;

class EEAResetAllPlayerInfo : public EEAction {
public:
    EEAResetAllPlayerInfo(std::vector<unsigned long> inPlayers) : mPlayers(inPlayers) {}   // inline (0x2813be)
    virtual ~EEAResetAllPlayerInfo();       // +0x00 (??_G 0x45f7, dtor 0x4444)
    virtual void Virtual4();                // +0x04 0x4464
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)

    unsigned long GetPlayersAt(unsigned long inIndex) const { return this->mPlayers[inIndex]; }
    std::vector<unsigned long> mPlayers;    // +0x0c the players (indices into gWorld.mPlayers)
};

// 0x2806e7 (cdecl; the civilization unit at 0x27fdfe..0x2813f4): EEAResetAllPlayerInfo passes the player's +0x7ac
// string and the player (provisional name; types inferred)
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer);

#endif
