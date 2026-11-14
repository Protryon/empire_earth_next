// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby screens' controls' shell and host (R6 WONLobby round 4): GameBrowserCtrl, GameStagingCtrl,
// LobbyOptionsCtrl, GameUserListCtrl and the EE room's EELobbyGameOptionsCtrl are one Rust object each (rust/crates/ee_won/src/lobbyctl.rs). This unit is
// the shell's two virtuals, the host (each op the original's statements on a control, an event, the lobby's model,
// its settings or a global, with no decision), the player row GameUserListItem (a list row: paint, size, order; its
// icon rule is Rust) and the two config readers that were in GameBrowserCtrl.cpp and GameUserListCtrl.cpp.
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
#include "rust/ee_won.h"

namespace WONAPI {
bool LobbyIAmReady();

// ---- the player row (GameUserListCtrl's list; the original's GameUserListCtrl.cpp)

// FUNCTION: WON 0x007bcad0
// Creates a game player list entry for a client
GameUserListItem::GameUserListItem(LobbyClient* inClient) : mClient(inClient) {}
// FUNCTION: WON 0x007bcdb0
// Sizes the entry to the list font's height, or the icon's if taller
void GameUserListItem::CalcDimensions(ListArea* inArea)
{
    this->mFont=inArea->mFont;
    this->mHeight=this->mFont->GetHeight();
    if(this->mImage.get()!=NULL && this->mImage->mHeight>this->mHeight) this->mHeight=this->mImage->mHeight;
}
// FUNCTION: WON 0x007bce10
// Orders the game captain first, then players by name ignoring case
int GameUserListItem::Compare(const ListItem* inOther,ListArea*) const
{
    const GameUserListItem* other=static_cast<const GameUserListItem*>(inOther);
    int mine=this->mClient->IsCaptain(1)?1:0;
    int theirs=other->mClient->IsCaptain(1)?1:0;
    if(mine!=theirs) return theirs-mine;
    return this->mClient->GetName().CompareNoCase(other->mClient->GetName());
}
// The ready mark is centered by the status icon's height, not its own (as in the original)
// FUNCTION: WON 0x007bce60
// Draws one column of the entry: selection background, status icon, ready mark, name or ping bars
void GameUserListItem::Paint(Graphics& inGraphics,ListArea* inArea)
{
    MultiListArea* area=static_cast<MultiListArea*>(inArea);
    int column=area->GetColumnPaintContext();
    unsigned long color=inGraphics.GetStandardColor(StandardColor_Text);
    if(this->mSelected) color=area->GetSelColor().GetForegroundColor(inGraphics,color);
    inGraphics.SetColor(color);
    inGraphics.SetFont(this->mFont.get());
    switch(column) {
    case -1:
        if(this->mSelected) {
            inGraphics.SetColor(area->GetSelColor().GetBackgroundColor(inGraphics));
            inGraphics.FillRect(0,0,area->GetPaintWidth(),this->mHeight);
        }
        return;
    case 0:
        inGraphics.DrawImage(this->mImage.get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 1:
        if(this->mClient->IsReady())
            inGraphics.DrawImage(::GameUserListCtrl_Ready_Image.get(),0,(this->mHeight-this->mImage->mHeight)/2);
        return;
    case 2:
        inGraphics.SetFont(this->mFont.get());
        inGraphics.DrawString(this->mClient->GetName(),0,(this->mHeight-this->mFont->GetHeight())/2);
        return;
    case 3:
        DrawLobbyPing(&inGraphics,this->mClient->GetPing(),this->mHeight-2,0,0);
        return;
    }
}

// ---- the shell

static intptr_t LobbyCtrlHost(void* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c);

LobbyRustCtrl::LobbyRustCtrl(EeWonLobbyCtlKind inKind) : mLobbyCtl(ee_won_lobbyctl_new(this, inKind, LobbyCtrlHost)) {}

LobbyRustCtrl::~LobbyRustCtrl() { ee_won_lobbyctl_free(this->mLobbyCtl); }

// the originals looked their controls up for a config init only and did not call the base
void LobbyRustCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    this->CCall(EeWonLobbyCtlCall::Init, (intptr_t) static_cast<ComponentConfigInit&>(inInit).mConfig);
}

void LobbyRustCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    typedef EeWonLobbyCtlEvent E;
    E aType = E::Other;
    switch (inEvent->mType) {
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
    default: break;
    }
    this->CCall(EeWonLobbyCtlCall::Event, (intptr_t)inEvent, (intptr_t)aType, (intptr_t)inEvent->mComponent);
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
    }
    return NULL;
}

static const GUIString& CtlString(intptr_t inString)
{
    typedef EeWonLobbyCtlString S;
    switch ((S)inString) {
    case S::Ready: return ::GameStagingCtrl_Ready_String;
    case S::NotReady: return ::GameStagingCtrl_NotReady_String;
    case S::StartGame: return ::GameStagingCtrl_StartGame_String;
    case S::LeaveGame: return ::GameStagingCtrl_LeaveGame_String;
    case S::DissolveGame: return ::GameStagingCtrl_DissolveGame_String;
    case S::Ban: return ::GameUserListCtrl_Ban_String;
    case S::Kick: return ::GameUserListCtrl_Kick_String;
    case S::CheckingAddress: return ::LobbyOptions_CheckingAddress_String;
    case S::AddressUnavailable: return ::LobbyOptions_AddressUnavailable_String;
    }
    return gEmptyGUIString;
}

static const SmartPtr<Image>& CtlImage(intptr_t inImage)
{
    typedef EeWonLobbyCtlImage I;
    switch ((I)inImage) {
    case I::Ignored: return ::UserListCtrl_Ignored_Image;
    case I::Blocked: return ::UserListCtrl_Blocked_Image;
    case I::Muted: return ::UserListCtrl_Muted_Image;
    case I::Away: return ::UserListCtrl_Away_Image;
    case I::Admin: return ::UserListCtrl_Admin_Image;
    case I::Moderator: return ::UserListCtrl_Moderator_Image;
    case I::Captain: return ::UserListCtrl_Captain_Image;
    case I::Firewall: return ::LobbyGlobal_Firewall_Image;
    case I::Searching: return ::LobbyGlobal_Searching_Image;
    case I::Normal: break;
    }
    return ::UserListCtrl_Normal_Image;
}

static bool ClientIs(LobbyClient* inClient, intptr_t inTest)
{
    typedef EeWonLobbyCtlClientTest T;
    switch ((T)inTest) {
    case T::IsIgnored: return inClient->IsIgnored();
    case T::IsBlocked: return inClient->IsBlocked();
    case T::IsMuted: return inClient->IsMuted(true);
    case T::IsAway: return inClient->IsAway();
    case T::IsAdmin: return inClient->IsAdmin();
    case T::IsModerator: return inClient->IsModerator();
    case T::IsCaptain: return inClient->IsCaptain(1);
    case T::HasPlayer: return inClient->HasPlayer();
    }
    return false;
}

static intptr_t PrefGet(intptr_t inPref)
{
    typedef EeWonLobbyCtlPref P;
    switch ((P)inPref) {
    case P::CrossPromotion: return LobbyGetCrossPromotion();
    case P::DoAutoAfk: return LobbyGetDoAutoAFK();
    case P::AutoPingGames: return LobbyGetAutoPingGames();
    case P::ShowGamesInProgress: return LobbyGetShowGamesInProgress();
    case P::LobbyMusic: return LobbyGetLobbyMusic();
    case P::LobbySoundEffects: return LobbyGetLobbySoundEffects();
    case P::ChatSoundEffects: return LobbyGetChatSoundEffects();
    case P::StagingSoundEffects: return LobbyGetStagingSoundEffects();
    case P::ShowConfirmations: return LobbyGetShowConfirmations();
    case P::FilterDirtyWords: return LobbyGetFilterDirtyWords();
    case P::AnonymousToFriends: return LobbyGetAnonymousToFriends();
    case P::ConnectionSpeed: return LobbyGetConnectionSpeed();
    }
    return 0;
}

static void PrefSet(intptr_t inPref, intptr_t inValue)
{
    typedef EeWonLobbyCtlPref P;
    bool aFlag = inValue != 0;
    switch ((P)inPref) {
    case P::CrossPromotion: LobbySetCrossPromotion(aFlag); return;
    case P::DoAutoAfk: LobbySetDoAutoAFK(aFlag); return;
    case P::AutoPingGames: LobbySetAutoPingGames(aFlag); return;
    case P::ShowGamesInProgress: LobbySetShowGamesInProgress(aFlag); return;
    case P::LobbyMusic: LobbySetLobbyMusic(aFlag); return;
    case P::LobbySoundEffects: LobbySetLobbySoundEffects(aFlag); return;
    case P::ChatSoundEffects: LobbySetChatSoundEffects(aFlag); return;
    case P::StagingSoundEffects: LobbySetStagingSoundEffects(aFlag); return;
    case P::ShowConfirmations: LobbySetShowConfirmations(aFlag); return;
    case P::FilterDirtyWords: LobbySetFilterDirtyWords(aFlag); return;
    case P::AnonymousToFriends: LobbySetAnonymousToFriends(aFlag); return;
    case P::ConnectionSpeed: LobbySetConnectionSpeed((int)inValue); return;
    }
}

static intptr_t LobbyCtrlHost(void* inCtl, EeWonLobbyCtlOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyCtlOp Op;
    LobbyRustCtrl* aCtl = static_cast<LobbyRustCtrl*>(inCtl);
    Component* aComp = reinterpret_cast<Component*>(a);
    Button* aButton = reinterpret_cast<Button*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    BrowserComponent* aBrowser = reinterpret_cast<BrowserComponent*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    GameUserListItem* anItem = reinterpret_cast<GameUserListItem*>(a);
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    case Op::Find:
        return (intptr_t)FindSlot(reinterpret_cast<ComponentConfig*>(a), ee_won_lobbyctl_slot((uint32_t)b));
    case Op::FindOptional:
        return (intptr_t)dynamic_cast<Button*>(
            reinterpret_cast<ComponentConfig*>(a)->Fn386860(ee_won_lobbyctl_slot((uint32_t)b)->name));
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
    case Op::BrowserGoHome: aBrowser->Go(static_cast<GameBrowserCtrl*>(aCtl)->mHomeLocation, false, 0, -1); return 0;
    case Op::BrowserBack: aBrowser->Back(); return 0;
    case Op::BrowserForward: aBrowser->Forward(); return 0;
    case Op::BrowserStop: aBrowser->Stop(); return 0;
    case Op::BrowserRefresh: aBrowser->Refresh(); return 0;
    case Op::LabelSetBrowserStatus:
        aLabel->SetText(reinterpret_cast<BrowserComponent*>(b)->GetCurStatus(), true);
        return 0;
    case Op::ButtonIsChecked: return aButton->IsChecked() ? 1 : 0;
    case Op::ButtonSetCheck: aButton->SetCheck(b != 0, c != 0); return 0;
    case Op::ButtonSetText: aButton->SetText(CtlString(b), true); return 0;
    case Op::LabelSetText: aLabel->SetText(CtlString(b), true); return 0;
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameIsHost: return aGame->GetIsHost() ? 1 : 0;
    case Op::GameCanStart: return aGame->CanStartGame() ? 1 : 0;
    case Op::GameCanReady: return aGame->CanReady() ? 1 : 0;
    case Op::GameMatchesSpec: return aGame->MatchesRoomSpec((int)b) ? 1 : 0;
    case Op::GameCaptainId: return (intptr_t)aGame->GetCaptainId();
    case Op::GameClients: return (intptr_t)aGame->GetClients().get();
    case Op::IAmReady: return LobbyIAmReady() ? 1 : 0;
    case Op::ClientsCount: return (intptr_t)reinterpret_cast<LobbyClientList*>(a)->GetCount();
    case Op::ClientsAt: return (intptr_t)reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b);
    case Op::ClientId: return (intptr_t)aClient->GetClientId();
    case Op::ClientHasPlayer: return aClient->HasPlayer() ? 1 : 0;
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
    case Op::ItemNew: {
        GameUserListItem* aNew = new GameUserListItem(aClient);
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::ItemClient: return (intptr_t)anItem->mClient.get();
    case Op::ItemClientIs: return ClientIs(anItem->mClient.get(), b) ? 1 : 0;
    case Op::ItemSetImage: anItem->mImage = CtlImage(b); return 0;
    case Op::ButtonOf: return (intptr_t)dynamic_cast<Button*>(aComp);
    case Op::ControlId: return aComp->mControlId;
    case Op::PrefGet: return PrefGet(a);
    case Op::PrefSet: PrefSet(a, b); return 0;
    case Op::ComboGetSel: return aCombo->GetSelItem();
    case Op::ComboSetSel: aCombo->SetSelItem((int)b, c != 0); return 0;
    case Op::ComboClear: aCombo->Clear(b != 0); return 0;
    case Op::ComboShowTabSelect: aCombo->SetTextAndSel(LobbyGetTabSelect(), false); return 0;
    case Op::SaveTabSelect: LobbySetTabSelect(aCombo->mEdit->GetText()); return 0;
    case Op::SaveLanName: LobbySetLanName(aCombo->mEdit->GetText()); return 0;
    case Op::LanNameCount: return (intptr_t)LobbyGetLanNameCount();
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
    case Op::RestoreDefaults: LobbyRestoreDefaults(); return 0;
    case Op::ResetNetworkAdapter: LobbySetNetworkAdapter(LobbyGetNetworkAdapter()); return 0;
    case Op::ResetFilters: LobbyResetFilters(); return 0;
    case Op::CurrentScreen: return (intptr_t)LobbyGetCurrentScreen();
    case Op::BroadcastDown: aComp->BroadcastEventDown(new LobbyEvent((int)b)); return 0;
    case Op::MusicResume: ::LobbyGlobal_LobbyMusic_Sound->Resume(); return 0;
    case Op::MusicStop: ::LobbyGlobal_LobbyMusic_Sound->Stop(); return 0;
    case Op::ClickSoundMute: ::LobbyGlobal_ButtonClick_Sound->SetMuted(a != 0); return 0;
    case Op::LogonSoundStop: ::LobbyGlobal_SierraLogon_Sound->Stop(); return 0;
    case Op::RemoveLanName: LobbyRemoveLanName(); return 0;
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
    case Op::GameOption: return (intptr_t)static_cast<EELobbyGame*>(aGame)->GetOption((int)b);
    case Op::GameSetOption: static_cast<EELobbyGame*>(aGame)->SetOption((int)b, (unsigned long)c); return 0;
    case Op::GameChangeOption:
        static_cast<EELobbyGame*>(aGame)->ChangeOption((int)b, (unsigned long)(long)c, true);
        return 0;
    case Op::GameSendState: static_cast<EELobbyGame*>(aGame)->SendGameState(); return 0;
    case Op::GameHasFile: return static_cast<EELobbyGame*>(aGame)->HasGameFile() ? 1 : 0;
    case Op::LabelSetGameFile:
        aLabel->SetText(static_cast<EELobbyGame*>(reinterpret_cast<LobbyGame*>(b))->GetGameFileName());
        return 0;
    case Op::GameSlot: return (intptr_t)static_cast<EELobbyGame*>(aGame)->GetPlayerSlot((int)b);
    case Op::SlotState: return reinterpret_cast<EELobbyPlayerSlot*>(a)->mOptions[0];
    case Op::SlotSetOpen: reinterpret_cast<EELobbyPlayerSlot*>(a)->SetOpen(); return 0;
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
    }
    return 0;
}

// ---- the config readers (GameBrowserCtrl.cpp's and GameUserListCtrl.cpp's)

// FUNCTION: WON 0x00765460
// Creates a config for a browser tab in the layout file
ComponentConfig* GameBrowserCtrlConfig::CfgFactory()
{
    return new GameBrowserCtrlConfig;
}

// The instruction keys and the flag and value names these readers compare are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
// FUNCTION: WON 0x007bc450
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

// FUNCTION: WON 0x007654c0
// Creates the layout reader for a game player list
ComponentConfig* GameUserListCtrlConfig::CfgFactory()
{
    return new GameUserListCtrlConfig;
}

// FUNCTION: WON 0x007bda70
// Accepts (and ignores) the CTRLLINKID layout key; passes other keys to the container's reader
bool GameUserListCtrlConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) { }
    else return ContainerConfig::Virtual24(instruction);
    return true;
}
}
