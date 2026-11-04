// C:\EEX\Empire Earth\Communications\EEServer.cpp (__FILE__ 0x4bb934): the game's lock-step server, 0xca90f..0xcd4f9
// (the WON error strings 0xcd4f9 open EEWonNet.cpp). Unit flags: the exe's module set.
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
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMiniMap.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDatabaseTables.h"
#include <mbstring.h>
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEUIEMainGameEvents.h"
#include "ee/EEWonNet.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEARemovePlayer.h"
#include "lle/UResourceFile.h"
#include "ee/EETeam.h"
#include "ee/EEClient.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EEServer.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_CASE in ProcessSystemMessage)
#include "ee/EEStringIDs.h"
long gDefaultPlayerColors[16] = {0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15};
static unsigned long sSample = 0xffffffff;   // 0x4bb8b8 (ExecuteRemovePlayer: the last booted/out-of-synch sound)

// FUNCTION: EXE 0x4ca90f
// Creates the game server with the lock-step rates from the game preferences, no vote or load running, and its locks
EEServer::EEServer(UWideString& inName, unsigned long inNetworkAdapter, unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick, bool inAllowNewConnections,
                   bool inAnnounce)
    : NESynchronousServer(16, 0x15e, 0x3c, gGamePreferences.mSettings.mMaxPTsPerSecond,
                          gGamePreferences.mSettings.mMinPTsPerSecond,
                          100.0f / gGamePreferences.mSettings.mPTRateTuningThreshold, gGamePreferences.mSettings.mMaxCTSendInterval,
                          gGamePreferences.mSettings.mCTSendIntervalStepUp, gGamePreferences.mSettings.mCTSendIntervalStepDown, inCurrentCommunicationsTick, inCurrentPhysicsTick, inName, inAllowNewConnections,
                          inAnnounce, inNetworkAdapter)
{
    this->mVoteEdict = 0;
    for (long i = 0; i < 256; i++)
        this->mVoted[i] = false;
    this->mYesVotes = 0;
    this->mNoVotes = 0;
    this->mNumVotes = 0;
    this->mNumLoadResponses = 0;
    this->mLoadState = 0;
    this->mNumDroppedLoaders = false;
    this->mHasLeftGame = false;
    this->mLagMask = 0;
    this->mIsMigrating = false;
    this->ResetFastSelection();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSearchForSlotLock) == keseOK, 103);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mFastSelectionLock) == keseOK, 106);

    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mAttemptToRemovePlayerLock) == keseOK, 109);
}

// SYNTHETIC: EXE 0x4caa6a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4caa86
// Destroys the server, stopping its threads and releasing its three locks
EEServer::~EEServer()
{
    this->StopThreads();
    TSScheduler::DestroyCriticalSection(this->mSearchForSlotLock);
    TSScheduler::DestroyCriticalSection(this->mFastSelectionLock);
    TSScheduler::DestroyCriticalSection(this->mAttemptToRemovePlayerLock);
}

// FUNCTION: EXE 0x4caad8
// Handles a join request: refuses a wrong version or checksum, a closed game, a name in use or a full game; else
// gives it a slot, sends it the options and players and starts the saved game or scenario transfer
void EEServer::ProcessConnectionRequest(unsigned char inSlot, const EEGameOptionsPlayer& inPlayer,
                                        const UWideString& inVersion, unsigned long inChecksum)
{
    EEGameOptionsPlayer thePlayer;
    EEGameOptionsPlayer theOther;
    EEPSaveGameHeader theHeader;
    EEWonNetGameInfo theInfo;
    NEMClientConnectionRequestResponse* theResponse;
    FSFileSpec* theFile;
    unsigned long theXferID;
    unsigned long theIndex;
    unsigned long theCount;
    unsigned long i;
    bool theOK = true;
    this->LockSendCommunicationsTick();
    gGameOptions.mLock->WriteLock();
    this->StopPing();
    this->mSearchForSlotLock->Enter();
    if (inVersion != gGameVersion) {
        theResponse = new NEMClientConnectionRequestResponse(kConnectionVersionMismatch, gGameVersion, inSlot, this->GetCurrentCommunicationsTick(),
                                                             0xff);
        theResponse->SetOriginatingSlot(kSlotServer);
        this->SendMessageTo(inSlot, theResponse, true);
        theResponse->Dereference();
        this->UnregisterSlot(inSlot);
        theOK = false;
    } else if (inChecksum != gBinaryChecksum) {
        theResponse = new NEMClientConnectionRequestResponse(kConnectionBadChecksum, gGameVersion, inSlot, this->GetCurrentCommunicationsTick(),
                                                             0xff);
        theResponse->SetOriginatingSlot(kSlotServer);
        this->SendMessageTo(inSlot, theResponse, true);
        theResponse->Dereference();
        this->UnregisterSlot(inSlot);
        theOK = false;
    }
    if (inSlot == kSlotServer) {
        this->mSendToSlots.clear();
        gLocalPlayerOut = false;
        theCount = gGameOptions.mIsMultiplayer ? 16 : 2;
        thePlayer.mType = 3;
        if (!EECheck2989A9(true)) {
            for (i = 0; i < 16; i++) {
                EEGameOptionsPlayer& theSlotPlayer = gGameOptions.mPlayers[i];
                thePlayer = theSlotPlayer;
                thePlayer.mColor = gDefaultPlayerColors[i];
                thePlayer.mIsLocal = false;
                thePlayer.mSlot = i;
                thePlayer.mStartingCitizens = gWorldTable.GetLongValue(0x30);
                thePlayer.mIsReady = false;
                thePlayer.mPing = 0.0f;
                thePlayer.mName = "";
                thePlayer.mTeam = 0;
                if (i + 1 > theCount || i >= 8)
                    thePlayer.mType = 4;
                else if (i == 1 && gGameOptions.mIsMultiplayer != true)
                    thePlayer.mTeam = 0;
                else
                    thePlayer.mType = 3;
                theSlotPlayer = thePlayer;
            }
        }
    }
    if (theOK == true && this->GetGameState() == kServerStateClosed) {
        theResponse = new NEMClientConnectionRequestResponse(kConnectionGameInProgressAsSent, gGameVersion, inSlot,
                                                             this->GetCurrentCommunicationsTick(), 0xff);
        theResponse->SetOriginatingSlot(kSlotServer);
        this->SendMessageTo(inSlot, theResponse, true);
        theResponse->Dereference();
        this->UnregisterSlot(inSlot);
        theOK = false;
    }
    if (!EECheck2989A9(true)) {
        if (theOK != true)
            goto done;
        for (i = 0; i < 16; i++) {
            thePlayer = gGameOptions.mPlayers[i];
            if ((thePlayer.mType == 1 || thePlayer.mType == 2) && _mbsicmp(reinterpret_cast<const unsigned char*>((const char*)thePlayer.mName),
                                                                          reinterpret_cast<const unsigned char*>((const char*)inPlayer.mName)) == 0) {
                theResponse = new NEMClientConnectionRequestResponse(kConnectionNameInUse, gGameVersion, inSlot,
                                                                     this->GetCurrentCommunicationsTick(), 0xff);
                theResponse->SetOriginatingSlot(kSlotServer);
                this->SendMessageTo(inSlot, theResponse, true);
                theResponse->Dereference();
                this->UnregisterSlot(inSlot);
                goto done;
            }
        }
        if (gGameOptions.mLoadType == 2) {
            theOK = false;
            for (i = 0; i < 16; i++) {
                thePlayer = gGameOptions.mPlayers[i];
                if (thePlayer.mName == inPlayer.mName && thePlayer.mType == 3) {
                    theOK = true;
                    break;
                }
            }
            if (!theOK) {
                theResponse = new NEMClientConnectionRequestResponse(kConnectionGameFull, gGameVersion, inSlot,
                                                                     this->GetCurrentCommunicationsTick(), 0xff);
                theResponse->SetOriginatingSlot(kSlotServer);
                this->SendMessageTo(inSlot, theResponse, true);
                theResponse->Dereference();
                this->UnregisterSlot(inSlot);
            }
        }
    }
    if (theOK == true) {
        for (theIndex = inSlot != kSlotServer; theIndex < 16; theIndex++) {
            thePlayer = gGameOptions.mPlayers[theIndex];
            if (EECheck2989A9(true)) {
                if (thePlayer.mType != 1 && thePlayer.mType != 2)
                    continue;
            } else {
                if (thePlayer.mType != 3)
                    continue;
                if (gGameOptions.mLoadType != 2)
                    break;
            }
            if (thePlayer.mName == inPlayer.mName)
                break;
        }
        if (theIndex >= 16) {
            theResponse = new NEMClientConnectionRequestResponse(kConnectionGameFull, gGameVersion, inSlot,
                                                                 this->GetCurrentCommunicationsTick(), 0xff);
            theResponse->SetOriginatingSlot(kSlotServer);
            this->SendMessageTo(inSlot, theResponse, true);
            this->UnregisterSlot(inSlot);
            theResponse->Dereference();
            goto done;
        }
        this->GetCurrentCommunicationsTick();
        theResponse = new NEMClientConnectionRequestResponse(kConnectionAccepted, gGameVersion, inSlot, this->GetCurrentCommunicationsTick(),
                                                             theIndex);
        theResponse->SetOriginatingSlot(kSlotServer);
        this->SendMessageTo(inSlot, theResponse, true);
        theResponse->Dereference();
        if (!EECheck2989A9(true)) {
            if (gGameOptions.mLoadType == 0) {
                thePlayer.mType = inPlayer.mType;
                thePlayer.mColor = gDefaultPlayerColors[theIndex];
                thePlayer.mName = inPlayer.mName;
                thePlayer.mTeam = inPlayer.mTeam;
                thePlayer.mPing = inPlayer.mPing;
                thePlayer.mIsReady = inPlayer.mIsReady;
                thePlayer.mSlot = theIndex;
                thePlayer.mIsLocal = theIndex == 0;
            } else if (gGameOptions.mLoadType == 2 && (thePlayer.mLoadedType == 1 || thePlayer.mLoadedType == 2)) {
                thePlayer.mType = thePlayer.mLoadedType;
            } else {
                thePlayer.mType = inPlayer.mType;
                thePlayer.mName = inPlayer.mName;
            }
        } else
            thePlayer.mIsReady = true;
        this->mSendToSlots.insert(std::map<unsigned char, unsigned char>::value_type((unsigned char)theIndex, inSlot));
        this->GetCurrentCommunicationsTick();
        gCommunications->ProcessLocalUserInput(new EEMFullGameOptions(gGameOptions), inSlot);
        for (i = 0; i < 16; i++) {
            if (i != theIndex) theOther = gWorld.GetLobbyOptions().mPlayers[i]; else theOther = thePlayer;
            this->GetCurrentCommunicationsTick();
            this->ProcessLocalUserInput(new EEMFullPlayerInformation(theOther), inSlot);
            gUserInterface->PostLobbyUpdatePlayer(theOther.mSlot, 8);
        }
        for (i = 0; i < 16; i++) {
            unsigned char theSendSlot = this->GetSendToSlot((unsigned char)i);
            if (theSendSlot != kSlotNone) {
                this->ProcessLocalUserInput(new EEMFullPlayerInformation(thePlayer), theSendSlot);
                gUserInterface->PostLobbyUpdatePlayer(thePlayer.mSlot, 8);
            }
        }
        this->RegisterSlot(this->GetSendToSlot((unsigned char)thePlayer.mSlot));
        this->ProcessLocalUserInput(new NEMClientConnectionRequestResponse(kConnectionPlayerJoined, gGameVersion, inSlot,
                                                                     this->GetCurrentCommunicationsTick(), theIndex),
                              inSlot);
        long theResult;
        if (gGameOptions.mLoadType == 2) {
            theResult = EEOpenSaveGameFile((const char*)gGameOptions.mMapType, "Saved Games\\", theFile);
        } else if (gGameOptions.mLoadType == 1) {
            theResult = EEOpenSaveGameFile((const char*)gGameOptions.mMapType, "Scenarios\\", theFile);
        } else
            goto loaded;
        if (theResult == kErrorOK) {
            theHeader.mMajorVersion = kSaveGameMajorVersion;
            theHeader.mMinorVersion = kSaveGameMinorVersion;
            if (theHeader.LoadFromFile(*theFile) == kErrorOK && !EECheck2989A9(true))
                this->XferFileToClient(*theFile, theHeader.mChecksum, this->GetSendToSlot((unsigned char)thePlayer.mSlot), theXferID, false);
        }
    loaded:
        gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
        if (gWonNet != NULL) {
            unsigned long thePlayers = 0;
            unsigned long theHumans = 0;
            gWonNet->GetGameInfo(theInfo);
            for (i = 0; i < 16; i++) {
                thePlayer = gWorld.GetLobbyOptions().mPlayers[i];
                if (thePlayer.mType == 3 || thePlayer.mType == 1 || thePlayer.mType == 2)
                    thePlayers++;
                if (thePlayer.mType == 1 || thePlayer.mType == 2)
                    theHumans++;
            }
            theInfo.mPlayers = thePlayers;
            theInfo.mHumans = theHumans;
            gWonNet->SetGameInfo(theInfo);
        }
    }
done:
    this->mSearchForSlotLock->Leave();
    this->StartPing();
    gGameOptions.mLock->WriteUnlock();
    this->UnlockSendCommunicationsTick();
}

// SYNTHETIC: EXE 0x4cb39a
// (NEMClientConnectionRequestResponse's ??_G, after its first user)

// SYNTHETIC: EXE 0x4cb3cf
// (EEWonNetGameInfo's implicit ctor, first used by ProcessConnectionRequest)

// SYNTHETIC: EXE 0x4cb3ea
// (EEWonNetGameInfo's implicit dtor)

// FUNCTION: EXE 0x4cb405
// Removes a client: in a running game drops its player; the host leaving tells every client the host is gone;
// in the lobby reopens its slot, tells it why and stands in its load response
void EEServer::BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting)
{
    EEGameOptionsPlayer thePlayer;
    EEWonNetGameInfo theInfo;
    unsigned long i;
    if (this->GetGameState() != 2 && (this->GetGameState() != kServerStateOpen || this->IsMigrating()) && this->GetGameState() != kServerStateClosed) {
        this->RemovePlayerFromGameInProgress(inSlot, inWasBooted, false, inGameIsExiting);
        if (gWonNet != NULL) {
            gWonNet->GetGameInfo(theInfo);
            theInfo.mHumans--;
            gWonNet->SetGameInfo(theInfo);
        }
    } else if (inSlot == kSlotServer) {
        for (i = 1; i < 16; i++) {
            unsigned char theSendSlot = this->GetSendToSlot((unsigned char)i);
            if (theSendSlot == kSlotNone)
                continue;
            NEMClientConnectionRequestResponse* theResponse = new NEMClientConnectionRequestResponse(kConnectionHostGone,
                gGameVersion, theSendSlot, this->GetCurrentCommunicationsTick(), 0xff);
            theResponse->SetOriginatingSlot(kSlotServer);
            this->SendMessageTo(theSendSlot, theResponse, true);
            theResponse->Dereference();
            this->UnregisterSlot(theSendSlot);
        }
        if (inGameIsExiting == true)
            this->ProcessLocalUserInput(new NEMClientConnectionRequestResponse(6, gGameVersion, kSlotServer,
                                                                         this->GetCurrentCommunicationsTick(), 0xff), kSlotServer);
        else
            this->ProcessLocalUserInput(new NEMClientConnectionRequestResponse(kConnectionRemovedByHost, gGameVersion, kSlotServer,
                                                                         this->GetCurrentCommunicationsTick(), 0xff), kSlotServer);
    } else {
        this->mAttemptToRemovePlayerLock->Enter();
        for (i = 0; i < 16; i++) {
            if (this->GetSendToSlot((unsigned char)i) != inSlot)
                continue;
            if (this->GetGameState() == kServerStateOpen) {
                gGameOptions.mLock->WriteLock();
                thePlayer = gGameOptions.mPlayers[i];
                this->AbortXfer(thePlayer.mTransferID);
                thePlayer.mIsReady = false;
                thePlayer.mIsLocal = false;
                thePlayer.mTransferID = 0;
                thePlayer.mType = 3;
                if (gGameOptions.mLoadType == 0) {
                    thePlayer.mLoadedType = 3;
                    thePlayer.mName = "";
                    thePlayer.mPing = 0.0f;
                    thePlayer.mColor = gDefaultPlayerColors[i];
                    thePlayer.mTeam = 0;
                    thePlayer.mB36 = false;
                    thePlayer.mStartingCitizens = gWorldTable.GetLongValue(0x30);
                }
                EEMFullPlayerInformation* theInformation = new EEMFullPlayerInformation(thePlayer);
                theInformation->SetOriginatingSlot(kSlotServer);
                this->ProcessLocalUserInput(theInformation, kSlotNone);
                gUserInterface->PostLobbyUpdatePlayer(thePlayer.mSlot, 8);
                gGameOptions.mPlayers[thePlayer.mSlot] = thePlayer;
                gGameOptions.mLock->WriteUnlock();
            }
            NEMClientConnectionRequestResponse* theResponse;
            if (inGameIsExiting == true) {
                theResponse = new NEMClientConnectionRequestResponse(6, gGameVersion, inSlot,
                                                                     this->GetCurrentCommunicationsTick(), 0xff);
                theResponse->SetOriginatingSlot(kSlotServer);
                this->SendMessageTo(inSlot, theResponse, true);
                theResponse->Dereference();
            } else if (inWasBooted == true) {
                theResponse = new NEMClientConnectionRequestResponse(kConnectionRemovedByHost, gGameVersion, inSlot,
                                                                     this->GetCurrentCommunicationsTick(), 0xff);
                theResponse->SetOriginatingSlot(kSlotServer);
                this->SendMessageTo(inSlot, theResponse, true);
                theResponse->Dereference();
            }
            if (this->GetGameState() == kServerStateClosed || this->GetGameState() == 2) {
                EEMessage* theMessage;
                switch (this->mLoadState) {
                case 2:
                    theMessage = new EEMLoadMainScreenResponse(inSlot, 0xff);
                    this->ProcessLocalUserInput(theMessage, kSlotServer);
                    this->mNumDroppedLoaders++;
                    break;
                case 1:
                    theMessage = new EEMStartLoadingResponse(inSlot, 0xff, 0);
                    this->ProcessLocalUserInput(theMessage, kSlotServer);
                    this->mNumDroppedLoaders++;
                    break;
                }
            }
            this->UnregisterSlot(inSlot);
            this->mSendToSlots.erase((unsigned char)i);
            if (gWonNet != NULL) {
                gWonNet->GetGameInfo(theInfo);
                theInfo.mHumans--;
                gWonNet->SetGameInfo(theInfo);
            }
            break;
        }
        this->mAttemptToRemovePlayerLock->Leave();
    }
    EE_LOST_DEBUG(8);   // LOST-DEBUG: none: exact only (end of body, smallest n)
}

// SYNTHETIC: EXE 0x4cb815
// (EEMStartLoadingResponse's inline ctor, first used by BootPlayer)

// FUNCTION: EXE 0x4cb850
// Removes a client from its lobby slot (removed-by-host response), forgets its slot and resends the slot's info
void EEServer::RefuseClient(EEGameOptionsPlayer& inPlayer)
{
    NEMClientConnectionRequestResponse* theResponse =
        new NEMClientConnectionRequestResponse(kConnectionRemovedByHost, gGameVersion, kSlotNone, this->GetCurrentCommunicationsTick(), 0xff);
    theResponse->SetOriginatingSlot(kSlotServer);
    this->SendMessageTo(this->GetSendToSlot(inPlayer.mSlot), theResponse, true);
    theResponse->Dereference();
    this->UnregisterSlot(this->GetSendToSlot(inPlayer.mSlot));
    this->mSendToSlots.erase((unsigned char)inPlayer.mSlot);
    this->SendFullPlayerInformation(inPlayer.mSlot, inPlayer);
}

// FUNCTION: EXE 0x4cb8f6
// Handles a player's removal on the host: ours shows the booted or out-of-sync box and ends the game, another's
// is announced in the chat log, unregistered and removed; plays the matching sound
void EEServer::ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    UWideString theText;
    UWideString theFormat;
    UWideString theName;
    unsigned long theUnused = 0;
    if (!this->mHasLeftGame) {
        if (inPlayer == kSlotServer) {
            this->mHasLeftGame = true;
            if (inGameIsExiting)
                FnB7992(&theUnused);
            else {
                if (inPlayerIsOOS)
                    gResource->GetString(IDS_YOU_ARE_OUT_OF_SYNCH, theFormat);
                else
                    gResource->GetString(IDS_YOU_HAVE_BEEN_BOOTED_DUE, theFormat);
                sprintf(theText, theFormat);
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
            if (!inWasBooted) {
                this->LockSendCommunicationsTick();
                this->UnregisterSlot(this->GetPlayerSendToSlot(inPlayer));
                this->UnlockSendCommunicationsTick();
            }
            gWorld.RemoveDisconnectedPlayer(inPlayer);
        }
        if (!inWasBooted)
            this->mNumHumanPlayers--;
        if (!inGameIsExiting) {
            if (inPlayerIsOOS)
                sSample = gSoundManager->Load(UString("sounds\\OutOfSynch.wav"), 0x31, 1);
            else
                sSample = gSoundManager->Load(UString("sounds\\Booted.wav"), 0x31, 1);
            if (sSample != (unsigned long)-1)
                gSoundManager->Play(sSample, 1.0f, 0.0f, 1.0f, false, NULL);
        }
    }
}

// FUNCTION: EXE 0x4cbb17
// Returns the first world player index below the maximum that holds no player, else 0xff
unsigned char EEServer::GetFreePlayerSlot()
{
    unsigned char theMax = this->GetMaxPlayers();
    unsigned char i;
    unsigned char theSlot;
    for (i = 0; i < theMax; i++) {
        if (gWorld.mPlayers[i] == NULL)
            break;
    }
    if (i < theMax)
        theSlot = i;
    else
        theSlot = 0xff;
    return theSlot;
}

// FUNCTION: EXE 0x4cbb41
// Returns true when the world has no teams
bool EEServer::ServerSlot28()
{
    return (gWorld.mTeams.size() > 0) ? false : true;
}

// FUNCTION: EXE 0x4cbb65
// Returns whether a server migration is in progress
bool EEServer::IsMigrating()
{
    return this->mIsMigrating;
}

// FUNCTION: EXE 0x4cbb6c
// Returns a player's name in a static string (left unchanged when the player is not present)
const UWideString& EEServer::ServerSlot54(unsigned char inPlayer)
{
    static UWideString sName("");
    if (this->IsPlayerPresent(inPlayer))
        sName = gWorld.mPlayers[inPlayer]->mName;
    return sName;
}

// SYNTHETIC: EXE 0x4cbbce
// (the atexit dtor of ServerSlot54's sName)

// FUNCTION: EXE 0x4cbbd9
// Returns a world player's network slot, 0xff when the player is not present
unsigned char EEServer::GetPlayerSendToSlot(unsigned char inPlayer)
{
    unsigned char theSlot = 0xff;
    if (this->IsPlayerPresent(inPlayer))
        theSlot = gWorld.mPlayers[inPlayer]->mSlot;
    return theSlot;
}

// FUNCTION: EXE 0x4cbc02
// Returns the network slot of a lobby slot, 0xff when none
unsigned char EEServer::GetSendToSlot(unsigned char inPlayer)
{
    unsigned char theResult = 0xff;
    std::map<unsigned char, unsigned char>::iterator theIt = this->mSendToSlots.find(inPlayer);
    if (theIt != this->mSendToSlots.end())
        theResult = theIt->second;
    return theResult;
}

// FUNCTION: EXE 0x4cbc35
// Returns the lobby slot that uses a network slot, 0 when none
unsigned char EEServer::GetLobbySlot(unsigned char inValue)
{
    unsigned char theResult = 0;
    std::map<unsigned char, unsigned char>::iterator theIt;
    for (theIt = this->mSendToSlots.begin(); theIt != this->mSendToSlots.end(); ++theIt) {
        if (theIt->second == inValue) {
            theResult = theIt->first;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4cbc77
// Handles a system message on the host: ping replies (sent on as quality stats), communications ticks, join
// requests, disconnections (removes the player), reconnections; load/save and lag messages execute
void EEServer::ProcessSystemMessage(NEMessage* inMessage)
{
    EEGameOptionsPlayer thePlayer;
    unsigned long theNumInputs = 0;
    NEMessage* theInput;
    unsigned long i;
    switch (inMessage->GetType()) {
    case kNetMessagePlayerConnectionCheckResponse:
        this->mSearchForSlotLock->Enter();
        for (i = 0; i < 16; i++) {
            if (this->GetSendToSlot((unsigned char)i) == inMessage->GetOriginatingSlot())
                break;
        }
        if (i < 16) {
            unsigned long theStart = static_cast<NEMPlayerConnectionCheckResponse*>(inMessage)->GetPingTimeStart();
            unsigned long thePing = (TMGetTime() - theStart) / 2;
            float theQuality;
            if (thePing > 750)
                theQuality = 0.0f;
            else if (thePing > 500)
                theQuality = 0.2f;
            else if (thePing > 250)
                theQuality = 0.4f;
            else if (thePing > 150)
                theQuality = 0.6f;
            else if (thePing > 50)
                theQuality = 0.8f;
            else
                theQuality = 1.0f;
            this->SendPingStat((unsigned char)i, theQuality);
        }
        this->mSearchForSlotLock->Leave();
        break;
    case kNetMessageCommunicationsTick: {
        NEMCommunicationsTick* theTick = static_cast<NEMCommunicationsTick*>(inMessage);
        unsigned long theCount = theTick->GetNumberOfPlayerInputs();
        unsigned char theTicksToPerform = theTick->GetPhysicsTicksToPerform();
        for (i = 0; i < theCount; i++) {
            theTick->GetPlayerInput(i, theInput);
            if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == kSlotServer)
                theNumInputs++;
        }
        if (theTicksToPerform != 0xff) {
            unsigned char theTicksPerSecond = theTick->GetPhysicsTicksPerSecond();
            NEMPhysicsTick* thePhysicsTick = new NEMPhysicsTick(theTicksToPerform, theNumInputs, theTicksPerSecond,
                                                                theTick->GetGenerateChecksum(),
                                                                theTick->GetDumpTheWorld());
            gPhysics->mServerMessageQueueLock->Enter();
            gPhysics->QueueServerMessage(thePhysicsTick);
            for (i = 0; i < theCount; i++) {
                theTick->GetPlayerInput(i, theInput);
                if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == kSlotServer) {
                    gPhysics->QueueServerMessage(theInput);
                    theInput->Reference();
                }
            }
            gPhysics->mServerMessageQueueLock->Leave();
        } else {
            gPhysics->mServerMessageQueueLock->Enter();
            gCommunications->IncrementNumberOfCTsProcessedInPhysics();
            gPhysics->mServerMessageQueueLock->Leave();
            unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();   // the original reads it (0xcbe8c) and drops it
            for (i = 0; i < theCount; i++) {
                theTick->GetPlayerInput(i, theInput);
                if (theInput->GetForSpecificSlot() == kSlotNone || theInput->GetForSpecificSlot() == kSlotServer)
                    this->HandleLobbyInput(theInput);
            }
        }
        break;
    }
    case kNetMessageClientConnectionRequest: {
        NEMClientConnectionRequest* theRequest = static_cast<NEMClientConnectionRequest*>(inMessage);
        thePlayer.mName = theRequest->GetPlayerName();
        thePlayer.mTeam = 0;
        thePlayer.mType = 1;
        thePlayer.mStartingCitizens = gWorldTable.GetLongValue(0x30);
        thePlayer.mPing = 0.0f;
        thePlayer.mIsReady = false;
        this->mBehindFirewall.insert(std::map<unsigned char, bool>::value_type(inMessage->GetOriginatingSlot(),
                                                                theRequest->GetIsBehindFirewall()));
        this->ProcessConnectionRequest(inMessage->GetOriginatingSlot(), thePlayer, theRequest->GetVersion(),
                                 theRequest->GetBinaryChecksum());
        inMessage->Dereference();
        break;
    }
    case kNetMessageClientDisconnectionNotification: {
        EEAction* theAction = new EEARemovePlayer(inMessage->GetOriginatingSlot(),
                                                  static_cast<NEMClientDisconnectionNotification*>(inMessage)->GetIsExiting());
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        inMessage->Dereference();
        break;
    }
    case kNetMessageClientReconnectionRequest:
        gWorld.GetPlayer(static_cast<NEMClientReconnectionRequest*>(inMessage)->GetPlayerNumber())->mSlot =
            inMessage->GetOriginatingSlot();
        break;
    case kMessageNewEdictEnacted:  // LOST-DEBUG: the compare chain keeps `sub eax, 0x43; je <exit>` (0xcbfaa), so the case had a body
                 // the optimiser removed; VC6 drops a label whose body is only `break`. EE_LOST_CASE stands in
        EE_LOST_CASE(theNumInputs);   // kept on the 2026-10-24 evidence rule (dispatch matches); function not yet exact
        break;
    case kMessageLoadOrSaveGame:
    case kMessageLagStatus:
        inMessage->Execute();
        break;
    }
}

// FUNCTION: EXE 0x4cbfd0
// Filters a client message before it is passed on (returns whether to pass it): takes votes, pause and join
// requests; when the host itself is removed it turns into a client
bool EEServer::FilterPlayerInput(NEMessage* inMessage)
{
    EEGameOptionsPlayer thePlayer;
    bool theResult = true;
    switch (inMessage->GetType()) {
    case kMessageLoadOrSaveGameResponse:
        this->LogResponse(inMessage);
        inMessage->Dereference();
        theResult = false;
        break;
    case kMessageEdictVote: {
        EEMEdictVote* theVote = static_cast<EEMEdictVote*>(inMessage);
        this->CastVote(theVote->GetOriginatingSlot(), theVote->mIsYes, theVote->mCount);
        theResult = false;
        break;
    }
    case kMessageEdictProposal:
        theResult = this->StartVote(static_cast<EEMEdictProposal*>(inMessage)->GetEdict());
        break;
    case kNetMessageClientRequestPause: {
        UIEvent* theEvent = new EEUIEMGPauseGame;
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        inMessage->Dereference();
        theResult = false;
        break;
    }
    case kNetMessageClientConnectionRequestResponse:
        if (inMessage->GetForSpecificSlot() == kSlotServer) {
            long theError = static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetError();
            if (theError == kConnectionRemovedByHost || theError == 6) {
                static_cast<NESynchronousServer*>(gCommunications)->StopPing();
                static_cast<NESynchronousServer*>(gCommunications)->ResetXfer();
                this->UnregisterSlot(kSlotServer);
                this->RegisterSlot(kSlotServer);
                if (gCommunications != NULL) {
                    delete gCommunications;
                    gCommunications = new EEClient;
                    static_cast<NESynchronousClient*>(gCommunications)->StartThreads();
                }
                theResult = false;
            } else if (theError == kConnectionPlayerJoined) {
                if (!EECheck2989A9(true))
                    gUIManager->Push(gUIManager->GetFormByID(kFormLobby));
                theResult = true;
            } else
                theResult = false;
        } else
            theResult = true;
        break;
    case kNetMessageClientConnectionRequest: {
        NEMClientConnectionRequest* theRequest = static_cast<NEMClientConnectionRequest*>(inMessage);
        thePlayer.mName = theRequest->GetPlayerName();
        thePlayer.mTeam = 0;
        thePlayer.mType = 1;
        thePlayer.mStartingCitizens = gWorldTable.GetLongValue(0x30);
        thePlayer.mPing = 0.0f;
        thePlayer.mIsReady = false;
        this->mBehindFirewall.insert(std::map<unsigned char, bool>::value_type(inMessage->GetOriginatingSlot(),
                                                                theRequest->GetIsBehindFirewall()));
        this->ProcessConnectionRequest(inMessage->GetOriginatingSlot(), thePlayer, theRequest->GetVersion(),
                                 theRequest->GetBinaryChecksum());
        inMessage->Dereference();
        theResult = false;
        break;
    }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x4cc1ce
// (EEUIEMGPauseGame's ??_G, after its first user)

// FUNCTION: EXE 0x4cc1f9
// Stamps the host's local input with the fast-selection number; selection messages take a new number and run at once
void EEServer::PrepareLocalInput(NEMessage* inMessage)
{
    switch (inMessage->GetType()) {
    default: {
        EEMessage* theMessage = dynamic_cast<EEMessage*>(inMessage);
        if (theMessage != NULL) {
            this->mFastSelectionLock->Enter();
            theMessage->mFastSelectionID = this->mFastSelectionID;
            this->mFastSelectionLock->Leave();
        }
        break;
    }
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
    }
}

// FUNCTION: EXE 0x4cc2ca
// Returns whether a world player index holds a player
bool EEServer::IsPlayerPresent(unsigned char inPlayer)
{
    bool theResult = false;
    if (inPlayer < gWorld.mNumPlayers && gWorld.mPlayers[inPlayer] != NULL)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x4cc2e7
// Plays a warning sound: 1 out of sync, 2 resending
void EEServer::PlayWarningSound(long inSound)
{
    unsigned long theSample;
    unsigned long theInstance;
    switch (inSound) {
    case 1:
        EELoadSound(UString("sounds\\OutOfSynch.wav"), 0x31, theSample, theInstance, 1);
        break;
    case 2:
        EELoadSound(UString("sounds\\Resending.wav"), 0x31, theSample, theInstance, 1);
        break;
    }
}

// (included here, below the ctor: placed so that the ctor's ESVERIFY keeps lines 103..109)
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEHumanPlayer.h"

// FUNCTION: EXE 0x4cc34e
// Handles a lobby message on the host: player info (and the WON game info), transfer status, the game start,
// a joined player, a player's slot info; the rest execute
// NEAR: frame layout (the "" temporary, the string slots), the mPing copy (the original loads it with fld),
// register choice
// player info falls through into the game options case (the WON game info follows the slots)
void EEServer::HandleLobbyInput(NEMessage* inMessage)
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    EEGameOptionsPlayer thePlayer;
    EEWonNetGameInfo theInfo;
    UWideString theText;
    UWideString theName;
    UWideString theMessage;
    UWideString theFormat;
    NEProtocol* theProtocol = NULL;
    switch (inMessage->GetType()) {
    case kMessageFullPlayerInformation:
        if (inMessage->GetOriginatingSlot() != kSlotServer) {
            EEMFullPlayerInformation* theUpdate = static_cast<EEMFullPlayerInformation*>(inMessage);
            gGameOptions.mLock->WriteLock();
            thePlayer.mColor = theUpdate->mColor;
            thePlayer.mType = theUpdate->mSlotType;
            thePlayer.mTeam = theUpdate->mTeam;
            thePlayer.mName = theUpdate->mName;
            thePlayer.mPing = theUpdate->mPing;
            thePlayer.mStartingCitizens = theUpdate->mCitizenBonus;
            thePlayer.mLoadedType = theUpdate->mLoadedType;
            thePlayer.mIsReady = theUpdate->mIsReady;
            thePlayer.mIsLocal = theUpdate->mIsLocal;
            thePlayer.mB36 = theUpdate->mB42;
            thePlayer.mSlot = theUpdate->mSlot;
            thePlayer.mTransferID = gGameOptions.mPlayers[thePlayer.mSlot].mTransferID;
            if (thePlayer.mSlot != 0 && this->GetSendToSlot(thePlayer.mSlot) != kSlotServer)
                thePlayer.mIsLocal = false;
            else
                thePlayer.mIsLocal = true;
            gWorld.GetLobbyOptions().mPlayers[thePlayer.mSlot] = thePlayer;
            gGameOptions.mLock->WriteUnlock();
            gUserInterface->PostLobbyUpdatePlayer(thePlayer.mSlot, 8);
        }
        if (gWonNet != NULL) {
            unsigned long thePlayers = 0;
            unsigned long theHumans = 0;
            gWonNet->GetGameInfo(theInfo);
            for (unsigned char i = 0; i < 16; i++) {
                thePlayer = gGameOptions.mPlayers[i];
                if (thePlayer.mType == 3 || thePlayer.mType == 1 || thePlayer.mType == 2)
                    thePlayers++;
                if (thePlayer.mType == 1 || thePlayer.mType == 2)
                    theHumans++;
            }
            theInfo.mPlayers = thePlayers;
            theInfo.mHumans = theHumans;
            gWonNet->SetGameInfo(theInfo);
        }
        break;
    case kNetMessageXferStatus: {
        NEMXferStatus* theStatus = static_cast<NEMXferStatus*>(inMessage);
        unsigned long theSlot = theStatus->GetClientSlot();
        if (inMessage->GetOriginatingSlot() != kSlotServer && theStatus->GetStatusID() == 3) {
            gGameOptions.mLock->WriteLock();
            thePlayer = gWorld.GetLobbyOptions().mPlayers[theSlot];
            unsigned long theXferID = theStatus->GetXferID();
            if (theXferID == 0xffff)
                theXferID = thePlayer.mTransferID;
            this->AbortXfer(theXferID);
            thePlayer.mTransferID = 0;
            gGameOptions.mPlayers[thePlayer.mSlot] = thePlayer;
            gGameOptions.mLock->WriteUnlock();
        }
        if (theStatus->GetStatusID() == 4) {
            gResource->GetString(IDS_FILE_TRANSFER_FAILED, theText);
            gUIManager->DisplayMessage(theText, true, NULL, NULL, true);
        }
        gUserInterface->PostLobbyShowTransferProgress(theSlot, theStatus->GetPercentDone());
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
        NEProtocol::GetProtocol(0, theProtocol);
        theProtocol->GetStatus("", 0xffff0000);
        gCommunications->SetIsMultiplayerSession(gCommunications->GetNumHumanPlayers() != 1);
        gGraphics->PostResume();
        while (gGraphics->mIsPaused)
            Sleep(1);
        this->StartGame();
        if (!gGameScreenActive)
            this->PauseGame();
        else {
            gPhysics->mThread->Post(0x404, NULL, NULL, true);
            while (true) {
                if (gPhysics->mTimerRunning)
                    break;
                Sleep(1);
            }
            gUIManager->mMainGameForm->SetUIState(0);
        }
        if (gCommunications->GetIsMultiplayerSession() == true) {
            for (unsigned long k = 1; k < gWorld.mNumPlayers; k++) {
                EEPlayer* thePlayer = gWorld.mPlayers[k];
                if (!thePlayer->IsHuman() || thePlayer->mSlot != kSlotNone)
                    continue;
                gResource->GetString(IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, theFormat);
                sprintf(theMessage, theFormat, (const char*)thePlayer->mName);
                gResource->GetString(IDS_MSG_GAME, theName);
                gUserInterface->AddChatLogEntry(theColor, theName, theMessage,
                                                gWorldTable.GetLongValue(25) * 2, 0);
                gWorld.RemoveDisconnectedPlayer(k);
            }
        }
        break;
    }
    case kNetMessageClientConnectionRequestResponse:
        if (static_cast<NEMClientConnectionRequestResponse*>(inMessage)->GetError() == kConnectionPlayerJoined) {
            gUserInterface->PostLobbyUpdateForm();
            if (gWonNet != NULL)
                gWonNet->PlayerJoined();
        }
        break;
    case kMessagePlayerInformation:
        if (gWonNet != NULL) {
            unsigned long thePlayers = 0;
            unsigned long theHumans = 0;
            gWonNet->GetGameInfo(theInfo);
            for (unsigned char i = 0; i < 16; i++) {
                thePlayer = gGameOptions.mPlayers[i];
                if (thePlayer.mType == 3 || thePlayer.mType == 1 || thePlayer.mType == 2)
                    thePlayers++;
                if (thePlayer.mType == 1 || thePlayer.mType == 2)
                    theHumans++;
            }
            theInfo.mPlayers = thePlayers;
            theInfo.mHumans = theHumans;
            gWonNet->SetGameInfo(theInfo);
        }
    case kMessageGameOptions:
        if (inMessage->GetOriginatingSlot() == kSlotServer)
            break;
        inMessage->Execute();
        break;
    case kMessagePlayerSlotInfo: {
        EEMPlayerSlotInfo* theSlotInfo = static_cast<EEMPlayerSlotInfo*>(inMessage);
        gWorld.mLock->WriteLock();
        EEPlayer* thePlayer = gWorld.mPlayers[theSlotInfo->mPlayerID];
        if (thePlayer != NULL) {
            thePlayer->mSlot = theSlotInfo->mSlot;
            if (thePlayer->IsHuman()) {
                static_cast<EEHumanPlayer*>(thePlayer)->SetName(theSlotInfo->mAddress);
                thePlayer->mBehindFirewall = theSlotInfo->mIsBehindFirewall;
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

// FUNCTION: EXE 0x4cc937
// Starts an edict vote, first closing a running one with yes votes for those who have not voted; returns true
bool EEServer::StartVote(unsigned short inEdict)
{
    unsigned char theIndex = 0;
    if (this->mVoteEdict != 0) {
        unsigned char i = this->mNumVotes;
        while (gWorld.mNumPlayers - 1 > i) {
            for (unsigned short j = (unsigned char)(theIndex + 1); j < 0x100; j++) {
                if (this->mVoted[j] != false)
                    continue;
                theIndex = (unsigned char)j;
                this->CastVote(theIndex, true, 0);
                i++;
                break;
            }
        }
    }
    this->mVoteEdict = inEdict;
    this->mNumVotes = 0;
    this->mNoVotes = 0;
    this->mYesVotes = 0;
    for (long k = 0; k < 256; k++)
        this->mVoted[k] = false;
    return true;
}

// FUNCTION: EXE 0x4cc9ce
// Clears the fast-selection state (no selection executing, id 0, empty table)
void EEServer::ResetFastSelection()
{
    this->mIsExecutingSelection = false;
    this->mU348 = 0;
    this->mFastSelectionID = 0;
    for (long i = 0; i < 16; i++)
        this->mFastSelection[i] = 0;
}

// FUNCTION: EXE 0x4cc9f1
// Restarts the game as the new host: sends the humans' migration slots, announces the new host and restarts
void EEServer::HandleReconnectWaitTimedOut()
{
    UWideString theText;
    UWideString theUnused;
    EEGameOptionsPlayer thePlayer;
    static_cast<NESynchronousServer*>(gCommunications)->SetMessageManagerLowestCTs(this->GetCurrentCommunicationsTick() - 1);
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* theOther = gWorld.mPlayers[i];
        if (theOther->mHumanPlayer != NULL)
            gCommunications->ProcessLocalUserInput(new EEMMigrationSlotInfo(theOther->mSlot, theOther->mPlayerID),
                                                   kSlotNone);
    }
    gCommunications->SetIsMultiplayerSession(gCommunications->GetNumHumanPlayers() != 1);
    gResource->GetString(IDS_NEW_HOST_ESTABLISHED, theText);
    gUserInterface->AddChatLogEntry(theText, 0);
    gUserInterface->PostHostMigrated();
    gCommunications->ProcessLocalUserInput(new EEMGameRestarting, kSlotNone);
    this->StartGame();
}

// SYNTHETIC: EXE 0x4ccb42
// (EEMGameRestarting's inline ctor)

// FUNCTION: EXE 0x4ccb66
// Broadcasts the lagging players: in multiplayer as a player bit mask (when it changed or forced), else the count
void EEServer::ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag)
{
    unsigned short theMask = 0;
    if (this->GetIsMultiplayerSession()) {
        UIForm* theForm = gUIManager->GetFormByID(kFormMainGame);
        if (gUIManager->GetCurrentForm() == theForm) {
            for (std::vector<unsigned char>::iterator it = inSlots.begin(); it != inSlots.end(); it++) {
                EEPlayer* thePlayer = gWorld.GetPlayerBySlot(*it);
                if (thePlayer != NULL)
                    theMask |= (unsigned char)(1 << ((unsigned char)thePlayer->mPlayerID - 1));
            }
            if (theMask != this->mLagMask || inFlag) {
                this->mLagMask = theMask;
                EEMLagStatus* theMessage = new EEMLagStatus(theMask, inFlag);
                theMessage->SetOriginatingSlot(kSlotServer);
                this->SendMessageToAll(theMessage, false);
                theMessage->Dereference();
            }
        }
    } else if (this->GetGameState() == kServerStateOpen || this->GetGameState() == kServerStateClosed) {
        EEMLagStatus* theMessage = new EEMLagStatus(inSlots.size(), inFlag);
        theMessage->SetOriginatingSlot(kSlotServer);
        this->SendMessageToAll(theMessage, false);
        theMessage->Dereference();
    }
}

// SYNTHETIC: EXE 0x4ccc92
// (EEMLagStatus's inline ctor)

// FUNCTION: EXE 0x4cccc8
// Sends every human player's network slot, address and firewall flag, when more than one human is left
void EEServer::SendPlayerSlotInfo()
{
    NEProtocol* theProtocol = NULL;
    UWideString theAddress;
    EEGameOptionsPlayer thePlayer;
    if (!(gCommunications->GetNumHumanPlayers() - this->mNumDroppedLoaders > 1))
        return;
    unsigned char theIndex = 1;
    NEProtocol::GetProtocol(0, theProtocol);
    for (unsigned long i = 1; i < gWorld.mNumPlayers; theIndex++, i++) {
        EEPlayer* theWorldPlayer = gWorld.mPlayers[i];
        if (theWorldPlayer->mHumanPlayer == NULL)
            continue;
        unsigned char j;
        for (j = 0; j < 16; j++) {
            thePlayer = gGameOptions.mPlayers[j];
            if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)thePlayer.mName),
                         reinterpret_cast<const unsigned char*>((const char*)theWorldPlayer->mName)) != 0
                || (thePlayer.mType != 1 && thePlayer.mType != 2))
                continue;
            unsigned char theSlot = this->GetSendToSlot(j);
            EEMPlayerSlotInfo* theMessage;
            if (theSlot == kSlotServer)
                theMessage = new EEMPlayerSlotInfo(kSlotServer, theIndex, EEGetLocalAddress(theProtocol, gGamePreferences.mSettings.GetNetworkAdapter()),
                                                   this->mBehindFirewall[theSlot]);
            else if (theSlot == kSlotNone) {
                theAddress = "";
                theMessage = new EEMPlayerSlotInfo(kSlotNone, theIndex, theAddress, this->mBehindFirewall[theSlot]);
            } else {
                this->GetClientAddress(theSlot, theAddress);
                theMessage = new EEMPlayerSlotInfo(theSlot, theIndex, theAddress, this->mBehindFirewall[theSlot]);
            }
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
            break;
        }
    }
}

// SYNTHETIC: EXE 0x4ccee8
// (EEMPlayerSlotInfo's inline ctor)

// FUNCTION: EXE 0x4ccf3a
// Sends a file transfer's progress for the lobby slot of the given network slot
void EEServer::HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, unsigned long& inXferID,
                            unsigned char& inStatusID)
{
    unsigned char thePlayer = this->GetLobbySlot(inSlot);
    this->ProcessLocalUserInput(new NEMXferStatus(thePlayer, inPercentDone, inXferID, inStatusID), kSlotNone);
}

// SYNTHETIC: EXE 0x4ccfbb
// (NEMXferStatus's ??_G)

// FUNCTION: EXE 0x4ccfe7
// Records a slot's vote on the edict; once all have voted enacts it on a yes majority (ties fail) and resets the vote
void EEServer::CastVote(unsigned char inIndex, bool inYes, unsigned char inCount)
{
    if (this->mVoted[inIndex] != false)
        return;
    this->mNumVotes++;
    this->mVoted[inIndex] = true;
    if (inCount > 0) {
        if (inYes == true)
            this->mYesVotes += inCount;
        else
            this->mNoVotes += inCount;
    }
    if (this->mNumVotes != this->GetNumberOfPlayers())
        return;
    EEMNewEdictEnacted* theMessage;
    if ((this->mNoVotes == 0 && this->mYesVotes == 0) || this->mYesVotes == this->mNoVotes)
        theMessage = new EEMNewEdictEnacted(this->mVoteEdict, this->mYesVotes, this->mNoVotes, false);
    else if (this->mYesVotes > this->mNoVotes)
        theMessage = new EEMNewEdictEnacted(this->mVoteEdict, this->mYesVotes, this->mNoVotes, true);
    else
        theMessage = new EEMNewEdictEnacted(this->mVoteEdict, this->mYesVotes, this->mNoVotes, false);

    if (theMessage != NULL) {
        theMessage->SetOriginatingSlot(kSlotServer);
        this->BroadcastMessage(theMessage);
        this->ProcessSystemMessage(theMessage);
        theMessage->Dereference();
    }
    this->mNumVotes = this->mYesVotes = 0;
    this->mNoVotes = this->mVoteEdict = 0;
}

// SYNTHETIC: EXE 0x4cd0f0
// (EEMNewEdictEnacted's inline ctor)

// FUNCTION: EXE 0x4cd144
// Sends a lobby slot's player options to everyone and refreshes the slot's line
void EEServer::SendFullPlayerInformation(unsigned char inPlayer, EEGameOptionsPlayer& inOptions)
{
    EEGameOptionsPlayer thePlayer;
    thePlayer.mType = inOptions.mType;
    thePlayer.mColor = inOptions.mColor;
    thePlayer.mName = inOptions.mName;
    thePlayer.mTeam = inOptions.mTeam;
    thePlayer.mStartingCitizens = inOptions.mStartingCitizens;
    thePlayer.mPing = inOptions.mPing;
    thePlayer.mIsReady = inOptions.mIsReady;
    thePlayer.mSlot = inOptions.mSlot;
    thePlayer.mB36 = inOptions.mB36;
    this->ProcessLocalUserInput(new EEMFullPlayerInformation(thePlayer), kSlotNone);
    gUserInterface->PostLobbyUpdatePlayer(thePlayer.mSlot, 8);
}

// FUNCTION: EXE 0x4cd1dc
// Sends a human slot's connection quality to everyone
void EEServer::SendPingStat(unsigned char inPlayer, float inPing)
{
    EEGameOptionsPlayer thePlayer;
    gGameOptions.mLock->ReadLock();
    thePlayer = gGameOptions.mPlayers[inPlayer];
    if (thePlayer.mType == 1 || thePlayer.mType == 2)
        this->ProcessLocalUserInput(new EEMUpdatePingStat(inPlayer, inPing), kSlotNone);
    gGameOptions.mLock->ReadUnlock();
}

// SYNTHETIC: EXE 0x4cd262
// (EEMUpdatePingStat's inline ctor)

// FUNCTION: EXE 0x4cd3e8
// Posts message 0x12 to the UI thread
void EEServer::ServerSlot48()
{
    long theParameter1 = 0;
    long theParameter2 = 0;
    gUserInterface->mThread->Post(0x12, &theParameter1, &theParameter2, true);
}

// FUNCTION: EXE 0x4cd411
// Returns the debug out-of-sync handling preference
bool EEServer::GetDebugOOSHandling()
{
    return gGamePreferences.mSettings.mDebugOOSHandling;
}

// SYNTHETIC: EXE 0x4cd4b3
// (EEWorldTable::GetLongValue, the inline COMDAT kept here: ProcessConnectionRequest refused it)

// TEMPLATE: EXE 0x4cd296
// TEMPLATE: EXE 0x4cd2c1
// TEMPLATE: EXE 0x4cd2ec
// TEMPLATE: EXE 0x4cd33d
// TEMPLATE: EXE 0x4cd417
// TEMPLATE: EXE 0x4cd48a
// TEMPLATE: EXE 0x4cd4c5
// TEMPLATE: EXE 0x4cd449
// TEMPLATE: EXE 0x4cd377
