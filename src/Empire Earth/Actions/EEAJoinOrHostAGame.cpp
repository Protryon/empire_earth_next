// C:\EEX\Empire Earth\Actions\EEAJoinOrHostAGame.cpp (file name inferred: no __FILE__; RTTI class
// EEAJoinOrHostAGame, vtable 0x438c08). The multiplayer join/host action: 0x2086..0x289f (EEACenterCamera's std
// COMDATs end at 0x2086, EEALasso's dtor starts at 0x289f). ctor (its ??_G after it), dtor, Virtual4 with the lobby
// events' and the connection request's ??_G after it (their first user), the message box callback, EEIsValidName.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"

#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUIManager.h"
#include "ee/EEAction.h"
#include "ee/EEAJoinOrHostAGame.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"   // ee_screens_valid_name (single_player.rs), the lobby's open events (lobby_form.rs)
// FUNCTION: EXE 0x402086
// Creates the join/host action for a mode, player name and game name
EEAJoinOrHostAGame::EEAJoinOrHostAGame(long inMode, const UWideString& inPlayerName, const UWideString& inGameName)
{
    this->mMode = inMode;
    this->mPlayerName = inPlayerName;
    this->mGameName = inGameName;
}

// SYNTHETIC: EXE 0x4020da
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4020f6
// Destroys the join/host action
EEAJoinOrHostAGame::~EEAJoinOrHostAGame()
{
}

// FUNCTION: EXE 0x402120
// Joins or hosts a multiplayer game in the chosen mode, starting the client or server and showing any error message
void EEAJoinOrHostAGame::Virtual4()
{
    UWideString theMessage;
    UString theAddress;
    UString theService;
    UWideString theGameName;
    EEGameOptionsPlayer thePlayer;
    bool theBehindFirewall = gWonNet ? gWonNet->mIsBehindFirewall == 1 : false;
    if (EEIsValidName(this->mPlayerName)) {
        if (this->mMode != 0 && this->mMode != 4) {
            if (gCommunications == NULL) {
                gCommunications = new EEClient;
                static_cast<EEClient*>(gCommunications)->StartThreads();
            } else if (gCommunications->IsServer() == true) {
                if (gCommunications)
                    delete gCommunications;
                gCommunications = NULL;
                gCommunications = new EEClient;
                dynamic_cast<EEClient*>(gCommunications)->StartThreads();
            }
        } else {
            if (gCommunications) {
                delete gCommunications;
                gCommunications = NULL;
            }
            if (EEIsValidName(this->mGameName)) {
                if (this->mMode == 4)
                    gCommunications = new EEServer(this->mGameName, gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
                else
                    gCommunications = new EEServer(this->mGameName, gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, true);
                static_cast<EEServer*>(gCommunications)->StartThreads();
                if (!static_cast<EEServer*>(gCommunications)->GetGameWasPublished()) {
                    gResource->GetString(IDS_GAME_NAME_ALREADY_EXISTS_ON_SUBNET, theMessage);
                    gUIManager->DisplayMessage(theMessage, true, NULL, EEAJoinOrHostAGameRestartClient, true);
                    if (gCommunications)
                        delete gCommunications;
                    gCommunications = NULL;
                    if (gWonNet) {
                        gWonNet->ClearGameList();
                        gWonNet->RejoinRoom(false);
                    }
                } else if (this->mMode != 4) {
                    gPlayerName = this->mPlayerName;
                    gGamePreferences.mSettings.mHostGameName = this->mGameName;
                }
            } else {
                gResource->GetString(IDS_GAME_NAME_IS_INVALID, theMessage);
                gUIManager->DisplayMessage(theMessage, true, NULL, EEAJoinOrHostAGameRestartClient, true);
            }
        }
        if (gCommunications) {
            gGameOptions.mLock->WriteLock();
            gGameOptions = gGamePreferences.GetLastGameOptions();
            gGameOptions.mIsMultiplayer = true;
            gGameOptions.mCheatCodes = false;
            gGameOptions.mLock->WriteUnlock();
            switch (this->mMode) {
            case 2:
            case 3: {
                theService = "EEService";
                theGameName = "";
                theAddress = this->mGameName;
                gCommunications->SearchForGame(false);
                if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
                    NEMClientConnectionRequest* theRequest =
                        new NEMClientConnectionRequest(this->mPlayerName, gGameVersion, gBinaryChecksum, theBehindFirewall);
                    static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
                    gCommunications->ProcessLocalUserInput(theRequest, kSlotNone);
                    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_LE_ACTIVATE_CLIENT);
                    gUIManager->GetFormByID(kFormLobby)->SetFormOpenEvent(theEvent);
                    theEvent->Dereference();
                    if (this->mMode == 3)
                        gGamePreferences.mSettings.mSierraUserID = this->mPlayerName;
                    else
                        gPlayerName = this->mPlayerName;
                }
                break;
            }
            case 1: {
                unsigned long theCount = static_cast<NESynchronousClient*>(gCommunications)->GetDetectedGameCount();
                for (unsigned long i = 0; i < theCount; i++) {
                    if (static_cast<NESynchronousClient*>(gCommunications)->GetDetectedGame(i, theAddress, theService,
                                                                                  theGameName) != 0)
                        break;
                    if (theService == "EEService" && theGameName == this->mGameName) {
                        if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
                            NEMClientConnectionRequest* theRequest = new NEMClientConnectionRequest(
                                this->mPlayerName, gGameVersion, gBinaryChecksum, theBehindFirewall);
                            static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
                            gCommunications->ProcessLocalUserInput(theRequest, kSlotNone);
                            gPlayerName = this->mPlayerName;
                            UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_LE_ACTIVATE_CLIENT);
                            gUIManager->GetFormByID(kFormLobby)->SetFormOpenEvent(theEvent);
                            theEvent->Dereference();
                        }
                        break;
                    }
                }
                break;
            }
            case 0:
            case 4: {
                UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_LE_ACTIVATE_SERVER);
                gUIManager->GetFormByID(kFormLobby)->SetFormOpenEvent(theEvent);
                theEvent->Dereference();
                thePlayer.mSlot = 0;
                thePlayer.mType = 1;
                thePlayer.mName = this->mPlayerName;
                thePlayer.mColor = 0;
                thePlayer.mTeam = 0;
                thePlayer.mStartingCitizens = gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
                thePlayer.mPing = 0.0f;
                thePlayer.mIsLocal = false;
                thePlayer.mIsReady = false;
                gCommunications->ProcessLocalUserInput(
                    new NEMClientConnectionRequest(this->mPlayerName, gGameVersion, gBinaryChecksum, theBehindFirewall), kSlotNone);
                break;
            }
            }
        }
    } else {
        gResource->GetString(IDS_PLAYER_NAME_IS_INVALID, theMessage);
        gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
    }
}

// SYNTHETIC: EXE 0x402769
// (NEMClientConnectionRequest's scalar deleting destructor: the game's own vtable copy of the imported class)

// FUNCTION: EXE 0x402786
// Message box callback: replaces the communications object with a freshly started client
void EEAJoinOrHostAGameRestartClient(UIForm* inForm)
{
    if (gCommunications) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEClient;
    static_cast<EEClient*>(gCommunications)->StartThreads();
}

// FUNCTION: EXE 0x4027c3
// Returns whether a player name is non-empty and free of control, quote, backslash and @ characters
bool EEIsValidName(const UWideString& inName)
{
    // the rule is Rust's (ee_screens single_player.rs valid_name), over each character's first byte
    unsigned long theLength = inName.GetLength();
    std::vector<unsigned char> theChars(theLength);
    for (unsigned long i = 0; i < theLength; i++)
        theChars[i] = (unsigned char)inName[(int)i];
    return ee_screens_valid_name(theChars.data(), theLength);
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAJoinOrHostAGame::Virtual8()
{
}
