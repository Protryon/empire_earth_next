// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby's small dialogs' shell and host (R6 WONLobby round 1): LanNameDialog, PasswordDialog, AddFriendDialog,
// DirectConnectDialog, LanPortDialog, CreateRoomDialog, CreateGameDialog and AdminActionDialog are one Rust object
// each (rust/crates/ee_won_lobby/src/lobbydlg.rs), as are StatusDialog and OkCancelDialog (round 2), RoomDialog,
// TOUDialog, BadUserDialog, ChooseNetworkAdapterDialog and CreditsDialog (round 3) and the EE game room's connect
// (round 5) and saved game (round 6) dialogs; the options popups
// (ClientOptionsPopup, GameOptionsPopup, FriendOptionsPopup) are lobbypopup.rs objects with their host below. This unit is the C++ shell's two virtuals and the host: each op is the
// original's statements on a control, the dialog's base, a lobby game or a resource string, with no decision.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "platform/PlatFile.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_lobby/dialogs.h"

using namespace WONAPI;


// One player's line of the EE connect dialog (EELobbyConnectDialog.cpp's ConnectListItem): a Rust text row since
// R6 WON round 34 (ee_won_lobby rows.rs Connect, made by dlgsplit.rs)

// ---- the list rows (R6 WONLobby round 3: moved here from BadUserDialog.cpp and ChooseNetworkAdapterDialog.cpp, whose
// dialogs are lobby dialog kinds now; the rows stay C++ widgets)

// One row of the bad user list: a Rust row since R6 WON round 34 (ee_won_lobby rows.rs BadUser; its texts made
// by dlgsplit.rs over TextRecord, TextFormatRes and the rows' TextNew)

// a bad user record's text (TextRecord)
static std::wstring RecordText(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    WonWideSink(&aText, inUnits, inCount);
    return aText;
}

// Adapter list row holding the adapter's host address (RTTI in the global namespace, not WONAPI)
class NetworkAdapterListItem : public MultiListItem {
public:
    NetworkAdapterListItem(long inHost) : mHost(inHost) {}
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    long mHost; // 0x28; 0x2c bytes in all (the inherited column vector is at 0x18)
};
// Sorts the default adapter row (host 0) first, the others by their column text
int NetworkAdapterListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int anOrder=ee_won_dlg_adapter_order((int32_t)this->mHost,(int32_t)static_cast<const NetworkAdapterListItem*>(inOther)->mHost);
    if(anOrder!=2) return anOrder;
    return MultiListItem::Compare(inOther,inList);
}

namespace WONAPI {

static intptr_t LobbyDialogHost(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c);

LobbyRustDialog::LobbyRustDialog(EeWonLobbyDlgKind inKind)
    : mLobbyDlg(ee_won_lobbydlg_new_model(this, inKind, LobbyDialogHost)), mKind(inKind)
{
    // the subclasses' constructors (the status dialog's start tick, the credits' scrolling text) are the Rust's
}


// the controls are released in reverse member order (the original SmartPtr members), before the Dialog base
LobbyRustDialog::~LobbyRustDialog() { ee_won_lobbydlg_free(this->mLobbyDlg); }

// the originals looked their controls up for a config init only and did not call the base
void LobbyRustDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    this->LCall(EeWonLobbyDlgCall::Init, (intptr_t) static_cast<ComponentConfigInit&>(inInit).mConfig);
}

void LobbyRustDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->LCall(EeWonLobbyDlgCall::RawEvent, (intptr_t)inEvent, inEvent->mEv.kind, inEvent->mEv.comp);
}

const GUIString& LobbyRustDialog::InputText(int inMember)
{
    return reinterpret_cast<InputBox*>(this->LCall(EeWonLobbyDlgCall::Slot, inMember))->GetText();
}

const GUIString& LobbyRustDialog::InputTextOf(intptr_t inInput)
{
    if (inInput == 0)
        return gEmptyGUIString;
    return reinterpret_cast<InputBox*>(inInput)->GetText();
}

SmartPtr<LobbyGame> LobbyRustDialog::GetGame()
{
    LobbyGame* aHeld = reinterpret_cast<LobbyGame*>(this->LCall(EeWonLobbyDlgCall::GetGame));
    SmartPtr<LobbyGame> aGame(aHeld);
    aHeld->Release(); // the op's reference: the SmartPtr holds the game now
    return aGame;
}

template <class T> static Component* FindHeld(ComponentConfig* inConfig, const EeWonLobbyDlgSlot* inSlot)
{
    SmartPtr<T> aPtr;
    SafeGetComponent(inConfig, aPtr, inSlot->name, inSlot->file, inSlot->line);
    Component* aComponent = aPtr.get();
    if (aComponent != NULL)
        aComponent->AddRef();
    return aComponent;
}

// the EE game's strings of the connect dialog (the lobby's are resource rows: lobbydlg.rs string_row)
static const GUIString& EeString(intptr_t inString)
{
    if ((EeWonLobbyDlgString)inString == EeWonLobbyDlgString::AbortLaunch)
        return EE_LOBBY_STRING(gEELobbyConnectDialog_AbortLaunch);
    if ((EeWonLobbyDlgString)inString == EeWonLobbyDlgString::LeaveGame)
        return EE_LOBBY_STRING(gEELobbyConnectDialog_LeaveGame);
    return gEmptyGUIString;
}

// an object with one reference the Rust gives back (NULL stays NULL)
static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}
static std::string sDlgItemText; // ItemText's text: the Rust copies it before its next op

static intptr_t LobbyDialogHost(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyDlgOp Op;
    LobbyRustDialog* aDialog = static_cast<LobbyRustDialog*>(inDialog);
    // the dialog itself (the Rust passes the pointer it was made with) or a held control
    Component* aComp = a == (intptr_t)inDialog ? static_cast<Component*>(aDialog) : reinterpret_cast<Component*>(a);
    InputBox* anInput = reinterpret_cast<InputBox*>(a);
    Button* aButton = reinterpret_cast<Button*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    ListArea* aList = reinterpret_cast<ListArea*>(a);
    MultiListArea* aMulti = reinterpret_cast<MultiListArea*>(a);
    switch (inOp) {
    case Op::Find: {
        ComponentConfig* aConfig = reinterpret_cast<ComponentConfig*>(a);
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        switch (aSlot->ty) {
        case EeWonLobbyDlgType::Input: return (intptr_t)FindHeld<InputBox>(aConfig, aSlot);
        case EeWonLobbyDlgType::Button: return (intptr_t)FindHeld<Button>(aConfig, aSlot);
        case EeWonLobbyDlgType::Label: return (intptr_t)FindHeld<Label>(aConfig, aSlot);
        case EeWonLobbyDlgType::Combo: return (intptr_t)FindHeld<ComboBox>(aConfig, aSlot);
        case EeWonLobbyDlgType::Component: return (intptr_t)FindHeld<Component>(aConfig, aSlot);
        case EeWonLobbyDlgType::Frame: { // a WONFrameComponent kind
            SmartPtr<Component> aPtr;
            SafeGetShell(aConfig, aPtr, EeWonShellKind::WonFrameComponent, aSlot->name, aSlot->file, aSlot->line);
            if (aPtr.get() != NULL) aPtr->AddRef();
            return (intptr_t)aPtr.get();
        }
        case EeWonLobbyDlgType::Browser: return (intptr_t)FindHeld<BrowserComponent>(aConfig, aSlot);
        case EeWonLobbyDlgType::MultiList: return (intptr_t)FindHeld<MultiListArea>(aConfig, aSlot);
        case EeWonLobbyDlgType::List: return (intptr_t)FindHeld<ListArea>(aConfig, aSlot);
        }
        return 0;
    }
    // a held control (the end of the original's SmartPtr)
    case Op::Release: aComp->Release(); return 0;
    case Op::Relayout: aDialog->Relayout(); return 0;
    case Op::SetEnabled: aComp->SetEnabled(b != 0); return 0;
    case Op::SetVisible: aComp->SetVisible(b != 0); return 0;
    case Op::InputSetNumeric: anInput->mInputFilter = InputBox::NumericFilter; return 0;
    case Op::LabelSetNumber: aLabel->SetText(GUIString(std::to_string((int)b).c_str()), true); return 0;
    // the lobby's strings come as resource rows (lobbydlg.rs string_row), the EE game's two as strings
    case Op::LabelSetRes: aLabel->SetText(gLobbyResources[b].mString, true); return 0;
    case Op::ButtonSetRes: aButton->SetText(gLobbyResources[b].mString, true); return 0;
    case Op::LabelSetString: aLabel->SetText(EeString(b), true); return 0;
    case Op::ButtonSetString: aButton->SetText(EeString(b), true); return 0;
    case Op::GameCreate: return Held(LobbyGame::CreateGame(static_cast<LobbyGameType>(b)));
    case Op::GameSetName: aGame->SetName(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetPassword: aGame->SetPassword(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetAskToJoin: aGame->SetAskToJoin(b != 0); return 0;
    case Op::GameSetInviteOnly: aGame->SetInviteOnly(b != 0); return 0;
    case Op::GameSetSkill: aGame->SetSkill((long)b); return 0;
    // round 2: the status and OK/Cancel boxes' sizing (the Component base's reads and calls: lobbydlg.rs comp_op)
    case Op::SetVertPad: aLabel->SetVertPad((int)b, (int)c); return 0;
    case Op::LabelSetText: aLabel->SetText(b != 0 ? *reinterpret_cast<const GUIString*>(b) : gEmptyGUIString, true);
        return 0;
    case Op::ButtonSetText: aButton->SetText(*reinterpret_cast<const GUIString*>(b), true); return 0;
    case Op::FrameLeft: return WONShell::Op(reinterpret_cast<Component*>(a), EeWonShellOp::GetFrameBorder, 0);
    case Op::FrameRight: return WONShell::Op(reinterpret_cast<Component*>(a), EeWonShellOp::GetFrameBorder, 2);
    case Op::RootWidth: return GetLobbyRoot()->Width();
    case Op::RootHeight: return GetLobbyRoot()->Height();
    case Op::FontStringWidth: return aLabel->mWrappedText->mFont->GetStringWidth(*reinterpret_cast<const GUIString*>(b));
    case Op::FontStringWidthRange: return aLabel->mWrappedText->mFont->GetStringWidth(*reinterpret_cast<const GUIString*>(b), 0, reinterpret_cast<const GUIString*>(b)->length());
    case Op::FontCharWidth: return aLabel->mWrappedText->mFont->GetCharacterWidth((int)b);
    case Op::FontHeight: return aLabel->mWrappedText->mFont->GetHeight();
    case Op::TickCount: return (intptr_t)(uint32_t)WONTickCount();
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::TitleText: return (intptr_t)&aLabel->mWrappedText->mText;
    // round 3: TOUDialog, BadUserDialog, ChooseNetworkAdapterDialog
    // as text (WON's HTML rendering is not ported)
    case Op::BrowserShowTou: reinterpret_cast<BrowserComponent*>(a)->ShowFiles(LobbyGetSysTOUPath(), LobbyGetGameTOUPath(), false); return 0;
    case Op::BrowserTop: reinterpret_cast<BrowserComponent*>(a)->GetHTMLView()->SetOffsets(0, 0); return 0;
    case Op::HttpDocType: return ((RustServerOp*)LobbyEvtGet(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Op))->GetNum(EE_WON_HTTPDOC_TYPE);
    case Op::IdsClear: reinterpret_cast<std::list<unsigned long>*>(a)->clear(); return 0;
    case Op::IdsPush: reinterpret_cast<std::list<unsigned long>*>(a)->push_back((unsigned long)b); return 0;
    case Op::LocalHost: return (intptr_t)WONAPI::IPAddr::GetLocalHost();
    case Op::AdaptersGet: {
        std::list<LobbyNetAddress> anAddresses;
        LobbyGetNetAddresses(anAddresses, true);
        return (intptr_t)new std::vector<LobbyNetAddress>(anAddresses.begin(), anAddresses.end());
    }
    case Op::AdaptersCount: return (intptr_t)reinterpret_cast<std::vector<LobbyNetAddress>*>(a)->size();
    case Op::AdapterHandle: return (intptr_t)(*reinterpret_cast<std::vector<LobbyNetAddress>*>(a))[b].mHandle;
    case Op::AdapterNameLength: return (intptr_t)GUIString((*reinterpret_cast<std::vector<LobbyNetAddress>*>(a))[b].mName).length();
    case Op::AdapterInsertRow: aMulti->InsertItem(new NetworkAdapterListItem((long)b), -1); return 0;
    case Op::AdapterSetName: aMulti->SetString(aMulti->GetNumItems() - 1, 0, GUIString((*reinterpret_cast<std::vector<LobbyNetAddress>*>(b))[c].mName), NULL); return 0;
    case Op::AdapterSetUnknownName: aMulti->SetString(aMulti->GetNumItems() - 1, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Unknown_String), NULL); return 0;
    case Op::AdapterSetAddress: aMulti->SetString(aMulti->GetNumItems() - 1, 1, IPAddr((long)b, 0).GetHostString(true), NULL); return 0;
    case Op::AdaptersFree: delete reinterpret_cast<std::vector<LobbyNetAddress>*>(a); return 0;
    case Op::AdapterHost: return (intptr_t)reinterpret_cast<NetworkAdapterListItem*>(a)->mHost;
    // round 3: CreditsDialog
    case Op::AddRef: aComp->AddRef(); return 0;
    case Op::CreditsPlace: aDialog->Fn2F6140(reinterpret_cast<Component*>(a), 0x5003, reinterpret_cast<Component*>(b), reinterpret_cast<Component*>(b)->mParent->Left(), reinterpret_cast<Component*>(b)->mParent->Top(), 0, 0, 0); return 0;
    case Op::ScrollStart: reinterpret_cast<ScrollingTextComponent*>(a)->Start(); return 0;
    case Op::ScrollStop: reinterpret_cast<ScrollingTextComponent*>(a)->Stop(); return 0;
    case Op::ScrollReset: reinterpret_cast<ScrollingTextComponent*>(a)->Reset(); return 0;
    case Op::ScrollAddLine: reinterpret_cast<ScrollingTextComponent*>(a)->AddLine(*reinterpret_cast<const GUIString*>(b), (unsigned long)(uint32_t)c); return 0;
    case Op::DefaultFontStringWidth: return aDialog->GetDefaultFont()->GetStringWidth(*reinterpret_cast<const GUIString*>(a), 0, reinterpret_cast<const GUIString*>(a)->length());
    // EELobbyConnectDialog
    case Op::GameIsHost: return reinterpret_cast<LobbyGame*>(a)->GetIsHost() ? 1 : 0;
    case Op::GameRust: return (intptr_t) reinterpret_cast<LobbyGame*>(a)->mEE;
    case Op::DoDialog: return LobbyDoDialog(aDialog);
    case Op::EndDialog: reinterpret_cast<Window*>(a)->EndDialog((int)b, aDialog); return 0;
    case Op::ConnectItemSetImage: static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a))->mImage = (b == 0 ? LOBBY_IMAGE(GameUserListCtrl_Ready_Image) : LOBBY_IMAGE(ServerListCtrl_PingFailed_Image)).mPtr; return 0;
    case Op::ConnectItemNotReady: return static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a))->mImage.mPtr != LOBBY_IMAGE(GameUserListCtrl_Ready_Image).mPtr ? 1 : 0;
    // EELobbyFileDialog
    case Op::ItemText: return (intptr_t)(sDlgItemText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a))->mText.operator std::string()).c_str();
    case Op::LoadSavedGame: return EELobbyLoadSavedGame(reinterpret_cast<const char*>(a)) ? 1 : 0;
    case Op::FileAskDelete: return LobbyMessageBoxYesNo(EE_LOBBY_STRING(gEELobbyFileDialog_DeleteTitle), EE_LOBBY_STRING(gEELobbyFileDialog_DeleteDetails), true);
    case Op::FileDelete: WONFile(reinterpret_cast<const char*>(a)).Delete(true); return 0;
    case Op::EventDoubleClick: return reinterpret_cast<ComponentEvent*>(a)->mEv.flag;
    case Op::EventButton: return reinterpret_cast<ComponentEvent*>(a)->mEv.button;
    // R6 WON round 34: the statements of the compound cases dlgsplit.rs composes (the bad user rows, the admin title,
    // the default adapter row, the file search) and of the constructors
    case Op::TextCompare: return ee_won_gstr_read(EeWonGstrRead::Compare, reinterpret_cast<const EeWonGstr*>(a), reinterpret_cast<const void*>(b), 0, 0);
    case Op::TextRecord: return Held(NewGUIStringData(RecordText(reinterpret_cast<const uint16_t*>(a), (size_t)b)));
    case Op::TextFormatRes: return Held(LobbyFormatString(gLobbyResources[a].mString, GUIString(reinterpret_cast<GUIStringData*>(b))).mData.get());
    case Op::TitleNew: return (intptr_t) new GUIString(gLobbyResources[a].mString);
    case Op::TitleAppendSpace: reinterpret_cast<GUIString*>(a)->append(GUIString(" ")); return 0;
    case Op::TitleAppendName: reinterpret_cast<GUIString*>(a)->append(reinterpret_cast<LobbyClient*>(b)->GetName()); return 0;
    case Op::TitleFree: delete reinterpret_cast<GUIString*>(a); return 0;
    case Op::AdapterSetDefaultName: aMulti->SetString(0, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Default_String), NULL); return 0;
    case Op::NameLength: return (intptr_t)GUIString(reinterpret_cast<const char*>(a)).length();
    case Op::FileListInsertName: aList->InsertString(GUIString(reinterpret_cast<const char*>(b)).substr(0, (unsigned short)c), -1); return 0;
    case Op::NewScrollText: return (intptr_t) new ScrollingTextComponent;
    }
    return 0;
}


// ---- the options popups (R6 WONLobby round 3): the shell and its host (each op the original's statements on the
// menu, the popup, its subject, a font, the lobby's config or an event)

static intptr_t LobbyPopupHost(void* inPopup, EeWonLobbyPopupOp inOp, intptr_t a, intptr_t b, intptr_t c);

LobbyRustPopup::LobbyRustPopup(EeWonLobbyPopupKind inKind)
    : mLobbyPopup(ee_won_lobbypopup_new_model(this, inKind, LobbyPopupHost))
{
    this->PCall(EeWonLobbyPopupCall::Construct);
}

void LobbyOpenRustPopup(EeWonLobbyPopupKind k, intptr_t a, intptr_t b, intptr_t c)
{
    SmartPtr<LobbyRustPopup> aPopup = new LobbyRustPopup(k);
    aPopup->PCall(EeWonLobbyPopupCall::Init, a, b, c);
    LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
}

// the SmartPtr members are released in reverse member order, before the Container base
LobbyRustPopup::~LobbyRustPopup() { ee_won_lobbypopup_free(this->mLobbyPopup); }

void LobbyRustPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->PCall(EeWonLobbyPopupCall::RawEvent, (intptr_t)inEvent, inEvent->mEv.kind);
}

static intptr_t LobbyPopupHost(void* inPopup, EeWonLobbyPopupOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyPopupOp Op;
    LobbyRustPopup* aPopup = static_cast<LobbyRustPopup*>(inPopup);
    PopupMenu* aMenu = reinterpret_cast<PopupMenu*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
    switch (inOp) {
    case Op::MenuNew: return (intptr_t) new PopupMenu;
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::MenuFont: return Held(aMenu->mFont.get());
    case Op::PopupFontMod: return Held(aPopup->GetFontMod(reinterpret_cast<Font*>(a), 1, 0));
    case Op::MenuFontMod: return Held(aMenu->GetFontMod(reinterpret_cast<Font*>(b), 1, 0));
    case Op::MenuSetFont: aMenu->SetFont(reinterpret_cast<Font*>(b)); return 0;
    case Op::MenuAssignFont: aMenu->mFont = reinterpret_cast<Font*>(b); return 0;
    case Op::MenuTitleClient: aMenu->AddItem(reinterpret_cast<LobbyClient*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleGame: aMenu->AddItem(reinterpret_cast<LobbyGame*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleFriend: aMenu->AddItem(reinterpret_cast<LobbyFriend*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleNumber: aMenu->AddItem(GUIString(std::to_string((int)b).c_str()), 0)->mSelectable = false; return 0;
    case Op::MenuAddRes: aMenu->AddItem(gLobbyResources[b].mString, NULL, NULL, (int)c); return 0;
    case Op::MenuAddSeparator: aMenu->AddSeperator(); return 0;
    case Op::Config: return (intptr_t)GetLobbyConfig();
    case Op::ConfigNoPingInProgress: return reinterpret_cast<LobbyConfig*>(a)->GetNoPingInProgress() ? 1 : 0;
    case Op::EventItem: return anEvent->mEv.item;
    case Op::EventButton: return anEvent->mEv.button;
    case Op::ItemMenuId: return reinterpret_cast<MenuItem*>(a)->mMenuId;
    case Op::DeliverClient: aPopup->DeliverEvent(ClientActionEvent((int)a, (int)c, reinterpret_cast<LobbyClient*>(b)), false); return 0;
    case Op::DeliverGame: aPopup->DeliverEvent(GameActionEvent((int)a, reinterpret_cast<LobbyGame*>(b)), false); return 0;
    case Op::DeliverFriend: aPopup->DeliverEvent(FriendActionEvent((int)a, reinterpret_cast<LobbyFriend*>(b), (int)c), false); return 0;
    case Op::ClosePopup: aPopup->GetRoot()->ClosePopup(aPopup); return 0;
    }
    return 0;
}

}
