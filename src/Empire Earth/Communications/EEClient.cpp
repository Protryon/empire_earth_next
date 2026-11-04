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
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEClient.h"

// 0x4e6058: default-constructed (its init chain 0xb71b5 opens the unit; use inferred, provisional name)
UWideString gU4E6058;

// SYNTHETIC: EXE 0x4b71b5
// SYNTHETIC: EXE 0x4b71bf
// SYNTHETIC: EXE 0x4b71ca
// SYNTHETIC: EXE 0x4b71d6

// FUNCTION: EXE 0x4b71e1
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

// SYNTHETIC: EXE 0x4b725c
// (the scalar deleting destructor, emitted with the vtable)

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
#include "ee/EEFormation.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGUnloadTransport.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectManager.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "LostDebug.h"                      // NOT original source: EE_LOST_DEBUG stand-in (0x4b7722)
#include "ee/EEStringIDs.h"
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)
void EELobbyFn295CE3(bool inFlag);          // 0x295ce3 (EELobbyInit.cpp; provisional name)

// FUNCTION: EXE 0x4b7278
// Destroys the client, stopping its threads and releasing the fast-selection lock
EEClient::~EEClient()
{
    this->StopThreads();
    TSScheduler::DestroyCriticalSection(this->mFastSelectionLock);
}

// FUNCTION: EXE 0x4b729e
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

// FUNCTION: EXE 0x4b7300
// Returns whether connection timeouts are ignored (set once this client is booted)
bool EEClient::IgnoreConnectionTimeout()
{
    return this->mIgnoreConnectionTimeout;
}

// FUNCTION: EXE 0x4b7307
// Reports a connection state: 0 could not connect (back to the game list), 1 connecting, 4 connecting with retries
void EEClient::SetConnectionState(long inState)
{
    UWideString theFormat;
    UWideString theText;
    switch (inState) {
    case 0:
        if (EECheck2989A9(true))
            EELobbyFn295CE3(false);
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
        if (!EECheck2989A9(true)) {
            gResource->GetString(IDS_CONNECTING_TO_SERVER, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, false);
        }
        break;
    case 4:
        gUserInterface->CloseForm(NULL);
        gResource->GetString(IDS_CONNECTING_TO_SERVER, theFormat);
        sprintf(theText, "%s Retry: %d times", (const char*)theFormat,
                static_cast<NESynchronousClient*>(gCommunications)->GetNumConnectionTrys());
        gUIManager->DisplayMessage(theText, true, NULL, NULL, false);
        break;
    }
}

// FUNCTION: EXE 0x4b743b
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
        if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
                     reinterpret_cast<const unsigned char*>((const char*)gSavedGameExtension)) == 0)
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
        else if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
                          reinterpret_cast<const unsigned char*>((const char*)gScenarioExtension)) == 0)
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        else if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)theExtension),
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

// SYNTHETIC: EXE 0x4b76c0
// (EEPSaveGameHeader's implicit dtor, out of line after its first user)

// SYNTHETIC: EXE 0x4b7705
// (FSFileSpec's ??_G: the exe's copy of the dllimport class's vtable)

// FUNCTION: EXE 0x4b7722
// Handles a player's removal: the host's starts the server migration, ours shows the booted or out-of-sync
// box and cancels the game, anyone else's is announced in the chat log and removed; plays the matching sound
// LOST-DEBUG: none: exact only (tools lostdebug_search)
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
                gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(25) * 2, 0);
            }
            if (!this->mAllowServerMigration)
                this->mIsServerLost = true;
            this->HandleConnectionLost();
        } else if (inPlayer == this->GetPlayerNumber()) {
            if (inGameIsExiting) {
                this->mHasLeftGame = true;
                FnB7992(&theUnused);
            } else {
                if (inPlayerIsOOS)
                    gResource->GetString(IDS_YOU_ARE_OUT_OF_SYNCH, theFormat);
                else
                    gResource->GetString(IDS_YOU_HAVE_BEEN_BOOTED_DUE, theFormat);
                sprintf(theText, theFormat);
                this->mIgnoreConnectionTimeout = true;
                this->mWasBooted = true;
                this->mHasLeftGame = true;
                gUIManager->DisplayMessage(theText, true, NULL, (EEUIFErrorCallback)FnB7992, true);
            }
        } else {
            if (inPlayerIsOOS)
                gResource->GetString(IDS_PLAYER_IS_OUT_OF_SYNCH, theFormat);
            else
                gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
            sprintf(theText, theFormat, (const char*)gWorld.mPlayers[inPlayer]->mName);
            gResource->GetString(IDS_MSG_GAME, theName);
            gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(25) * 2, 0);
            gWorld.RemoveDisconnectedPlayer(inPlayer);
        }
        if (!inGameIsExiting) {
            if (inPlayerIsOOS)
                EELoadSound(UString("sounds\\OutOfSynch.wav"), 0x31, theSample, theInstance, 1);
            else
                EELoadSound(UString("sounds\\Booted.wav"), 0x31, theSample, theInstance, 1);
        }
    }
    EE_LOST_DEBUG(3);
}

// FUNCTION: EXE 0x4b7992
// Cancels the game (the callback of the booted message box)
void FnB7992(void* inData)
{
    gUserInterface->PostCancelGame();
}

// FUNCTION: EXE 0x4b799d
// Returns whether a server migration is in progress
bool EEClient::IsPlaybackRecordingSession()
{
    return this->mIsMigrating;
}

// FUNCTION: EXE 0x4b79a4
// Returns the connect timeout, kept in the global at 0x5308a0
unsigned long EEClient::GetConnectTimeout()
{
    return gU5308A0;
}

// FUNCTION: EXE 0x4b79aa
// Returns the client's lobby slot (0xff until the server accepts it)
unsigned char EEClient::GetClientSlot()
{
    return this->mUISlotNumber;
}

// FUNCTION: EXE 0x4b79b1
// Handles a server message: connection responses (lobby or an error box), reconnection, communications ticks whose
// player inputs go to the physics queue (or the lobby before the game); load/save and lag messages execute at once
// NEAR: the dead reads of mNumberOfCTsProcessedInPhysics are kept through dead locals (as EEServer 0x4cbc77);
// case 0x43 keeps its compare in the original (EE_LOST_CASE does not make it exact, so it is not applied); the
// mNoPop store is sunk into the shared tail in the original
void EEClient::HandleMessage(NEMessage* inMessage)
{
    UWideString theText;
    UWideString theFormat;
    EEGameOptionsPlayer thePlayer;
    unsigned long theCount = 0;

    switch (inMessage->GetType()) {
    case kNetMessageClientReconnectionRequestResponse: {
        NEMClientReconnectionRequestResponse* theResponse = static_cast<NEMClientReconnectionRequestResponse*>(inMessage);
        gCommunications->SetNumberOfCTsProcessedInPhysics(theResponse->GetCurrentCT());
        gCommunications->SetCurrentCommunicationsTick(theResponse->GetCurrentCT());
        unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();   // the original reads it (0xb7a2a) and drops it
        gCommunications->GetCurrentCommunicationsTick();
        break;
    }
    case kNetMessageCommunicationsTick: {
        NEMCommunicationsTick* theTickMessage = static_cast<NEMCommunicationsTick*>(inMessage);
        NEMessage* theInput;
        unsigned char theTicksToPerform = theTickMessage->GetPhysicsTicksToPerform();
        unsigned long theNumberOfInputs = theTickMessage->GetNumberOfPlayerInputs();
        for (unsigned long i = 0; i < theNumberOfInputs; i++) {
            theTickMessage->GetPlayerInput(i, theInput);
            if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == this->GetMySendToSlot())
                theCount++;
        }
        if (theTicksToPerform != 0xff) {
            unsigned long thePhysicsTick = this->GetCurrentPhysicsTick() + theTicksToPerform;
            this->SetCurrentPhysicsTick(thePhysicsTick);
            unsigned char thePhysicsTicksPerSecond = theTickMessage->GetPhysicsTicksPerSecond();
            NEMPhysicsTick* thePhysicsTickMessage = new NEMPhysicsTick(theTicksToPerform, theCount,
                thePhysicsTicksPerSecond, theTickMessage->GetGenerateChecksum(), theTickMessage->GetDumpTheWorld());
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
        NEMClientConnectionRequestResponse* theResponse = static_cast<NEMClientConnectionRequestResponse*>(inMessage);
        if (theResponse->GetError() == kConnectionAccepted) {
            this->mUISlotNumber = theResponse->GetUISlotNumber();
            gLocalPlayerOut = false;
            if (EECheck2989A9(true))
                EELobbyFn295CE3(true);
            else
                gUserInterface->OpenForm(gUIManager->GetFormByID(kFormLobby));
            gCommunications->SetNumberOfCTsProcessedInPhysics(theResponse->GetCurrentCT());
            gCommunications->SetCurrentCommunicationsTick(theResponse->GetCurrentCT());
            unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();
        } else if (this->mUISlotNumber == 0xff) {
            if (EECheck2989A9(true))
                EELobbyFn295CE3(false);
            else if (theResponse->GetError() == kConnectionNameInUse) {
                if (gWonNet != NULL) {
                    gWonNet->ClearGameList();
                    gWonNet->RejoinRoom(false);
                }
                gUserInterface->CloseForm(NULL);
                gResource->GetString(IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME, theFormat);
                gUIManager->DisplayMessage(theFormat, true, NULL, NULL, true);
                this->Disconnect();
            } else if (theResponse->GetError() == kConnectionBadChecksum) {
                if (gWonNet != NULL) {
                    gWonNet->ClearGameList();
                    gWonNet->RejoinRoom(false);
                }
                gUserInterface->CloseForm(NULL);
                gResource->GetString(IDS_ONE_OR_MORE_OF_YOUR_FILES, theFormat);
                gUIManager->DisplayMessage(theFormat, true, NULL, NULL, true);
                this->Disconnect();
            } else if (theResponse->GetError() == kConnectionGameFull) {
                if (gWonNet != NULL) {
                    gWonNet->ClearGameList();
                    gWonNet->RejoinRoom(false);
                }
                gUserInterface->CloseForm(NULL);
                gResource->GetString(IDS_NO_OPEN_SLOTS_LEFT_IN_GAME, theFormat);
                gUIManager->DisplayMessage(theFormat, true, NULL, NULL, true);
                this->Disconnect();
            } else if (theResponse->GetError() == kConnectionVersionMismatch) {
                if (gWonNet != NULL) {
                    gWonNet->ClearGameList();
                    gWonNet->RejoinRoom(false);
                }
                gUserInterface->CloseForm(NULL);
                gResource->GetString(IDS_TO_JOIN_THIS_GAME_YOU, theFormat);
                sprintf(theText, theFormat, (const char*)theResponse->GetVersion(), (const char*)gGameVersion);
                gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
                this->Disconnect();
            } else if (theResponse->GetError() == kConnectionGameInProgress
                       || theResponse->GetError() == kConnectionGameInProgressAsSent) {
                // Fix: a host refuses a join into a running game with the code kConnectionGameInProgressAsSent (the
                // message's string id) instead of kConnectionGameInProgress, and this joining branch had no case
                // for either, so the joining player was never told and kept waiting. The host is left unchanged
                // (it must stay compatible with other hosts); the joining player now gets the message.
                if (gWonNet != NULL) {
                    gWonNet->ClearGameList();
                    gWonNet->RejoinRoom(false);
                }
                gUserInterface->CloseForm(NULL);
                gResource->GetString(IDS_GAME_IS_IN_PROGRESS, theFormat);
                gUIManager->DisplayMessage(theFormat, true, NULL, NULL, true);
                this->Disconnect();
            }
        } else if (theResponse->GetError() == kConnectionRemovedByHost) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            EEAction* theAction = new EEABootClientFromCanceledGame(IDS_SERVER_HAS_REMOVED_YOU_FROM_GAME);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
            this->Disconnect();
        } else if (theResponse->GetError() == kConnectionHostGone) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            EEAction* theAction = new EEABootClientFromCanceledGame(IDS_HOST_OF_THIS_GAME_HAS_GONE);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
            this->Disconnect();
        } else if (theResponse->GetError() == kConnectionGameInProgress) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            gUserInterface->CloseForm(NULL);
            gResource->GetString(IDS_GAME_IS_IN_PROGRESS, theFormat);
            sprintf(theText, theFormat, (const char*)theResponse->GetVersion(), (const char*)gGameVersion);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
            this->Disconnect();
        } else if (theResponse->GetError() == 6) {
            if (gWonNet != NULL) {
                gWonNet->ClearGameList();
                gWonNet->RejoinRoom(false);
            }
            this->Disconnect();
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

// FUNCTION: EXE 0x4b7ed4
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
        EEMessage* theMessage = dynamic_cast<EEMessage*>(inMessage);
        if (theMessage != NULL) {
            this->mFastSelectionLock->Enter();
            theMessage->mFastSelectionID = this->mFastSelectionID;
            this->mFastSelectionLock->Leave();
        }
        break;
    }
    }
}

// FUNCTION: EXE 0x4b7fa5
// Handles the lost server: in a game starts the server migration (if allowed); after loading cancels the game;
// in the lobby goes back to the game list
void EEClient::HandleConnectionLost()
{
    UWideString theText;
    if (this->GetCurrentPhysicsTick() > 0) {
        if (this->mAllowServerMigration) {
            gResource->GetString(IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT, theText);
            gUserInterface->AddChatLogEntry(theText, 1);
            Sleep(500);
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

// FUNCTION: EXE 0x4b80ef
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
            EEMFullPlayerInformation* theInfo = static_cast<EEMFullPlayerInformation*>(inMessage);
            thePlayer.mColor = theInfo->mColor;
            thePlayer.mType = theInfo->mSlotType;
            thePlayer.mTeam = theInfo->mTeam;
            thePlayer.mName = theInfo->mName;
            thePlayer.mPing = theInfo->mPing;
            thePlayer.mLoadedType = theInfo->mLoadedType;
            thePlayer.mStartingCitizens = theInfo->mCitizenBonus;
            thePlayer.mB36 = theInfo->mB42;
            thePlayer.mIsReady = theInfo->mIsReady;
            thePlayer.mSlot = theInfo->mSlot;
            thePlayer.mIsLocal = theInfo->mSlot == this->mUISlotNumber;
            gGameOptions.mLock->WriteLock();
            gGameOptions.mPlayers[thePlayer.mSlot] = thePlayer;
            gGameOptions.mLock->WriteUnlock();
            this->HandleSlotUpdate(theInfo->mSlot, thePlayer);
        }
        break;
    case kNetMessageXferStatus: {
        NEMXferStatus* theStatus = static_cast<NEMXferStatus*>(inMessage);
        gUserInterface->PostLobbyShowTransferProgress(theStatus->GetClientSlot(), theStatus->GetPercentDone());
        if (theStatus->GetStatusID() == 4) {
            gResource->GetString(IDS_FILE_TRANSFER_FAILED, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
        }
        break;
    }
    case kNetMessageStartGame: {
        gStartTime = static_cast<NEMStartGame*>(inMessage)->GetOffsetStartTime();
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
        NEMessage* theResponse = new NEMPlayerConnectionCheckResponse(
            static_cast<NEMPlayerConnectionCheck*>(inMessage)->GetPingTimeStart());
        this->ProcessLocalUserInput(theResponse, kSlotNone);
        break;
    }
    case kNetMessageClientConnectionRequestResponse:
        if (static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetError() == kConnectionPlayerJoined) {
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
        EEMPlayerSlotInfo* theInfo = static_cast<EEMPlayerSlotInfo*>(inMessage);
        gWorld.mLock->WriteLock();
        EEPlayer* thePlayer = gWorld.mPlayers[theInfo->mPlayerID];
        if (thePlayer != NULL) {
            thePlayer->mSlot = theInfo->mSlot;
            if (thePlayer->IsHuman()) {
                static_cast<EEHumanPlayer*>(thePlayer)->SetName(theInfo->mAddress);
                thePlayer->mBehindFirewall = theInfo->mIsBehindFirewall;
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

// FUNCTION: EXE 0x4b8418
// Clears the fast-selection state (no selection executing, id 0, empty table)
void EEClient::ResetFastSelection()
{
    this->mIsExecutingSelection = false;
    this->mUF2 = 0;
    this->mFastSelectionID = 0;
    for (long i = 0; i < 16; ++i)
        this->mFastSelection[i] = 0;
}

// FUNCTION: EXE 0x4b843b
// Starts the game: stops the graphics, starts the physics loop, opens the main game UI, drops humans without a slot
unsigned long EEClient::HandleStartGame(unsigned long inParameter)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theName;
    UWideString theText;
    UWideString theFormat;
    gGraphics->PostResume();
    for (bool theStopping = gGraphics->mIsPaused; theStopping; theStopping = gGraphics->mIsPaused)
        Sleep(1);
    gPhysics->mThread->Post(0x404, NULL, NULL, true);
    while (true) {
        if (gPhysics->mTimerRunning)
            break;
        Sleep(1);
    }
    gUIManager->mMainGameForm->SetUIState(0);
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (!thePlayer->IsHuman() || thePlayer->mSlot != kSlotNone)
            continue;
        gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
        sprintf(theText, theFormat, (const char*)thePlayer->mName);
        gResource->GetString(IDS_MSG_GAME, theName);
        gUserInterface->AddChatLogEntry(theColor, theName, theText, gWorldTable.GetLongValue(25) * 2, 0);
        gWorld.RemoveDisconnectedPlayer(i);
    }
    return 0;
}

// FUNCTION: EXE 0x4b8599
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

// SYNTHETIC: EXE 0x4b869e
// (EESMMediaStrings' implicit dtor)

// FUNCTION: EXE 0x4b86cd
// Shows the local command markers of a group order (move, attack, target, unload) on the goal the units just
// queued, else on the first unit whose last goal fits
// NEAR: the original refuses vector::operator[]'s begin()/operator+ in the first loop (more inline pressure than
// ours), mL29C compares unsigned (jb; EEUnit.h has long)
// effects 0xb8 move, 0xb9 attack, 0xba target, 0xbb unload
void EESendGroupCommand(EEGroup* inGroup, EEUnit* inTarget, unsigned long inPlayerID, long inCommand, long inGoalQueue)
{
    GE3DPoint theLocation;
    GE3DPoint theGround;
    unsigned long theIndex = inGoalQueue;
    long theFormation = inGroup->mFormation->mType;
    unsigned long theBestTime = 0;
    long theMarkers = 0;
    bool theDone = false;
    EEEffectHandle theHandle;
    if (gCommunications->GetIsGamePaused())
        return;
    EEGraphicEffect* theAttackEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationAttack);
    EEGraphicEffect* theMoveEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMove);
    EEGraphicEffect* theTargetEffect = EEGraphicEffect::Get(kGraphicEffectCommandConfirmationGatherOther);
    float theFraction = inGroup->GetNumUnits() * 0.05f;
    long theRounded = UMFloatToLong(theFraction);
    if (theRounded < theFraction)
        theRounded++;
    long theStep = theRounded;
    if (theRounded == 0)
        theStep = 1;
    if (theFormation == 1 || theFormation == 2 || theFormation == 3 || theFormation == 4 || theFormation == 5)
        theFormation = 1;
    unsigned long i;
    for (i = 0; i < inGroup->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = inGroup->GetUnit(i);
        if ((theFormation != 0 || !(theUnit->mFormationSlot < 15)) && (theFormation != 1 || !(theUnit->mFormationSlot < 20)) &&
            ((theFormation != 6 && theFormation != 7) || i % theStep != 0))
            continue;
        EEUnitClass* theClass = theUnit->mUnitClass;
        if (theClass->mAIKind == kAIKindAircraftCarrier || theClass->mAIKind == kAIKindSpaceCarrier || theClass->mUnitType == kUnitTypeAirport) {
            for (unsigned long j = 0; j < theUnit->GetNumGoalQueues(); j++) {
                if (theUnit->mGoalQueues[j].mGoals.size() == 0)
                    continue;
                EEUnitGoal* theQueued = theUnit->mGoalQueues[j].mGoals.back();
                if (theQueued != NULL && theBestTime <= theQueued->GetCreationTime()) {
                    theBestTime = theQueued->GetCreationTime();
                    theIndex = j;
                }
            }
        } else
            theIndex = inGoalQueue;
        EEUnitGoal* theGoal;
        theGoal = theIndex < theUnit->GetNumGoalQueues() && theUnit->mGoalQueues[theIndex].mGoals.size() != 0 ? theUnit->mGoalQueues[theIndex].mGoals.back() : NULL;
        if (theGoal == NULL) {
            theDone = true;
            continue;
        }
        if (inCommand == 0xb8 && dynamic_cast<EEUGLocation*>(theGoal) != NULL) {
            theGoal->GetGoalLocation(theLocation);
            EEMap* theMap = gWorld.mMap;
            if (theMap->GetTile((long)theLocation.GetX(), (long)theLocation.GetY()) != NULL) {
                theMap->GetGroundPoint(theLocation, theGround);
                theUnit->mUnitClass->GroundLocation(theGround);
                if (theGround.GetZ() < 0.0f)
                    theGround.SetZ(0.0f);
                gWorld.GetEffectManager()->CreateEffect(theUnit, theGround, theMoveEffect, &theHandle, theIndex);
                EEEffect* theEffect = gWorld.GetEffectManager()->GetEffect(theHandle);
                theEffect->mIsLocalOnly = true;
                theEffect->mPlayer = inPlayerID;
                theEffect->mIsSaved = false;
                theDone = true;
            }
            continue;
        }
        if (inCommand == 0xb9) {
            if (dynamic_cast<EEUGAttackUnit*>(theGoal) != NULL)
                break;
            if (dynamic_cast<EEUGAttackGround*>(theGoal) != NULL) {
                theGoal->GetGoalLocation(theGround);
                gWorld.GetEffectManager()->CreateEffect(NULL, theGround, theAttackEffect, &theHandle, theIndex);
                EEEffect* theEffect = gWorld.GetEffectManager()->GetEffect(theHandle);
                theEffect->mIsLocalOnly = true;
                theEffect->mPlayer = inPlayerID;
                theEffect->mIsSaved = false;
                return;
            }
        }
        if (inCommand == 0xbb) {
            theGoal->GetGoalLocation(theGround);
            if (theGoal->mKind == kGoalUnloadTransport) {
                EEUGUnloadTransport* theUnload = dynamic_cast<EEUGUnloadTransport*>(theGoal);
                if (theUnload != NULL && theUnload->mTransport != NULL &&
                    theUnload->mTransport->mUnitClass->mCategory == kUnitCategoryFortress)
                    theUnload->mTransport->GetPosition(theGround);
            }
            gWorld.GetEffectManager()->CreateEffect(theUnit, theGround, theMoveEffect, &theHandle, theIndex);
            EEEffect* theEffect = gWorld.GetEffectManager()->GetEffect(theHandle);
            theMarkers++;
            theEffect->mIsLocalOnly = true;
            theEffect->mPlayer = inPlayerID;
            theEffect->mIsSaved = false;
            theDone = true;
            if (theMarkers == 10)
                return;
            continue;
        }
        if (inTarget != NULL) {
            if (dynamic_cast<EEUGBuildRepair*>(theGoal) != NULL || dynamic_cast<EEUGGather*>(theGoal) != NULL ||
                dynamic_cast<EEUGLoadGarrison*>(theGoal) != NULL)
                break;
            gWorld.GetEffectManager()->CreateUnitEffect(inTarget, theTargetEffect, &theHandle, 0, 0, theIndex);
        } else {
            theGoal->GetGoalLocation(theGround);
            gWorld.GetEffectManager()->CreateEffect(NULL, theGround, theTargetEffect, &theHandle, theIndex);
        }
        EEEffect* theEffect = gWorld.GetEffectManager()->GetEffect(theHandle);
        theEffect->mIsLocalOnly = true;
        theEffect->mPlayer = inPlayerID;
        theEffect->mIsSaved = false;
        theDone = true;
        break;
    }
    if (theDone)
        return;
    for (i = 0; i < inGroup->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = inGroup->GetUnit(i);
        if (!(theIndex < theUnit->GetNumGoalQueues()))
            continue;
        EEUnitGoal* theGoal = theUnit->mGoalQueues[theIndex].mGoals.size() > 0 ? theUnit->mGoalQueues[theIndex].mGoals.back()
                                                                          : NULL;
        if (theGoal == NULL || inCommand == 0xb8 ||
            ((inCommand != 0xb9 || dynamic_cast<EEUGLocation*>(theGoal) == NULL) &&
             (inCommand != 0xba || dynamic_cast<EEUGLocation*>(theGoal) == NULL)))
            continue;
        theGoal->GetGoalLocation(theLocation);
        gWorld.mMap->GetGroundPoint(theLocation, theGround);
        theUnit->mUnitClass->GroundLocation(theGround);
        gWorld.GetEffectManager()->CreateEffect(theUnit, theGround, theMoveEffect, &theHandle, theIndex);
        EEEffect* theEffect = gWorld.GetEffectManager()->GetEffect(theHandle);
        theEffect->mIsLocalOnly = true;
        theEffect->mPlayer = inPlayerID;
        theEffect->mIsSaved = false;
        return;
    }
}
