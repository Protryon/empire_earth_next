// Access to the lobby's login (auth) context
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "rust/won_lobby/lobbyprv.h"
namespace WONAPI {
// Returns the login context; with CRC checks on, first gives it the files to hash for the community (once)
AuthContext* GetLobbyAuthContext()
{
    if(gLobbyData.get()==NULL)
        return NULL;
    if(gLobbyData->mConfig->mCRCCheck) // the files once (lobbyprv.rs: the list, or the running executable)
        ee_won_lobbyprv_hash_login(ee_won_authctx_login(gLobbyData->mAuthContext->Core()), gLobbyData->mLoginCommunity.data(),
                                   sizeof(wchar_t), gLobbyData->mLoginCommunity.size());
    return gLobbyData->mAuthContext.get();
}
}
