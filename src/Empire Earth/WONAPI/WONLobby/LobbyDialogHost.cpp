// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby's small dialogs' shell and host (R6 WONLobby round 1): LanNameDialog, PasswordDialog, AddFriendDialog,
// DirectConnectDialog, LanPortDialog, CreateRoomDialog, CreateGameDialog and AdminActionDialog are one Rust object
// each (rust/crates/ee_won/src/lobbydlg.rs), as are StatusDialog and OkCancelDialog (round 2) and RoomDialog,
// TOUDialog, BadUserDialog and ChooseNetworkAdapterDialog (round 3). This unit is the C++ shell's two virtuals and the host: each op is the
// original's statements on a control, the dialog's base, a lobby game or a resource string, with no decision.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/ee_won.h"

using namespace WONAPI;

// ---- the list rows (R6 WONLobby round 3: moved here from BadUserDialog.cpp and ChooseNetworkAdapterDialog.cpp, whose
// dialogs are lobby dialog kinds now; the rows stay C++ widgets)

// One row of the bad user list (global namespace, 0x50 bytes)
class BadUserItem : public ListItem {
public:
    BadUserItem(const RoutingBadUser& inUser,bool inSimple);
    virtual void Paint(Graphics& inGraphics,ListArea* inList);
    virtual void CalcDimensions(ListArea* inList);
    virtual int Compare(const ListItem* inOther,ListArea* inList) const;
    SmartPtr<Font> mFont; // 0x18
    GUIString mStrings[6]; // 0x1c..0x30: name, WON id, expiry, moderator, moderator's WON id, comment
    unsigned long mValues[6]; // 0x34..0x48: sort keys; only the WON id (1), expiry (2) and moderator id (4) are set
    bool mIsSimple; // 0x4c (padded to 0x50)
};
// FUNCTION: WON 0x7f5290
// Creates a row from a bad user entry, writing the expiry as days, hours, minutes, seconds or infinite
BadUserItem::BadUserItem(const RoutingBadUser& inUser, bool inSimple)
{
    this->mIsSimple = inSimple;
    this->mStrings[0] = GUIString(new GUIStringData(inUser.mName));
    char aBuffer[100];
    sprintf(aBuffer, "%d", inUser.mWONId);
    this->mStrings[1] = aBuffer;
    this->mValues[1] = inUser.mWONId;

    unsigned long aDuration = inUser.mExpirationDiff;
    GUIString aText;
    GUIString aUnit;
    if (aDuration > 86400) {
        aDuration /= 86400;
        aUnit = aDuration > 1 ? ::BadUserDialog_XDays_String : ::BadUserDialog_OneDay_String;
    } else if (aDuration > 3600) {
        aDuration /= 3600;
        aUnit = aDuration > 1 ? ::BadUserDialog_XHours_String : ::BadUserDialog_OneHour_String;
    } else if (aDuration > 60) {
        aDuration /= 60;
        aUnit = aDuration > 1 ? ::BadUserDialog_XMinutes_String : ::BadUserDialog_OneMinute_String;
    } else if (aDuration > 0) {
        aUnit = aDuration > 1 ? ::BadUserDialog_XSeconds_String : ::BadUserDialog_OneSecond_String;
    } else {
        aUnit = ::BadUserDialog_Infinite_String;
    }
    sprintf(aBuffer, "%d", aDuration);
    this->mValues[2] = inUser.mExpirationDiff;
    this->mStrings[2] = LobbyFormatString(aUnit, GUIString(aBuffer));
    this->mStrings[3] = GUIString(inUser.mModeratorName);
    sprintf(aBuffer, "%d", inUser.mModeratorWONId);
    this->mStrings[4] = GUIString(aBuffer);
    this->mValues[4] = inUser.mModeratorWONId;
    this->mStrings[5] = GUIString(inUser.mModeratorComment);
}

// FUNCTION: WON 0x7f5790
// Takes the list's font and makes the row one text line high
void BadUserItem::CalcDimensions(ListArea* inList)
{
    this->mFont=inList->mFont;
    this->mHeight=this->mFont->GetHeight();
}
// FUNCTION: WON 0x7f5800
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
// FUNCTION: WON 0x7f58c0
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
// FUNCTION: WON 0x7f9c00
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
    : mLobbyDlg(ee_won_lobbydlg_new(this, inKind, LobbyDialogHost)) {}

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

static const GUIString& AdminString(intptr_t inString)
{
    static GUIString sEmpty;
    switch ((EeWonLobbyDlgString)inString) {
    case EeWonLobbyDlgString::Warn: return ::AdminActionDialog_Warn_String;
    case EeWonLobbyDlgString::Mute: return ::AdminActionDialog_Mute_String;
    case EeWonLobbyDlgString::UnMute: return ::AdminActionDialog_UnMute_String;
    case EeWonLobbyDlgString::Kick: return ::AdminActionDialog_Kick_String;
    case EeWonLobbyDlgString::Ban: return ::AdminActionDialog_Ban_String;
    case EeWonLobbyDlgString::ExplanationOpt: return ::AdminActionDialog_ExplanationOpt_String;
    case EeWonLobbyDlgString::ExplanationReq: return ::AdminActionDialog_ExplanationReq_String;
    case EeWonLobbyDlgString::Ok: return ::LobbyGlobal_OK_String;
    case EeWonLobbyDlgString::Cancel: return ::LobbyGlobal_Cancel_String;
    default: return sEmpty;
    }
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
    case Op::LabelSetString: aLabel->SetText(AdminString(b), true); return 0;
    case Op::ButtonSetString: aButton->SetText(AdminString(b), true); return 0;
    case Op::AdminTitle: {
        GUIString aTitle = AdminString(b);
        aTitle.append(GUIString(new GUIStringData(" ")));
        aTitle.append(reinterpret_cast<LobbyClient*>(c)->GetName());
        aLabel->SetText(aTitle, true);
        return 0;
    }
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
        const RoutingBadUserList& aUsers = *reinterpret_cast<const RoutingBadUserList*>(b);
        for (RoutingBadUserList::const_iterator anItr = aUsers.begin(); anItr != aUsers.end(); ++anItr)
            aList->InsertItem(new BadUserItem(*anItr, c != 0), -1);
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
        aMulti->SetString(0, 0, ::ChooseNetworkAdapterDialog_Default_String, NULL);
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
        aMulti->SetString(aMulti->GetNumItems() - 1, 0, ::ChooseNetworkAdapterDialog_Unknown_String, NULL);
        return 0;
    case Op::AdapterSetAddress: {
        IPAddr anAddress((long)b, 0);
        aMulti->SetString(aMulti->GetNumItems() - 1, 1, anAddress.GetHostString(true), NULL);
        return 0;
    }
    case Op::AdaptersFree: delete reinterpret_cast<std::vector<LobbyNetAddress>*>(a); return 0;
    case Op::ListGetSel: return (intptr_t)aList->GetSelItem();
    case Op::AdapterHost: return (intptr_t)reinterpret_cast<NetworkAdapterListItem*>(a)->mHost;
    }
    return 0;
}

}
