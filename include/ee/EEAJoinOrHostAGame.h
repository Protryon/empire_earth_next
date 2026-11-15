// C:\EEX\Empire Earth\Actions\EEAJoinOrHostAGame.h (RECONSTRUCTION; file name inferred). The join-or-host action of
// the multiplayer screens (RTTI EEAJoinOrHostAGame > EEAction > UReferenceCounter; vtable 0x438c08; sizeof 0x38;
// EEAJoinOrHostAGame.cpp 0x2086..0x289f): ctor 0x2086 (called from 0xce421), ??_G 0x20da, dtor 0x20f6, Virtual4
// 0x2120 (checks the names, creates the client or the server, connects and opens the lobby form 0x1a). Modes (mMode):
// 0 host, 1 join the detected game named mGameName, 2/3 connect to the
// address mGameName (3: a Sierra.com game), 4 host a Sierra.com game. Names are provisional (by behaviour).
// Requires ee/EEAction.h, lle/UWideString.h.
#ifndef EE_EEAJOINORHOSTAGAME_H
#define EE_EEAJOINORHOSTAGAME_H

#include "ee/EEAction.h"

class UIForm;

class EEAJoinOrHostAGame : public EEAction {
public:
    EEAJoinOrHostAGame(long inMode, const UWideString& inPlayerName, const UWideString& inGameName);   // 0x2086
    virtual ~EEAJoinOrHostAGame();          // +0x00 (??_G 0x20da, dtor 0x20f6)
    virtual void Virtual4();                // +0x04 0x2120
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)

    long mMode;                             // +0x0c
    UWideString mPlayerName;                // +0x10
    UWideString mGameName;                  // +0x24 (the host's game name, or the address to connect to)
};

// 0x2786: the "cannot host" message box's callback: back to a fresh client (provisional name)
void EEAJoinOrHostAGameRestartClient(UIForm* inForm);

#endif
