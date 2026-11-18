// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby screens' controls' shell and host (R6 WONLobby round 4): GameBrowserCtrl, GameStagingCtrl,
// LobbyOptionsCtrl, GameUserListCtrl and the EE room's EELobbyGameOptionsCtrl are one Rust object each (rust/crates/ee_won_lobby/src/lobbyctl.rs). This unit is
// the shell's two virtuals, the host (each op the original's statements on a control, an event, the lobby's model,
// its settings or a global, with no decision) and the two config readers that were in GameBrowserCtrl.cpp and GameUserListCtrl.cpp.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONURL.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/rows.h"

namespace WONAPI {
bool LobbyIAmReady();

// ---- the shell

static intptr_t LobbyCtrlHost(void* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c);
intptr_t LobbyNetCtrlHost(Container* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c);

// the lobby's rows (rows.rs) call this host with no control
[[maybe_unused]] static const bool sRowHost = (ee_won_lobby_rows_host_register(LobbyCtrlHost), true);

LobbyRustCtrl::LobbyRustCtrl(EeWonLobbyCtlKind inKind)
    : mLobbyCtl(ee_won_lobbyctl_new_model(static_cast<Container*>(this), inKind, LobbyCtrlHost)) {}

LobbyRustCtrl::~LobbyRustCtrl() { ee_won_lobbyctl_free(this->mLobbyCtl); }

// the originals looked their controls up for a config init only and did not call the base
void LobbyRustCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    this->CCall(EeWonLobbyCtlCall::Init, (intptr_t) static_cast<ComponentConfigInit&>(inInit).mConfig);
}

// a ComponentEvent's type as the controls' event (the rest Other)
static EeWonLobbyCtlEvent CtlEventOf(int inType)
{
    typedef EeWonLobbyCtlEvent E;
    E aType = E::Other;
    switch (inType) {
    case ComponentEvent_ButtonPressed: aType = E::ButtonPressed; break;
    case ComponentEvent_ComboSelChanged: aType = E::ComboSelChanged; break;
    case ComponentEvent_ListSelChanged: aType = E::ListSelChanged; break;
    case ComponentEvent_ListItemClicked: aType = E::ListItemClicked; break;
    case ComponentEvent_BrowserButtonChange: aType = E::BrowserButtonChange; break;
    case ComponentEvent_BrowserStatusChange: aType = E::BrowserStatusChange; break;
    case 100029: aType = E::JoinedGame; break;
    case 100032: aType = E::LeftGame; break;
    case LobbyEvent_SetRoomSpecFlags: aType = E::SetRoomSpecFlags; break;
    case 100061: aType = E::ReadyChanged; break;
    case LobbyEvent_ClientChanged: aType = E::ClientChanged; break;
    case LobbyEvent_PlayerChanged: aType = E::PlayerChanged; break;
    case 100014: aType = E::SyncOptions; break;
    case 100050: aType = E::LanOptions; break;
    case 100055: aType = E::PublicAddress; break;
    case 100065: aType = E::AllOptions; break;
    case LobbyEvent_EELobbySyncOption: aType = E::SyncOption; break;
    case LobbyEvent_EELobbySyncPlayer: aType = E::SyncPlayer; break;
    case 100064: aType = E::ColorChanged; break;
    case ComponentEvent_InputTextChanged: aType = E::InputTextChanged; break;
    case ComponentEvent_InputReturn: aType = E::InputReturn; break;
    case LobbyEvent_GameChanged: aType = E::GameChanged; break;
    case LobbyEvent_SyncGameList: aType = E::SyncGameList; break;
    case 100034: aType = E::FiltersChanged; break;
    case 100059: aType = E::RefilterAll; break;
    case 100058: aType = E::RefilterShown; break;
    case ComponentEvent_TextLinkActivated: aType = E::TextLinkActivated; break;
    case LobbyEvent_Alert: aType = E::Alert; break;
    case LobbyEvent_AnnounceFriend: aType = E::AnnounceFriend; break;
    case LobbyEvent_AnnounceClientLocation: aType = E::AnnounceClientLocation; break;
    case LobbyEvent_ClientModerated: aType = E::ClientModerated; break;
    case LobbyEvent_ChatCommandError: aType = E::ChatCommandError; break;
    case LobbyEvent_GroupInvitation: aType = E::GroupInvitation; break;
    case LobbyEvent_GroupJoinAttempt: aType = E::GroupJoinAttempt; break;
    case LobbyEvent_JoinedRoom: aType = E::JoinedRoom; break;
    case LobbyEvent_LeftRoom: aType = E::LeftRoom; break;
    case LobbyEvent_PlayerTeamChanged: aType = E::PlayerTeamChanged; break;
    case LobbyEvent_RecvChat: aType = E::RecvChat; break;
    case LobbyEvent_SetChatInput: aType = E::SetChatInput; break;
    case ComponentEvent_TreeItemOpened: aType = E::TreeItemOpened; break;
    case LobbyEvent_GroupChanged: aType = E::GroupChanged; break;
    case LobbyEvent_ServerChanged: aType = E::ServerChanged; break;
    case 100053: aType = E::SetRoomList; break;
    case LobbyEvent_FriendChanged: aType = E::FriendChanged; break;
    case 100056: aType = E::RefreshFriends; break;
    case LobbyEvent_LoginInitStatus: aType = E::LoginInitStatus; break;
    case LobbyEvent_SetLoginFields: aType = E::SetLoginFields; break;
    case 100052: aType = E::FillUserNames; break;
    case LobbyEvent_QueryAccountResult: aType = E::QueryAccountResult; break;
    case LobbyEvent_GotHTTPDocument: aType = E::GotHttpDocument; break;
    default: break;
    }
    return aType;
}

void LobbyRustCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->CCall(EeWonLobbyCtlCall::Event, (intptr_t)inEvent, (intptr_t)CtlEventOf(inEvent->mType), (intptr_t)inEvent->mComponent);
}

// the Dialog shell (QuickPlayCtrl, LoginCtrl): the same forwards
LobbyRustDialogCtrl::LobbyRustDialogCtrl(EeWonLobbyCtlKind inKind)
    : mLobbyCtl(ee_won_lobbyctl_new_model(static_cast<Container*>(this), inKind, LobbyCtrlHost)) {}
LobbyRustDialogCtrl::~LobbyRustDialogCtrl() { ee_won_lobbyctl_free(this->mLobbyCtl); }
void LobbyRustDialogCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ee_won_lobbyctl_call(this->mLobbyCtl, EeWonLobbyCtlCall::Init, (intptr_t) static_cast<ComponentConfigInit&>(inInit).mConfig, 0, 0);
}
void LobbyRustDialogCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    ee_won_lobbyctl_call(this->mLobbyCtl, EeWonLobbyCtlCall::Event, (intptr_t)inEvent,
                         (intptr_t)CtlEventOf(inEvent->mType), (intptr_t)inEvent->mComponent);
}

// ---- the host

template <class T> static Component* FindAs(ComponentConfig* inConfig, const EeWonLobbyCtlSlot* inSlot)
{
    T* aComponent = dynamic_cast<T*>(ComponentConfigGet(inConfig, inSlot->name));
    if (aComponent == NULL)
        ComponentConfigMissing(inSlot->name, inSlot->file, inSlot->line);
    return aComponent;
}

static Component* FindSlot(ComponentConfig* inConfig, const EeWonLobbyCtlSlot* inSlot)
{
    typedef EeWonLobbyCtlType T;
    switch (inSlot->ty) {
    case T::Button: return FindAs<Button>(inConfig, inSlot);
    case T::Label: return FindAs<Label>(inConfig, inSlot);
    case T::Combo: return FindAs<ComboBox>(inConfig, inSlot);
    case T::Input: return FindAs<InputBox>(inConfig, inSlot);
    case T::Browser: return FindAs<BrowserComponent>(inConfig, inSlot);
    case T::MultiList: return FindAs<MultiListArea>(inConfig, inSlot);
    case T::Image: return FindAs<ImageComponent>(inConfig, inSlot);
    case T::Container: return FindAs<Container>(inConfig, inSlot);
    case T::Component: return FindAs<Component>(inConfig, inSlot);
    case T::ColorButton: return FindAs<EELobbyColorButton>(inConfig, inSlot);
    case T::TextArea: return FindAs<TextArea>(inConfig, inSlot);
    case T::TreeArea: return FindAs<TreeArea>(inConfig, inSlot);
    case T::ListArea: return FindAs<ListArea>(inConfig, inSlot);
    case T::ScreenContainer: return FindAs<ScreenContainer>(inConfig, inSlot);
    case T::ServerListCtrl: return FindAs<ServerListCtrl>(inConfig, inSlot);
    }
    return NULL;
}

static const SmartPtr<Image>& CtlImage(intptr_t inImage)
{
    typedef EeWonLobbyCtlImage I;
    switch ((I)inImage) {
    case I::Ignored: return LOBBY_IMAGE(UserListCtrl_Ignored_Image);
    case I::Blocked: return LOBBY_IMAGE(UserListCtrl_Blocked_Image);
    case I::Muted: return LOBBY_IMAGE(UserListCtrl_Muted_Image);
    case I::Away: return LOBBY_IMAGE(UserListCtrl_Away_Image);
    case I::Admin: return LOBBY_IMAGE(UserListCtrl_Admin_Image);
    case I::Moderator: return LOBBY_IMAGE(UserListCtrl_Moderator_Image);
    case I::Captain: return LOBBY_IMAGE(UserListCtrl_Captain_Image);
    case I::Firewall: return LOBBY_IMAGE(LobbyGlobal_Firewall_Image);
    case I::Searching: return LOBBY_IMAGE(LobbyGlobal_Searching_Image);
    case I::Normal: break;
    }
    return LOBBY_IMAGE(UserListCtrl_Normal_Image);
}

static intptr_t LobbyCtrlHost(void* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    // the shell (a LobbyRustCtrl or a LobbyRustDialogCtrl) as its Container
    Container* aCtl = static_cast<Container*>(inCtl);
    Component* aComp = reinterpret_cast<Component*>(a);
    Button* aButton = reinterpret_cast<Button*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    BrowserComponent* aBrowser = reinterpret_cast<BrowserComponent*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    case Op::Find:
        return (intptr_t)FindSlot(reinterpret_cast<ComponentConfig*>(a), ee_won_lobbyctl_slot((uint32_t)b));
    case Op::FindOptional: {
        const EeWonLobbyCtlSlot* aSlot = ee_won_lobbyctl_slot((uint32_t)b);
        Component* aFound = reinterpret_cast<ComponentConfig*>(a)->Fn386860(aSlot->name);
        return aSlot->ty == EeWonLobbyCtlType::Component ? (intptr_t)aFound : (intptr_t)dynamic_cast<Button*>(aFound);
    }
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::SetControlId: aComp->mControlId = (int)b; return 0;
    case Op::SetEnabled: aComp->SetEnabled(b != 0); return 0;
    case Op::SetVisible: aComp->SetVisible(b != 0); return 0;
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::SubscribeBroadcast: aCtl->SubscribeToBroadcast(a != 0); return 0;
    case Op::BaseEvent: aCtl->Component::HandleComponentEvent(anEvent); return 0;
    case Op::BasePaint: aCtl->Container::Paint(*reinterpret_cast<Graphics*>(a)); return 0;
    case Op::EventControlId: return anEvent->GetControlId();
    case Op::EventFlags: return static_cast<SetRoomSpecFlagsEvent*>(anEvent)->mFlags;
    case Op::EventRoomSpec: return static_cast<ClientChangedEvent*>(anEvent)->mRoomSpec;
    case Op::EventIsModify: return static_cast<LobbyChangeEvent*>(anEvent)->IsModify() ? 1 : 0;
    case Op::EventIsAdd: return static_cast<LobbyChangeEvent*>(anEvent)->IsAdd() ? 1 : 0;
    case Op::EventIsDelete: return static_cast<LobbyChangeEvent*>(anEvent)->IsDelete() ? 1 : 0;
    case Op::ClientChangedClient: return (intptr_t) static_cast<ClientChangedEvent*>(anEvent)->mClient.get();
    case Op::PlayerChangedClient: return (intptr_t) static_cast<PlayerChangedEvent*>(anEvent)->mClient.get();
    case Op::EventIsSelectedItem: return static_cast<ListItemClickedEvent*>(anEvent)->IsSelectedItem() ? 1 : 0;
    case Op::EventButton: return static_cast<ListItemClickedEvent*>(anEvent)->mButton;
    case Op::EventDoubleClick: return static_cast<ListItemClickedEvent*>(anEvent)->mDoubleClick ? 1 : 0;
    case Op::BrowserBackEnabled: return aBrowser->BackButtonEnabled() ? 1 : 0;
    case Op::BrowserForwardEnabled: return aBrowser->ForwardButtonEnabled() ? 1 : 0;
    case Op::BrowserStopEnabled: return aBrowser->StopButtonEnabled() ? 1 : 0;
    case Op::BrowserFind:
        aBrowser->FindStr(reinterpret_cast<InputBox*>(b)->GetText(), (c & 1) != 0, (c & 2) != 0);
        return 0;
    case Op::BrowserReset: aBrowser->Reset(); return 0;
    case Op::BrowserGoHome: aBrowser->Go(static_cast<GameBrowserCtrl*>(static_cast<LobbyRustCtrl*>(aCtl))->mHomeLocation, false, 0, -1); return 0;
    case Op::BrowserBack: aBrowser->Back(); return 0;
    case Op::BrowserForward: aBrowser->Forward(); return 0;
    case Op::BrowserStop: aBrowser->Stop(); return 0;
    case Op::BrowserRefresh: aBrowser->Refresh(); return 0;
    case Op::LabelSetBrowserStatus:
        aLabel->SetText(reinterpret_cast<BrowserComponent*>(b)->GetCurStatus(), true);
        return 0;
    case Op::ButtonIsChecked: return aButton->IsChecked() ? 1 : 0;
    case Op::ButtonSetCheck: aButton->SetCheck(b != 0, c != 0); return 0;
    // ButtonSetText / LabelSetText: their strings come as resource rows (lobbyctl.rs string_row: ButtonSetRes,
    // LabelSetRes, LobbyNetCtrlHost.cpp's)
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameCanReady: return aGame->CanReady() ? 1 : 0;
    case Op::GameMatchesSpec: return aGame->MatchesRoomSpec((int)b) ? 1 : 0;
    case Op::IAmReady: return LobbyIAmReady() ? 1 : 0;
    case Op::ClientsAt: return (intptr_t)reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b);
    case Op::KickClient: LobbyKickClient(aClient, b != 0); return 0;
    case Op::DeliverClientAction:
        aCtl->DeliverEvent(new ClientActionEvent((int)a, (int)b, reinterpret_cast<LobbyClient*>(c)), false);
        return 0;
    case Op::BroadcastWhisper:
        LobbyBroadcastEvent(new SetChatInputEvent(2, aClient->GetName(), (int)b, false), true);
        return 0;
    case Op::ListGetSel: return (intptr_t)aList->GetSelItem();
    case Op::ListNumItems: return aList->GetNumItems();
    case Op::ListItem: return (intptr_t)aList->GetItem((int)b);
    case Op::ListRefresh: aList->RefreshItem(reinterpret_cast<ListItem*>(b)); return 0;
    case Op::ListInsert: aList->ListArea::InsertItem(reinterpret_cast<ListItem*>(b), -1); return 0;
    case Op::ListRemove: aList->RemoveItem(reinterpret_cast<ListItem*>(b)); return 0;
    case Op::ListClear: aList->Clear(b != 0); return 0;
    case Op::ListBeginUpdate: aList->BeginUpdate(); return 0;
    case Op::ListEndUpdate: aList->EndUpdate(); return 0;
    case Op::ButtonOf: return (intptr_t)dynamic_cast<Button*>(aComp);
    case Op::ControlId: return aComp->mControlId;
    case Op::ComboGetSel: return aCombo->GetSelItem();
    case Op::ComboSetSel: aCombo->SetSelItem((int)b, c != 0); return 0;
    case Op::ComboClear: aCombo->Clear(b != 0); return 0;
    case Op::ComboShowTabSelect: aCombo->SetTextAndSel(LobbyGetTabSelect(), false); return 0;
    case Op::SaveTabSelect: LobbySetTabSelect(aCombo->mEdit->GetText()); return 0;
    case Op::SaveLanName: LobbySetLanName(aCombo->mEdit->GetText()); return 0;
    case Op::ComboAddLanName: aCombo->AddString(LobbyGetLanNameAt((size_t)b), -1); return 0;
    case Op::ComboShowLanName: aCombo->SetTextAndSel(LobbyGetLanName(), true); return 0;
    case Op::LanBroadcastPort: return (intptr_t)LobbyGetLanBroadcastPort();
    case Op::LabelSetNumber: {
        char aPort[20]; // a 20-byte buffer, as in the original
        sprintf(aPort, "%d", (int)b);
        aLabel->SetText(aPort, true);
        return 0;
    }
    case Op::LabelSetLocalAddress: aLabel->SetText(IPAddr::GetLocalAddr().GetHostString(true), true); return 0;
    case Op::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)a, b != 0); return 0;
    case Op::ResetNetworkAdapter: LobbySetNetworkAdapter(LobbyGetNetworkAdapter()); return 0;
    case Op::CurrentScreen: return (intptr_t)LobbyGetCurrentScreen();
    case Op::BroadcastDown: aComp->BroadcastEventDown(new LobbyEvent((int)b)); return 0;
    case Op::MusicResume: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Resume(); return 0;
    case Op::MusicStop: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop(); return 0;
    case Op::ClickSoundMute: LOBBY_SOUND(LobbyGlobal_ButtonClick_Sound)->SetMuted(a != 0); return 0;
    case Op::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop(); return 0;
    case Op::OnLoginScreen: return LobbyOnLoginScreen() ? 1 : 0;
    case Op::OnLanScreen: return LobbyOnLanScreen() ? 1 : 0;
    case Op::FirewallTestDone: return LobbyIsFirewallTestDone() ? 1 : 0;
    case Op::BehindFirewall: return LobbyIsBehindFirewall() ? 1 : 0;
    case Op::IconSetImage: reinterpret_cast<ImageComponent*>(a)->SetImage(CtlImage(b).get()); return 0;
    case Op::PublicAddressValid: return LobbyGetPublicAddress().IsValid() ? 1 : 0;
    case Op::LabelSetPublicAddress: aLabel->SetText(LobbyGetPublicAddress().GetHostString(true), true); return 0;
    // EELobbyGameOptionsCtrl
    case Op::ReportInvalidType: {
        const EeWonLobbyCtlSlot* aSlot = ee_won_lobbyctl_slot((uint32_t)a);
        ComponentConfigMissing(std::string("Invalid type for ") + aSlot->name, aSlot->file, 89);
        return 0;
    }
    case Op::IsButton: return dynamic_cast<Button*>(aComp) != NULL ? 1 : 0;
    case Op::IsCombo: return dynamic_cast<ComboBox*>(aComp) != NULL ? 1 : 0;
    case Op::ComboAddNumber: {
        char aBuffer[50];
        sprintf(aBuffer, "%d", (int)b);
        aCombo->AddString(aBuffer);
        return 0;
    }
    case Op::FillMapTypes: EELobbyFillMapTypeCombo(aCombo); return 0;
    case Op::ComboHasSel: return aCombo->mListArea->GetSelItem() != NULL ? 1 : 0;
    case Op::GameRust: return (intptr_t) static_cast<EELobbyGame*>(aGame)->mRust;
    case Op::LabelSetName: aLabel->SetText(EELobbyStr(b)); return 0;
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::EventParam: return static_cast<EELobbySyncOptionEvent*>(anEvent)->mParam;
    case Op::ChooseSavedGame: {
        // the saved games' directory, the file dialog over its .ees files (no dialog: -1)
        GUIString theDirectory = EELobbyFn2988E9();
        SmartPtr<EELobbyFileDialog> theDialog = EELobbyGetFileDialog();
        int theResult = -1;
        if (theDialog.mPtr != NULL) {
            theDialog->ListFiles(theDirectory, ".ees");
            theResult = LobbyDoDialog(theDialog.mPtr);
        }
        return theResult;
    }
    // EELobbyPlayerOptionsCtrl
    case Op::ComboSetName: aCombo->SetText(EELobbyStr(b)); return 0;
    case Op::ComboSetTextColor: aCombo->mEdit->mTextColor = (unsigned long)b; return 0;
    case Op::MyPlayerReady: return GetMyLobbyPlayer()->GetReady() ? 1 : 0;
    case Op::MyPlayer: return (intptr_t) static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    case Op::PlayerSlot: return reinterpret_cast<EELobbyPlayer*>(a)->GetSlot();
    case Op::ColorSetColor: reinterpret_cast<EELobbyColorButton*>(a)->SetColor((int)b); return 0;
    case Op::ColorGetColor: return reinterpret_cast<EELobbyColorButton*>(a)->GetColor();
    case Op::LabelSetPercent: {
        char theBuffer[50];
        sprintf(theBuffer, "%d", (int)b);
        aLabel->SetText(LobbyFormatString(EE_LOBBY_STRING(gEELobbyPlayerOptionsCtrl_TransferPercent), theBuffer));
        return 0;
    }
    default: break;
    }
    // round 13's ops (the Internet controls)
    return LobbyNetCtrlHost(aCtl, inOp, a, b, c);
}

// ---- the config readers (GameBrowserCtrl.cpp's and GameUserListCtrl.cpp's)

// Creates a config for a browser tab in the layout file
ComponentConfig* GameBrowserCtrlConfig::CfgFactory()
{
    return new GameBrowserCtrlConfig;
}

// The instruction keys and the flag and value names these readers compare are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// Reads the HOMELOCATION layout key, resolving a file URL to the resource file path
bool GameBrowserCtrlConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_GAMEBROWSERCTRLCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(ContainerConfig::Virtual24(instruction))
        return true;
    if(aKeyIdx==0)
    {
        std::string path=this->ReadString(false);
        URL url=path;
        if(url.GetType()==URLType_File)
            path=ComponentConfig::GetResourceFile(path);
        this->mBrowser->SetHomeLocation(path);
    }
    else
        return false;
    return true;
}

// Creates the layout reader for a game player list
ComponentConfig* GameUserListCtrlConfig::CfgFactory()
{
    return new GameUserListCtrlConfig;
}

// Accepts (and ignores) the CTRLLINKID layout key; passes other keys to the container's reader
bool GameUserListCtrlConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) { }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
}
