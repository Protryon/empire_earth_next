// C:\EEX\Empire Earth\WONAPI\WONLobby\LobbyPersistentData.cpp (file name inferred: no __FILE__)
// LobbyPersistentData (RTTI name; 1164-byte object, constructor 0x75bcb0, destructor 0x75c2c0, vtable 0x851c28):
// the lobby's saved settings. Since R6 WONLobby round 15 the class and every Lobby* function over it are one Rust
// object (rust/crates/ee_won_lobby/src/persistobj.rs: the containers, the files, the password key, the rules); the
// functions are inline forwards in WONLobbyPersistence.h. This unit is the handle's constructor and the object's
// host: the friends (LobbyFriend shells), the resource strings, the click sound, the config's LAN-number switch and
// the clock.

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <time.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONAuthContext.h"

namespace WONAPI {
static intptr_t PersistHost(void*, EeWonPersistOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeWonPersistOp::FriendNew: {
        LobbyFriend* aFriend = new LobbyFriend(LobbyPersistString(reinterpret_cast<const uint32_t*>(a), (size_t)b),
                                               (unsigned long)c);
        aFriend->AddRef();
        return (intptr_t)aFriend;
    }
    case EeWonPersistOp::FriendRef:
        if (b) reinterpret_cast<LobbyFriend*>(a)->AddRef();
        else reinterpret_cast<LobbyFriend*>(a)->Release();
        return 0;
    case EeWonPersistOp::FriendName: {
        // the friend's own text (GetName returns a copy sharing it)
        GUIStringData* aName = (GUIStringData*)reinterpret_cast<LobbyFriend*>(a)->MGet(LobbyMF::FriendName);
        *reinterpret_cast<size_t*>(b) = aName->mLength;
        return (intptr_t)aName->mText;
    }
    case EeWonPersistOp::ResText: {
        const GUIString& aText = gLobbyResources[a].mString;
        *reinterpret_cast<size_t*>(b) = aText.length();
        return (intptr_t)aText.mData->mText;
    }
    // the click sound is used without a null check, as the original did
    case EeWonPersistOp::MuteClick: LOBBY_SOUND(LobbyGlobal_ButtonClick_Sound)->SetMuted(a != 0); return 0;
    case EeWonPersistOp::AppendLanNumber: {
        LobbyConfig* aConfig = GetLobbyConfig();
        return aConfig == NULL || aConfig->mAppendNumberToDefaultLanName;
    }
    case EeWonPersistOp::Now: return (intptr_t)(long)time(NULL);
    }
    return 0;
}

// Creates the lobby's saved settings with default options and a password key bound to this machine
LobbyPersistentData::LobbyPersistentData()
{
    int32_t aKey[2] = {0x22fa655f, (int32_t)CDKey::GetMachineId(NULL)}; // two little-endian longs (AppendLong)
    EeWonPersistHost aHost = {NULL, PersistHost};
    this->mRust = ee_won_persist_new(aHost, sizeof(wchar_t), reinterpret_cast<const uint8_t*>(aKey), sizeof(aKey));
}

EeWonPersist* LobbyPersist()
{
    return gLobbyData.get() != NULL && gLobbyData->mPersistentData.get() != NULL ? gLobbyData->mPersistentData->mRust : NULL;
}
}
