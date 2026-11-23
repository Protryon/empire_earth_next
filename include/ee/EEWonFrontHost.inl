#include "rust/won_lobby/wonfront.h"
// The WON front end's screens' host (R6 WON round 32): every C++ statement the Rust screens (ee_won_lobby
// wonfront.rs) run on a form, control, text, list line, the UI manager or gWonNet, one statement each, in the
// original's order (EeWonFrontOp documents them). Included by EEUIEWonNetArenaGameEvents.cpp and by
// tests/won/won_wonnet_test.cpp (over its stand-ins), so both run these very statements. Needs, in scope: the UI
// classes, the three boxes, EEWonNetAvailableGameFormManager, the screens' event classes, OnGamePassword, gWonNet,
// gUIManager, gResource, gHostedGameName, EEIsValidName, TMGetTime, WONAPI::WStringToString.
static_assert(sizeof(EeWonFrontRect) == sizeof(GERectangle), "EeWonFrontRect is GERectangle's layout");

// a C string into a Rust sink
static intptr_t FrontBytes(const char* inText, intptr_t inSink) { return (ee_won_front_sink((void*)inSink, (const uint8_t*)inText, strlen(inText)), 0); }

static intptr_t FrontRes(long inId, intptr_t inSink)
{
    UWideString theText;
    gResource->GetString(inId, theText);
    return FrontBytes(theText, inSink);
}

// a server's name as the server list shows it (wcstombs into the original's buffer)
static intptr_t FrontMbs(const std::wstring& inName, intptr_t inSink)
{
    char theBuffer[1024] = {0};
    wcstombs(theBuffer, inName.c_str(), sizeof(theBuffer));
    return FrontBytes(theBuffer, inSink);
}

static WONAPI::DirEntity* FrontServer(intptr_t inIndex) { return std::next(gWonNet->mServers.begin(), inIndex)->get(); }

static UIEvent* FrontNewEvent(EeWonFrontEvent inEvent)
{
    typedef EeWonFrontEvent E;
    switch (inEvent) {
    case E::AgActivateForm: return new EEUIEWNAGActivateForm;
    case E::AgBack: return new EEUIEWNAGBack;
    case E::AgCheckPassword: return new EEUIEWNAGCheckPassword;
    case E::AgClickGameFilter: return new EEUIEWNAGClickGameFilter;
    case E::AgCloseForm: return new EEUIEWNAGCloseForm;
    case E::AgDeactivateForm: return new EEUIEWNAGDeactivateForm;
    case E::AgDoubleclickUser: return new EEUIEWNAGDoubleclickUser;
    case E::AgHostOrJoinGame: return new EEUIEWNAGHostOrJoinGame;
    case E::AgSendChat: return new EEUIEWNAGSendChat;
    case E::AgToggleHostScreen: return new EEUIEWNAGToggleHostScreen;
    case E::AgToggleJoinScreen: return new EEUIEWNAGToggleJoinScreen;
    case E::AgUpdateForm: return new EEUIEWNAGUpdateForm;
    case E::ArActivateForm: return new EEUIEWNARActivateForm;
    case E::ArBack: return new EEUIEWNARBack;
    case E::ArClickUpdateAccount: return new EEUIEWNARClickUpdateAccount;
    case E::ArDeactivateForm: return new EEUIEWNARDeactivateForm;
    case E::ArDoubleClickRoom: return new EEUIEWNARDoubleClickRoom;
    case E::ArJoinRoom: return new EEUIEWNARJoinRoom;
    case E::ArUpdateForm: return new EEUIEWNARUpdateForm;
    case E::MuaCancel: return new EEUIEWNMUACancel;
    case E::MuaOpenScreen: return new EEUIEWNMUAOpenScreen;
    case E::MuaSubmit: return new EEUIEWNMUASubmit;
    case E::SsCancel: return new EEUIEWNSSCancel;
    case E::SsOk: return new EEUIEWNSSOk;
    default: return NULL;
    }
}

// the list lines' numbers and flags (EeWonFrontField)
static intptr_t FrontGamesField(const EEUICWNGBLine& inLine, intptr_t inField)
{
    typedef EeWonFrontField F;
    switch ((F)inField) {
    case F::Humans: return inLine.mHumans;
    case F::Players: return inLine.mPlayers;
    case F::HasPassword: return inLine.mHasPassword;
    case F::IsPrivate: return inLine.mIsPrivate;
    case F::IsInProgress: return inLine.mIsInProgress;
    default: return 0;
    }
}

static intptr_t FrontRoomsField(const EEUICWNRBLine& inLine, intptr_t inField) { return (EeWonFrontField)inField == EeWonFrontField::NumGames ? inLine.mNumGames : inLine.mNumPlayers; }

static intptr_t FrontStatField(const EEUICWNSBStat& inLine, intptr_t inField)
{
    typedef EeWonFrontField F;
    switch ((F)inField) {
    case F::L14: return (intptr_t)inLine.mL14;
    case F::L18: return (intptr_t)inLine.mL18;
    case F::StatNumGames: return (intptr_t)inLine.mNumGames;
    case F::L20: return (intptr_t)inLine.mL20;
    case F::L24: return (intptr_t)inLine.mL24;
    case F::L28: return (intptr_t)inLine.mL28;
    case F::L2C: return (intptr_t)inLine.mL2C;
    case F::TotalTime: return (intptr_t)inLine.mTotalTime;
    default: return 0;
    }
}

// the stat box's refresh line (the original's local)
static EEUICWNSBStat sFrontStat;

static void FrontStatSet(intptr_t inField, intptr_t inValue)
{
    typedef EeWonFrontField F;
    switch ((F)inField) {
    case F::L14: sFrontStat.mL14 = (unsigned long)inValue; break;
    case F::L18: sFrontStat.mL18 = (unsigned long)inValue; break;
    case F::StatNumGames: sFrontStat.mNumGames = (unsigned long)inValue; break;
    case F::L20: sFrontStat.mL20 = (unsigned long)inValue; break;
    case F::L24: sFrontStat.mL24 = (unsigned long)inValue; break;
    case F::L28: sFrontStat.mL28 = (unsigned long)inValue; break;
    case F::L2C: sFrontStat.mL2C = (long)inValue; break;
    case F::TotalTime: sFrontStat.mTotalTime = (unsigned long)inValue; break;
    default: break;
    }
}

// the games box's labels and their widths (0 password, 1 private, 2 in progress)
static UWideString& FrontGamesLabel(EEUICWonNetGamesBox* inBox, intptr_t inWhich) { return inWhich == 0 ? inBox->mPasswordLabel : inWhich == 1 ? inBox->mPrivateLabel : inBox->mInProgressLabel; }
static float& FrontGamesWidth(EEUICWonNetGamesBox* inBox, intptr_t inWhich) { return inWhich == 0 ? inBox->mPasswordWidth : inWhich == 1 ? inBox->mPrivateWidth : inBox->mInProgressWidth; }

static intptr_t WonFrontHost(EeWonFrontOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonFrontOp Op;
    typedef EEUICWonNetGamesBox G;
    typedef EEUICWonNetRoomsBox R;
    typedef EEUICWonNetStatBox S;
    switch (inOp) {
    // forms and the UI manager
    case Op::FormControl: return (intptr_t)((UIForm*)a)->GetControl((unsigned long)b);
    case Op::FormById: return (intptr_t)gUIManager->GetFormByID((unsigned long)a);
    case Op::FormGroupEnable: ((UIForm*)a)->GetEntityGroup((unsigned long)b)->Enable(); return 0;
    case Op::FormGroupDisable: ((UIForm*)a)->GetEntityGroup((unsigned long)b)->Disable(); return 0;
    case Op::FormGroupIsEnabled: return ((UIForm*)a)->GetEntityGroup((unsigned long)b)->GetIsEnabled();
    case Op::FormSetTabbing: ((UIForm*)a)->SetTabbing(b != 0); return 0;
    case Op::FormSetTabStop: ((UIForm*)a)->SetTabStop((unsigned long)b); return 0;
    case Op::FormUpdateEvent: return (intptr_t)((UIForm*)a)->GetFormUpdateEvent();
    case Op::FormSetUpdateEvent: ((UIForm*)a)->SetFormUpdateEvent((UIEvent*)b); return 0;
    case Op::FormSetActivateEvent: ((UIForm*)a)->SetFormActivateEvent((UIEvent*)b); return 0;
    case Op::FormSetDeactivateEvent: ((UIForm*)a)->SetFormDeactivateEvent((UIEvent*)b); return 0;
    case Op::FormSetCloseEvent: ((UIForm*)a)->SetFormCloseEvent((UIEvent*)b); return 0;
    case Op::FormSetOpenEvent: ((UIForm*)a)->SetFormOpenEvent((UIEvent*)b); return 0;
    case Op::FormPop: return ((UIForm*)a)->GetManager()->Pop();
    case Op::FormManager: return (intptr_t)static_cast<EEWonNetAvailableGameFormManager*>(((UIForm*)a)->GetFormManager());
    case Op::FormSetManager: ((UIForm*)a)->SetFormManager((EEWonNetAvailableGameFormManager*)b); return 0;
    case Op::FormManagerNew: return (intptr_t)new EEWonNetAvailableGameFormManager;
    case Op::FormManagerPoll: return (intptr_t)((EEWonNetAvailableGameFormManager*)a)->GetPoll();
    case Op::CurrentFormClearUpdateEvent: gUIManager->GetCurrentForm()->SetFormUpdateEvent(NULL); return 0;
    case Op::UiLock: gUIManager->LockUIForCopyData(); return 0;
    case Op::UiUnlock: gUIManager->UnlockUIForCopyData(); return 0;
    case Op::UiPush: gUIManager->Push((UIForm*)a); return 0;
    case Op::UiPop: gUIManager->Pop(); return 0;
    case Op::DisplayMessage: gUIManager->DisplayMessage(UWideString((const char*)a), true, NULL, 0, true); return 0;
    case Op::DisplayPasswordPrompt: gUIManager->DisplayPasswordPrompt(UWideString((const char*)a), true, OnGamePassword, true); return 0;
    case Op::ResString: return FrontRes((long)a, c);
    case Op::Time: return (intptr_t)TMGetTime();
    case Op::TextLength: return (intptr_t)UWideString((const char*)a).GetLength();
    case Op::IsValidName: return EEIsValidName(UWideString((const char*)a));
    // events
    case Op::EventNew: return (intptr_t)FrontNewEvent((EeWonFrontEvent)a);
    case Op::EventDeref: ((UIEvent*)a)->Dereference(); return 0;
    case Op::SetTrigger: EEUIEWNSetTriggerEvent((UIEvent*)a, (UIControl*)b, (unsigned long)c); return 0;
    case Op::ControlSetTriggerEvent: ((UIControl*)a)->SetTriggerEvent((unsigned long)b, (UIEvent*)c); return 0;
    // controls
    case Op::ControlSetDisplayMode: ((UIControl*)a)->SetDisplayMode(b != 0 ? kDisplayModeShown : kDisplayModeHidden); return 0;
    case Op::EditDeleteText: static_cast<UIEditBox*>((UIControl*)a)->DeleteText(); return 0;
    case Op::EditText: return FrontBytes(static_cast<UIEditBox*>((UIControl*)a)->GetText(0), c);
    case Op::EditSetText: static_cast<UIEditBox*>((UIControl*)a)->SetText(UWideString((const char*)b)); return 0;
    case Op::EditSetCaret: static_cast<UIEditBox*>((UIControl*)a)->SetCaretLocation((unsigned long)b, 0); return 0;
    case Op::EditSetHostedName: static_cast<UIEditBox*>((UIControl*)a)->SetText(gHostedGameName); return 0;
    case Op::CheckIsChecked: return static_cast<UICheckBox*>((UIControl*)a)->GetIsChecked();
    case Op::CheckSetChecked: static_cast<UICheckBox*>((UIControl*)a)->SetChecked(b != 0); return 0;
    case Op::RadioCheck: static_cast<UIRadioButton*>((UIControl*)a)->Check(); return 0;
    case Op::ButtonSetText: static_cast<UIButton*>((UIControl*)a)->SetText(UWideString((const char*)b), static_cast<UIButton*>((UIControl*)a)->GetGEText()->GetFont()); return 0;
    case Op::LabelSetText: static_cast<UITextLabel*>((UIControl*)a)->SetText(UWideString((const char*)b)); return 0;
    case Op::ListSelection: return ((UIListBox<UWideString>*)(UIControl*)a)->GetCurrentSelection(*(unsigned long*)b);
    case Op::ListData: return FrontBytes(UWideString(((UIListBox<UWideString>*)(UIControl*)a)->GetData((unsigned long)b)), c);
    case Op::ListHasSelection: return ((UIListBox<UWideString>*)(UIControl*)a)->GetHasSelection();
    case Op::ListClear: ((UIListBox<UWideString>*)(UIControl*)a)->DeleteAllEntries(); return 0;
    case Op::ListAppend: ((UIListBox<UWideString>*)(UIControl*)a)->AppendLine(UWideString((const char*)b)); return 0;
    case Op::ListSelectIndex: ((UIListBox<UWideString>*)(UIControl*)a)->ChangeSelectionIndex((unsigned long)b, true, false); return 0;
    case Op::GamesSelection: return static_cast<G*>((UIControl*)a)->GetCurrentSelection(*(unsigned long*)b);
    case Op::GamesInProgress: return static_cast<G*>((UIControl*)a)->GetIsInProgress((unsigned long)b);
    case Op::GamesHasPassword: return static_cast<G*>((UIControl*)a)->GetHasPassword((unsigned long)b);
    case Op::GamesName: return FrontBytes(static_cast<G*>((UIControl*)a)->GetGameName((unsigned long)b), c);
    case Op::RoomsSelection: return static_cast<R*>((UIControl*)a)->GetCurrentSelection(*(unsigned long*)b);
    case Op::RoomsName: return FrontBytes(static_cast<R*>((UIControl*)a)->GetData((unsigned long)b).mName, c);
    case Op::StatBoxRefresh: static_cast<S*>((UIControl*)a)->Refresh(); return 0;
    // the poll
    case Op::PollPoll: return ((TMPoll*)a)->Poll();
    case Op::PollIsRunning: return ((TMPoll*)a)->GetIsRunning();
    case Op::PollStart: ((TMPoll*)a)->Start(); return 0;
    case Op::PollStop: ((TMPoll*)a)->Stop(); return 0;
    // gWonNet
    case Op::WonNetExists: return gWonNet != NULL;
    case Op::GameListPort: return gWonNet->GetGameListPort();
    case Op::RefreshGameList: gWonNet->RefreshGameList((UIControl*)a); return 0;
    case Op::UpdateGameList: gWonNet->UpdateGameList(); return 0;
    case Op::LeaveRoom: gWonNet->LeaveRoom(); return 0;
    case Op::HostGame: gWonNet->HostGame((const char*)a, (const char*)b); return 0;
    case Op::JoinGame: gWonNet->JoinGame((const char*)a, (const char*)b); return 0;
    case Op::SendChat: gWonNet->SendChat(UWideString((const char*)a)); return 0;
    case Op::UserName: return FrontBytes(WONAPI::WStringToString(gWonNet->mUser->GetName()).c_str(), c);
    case Op::RoomName: return FrontBytes(WONAPI::WStringToString(gWonNet->GetRoomName()).c_str(), c);
    case Op::RoomListChanged: return gWonNet->GetRoomListChanged();
    case Op::UpdateRoomList: gWonNet->UpdateRoomList(dynamic_cast<R*>((UIControl*)a), dynamic_cast<UITextLabel*>((UIControl*)b), dynamic_cast<UITextLabel*>((UIControl*)c)); return 0;
    case Op::JoinRoom: gWonNet->JoinRoom((const char*)a, NULL); return 0;
    case Op::OpenUpdateAccount: gWonNet->OpenUpdateAccount(); return 0;
    case Op::ConnectToLobby: gWonNet->ConnectToLobby(); return 0;
    case Op::UpdateAccount: gWonNet->UpdateAccount(UWideString((const char*)a), UWideString((const char*)b), c != 0); return 0;
    case Op::ServerCount: return (intptr_t)gWonNet->mServers.size();
    case Op::ServerDisplayNameSize: return (intptr_t)FrontServer(a)->mDisplayName.size();
    case Op::ServerDisplayName: return FrontMbs(FrontServer(a)->mDisplayName, c);
    case Op::ServerName: return FrontMbs(FrontServer(a)->mName, c);
    case Op::SelectServer: gWonNet->SelectServer(FrontServer(a)->mDisplayName, FrontServer(a)->GetNetAddrAsIP()); return 0;
    // texts
    case Op::TextSetText: ((GEText*)a)->SetText(UWideString((const char*)b)); return 0;
    case Op::TextSetFgDefault: ((GEText*)a)->SetFgColor(GEColor()); return 0;
    case Op::TextSetFg: ((GEText*)a)->SetFgColor(GEColor(((const float*)b)[0], ((const float*)b)[1], ((const float*)b)[2])); return 0;
    case Op::TextSetBg: ((GEText*)a)->SetBgColor(GEColor(((const float*)b)[0], ((const float*)b)[1], ((const float*)b)[2])); return 0;
    case Op::TextSetLocation: ((GEText*)a)->SetLocation(GE2DPoint(((const float*)b)[0], ((const float*)b)[1])); return 0;
    case Op::TextSetTransparent: ((GEText*)a)->SetIsTransparent(b != 0); return 0;
    case Op::TextSetAlignLeft: ((GEText*)a)->SetAlignment(kTextAlignLeft); return 0;
    case Op::TextFontBold: ((GEText*)a)->SetFont(UIFontTable::FindOrCreateFont(((GEText*)a)->GetFont()->GetFontName(), ((GEText*)a)->GetFont()->GetFontStyle() | kFontStyleBold, ((GEText*)a)->GetFont()->GetFontQuality(), ((GEText*)a)->GetFont()->GetFontSize())); return 0;
    case Op::TextFontPlain: ((GEText*)a)->SetFont(UIFontTable::FindOrCreateFont(((GEText*)a)->GetFont()->GetFontName(), ((GEText*)a)->GetFont()->GetFontStyle() & ~kFontStyleBold, ((GEText*)a)->GetFont()->GetFontQuality(), ((GEText*)a)->GetFont()->GetFontSize())); return 0;
    case Op::TextExtentWidth: *(float*)b = ((GEText*)a)->GetTextExtent().GetWidth(); return 0;
    case Op::TextDelete: delete (GEText*)a; return 0;
    // the games box
    case Op::GamesCopiedOf: return (intptr_t)((G*)a)->mCopiedText;
    case Op::GamesTextOf: return (intptr_t)((G*)a)->mText;
    case Op::GamesSetCopied: ((G*)a)->mCopiedText = new GEText((GEFont*)b); return 0;
    case Op::GamesSetText: ((G*)a)->mText = new GEText((GEFont*)b); return 0;
    case Op::GamesSelectedBlack: ((G*)a)->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 0.0f)); return 0;
    case Op::GamesFocusBlack: ((G*)a)->SetFocusedRectColor(GEColor(0.0f, 0.0f, 0.0f)); return 0;
    case Op::GamesLabelSet: FrontGamesLabel((G*)a, b) = UWideString((const char*)c); return 0;
    case Op::GamesTextSetLabel: ((G*)a)->mCopiedText->SetText(FrontGamesLabel((G*)a, b)); return 0;
    case Op::GamesWidthGet: *(float*)c = FrontGamesWidth((G*)a, b); return 0;
    case Op::GamesWidthSet: FrontGamesWidth((G*)a, b) = *(const float*)c; return 0;
    case Op::GamesBaseCopyData: return ((G*)a)->UIListBox<EEUICWNGBLine>::CopyData();
    case Op::GamesCopyText: *((G*)a)->mCopiedText = *((G*)a)->mText; return 0;
    case Op::GamesDrawScrollBar: return ((G*)a)->FrontScrollBarText();
    case Op::GamesBaseDrawText: ((G*)a)->UIControl::DrawTextA(); return 0;
    case Op::GamesDrawCount: return (intptr_t)((G*)a)->FrontDrawCount();
    case Op::GamesLineHeight: return (intptr_t)((G*)a)->FrontLineHeight();
    case Op::GamesCopyRect: *(GERectangle*)b = ((G*)a)->FrontCopyRect(); return 0;
    case Op::GamesLineGet: return FrontGamesField(((G*)a)->FrontCopyLine((unsigned long)b), c);
    case Op::GamesTextSetLineName: ((G*)a)->mCopiedText->SetText(((G*)a)->FrontCopyLine((unsigned long)b).mName); return 0;
    case Op::GamesDrawClipped: return ((G*)a)->GetForm()->DrawClippedText(*(const GERectangle*)b, ((G*)a)->mCopiedText, (G*)a);
    // the rooms box
    case Op::RoomsCopiedOf: return (intptr_t)((R*)a)->mCopiedText;
    case Op::RoomsTextOf: return (intptr_t)((R*)a)->mText;
    case Op::RoomsSetCopied: ((R*)a)->mCopiedText = new GEText((GEFont*)b); return 0;
    case Op::RoomsSetText: ((R*)a)->mText = new GEText((GEFont*)b); return 0;
    case Op::RoomsSelectedBlack: ((R*)a)->GetSelectedBackground()->SetColor(GEColor(0.0f, 0.0f, 0.0f)); return 0;
    case Op::RoomsFocusBlack: ((R*)a)->SetFocusedRectColor(GEColor(0.0f, 0.0f, 0.0f)); return 0;
    case Op::RoomsBaseCopyData: return ((R*)a)->UIListBox<EEUICWNRBLine>::CopyData();
    case Op::RoomsCopyText: *((R*)a)->mCopiedText = *((R*)a)->mText; return 0;
    case Op::RoomsDrawScrollBar: return ((R*)a)->FrontScrollBarText();
    case Op::RoomsBaseDrawText: ((R*)a)->UIControl::DrawTextA(); return 0;
    case Op::RoomsDrawCount: return (intptr_t)((R*)a)->FrontDrawCount();
    case Op::RoomsLineHeight: return (intptr_t)((R*)a)->FrontLineHeight();
    case Op::RoomsCopyRect: *(GERectangle*)b = ((R*)a)->FrontCopyRect(); return 0;
    case Op::RoomsLineGet: return FrontRoomsField(((R*)a)->FrontCopyLine((unsigned long)b), c);
    case Op::RoomsTextSetLine: ((R*)a)->mCopiedText->SetText(c == 0 ? ((R*)a)->FrontCopyLine((unsigned long)b).mName : ((R*)a)->FrontCopyLine((unsigned long)b).mDescription); return 0;
    case Op::RoomsDrawClipped: return ((R*)a)->GetForm()->DrawClippedText(*(const GERectangle*)b, ((R*)a)->mCopiedText, (R*)a);
    // the stat box
    case Op::StatTextOf: return (intptr_t)((S*)a)->mText;
    case Op::StatSetRowHeight: ((S*)a)->mRowHeight = ((GEFont*)b)->GetHeight(); return 0;
    case Op::StatSetText: ((S*)a)->mText = new GEText((GEFont*)b); return 0;
    case Op::StatRowHeight: return (intptr_t)((S*)a)->mRowHeight;
    case Op::StatBaseCopyData: return ((S*)a)->UIListBox<EEUICWNSBStat>::CopyData();
    case Op::StatLinesDisplayed: return (intptr_t)((S*)a)->GetNumberLinesDisplayed();
    case Op::StatClearCopiedSelection: ((S*)a)->FrontClearCopiedSelection(); return 0;
    case Op::StatDrawScrollBar: return ((S*)a)->FrontScrollBarText();
    case Op::StatBaseDrawText: ((S*)a)->UIControl::DrawTextA(); return 0;
    case Op::StatCopyCount: return (intptr_t)((S*)a)->FrontCopyCount();
    case Op::StatCopyRect: *(GERectangle*)b = ((S*)a)->FrontCopyRect(); return 0;
    case Op::StatLineGet: return FrontStatField(((S*)a)->FrontCopyLine((unsigned long)b), c);
    case Op::StatTextSetLineText: ((S*)a)->mText->SetText(((S*)a)->FrontCopyLine((unsigned long)b).mText); return 0;
    case Op::StatDrawClipped: return ((S*)a)->GetForm()->DrawClippedText(*(const GERectangle*)b, ((S*)a)->mText, (S*)a);
    case Op::StatClear: ((S*)a)->DeleteAllEntries(); return 0;
    case Op::StatLineSet: FrontStatSet(a, b); return 0;
    case Op::StatLineSetText: sFrontStat.mText = UWideString((const char*)a); return 0;
    case Op::StatAppendLine: ((S*)a)->AppendLine(sFrontStat); return 0;
    default: return 0;
    }
}
