// C:\EEX\Empire Earth\Communications\EEClient.cpp (__FILE__ 0x4ba13c): the game's lock-step client, 0xb71b5..0xb8cda
// (EEUnitCalamityClass.cpp ends at 0xb71b5; EEMessage.cpp starts at 0xb8cda). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEClient.h"

#include <list>
#include <string>
#include <mbstring.h>


// Boots this client from the canceled game when the host's slot (0) has become open, else refreshes the slot's line
void EEClient::HandleSlotUpdate(unsigned char inSlot, EEGameOptionsPlayer& inPlayer)
{
    ee_gamecpp_client_slot_update(inSlot, static_cast<EeAppOptionsPlayerFields*>(&inPlayer));
}

// Reports a connection state: 0 could not connect (back to the game list), 1 connecting, 4 connecting with retries
void EEClient::SetConnectionState(long inState)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_CONNECTION_STATE, inState, 0, 0, 0, NULL, NULL);
}

// A player's removal: the host's starts the server migration, ours cancels the game, another's is announced
void EEClient::ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_REMOVE, inPlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting, NULL, NULL);
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
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_CLIENT_MESSAGE, (long long)(uintptr_t)inMessage, 0, 0, 0, NULL, NULL);
}

// Stamps the local player's input with the fast-selection number; selection messages take a new one and run at once
void EEClient::HandleUserInput(NEMessage* inMessage)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_LOCAL_INPUT, (long long)(uintptr_t)inMessage, 0, 0, 0, NULL, NULL);
}

// The lost server: in a game the server migration, after loading the game cancelled, in the lobby the game list
void EEClient::HandleConnectionLost()
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_CONNECTION_LOST, 0, 0, 0, 0, NULL, NULL);
}

// A lobby message: player and slot info, transfer progress, the game start, a connection check, a joined player
void EEClient::HandleLobbyInput(NEMessage* inMessage)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_LOBBY_INPUT, (long long)(uintptr_t)inMessage, 0, 0, 0, NULL, NULL);
}

// Starts the game: the graphics, the physics loop, the main game UI; humans without a slot dropped
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_START_GAME, 0, 0, 0, 0, NULL, NULL);
    return 0;
}

// ------------------------------------------------------------------------- the client's plain tail

// the client's plain tail (gamecpp session_objs.rs EeGamecppClientTail): mHasLeftGame .. mWasBooted
static_assert(offsetof(EEClient, mUISlotNumber) - offsetof(EEClient, mHasLeftGame) == 1
                  && offsetof(EEClient, mUF2) - offsetof(EEClient, mHasLeftGame) == 18
                  && offsetof(EEClient, mIsExecutingSelection) - offsetof(EEClient, mHasLeftGame) == 20
                  && offsetof(EEClient, mWasBooted) - offsetof(EEClient, mHasLeftGame) == 27,
              "gamecpp session_objs.rs EeGamecppClientTail");
