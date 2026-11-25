// C:\EEX\Empire Earth\Communications\EEClient.cpp (__FILE__ 0x4ba13c): the game's lock-step client, 0xb71b5..0xb8cda
// (EEUnitCalamityClass.cpp ends at 0xb71b5; EEMessage.cpp starts at 0xb8cda). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/UResourceFile.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEARust.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "rust/sim/netmsg.h"   // ee_netmsg session.rs (EESession: EEServer.cpp's host)
#include "rust/game/gamecpp.h"

// 0x4e6058: default-constructed (its init chain 0xb71b5 opens the unit; use inferred, provisional name)
UWideString gU4E6058;

#include <list>
#include <string>
#include <mbstring.h>

#include "ee/EESaveGameFiles.h"
#include "ee/EEStringIDs.h"
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)

// Boots this client from the canceled game when the host's slot (0) has become open, else refreshes the slot's line
void EEClient::HandleSlotUpdate(unsigned char inSlot, EEGameOptionsPlayer& inPlayer)
{
    ee_gamecpp_client_slot_update(inSlot, static_cast<EeAppOptionsPlayerFields*>(&inPlayer));
}

// Reports a connection state: 0 could not connect (back to the game list), 1 connecting, 4 connecting with retries
void EEClient::SetConnectionState(long inState)
{
    EESession(this, false, EE_NETMSG_SESS_DO_CONNECTION_STATE, inState);
}

// A player's removal: the host's starts the server migration, ours cancels the game, another's is announced
void EEClient::ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    EESession(this, false, EE_NETMSG_SESS_DO_REMOVE, inPlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
}

// Cancels the game (the callback of the booted message box)
void CancelGameCallback(void* inData)
{
    gUserInterface->PostCancelGame();
}

// Returns the connect timeout, kept in the global at 0x5308a0
unsigned long EEClient::GetConnectTimeout()
{
    return gNetworkAdapter;
}

// A server message: connection responses (the lobby or a refusal), reconnection, communications ticks; load/save and
// lag messages execute at once
void EEClient::HandleMessage(NEMessage* inMessage)
{
    EESession(this, false, EE_NETMSG_SESS_DO_CLIENT_MESSAGE, (long long)(uintptr_t)inMessage);
}

// Stamps the local player's input with the fast-selection number; selection messages take a new one and run at once
void EEClient::HandleUserInput(NEMessage* inMessage)
{
    EESession(this, false, EE_NETMSG_SESS_DO_LOCAL_INPUT, (long long)(uintptr_t)inMessage);
}

// The lost server: in a game the server migration, after loading the game cancelled, in the lobby the game list
void EEClient::HandleConnectionLost()
{
    EESession(this, false, EE_NETMSG_SESS_DO_CONNECTION_LOST);
}

// A lobby message: player and slot info, transfer progress, the game start, a connection check, a joined player
void EEClient::HandleLobbyInput(NEMessage* inMessage)
{
    EESession(this, false, EE_NETMSG_SESS_DO_LOBBY_INPUT, (long long)(uintptr_t)inMessage);
}

// Starts the game: the graphics, the physics loop, the main game UI; humans without a slot dropped
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    EESession(this, false, EE_NETMSG_SESS_DO_START_GAME);
    return 0;
}

// ------------------------------------------------------------------------- round 9, lane D: the client's bodies' C++


#define P64(x) ((int64_t)(intptr_t)(x))
#define FILESPEC ((FSFileSpec*)a[0])
#define WSTR(k) (*(UWideString*)a[k])

// ------------------------------------------------------------------------- round 11, lane C: the client's bodies' C++


// the client's plain tail (gamecpp session_objs.rs EeGamecppClientTail): mHasLeftGame .. mWasBooted
static_assert(offsetof(EEClient, mUISlotNumber) - offsetof(EEClient, mHasLeftGame) == 1
                  && offsetof(EEClient, mUF2) - offsetof(EEClient, mHasLeftGame) == 18
                  && offsetof(EEClient, mIsExecutingSelection) - offsetof(EEClient, mHasLeftGame) == 20
                  && offsetof(EEClient, mWasBooted) - offsetof(EEClient, mHasLeftGame) == 27,
              "gamecpp session_objs.rs EeGamecppClientTail");

#define CL ((EEClient*)p)

