// C:\EEX\Empire Earth\Actions\EEARemovePlayer.cpp (file name inferred: no __FILE__; RTTI class EEARemovePlayer,
// vtable 0x43e42c). The action that boots a player from the server: 0x4413..0x4444 (EEAPlayerEvent's Virtual4 ends
// at 0x4413, EEAResetAllPlayerInfo's ctor starts at 0x4444). Ctor inline (ee/EEARemovePlayer.h), ??_G the shared
// EEAction one, Virtual8 the shared empty slot.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NESynchronousServer.h"

#include "ee/EECommunications.h"
#include "ee/EEAction.h"
#include "ee/EEARemovePlayer.h"

// FUNCTION: EXE 0x404413
// Has the server boot the action's player from the game
void EEARemovePlayer::Virtual4()
{
    if (gCommunications->IsServer() == true)
        static_cast<NESynchronousServer*>(gCommunications)->BootPlayer(this->mPlayer, this->mB0D, this->mIsExiting);
}

// ICF-FOLDED: EXE 0x5720c6
// (empty: folded into the shared empty slot; vtable 0x43e42c +0x08 points there)
void EEARemovePlayer::Virtual8()
{
}
