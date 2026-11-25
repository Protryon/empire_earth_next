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

// Creates the game client: no lobby slot yet, server migration allowed, fast selection reset, its lock created
EEClient::EEClient() : NESynchronousClient(GameHost<EEClient>)
{
    ee_gamecpp_client_init(this);   // gamecpp session_objs.rs (the lock's ESVERIFY_AT keeps line 67: EECore11OpC3)
}

#include <list>
#include <string>
#include <mbstring.h>

#include "ee/EESaveGameFiles.h"
#include "ee/EEStringIDs.h"
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)

// Destroys the client, stopping its threads and releasing the fast-selection lock
EEClient::~EEClient()
{
    ee_gamecpp_client_drop(this);
}

// Boots this client from the canceled game when the host's slot (0) has become open, else refreshes the slot's line
void EEClient::HandleSlotUpdate(unsigned char inSlot, EEGameOptionsPlayer& inPlayer)
{
    ee_gamecpp_client_slot_update(inSlot, static_cast<EeAppOptionsPlayerFields*>(&inPlayer));
}

// Returns whether connection timeouts are ignored (set once this client is booted)
bool EEClient::IgnoreConnectionTimeout()
{
    return this->mIgnoreConnectionTimeout;
}

// Reports a connection state: 0 could not connect (back to the game list), 1 connecting, 4 connecting with retries
void EEClient::SetConnectionState(long inState)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_CONNECTION_STATE, inState, 0, 0, 0, NULL, NULL);
}

// Creates the local file for an incoming transfer in the directory of its extension (saved game, scenario, campaign)
// Returns NULL for a short name, an unknown extension or a file of the same size already there
// NEAR: SetFile takes the implicit UString from (const char*)inName, built in place; the original calls
// theFile->GetFile() at every exit and drops the result (a compiled-away trace; EE_LOST_TRACE does not make it
// exact, so it is not used); on the short-name exit theFile is NULL there, so the original crashes; exit tails differ
FSFile* EEClient::CreateXferFile(const UWideString& inName, unsigned long inSize)
{
    return (FSFile*)ee_gamecpp_client_xfer_file(&inName, inSize);
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

// Returns whether a server migration is in progress
bool EEClient::IsPlaybackRecordingSession()
{
    return this->mIsMigrating;
}

// Returns the connect timeout, kept in the global at 0x5308a0
unsigned long EEClient::GetConnectTimeout()
{
    return gNetworkAdapter;
}

// Returns the client's lobby slot (0xff until the server accepts it)
unsigned char EEClient::GetClientSlot()
{
    return this->mUISlotNumber;
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

// Clears the fast-selection state (no selection executing, id 0, empty table)
void EEClient::ResetFastSelection()
{
    ee_gamecpp_client_reset_fast(this);
}

// Starts the game: the graphics, the physics loop, the main game UI; humans without a slot dropped
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    ee_gamecpp_session(this, false, EE_NETMSG_SESS_DO_START_GAME, 0, 0, 0, 0, NULL, NULL);
    return 0;
}

// Shows the server migration's countdown in the chat log (host not responding, %d seconds)
void EEClient::HandleConnectionTimeoutWarning(unsigned long inRemaining)
{
    ee_gamecpp_client_timeout_warning(inRemaining);
}

void EEClient::HandleStopGame()
{
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

