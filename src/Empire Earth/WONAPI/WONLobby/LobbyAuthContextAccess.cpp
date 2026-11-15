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
namespace WONAPI {
// FUNCTION: WON 0x0075b490
// Returns the login context; with CRC checks on, first gives it the files to hash for the community (once)
AuthContext* GetLobbyAuthContext()
{
    if(gLobbyData.get()==NULL)
        return NULL;
    if(gLobbyData->mConfig->mCRCCheck && !gLobbyData->mHasSetHashFileList) {
        gLobbyData->mHasSetHashFileList=true;
        if(gLobbyData->mHashFileList.empty())
            gLobbyData->mAuthContext->SetHashFile(gLobbyData->mLoginCommunity);
        else
            gLobbyData->mAuthContext->SetHashFileList(gLobbyData->mLoginCommunity,gLobbyData->mHashFileList);
    }
    return gLobbyData->mAuthContext.get();
}
}
