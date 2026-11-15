// C:\EEX\Empire Earth\UI Events\EEUIEWonNetArenaRoomEvents.cpp (file name inferred: no __FILE__; class prefix
// EEUIEWNAR). The WON room list form's events: 0x1eafcb..0x1eb5e2. Unit flags: the exe module set.
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

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEWonNet.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUIEWonNetModifyUserAccountEvents.h"
#include "ee/EEUIEWonNetArenaRoomEvents.h"

static unsigned long sLastUpdateTime;

static long JoinSelectedRoom(UIForm* inForm);

// FUNCTION: EXE 0x5eafcb
// Refreshes the WON room list, its server name and game count every 10 seconds or when the list changed
long EEUIEWNARUpdateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWonNet) {
        if (TMGetTime() - sLastUpdateTime > 10000 || gWonNet->mRoomListChanged == true) {
            sLastUpdateTime = TMGetTime();
            UITextLabel* theLabel14 = dynamic_cast<UITextLabel*>(inForm->GetControl(kAvailableRoomsTotalGamesNumber));
            UITextLabel* theLabel16 = dynamic_cast<UITextLabel*>(inForm->GetControl(kAvailableRoomsServersNameLabel));
            EEUICWonNetRoomsBox* theRooms = dynamic_cast<EEUICWonNetRoomsBox*>(inForm->GetControl(kAvailableRoomsAvailRoomsListBox));
            gWonNet->UpdateRoomList(theRooms, theLabel16, theLabel14);
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb075
// Installs the room list's update event the first time and refreshes the room list, server name and game count
long EEUIEWNARActivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (inForm->GetFormUpdateEvent() == NULL) {
        UIEvent* theEvent = new EEUIEWNARUpdateForm;
        inForm->SetFormUpdateEvent(theEvent);
        theEvent->Dereference();
    }
    if (gWonNet) {
        UITextLabel* theLabel14 = dynamic_cast<UITextLabel*>(inForm->GetControl(kAvailableRoomsTotalGamesNumber));
        UITextLabel* theLabel16 = dynamic_cast<UITextLabel*>(inForm->GetControl(kAvailableRoomsServersNameLabel));
        EEUICWonNetRoomsBox* theRooms = dynamic_cast<EEUICWonNetRoomsBox*>(inForm->GetControl(kAvailableRoomsAvailRoomsListBox));
        gWonNet->UpdateRoomList(theRooms, theLabel16, theLabel14);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb12c
// Destroys the room list's update event and frees it when asked (scalar deleting destructor)
// Back below returns to the previous screen (ICF-folded into EEUIEWNMUACancel 0x5eb7e8: no address of its own)

long EEUIEWNARBack::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return inForm->GetManager()->Pop();
}

// FUNCTION: EXE 0x5eb157
// Removes the current form's update event when the room list is deactivated
long EEUIEWNARDeactivateForm::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUIManager->GetCurrentForm()->SetFormUpdateEvent(NULL);
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb172
// Joins the double-clicked WON room
long EEUIEWNARDoubleClickRoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return JoinSelectedRoom(inForm);
}

// FUNCTION: EXE 0x5eb17f
// Joins the WON room selected in the room list; always returns 0
static long JoinSelectedRoom(UIForm* inForm)
{
    UWideString theRoom;
    unsigned long theSelection;
    if (static_cast<EEUICWonNetRoomsBox*>(inForm->GetControl(kAvailableRoomsAvailRoomsListBox))->GetCurrentSelection(theSelection) == kErrorOK) {
        theRoom = static_cast<EEUICWonNetRoomsBox*>(inForm->GetControl(kAvailableRoomsAvailRoomsListBox))->GetData(theSelection).mName;
        if (gWonNet)
            gWonNet->JoinRoom(theRoom, NULL);
    }
    return 0;
}

// FUNCTION: EXE 0x5eb1ec
// Opens the update-account screen and asks WON for the account details
long EEUIEWNARClickUpdateAccount::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theUnused;
    UIEvent* theEvent = new EEUIEWNMUAOpenScreen;
    gUIManager->GetFormByID(kFormUpdateSierraAccount)->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    gUIManager->LockUIForCopyData();
    gUIManager->Push(gUIManager->GetFormByID(kFormUpdateSierraAccount));
    gUIManager->UnlockUIForCopyData();
    gWonNet->OpenUpdateAccount();
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb28d
// Destroys the update-account open event and frees it when asked (scalar deleting destructor)
// (??_G of EEUIEWNMUAOpenScreen)

// FUNCTION: EXE 0x5eb2b8
// Joins the selected WON room, at most once every 5 seconds
long EEUIEWNARJoinRoom::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static unsigned long sLastTime = 0;
    unsigned long theTime = TMGetTime();
    if (theTime - sLastTime > 5000) {
        sLastTime = theTime;
        JoinSelectedRoom(inForm);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb2e2
// Sets up the WON room list: hooks its buttons and list to their events, shows the user name and connects to the lobby
long EEUIEWNAROpenScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    EEUIEWNSetTriggerEvent(new EEUIEWNARBack, inForm->GetControl(kAvailableRoomsBackButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNARJoinRoom, inForm->GetControl(kAvailableRoomsJoinRoomButton), kControlTriggerButtonUp);
    EEUIEWNSetTriggerEvent(new EEUIEWNARDoubleClickRoom, inForm->GetControl(kAvailableRoomsAvailRoomsListBox), kControlTriggerDoubleClicked);
    EEUIEWNSetTriggerEvent(new EEUIEWNARDoubleClickRoom, inForm->GetControl(kAvailableRoomsAvailRoomsListBox), kControlTriggerEnterPressed);
    UIEvent* theEvent = new EEUIEWNARActivateForm;
    inForm->SetFormActivateEvent(theEvent);
    theEvent->Dereference();
    theEvent = new EEUIEWNARDeactivateForm;
    inForm->SetFormDeactivateEvent(theEvent);
    theEvent->Dereference();
    EEUIEWNSetTriggerEvent(new EEUIEWNARClickUpdateAccount, inForm->GetControl(kAvailableRoomsUpdateMyAccount), kControlTriggerButtonUp);
    UITextLabel* theUserLabel = static_cast<UITextLabel*>(inForm->GetControl(kAvailableRoomsPlayersNameLabel));
    std::string theUser = WONAPI::WStringToString(gWonNet->mUser->GetName());
    theUserLabel->SetText(UWideString(theUser.c_str()));
    if (gWonNet)
        gWonNet->ConnectToLobby();
    sLastUpdateTime = TMGetTime();
    return kErrorOK;
}

// FUNCTION: EXE 0x5eb4e0
// Destroys the room list's activate event and frees it when asked (scalar deleting destructor)
// (??_G in declaration order: ActivateForm, Back, DeactivateForm, DoubleClickRoom, ClickUpdateAccount, JoinRoom)

// FUNCTION: EXE 0x5eb50b
// Destroys the room list's back event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eb536
// Destroys the room list's deactivate event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eb561
// Destroys the room list's double-click event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eb58c
// Destroys the room list's update-account event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5eb5b7
// Destroys the room list's join event and frees it when asked (scalar deleting destructor)
