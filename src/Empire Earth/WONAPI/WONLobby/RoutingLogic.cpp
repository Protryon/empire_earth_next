// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// RoutingLogic: internet play over WON routing servers (rooms, games, friends, chat, moderation). A Rust object
// since R6 WONLobby round 9 (rust/crates/ee_won_lobby/src/routelogic.rs: the servers it is on and connecting to, the
// joined rooms, the idle and moderator state, the server list's refresh time and every decision, over routing.rs's);
// this unit keeps the shell (NetLogic's vtable, the tracker and completions, the server and game lists, the dialogs,
// the game data type) and the host's ops, each one the original's calls on a C++ object (the servers, rooms,
// players, friends and games, the routing connection and its ops, the dialogs, the events). Internet only: the
// routing servers are gone.
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "rust/won_net/routeop_types.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "rust/won_net/status_codes.h"
#include "ee/wonapi/WONLobbyResourceGlobals.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_net/hostop.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/reqops.h"
#include "rust/won_net/routeop.h"
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
static UDPManagerSendRecvOp* US(intptr_t a) { return static_cast<UDPManagerSendRecvOp*>(AO(a)); }
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
    case O::IterFree: delete reinterpret_cast<LobbyClientIterator*>(a); return 0;
    case O::TextUnits: // the text's wchar_t units (to b when not 0); 0 is the empty text
        if (b != 0)
            for (unsigned i = 0; i < TX(a).mData->mLength; i++) reinterpret_cast<uint32_t*>(b)[i] = (uint32_t)TX(a).mData->mText[i];
        return TX(a).mData->mLength;
    // ---- the lobby
    case O::Killed: return L->mKilled;
    case O::RunOp: L->RunOp(AO(a), NULL, 0); return 0;
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
    case O::ReconnectId: return (intptr_t)LobbyGetReconnectId(S(a)->GetAddress());
    case O::SetReconnectId: LobbySetReconnectId(S(a)->GetAddress(), (unsigned long)b); return 0;
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
    case O::ListNum: return b == 0 ? (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->GetCount() : L->Slot<LobbyServerList>(kRoutingServerList)->IsEmpty();
    case O::ListAt: return (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->ServerAt((size_t)a);
    case O::ListAddressIs: return S(b)->GetAddress() == L->Slot<LobbyServerList>(kRoutingServerList)->AddressAt((size_t)a);
    case O::GroupNum: return b == 7 ? G(a)->GetName().length() : G(a)->GetName() == LobbyGetLastRoomName();
    case O::AddFriendOfClient: return (intptr_t)LobbyAddFriend(C(a)->GetName(), C(a)->GetWONUserId());
    case O::AddFriendNamed: return (intptr_t)LobbyAddFriend(*T(a), (unsigned long)b);
    case O::GameText: return (intptr_t) new GUIString(b == 0 ? GM(a)->GetName() : GM(a)->GetPassword());
    case O::GameCreate: return Held(SmartPtr<LobbyGame>(LobbyGame::CreateGame(LobbyGameType_Internet)));
    case O::GameList:
        switch (a) {
        case 1: L->Slot<LobbyGameList>(kRoutingGameList)->Clear(); return 0;
        case 2: L->Slot<LobbyGameList>(kRoutingGameList)->Rewind(); return 0;
        case 3: return L->Slot<LobbyGameList>(kRoutingGameList)->HasMore();
        case 4: return (intptr_t)L->Slot<LobbyGameList>(kRoutingGameList)->Next();
        case 5: return (intptr_t)L->Slot<LobbyGameList>(kRoutingGameList)->GetGame((unsigned short)b);
        case 6: L->Slot<LobbyGameList>(kRoutingGameList)->AddGame(GM(b)); return 0;
        case 7: return Held(L->Slot<LobbyGameList>(kRoutingGameList)->RemoveGame((unsigned short)b));
        case 8: return (intptr_t)L->Slot<LobbyGameList>(kRoutingGameList)->GetOrCreateGame((unsigned short)b, reinterpret_cast<bool*>(c));
        }
        return 0;
    // ---- the routing ops and connection (the ops' and connections' one-call cases are Rust's: netops.rs route_op)
    case O::RSetText: RO(a)->SetText((int)b, TX(c)); return 0;
    case O::RConnection: return (intptr_t)RO(a)->mConnection.get();
    case O::ConnNew: { RoutingConnection* aConnection = new RoutingConnection; aConnection->AddRef(); return (intptr_t)aConnection; }
    case O::ConnSetAuth: RC(a)->SetAuth(reinterpret_cast<AuthContext*>(b), AUTH_TYPE_PERSISTENT); return 0;
    case O::ConnSetNotice: RC(a)->SetOpCompletion((int)b, new ShellCompletion(L, EeWonLogicShellDone::RouteNotify, (int)c)); return 0;
    case O::ConnSetClose: RC(a)->SetCloseCompletion(new ShellCompletion(L, EeWonLogicShellDone::RouteClosed)); return 0;
    // SubscribeDataObject(0xffff, the game data type "G", false, the notice completion of code 21)
    case O::ConnSubscribeGames: RoutingRequestValues().N(1).N(0xffff).S(std::string("G")).N(false).Run(RC(a), EE_WON_RTQ_READ_DATA, new ShellCompletion(L, EeWonLogicShellDone::RouteNotify, 21)); return 0;
    case O::BufNew: return (intptr_t) new WriteBuffer(0, true);
    case O::GameWriteSummary: GM(a)->WriteSummary(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    // CreateDataObject(true, c, the game data type "G", gEmptyWideString, the summary, NULL)
    case O::ConnCreateGameObject: RoutingRequestValues().N(true).N((unsigned short)c).S(std::string("G")).W(gEmptyWideString).D(reinterpret_cast<WriteBuffer*>(b)->ToByteBuffer(true).get()).Run(RC(a), EE_WON_RTQ_CREATE_DATA, NULL); return 0;
    case O::BufFree: delete reinterpret_cast<WriteBuffer*>(a); return 0;
    case O::ConnectOpNew: return HeldOp(RustServerOp::MadeOp(ee_won_servconn_connect_op_new(static_cast<RefCount*>(RC(a)), S(b)->GetAddress().GetValue())));
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
    case O::IterNew: return (intptr_t) new LobbyClientIterator(reinterpret_cast<LobbyClientList*>(a));
    case O::IterHasMore: return reinterpret_cast<LobbyClientIterator*>(a)->HasMore();
    case O::IterNext: return (intptr_t)reinterpret_cast<LobbyClientIterator*>(a)->Next();
    case O::EventComponent: return (intptr_t)E(a)->mComponent;
    case O::EventType: return E(a)->mType;
    case O::EventFindServer: return (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->FindServer(LobbyEvtAddr(E(a), LobbyEvtField::Address));
    case O::EventAddressIs: return S(b)->GetAddress() == LobbyEvtAddr(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Address);
    case O::GameNameIsEvent: return GM(a)->GetName() == LobbyEvtText(reinterpret_cast<ComponentEvent*>(b), LobbyEvtField::RoomName);
    case O::FriendAddressValid: return F(a)->GetAddress().IsValid();
    case O::FriendServer: { IPAddr anAddress = F(a)->GetAddress(); return (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->FindServer(anAddress); }
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
    case O::StatusOpRun:
        L->RunOp(AO(a), new ShellCompletion(L, EeWonLogicShellDone::RouteUserStatus), 10000);
        return 0;
    case O::StatusOpServer: return (intptr_t)L->Slot<LobbyServerList>(kRoutingServerList)->FindServer(US(a)->GetDestAddr());
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    // GetUserIdFromNameOp: a Rust op (ee_won reqops.rs) with the name, its id read back
    case O::UserIdOpNew: return HeldOp(RustServerOp::NewRequest(ee_won_reqop_user_id(reinterpret_cast<const uint8_t*>(a), (size_t)b), reinterpret_cast<ServerContext*>(c)));
    case O::LobbyServerList: return (intptr_t)LobbyGetServerList();
    case O::ServerListFind: return (intptr_t)reinterpret_cast<LobbyServerList*>(a)->FindServer(RS(b)->GetAddr());
    case O::GroupQueryNew: return HeldOp(RustServerOp::NewGroupListReq(S(a)->GetAddress(), 0x29));
    case O::RunTrackedGroups: L->RunTrackedOp(AO(a), new ShellCompletion(L, EeWonLogicShellDone::RouteGroups), 0); return 0;
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
            ee_won_routelogic_new_model(inSelf, RoutingHost));
}
}
