// Restoring the lobby's default settings
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
// Defined in LobbyPersistentData.cpp (0x75c900)
extern LobbyPersistentData* GetLobbyPersistentData();
// FUNCTION: WON 0x0075e6a0
// Restores the default settings, marks the user settings changed and the global ones if sound, music or adapter changed
void LobbyRestoreDefaults()
{
    LobbyPersistentData* aData = GetLobbyPersistentData();
    if(aData == NULL)
        return;
    int anOldAdapter = aData->mNetworkAdapter;
    aData->RestoreDefaults();
    aData->mUserDirty = true;
    aData->mGlobalDirty = aData->mSavedLobbySoundEffects != aData->mLobbySoundEffects ||
        aData->mSavedLobbyMusic != aData->mLobbyMusic ||
        aData->mNetworkAdapter != anOldAdapter;
}
}
