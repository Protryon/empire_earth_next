// C:\EEX\Empire Earth\Actions\EEAServerMigration.cpp (file name inferred: no __FILE__; RTTI class
// EEAServerMigration, vtable 0x439090). The host migration action: 0x4a0b..0x505b (EEAScroll's Update ends at 0x4a0b,
// EEAUpgradeImprovement's ctor starts at 0x505b). ctor, Virtual4 ("Migrated Server"), then its first-use COMDATs:
// NEMClientReconnectionRequest's ??_G, map<uchar,uchar>::insert (EEServer::SetSendToSlotForPlayer), logic_error's dtor
// and _Rrotate.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
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
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAction.h"
#include "ee/EEAServerMigration.h"
#include "ee/EEStringIDs.h"
extern unsigned short& gLagStatus;           // 0x5318d4 (cleared after a migration; provisional name)

// FUNCTION: EXE 0x404a0b
// Creates the host migration action
EEAServerMigration::EEAServerMigration()
{
}

// FUNCTION: EXE 0x404a1f
// Migrates the game to a new host after the server drops: picks the next human, restarts as server or reconnects
void EEAServerMigration::Virtual4()
{
    UString theAddress;
    UString theService;
    UWideString theGameName;
    UWideString theMessage;
    UWideString theFormat;
    UWideString theDroppedName;
    UWideString theHostName;
    long theCount = 0;
    bool theFound = false;
    bool theIsHost = false;
    bool theHumanAvailable = false;
    gPhysics->RunOwedTicks();
    static_cast<EEClient*>(gCommunications)->mIsMigrating = true;
    if (static_cast<EEClient*>(gCommunications)->mHasLeftGame == 0) {
        unsigned long i;
        for (i = 1; i < ::gWorld.mNumPlayers; i++) {
            EEPlayer* thePlayer = ::gWorld.mPlayers[i];
            if (thePlayer->mSlot != kSlotServer && !thePlayer->mBehindFirewall && thePlayer->mHumanPlayer && thePlayer->mSlot != kSlotNone) {
                theHumanAvailable = true;
                break;
            }
        }
        if (!gLoadingSavedGame) {
            for (i = 0; i < ::gWorld.mNumPlayers; i++)
                ::gWorld.mPlayers[i]->mActiveGroup->Clear();
            ::gWorld.GetCurrentPlayer()->mSelectedGroup->CopySelection(::gWorld.GetCurrentPlayer()->mActiveGroup);
            static_cast<EEClient*>(gCommunications)->ResetFastSelection();
        }
        for (i = 1; i < ::gWorld.mNumPlayers; i++) {
            EEPlayer* thePlayer = ::gWorld.mPlayers[i];
            if (!thePlayer->mHumanPlayer)
                continue;
            if (thePlayer->mSlot == kSlotServer) {
                thePlayer->mSlot = kSlotNone;
                theDroppedName = thePlayer->mName;
            } else if (::gWorld.GetCurrentPlayer() == thePlayer) {
                if (!theFound && (!thePlayer->mBehindFirewall || !theHumanAvailable)) {
                    theIsHost = true;
                    theFound = true;
                    thePlayer->mSlot = kSlotServer;
                }
                theCount++;
            } else if (thePlayer->mSlot != kSlotNone) {
                if (!theFound && (!thePlayer->mBehindFirewall || !theHumanAvailable)) {
                    theIsHost = false;
                    theFound = true;
                    theAddress = static_cast<EEHumanPlayer*>(thePlayer)->GetName();
                    thePlayer->mSlot = kSlotServer;
                    theHostName = thePlayer->mName;
                } else {
                    thePlayer->mSlot = kSlotNone;
                }
                theCount++;
            }
        }
        if (theIsHost) {
            gResource->GetString(IDS_HOST_IS_GONE, theFormat);
            sprintf(theMessage, theFormat, (const char*)theDroppedName);
            gUserInterface->AddChatLogEntry(theMessage, 0);
            gResource->GetString(IDS_YOU_ARE_BEING_PROMOTED_TO_HOST, theMessage);
            gUserInterface->AddChatLogEntry(theMessage, 0);
            unsigned long theTick = gCommunications->GetNumberOfCTsProcessedInPhysics();
            unsigned long theCurrentTick = gPhysics->mCurrentTick;
            unsigned char theSelectionID = static_cast<EEClient*>(gCommunications)->mFastSelectionID;
            if (gCommunications) {
                delete gCommunications;
                gCommunications = NULL;
            }
            gCommunications = new EEServer(UWideString("Migrated Server"), gGamePreferences.mSettings.GetNetworkAdapter(), theTick, theCurrentTick, false, false);
            static_cast<EEServer*>(gCommunications)->StartThreads();
            gPhysics->ClearServerMessages();
            gCommunications->SetIsMultiplayerSession(true);
            static_cast<EEServer*>(gCommunications)->SetSendToSlotForPlayer(::gWorld.mCurrentPlayer, kSlotServer);
            ::gWorld.GetCurrentPlayer()->mSlot = 0xfe;
            static_cast<EEServer*>(gCommunications)->mFastSelectionID = theSelectionID;
            static_cast<EEServer*>(gCommunications)->mIsMigrating = true;
            if (theCount == 1)
                (static_cast<EEServer*>(gCommunications))->HandleReconnectWaitTimedOut();
            else
                (static_cast<EEServer*>(gCommunications))->StartWaitingForReconnects(theCount - 1);
        } else {
            PlatSleep(7000);
            gResource->GetString(IDS_HOST_IS_GONE, theFormat);
            sprintf(theMessage, theFormat, (const char*)theDroppedName);
            gUserInterface->AddChatLogEntry(theMessage, 0);
            gResource->GetString(IDS_ATTEMPTING_TO_CONNECT_TO_NEW_HOST, theFormat);
            sprintf(theMessage, theFormat, (const char*)theHostName);
            gUserInterface->AddChatLogEntry(theMessage, 0);
            gCommunications->SearchForGame(false);
            theService = "EEService";
            theGameName = "";
            if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
                gCommunications->SetLastTickReceived(-2);
                static_cast<NESynchronousClient*>(gCommunications)->SetMySendToSlot(kSlotNone);
                NEMClientReconnectionRequest* theRequest = new NEMClientReconnectionRequest(
                    gCommunications->GetNumberOfCTsProcessedInPhysics(), ::gWorld.mCurrentPlayer);
                gCommunications->ProcessLocalUserInput(theRequest, kSlotNone);
            }
        }
    }
    gLagStatus = 0;
}

// NEAR: exact with a user-declared empty virtual dtor in NEMClientReconnectionRequest (the original stores its own
// vptr first)
// FUNCTION: EXE 0x404f19
// Destroys the client reconnection request and frees it when asked (scalar deleting destructor)
// (NEMClientReconnectionRequest's ??_G: inline, first user)

// FUNCTION: EXE 0x404f45
// Inserts a key into the slot-to-slot map unless present; returns the position and whether it was added
// (map<unsigned char, unsigned char>::insert: EEServer::SetSendToSlotForPlayer's, first user)

// FUNCTION: EXE 0x404ffb
// Destroys a logic_error exception
// (std::logic_error's dtor)

// FUNCTION: EXE 0x405017
// Rotates the slot map's tree right around a node
// (map<unsigned char, unsigned char>::_Rrotate)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAServerMigration::Virtual8()
{
}
