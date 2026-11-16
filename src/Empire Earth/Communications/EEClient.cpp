// C:\EEX\Empire Earth\Communications\EEClient.cpp (__FILE__ 0x4ba13c): the game's lock-step client, 0xb71b5..0xb8cda
// (EEUnitCalamityClass.cpp ends at 0xb71b5; EEMessage.cpp starts at 0xb8cda). Unit flags: the exe's module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/U2DSparseArray.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UResourceFile.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EEUnitKind.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPEngine.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEClient.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEServer.h"
#include "rust/ee_netmsg.h"   // ee_netmsg session.rs (EESession: EEServer.cpp's host)

// 0x4e6058: default-constructed (its init chain 0xb71b5 opens the unit; use inferred, provisional name)
UWideString gU4E6058;

// Creates the game client: no lobby slot yet, server migration allowed, fast selection reset, its lock created
EEClient::EEClient()
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

#include "ee/EEWonNet.h"
#include "ee/EESaveGameFiles.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEHumanPlayer.h"
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
    if (inPlayer.mSlot == 0 && inPlayer.mType == 3) {
        EEAction* theAction = new EEABootClientFromCanceledGame;
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    } else
        gUserInterface->PostLobbyUpdatePlayer(inSlot, 8);
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
    EEPSaveGameHeader theHeader;
    UWideString theExtension;
    FSFileSpec* theFile = NULL;
    bool theOK;
    if (inName.GetLength() >= 5) {
        theExtension = inName.GetIndexedSubstring(inName.GetLength() - 4, inName.GetLength());
        theFile = new FSFileSpec;
        theFile->SetVolume(gStartupVolume);
        if (PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
                     reinterpret_cast<const unsigned char*>((const char*)gSavedGameExtension)) == 0)
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
        else if (PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
                          reinterpret_cast<const unsigned char*>((const char*)gScenarioExtension)) == 0)
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        else if (PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
                          reinterpret_cast<const unsigned char*>((const char*)gCampaignExtension)) == 0)
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
        else {
            delete theFile;
            return NULL;
        }
        theFile->SetFile((const char*)inName);

        if (theFile->Open(kFileAccessRead) == kErrorOK) {
            theFile->Close();
            theHeader.SetMajorVersion(kSaveGameMajorVersion);
            theHeader.SetMinorVersion(kSaveGameMinorVersion);
            theHeader.LoadFromFile(*theFile);
            unsigned long theSize = theHeader.mChecksum;

            if (theSize == inSize)
                theOK = false;
            else {
                theFile->Delete();
                // the re-created file must be created AND opened for writing, as in the no-file branch below
                theOK = theFile->Create() == kErrorOK && theFile->Open(kFileAccessWrite) == kErrorOK;
            }
        } else {
            theFile->Delete();
            theOK = theFile->Create() == kErrorOK && theFile->Open(kFileAccessWrite) == kErrorOK;
        }

        if (!theOK) {
            delete theFile;
            theFile = NULL;
        }
    }

    return theFile;
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
    UWideString theText;
    UWideString theSeconds;
    UWideString theFormat;
    gResource->GetString(IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT, theText);
    gResource->GetString(IDS_SECONDS, theSeconds);
    theFormat = theText + UWideString("  (%d ") + theSeconds + UWideString(")");
    sprintf(theText, theFormat, inRemaining / 1000);
    gUserInterface->AddChatLogEntry(theText, 1);
}

void EEClient::HandleStopGame()
{
}

bool EEClient::IsClient()
{
    return true;
}

bool EEClient::IsServer()
{
    return false;
}
