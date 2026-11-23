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

// Creates the game client: no lobby slot yet, server migration allowed, fast selection reset, its lock created
EEClient::EEClient() : NESynchronousClient(GameHost<EEClient>)
{
    this->mUISlotNumber = 0xff;
    this->mHasLeftGame = false;
    this->mIsMigrating = false;
    this->mAllowServerMigration = true;
    this->mIgnoreConnectionTimeout = false;
    this->mIsServerLost = false;
    this->mHasLoaded = false;
    this->mServerLostAfterLoading = false;
    this->mWasBooted = false;
    this->ResetFastSelection();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mFastSelectionLock) == keseOK, 67);
}

// (included here, below the ctor: placed so that the ctor's ESVERIFY keeps line 67)
#include <list>
#include <string>
#include <mbstring.h>

#include "ee/EESaveGameFiles.h"
#include "ee/EEStringIDs.h"
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)

// Destroys the client, stopping its threads and releasing the fast-selection lock
EEClient::~EEClient()
{
    this->StopThreads();
    TSScheduler::DestroyCriticalSection(this->mFastSelectionLock);
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
    EESession(this, false, EE_NETMSG_SESS_DO_CONNECTION_STATE, inState);
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
    EESession(this, false, EE_NETMSG_SESS_DO_REMOVE, inPlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
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

// Clears the fast-selection state (no selection executing, id 0, empty table)
void EEClient::ResetFastSelection()
{
    this->mIsExecutingSelection = false;
    this->mUF2 = 0;
    this->mFastSelectionID = 0;
    for (long i = 0; i < 16; ++i)
        this->mFastSelection[i] = 0;
}

// Starts the game: the graphics, the physics loop, the main game UI; humans without a slot dropped
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    EESession(this, false, EE_NETMSG_SESS_DO_START_GAME);
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

int64_t EECore9OpD6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTree.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
#define FILESPEC ((FSFileSpec*)a[0])
#define WSTR(k) (*(UWideString*)a[k])

// the C++ calls of the client's bodies in Rust (gamecpp server.rs; EEServer.cpp's EECore9OpD2 default): one statement
// each
int64_t EECore9OpD3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::ClNewBootDefault: return P64(static_cast<EEAction*>(new EEABootClientFromCanceledGame));
    case O::ClNameLength: return WSTR(0).GetLength();
    case O::ClExtension: WSTR(1) = WSTR(0).GetIndexedSubstring(WSTR(0).GetLength() - 4, WSTR(0).GetLength());
        break;
    case O::ClExtIsSaved: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gSavedGameExtension)) == 0;
    case O::ClExtIsScenario: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gScenarioExtension)) == 0;
    case O::ClExtIsCampaign: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gCampaignExtension)) == 0;
    case O::ClNewFileSpec: return P64(new FSFileSpec);
    case O::ClFileSetVolume: FILESPEC->SetVolume(gStartupVolume);
        break;
    case O::ClFileSetDirectory: FILESPEC->SetDirectory(gStartupDirectory + "data\\" + (a[1] == 0 ? "Saved Games\\" : a[1] == 1 ? "Scenarios\\" : "Campaigns\\"));
        break;
    case O::ClFileSetFile: FILESPEC->SetFile((const char*)WSTR(1));
        break;
    case O::ClFileFree: delete FILESPEC;
        break;
    case O::ClFileOpenRead: return FILESPEC->Open(kFileAccessRead) == kErrorOK;
    case O::ClFileClose: FILESPEC->Close();
        break;
    case O::ClFileDeleteOnDisk: FILESPEC->Delete();
        break;
    case O::ClFileCreateWrite: return FILESPEC->Create() == kErrorOK && FILESPEC->Open(kFileAccessWrite) == kErrorOK;
    case O::ClHeaderLoad: ((EEPSaveGameHeader*)a[1])->LoadFromFile(*FILESPEC);
        break;
    case O::ClHeaderChecksum: return (long long)((EEPSaveGameHeader*)a[0])->mChecksum;
    case O::ClIdsNotResponding: return IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT;
    case O::ClIdsSeconds: return IDS_SECONDS;
    case O::ClTimeoutFormat: WSTR(0) = WSTR(1) + UWideString("  (%d ") + WSTR(2) + UWideString(")");
        break;
    case O::ClSprintfSeconds: sprintf(WSTR(0), WSTR(1), (unsigned long)a[2]);
        break;
    default: return EECore9OpD6(inOp, p, a, f);
    }
    return 0;
}
