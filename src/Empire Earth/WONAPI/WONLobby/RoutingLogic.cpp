// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// RoutingLogic: internet play over WON routing servers (rooms, games, friends, chat, moderation). A Rust object
// since R6 WONLobby round 9 (rust/crates/ee_won/src/routelogic.rs: the servers it is on and connecting to, the
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
#include "ee/wonapi/WONRoutingOpTypes.h"
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
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONRoutingTypes.h"
#include "ee/wonapi/WONLobbyResourceGlobals.h"
#include "rust/ee_won.h"
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in EEWonNet.cpp)

// Routes one server notification on a routing connection to the logic (its code, routelogic.rs's).
// The original's RTTI names this class in the global namespace, not in WONAPI.
class RoutingOpCompletion : public WONAPI::LogicCompletionBase {
public:
    RoutingOpCompletion(int inCode, WONAPI::RoutingLogic* inLogic) : mCode(inCode), mLogic(inLogic) {}
    // The implicit destructor (0x7ee8f0) releases mLogic (+0x10) and stores no vptr of its own.
    // Run (0x7ee8a0): the logic skips a killed logic or op and an op of no server it knows
    virtual void Run(WONAPI::AsyncOp* inOp)
    {
        ee_won_routelogic_call(this->mLogic->mRust, EeWonRouteLogicCall::Notify, this->mCode,
                               (intptr_t) static_cast<WONAPI::RoutingOp*>(inOp));
    }
    int mCode; //0c
    WONAPI::SmartPtr<WONAPI::RoutingLogic> mLogic; //10
};

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
static RoutingGetUserStatusOp* US(intptr_t a) { return static_cast<RoutingGetUserStatusOp*>(AO(a)); }
static ChatCommandParser* P(intptr_t a) { return reinterpret_cast<ChatCommandParser*>(a); }
template <class X> static intptr_t Held(const SmartPtr<X>& inPtr)
{
    if (inPtr.get() != NULL) inPtr->AddRef();
    return (intptr_t)inPtr.get();
}
static void Broadcast(LobbyEvent* inEvent) { LobbyBroadcastEvent(inEvent, true); }
typedef std::vector<RoutingDataObjectPtr> RoutingDataObjectVector;

static intptr_t RoutingHost(void* inObj, EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d,
                            intptr_t e)
{
    typedef EeWonRouteLogicOp O;
    RoutingLogic* L = static_cast<RoutingLogic*>(inObj);
    switch (inOp) {
    case O::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::TextFree: delete T(a); return 0;
    case O::TextLength: return T(a)->length();
    case O::ListFree:
        if (b == 1) delete reinterpret_cast<RoutingDataObjectVector*>(a);
        else if (b == 2) delete reinterpret_cast<DirEntityList*>(a);
        else delete reinterpret_cast<LobbyClientIterator*>(a);
        return 0;
    // ---- the lobby
    case O::Killed: return L->mKilled;
    case O::OpStatus: return (int)AO(a)->GetStatus();
    case O::OpKilled: return AO(a)->IsKilled();
    case O::OpKill: AO(a)->Kill(true); return 0;
    case O::OpFinish: AO(a)->Finish(WS_Success); return 0;
    case O::AsyncOpNew: { AsyncOp* anOp = new AsyncOp; anOp->AddRef(); return (intptr_t)anOp; }
    case O::RunOp: L->RunOp(AO(a), NULL, 0); return 0;
    case O::RunWithStatus:
        LobbyRunOpWithStatus(AO(a), gLobbyResources[b].mString, gLobbyResources[c].mString, (int)(d & 0xff),
                             (unsigned long)(d >> 8));
        return 0;
    case O::Now: return (intptr_t)time(NULL);
    case O::Tick: return (intptr_t)WONTickCount();
    case O::LastInputTick: return (intptr_t)GetLobbyWindow()->GetLastInputTick();
    case O::BroadcastLobby: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case O::Pref:
        switch (a) {
        case 1: return LobbyGetChatSoundEffects();
        case 2: return LobbyGetStagingSoundEffects();
        case 3: return LobbyGetLobbySoundEffects();
        case 4: return LobbyGetAutoPingGames();
        case 5: return LobbyGetLobbyMusic();
        case 6: return LobbyGetAnonymousToFriends();
        case 7: return LobbyGetFilterDirtyWords();
        case 8: return LobbyOnInternetScreen();
        case 9: return LobbyUserCanceledLastStatusOp();
        }
        return 0;
    case O::Sound: LobbyPlaySound(gLobbyResources[a].mSound.get()); return 0;
    case O::MessageBox: LobbyMessageBox(gLobbyResources[a].mString, gLobbyResources[b].mString); return 0;
    case O::ErrorDialog: LobbyDoErrorDialog(gLobbyResources[a].mString, (WONStatus)b); return 0;
    case O::YesNo: return LobbyDoYesNoDialog(gLobbyResources[a].mString, gLobbyResources[b].mString, true);
    case O::HasConfig: return GetLobbyConfig() != NULL;
    case O::ConfigPingCeiling: return GetLobbyConfig() != NULL ? (intptr_t)(uint16_t)GetLobbyConfig()->mPingCeiling : -1;
    case O::AuthContext: return (intptr_t)GetLobbyAuthContext();
    case O::AuthStopRefresh: reinterpret_cast<AuthContext*>(a)->SetDoAutoRefresh(false); return 0;
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
        SmartPtr<ClientOptionsPopup> popup = new ClientOptionsPopup;
        popup->Init(C(a), C(b), (int)c);
        LobbyOpenPopup(popup.get(), PopupFlag_StandardPopup, -1, -1);
        return 0;
    }
    case O::FriendOptions: {
        SmartPtr<FriendOptionsPopup> popup = new FriendOptionsPopup;
        popup->Init(F(a), (int)b);
        LobbyOpenPopup(popup.get(), PopupFlag_StandardPopup, -1, -1);
        return 0;
    }
    // ---- the model
    // (the model's other values are its Rust objects': modelobj.rs route_op)
    case O::ServerNum: return b == 4 ? S(a)->GetName() == LobbyGetLastServerName() : S(a)->HasAddress(GetLobbyConfig()->mLanguage);
    case O::ServerAddGroup: return (intptr_t)S(a)->AddGroup(RO(b)->GetGroupInfo().get());
    case O::ServerAddClient: return (intptr_t)S(a)->AddClient(RO(b)->GetClientInfo().get());
    case O::ServerSyncClients: { RoutingClientMap m; RO(b)->GetClients(m); S(a)->SyncClients(m); return 0; }
    case O::ServerSyncGroups: { RoutingGroupMap m; RO(b)->GetGroups(m); S(a)->SyncGroups(m, c != 0); return 0; }
    case O::ListNum: return b == 0 ? (intptr_t)L->mServerList->GetCount() : L->mServerList->IsEmpty();
    case O::ListAt: return (intptr_t)L->mServerList->ServerAt((size_t)a);
    case O::ListAddressIs: return S(b)->GetAddress() == L->mServerList->AddressAt((size_t)a);
    case O::GroupNum: return b == 7 ? G(a)->GetName().length() : G(a)->GetName() == LobbyGetLastRoomName();
    case O::FindFriend: return (intptr_t)LobbyFindFriend((unsigned long)a);
    case O::AddFriendOfClient: return (intptr_t)LobbyAddFriend(C(a)->GetName(), C(a)->GetWONUserId());
    case O::AddFriendNamed: return (intptr_t)LobbyAddFriend(*T(a), (unsigned long)b);
    case O::RemoveFriend: LobbyRemoveFriend((unsigned long)a); return 0;
    case O::FriendCount: return (intptr_t)LobbyGetFriendCount();
    case O::FriendAt: return (intptr_t)LobbyGetFriendAt((size_t)a);
    case O::GameReadSummary: return GM(a)->ReadSummaryBuffer(RO(b)->GetData().get());
    case O::GameText: return (intptr_t) new GUIString(b == 0 ? GM(a)->GetName() : GM(a)->GetPassword());
    case O::GameCreate: return Held(SmartPtr<LobbyGame>(LobbyGame::CreateGame(LobbyGameType_Internet)));
    case O::GameList:
        switch (a) {
        case 1: L->mGameList->Clear(); return 0;
        case 2: L->mGameList->Rewind(); return 0;
        case 3: return L->mGameList->HasMore();
        case 4: return (intptr_t)L->mGameList->Next();
        case 5: return (intptr_t)L->mGameList->GetGame((unsigned short)b);
        case 6: L->mGameList->AddGame(GM(b)); return 0;
        case 7: return Held(L->mGameList->RemoveGame((unsigned short)b));
        case 8: return (intptr_t)L->mGameList->GetOrCreateGame((unsigned short)b, reinterpret_cast<bool*>(c));
        }
        return 0;
    // ---- the routing ops and connection
    case O::RNum: return (intptr_t)RO(a)->Num((int)b);
    case O::RSetNum: RO(a)->SetNum((int)b, (long long)c); return 0;
    case O::RSetText: RO(a)->SetText((int)b, TX(c)); return 0;
    case O::RIsGameData: return RO(a)->DataType() == L->mGameDataType;
    case O::RRecipient: {
        std::list<unsigned short> aRecipients = RO(a)->Recipients();
        return aRecipients.empty() ? -1 : (intptr_t)aRecipients.front();
    }
    case O::RConnection: return (intptr_t)RO(a)->mConnection.get();
    case O::RNew: { RoutingOp* anOp = new RoutingOp(RC(a), (RoutingOpType)b); anOp->AddRef(); return (intptr_t)anOp; }
    case O::RDataObjects: {
        RoutingDataObjectList aList;
        RO(a)->GetDataObjects(aList);
        RoutingDataObjectVector* aVector = new RoutingDataObjectVector(aList.begin(), aList.end());
        *reinterpret_cast<intptr_t*>(b) = (intptr_t)aVector->size();
        return (intptr_t)aVector;
    }
    case O::DataObjectLink: return (*reinterpret_cast<RoutingDataObjectVector*>(a))[(size_t)b]->mLinkId;
    case O::GameReadDataObject:
        return GM(a)->ReadSummaryBuffer((*reinterpret_cast<RoutingDataObjectVector*>(b))[(size_t)c]->mData.get());
    case O::OpSocket: return (intptr_t) static_cast<RefCount*>(static_cast<SocketOp*>(AO(a))->GetSocket());
    case O::ConnNew: { RoutingConnection* aConnection = new RoutingConnection; aConnection->AddRef(); return (intptr_t)aConnection; }
    case O::ConnDo:
        switch (b) {
        case 1: RC(a)->SetAuth(reinterpret_cast<AuthContext*>(c), AUTH_TYPE_PERSISTENT); return 0;
        case 2: {
            // the notices' op types and codes (routelogic.rs N_*), in the original's order
            static const int kTypes[20][2] = {{8, 1}, {10, 2}, {7, 3}, {9, 4}, {39, 5}, {40, 6}, {6, 7}, {36, 8},
                                              {24, 9}, {25, 10}, {23, 11}, {13, 12}, {14, 13}, {27, 14}, {28, 15},
                                              {58, 16}, {59, 17}, {48, 18}, {50, 19}, {49, 20}};
            for (int i = 0; i < 20; i++)
                RC(a)->SetOpCompletion(kTypes[i][0], new ::RoutingOpCompletion(kTypes[i][1], L));
            return 0;
        }
        case 3: RC(a)->SetCloseCompletion(new LogicCompletion<RoutingLogic>(&RoutingLogic::ConnectionClosed, L)); return 0;
        case 4: RC(a)->SubscribeDataObject(0xffff, L->mGameDataType, false, new ::RoutingOpCompletion(21, L)); return 0;
        case 5: return RC(a)->IsConnected();
        case 6: RC(a)->AbortServerShutdown(NULL); return 0;
        case 7: RC(a)->BecomeModerator(c != 0, NULL); return 0;
        case 8: RC(a)->LeaveGroup((unsigned short)c, NULL); return 0;
        case 9: RC(a)->CancelJoinGroup((unsigned short)c, true, NULL); return 0;
        case 10: RC(a)->ReplaceDataObject((unsigned short)c, L->mGameDataType, gEmptyWideString, NULL); return 0;
        }
        return 0;
    case O::ConnSendData: RC(a)->SendData(reinterpret_cast<const ByteBuffer*>(b), 0, (unsigned short)c, NULL); return 0;
    case O::ConnAdvertise: {
        WriteBuffer aBuffer(0, true);
        GM(b)->WriteSummary(aBuffer);
        RC(a)->CreateDataObject(true, (unsigned short)c, L->mGameDataType, gEmptyWideString,
                                aBuffer.ToByteBuffer(true).get(), NULL);
        return 0;
    }
    case O::ConnSetClientFlags:
        RC(a)->SetClientFlags((unsigned long)b, (unsigned long)c, (unsigned long)d, (unsigned long)e, NULL);
        return 0;
    case O::ConnSetGroupFlags:
        RC(a)->SetGroupFlags((unsigned short)b, (unsigned long)c, (unsigned long)d, 0, 0, NULL);
        return 0;
    case O::ConnBan:
        RC(a)->BanClient((unsigned short)b, (b >> 16) != 0, (unsigned long)c, TX(d), (unsigned short)e, NULL);
        return 0;
    case O::ConnMute:
        RC(a)->MuteClient((unsigned short)b, (b >> 16) != 0, (unsigned long)c, TX(d), (unsigned short)e, NULL);
        return 0;
    case O::ConnBanByWonId:
        RC(a)->BanClientByWONId((unsigned short)b, false, 0, std::wstring(gEmptyWideString), (unsigned long)c, NULL);
        return 0;
    case O::ConnMuteByWonId:
        RC(a)->MuteClientByWONId((unsigned short)b, false, 0, std::wstring(gEmptyWideString), (unsigned long)c, NULL);
        return 0;
    case O::ConnAccept: RC(a)->AcceptClient((unsigned short)b, (unsigned short)c, d != 0, gEmptyWideString, NULL); return 0;
    case O::ConnInvite: RC(a)->InviteClient((unsigned short)b, (unsigned short)c, d != 0, TX(e), NULL); return 0;
    case O::ConnSendChat: RC(a)->SendChat(TX(b), (unsigned long)c, (unsigned short)d, NULL); return 0;
    case O::ConnSendChatTo: {
        const uint16_t* anIds = reinterpret_cast<const uint16_t*>(e);
        std::list<unsigned short> aRecipients(anIds, anIds + d);
        RC(a)->SendChatTo(TX(b), (unsigned short)c, aRecipients, NULL);
        return 0;
    }
    case O::ConnAlert: RC(a)->SendServerAlert(TX(b), NULL); return 0;
    case O::ConnStartShutdown: RC(a)->StartServerShutdown(TX(b), (unsigned long)c, NULL); return 0;
    case O::ConnectOpNew: {
        ServerConnectionConnectOp* anOp = new ServerConnectionConnectOp(RC(a), S(b)->GetAddress());
        anOp->AddRef();
        return (intptr_t)anOp;
    }
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
    case O::EventNum: {
        ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
        switch (b) {
        case 1: return (intptr_t) static_cast<AcceptClientEvent*>(anEvent)->mClient.get();
        case 2: return static_cast<AcceptClientEvent*>(anEvent)->mAccepted;
        case 3: return (intptr_t) static_cast<FriendActionEvent*>(anEvent)->mFriend.get();
        case 4: return static_cast<FriendActionEvent*>(anEvent)->mAction;
        case 5: return static_cast<FriendActionEvent*>(anEvent)->mValue;
        case 6: return static_cast<JumpToRoomEvent*>(anEvent)->mGroupId;
        case 7: return static_cast<QuickPlayEvent*>(anEvent)->mGameType;
        case 8: return static_cast<ClientActionEvent*>(anEvent)->GetRoomSpec();
        case 9: return (intptr_t) static_cast<ClientActionEvent*>(anEvent)->GetClient();
        case 10: return static_cast<ClientActionEvent*>(anEvent)->GetAction();
        case 11: return (intptr_t) static_cast<JoinRoomEvent*>(anEvent)->mGroupId;
        case 12: return (intptr_t) static_cast<JoinFriendsRoomEvent*>(anEvent)->mFriend.get();
        case 13: return (intptr_t)anEvent->mComponent;
        }
        return 0;
    }
    case O::EventServer: {
        ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
        switch (anEvent->mType) {
        case LobbyEvent_CreateRoom: return (intptr_t)L->mServerList->FindServer(static_cast<CreateRoomEvent*>(anEvent)->mAddress);
        case LobbyEvent_JoinRoom: return (intptr_t)L->mServerList->FindServer(static_cast<JoinRoomEvent*>(anEvent)->mAddress);
        case LobbyEvent_QueryServerGroups:
            return (intptr_t)L->mServerList->FindServer(static_cast<QueryServerGroupsEvent*>(anEvent)->mAddress);
        }
        return 0;
    }
    case O::EventAddressIs: return S(b)->GetAddress() == reinterpret_cast<JumpToRoomEvent*>(a)->mAddress;
    case O::GameNameIsEvent: return GM(a)->GetName() == reinterpret_cast<JumpToRoomEvent*>(b)->mRoomName;
    case O::FriendAddressValid: return F(a)->GetAddress().IsValid();
    case O::FriendServer: { IPAddr anAddress = F(a)->GetAddress(); return (intptr_t)L->mServerList->FindServer(anAddress); }
    case O::DialogDo:
        switch (a) {
        case 1: return LobbyDoDialog(L->mAddFriendDialog.get());
        case 2: return LobbyDoDialog(L->mAdminActionDialog.get());
        case 3: return LobbyDoDialog(L->mBadUserDialog.get());
        case 4: return LobbyDoDialog(L->mCreateRoomDialog.get());
        case 5: return LobbyDoDialog(L->mPasswordDialog.get());
        case 6: return LobbyDoDialog(L->mRoomDialog.get());
        }
        return 0;
    case O::DialogReset:
        switch (a) {
        case 1: L->mAddFriendDialog->Reset(); break;
        case 2: L->mAdminActionDialog->Reset((int)b, C(c)); break;
        case 4: L->mCreateRoomDialog->Reset(b != 0); break;
        case 5: L->mPasswordDialog->Reset(); break;
        }
        return 0;
    case O::DialogText:
        switch (a) {
        case 1: return (intptr_t) new GUIString(L->mCreateRoomDialog->GetRoomName());
        case 2: return (intptr_t) new GUIString(L->mCreateRoomDialog->GetPassword());
        case 3: return (intptr_t) new GUIString(L->mPasswordDialog->GetPassword());
        case 4: return (intptr_t) new GUIString(L->mAdminActionDialog->GetExplanation());
        case 5: return (intptr_t) new GUIString(L->mAddFriendDialog->GetName());
        }
        return 0;
    case O::DialogNum:
        switch (a) {
        case 1: return L->mAdminActionDialog->GetApplyToServer();
        case 2: return (intptr_t)L->mAdminActionDialog->GetNumSeconds();
        case 3: return L->mRoomDialog->GetParent() != NULL;
        }
        return 0;
    case O::RoomDialogEnd: GetLobbyWindow()->EndDialog(-1, NULL, L->mRoomDialog.get()); return 0;
    case O::BadUserSet: {
        RoutingBadUserList aBadUsers;
        RO(a)->GetBadUsers(aBadUsers);
        L->mBadUserDialog->SetUserList(aBadUsers, b != 0);
        return 0;
    }
    case O::BadUserIds: {
        std::list<unsigned long> ids;
        L->mBadUserDialog->GetIds(ids);
        for (std::list<unsigned long>::iterator i = ids.begin(); i != ids.end(); ++i)
            ee_won_routelogic_push(reinterpret_cast<void*>(a), *i);
        return 0;
    }
    // ---- the events
    case O::EvClientChanged: Broadcast(new ClientChangedEvent(C(a), (unsigned int)b, (int)c)); return 0;
    case O::EvGroupChanged: Broadcast(new GroupChangedEvent(G(a), S(b), (int)c)); return 0;
    case O::EvFriendChanged: Broadcast(new FriendChangedEvent(F(a), (int)b)); return 0;
    case O::EvAnnounceFriend: Broadcast(new AnnounceFriendEvent(F(a), (int)b, (int)c)); return 0;
    case O::EvAnnounceClient:
        Broadcast(new AnnounceClientLocationEvent(C(a), b != 0 ? G(b)->GetName() : gEmptyGUIString,
                                                  c != 0 ? G(c)->GetName() : gEmptyGUIString, (int)d));
        return 0;
    case O::EvGameChanged: Broadcast(new GameChangedEvent(GM(a), (int)b)); return 0;
    case O::EvInvitationRecv:
        Broadcast(new GroupInvitationEvent(4, GM(a)->GetName(), GM(a)->GetGroupId(), C(b), c != 0,
                                           GUIString(new GUIStringData(RO(d)->Text(EE_WON_RTT_COMMENT))), false));
        return 0;
    case O::EvInvitationSent:
        Broadcast(new GroupInvitationEvent(4, G(a)->GetName(), G(a)->GetId(), C(b), c != 0, TX(d), true));
        return 0;
    case O::EvJoinAttempt:
        Broadcast(new GroupJoinAttemptEvent(6, C(a), GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_COMMENT)))));
        return 0;
    case O::EvRecvChat:
        Broadcast(new RecvChatEvent((unsigned int)a, (unsigned short)b, C(c), GUIString(RO(d)->Text(EE_WON_RTT_TEXT)), C(e)));
        return 0;
    case O::EvAlert:
        Broadcast(new AlertEvent(4, (int)a, a == 1 ? (unsigned long)RO(b)->Num(EE_WON_RTF_TIME) : 0,
                                 GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT)))));
        return 0;
    case O::EvModerated:
        Broadcast(new ClientModeratedEvent(C(a), (int)b, (d & 0x100) ? G(c)->GetName() : S(c)->GetName(), (int)(d & 0xff),
                                           (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true,
                                           GUIString(RO(e)->Text(EE_WON_RTT_COMMENT))));
        return 0;
    case O::EvLeftRoom: Broadcast(new LeftRoomEvent((int)a, b != 0, c != 0)); return 0;
    case O::EvSyncGameList: Broadcast(new SyncGameListEvent(L->mGameList.get())); return 0;
    case O::EvJoinedRoom:
        Broadcast(new JoinedRoomEvent(S(a)->GetName(), G(b)->GetName(), (unsigned int)c, S(a)->GetMyClientId(),
                                      G(b)->GetClients().get()));
        return 0;
    case O::EvServerChanged: Broadcast(new ServerChangedEvent(S(a), (int)b)); return 0;
    case O::EvSetChatInput: Broadcast(new SetChatInputEvent(2, C(a)->GetName(), (int)b, false)); return 0;
    case O::EvNotCaptain:
        LobbySendEventTo(new ChatCommandErrorEvent(LOBBY_STRING(RoutingLogic_NotCaptain_String)), reinterpret_cast<Component*>(a));
        return 0;
    // ---- the friends' searches, the server and room lists
    case O::UdpManager: return (intptr_t)LobbyGetUDPManager();
    case O::StatusOpNew: {
        RoutingGetUserStatusOp* anOp =
            new RoutingGetUserStatusOp(L->mServerList->AddressAt((size_t)a), reinterpret_cast<UDPManager*>(b));
        anOp->AddRef();
        return (intptr_t) static_cast<AsyncOp*>(anOp);
    }
    case O::StatusOpAddId: US(a)->AddWONUserId((unsigned long)b); return 0;
    case O::StatusOpSetMap: {
        RoutingGetUserStatusOp::FriendStatusMap aMap;
        const uint32_t* anIds = reinterpret_cast<const uint32_t*>(b);
        for (intptr_t i = 0; i < c; i++) aMap[anIds[i]] = 0;
        US(a)->SetFriendStatusMap(aMap);
        return 0;
    }
    case O::StatusOpRun:
        L->RunOp(AO(a), new LogicCompletion<RoutingLogic>(&RoutingLogic::GetUserStatusComplete, L), 10000);
        return 0;
    case O::StatusOpServer: return (intptr_t)L->mServerList->FindServer(US(a)->GetDestAddr());
    case O::StatusOpMap: {
        RoutingGetUserStatusOp::FriendStatusMap aMap = US(a)->GetFriendStatusMap();
        for (RoutingGetUserStatusOp::FriendStatusMap::iterator i = aMap.begin(); i != aMap.end(); ++i) {
            ee_won_routelogic_push(reinterpret_cast<void*>(b), i->first);
            ee_won_routelogic_push(reinterpret_cast<void*>(b), i->second);
        }
        return 0;
    }
    case O::AccountServers: return (intptr_t)LobbyGetAccountServers();
    case O::UserIdLookup: {
        // GetUserIdFromNameOp: a Rust op (ee_won reqops.rs) with the name, its id read back
        std::wstring aWide = *T(b);
        std::string aName;
        WideAppendUtf16LE(aName, aWide);
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_user_id(reinterpret_cast<const uint8_t*>(aName.data()), aName.size()),
            reinterpret_cast<ServerContext*>(a));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(AddFriendDialog_StatusTitle_String),
                             LOBBY_STRING(AddFriendDialog_StatusDetails_String), 1, 0);
        intptr_t aUserId = (intptr_t)(unsigned long)ee_won_hostop_num(op->GetRustOp(), EE_WON_REQOP_USER_ID, 0);
        *reinterpret_cast<int*>(c) = (int)op->GetStatus();
        return aUserId;
    }
    case O::LobbyServerList: return (intptr_t)LobbyGetServerList();
    case O::ServerListFind: return (intptr_t)reinterpret_cast<LobbyServerList*>(a)->FindServer(RS(b)->GetAddr());
    case O::SyncGroupsOfQuery: {
        // the op's replies read again (reqlast.rs) into a connection-less group list op (routeop.rs)
        SmartPtr<RoutingOp> groups = new RoutingOp(NULL, RoutingOp_GetGroupList);
        ee_won_routeop_read_group_list_of(groups->mRust, RS(b)->GetRustOp());
        RoutingGroupMap aGroups;
        groups->GetGroups(aGroups);
        S(a)->SyncGroups(aGroups, false);
        return 0;
    }
    case O::GroupQueryRun: {
        SmartPtr<RustServerOp> op = RustServerOp::NewGroupListReq(S(a)->GetAddress(), 0x29);
        L->RunTrackedOp(op.get(), new LogicCompletion<RoutingLogic>(&RoutingLogic::QueryServerGroupsDone, L), 0);
        return 0;
    }
    case O::RoomDirServers: return Held(SmartPtr<ServerContext>(LobbyGetRoomDirServers()));
    case O::DirOpRun: {
        static const char* const kTypes[] = {"_C", "_L"};
        SmartPtr<RustServerOp> op = RustServerOp::NewGetDir(reinterpret_cast<ServerContext*>(a),
                                                            GetLobbyConfig()->mProductDirectory, 0x04000622, kTypes, 2);
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_GettingRoomsTitle_String),
                             LOBBY_STRING(LobbyContainer_GettingRoomsDetails_String), 1, 0);
        return Held(op);
    }
    case O::DirEntities: {
        DirEntityList* aList = new DirEntityList(RS(a)->GetEntities(EE_WON_REQOP_DIR_LIST));
        *reinterpret_cast<intptr_t*>(b) = aList->empty();
        return (intptr_t)aList;
    }
    case O::DirNotifyFailed: reinterpret_cast<ServerContext*>(a)->NotifyFailed(RS(b)->GetCurAddr()); return 0;
    case O::DirUpdate: L->mServerList->UpdateFromDirectory(*reinterpret_cast<DirEntityList*>(a)); return 0;
    }
    return 0;
}

static intptr_t Call(RoutingLogic* inLogic, EeWonRouteLogicCall inCall, intptr_t a = 0, intptr_t b = 0)
{
    return ee_won_routelogic_call(inLogic->mRust, inCall, a, b);
}

// FUNCTION: WON 0x007e7200
// Looks up the add-friend, admin-action, bad-user, create-room, password and room dialogs from the lobby layout
void RoutingLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mAddFriendDialog, "AddFriendDialog", __FILE__, 125);
    SafeGetComponent(inConfig, this->mAdminActionDialog, "AdminActionDialog", __FILE__, 126);
    SafeGetComponent(inConfig, this->mBadUserDialog, "BadUserDialog", __FILE__, 127);
    SafeGetComponent(inConfig, this->mCreateRoomDialog, "CreateRoomDialog", __FILE__, 128);
    SafeGetComponent(inConfig, this->mPasswordDialog, "PasswordDialog", __FILE__, 129);
    SafeGetComponent(inConfig, this->mRoomDialog, "RoomDialog", __FILE__, 130);
}

// FUNCTION: WON 0x007e6e80
// Creates the Internet lobby logic with an empty server list and game list (the Rust object: not idle, not yet a
// moderator, no server)
RoutingLogic::RoutingLogic() : mRust(ee_won_routelogic_new_model(this, RoutingHost))
{
    this->mServerList = new LobbyServerList;
    this->mGameList = new LobbyGameList(2);
    this->mGameDataType = "G";
}

// FUNCTION: WON 0x007e7070
// Destroys the Internet lobby logic (the Rust object gives back the servers and rooms it holds)
RoutingLogic::~RoutingLogic() { ee_won_routelogic_free(this->mRust); }

bool RoutingLogic::HostGame(LobbyGame* inGame) { return Call(this, EeWonRouteLogicCall::HostGame, (intptr_t)inGame) != 0; }
bool RoutingLogic::JoinGame(LobbyGame* inGame) { return Call(this, EeWonRouteLogicCall::JoinGame, (intptr_t)inGame) != 0; }
LobbyClientList* RoutingLogic::GetClientList()
{
    return reinterpret_cast<LobbyClientList*>(Call(this, EeWonRouteLogicCall::ClientList));
}
LobbyServer* RoutingLogic::GetCurrentServer() { return S(Call(this, EeWonRouteLogicCall::Field, 0)); }
LobbyGroup* RoutingLogic::GetChatGroup() { return G(Call(this, EeWonRouteLogicCall::Field, 1)); }
LobbyClient* RoutingLogic::GetMyClient() { return C(Call(this, EeWonRouteLogicCall::MyClient)); }
void RoutingLogic::SendToCaptain(const ByteBuffer* inData) { Call(this, EeWonRouteLogicCall::SendToCaptain, (intptr_t)inData); }
void RoutingLogic::BroadcastToGame(const ByteBuffer* inData) { Call(this, EeWonRouteLogicCall::BroadcastToGame, (intptr_t)inData); }
void RoutingLogic::SendToClient(LobbyClient* inClient, const ByteBuffer* inData)
{
    Call(this, EeWonRouteLogicCall::SendToClient, (intptr_t)inClient, (intptr_t)inData);
}
void RoutingLogic::AdvertiseGame(LobbyGame* inGame) { Call(this, EeWonRouteLogicCall::Advertise, (intptr_t)inGame); }
void RoutingLogic::RemoveGameAdvertisement(LobbyGame*) { Call(this, EeWonRouteLogicCall::RemoveAdvertisement); }
void RoutingLogic::KickClient(LobbyClient* inClient, bool inBan) { Call(this, EeWonRouteLogicCall::KickClient, (intptr_t)inClient, inBan); }
void RoutingLogic::SetGameInProgress(bool inClose) { Call(this, EeWonRouteLogicCall::SetGameInProgress, inClose); }
void RoutingLogic::Reset() { Call(this, EeWonRouteLogicCall::Reset); }
void RoutingLogic::LeaveGame(LobbyGame*) { Call(this, EeWonRouteLogicCall::LeaveGame); }
void RoutingLogic::Suspend() { Call(this, EeWonRouteLogicCall::Suspend); }
void RoutingLogic::Resume() { Call(this, EeWonRouteLogicCall::Resume); }
void RoutingLogic::EnterInternetScreen(bool inJoinDefaultRoom) { Call(this, EeWonRouteLogicCall::EnterInternet, inJoinDefaultRoom); }
void RoutingLogic::GetUserStatusComplete(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::UserStatusDone, (intptr_t)inOp); }
void RoutingLogic::QueryServerGroupsDone(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::GroupsDone, (intptr_t)inOp); }
void RoutingLogic::ConnectionClosed(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::ConnectionClosed, (intptr_t)inOp); }

// FUNCTION: WON 0x007ea310
// Handles the Internet lobby buttons: change room, add friend, anonymous and filter checks, refresh lists
void RoutingLogic::HandleControlEvent(int controlId)
{
    int aWhich = controlId == ID_ChangeRoom ? 1 : controlId == ID_AddFriend ? 2 : controlId == ID_AnonymousToFriendsCheck ? 3
               : controlId == ID_FilterDirtyWordsCheck ? 4 : controlId == ID_RefreshFriends ? 5
               : controlId == ID_RefreshGameList ? 6 : 0;
    if (aWhich != 0) Call(this, EeWonRouteLogicCall::Control, aWhich);
}

// FUNCTION: WON 0x007ea3a0
// Dispatches the Internet lobby's component events (accept, actions, rooms, chat, friends, Quick Play)
void RoutingLogic::HandleComponentEvent(ComponentEvent* event)
{
    Call(this, EeWonRouteLogicCall::Event, (intptr_t)event, event->mType);
}

// FUNCTION: WON 0x007eeed0
// Logs out of the Internet lobby: stops auth refresh, resets the room dialog and leaves the server
void RoutingLogic::Logout() { Call(this, EeWonRouteLogicCall::Logout); }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e7050
// (public: virtual void * __thiscall WONAPI::RoutingLogic::`scalar deleting destructor'(unsigned int)    [??_GRoutingLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e79f0
// (public: class WONAPI::SmartPtr<class WONAPI::ByteBuffer> __thiscall WONAPI::RoutingPeerDataOp::GetData(void)const     [?GetData@RoutingPeerDataOp@WONA)
// TEMPLATE: WON 0x007eae90
// (public: __thiscall WONAPI::PendingServerCleanup::~PendingServerCleanup(void)    [??1PendingServerCleanup@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007ee8a0
// (public: virtual void __thiscall RoutingOpCompletion::Run(class WONAPI::AsyncOp *)    [?Run@RoutingOpCompletion@@UAEXPAVAsyncOp@WONAPI@@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1330
// (public: __thiscall std::list<struct WONAPI::RoutingBadUser,class std::allocator<struct WONAPI::RoutingBadUser> >::list<struct WONAPI::RoutingBadUser,c)
// TEMPLATE: WON 0x007f1bb0
// (public: struct std::pair<class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::map<unsigned long,unsigned ch)
// TEMPLATE: WON 0x007f30b0
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::map<unsigned long,unsigned char,struct std)
// TEMPLATE: WON 0x007f3e50
// TEMPLATE: WON 0x007f3fd0
// TEMPLATE: WON 0x007f4150
// TEMPLATE: WON 0x007f42d0
// TEMPLATE: WON 0x007f4450

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e73e0
// (public: virtual void * __thiscall WONAPI::LeftRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GLeftRoomEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e7400
// (public: virtual __thiscall WONAPI::LeftRoomEvent::~LeftRoomEvent(void)    [??1LeftRoomEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ee8d0
// (public: virtual void * __thiscall RoutingOpCompletion::`scalar deleting destructor'(unsigned int)    [??_GRoutingOpCompletion@@UAEPAXI@Z])
// TEMPLATE: WON 0x007ee8f0
// (public: virtual __thiscall RoutingOpCompletion::~RoutingOpCompletion(void)    [??1RoutingOpCompletion@@UAE@XZ])
// TEMPLATE: WON 0x007f1490
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::RoutingLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicCompl)
// TEMPLATE: WON 0x007f1630
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::RoutingLogic>::~LogicCompletion<class WONAPI::RoutingLogic>(void)    [??1?$LogicCompl)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ece80
// (public: __thiscall std::multimap<unsigned short,class WONAPI::LobbyGame *,struct std::less<unsigned short>,class std::allocator<struct std::pair<unsig)
// TEMPLATE: WON 0x007f1360
// (public: __thiscall std::map<unsigned long,unsigned char,struct std::less<unsigned long>,class std::allocator<struct std::pair<unsigned long const ,uns)
// TEMPLATE: WON 0x007f1ca0
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char,struct std::)
// TEMPLATE: WON 0x007f1d50
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char)
// TEMPLATE: WON 0x007f1e20
// (std::_Tree::insert)
// TEMPLATE: WON 0x007f1ea0
// (std::_Tree::erase)
// TEMPLATE: WON 0x007f27f0
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char,struct s)
// TEMPLATE: WON 0x007f2880
// (public: class std::list<unsigned long,class std::allocator<unsigned long> >::iterator __thiscall std::list<unsigned long,class std::allocator<unsigned)
// TEMPLATE: WON 0x007f28c0
// (std::_Tree::erase)
// TEMPLATE: WON 0x007f2d70
// (std::_Tree::_Erase)
// TEMPLATE: WON 0x007f2db0
// (std::_Tree::_Insert)
// TEMPLATE: WON 0x006d9f60
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)
// TEMPLATE: WON 0x00769620
// (protected: struct std::list<unsigned long,class std::allocator<unsigned long> >::_Node * __thiscall std::list<unsigned long,class std::allocator<unsig)
// TEMPLATE: WON 0x006d99b0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)
