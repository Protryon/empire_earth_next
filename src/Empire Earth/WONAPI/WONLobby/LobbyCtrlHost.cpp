// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby screens' controls' shell and host (R6 WONLobby round 4): GameBrowserCtrl, GameStagingCtrl,
// LobbyOptionsCtrl, GameUserListCtrl and the EE room's EELobbyGameOptionsCtrl are one Rust object each (rust/crates/ee_won_lobby/src/lobbyctl.rs). This unit is
// the shell's two virtuals, the host (each op the original's statements on a control, an event, the lobby's model,
// its settings or a global, with no decision). The two config readers that were in GameBrowserCtrl.cpp and
// GameUserListCtrl.cpp are lobby config kinds (lobbycfgrd.rs, R6 WON lobby/net round 40; LobbyNetCtrlHost.cpp).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "rust/won_net/http.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/won_gui/guiconf.h"
#include "rust/won_lobby/rows.h"
#include <set>
#include "ee/wonapi/WONLobbyResourceGlobals.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/lobbyids.h"

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
    case Op::MyPlayer: return (intptr_t)GetMyLobbyPlayer();
    case Op::PlayerSlot: return reinterpret_cast<LobbyPlayer*>(a)->MGet(LobbyMF::PlayerSlot);
    case Op::ColorSetColor: reinterpret_cast<EELobbyColorButton*>(a)->SetColor((int)b); return 0;
    case Op::ColorGetColor: return reinterpret_cast<EELobbyColorButton*>(a)->GetColor();
    default: break;
    }
    // round 13's ops (the Internet controls)
    return LobbyNetCtrlHost(aCtl, inOp, a, b, c);
}
}

// ---- WONLobbyResource.cpp until R6 WON GUI round 41 (the lobby's resource slots)
using namespace WONAPI;
// The lobby's resources: their slots, the table by id, registration, load and unload. The 469 resources are the
// rows of ee_won's table (rust/crates/ee_won_lobby/src/resource.rs: each one's section, key and kind in the original's load
// order, EeWonLobbyRes their names); the original's named globals are one slot per row (LOBBY_STRING(name) etc.).

// the original's globals (0x5663f4..0x566a9c) and its id table (0x566944..), one slot per row
LobbyResourceSlot gLobbyResources[EE_WON_LOBBYRES_COUNT];

// Registers the lobby's resource loader and control ids on the given (or a new) resource table and returns it
ResourceConfigTablePtr LobbyResource_Init(ResourceConfigTable* theTable)
{
    ResourceConfigTablePtr aTable=theTable;
    if(theTable==NULL)
        aTable=theTable=new ResourceConfigTable;
    theTable->RegisterGlueResourceFunc(LobbyResource_SafeGlueResources);
    // the controls by name, in the original's order (lobbyids.rs's rows)
    int anId=0;
    for(size_t i=0; const char* aName=ee_won_lobbyids_control(i,&anId); ++i)
        theTable->RegisterControlId(aName,anId);
    return aTable;
}

// Loads every lobby string, image and sound from its section of the resource table (ee_won's rows in order: a
// section is looked up again where it changes, as the original's statements did)
void LobbyResource_SafeGlueResources(ResourceConfigTable* theTable)
{
    ResourceConfig* aConfig = NULL;
    for (size_t i = 0; i < EE_WON_LOBBYRES_COUNT; ++i) {
        EeWonResource aRow;
        if (!ee_won_resource_row(i, &aRow))
            break;
        LobbyResourceSlot& aSlot = gLobbyResources[i];
        if (aRow.new_section)
            aConfig = theTable->SafeGetConfig(aRow.section);
        switch (aRow.kind) {
        case EeWonResKind::String: aSlot.mString = aConfig->SafeGetString(aRow.key); break;
        case EeWonResKind::Image: aSlot.mImage = aConfig->SafeGetImage(aRow.key); break;
        case EeWonResKind::Sound: aSlot.mSound = aConfig->SafeGetSound(aRow.key); break;
        }
    }
}

// Releases every lobby image and sound and empties every lobby string (ee_won's rows in order)
void LobbyResource_UnloadResources()
{
    for (size_t i = 0; i < EE_WON_LOBBYRES_COUNT; ++i) {
        LobbyResourceSlot& aSlot = gLobbyResources[i];
        switch (ee_won_lobbyres_kind((EeWonLobbyRes)i)) {
        case EeWonResKind::String: aSlot.mString.erase(); break;
        case EeWonResKind::Image: aSlot.mImage = NULL; break;
        case EeWonResKind::Sound: aSlot.mSound = NULL; break;
        }
    }
}
