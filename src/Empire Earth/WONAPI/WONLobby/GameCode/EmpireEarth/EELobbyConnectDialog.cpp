// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyConnectDialog.cpp (__FILE__ 0x8d03b0, InitComponent's
// lines 61..62). The dialog the players wait in while a game launches: one list line per player, with the ready
// icon once the player answered; it ends by itself when all have (End) or with its cancel button:
// 0x6909a1..0x690e89 (the two SafeGetComponent functions close it; EELobbyFileDialog.cpp's ctor follows). The list
// line class lives in the file's anonymous namespace (RTTI ConnectListItem@?%<file><hash>@@).
#include <windows.h>
#include <string>
#include <list>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;

extern GUIString gEELobbyConnectDialog_AbortLaunch;    // 0x96598c (EELobbyResource.cpp)
extern GUIString gEELobbyConnectDialog_LeaveGame;      // 0x965948 (EELobbyResource.cpp)

namespace {
// One player's line (0x2c bytes; vtable 0x84c2c0: TextListItem's slots)
class ConnectListItem : public TextListItem {
public:
    ConnectListItem(const GUIString& inName, Image* inImage, unsigned char inSlot) : TextListItem(inName, inImage)
    {
        this->mSlot = inSlot;
    }

    unsigned char mSlot;                    // +0x28
};
}

// FUNCTION: EXE 0x6909a1
// Creates the launch wait dialog, not running
EELobbyConnectDialog::EELobbyConnectDialog() : mIsRunning(false)
{
}

// FUNCTION: EXE 0x6909f6
// Destroys the launch wait dialog (members only)
EELobbyConnectDialog::~EELobbyConnectDialog()
{
}

// FUNCTION: EXE 0x690a41
// Looks up the player list and the cancel button from the dialog's layout
void EELobbyConnectDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType == 2) {
        ComponentConfig* theConfig = ((ComponentConfigInit&)inInit).mConfig;
        SafeGetComponent(theConfig, this->mConnectList, "ConnectList", __FILE__, 61);
        SafeGetComponent(theConfig, this->mCancelButton, "CancelButton", __FILE__, 62);
    }
}

// FUNCTION: EXE 0x690a8a
// Lists the game's players and waits until all have connected; returns whether the launch goes ahead
// Only the host's line starts out ready. ConnectListItem's vtable is named after the anonymous namespace
// (?%<file><number>), whose number changes with every compile (the original's is 2082521601)
bool EELobbyConnectDialog::Run(EELobbyGame* inGame)
{
    if (this->mIsRunning)
        return false;
    bool theAllConnected = true;
    this->mCancelButton->SetText(inGame->mIsHost ? gEELobbyConnectDialog_AbortLaunch : gEELobbyConnectDialog_LeaveGame, true);
    this->mConnectList->BeginUpdate();
    this->mConnectList->Clear(true);
    for (int i = 0; i < 8; i++) {
        EELobbyPlayerSlot* theSlot = inGame->GetPlayerSlot(i);
        if (theSlot != NULL && theSlot->mOptions[0] == 3) {
            Image* theImage = (i == 0 ? ::GameUserListCtrl_Ready_Image : ::LobbyGlobal_Searching_Image).mPtr;
            this->mConnectList->InsertItem(new ConnectListItem(theSlot->GetName(), theImage, i), -1);
            if (theImage != ::GameUserListCtrl_Ready_Image.mPtr)
                theAllConnected = false;
        }
    }
    this->mConnectList->EndUpdate();
    if (theAllConnected)
        return true;
    this->mConnectList->RequestFocus();
    this->mIsEnded = false;
    this->mIsRunning = true;
    int theResult = LobbyDoDialog(this);
    this->mIsRunning = false;
    return theResult == 100001;
}

// FUNCTION: EXE 0x690c85
// Closes the dialog as launched (OK) or cancelled
void EELobbyConnectDialog::End(bool inOK)
{
    Window* theRoot = this->GetRootWindow();
    if (theRoot != NULL) {
        this->mIsEnded = true;
        theRoot->EndDialog(inOK ? 100001 : 100002, this);
    }
}

// FUNCTION: EXE 0x690cb3
// Marks a player slot as connected or failed and launches once every listed player is connected
void EELobbyConnectDialog::SetConnected(unsigned char inSlot, bool inConnected)
{
    if (!this->mIsRunning)
        return;
    bool theAllConnected = true;
    SmartPtr<ListItem> theChanged;
    for (int i = 0; i < this->mConnectList->GetNumItems(); i++) {
        ConnectListItem* theItem = (ConnectListItem*)this->mConnectList->GetItem(i);
        if (theItem->mSlot == inSlot) {
            theItem->mImage = (inConnected ? ::GameUserListCtrl_Ready_Image : ::ServerListCtrl_PingFailed_Image).mPtr;
            theChanged = theItem;
        }
        if (theItem->mImage.mPtr != ::GameUserListCtrl_Ready_Image.mPtr)
            theAllConnected = false;
    }
    if (theChanged.mPtr != NULL)
        this->mConnectList->RefreshItem(theChanged.mPtr);
    if (theAllConnected)
        this->End(true);
}

// The unit's compiler-generated functions:
// SYNTHETIC: EXE 0x6909c7
// (Container::FindClickChildAt, inline)
// SYNTHETIC: EXE 0x6909da
// (??_G EELobbyConnectDialog)
// SYNTHETIC: EXE 0x690a26
// (Dialog's implicit dtor)
// SYNTHETIC: EXE 0x690bb4
// (ListItem::CanSelect, inline)
// SYNTHETIC: EXE 0x690bba
// (ListItem::SetPos, inline)
// SYNTHETIC: EXE 0x690bc4
// (ListItem::Contains, inline)
// SYNTHETIC: EXE 0x690be2
// (TextListItem's inline ctor)
// SYNTHETIC: EXE 0x690c34
// (??_G TextListItem; ICF-shared with ConnectListItem's)
// SYNTHETIC: EXE 0x690c50
// (TextListItem's inline dtor)
// SYNTHETIC: EXE 0x690d70
// (ListArea::GetNumItems, inline)
// TEMPLATE: EXE 0x690d89
// (SafeGetComponent<ListArea>)
// TEMPLATE: EXE 0x690e09
// (SafeGetComponent<Button>)
