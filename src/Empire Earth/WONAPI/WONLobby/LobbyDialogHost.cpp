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
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/won_lobby/dialogs.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbypopup.h"
#include "rust/won_net/httpdoc.h"

using namespace WONAPI;


// the EE file dialog's search (EELobbyFileDialog::ListFiles' locals)
struct EELobbyFind {
    PlatFind* mFind;
    PlatFindData mData;
};

namespace {
// One player's line of the EE connect dialog (EELobbyConnectDialog.cpp's anonymous namespace; 0x2c bytes; vtable
// 0x84c2c0: TextListItem's slots)
class ConnectListItem : public TextListItem {
public:
    ConnectListItem(const GUIString& inName, Image* inImage, unsigned char inSlot) : TextListItem(inName, inImage)
    {
        this->mSlot = inSlot;
    }

    unsigned char mSlot;                    // +0x28
};
}

// ---- the list rows (R6 WONLobby round 3: moved here from BadUserDialog.cpp and ChooseNetworkAdapterDialog.cpp, whose
// dialogs are lobby dialog kinds now; the rows stay C++ widgets)

// One row of the bad user list (global namespace, 0x50 bytes)
class BadUserItem : public ListItem {
public:
    BadUserItem(const EeWonRouteRecord& inUser,bool inSimple);
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<Font> mFont; // 0x18
    GUIString mStrings[6]; // 0x1c..0x30: name, WON id, expiry, moderator, moderator's WON id, comment
    unsigned long mValues[6]; // 0x34..0x48: sort keys; only the WON id (1), expiry (2) and moderator id (4) are set
    bool mIsSimple; // 0x4c (padded to 0x50)
};
static std::wstring RecordText(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    WonWideSink(&aText, inUnits, inCount);
    return aText;
}
// Creates a row from a bad user record (the routing op's: ee_won routeop.rs), writing the expiry as days, hours,
// minutes, seconds or infinite
BadUserItem::BadUserItem(const EeWonRouteRecord& inUser, bool inSimple)
{
    this->mIsSimple = inSimple;
    this->mStrings[0] = GUIString(new GUIStringData(RecordText(inUser.name, inUser.name_len)));
    char aBuffer[100];
    sprintf(aBuffer, "%d", inUser.user_id);
    this->mStrings[1] = aBuffer;
    this->mValues[1] = inUser.user_id;

    unsigned long aDuration = inUser.expiration;
    GUIString aText;
    GUIString aUnit;
    if (aDuration > 86400) {
        aDuration /= 86400;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XDays_String) : LOBBY_STRING(BadUserDialog_OneDay_String);
    } else if (aDuration > 3600) {
        aDuration /= 3600;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XHours_String) : LOBBY_STRING(BadUserDialog_OneHour_String);
    } else if (aDuration > 60) {
        aDuration /= 60;
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XMinutes_String) : LOBBY_STRING(BadUserDialog_OneMinute_String);
    } else if (aDuration > 0) {
        aUnit = aDuration > 1 ? LOBBY_STRING(BadUserDialog_XSeconds_String) : LOBBY_STRING(BadUserDialog_OneSecond_String);
    } else {
        aUnit = LOBBY_STRING(BadUserDialog_Infinite_String);
    }
    sprintf(aBuffer, "%d", aDuration);
    this->mValues[2] = inUser.expiration;
    this->mStrings[2] = LobbyFormatString(aUnit, GUIString(aBuffer));
    this->mStrings[3] = GUIString(RecordText(inUser.moderator_name, inUser.moderator_name_len));
    sprintf(aBuffer, "%d", inUser.moderator_id);
    this->mStrings[4] = GUIString(aBuffer);
    this->mValues[4] = inUser.moderator_id;
    this->mStrings[5] = GUIString(RecordText(inUser.comment, inUser.comment_len));
}

// Takes the list's font and makes the row one text line high
void BadUserItem::CalcDimensions(ListArea* inList)
{
    this->mFont=inList->mFont;
    this->mHeight=this->mFont->GetHeight();
}
// Draws the selection background and the text of the column being painted (the name in the simple list)
void BadUserItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    DWORD aColor=inList->GetTextColor();
    if(this->mSelected) aColor=inList->GetSelColor().GetForegroundColor(inGraphics,aColor);
    int aColumn=0;
    bool aSelected=this->mSelected;
    bool aSimple=this->mIsSimple;
    if(!aSimple) aColumn=static_cast<MultiListArea*>(inList)->GetColumnPaintContext();
    if((aSimple || aColumn==-1) && aSelected) {
        inGraphics.SetColor(inList->GetSelColor().GetBackgroundColor(inGraphics));
        int aWidth=inList->GetPaintWidth();
        int aHeight=this->mFont->GetHeight();
        inGraphics.FillRect(0,0,aWidth,aHeight);
        if(!this->mIsSimple) return;
    }
    if(aColumn<0 || aColumn>5)
        return;
    inGraphics.SetColor(aColor);
    inGraphics.SetFont(this->mFont.get());
    inGraphics.DrawString(this->mStrings[aColumn],0,0);
}
// Orders two rows by the sort column: numerically for the ids and expiry, else by text
int BadUserItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    int aColumn=static_cast<MultiListArea*>(inList)->GetSortColumn();
    if(aColumn<0 || aColumn>5) return this<inOther;
    const BadUserItem* anOther=static_cast<const BadUserItem*>(inOther);
    switch(aColumn) {
    case 1:
    case 2:
    case 4:
        return this->mValues[aColumn]-anOther->mValues[aColumn];
    default:
        return this->mStrings[aColumn].mData->Compare(anOther->mStrings[aColumn].mData.get());
    }
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
    : mLobbyDlg(ee_won_lobbydlg_new_model(this, inKind, LobbyDialogHost)) {}

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
    EeWonLobbyDlgEvent aType = EeWonLobbyDlgEvent::Other;
    if (inEvent->mType == ComponentEvent_InputTextChanged)
        aType = EeWonLobbyDlgEvent::InputTextChanged;
    else if (inEvent->mType == ComponentEvent_ButtonPressed)
        aType = EeWonLobbyDlgEvent::ButtonPressed;
    else if (inEvent->mType == ComponentEvent_ComboSelChanged)
        aType = EeWonLobbyDlgEvent::ComboSelChanged;
    else if (inEvent->mType == LobbyEvent_GotHTTPDocument)
        aType = EeWonLobbyDlgEvent::GotHttpDocument;
    else if (inEvent->mType == ComponentEvent_ListItemClicked)
        aType = EeWonLobbyDlgEvent::ListItemClicked;
    this->LCall(EeWonLobbyDlgCall::Event, (intptr_t)inEvent, (intptr_t)aType, (intptr_t)inEvent->mComponent);
}

StatusDialog::StatusDialog() : LobbyRustDialog(EeWonLobbyDlgKind::Status) { this->SetStartTick(WONTickCount()); }

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

SmartPtr<LobbyGame> CreateGameDialog::GetGame()
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

// a bad user record into the list (BadUsersInsert)
static void InsertBadUser(void* inTarget, const EeWonRouteRecord* inRecord)
{
    std::pair<ListArea*, bool>& aTarget = *static_cast<std::pair<ListArea*, bool>*>(inTarget);
    aTarget.first->InsertItem(new BadUserItem(*inRecord, aTarget.second), -1);
}
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
    char aBuf[10];
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
        case EeWonLobbyDlgType::Frame: return (intptr_t)FindHeld<WONFrameComponent>(aConfig, aSlot);
        case EeWonLobbyDlgType::Browser: return (intptr_t)FindHeld<BrowserComponent>(aConfig, aSlot);
        case EeWonLobbyDlgType::MultiList: return (intptr_t)FindHeld<MultiListArea>(aConfig, aSlot);
        case EeWonLobbyDlgType::List: return (intptr_t)FindHeld<ListArea>(aConfig, aSlot);
        }
        return 0;
    }
    // a held control (the end of the original's SmartPtr)
    case Op::Release: aComp->Release(); return 0;
    case Op::BaseEvent: aDialog->Dialog::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(a)); return 0;
    case Op::Relayout: aDialog->Relayout(); return 0;
    case Op::RequestFocus: aComp->RequestFocus(); return 0;
    case Op::SetEnabled: aComp->SetEnabled(b != 0); return 0;
    case Op::SetVisible: aComp->SetVisible(b != 0); return 0;
    case Op::IsVisible: return aComp->IsVisible();
    case Op::InputClear: anInput->Clear(b != 0); return 0;
    case Op::InputTextLength: return (intptr_t)anInput->GetText().length();
    case Op::InputShownLength: return (intptr_t)anInput->mText.length();
    case Op::InputToInt: return anInput->GetText().mData->ToInt(0);
    case Op::InputSetNumeric: anInput->mInputFilter = InputBox::NumericFilter; return 0;
    case Op::InputSetNumber:
        sprintf(aBuf, "%d", (int)b);
        anInput->SetText(GUIString(new GUIStringData(aBuf)), c != 0);
        return 0;
    case Op::InputSetEmpty: anInput->SetText(GUIString(""), b != 0); return 0;
    case Op::InputSelectRange: anInput->SelectRange((int)b, (int)c); return 0;
    case Op::LabelSetNumber:
        sprintf(aBuf, "%d", (int)b);
        aLabel->SetText(GUIString(new GUIStringData(aBuf)), true);
        return 0;
    // the lobby's strings come as resource rows (lobbydlg.rs string_row), the EE game's two as strings
    case Op::LabelSetRes: aLabel->SetText(gLobbyResources[b].mString, true); return 0;
    case Op::ButtonSetRes: aButton->SetText(gLobbyResources[b].mString, true); return 0;
    case Op::AdminTitleRes: {
        GUIString aTitle = gLobbyResources[b].mString;
        aTitle.append(GUIString(new GUIStringData(" ")));
        aTitle.append(reinterpret_cast<LobbyClient*>(c)->GetName());
        aLabel->SetText(aTitle, true);
        return 0;
    }
    case Op::LabelSetString: aLabel->SetText(EeString(b), true); return 0;
    case Op::ButtonSetString: aButton->SetText(EeString(b), true); return 0;
    case Op::ButtonIsDisabled: return aButton->IsDisabled();
    case Op::ButtonIsChecked: return aButton->IsChecked();
    case Op::ButtonSetCheck: aButton->SetCheck(b != 0, c != 0); return 0;
    case Op::ComboGetSel: return aCombo->GetSelItem();
    case Op::ComboSetSel: aCombo->SetSelItem((int)b, c != 0); return 0;
    case Op::GameCreate: {
        LobbyGame* aNew = LobbyGame::CreateGame(static_cast<LobbyGameType>(b));
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::GameSetName: aGame->SetName(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetPassword: aGame->SetPassword(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetAskToJoin: aGame->SetAskToJoin(b != 0); return 0;
    case Op::GameSetInviteOnly: aGame->SetInviteOnly(b != 0); return 0;
    case Op::GameSetSkill: aGame->SetSkill((long)b); return 0;
    // round 2: the status and OK/Cancel boxes' sizing
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::Left: return aComp->Left();
    case Op::Top: return aComp->Top();
    case Op::SetSize: aComp->SetSize((int)b, (int)c); return 0;
    case Op::SetSizeAtLeast: aComp->SetSizeAtLeast((int)b, (int)c); return 0;
    case Op::SetDesiredSize: aComp->SetDesiredSize((int)b, (int)c); return 0;
    case Op::SetDesiredWidth: aComp->SetDesiredWidth((int)b); return 0;
    case Op::SetPos: aComp->SetPos((int)b, (int)c); return 0;
    case Op::SetVertPad: aLabel->SetVertPad((int)b, (int)c); return 0;
    case Op::LabelSetText:
        aLabel->SetText(b != 0 ? *reinterpret_cast<const GUIString*>(b) : gEmptyGUIString, true);
        return 0;
    case Op::ButtonSetText: aButton->SetText(*reinterpret_cast<const GUIString*>(b), true); return 0;
    case Op::FrameLeft: return reinterpret_cast<WONFrameComponent*>(a)->GetFrameLeft();
    case Op::FrameRight: return reinterpret_cast<WONFrameComponent*>(a)->GetFrameRight();
    case Op::RootWidth: return GetLobbyRoot()->Width();
    case Op::RootHeight: return GetLobbyRoot()->Height();
    case Op::FontStringWidth:
        return aLabel->mWrappedText->mFont->GetStringWidth(*reinterpret_cast<const GUIString*>(b));
    case Op::FontStringWidthRange: {
        const GUIString& aText = *reinterpret_cast<const GUIString*>(b);
        Font* aFont = aLabel->mWrappedText->mFont;
        return aFont->GetStringWidth(aText, 0, aText.length());
    }
    case Op::FontCharWidth: return aLabel->mWrappedText->mFont->GetCharacterWidth((int)b);
    case Op::FontHeight: return aLabel->mWrappedText->mFont->GetHeight();
    case Op::TickCount: return (intptr_t)(uint32_t)WONTickCount();
    case Op::Invalidate: aComp->Invalidate(); return 0;
    case Op::IsDragging: return aDialog->IsDragging() ? 1 : 0;
    case Op::SetDragging: aDialog->SetDragging(b != 0); return 0;
    case Op::TitleText: return (intptr_t)&aLabel->mWrappedText->mText;
    // round 3: TOUDialog, BadUserDialog, ChooseNetworkAdapterDialog
    case Op::ButtonSetControlId: aButton->mControlId = (int)b; return 0;
    case Op::SubscribeBroadcast: aComp->SubscribeToBroadcast(b != 0); return 0;
    case Op::BrowserShowTou:
        // as text (WON's HTML rendering is not ported)
        reinterpret_cast<BrowserComponent*>(a)->ShowFiles(LobbyGetSysTOUPath(), LobbyGetGameTOUPath(), false);
        return 0;
    case Op::BrowserTop: reinterpret_cast<BrowserComponent*>(a)->GetHTMLView()->SetOffsets(0, 0); return 0;
    case Op::HttpDocType: return reinterpret_cast<GotHTTPDocumentEvent*>(a)->mOp->GetNum(EE_WON_HTTPDOC_TYPE);
    case Op::ListBeginUpdate: aList->BeginUpdate(); return 0;
    case Op::ListEndUpdate: aList->EndUpdate(); return 0;
    case Op::ListClear: aList->Clear(b != 0); return 0;
    case Op::BadUsersInsert: {
        std::pair<ListArea*, bool> aTarget(aList, c != 0);
        ee_won_routeop_records(reinterpret_cast<const EeWonRouteOp*>(b), EE_WON_RTR_BAD_USER, InsertBadUser, &aTarget);
        return 0;
    }
    case Op::IdsClear: reinterpret_cast<BadUserDialog::IdList*>(a)->clear(); return 0;
    case Op::ListRewind: aList->RewindSelections(); return 0;
    case Op::ListHasMore: return aList->HasMoreSelections() ? 1 : 0;
    case Op::ListNextSel: return (intptr_t)aList->GetNextSelection();
    // the original read the id (0x7f5ca5) before allocating the list node
    case Op::BadUserId: return (intptr_t)reinterpret_cast<BadUserItem*>(a)->mValues[1];
    case Op::IdsPush: reinterpret_cast<BadUserDialog::IdList*>(a)->push_back((unsigned long)b); return 0;
    case Op::AdapterAddDefault:
        aMulti->InsertItem(new NetworkAdapterListItem(0), -1);
        aMulti->SetString(0, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Default_String), NULL);
        return 0;
    case Op::LocalHost: return (intptr_t)WONAPI::IPAddr::GetLocalHost();
    case Op::ListSetSel: aList->SetSelItem((int)b, c != 0); return 0;
    case Op::ListNumItems: return aList->GetNumItems();
    case Op::AdaptersGet: {
        std::list<LobbyNetAddress> anAddresses;
        LobbyGetNetAddresses(anAddresses, true);
        return (intptr_t)new std::vector<LobbyNetAddress>(anAddresses.begin(), anAddresses.end());
    }
    case Op::AdaptersCount: return (intptr_t)reinterpret_cast<std::vector<LobbyNetAddress>*>(a)->size();
    case Op::AdapterHandle: return (intptr_t)(*reinterpret_cast<std::vector<LobbyNetAddress>*>(a))[b].mHandle;
    case Op::AdapterNameLength:
        return (intptr_t)GUIString((*reinterpret_cast<std::vector<LobbyNetAddress>*>(a))[b].mName).length();
    case Op::AdapterInsertRow: aMulti->InsertItem(new NetworkAdapterListItem((long)b), -1); return 0;
    case Op::AdapterSetName: {
        GUIString aName((*reinterpret_cast<std::vector<LobbyNetAddress>*>(b))[c].mName);
        aMulti->SetString(aMulti->GetNumItems() - 1, 0, aName, NULL);
        return 0;
    }
    case Op::AdapterSetUnknownName:
        aMulti->SetString(aMulti->GetNumItems() - 1, 0, LOBBY_STRING(ChooseNetworkAdapterDialog_Unknown_String), NULL);
        return 0;
    case Op::AdapterSetAddress: {
        IPAddr anAddress((long)b, 0);
        aMulti->SetString(aMulti->GetNumItems() - 1, 1, anAddress.GetHostString(true), NULL);
        return 0;
    }
    case Op::AdaptersFree: delete reinterpret_cast<std::vector<LobbyNetAddress>*>(a); return 0;
    case Op::ListGetSel: return (intptr_t)aList->GetSelItem();
    case Op::AdapterHost: return (intptr_t)reinterpret_cast<NetworkAdapterListItem*>(a)->mHost;
    // round 3: CreditsDialog
    case Op::AddRef: aComp->AddRef(); return 0;
    case Op::CreditsPlace: {
        Component* aButton = reinterpret_cast<Component*>(b);
        Component* aParent = aButton->mParent;
        aDialog->Fn2F6140(reinterpret_cast<Component*>(a), 0x5003, aButton, aParent->Left(), aParent->Top(), 0, 0, 0);
        return 0;
    }
    case Op::AddChild: aDialog->AddChild(reinterpret_cast<Component*>(a)); return 0;
    case Op::ScrollStart: reinterpret_cast<ScrollingTextComponent*>(a)->Start(); return 0;
    case Op::ScrollStop: reinterpret_cast<ScrollingTextComponent*>(a)->Stop(); return 0;
    case Op::ScrollReset: reinterpret_cast<ScrollingTextComponent*>(a)->Reset(); return 0;
    case Op::ScrollAddLine:
        reinterpret_cast<ScrollingTextComponent*>(a)->AddLine(*reinterpret_cast<const GUIString*>(b),
                                                             (unsigned long)(uint32_t)c);
        return 0;
    case Op::DefaultFontStringWidth: {
        const GUIString& aText = *reinterpret_cast<const GUIString*>(a);
        return aDialog->GetDefaultFont()->GetStringWidth(aText, 0, aText.length());
    }
    // EELobbyConnectDialog
    case Op::GameIsHost: return reinterpret_cast<EELobbyGame*>(a)->GetIsHost() ? 1 : 0;
    case Op::GameRust: return (intptr_t) reinterpret_cast<EELobbyGame*>(a)->mRust;
    case Op::ConnectInsert: {
        Image* theImage = ((c >> 8) == 0 ? LOBBY_IMAGE(GameUserListCtrl_Ready_Image) : LOBBY_IMAGE(LobbyGlobal_Searching_Image)).mPtr;
        reinterpret_cast<ListArea*>(a)->InsertItem(
            new ConnectListItem(EELobbyStr(b), theImage, (unsigned char)c), -1);
        return theImage != LOBBY_IMAGE(GameUserListCtrl_Ready_Image).mPtr ? 1 : 0;
    }
    case Op::DoDialog: return LobbyDoDialog(aDialog);
    case Op::RootWindow: return (intptr_t)aDialog->GetRootWindow();
    case Op::EndDialog: reinterpret_cast<Window*>(a)->EndDialog((int)b, aDialog); return 0;
    case Op::ListGetItem: return (intptr_t) reinterpret_cast<ListArea*>(a)->GetItem((int)b);
    case Op::ConnectItemSlot: return reinterpret_cast<ConnectListItem*>(a)->mSlot;
    case Op::ConnectItemSetImage:
        reinterpret_cast<ConnectListItem*>(a)->mImage =
            (b == 0 ? LOBBY_IMAGE(GameUserListCtrl_Ready_Image) : LOBBY_IMAGE(ServerListCtrl_PingFailed_Image)).mPtr;
        return 0;
    case Op::ConnectItemNotReady:
        return reinterpret_cast<ConnectListItem*>(a)->mImage.mPtr != LOBBY_IMAGE(GameUserListCtrl_Ready_Image).mPtr ? 1 : 0;
    case Op::ListRefresh: reinterpret_cast<ListArea*>(a)->RefreshItem(reinterpret_cast<ListItem*>(b)); return 0;
    // EELobbyFileDialog
    case Op::FindFirst: {
        EELobbyFind* aFind = new EELobbyFind;
        aFind->mFind = PlatFindFirst(reinterpret_cast<const char*>(a), &aFind->mData);
        if (aFind->mFind != NULL)
            return (intptr_t)aFind;
        delete aFind;
        return 0;
    }
    case Op::FindNameLength: return (intptr_t)GUIString(reinterpret_cast<EELobbyFind*>(a)->mData.mName).length();
    case Op::FileListInsert: {
        GUIString theName(reinterpret_cast<EELobbyFind*>(b)->mData.mName);
        theName.resize((int)c);
        aList->InsertString(theName, -1);
        return 0;
    }
    case Op::FindNext:
        return PlatFindNext(reinterpret_cast<EELobbyFind*>(a)->mFind, &reinterpret_cast<EELobbyFind*>(a)->mData) == PLAT_FILE_OK;
    case Op::FindClose:
        PlatFindClose(reinterpret_cast<EELobbyFind*>(a)->mFind);
        delete reinterpret_cast<EELobbyFind*>(a);
        return 0;
    case Op::ItemText: {
        static std::string sText; // the Rust copies it before its next op
        sText = static_cast<TextListItem*>(reinterpret_cast<ListItem*>(a))->mText.mData->GetString();
        return (intptr_t)sText.c_str();
    }
    case Op::LoadSavedGame: return EELobbyLoadSavedGame(reinterpret_cast<const char*>(a)) ? 1 : 0;
    case Op::Root: return (intptr_t)aDialog->GetRoot();
    case Op::FileAskDelete:
        return LobbyMessageBoxYesNo(EE_LOBBY_STRING(gEELobbyFileDialog_DeleteTitle),
                                    EE_LOBBY_STRING(gEELobbyFileDialog_DeleteDetails), true);
    case Op::FileDelete: WONFile(reinterpret_cast<const char*>(a)).Delete(true); return 0;
    case Op::ListRemove: aList->RemoveItem(reinterpret_cast<ListItem*>(b)); return 0;
    case Op::EventDoubleClick: return reinterpret_cast<ListItemClickedEvent*>(a)->mDoubleClick ? 1 : 0;
    case Op::EventButton: return reinterpret_cast<ListItemClickedEvent*>(a)->mButton;
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

// the SmartPtr members are released in reverse member order, before the Container base
LobbyRustPopup::~LobbyRustPopup() { ee_won_lobbypopup_free(this->mLobbyPopup); }

void LobbyRustPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    EeWonLobbyPopupEvent aType = inEvent->mType == ComponentEvent_ListItemUpClicked
                                     ? EeWonLobbyPopupEvent::ItemUpClicked
                                     : EeWonLobbyPopupEvent::Other;
    this->PCall(EeWonLobbyPopupCall::Event, (intptr_t)inEvent, (intptr_t)aType);
}

static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}

static intptr_t LobbyPopupHost(void* inPopup, EeWonLobbyPopupOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyPopupOp Op;
    LobbyRustPopup* aPopup = static_cast<LobbyRustPopup*>(inPopup);
    PopupMenu* aMenu = reinterpret_cast<PopupMenu*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    ListItemClickedEvent* anEvent = reinterpret_cast<ListItemClickedEvent*>(a);
    switch (inOp) {
    case Op::MenuNew: return (intptr_t) new PopupMenu;
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::AddChild: aPopup->AddChild(reinterpret_cast<Component*>(a)); return 0;
    case Op::MenuClear: aMenu->Clear(false); return 0;
    case Op::MenuFont: return Held(aMenu->mFont.get());
    case Op::PopupFontMod: return Held(aPopup->GetFontMod(reinterpret_cast<Font*>(a), 1, 0));
    case Op::MenuFontMod: return Held(aMenu->GetFontMod(reinterpret_cast<Font*>(b), 1, 0));
    case Op::MenuSetFont: aMenu->SetFont(reinterpret_cast<Font*>(b)); return 0;
    case Op::MenuAssignFont: aMenu->mFont = reinterpret_cast<Font*>(b); return 0;
    case Op::MenuTitleClient: aMenu->AddItem(reinterpret_cast<LobbyClient*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleGame: aMenu->AddItem(reinterpret_cast<LobbyGame*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleFriend: aMenu->AddItem(reinterpret_cast<LobbyFriend*>(b)->GetName(), 0)->mSelectable = false; return 0;
    case Op::MenuTitleNumber: {
        char aBuffer[256];
        sprintf(aBuffer, "%d", (int)b);
        aMenu->AddItem(GUIString(aBuffer), 0)->mSelectable = false;
        return 0;
    }
    case Op::MenuAddRes: aMenu->AddItem(gLobbyResources[b].mString, NULL, NULL, (int)c); return 0;
    case Op::MenuAddSeparator: aMenu->AddSeperator(); return 0;
    case Op::Width: return reinterpret_cast<Component*>(a)->Width();
    case Op::Height: return reinterpret_cast<Component*>(a)->Height();
    case Op::SetSize: aPopup->SetSize((int)a, (int)b); return 0;
    case Op::Config: return (intptr_t)GetLobbyConfig();
    case Op::ConfigNoPingInProgress: return reinterpret_cast<LobbyConfig*>(a)->GetNoPingInProgress() ? 1 : 0;
    case Op::BaseEvent: aPopup->Container::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(a)); return 0;
    case Op::EventItem: return (intptr_t)anEvent->mItem.get();
    case Op::EventButton: return anEvent->mButton;
    case Op::ItemMenuId: return reinterpret_cast<MenuItem*>(a)->mMenuId;
    case Op::DeliverClient:
        aPopup->DeliverEvent(new ClientActionEvent((int)a, (int)c, reinterpret_cast<LobbyClient*>(b)), false);
        return 0;
    case Op::DeliverGame: aPopup->DeliverEvent(new GameActionEvent((int)a, reinterpret_cast<LobbyGame*>(b)), false); return 0;
    case Op::DeliverFriend:
        aPopup->DeliverEvent(new FriendActionEvent((int)a, reinterpret_cast<LobbyFriend*>(b), (int)c), false);
        return 0;
    case Op::ClosePopup: aPopup->GetRoot()->ClosePopup(aPopup); return 0;
    }
    return 0;
}

}
