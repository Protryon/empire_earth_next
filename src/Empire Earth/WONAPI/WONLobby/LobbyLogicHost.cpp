// The lobby logics' C++ (NEW, no original address: units.EXTRA_UNITS): LobbyLogic and RustLogic, the shell of the
// logic modules that are Rust objects (logicshell.rs routes its calls), and each kind's maker and host: the one table
// of ops each Rust object calls out with, each op a run of the original's C++ statements on a C++ object with no
// decision between them (LanLogic's in LanLogicHost.cpp, InitLogic's in InitLogic.cpp). StagingLogic.cpp, and since
// R6 WON lobby and network round 40 AccountLogic.cpp, DialogLogic.cpp, LobbyLogic.cpp, PingLogic.cpp and
// RoutingLogic.cpp, are merged into this unit.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "rust/won_net/routeop_types.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/adminop.h"
#include "rust/won_lobby/staging.h"

// ---- LobbyLogic.cpp: the base (runs network ops, tracked so they can be killed) and RustLogic, the shell

namespace WONAPI {
// Defined in another unit; returns void
void LobbyRunOp(AsyncOp* inOp, unsigned long inTimeout);

// Creates the logic object, not killed, with an empty operation tracker
LobbyLogic::LobbyLogic() { this->mKilled = false; }

// Destroys the logic object (members only)
LobbyLogic::~LobbyLogic() {}

void LobbyLogic::KillHook() {}
void LobbyLogic::InitComponent(ComponentConfig*) {}
void LobbyLogic::Reset() {}
void LobbyLogic::HandleControlEvent(int) {}
void LobbyLogic::HandleComponentEvent(ComponentEvent*) {}

// Marks the logic killed, kills every tracked operation and runs the subclass kill hook
void LobbyLogic::Kill()
{
    this->mKilled = true;
    this->mTracker.KillAll();
    this->KillHook();
}

// Runs a network operation with its completion, tracking it so Kill can cancel it
void LobbyLogic::RunTrackedOp(AsyncOp* inOp, ShellCompletion* inCompletion,
                             unsigned long inTimeout)
{
    inOp->SetCompletion(inCompletion);
    if (inCompletion != NULL)
        inCompletion->mCheckTracker = true;
    this->mTracker.AddOp(inOp, 0);
    LobbyRunOp(inOp, inTimeout);
}

// Runs a network operation with its completion, untracked
void LobbyLogic::RunOp(AsyncOp* inOp, CompletionBase<AsyncOpPtr>* inCompletion,
                      unsigned long inTimeout)
{
    inOp->SetCompletion(inCompletion);
    LobbyRunOp(inOp, inTimeout);
}

// Stops tracking an operation; returns whether it was tracked
bool LobbyLogic::RemoveTrackedOp(AsyncOp* inOp)
{
    return this->mTracker.RemoveOp(inOp);
}

// The shell's completions and tracker for the Rust that runs the kinds' ops (logicshell.rs EeWonLogicShellCpp)
static void* ShellCompletionNew(void* inLogic, EeWonLogicShellDone inDone, int32_t inCode, bool inTracked)
{
    ShellCompletion* aCompletion = new ShellCompletion(static_cast<RustLogic*>(inLogic), inDone, inCode);
    aCompletion->mCheckTracker = inTracked;
    return static_cast<RefCount*>(aCompletion);
}
static EeWonOpTracker* ShellTracker(void* inLogic) { return static_cast<RustLogic*>(inLogic)->mTracker.GetTrackerObject(); }
static const EeWonLogicShellCpp kShellCpp = {ShellCompletionNew, ShellTracker};
[[maybe_unused]] static const bool sShellBound = (ee_won_logicshell_bind(&kShellCpp), true);

// ---- RustLogic: the shell of the six logics that are Rust objects (logicshell.rs routes its calls)

// a dialog of the kind's InitComponent into its slot (SafeGetDialog throws for a missing one: kept for the entry)
static intptr_t ShellFindDialog(void* inLogic, void* inConfig, int32_t inSlot, EeWonLobbyDlgKind inKind, const char* inName,
                                const char* inFile, int32_t inLine)
{
    return WON_GUARDED_V(SafeGetDialog(static_cast<ComponentConfig*>(inConfig), static_cast<RustLogic*>(inLogic)->mDialog[inSlot], inKind, inName, inFile, inLine));
}
RustLogic::RustLogic(EeWonLogicShellKind inKind, Maker inMake) : mKind(inKind), mRust(inMake(this)) {}
// Frees the Rust object first (it gives back what it holds), then the slots
RustLogic::~RustLogic() { ee_won_logicshell_free(this->mKind, this->mRust); }
void RustLogic::HandleComponentEvent(ComponentEvent* inEvent) { this->V(EeWonLogicShellV::HandleComponentEvent, (intptr_t)inEvent, inEvent->mType); }
// LanLogic's Resume left the current game when the lobby resumes (the others' are their Rust objects')
void RustLogic::Resume() { if (this->V(EeWonLogicShellV::Resume) != 0) LobbyLeaveGame(0); }
intptr_t RustLogic::V(EeWonLogicShellV inVirtual, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ee_won_logicshell_virtual(this->mKind, this->mRust, inVirtual, a, b, c, d, this, ShellFindDialog);
}
}

// ---- RoutingLogic.cpp: internet play over WON routing servers (routelogic.rs: every decision); the host's ops,
// each the original's calls on a C++ object. Internet only

extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in WONGUI/GUIEmptyWideString.cpp)

namespace WONAPI {
static GUIString* T(intptr_t a) { return reinterpret_cast<GUIString*>(a); }
static const GUIString& TX(intptr_t a) { return a != 0 ? *T(a) : gEmptyGUIString; }
static LobbyServer* S(intptr_t a) { return reinterpret_cast<LobbyServer*>(a); }
static LobbyGroup* G(intptr_t a) { return reinterpret_cast<LobbyGroup*>(a); }
static LobbyClient* C(intptr_t a) { return reinterpret_cast<LobbyClient*>(a); }
static LobbyFriend* F(intptr_t a) { return reinterpret_cast<LobbyFriend*>(a); }
static LobbyGame* GM(intptr_t a) { return reinterpret_cast<LobbyGame*>(a); }
static RoutingOp* RO(intptr_t a) { return reinterpret_cast<RoutingOp*>(a); }
static RoutingConnection* RC(intptr_t a) { return reinterpret_cast<RoutingConnection*>(a); }
static AsyncOp* AO(intptr_t a) { return reinterpret_cast<AsyncOp*>(a); }
static RustServerOp* RS(intptr_t a) { return static_cast<RustServerOp*>(AO(a)); }
static ChatCommandParser* P(intptr_t a) { return reinterpret_cast<ChatCommandParser*>(a); }
template <class X> static intptr_t Held(const SmartPtr<X>& inPtr)
{
    if (inPtr.get() != NULL) inPtr->AddRef();
    return (intptr_t)inPtr.get();
}
static void Broadcast(LobbyEvent* inEvent) { LobbyBroadcastEvent(inEvent, true); }
// a new op with one reference the Rust gives back
static intptr_t HeldOp(AsyncOp* inOp)
{
    inOp->AddRef();
    return (intptr_t)inOp;
}
static ComponentEvent* E(intptr_t a) { return reinterpret_cast<ComponentEvent*>(a); }
static const char* const kDirTypes[] = {"_C", "_L"}; // the room directory's entity types

static intptr_t RoutingHost(void* inObj, EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d,
                            intptr_t e)
{
    typedef EeWonRouteLogicOp O;
    RustLogic* L = static_cast<RustLogic*>(inObj);
    switch (inOp) {
    case O::TextFree: delete T(a); return 0;
    case O::TextLength: return T(a)->length();
    case O::DirListFree: delete reinterpret_cast<DirList*>(a); return 0;
    case O::TextUnits: // the text's wchar_t units (to b when not 0); 0 is the empty text
        if (b != 0)
            for (unsigned i = 0; i < TX(a).mData->mLength; i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)TX(a).mData->mText[i];
        return TX(a).mData->mLength;
    // ---- the lobby
    case O::Killed: return L->mKilled;
    case O::RunWithStatus:
        LobbyRunOpWithStatus(AO(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff),
                             (unsigned long)(d >> 8));
        return 0;
    case O::LastInputTick: return (intptr_t)GetLobbyWindow()->GetLastInputTick();
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::OnInternetScreen: return LobbyOnInternetScreen();
    case O::UserCanceledLast: return LobbyUserCanceledLastStatusOp();
    case O::Sound: LobbyPlaySound(gLobbyResources[a].mSound.get()); return 0;
    case O::MessageBox: LobbyMessageBox(gLobbyResources[a].mString, gLobbyResources[b].mString); return 0;
    case O::ErrorDialog: LobbyDoErrorDialog(gLobbyResources[a].mString, (WONStatus)b); return 0;
    case O::YesNo: return LobbyDoYesNoDialog(gLobbyResources[a].mString, gLobbyResources[b].mString, true);
    case O::HasConfig: return GetLobbyConfig() != NULL;
    case O::ConfigPingCeiling: return GetLobbyConfig() != NULL ? (intptr_t)(uint16_t)GetLobbyConfig()->mPingCeiling : -1;
    case O::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case O::MyGame: return (intptr_t)GetMyLobbyGame();
    case O::MyClient: return (intptr_t)GetMyLobbyClient();
    case O::Parser: return (intptr_t)LobbyGetChatCommandParser();
    case O::Lobby:
        switch (a) {
        case 1: LobbyKillPings(NULL); return 0;
        case 2: LobbyGameDisconnected(b != 0); return 0;
        case 3: return LobbyLeaveGame(b != 0);
        case 4: LobbyShowLobbyTab(); return 0;
        case 5: LobbyHostGame(); return 0;
        case 6: return LobbyGetNumPingsToDo();
        case 7: LobbyLogout(false); return 0;
        case 8: LobbyShowInternetScreen(); return 0;
        }
        return 0;
    case O::HandleStaging: LobbyHandleStagingMessage(RO(a)->GetData().get(), C(b)); return 0;
    case O::ClientLeftGame: LobbyClientLeftGame(C(a)); return 0;
    case O::ClientKicked: LobbyClientKicked(C(a), b != 0); return 0;
    case O::KickClient: LobbyKickClient(C(a), b != 0); return 0;
    case O::PingGame: LobbyPingGame(GM(a), false); return 0;
    case O::JoinGame: LobbyJoinGame(GM(a)); return 0;
    case O::SetBlocked: LobbySetClientBlocked(C(a), b != 0, 4); return 0;
    case O::SetIgnored: LobbySetClientIgnored(C(a), b != 0, 4); return 0;
    case O::SetLastRoom: LobbySetLastRoom(S(a)->GetName(), G(b)->GetName()); return 0;
    case O::ClientOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Client, (intptr_t)C(a), (intptr_t)C(b), (int)c);
        return 0;
    }
    case O::FriendOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Friend, (intptr_t)F(a), (int)b);
        return 0;
    }
    // ---- the model
    // (the model's other values, and the routing records read into it, are its Rust objects': modelobj.rs route_op)
    case O::ServerNum: return b == 4 ? S(a)->GetName() == LobbyGetLastServerName() : S(a)->HasAddress(GetLobbyConfig()->mLanguage);
    case O::GroupNum: return b == 7 ? G(a)->GetName().length() : G(a)->GetName() == LobbyGetLastRoomName();
    case O::AddFriendOfClient: return (intptr_t)LobbyAddFriend(C(a)->GetName(), C(a)->GetWONUserId());
    case O::AddFriendNamed: return (intptr_t)LobbyAddFriend(*T(a), (unsigned long)b);
    case O::GameText: return (intptr_t) new GUIString(b == 0 ? GM(a)->GetName() : GM(a)->GetPassword());
    case O::GameCreate: return Held(SmartPtr<LobbyGame>(LobbyGame::CreateGame(LobbyGameType_Internet)));
    // ---- the routing ops and connection (the ops' and connections' one-call cases are Rust's: netops.rs route_op)
    case O::RSetText: RO(a)->SetText((int)b, TX(c)); return 0;
    case O::ConnNew: { RoutingConnection* aConnection = new RoutingConnection; aConnection->AddRef(); return (intptr_t)aConnection; }
    case O::BufNew: return (intptr_t) new WriteBuffer(0, true);
    case O::GameWriteSummary: GM(a)->WriteSummary(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    // CreateDataObject(true, c, the game data type "G", gEmptyWideString, the summary, NULL)
    case O::ConnCreateGameObject: RoutingRequestValues().N(true).N((unsigned short)c).S(std::string("G")).W(gEmptyWideString).D(reinterpret_cast<WriteBuffer*>(b)->ToByteBuffer(true).get()).Run(RC(a), EE_WON_RTQ_CREATE_DATA, NULL); return 0;
    case O::BufFree: delete reinterpret_cast<WriteBuffer*>(a); return 0;
    // ---- the parser, the events, the dialogs
    case O::ParserNum:
        switch (b) {
        case 0: return (intptr_t)P(a)->GetRoomFlags();
        case 1: return P(a)->GetCommand();
        case 2: return (intptr_t)P(a)->GetDuration();
        case 3: return (intptr_t)P(a)->GetTarget();
        case 4: return (intptr_t)P(a)->GetClients();
        }
        return 0;
    case O::ParserText: return (intptr_t) new GUIString(P(a)->GetText());
    case O::ParserSetInput: P(a)->SetInput(C(b)->GetName()); return 0;
    case O::EventComponent: return (intptr_t)E(a)->mComponent;
    case O::EventType: return E(a)->mType;
    case O::EventFindServer: return (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->FindServer(LobbyEvtAddr(E(a), LobbyEvtField::Address));
    case O::EventAddressIs: return S(b)->GetAddress() == LobbyEvtAddr(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Address);
    case O::GameNameIsEvent: return GM(a)->GetName() == LobbyEvtText(reinterpret_cast<ComponentEvent*>(b), LobbyEvtField::RoomName);
    case O::DialogDo:
        switch (a) {
        case 1: return LobbyDoDialog(L->mDialog[kRoutingAddFriend].get());
        case 2: return LobbyDoDialog(L->mDialog[kRoutingAdminAction].get());
        case 3: return LobbyDoDialog(L->mDialog[kRoutingBadUser].get());
        case 4: return LobbyDoDialog(L->mDialog[kRoutingCreateRoom].get());
        case 5: return LobbyDoDialog(L->mDialog[kRoutingPassword].get());
        case 6: return LobbyDoDialog(L->mDialog[kRoutingRoom].get());
        }
        return 0;
    case O::DialogReset:
        switch (a) {
        case 1: L->mDialog[kRoutingAddFriend]->LCall(EeWonLobbyDlgCall::Reset); break;
        case 2: L->mDialog[kRoutingAdminAction]->LCall(EeWonLobbyDlgCall::Reset, (int)b, (intptr_t)C(c)); break;
        case 4: L->mDialog[kRoutingCreateRoom]->LCall(EeWonLobbyDlgCall::Reset, b != 0); break;
        case 5: L->mDialog[kRoutingPassword]->LCall(EeWonLobbyDlgCall::Reset); break;
        }
        return 0;
    case O::DialogText:
        switch (a) {
        case 1: return (intptr_t) new GUIString(L->mDialog[kRoutingCreateRoom]->InputText(0));
        case 2: return (intptr_t) new GUIString(L->mDialog[kRoutingCreateRoom]->InputTextOf(L->mDialog[kRoutingCreateRoom]->LCall(EeWonLobbyDlgCall::GetPassword)));
        case 3: return (intptr_t) new GUIString(L->mDialog[kRoutingPassword]->InputText(0));
        case 4: return (intptr_t) new GUIString(L->mDialog[kRoutingAdminAction]->InputText(2));
        case 5: return (intptr_t) new GUIString(L->mDialog[kRoutingAddFriend]->InputText(0));
        }
        return 0;
    case O::DialogNum:
        switch (a) {
        case 1: return L->mDialog[kRoutingAdminAction]->LCall(EeWonLobbyDlgCall::ApplyToServer) != 0;
        case 2: return (intptr_t)(unsigned long)L->mDialog[kRoutingAdminAction]->LCall(EeWonLobbyDlgCall::NumSeconds);
        case 3: return L->mDialog[kRoutingRoom]->GetParent() != NULL;
        }
        return 0;
    case O::RoomDialogEnd: GetLobbyWindow()->EndDialog(-1, NULL, L->mDialog[kRoutingRoom].get()); return 0;
    case O::BadUserSet: L->mDialog[kRoutingBadUser]->LCall(EeWonLobbyDlgCall::SetUserList, (intptr_t)RO(a)->mRust, b != 0); return 0;
    case O::BadUserIds: {
        std::list<unsigned long> ids;
        L->mDialog[kRoutingBadUser]->LCall(EeWonLobbyDlgCall::GetIds, (intptr_t)&ids);
        for (std::list<unsigned long>::iterator i = ids.begin(); i != ids.end(); ++i)
            ee_won_routelogic_push(reinterpret_cast<void*>(a), *i);
        return 0;
    }
    // ---- the events
    case O::EvClientChanged: Broadcast(ClientChangedEvent(C(a), (unsigned int)b, (int)c)); return 0;
    case O::EvGroupChanged: Broadcast(GroupChangedEvent(G(a), S(b), (int)c)); return 0;
    case O::EvFriendChanged: Broadcast(FriendChangedEvent(F(a), (int)b)); return 0;
    case O::EvAnnounceFriend: Broadcast(AnnounceFriendEvent(F(a), (int)b, (int)c)); return 0;
    case O::EvAnnounceClient:
        Broadcast(AnnounceClientLocationEvent(C(a), b != 0 ? G(b)->GetName() : gEmptyGUIString,
                                                  c != 0 ? G(c)->GetName() : gEmptyGUIString, (int)d));
        return 0;
    case O::EvGameChanged: Broadcast(GameChangedEvent(GM(a), (int)b)); return 0;
    case O::EvInvitationRecv:
        Broadcast(GroupInvitationEvent(4, GM(a)->GetName(), GM(a)->GetGroupId(), C(b), c != 0,
                                           GUIString(new GUIStringData(RO(d)->Text(EE_WON_RTT_COMMENT))), false));
        return 0;
    case O::EvInvitationSent:
        Broadcast(GroupInvitationEvent(4, G(a)->GetName(), G(a)->GetId(), C(b), c != 0, TX(d), true));
        return 0;
    case O::EvJoinAttempt:
        Broadcast(GroupJoinAttemptEvent(6, C(a), GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_COMMENT)))));
        return 0;
    case O::EvRecvChat:
        Broadcast(RecvChatEvent((unsigned int)a, (unsigned short)b, C(c), GUIString(RO(d)->Text(EE_WON_RTT_TEXT)), C(e)));
        return 0;
    case O::EvAlertTimed: Broadcast(AlertEvent(4, (int)a, (unsigned long)RO(b)->Num(EE_WON_RTF_TIME), GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT))))); return 0;
    case O::EvAlertPlain: Broadcast(AlertEvent(4, (int)a, 0, GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT))))); return 0;
    case O::EvModeratedGroup: Broadcast(ClientModeratedEvent(C(a), (int)b, G(c)->GetName(), (int)d, (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true, GUIString(RO(e)->Text(EE_WON_RTT_COMMENT)))); return 0;
    case O::EvModeratedServer: Broadcast(ClientModeratedEvent(C(a), (int)b, S(c)->GetName(), (int)d, (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true, GUIString(RO(e)->Text(EE_WON_RTT_COMMENT)))); return 0;
    case O::EvLeftRoom: Broadcast(LeftRoomEvent((int)a, b != 0, c != 0)); return 0;
    case O::EvSyncGameList: Broadcast(SyncGameListEvent(L->Slot<LobbyGameList>(kRoutingGameList))); return 0;
    case O::EvJoinedRoom:
        Broadcast(JoinedRoomEvent(S(a)->GetName(), G(b)->GetName(), (unsigned int)c, S(a)->GetMyClientId(),
                                      G(b)->GetClients().get()));
        return 0;
    case O::EvServerChanged: Broadcast(ServerChangedEvent(S(a), (int)b)); return 0;
    case O::EvSetChatInput: Broadcast(SetChatInputEvent(2, C(a)->GetName(), (int)b, false)); return 0;
    case O::EvNotCaptain:
        LobbySendEventTo(ChatCommandErrorEvent(LOBBY_STRING(RoutingLogic_NotCaptain_String)), reinterpret_cast<Component*>(a));
        return 0;
    // ---- the friends' searches, the server and room lists
    case O::UdpManager: return (intptr_t)LobbyGetUDPManager();
    case O::StatusOpNew: return HeldOp(new UDPManagerSendRecvOp(ee_won_userstatus_body(), L->Slot<LobbyServerList>(kRoutingServerList)->AddressAt((size_t)a), reinterpret_cast<UDPManager*>(b)));
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    case O::LobbyServerList: return (intptr_t)LobbyGetServerList();
    case O::RoomDirServers: return Held(SmartPtr<ServerContext>(LobbyGetRoomDirServers()));
    case O::DirOpNew: return HeldOp(RustServerOp::NewGetDir(reinterpret_cast<ServerContext*>(a), GetLobbyConfig()->mProductDirectory, 0x04000622, kDirTypes, 2));
    case O::DirEntitiesNew: return (intptr_t) new DirList(RS(a)->GetEntities(EE_WON_REQOP_DIR_LIST));
    case O::DirListEmpty: return reinterpret_cast<DirList*>(a)->empty();
    case O::DirUpdate: L->Slot<LobbyServerList>(kRoutingServerList)->UpdateFromDirectory(*reinterpret_cast<DirList*>(a)); return 0;
    }
    return 0;
}

// The Internet lobby logic's Rust object over the shell (not idle, not yet a moderator, no server), with an empty
// server list and game list (the shell's virtuals, completions and its six dialogs' lookup: logicshell.rs)
void* RoutingLogicMake(RustLogic* inSelf)
{
    return (inSelf->mSlot[kRoutingServerList] = new LobbyServerList, inSelf->mSlot[kRoutingGameList] = new LobbyGameList(2),
            ee_won_routelogic_new_model(inSelf, RoutingHost, inSelf->Slot<LobbyServerList>(kRoutingServerList),
                                        inSelf->Slot<LobbyGameList>(kRoutingGameList)));
}
}

// ---- AccountLogic.cpp: login, accounts, lost password/user name, credits, terms of use (logics.rs); its host

namespace WONAPI {
// the user's terms-of-use times set to the lobby's
static void UserSetTOUNow(LobbyUserInfo* inUser)
{
    long aSys, aGame;
    LobbyGetTOUTimes(aSys, aGame);
    LobbyUserSetTOU(inUser, aSys, aGame);
}

static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static AuthContext* A(intptr_t a) { return reinterpret_cast<AuthContext*>(a); }
static ServerContext* SC(intptr_t a) { return reinterpret_cast<ServerContext*>(a); }

// The host: each case one statement; the decisions and the order are the Rust's (logics.rs, and netlogicops.rs
// for the cases R6 WON round 32 split).
static intptr_t AccountHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    typedef LobbyEvtField EF;
    RustLogic* aLogic = static_cast<RustLogic*>(inObj);
    switch (inOp) {
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Exit: LobbyExit((int)a); return 0;
    case O::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case O::YesNoRes: return LobbyDoYesNoDialog(gLobbyResources[a].mString, gLobbyResources[b].mString, true);
    case O::MessageBoxRes: LobbyMessageBox(gLobbyResources[a].mString, gLobbyResources[b].mString); return 0;
    case O::CreditsLCall: aLogic->mDialog[kAccountCredits]->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::CreditsDo: return LobbyDoDialog(aLogic->mDialog[kAccountCredits].get());
    case O::TouLCall: aLogic->mDialog[kAccountTOU]->LCall((EeWonLobbyDlgCall)a); return 0;
    case O::TouDo: return LobbyDoDialog(aLogic->mDialog[kAccountTOU].get());
    case O::ShowCreateAccount: LobbyShowCreateAccount(); return 0;
    case O::ShowLostPassword: LobbyShowLostPassword(); return 0;
    case O::ShowLostUsername: LobbyShowLostUsername(); return 0;
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    case O::RunWithStatusRes: LobbyRunOpWithStatus(reinterpret_cast<AsyncOp*>(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff), (unsigned long)(d >> 8)); return 0;
    case O::GetCertNew: return HeldOp(RustServerOp::NewGetCert(A(a)));
    case O::GetAccountNew: return HeldOp(RustServerOp::NewGetAccount(SC(a), A(b)));
    case O::PasswordEmailNew: return HeldOp(RustServerOp::NewRequest(ee_won_reqop_password_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::InfoEmailNew: return HeldOp(RustServerOp::NewRequest(ee_won_reqop_info_email(reinterpret_cast<const uint8_t*>(a), (size_t)b), SC(c)));
    case O::UpdateAccountNew: return HeldOp(RustServerOp::NewUpdateAccount(SC(a), A(b), LobbyEvtText(E(c), EF::NewPassword), LobbyEvtText(E(c), EF::Email)));
    case O::CreateAccountNew: return HeldOp(RustServerOp::NewCreateAccount(SC(a), LobbyEvtStr<std::wstring>(E(b), EF::UserName), LobbyEvtStr<std::wstring>(E(b), EF::Password), LobbyEvtStr<std::string>(E(b), EF::Email), LobbyEvtGet(E(b), EF::Newsletter) != 0, (unsigned char)LobbyEvtGet(E(b), EF::Month), (unsigned char)LobbyEvtGet(E(b), EF::Day), (unsigned short)LobbyEvtGet(E(b), EF::Year)));
    case O::BroadcastAccount:
        LobbyBroadcastEvent(QueryAccountResultEvent(R(a)->GetBytes(EE_WON_REQOP_ACCOUNT_EMAIL), R(a)->GetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1) != 0), true);
        return 0;
    case O::AddUserNamed: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), "", false);
    case O::AddUserNoPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(GetEmptyGUIStringData()), false);
    case O::AddUserWithPassword: return (intptr_t)LobbyAddUser(LobbyEvtStr<std::wstring>(E(a), EF::UserName), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false);
    case O::UserSetTOUNow: UserSetTOUNow(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::SaveGlobalPrefsNow: LobbySaveGlobalPrefs(true); return 0;
    case O::SetCurrentUser: LobbySetCurrentUser(reinterpret_cast<LobbyUserInfo*>(a)); return 0;
    case O::BroadcastLoginFields: LobbyBroadcastEvent(SetLoginFieldsEvent(GUIString(LobbyEvtStr<std::wstring>(E(a), EF::UserName)), GUIString(LobbyEvtStr<std::wstring>(E(a), EF::Password)), false), true); return 0;
    case O::RootWindow: return (intptr_t)LobbyGetRootWindow();
    case O::KeyMod: return reinterpret_cast<Window*>(a)->GetKeyMod();
    case O::FindUserOfEvent: return (intptr_t)LobbyFindUser(GUIString(new GUIStringData(LobbyEvtStr<std::wstring>(E(a), EF::UserName))));
    case O::UserTOUNewer: return LobbyIsTOUNewer(LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 0), LobbyUserTOU(reinterpret_cast<LobbyUserInfo*>(a), 1));
    case O::AuthSetUserNameOf: A(a)->SetUserName(LobbyEvtStr<std::wstring>(E(b), EF::UserName)); return 0;
    case O::AuthSetPasswordOf: A(a)->SetPassword(LobbyEvtStr<std::wstring>(E(b), EF::Password)); return 0;
    case O::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case O::CDKeyValid: return LobbyIsCDKeyValid();
    case O::EnterInternet: LobbyEnterInternetScreen(a != 0); return 0;
    default: return 0;
    }
}

// The account logic's Rust object over the shell (its virtuals, its two dialogs' lookup: logicshell.rs)
void* AccountLogicMake(RustLogic* inSelf) { return ee_won_logic_new(EeWonLogicKind::Account, inSelf, AccountHost); }
}

// ---- PingLogic.cpp: game pings over the lobby UDP manager (logics.rs: the queue, ids, retries, bytes); its host

namespace WONAPI {
void LobbyNotifyPingChange(LobbyGame*); // update game UI/status
}

// One ping of a game host, retried by PingLogic: the original's PingGameOp (0x98 bytes) is a UDPManagerSendRecvOp
// with a Rust body since R6 WON lobby and network round 36 (logics.rs EeWonPingOp: the game, the request id, the
// ping, the flags, the retries, the reply; its RunHook and CheckResponse)
namespace WONAPI {
// a reader over a Rust reader's state, the position written back
template <class F> static intptr_t WithReader(intptr_t inState, F inRead)
{
    ReadBuffer aReader(NULL, 0);
    aReader.mState = *reinterpret_cast<EeWonReadBuffer*>(inState);
    intptr_t aResult = inRead(aReader);
    *reinterpret_cast<EeWonReadBuffer*>(inState) = aReader.mState;
    return aResult;
}

static intptr_t PingLogicHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    LobbyConfig* aConfig;
    switch (inOp) {
    case O::UdpManager: return (intptr_t)LobbyGetUDPManager();
    case O::EventGame: return LobbyEvtGet(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Game);
    case O::EventFlag: return LobbyEvtGet(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Flag);
    case O::MyGame: return (intptr_t)GetMyLobbyGame();
    case O::GameIsHost: return GM(a)->GetIsHost();
    case O::GameType: return (intptr_t)GM(a)->GetGameType();
    case O::GameInProgress: return GM(a)->IsInProgress();
    case O::GameIsPinging: return GM(a)->IsPinging();
    case O::GameQueryingDetails: return GM(a)->IsQueryingDetails();
    case O::GameSetPing: GM(a)->SetPing((unsigned short)b); return 0;
    case O::GameSetPingReplied: GM(a)->SetPingReplied(true); return 0;
    case O::NoPingInProgress: aConfig = GetLobbyConfig(); return aConfig != NULL && aConfig->mNoPingInProgress;
    case O::PingCeiling: aConfig = GetLobbyConfig(); return aConfig != NULL ? (intptr_t)aConfig->mPingCeiling : 5000;
    case O::ConnectionSpeed: return LobbyGetConnectionSpeed();
    case O::ReplyNew: return (intptr_t)new WriteBuffer(3, true);
    case O::BufAppend: reinterpret_cast<WriteBuffer*>(a)->AppendBytes(reinterpret_cast<const void*>(b), (unsigned long)c); return 0;
    case O::WriteSummary: GM(a)->WriteSummary(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case O::WriteDetails: GM(a)->WriteDetails(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case O::ReplySend:
        reinterpret_cast<UDPManager*>(a)->Send(reinterpret_cast<WriteBuffer*>(b)->ToByteBuffer(true).get(),
                                               reinterpret_cast<UDPManagerRecvOp*>(c)->GetFromAddr());
        return 0;
    case O::BufFree: delete reinterpret_cast<WriteBuffer*>(a); return 0;
    // (the ping op's request bytes and time are its Rust body's: logics.rs EeWonPingOp, R6 WON round 40)
    case O::ReadSummary: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { return GM(a)->ReadSummary(r) ? 1 : 0; });
    case O::ReadDetails: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { GM(a)->ReadDetails(r); return 0; });
    case O::BroadcastGameChanged: LobbyBroadcastEvent(GameChangedEvent(GM(a), 2), true); return 0;
    case O::NotifyPing: LobbyNotifyPingChange(GM(a)); return 0;
    case O::NewPingOp: return HeldOp(new UDPManagerSendRecvOp(ee_won_pingop_body(a, PingLogicHost, (unsigned short)b, (unsigned char)c), GM(a)->GetAddress(), reinterpret_cast<UDPManager*>(d)));
    default: return 0;
    }
}

// The ping logic's Rust object over the shell
void* PingLogicMake(RustLogic* inSelf) { return ee_won_logic_new(EeWonLogicKind::Ping, inSelf, PingLogicHost); }
}

// ---- DialogLogic.cpp: the lobby's modal dialogs (dlglogic.rs: the stack, the status op, every method); its host

namespace WONAPI {

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

// The dialog logic's Rust object over the shell (its virtuals and its status completion: logicshell.rs)
void* DialogLogicMake(RustLogic* inSelf) { return ee_won_dlglogic_new(inSelf, DialogLogicHost); }

// ---- the host

static Component* FindDialog(ComponentConfig* inConfig, bool inStatus)
{
    SmartPtr<LobbyRustDialog> aPtr;
    if (inStatus) SafeGetDialog(inConfig, aPtr, EeWonLobbyDlgKind::Status, "StatusDialog", "DialogLogic.cpp", 49);
    else SafeGetDialog(inConfig, aPtr, EeWonLobbyDlgKind::OkCancel, "OkCancelDialog", "DialogLogic.cpp", 48);
    Component* aFound = aPtr.get();
    if (aFound != NULL)
        aFound->AddRef();
    return aFound;
}

// the composed cases' locals (logicsplit.rs): NewWindow's window, StatusSetStatus's text, OkCancelSetText's buttons
static SmartPtr<Window> sDlgNewWindow;
static GUIString sDlgStatusText;
static const GUIString* sDlgButtons[2];

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonDlgLogicOp Op;
    Component* aComp = reinterpret_cast<Component*>(a);
    Window* aWindow = reinterpret_cast<Window*>(a);
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    switch (inOp) {
    case Op::FindOkCancel: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), false);
    case Op::FindStatus: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), true);
    case Op::ReleaseComponent: aComp->Release(); return 0;
    case Op::AddRefComponent: aComp->AddRef(); return 0;
    case Op::ReleaseWindow: aWindow->Release(); return 0;
    case Op::AddRefWindow: aWindow->AddRef(); return 0;
    case Op::ReleaseOp: anOp->Release(); return 0;
    case Op::AddRefOp: anOp->AddRef(); return 0;
    case Op::OpKill: anOp->Kill(true); return 0;
    case Op::OpForceFinishKilled: anOp->ForceFinish(WS_Killed); return 0;
    case Op::OpGetStatus: return (intptr_t)anOp->GetStatus();
    case Op::EndDialog: GetLobbyWindow()->EndDialog(-1, NULL, aComp); return 0;
    case Op::LobbyRoot: return (intptr_t)static_cast<Component*>(GetLobbyRoot());
    case Op::RootWindow: return (intptr_t)LobbyGetRootWindow();
    case Op::Parent: return (intptr_t)static_cast<Component*>(aComp->mParent);
    case Op::RemoveChild:
        static_cast<Container*>(aComp)->RemoveChild(reinterpret_cast<Component*>(b));
        return 0;
    case Op::WindowClose: aWindow->Close(); return 0;
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::SetClientSize: aWindow->SetClientSize((int)b, (int)c); return 0;
    case Op::WindowRoot: return (intptr_t)static_cast<Component*>(aWindow->mRoot.get());
    case Op::WindowCenter: aWindow->Center(true); return 0;
    case Op::CenterLayout:
        CenterLayout::Center(static_cast<Container*>(aComp), reinterpret_cast<Component*>(b), true, true);
        return 0;
    case Op::AddChild: static_cast<Container*>(aComp)->AddChild(reinterpret_cast<Component*>(b)); return 0;
    case Op::WindowShow: aWindow->Show(true); return 0;
    case Op::RunDialog: return aWindow->RunDialog(reinterpret_cast<Component*>(b));
    case Op::LobbySuspended: return LobbyIsSuspended() ? 1 : 0;
    case Op::StatusSetText:
        static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, d != 0);
        return 0;
    case Op::StatusSetStartTick: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStartTick, (intptr_t)WONTickCount()); return 0;
    case Op::SoundEffects: return LobbyGetLobbySoundEffects() ? 1 : 0;
    case Op::PlayErrorSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_Error_Sound).get()); return 0;
    case Op::RootWindowOf: return (intptr_t)aComp->GetRootWindow();
    // R6 WON lobby and network round 35: the statements of the cases logicsplit.rs composes (WmPopups, Bounds,
    // NewWindow, SetUpPopup, StatusSetStatus, OkCancelSetText); the locals are the statics above
    case Op::WmDefault: return (intptr_t)WONAPI::WindowManager::GetDefaultWindowManager();
    case Op::WmPopupsOf: return reinterpret_cast<WindowManager*>(a)->GetPopupsInOwnWindow() ? 1 : 0;
    case Op::BoundsX: return aComp->GetBounds().x;
    case Op::BoundsY: return aComp->GetBounds().y;
    case Op::BoundsWidth: return aComp->GetBounds().width;
    case Op::BoundsHeight: return aComp->GetBounds().height;
    case Op::NewWindowMake: sDlgNewWindow = CreateNativeWindow(); return 0;
    case Op::NewWindowOneComponent: sDlgNewWindow->SetOneComponentWindow(true); return 0;
    case Op::NewWindowGet: return (intptr_t)sDlgNewWindow.get();
    case Op::NewWindowDrop: sDlgNewWindow = NULL; return 0;
    case Op::WindowSetParent: aWindow->SetParent(reinterpret_cast<Window*>(b)); return 0;
    case Op::WindowSetPopupStyle: aWindow->SetStyle(0x22); return 0;
    case Op::WindowCreatePopup: aWindow->Create(WONRectangle(0, 0, 100, 100)); return 0;
    case Op::WindowListenRoot: LobbyAddLogicListener(aWindow->mRoot.get()); return 0;
    case Op::WindowRootSetScheme: aWindow->mRoot->SetColorScheme(LobbyGetColorScheme()); return 0;
    case Op::StatusTextOf: sDlgStatusText = GetWONStatusString((WONStatus)a); return 0;
    case Op::StatusSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStatus, (intptr_t)&sDlgStatusText, false); return 0;
    case Op::StatusTextDrop: sDlgStatusText = GUIString(); return 0;
    case Op::ButtonTextSet: sDlgButtons[a] = &gLobbyResources[b].mString; return 0;
    case Op::OkCancelSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, (intptr_t)sDlgButtons); return 0;
    case Op::InvalidateRect: {
        const int* aRect = reinterpret_cast<const int*>(b);
        WONRectangle aBounds(aRect[0], aRect[1], aRect[2], aRect[3]);
        static_cast<Container*>(aComp)->InvalidateRect(aBounds);
        return 0;
    }
    }
    return 0;
}
}

namespace WONAPI {
void LobbyNetSendToCaptain(const ByteBuffer*);
void LobbyNetBroadcastToGame(const ByteBuffer*);
void LobbyNetSendToClient(LobbyClient*, const ByteBuffer*);
void LobbyNetKickClient(LobbyClient*, bool);
void LobbyNetSetGameInProgress(bool);
void LobbyNetRemoveGameAdvertisement(LobbyGame*);
void LobbyNetLeaveGame(LobbyGame*);
}
using namespace WONAPI;

intptr_t LanLogicHostCall(RustLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD);
extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD);

// ---- StagingLogic: the RustLogic of kind Staging (its virtuals and its dialog's lookup: logicshell.rs)

namespace WONAPI {
void* StagingLogicMake(RustLogic* inSelf) { return ee_won_staging_new_model(inSelf, ee_won_lobbycpp_call); }

// The room message from the buffer's position (ee_won reads its type and fixed fields)
void StagingHandleMessage(RustLogic* inStaging, ReadBuffer& inBuffer, LobbyClient* inClient)
{
    inStaging->Call(EeWonStagingCall::HandleMessage, &inBuffer, inClient, (intptr_t)(reinterpret_cast<const uint8_t*>(inBuffer.data()) + inBuffer.pos()), (intptr_t)(inBuffer.length() - inBuffer.pos()));
}
}

// ---- the calls out

// a message op's message: the C++ ByteBuffer, or one over the bytes Rust built, or none
const ByteBuffer* WONAPI::LobbyHostMessage(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

template <class T> static intptr_t Held(T* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
}

extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD)
{
    // the C++ object the Rust one belongs to (a StagingLogic for the St ops; a LanLogic's ops go to LanLogicHost.cpp)
    RustLogic* st = static_cast<RustLogic*>(inLogic);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyClient* clB = static_cast<LobbyClient*>(inB);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    // a box's or status wait's texts: the title's and the details' resource rows (lobbyhost.rs `lobby_text`)
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    ByteBufferPtr aHold;
    switch (inOp) {
    // StagingLogic's members
    case EeWonLobbyOp::StGame: return (intptr_t)st->Slot<LobbyGame>(kStagingGame);
    case EeWonLobbyOp::StMyClient: return (intptr_t)st->Slot<LobbyClient>(kStagingMyClient);
    case EeWonLobbyOp::StSetMyClientFromNet: st->mSlot[kStagingMyClient] = LobbyGetNetMyClient(); return (intptr_t)st->Slot<LobbyClient>(kStagingMyClient);
    case EeWonLobbyOp::StSetGame: st->mSlot[kStagingGame] = g; return 0;
    case EeWonLobbyOp::StJoinOpNew: st->mSlot[kStagingJoinOp] = new AsyncOp; return 0;
    case EeWonLobbyOp::StSendJoinRequest: st->CallMessage(EeWonStagingCall::SendToCaptain, st->Slot<LobbyGame>(kStagingGame)->GetJoinRequest().get(), NULL); return 0;
    // StCreateDialogRun, StTakeDialogGame and GameNewCopy are lobbyops.rs's over these (R6 WON round 32)
    case EeWonLobbyOp::StCreateSetType: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::SetGameType, (int)inC); return 0;
    case EeWonLobbyOp::StCreateReset: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::Reset, false); return 0;
    case EeWonLobbyOp::StCreateDo: return LobbyDoDialog(st->mDialog[kStagingCreateGame].get());
    case EeWonLobbyOp::StDialogGameHeld: return Held(st->mDialog[kStagingCreateGame]->GetGame().get());
    case EeWonLobbyOp::GameSetMyAddress: g->SetAddress(LobbyGetMyAddress((int)inC)); return 0;
    case EeWonLobbyOp::GameCreateHeld: return Held(LobbyGame::CreateGame(static_cast<LobbyGameType>(g->GetGameType())));
    case EeWonLobbyOp::GameCopyFrom: g->CopyFrom(static_cast<LobbyGame*>(inB)); return 0;
    case EeWonLobbyOp::StClientModerated: LobbyBroadcastEvent(ClientModeratedEvent(cl, (int)inC, st->Slot<LobbyGame>(kStagingGame)->GetName(), (int)(inD >> 1), 0, (inD & 1) != 0, gEmptyGUIString), true); return 0;
    // a game
    case EeWonLobbyOp::GameHold: return Held(g);
    case EeWonLobbyOp::GameRelease: g->Release(); return 0;
    case EeWonLobbyOp::GameCanReady: return g->CanReady();
    case EeWonLobbyOp::GameCanStartAndTry: return g->CanStartGame() && g->TryStartGame();
    case EeWonLobbyOp::GamePreNetCreate: return g->PreNetCreateGame();
    case EeWonLobbyOp::GamePostNetCreate: return g->PostNetCreateGame();
    case EeWonLobbyOp::GamePreNetJoin: return g->PreNetJoinGame();
    case EeWonLobbyOp::GamePostNetJoin: return g->PostNetJoinGame();
    case EeWonLobbyOp::GameHandlePlayerJoined: g->HandlePlayerJoined(clB); return 0;
    // GameResetReadyFlags and GameJoinReplySend are lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::GameClientsIterNew: return (intptr_t) new LobbyClientSetIterator(g->GetClients().get());
    case EeWonLobbyOp::ClientsIterHasMore: return static_cast<LobbyClientSetIterator*>(inA)->HasMore();
    case EeWonLobbyOp::ClientsIterNext: return (intptr_t) static_cast<LobbyClientSetIterator*>(inA)->Next(static_cast<LobbyClient**>(inB));
    case EeWonLobbyOp::ClientsIterFree: delete static_cast<LobbyClientSetIterator*>(inA); return 0;
    case EeWonLobbyOp::PlayerSetReady: static_cast<LobbyPlayer*>(inA)->SetReady(inC != 0); return 0;
    case EeWonLobbyOp::GameJoinReplyHeld: return LobbyReadGuard([&] { return Held(const_cast<ByteBuffer*>(g->GetJoinReply(*buf, reinterpret_cast<LobbyClient*>(inD)).get())); });
    case EeWonLobbyOp::StSendToClientBuf: st->CallMessage(EeWonStagingCall::SendToClient, static_cast<const ByteBuffer*>(inA), clB); return 0;
    case EeWonLobbyOp::GameHandleJoinReply: return LobbyReadGuard([&] { return g->HandleJoinReply(*buf); });
    case EeWonLobbyOp::GameHandleGameMessage: return LobbyReadGuard([&] { g->HandleGameMessage((unsigned char)inC, *buf, reinterpret_cast<LobbyClient*>(inD)); return 0; });
    case EeWonLobbyOp::GameShowStatus:
        LobbyMessageBox(LOBBY_STRING(GameStagingLogic_JoinGameErrorTitle_String), g->GetStatusString((short)inC));
        return 0;
    case EeWonLobbyOp::GameDirectConnect: LobbyDispatchEvent(ChangeDirectConnectEvent(1, g)); return 0;
    case EeWonLobbyOp::GameShowOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Game, (intptr_t)g);
        return 0;
    }
    // a client
    // ClientReadPlayer and ClientBroadcastJoined are lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::PlayerCreateHeld: return Held(LobbyPlayer::CreatePlayer());
    case EeWonLobbyOp::PlayerReadFrom: return LobbyReadGuard([&] { return static_cast<LobbyPlayer*>(inA)->ReadFromBuffer(*buf); });
    case EeWonLobbyOp::ClientSetPlayer: cl->SetPlayer(static_cast<LobbyPlayer*>(inB)); return 0;
    case EeWonLobbyOp::WbufNew: return (intptr_t) new WriteBuffer(0, true);
    case EeWonLobbyOp::WbufAppend: static_cast<WriteBuffer*>(inA)->AppendBytes(reinterpret_cast<const void*>(inC), (unsigned long)inD); return 0;
    case EeWonLobbyOp::ClientWritePlayer: cl->GetPlayer()->WriteToBuffer(*static_cast<WriteBuffer*>(inB)); return 0;
    case EeWonLobbyOp::StBroadcastWbuf: st->CallMessage(EeWonStagingCall::BroadcastToGame, static_cast<WriteBuffer*>(inA)->ToByteBuffer(true).get(), NULL); return 0;
    case EeWonLobbyOp::WbufFree: delete static_cast<WriteBuffer*>(inA); return 0;
    case EeWonLobbyOp::ClientChanged: LobbyBroadcastEvent(PlayerChangedEvent(cl, (int)inC), true); return 0;
    case EeWonLobbyOp::ClientTeamChanged: LobbyBroadcastEvent(PlayerTeamChangedEvent(cl, (int)inC), true); return 0;
    // a message, a read buffer, events
    case EeWonLobbyOp::BufSkip: buf->SetPos((int)(buf->pos() + inC)); return 0;
    case EeWonLobbyOp::HandleBytes: StagingHandleBytes(st, ReadBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD), clB); return 0;
    case EeWonLobbyOp::NetSendToCaptain: LobbyNetSendToCaptain(LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetBroadcastToGame: LobbyNetBroadcastToGame(LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetSendToClient: LobbyNetSendToClient(clB, LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetKickClient: LobbyNetKickClient(clB, inC != 0); return 0;
    case EeWonLobbyOp::NetRemoveGameAdvertisement: LobbyNetRemoveGameAdvertisement(g); return 0;
    case EeWonLobbyOp::NetHostGame: return LobbyNetHostGame(g);
    case EeWonLobbyOp::NetJoinGame: return LobbyNetJoinGame(g);
    case EeWonLobbyOp::NetAdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    // the lobby
    case EeWonLobbyOp::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)inC, true); return 0;
    case EeWonLobbyOp::MessageBox: LobbyMessageBox(aTitle, aDetails); return 0;
    case EeWonLobbyOp::YesNo: return LobbyDoYesNoDialog(aTitle, aDetails, true);
    // RunWaitOp is lobbyops.rs's over these and LanTimerAlloc (R6 WON round 36)
    case EeWonLobbyOp::RunOpWithStatusMode: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, (int)inD, 500); return 0;
    case EeWonLobbyOp::OpIsKilled: return static_cast<AsyncOp*>(static_cast<RefCount*>(inA))->IsKilled();
    case EeWonLobbyOp::PlaySound: LobbyPlaySound(gLobbyResources[inC].mSound.get()); return 0; // a resource row
    case EeWonLobbyOp::FirewallTestDone: return LobbyIsFirewallTestDone();
    case EeWonLobbyOp::BehindFirewall: return LobbyIsBehindFirewall();
    case EeWonLobbyOp::GameScreenType: return LobbyGetGameScreenType();
    // the statements of the cases lobbyops.rs answers (R6 WONLobby round 29: the join op, the logout, leaving a game,
    // the game's progress and details, the join without a ping)
    case EeWonLobbyOp::StJoinOpIs: return st->mSlot[kStagingJoinOp].get() != NULL;
    case EeWonLobbyOp::StJoinOpGet: return (intptr_t)st->Slot<AsyncOp>(kStagingJoinOp);
    case EeWonLobbyOp::StJoinOpFinishSuccess: st->Slot<AsyncOp>(kStagingJoinOp)->Finish(WS_Success);
        return 0;
    case EeWonLobbyOp::StJoinOpClear: st->mSlot[kStagingJoinOp] = NULL;
        return 0;
    case EeWonLobbyOp::StJoinOpKillNow: st->Slot<AsyncOp>(kStagingJoinOp)->Kill(true);
        return 0;
    case EeWonLobbyOp::RunOpWithStatusRows: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, 1, 0);
        return 0;
    case EeWonLobbyOp::MessageBoxRows: LobbyMessageBox(aTitle, aDetails);
        return 0;
    case EeWonLobbyOp::StCreateDialogResetAll: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::Reset, true);
        return 0;
    case EeWonLobbyOp::NetLogout: LobbyNetLogout();
        return 0;
    case EeWonLobbyOp::KillPings: LobbyKillPings(0);
        return 0;
    case EeWonLobbyOp::ShowLoginScreen: LobbyShowLoginScreen();
        return 0;
    case EeWonLobbyOp::SaveGlobalPrefs: LobbySaveGlobalPrefs(false);
        return 0;
    case EeWonLobbyOp::SaveUserPrefs: LobbySaveUserPrefs(false);
        return 0;
    case EeWonLobbyOp::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop();
        return 0;
    case EeWonLobbyOp::GameClearPlayers: g->ClearPlayers();
        return 0;
    case EeWonLobbyOp::StClearMyClient: st->mSlot[kStagingMyClient] = NULL;
        return 0;
    case EeWonLobbyOp::GamePreNetLeave: g->PreNetLeaveGame();
        return 0;
    case EeWonLobbyOp::NetLeaveGame: LobbyNetLeaveGame(g);
        return 0;
    case EeWonLobbyOp::GamePostNetLeave: g->PostNetLeaveGame();
        return 0;
    case EeWonLobbyOp::GameSetInProgress: g->SetInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::NetSetGameInProgress: LobbyNetSetGameInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::LanPingGameNotify: LobbyPingGame(g, true);
        return 0;
    case EeWonLobbyOp::QueryGameDetails: LobbyQueryGameDetails(g);
        return 0;
    case EeWonLobbyOp::GameSetShowDetails: g->SetShowDetails(inC != 0);
        return 0;
    case EeWonLobbyOp::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case EeWonLobbyOp::ConfigAllowJoinWithoutPing: return static_cast<LobbyConfig*>(inA)->mAllowJoinGameWithoutPing;
    case EeWonLobbyOp::Exit: LobbyExit((int)inC); return 0;
    case EeWonLobbyOp::Now: return (intptr_t)time(NULL);
    default: return LanLogicHostCall(static_cast<RustLogic*>(inLogic), inOp, inA, inB, inC, inD);
    }
}
