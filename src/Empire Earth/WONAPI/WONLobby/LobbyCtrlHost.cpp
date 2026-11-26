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
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbycfgrd.h"
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

// the event's type goes as it is: the Rust chooses the controls' event (evmap.rs)
void LobbyRustCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->CCall(EeWonLobbyCtlCall::RawEvent, (intptr_t)inEvent, inEvent->mEv.kind, inEvent->mEv.comp);
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
    ee_won_lobbyctl_call(this->mLobbyCtl, EeWonLobbyCtlCall::RawEvent, (intptr_t)inEvent, inEvent->mEv.kind,
                         inEvent->mEv.comp);
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
    case Op::SetEnabled: aComp->SetEnabled(b != 0); return 0;
    case Op::SetVisible: aComp->SetVisible(b != 0); return 0;
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::EventIsSelectedItem: return anEvent->GetItem<ListItem>() == static_cast<ListArea*>(anEvent->GetComponent())->GetSelItem() ? 1 : 0;
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
    // ButtonSetText / LabelSetText: their strings come as resource rows (lobbyctl.rs string_row: ButtonSetRes,
    // LabelSetRes, LobbyNetCtrlHost.cpp's)
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameCanReady: return aGame->CanReady() ? 1 : 0;
    case Op::GameMatchesSpec: return aGame->MatchesRoomSpec((int)b) ? 1 : 0;
    case Op::IAmReady: return LobbyIAmReady() ? 1 : 0;
    case Op::ClientsAt: return (intptr_t)reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b);
    case Op::KickClient: LobbyKickClient(aClient, b != 0); return 0;
    case Op::DeliverClientAction:
        aCtl->DeliverEvent(ClientActionEvent((int)a, (int)b, reinterpret_cast<LobbyClient*>(c)), false);
        return 0;
    case Op::BroadcastWhisper:
        LobbyBroadcastEvent(SetChatInputEvent(2, aClient->GetName(), (int)b, false), true);
        return 0;
    case Op::ButtonOf: return (intptr_t)dynamic_cast<Button*>(aComp);
    case Op::ComboShowTabSelect: aCombo->SetTextAndSel(LobbyGetTabSelect(), false); return 0;
    case Op::SaveTabSelect: LobbySetTabSelect(aCombo->mEdit->GetText()); return 0;
    case Op::SaveLanName: LobbySetLanName(aCombo->mEdit->GetText()); return 0;
    case Op::ComboAddLanName: aCombo->AddString(LobbyGetLanNameAt((size_t)b), -1); return 0;
    case Op::ComboShowLanName: aCombo->SetTextAndSel(LobbyGetLanName(), true); return 0;
    case Op::LanBroadcastPort: return (intptr_t)LobbyGetLanBroadcastPort();
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
    // IconSetImage: its picture comes as a resource row (rows.rs icon_res)
    case Op::IconSetRes: reinterpret_cast<ImageComponent*>(a)->SetImage(gLobbyResources[b].mImage.get()); return 0;
    case Op::PublicAddressValid: return LobbyGetPublicAddress().IsValid() ? 1 : 0;
    case Op::LabelSetPublicAddress: aLabel->SetText(LobbyGetPublicAddress().GetHostString(true), true); return 0;
    // EELobbyGameOptionsCtrl
    case Op::ReportInvalidType: ComponentConfigMissing(std::string("Invalid type for ") + ee_won_lobbyctl_slot((uint32_t)a)->name, ee_won_lobbyctl_slot((uint32_t)a)->file, 89); return 0;
    case Op::IsButton: return dynamic_cast<Button*>(aComp) != NULL ? 1 : 0;
    case Op::IsCombo: return dynamic_cast<ComboBox*>(aComp) != NULL ? 1 : 0;
    case Op::FillMapTypes: EELobbyFillMapTypeCombo(aCombo); return 0;
    case Op::ComboHasSel: return aCombo->mListArea->GetSelItem() != NULL ? 1 : 0;
    case Op::GameRust: return (intptr_t) aGame->mEE;
    case Op::LabelSetName: aLabel->SetText(EELobbyStr(b)); return 0;
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::EventParam: return LobbyEvtGet(anEvent, LobbyEvtField::Value);
    // EELobbyPlayerOptionsCtrl
    case Op::ComboSetName: aCombo->SetText(EELobbyStr(b)); return 0;
    case Op::ComboSetTextColor: aCombo->mEdit->mTextColor = (unsigned long)b; return 0;
    case Op::MyPlayerReady: return GetMyLobbyPlayer()->GetReady() ? 1 : 0;
    case Op::MyPlayer: return (intptr_t) static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    case Op::PlayerSlot: return reinterpret_cast<EELobbyPlayer*>(a)->GetSlot();
    case Op::ColorSetColor: reinterpret_cast<EELobbyColorButton*>(a)->SetColor((int)b); return 0;
    case Op::ColorGetColor: return reinterpret_cast<EELobbyColorButton*>(a)->GetColor();
    default: break;
    }
    // round 13's ops (the Internet controls)
    return LobbyNetCtrlHost(aCtl, inOp, a, b, c);
}

// ---- the config readers (GameBrowserCtrl.cpp's and GameUserListCtrl.cpp's)


// The instruction keys and the flag and value names these readers compare are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// the readers' statements (lobbycfgrd.rs composes them, R6 WON round 33); HOMELOCATION's path is this static
// the config readers' reads (LobbyNetCtrlHost.cpp)
intptr_t LobbyCfgReadHost(ComponentConfig* c, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc);
static std::string sCfgRdPath;
static intptr_t CfgRdHost(void* inCfg, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyCfgRdOp Op;
    ComponentConfig* c = static_cast<ComponentConfig*>(inCfg);
    GameBrowserCtrlConfig* gb = static_cast<GameBrowserCtrlConfig*>(c);
    GameUserListCtrlConfig* gu = static_cast<GameUserListCtrlConfig*>(c);
    switch (inOp) {
    case Op::BrowserBase: return WON_GUARDED(gb->ContainerConfig::Virtual24(*(const std::string*)a));
    case Op::BrowserPathRead: return WON_GUARDED_V(sCfgRdPath = (std::string)c->ReadString(false));
    case Op::BrowserPathIsFile: return WON_GUARDED(URL(sCfgRdPath).GetType() == URLType_File);
    case Op::BrowserPathResolve: return WON_GUARDED_V(sCfgRdPath = ComponentConfig::GetResourceFile(sCfgRdPath));
    case Op::BrowserSetHome: return WON_GUARDED_V(gb->mBrowser->SetHomeLocation(sCfgRdPath));
    case Op::GameUserListBase: return WON_GUARDED(gu->ContainerConfig::Virtual24(*(const std::string*)a));
    default: return LobbyCfgReadHost(c, inOp, a, b, cc);
    }
}
static intptr_t CfgRd(ComponentConfig* inCfg, EeWonLobbyCfgRdCall inCall, const std::string& inKey, intptr_t d = 0) { return WonRethrow(ee_won_lobbycfgrd_call(inCfg, CfgRdHost, inCall, (intptr_t)inKey.data(), (intptr_t)inKey.size(), (intptr_t)&inKey, d)); }

// Reads the HOMELOCATION layout key, resolving a file URL to the resource file path
bool GameBrowserCtrlConfig::Virtual24(const std::string& instruction) { return CfgRd(this, EeWonLobbyCfgRdCall::GameBrowser, instruction) != 0; }


// Accepts (and ignores) the CTRLLINKID layout key; passes other keys to the container's reader
bool GameUserListCtrlConfig::Virtual24(const std::string& instruction) { return CfgRd(this, EeWonLobbyCfgRdCall::GameUserList, instruction) != 0; }
}
