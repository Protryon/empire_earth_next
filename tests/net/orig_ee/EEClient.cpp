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
#include "ee/EEABootClientFromCanceledGame.h"
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
#include "ee/EEAServerMigration.h"
#include "ee/EESaveGameFiles.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEStringIDs.h"
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)
void EELobbyPostConnectStatus(bool inConnected);  // 0x295ce3 (EELobbyInit.cpp; provisional name)

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
    UWideString theFormat;
    UWideString theText;
    switch (inState) {
    case 0:
        if (EEIsMultiplayerGame(true))
            EELobbyPostConnectStatus(false);
        else {
            gUserInterface->CloseForm(NULL);
            if (gWonNet != NULL && gUIManager->IsFormVisible(gUIManager->GetFormByID(kFormAvailableGames))) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            gResource->GetString(IDS_COULD_NOT_CONNECT_TO_SELECTED, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
        }
        break;
    case 1:
        if (!EEIsMultiplayerGame(true)) {
            gResource->GetString(IDS_CONNECTING_TO_SERVER, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, false);
        }
        break;
    case 4:
        gUserInterface->CloseForm(NULL);
        gResource->GetString(IDS_CONNECTING_TO_SERVER, theFormat);
        sprintf(theText, "%s Retry: %ld times", (const char*)theFormat,
                static_cast<NESynchronousClient*>(gCommunications)->GetNumConnectionTrys());
        gUIManager->DisplayMessage(theText, true, NULL, NULL, false);
        break;
    }
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

// Handles a player's removal: the host's starts the server migration, ours shows the booted or out-of-sync
// box and cancels the game, anyone else's is announced in the chat log and removed; plays the matching sound
void EEClient::ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theText;
    UWideString theFormat;
    UWideString theName;
    unsigned long theUnused = 0;
    unsigned long theSample;
    unsigned long theInstance;
    if (!this->mWasBooted && !this->mHasLeftGame) {
        if (inPlayer == kSlotServer) {
            if (inPlayerIsOOS) {
                gResource->GetString(IDS_HOST_IS_OUT_OF_SYNCH, theFormat);
                gResource->GetString(IDS_MSG_GAME, theName);
                sprintf(theText, theFormat, (const char*)gWorld.GetPlayerBySlot(0xfe)->mName);
                gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
            }
            if (!this->mAllowServerMigration)
                this->mIsServerLost = true;
            this->HandleConnectionLost();
        } else if (inPlayer == this->GetPlayerNumber()) {
            if (inGameIsExiting) {
                this->mHasLeftGame = true;
                CancelGameCallback(&theUnused);
            } else {
                if (inPlayerIsOOS)
                    gResource->GetString(IDS_YOU_ARE_OUT_OF_SYNCH, theFormat);
                else
                    gResource->GetString(IDS_YOU_HAVE_BEEN_BOOTED_DUE, theFormat);
                sprintf(theText, "%s", (const char*)theFormat);   // a resource text, not a format
                this->mIgnoreConnectionTimeout = true;
                this->mWasBooted = true;
                this->mHasLeftGame = true;
                gUIManager->DisplayMessage(theText, true, NULL, (EEUIFErrorCallback)CancelGameCallback, true);
            }
        } else {
            if (inPlayerIsOOS)
                gResource->GetString(IDS_PLAYER_IS_OUT_OF_SYNCH, theFormat);
            else
                gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
            sprintf(theText, theFormat, (const char*)gWorld.mPlayers[inPlayer]->mName);
            gResource->GetString(IDS_MSG_GAME, theName);
            gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
            gWorld.RemoveDisconnectedPlayer(inPlayer);
        }
        if (!inGameIsExiting) {
            if (inPlayerIsOOS)
                EELoadSound(UString("sounds\\OutOfSynch.wav"), 0x31, theSample, theInstance, 1);
            else
                EELoadSound(UString("sounds\\Booted.wav"), 0x31, theSample, theInstance, 1);
        }
    }
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

// Handles a server message: connection responses (lobby or an error box), reconnection, communications ticks whose
// player inputs go to the physics queue (or the lobby before the game); load/save and lag messages execute at once
// NEAR: the dead reads of mNumberOfCTsProcessedInPhysics are kept through dead locals (as EEServer 0x4cbc77)
void EEClient::HandleMessage(NEMessage* inMessage)
{
    UWideString theText;
    UWideString theFormat;
    EEGameOptionsPlayer thePlayer;
    unsigned long theCount = 0;

    switch (inMessage->GetType()) {
    case kNetMessageClientReconnectionRequestResponse: {
        unsigned long theTick = static_cast<NEMRecord*>(inMessage)->Get<uint32_t>(2);   // error, new slot, tick
        gCommunications->SetNumberOfCTsProcessedInPhysics(theTick);
        gCommunications->SetCurrentCommunicationsTick(theTick);
        unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();   // the original reads it (0xb7a2a) and drops it
        gCommunications->GetCurrentCommunicationsTick();
        break;
    }
    case kNetMessageCommunicationsTick: {
        // the tick, its PTs to perform, PTs per second, checksum and dump flags, the players' inputs
        NEMRecord* theTickMessage = static_cast<NEMRecord*>(inMessage);
        NEMessage* theInput;
        unsigned char theTicksToPerform = theTickMessage->Get<unsigned char>(1);
        unsigned long theNumberOfInputs = theTickMessage->GetNumberOfPlayerInputs();
        for (unsigned long i = 0; i < theNumberOfInputs; i++) {
            theTickMessage->GetPlayerInput(i, theInput);
            if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == this->GetMySendToSlot())
                theCount++;
        }
        if (theTicksToPerform != 0xff) {
            unsigned long thePhysicsTick = this->GetCurrentPhysicsTick() + theTicksToPerform;
            this->SetCurrentPhysicsTick(thePhysicsTick);
            NEMessage* thePhysicsTickMessage = new NEMRecord(kNetMessagePhysicsTick,
                {theCount, theTicksToPerform, theTickMessage->Get<bool>(3), theTickMessage->Get<bool>(4),
                 theTickMessage->Get<unsigned char>(2)});
            gPhysics->mServerMessageQueueLock->Enter();
            gPhysics->QueueServerMessage(thePhysicsTickMessage);
            for (unsigned long j = 0; j < theNumberOfInputs; j++) {
                theTickMessage->GetPlayerInput(j, theInput);
                if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == this->GetMySendToSlot()) {
                    theInput->Reference();
                    gPhysics->QueueServerMessage(theInput);
                }
            }
            gPhysics->mServerMessageQueueLock->Leave();
        } else {
            gPhysics->mServerMessageQueueLock->Enter();
            unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();
            gCommunications->IncrementNumberOfCTsProcessedInPhysics();
            gPhysics->mServerMessageQueueLock->Leave();
            for (unsigned long k = 0; k < theNumberOfInputs; k++) {
                theTickMessage->GetPlayerInput(k, theInput);
                if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == this->GetMySendToSlot())
                    this->HandleLobbyInput(theInput);
            }
        }
        break;
    }
    case kNetMessageClientConnectionRequestResponse: {
        // the error, the lobby slot, the send-to slot, the tick, the version
        NEMRecord* theResponse = static_cast<NEMRecord*>(inMessage);
        long theError = theResponse->Get<int32_t>(0);
        // leaving the game: back to the game list, then the form closed and the reason shown (formatted with the
        // host's version and ours), or the boot action posted with it, or nothing; then disconnected
        enum { kLeaveShow, kLeaveShowVersions, kLeaveBoot, kLeaveQuiet };
        auto theLeave = [&](long inText, int inHow) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            if (inHow == kLeaveBoot) {
                EEAction* theAction = new EEABootClientFromCanceledGame(inText);
                gUserInterface->PostAction(theAction);
                theAction->Dereference();
            } else if (inHow != kLeaveQuiet) {
                gUserInterface->CloseForm(NULL);
                gResource->GetString(inText, theFormat);
                if (inHow == kLeaveShowVersions)
                    sprintf(theText, theFormat, (const char*)theResponse->GetString(4), (const char*)gGameVersion);
                gUIManager->DisplayMessage(inHow == kLeaveShowVersions ? theText : theFormat, true, NULL, NULL, true);
            }
            this->Disconnect();
        };
        if (theError == kConnectionAccepted) {
            this->mUISlotNumber = theResponse->Get<unsigned char>(1);
            gLocalPlayerOut = false;
            if (EEIsMultiplayerGame(true))
                EELobbyPostConnectStatus(true);
            else
                gUserInterface->OpenForm(gUIManager->GetFormByID(kFormLobby));
            gCommunications->SetNumberOfCTsProcessedInPhysics(theResponse->Get<uint32_t>(3));
            gCommunications->SetCurrentCommunicationsTick(theResponse->Get<uint32_t>(3));
            unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();
        } else if (this->mUISlotNumber == 0xff) {
            // refused while joining: the lobby is told, or the reason shown (a version mismatch with the host's
            // version and ours; the game in progress also under the code the host sends, a fix: the original had no
            // case for either while joining and left the player waiting)
            if (EEIsMultiplayerGame(true))
                EELobbyPostConnectStatus(false);
            else if (theError == kConnectionNameInUse)
                theLeave(IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME, kLeaveShow);
            else if (theError == kConnectionBadChecksum)
                theLeave(IDS_ONE_OR_MORE_OF_YOUR_FILES, kLeaveShow);
            else if (theError == kConnectionGameFull)
                theLeave(IDS_NO_OPEN_SLOTS_LEFT_IN_GAME, kLeaveShow);
            else if (theError == kConnectionVersionMismatch)
                theLeave(IDS_TO_JOIN_THIS_GAME_YOU, kLeaveShowVersions);
            else if (theError == kConnectionGameInProgress || theError == kConnectionGameInProgressAsSent)
                theLeave(IDS_GAME_IS_IN_PROGRESS, kLeaveShow);
        } else if (theError == kConnectionRemovedByHost) {
            theLeave(IDS_SERVER_HAS_REMOVED_YOU_FROM_GAME, kLeaveBoot);
        } else if (theError == kConnectionHostGone) {
            theLeave(IDS_HOST_OF_THIS_GAME_HAS_GONE, kLeaveBoot);
        } else if (theError == kConnectionGameInProgress) {
            theLeave(IDS_GAME_IS_IN_PROGRESS, kLeaveShowVersions);
        } else if (theError == 6) {
            theLeave(0, kLeaveQuiet);
        }
        break;
    }
    case kMessageNewEdictEnacted:
        break;
    case kMessageLoadOrSaveGame:
    case kMessageLagStatus:
        inMessage->Execute();
        inMessage->Dereference();
        break;
    case kMessageToggleDayNight:
    case kMessageSnapToDaylight:
        inMessage->Execute();
        break;
    }
}

// Stamps the local player's input with the fast-selection number; selection messages take a new number and run at once
void EEClient::HandleUserInput(NEMessage* inMessage)
{
    switch (inMessage->GetType()) {
    case kMessageSelectUnit:
    case kMessageAddUnitToSelectedGroup:
    case kMessageRemoveUnitFromSelectedGroup:
    case kMessageUnselectAllUnits:
    case kMessageSelectUnits:
    case kMessageAddUnitsToSelectedGroup:
    case kMessageAddAndRemoveUnitsFromSelectedGroup:
    case kMessageAlwaysAddUnitToSelectedGroup:
    case kMessageSavedGroupAddSelection:
    case kMessageSavedGroupSelect:
    case kMessageSelectTheseUnits:
        if (!this->mIsMigrating) {
            this->mFastSelectionLock->Enter();
            unsigned char theID = this->mFastSelectionID + 1;
            this->mFastSelectionID = theID;
            static_cast<EEMessage*>(inMessage)->mFastSelectionID = theID;
            this->mIsExecutingSelection = true;
            gWorld.GetCurrentPlayer()->ExecuteSelectionMessage(inMessage);
            this->mIsExecutingSelection = false;
            this->mFastSelectionLock->Leave();
        }
        break;
    default: {
        EEMessage* theMessage = EEGameMessageCast(inMessage);
        if (theMessage != NULL) {
            this->mFastSelectionLock->Enter();
            theMessage->mFastSelectionID = this->mFastSelectionID;
            this->mFastSelectionLock->Leave();
        }
        break;
    }
    }
}

// Handles the lost server: in a game starts the server migration (if allowed); after loading cancels the game;
// in the lobby goes back to the game list
void EEClient::HandleConnectionLost()
{
    UWideString theText;
    if (this->GetCurrentPhysicsTick() > 0) {
        if (this->mAllowServerMigration) {
            gResource->GetString(IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT, theText);
            gUserInterface->AddChatLogEntry(theText, 1);
            PlatSleep(500);
            EEAction* theAction = new EEAServerMigration;
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
    } else if (!this->mHasLeftGame) {
        if (this->mHasLoaded) {
            this->mServerLostAfterLoading = true;
            this->mHasLeftGame = true;
            gUserInterface->PostCancelGame();
        } else if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormLobby)) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            EEAction* theAction = new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
            this->Disconnect();
        }
    }
}

// Handles a lobby message: player and slot info, transfer progress, the game start, a connection check (answered),
// a joined player; game options and player info from others and the rest execute
// NEAR: frame layout (this spilled, the 0x2d loop in memory), the 0x29 response ctor called out of line (the
// original expands it), the 0x6a block after the epilogue
void EEClient::HandleLobbyInput(NEMessage* inMessage)
{
    UWideString theText;
    EEGameOptionsPlayer thePlayer;
    UWideString theUnused1;
    UWideString theUnused2;
    switch (inMessage->GetType()) {
    case kMessageFullPlayerInformation:
        if (inMessage->GetOriginatingSlot() != this->GetMySendToSlot()) {
            EEMRecord* theInfo = static_cast<EEMRecord*>(inMessage);
            // the record, in wire order: slot 1, type 2, loaded type 3, the name (mString), ping 5, colour 6, team 7,
            // citizens 8, ready 9, local 10, mB42 11, engine 12
            thePlayer.mColor = theInfo->Get<int32_t>(6);
            thePlayer.mType = theInfo->Get<int32_t>(2);
            thePlayer.mTeam = theInfo->Get<int32_t>(7);
            thePlayer.mName = theInfo->mString;
            thePlayer.mPing = theInfo->Get<float>(5);
            thePlayer.mLoadedType = theInfo->Get<int32_t>(3);
            thePlayer.mStartingCitizens = theInfo->Get<int32_t>(8);
            thePlayer.mEngine = (unsigned long)theInfo->Get<int32_t>(12) < EECPEngineChoiceCount() ? (unsigned long)theInfo->Get<int32_t>(12)
                                                                                       : (unsigned long)kEECPChoiceClassic;   // port
            thePlayer.mB36 = theInfo->Get<bool>(11);
            thePlayer.mIsReady = theInfo->Get<bool>(9);
            thePlayer.mSlot = theInfo->Get<int32_t>(1);
            thePlayer.mIsLocal = theInfo->Get<int32_t>(1) == this->mUISlotNumber;
            gGameOptions.mLock->WriteLock();
            gGameOptions.mPlayers[thePlayer.mSlot] = thePlayer;
            gGameOptions.mLock->WriteUnlock();
            this->HandleSlotUpdate(theInfo->Get<int32_t>(1), thePlayer);
        }
        break;
    case kNetMessageXferStatus: {
        // the client's slot, the percent done, the transfer's id, the status
        NEMRecord* theStatus = static_cast<NEMRecord*>(inMessage);
        gUserInterface->PostLobbyShowTransferProgress(theStatus->Get<unsigned char>(0), theStatus->Get<unsigned char>(1));
        if (theStatus->Get<unsigned char>(3) == 4) {
            gResource->GetString(IDS_FILE_TRANSFER_FAILED, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
        }
        break;
    }
    case kNetMessageStartGame: {
        gStartTime = static_cast<NEMRecord*>(inMessage)->Get<uint32_t>(0);   // the offset start time
        long theSeed = gPhysics->GetRandom(0L, 0x7fffffffL);
        theSeed += gWorld.mNumPlayers;
        for (unsigned long i = 0; i < gWorld.mNumPlayers; i++) {
            theText = gWorld.mPlayers[i]->mName;
            for (int j = 0; j < theText.GetLength(); j++)
                theSeed += theText[j];
        }
        gWorld.GetNumTeams();
        NEProtocol* theProtocol;
        NEProtocol::GetProtocol(0, theProtocol);
        theProtocol->GetStatus("", 0xffff0000);
        gCommunications->SetIsMultiplayerSession(true);
        this->HandleStartGame(0);
        break;
    }
    case kNetMessagePlayerConnectionCheck: {
        NEMessage* theResponse = new NEMRecord(kNetMessagePlayerConnectionCheckResponse,
                                               {static_cast<NEMRecord*>(inMessage)->Get<uint32_t>(0)});   // the ping's start
        this->ProcessLocalUserInput(theResponse, kSlotNone);
        break;
    }
    case kNetMessageClientConnectionRequestResponse:
        if (static_cast<NEMRecord*>(inMessage)->Get<int32_t>(0) == kConnectionPlayerJoined) {   // the error
            gUserInterface->PostLobbyUpdateForm();
            if (gWonNet != NULL)
                gWonNet->PlayerJoined();
        }
        break;
    case kMessageGameOptions:
    case kMessagePlayerInformation:
        if (inMessage->GetOriginatingSlot() == this->GetMySendToSlot())
            break;
        inMessage->Execute();
        break;
    case kMessagePlayerSlotInfo: {
        EEMRecord* theInfo = static_cast<EEMRecord*>(inMessage);
        gWorld.mLock->WriteLock();
        EEPlayer* thePlayer = gWorld.mPlayers[theInfo->Get<unsigned char>(1)];   // the player, slot, firewall, address
        if (thePlayer != NULL) {
            thePlayer->mSlot = theInfo->Get<unsigned char>(2);
            if (thePlayer->IsHuman()) {
                static_cast<EEHumanPlayer*>(thePlayer)->SetName(theInfo->mString);
                thePlayer->mBehindFirewall = theInfo->Get<bool>(3);
            }
        }
        gWorld.mLock->WriteUnlock();
        break;
    }
    case kMessageStartLoading:
        inMessage->Execute();
        break;
    case kMessageUpdatePingStat:
        inMessage->Execute();
        break;
    default:
        inMessage->Execute();
        break;
    }
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

// Starts the game: stops the graphics, starts the physics loop, opens the main game UI, drops humans without a slot
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theName;
    UWideString theText;
    UWideString theFormat;
    gGraphics->PostResume();
    for (bool theStopping = gGraphics->mIsPaused; theStopping; theStopping = gGraphics->mIsPaused)
        PlatSleep(1);
    gPhysics->mThread->Post(0x404, NULL, NULL, true);
    while (true) {
        if (gPhysics->mTimerRunning)
            break;
        PlatSleep(1);
    }
    gUIManager->mMainGameForm->SetUIState(0);
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (!thePlayer->IsHuman() || thePlayer->mSlot != kSlotNone)
            continue;
        gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
        sprintf(theText, theFormat, (const char*)thePlayer->mName);
        gResource->GetString(IDS_MSG_GAME, theName);
        gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        gWorld.RemoveDisconnectedPlayer(i);
    }
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
