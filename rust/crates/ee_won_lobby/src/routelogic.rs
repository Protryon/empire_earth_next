//! RoutingLogic, the Internet lobby's logic over WON's routing servers, as one Rust object (R6 WONLobby round 9):
//! the servers it is on and connecting to, the joined chat and game rooms, the idle and moderator state, the
//! server list's refresh time, and every method's decisions (the rooms' and players' notices, Quick Play, the
//! friends' searches, the chat commands, the moderation, the connect and join sequences).
//!
//! The C++ keeps `RoutingLogic` as the shell callers use (`NetLogic`'s vtable, `LobbyLogic`'s tracker and
//! completions, the server list and game list it owns, the six dialogs' `SmartPtr`s, the game data type) and
//! forwards every method here. Every call on a C++ object (the model's servers, rooms, players, friends and games,
//! the routing connection and its ops, the dialogs, the events, the `Lobby*` helpers) is one op of the host in
//! `RoutingLogic.cpp`, the original's statements with no decision between them. The decisions over values
//! (room flags, Quick Play's rank, the friend search rule, the default server, the client flags) are `routing.rs`'s.
//!
//! Objects cross as pointers (`isize`). The servers, rooms, connection and wait op the logic holds are counted
//! references (`AddRef` / `Release`, `SmartPtr`'s order: the old one released before the new one is taken); an op
//! documented "held" returns one Rust gives back. Texts are `GUIString*`s the host makes and `TextFree` frees;
//! the text 0 is the empty string.

use std::cell::Cell;
use std::ffi::c_void;

use crate::resource::EeWonLobbyRes as Res;
use crate::routing;
use ee_won_net::routecpp::Value;
use ee_won_net::routeop::{
    EE_WON_RTF_BY_WON_ID, EE_WON_RTF_CAPTAIN_ID, EE_WON_RTF_CLIENT_FLAGS, EE_WON_RTF_CLIENT_ID, EE_WON_RTF_FLAGS16,
    EE_WON_RTF_FLAGS8, EE_WON_RTF_GROUP_FLAGS, EE_WON_RTF_GROUP_ID, EE_WON_RTF_IS_CDKEY, EE_WON_RTF_JOIN_FLAGS,
    EE_WON_RTF_LEAVE_ON_CANCEL, EE_WON_RTF_LINK_ID, EE_WON_RTF_LIST_TYPE, EE_WON_RTF_MEMBER_FLAGS, EE_WON_RTF_ON,
    EE_WON_RTF_RECONNECT_ID, EE_WON_RTF_REGISTER_FLAGS, EE_WON_RTF_TIME, EE_WON_RTF_WON_ID, EE_WON_RTT_COMMENT,
    EE_WON_RTT_NAME, EE_WON_RTT_PASSWORD,
};

/// The host's ops. `a`..`e` are pointers or integers as each op says; `L` is the shell (`RoutingLogic*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRouteLogicOp {
    // ---- references, texts, lists (the golden counts AddRef, Release, TextFree and ListFree instead of tracing them)
    /// `((RefCount*)a)->AddRef()`.
    AddRef = 1,
    /// `((RefCount*)a)->Release()`.
    Release,
    /// `delete (GUIString*)a`.
    TextFree,
    /// `((GUIString*)a)->length()`.
    TextLength,
    /// a list `a` the host made (`kind` b: 1 data objects, 2 directory entities, 3 a client iterator) deleted.
    ListFree,
    /// text `a`'s (`GUIString*`, 0 the empty text) `wchar_t` unit count; its units written to `b` (`uint32_t`s)
    /// when not 0 (WONLobby round 14: the requests [`conn_request`] makes read their texts with it).
    TextUnits,

    // ---- the lobby
    /// `L->mKilled`.
    Killed = 10,
    /// `(int)((AsyncOp*)a)->GetStatus()`.
    OpStatus,
    /// `((AsyncOp*)a)->IsKilled()`.
    OpKilled,
    /// `((AsyncOp*)a)->Kill(true)`.
    OpKill,
    /// `((AsyncOp*)a)->Finish(WS_Success)`.
    OpFinish,
    /// `new AsyncOp`, held.
    AsyncOpNew,
    /// `L->RunOp((AsyncOp*)a, NULL, 0)`.
    RunOp,
    /// `LobbyRunOpWithStatus((AsyncOp*)a, LOBBY_STRING(b), LOBBY_STRING(c), d & 0xff, d >> 8)`.
    RunWithStatus,
    /// `time(NULL)`.
    Now,
    /// `WONTickCount()`.
    Tick,
    /// `GetLobbyWindow()->GetLastInputTick()`.
    LastInputTick,
    /// `LobbyBroadcastLobbyEvent(a, true)`.
    BroadcastLobby,
    /// a setting: 1 chat sounds, 2 staging sounds, 3 lobby sounds, 4 auto-ping games, 5 lobby music, 6 anonymous
    /// to friends, 7 filter dirty words, 8 on the Internet screen, 9 the user cancelled the last status op.
    Pref,
    /// `LobbyPlaySound(LOBBY_SOUND(a).get())`.
    Sound,
    /// `LobbyMessageBox(LOBBY_STRING(a), LOBBY_STRING(b))`.
    MessageBox,
    /// `LobbyDoErrorDialog(LOBBY_STRING(a), (WONStatus)b)`.
    ErrorDialog,
    /// `LobbyDoYesNoDialog(LOBBY_STRING(a), LOBBY_STRING(b), true)`.
    YesNo,
    /// `GetLobbyConfig() != NULL`.
    HasConfig,
    /// `GetLobbyConfig()->mPingCeiling`, or -1 without a config.
    ConfigPingCeiling,
    /// `GetLobbyAuthContext()`.
    AuthContext,
    /// `((AuthContext*)a)->SetDoAutoRefresh(false)`.
    AuthStopRefresh,
    /// `GetMyLobbyGame()`.
    MyGame,
    /// `GetMyLobbyClient()`.
    MyClient,
    /// `LobbyGetChatCommandParser()`.
    Parser,
    /// a lobby call: 1 `LobbyKillPings(NULL)`, 2 `LobbyGameDisconnected(b != 0)`, 3 `LobbyLeaveGame(b != 0)`
    /// (its result), 4 `LobbyShowLobbyTab()`, 5 `LobbyHostGame()`, 6 `LobbyGetNumPingsToDo()`, 7 `LobbyLogout(false)`,
    /// 8 `LobbyShowInternetScreen()`.
    Lobby,
    /// `LobbyHandleStagingMessage(((RoutingOp*)a)->GetData().get(), (LobbyClient*)b)`.
    HandleStaging,
    /// `LobbyClientLeftGame((LobbyClient*)a)`.
    ClientLeftGame,
    /// `LobbyClientKicked((LobbyClient*)a, b != 0)`.
    ClientKicked,
    /// `LobbyKickClient((LobbyClient*)a, b != 0)`.
    KickClient,
    /// `LobbyPingGame((LobbyGame*)a, false)`.
    PingGame,
    /// `LobbyJoinGame((LobbyGame*)a)`.
    JoinGame,
    /// `LobbySetClientBlocked((LobbyClient*)a, b != 0, 4)`.
    SetBlocked,
    /// `LobbySetClientIgnored((LobbyClient*)a, b != 0, 4)`.
    SetIgnored,
    /// `LobbySetLastRoom(((LobbyServer*)a)->GetName(), ((LobbyGroup*)b)->GetName())`.
    SetLastRoom,
    /// `LobbyGetReconnectId(((LobbyServer*)a)->GetAddress())`.
    ReconnectId,
    /// `LobbySetReconnectId(((LobbyServer*)a)->GetAddress(), b)`.
    SetReconnectId,
    /// `new ClientOptionsPopup` held by a `SmartPtr`, `Init(a, b, c)`, `LobbyOpenPopup(it, PopupFlag_StandardPopup,
    /// -1, -1)`.
    ClientOptions,
    /// `new FriendOptionsPopup`, `Init(a, b)`, opened as `ClientOptions`.
    FriendOptions,

    // ---- the model
    /// a server's (`a`) value: 0 `GetClients().get()`, 1 `GetConnection().get()`, 2 `GetMyClient()`, 3
    /// `GetMyClientId()`, 4 `GetName() == LobbyGetLastServerName()`, 5 `GetNumClients()`, 6
    /// `HasAddress(GetLobbyConfig()->mLanguage)`, 7 `IsConnected()`, 8 `GetGroupCount()`, 9
    /// `GetQueryingGroups()`, 10 `GetLastPing()`.
    ServerNum = 50,
    /// `((LobbyServer*)a)->FindClientByWONId(b)`.
    ServerFindByWonId,
    /// `((LobbyServer*)a)->GetGroup(b)`.
    ServerGroup,
    /// `((LobbyServer*)a)->GetClient(b)`.
    ServerClient,
    /// `((LobbyServer*)a)->GroupAt(b)`.
    ServerGroupAt,
    /// `((LobbyServer*)a)->AddClientToGroup((LobbyClient*)b, (unsigned char)c, (LobbyGroup*)d)`.
    ServerAddClientToGroup,
    /// `((LobbyServer*)a)->AddGroup(((RoutingOp*)b)->GetGroupInfo().get())`.
    ServerAddGroup,
    /// `((LobbyServer*)a)->AddClient(((RoutingOp*)b)->GetClientInfo().get())`.
    ServerAddClient,
    /// `((LobbyServer*)a)->RemoveGroup(b)`, held.
    ServerRemoveGroup,
    /// `((LobbyServer*)a)->RemoveClient(b)`, held.
    ServerRemoveClient,
    /// `((LobbyServer*)a)->RemoveClientFromGroup(b, (LobbyGroup*)c)`, held.
    ServerRemoveClientFromGroup,
    /// a server's (`a`) call: 1 `Disconnect()`, 2 `GetClients()->ClearInvited()`, 3 `UpdateFriends()`, 4
    /// `ClearGroups()`, 5 `SetQueryingGroups(c != 0)`, 6 `SetLastPing(c)`, 7 `SetMyClientId(c)`, 8
    /// `SetConnection((RoutingConnection*)c)`.
    ServerDo,
    /// `RoutingClientMap m; ((RoutingOp*)b)->GetClients(m); ((LobbyServer*)a)->SyncClients(m)`.
    ServerSyncClients,
    /// `RoutingGroupMap m; ((RoutingOp*)b)->GetGroups(m); ((LobbyServer*)a)->SyncGroups(m, c != 0)`.
    ServerSyncGroups,
    /// `((LobbyFriend*)a)->GetAddress() == ((LobbyServer*)b)->GetAddress()`.
    FriendAtServer,
    /// `L->mServerList`'s: 0 `GetCount()`, 1 `IsEmpty()`.
    ListNum,
    /// `L->mServerList->ServerAt(a)`.
    ListAt,
    /// `L->mServerList->AddressAt(a) == ((LobbyServer*)b)->GetAddress()`.
    ListAddressIs,
    /// a group's (`a`) value: 0 `GetId()`, 1 `IsGameGroup()`, 2 `IsPublicChatGroup()`, 3 `GetNumClients()`, 4
    /// `IsPasswordProtected()`, 5 `IsAskToJoin()`, 6 `GetCaptainId()`, 7 `GetName().length()`, 8
    /// `GetName() == LobbyGetLastRoomName()`, 9 `GetClients().get()`.
    GroupNum,
    /// `((LobbyGroup*)a)->FindClient(b)`.
    GroupFindClient,
    /// `((LobbyGroup*)a)->SetCaptain(b, (LobbyClient**)c)`.
    GroupSetCaptain,
    /// a client's (`a`) value: 0 `GetClientId()`, 1 `GetWONUserId()`, 2 `IsFriend()`, 3 `IsAnonymous()`, 4
    /// `IsIgnored()`, 5 `IsBlocked()`, 6 `IsInChatGroup()`, 7 `GetChatGroupId()`, 8 `IsInGameGroup()`, 9
    /// `GetGameGroupId()`, 10 `IsAway()`, 11 `IsModerator()`, 12 `IsAdmin()`, 13 `GetCDKeyId()`.
    ClientNum,
    /// a client's (`a`) call: 1 `SetMemberFlags((unsigned char)c, d != 0)`, 2 `SetClientFlags(c)`, 3
    /// `SetIsFriend(c != 0)`, 4 `SetInvited(c != 0)`.
    ClientDo,
    /// `((LobbyClient*)a)->IsTeammate((LobbyClient*)b)`.
    ClientIsTeammate,
    /// a friend's (`a`) value: 0 `GetFlags()`, 1 `GetLastLocate()`, 2 `GetUserId()`, 3 `GetAnonymous()`, 4
    /// `GetLastSearch()`, 5 `GetSearchStart()`.
    FriendNum,
    /// a friend's (`a`) call: 1 `SetAnonymous(c != 0)`, 2 `SetStatusFromClient((LobbyClient*)c)`, 3
    /// `SetLocation(server c's address, its name, d)`, 4 `ClearLocation()`, 5 `StartLocate(c)`, 6
    /// `SetLastSearch(c)`, 7 `EndLocate(c)`.
    FriendDo,
    /// `LobbyFindFriend(a)`.
    FindFriend,
    /// `LobbyAddFriend(((LobbyClient*)a)->GetName(), ((LobbyClient*)a)->GetWONUserId())`.
    AddFriendOfClient,
    /// `LobbyAddFriend(*(GUIString*)a, b)`.
    AddFriendNamed,
    /// `LobbyRemoveFriend(a)`.
    RemoveFriend,
    /// `LobbyGetFriendCount()`.
    FriendCount,
    /// `LobbyGetFriendAt(a)`.
    FriendAt,
    /// a game's (`a`) value: 0 `GetAskToJoin()`, 1 `GetInviteOnly()`, 2 `GetGameType()`, 3 `GetGroupId()`, 4
    /// `GetIsHost()`, 5 `GetIsFiltered()`, 6 `GetHasPassword()`, 7 `CanJoin()`, 8 `GetPing()`, 9 `GetSkill()`.
    GameNum,
    /// a game's (`a`) call: 1 `SetGroupId(c)`, 2 `SetClients((LobbyClientList*)c)`, 3 `SetInvited(c != 0)`, 4
    /// `ReadGroup((LobbyGroup*)c)`.
    GameDo,
    /// `((LobbyGame*)a)->ReadSummaryBuffer(((RoutingOp*)b)->GetData().get())`.
    GameReadSummary,
    /// `new GUIString(((LobbyGame*)a)->GetName())` (b 0) or its `GetPassword()` (b 1).
    GameText,
    /// `LobbyGame::CreateGame(LobbyGameType_Internet)`, held.
    GameCreate,
    /// `L->mGameList`'s call: 1 `Clear()`, 2 `Rewind()`, 3 `HasMore()`, 4 `Next()`, 5 `GetGame(b)`, 6
    /// `AddGame((LobbyGame*)b)`, 7 `RemoveGame(b)` (held), 8 `GetOrCreateGame(b, (bool*)c)`.
    GameList,

    // ---- the routing ops and connection
    /// `((RoutingOp*)a)->Num(b)`.
    RNum = 100,
    /// `((RoutingOp*)a)->SetNum(b, c)`.
    RSetNum,
    /// `((RoutingOp*)a)->SetText(b, text c)`.
    RSetText,
    /// `((RoutingOp*)a)->DataType() == L->mGameDataType`.
    RIsGameData,
    /// `((RoutingOp*)a)->Recipients()`'s front, or -1 when empty.
    RRecipient,
    /// `((RoutingOp*)a)->mConnection.get()`.
    RConnection,
    /// `new RoutingOp((RoutingConnection*)a, (RoutingOpType)b)`, held.
    RNew,
    /// `((RoutingOp*)a)->GetDataObjects(list)` into a new list (held: `ListFree` kind 1); its size.
    RDataObjects,
    /// the data object `b` of list `a`: its `mLinkId`.
    DataObjectLink,
    /// `((LobbyGame*)a)->ReadSummaryBuffer(list b's object c's mData.get())`.
    GameReadDataObject,
    /// `(intptr_t)static_cast<RefCount*>(((SocketOp*)a)->GetSocket())`.
    OpSocket,
    /// `new RoutingConnection`, held.
    ConnNew,
    /// a connection's (`a`) call: 1 `SetAuth((AuthContext*)b, AUTH_TYPE_PERSISTENT)`, 2 the twenty notification
    /// completions (`SetOpCompletion(type, new RoutingOpCompletion(code, L))`), 3 `SetCloseCompletion(new
    /// LogicCompletion<RoutingLogic>(&RoutingLogic::ConnectionClosed, L))`, 4 `SubscribeDataObject(0xffff,
    /// L->mGameDataType, false, new RoutingOpCompletion(21, L))`, 5 `IsConnected()`, 6 `AbortServerShutdown(NULL)`,
    /// 7 `BecomeModerator(b != 0, NULL)`, 8 `LeaveGroup(b, NULL)`, 9 `CancelJoinGroup(b, true, NULL)`, 10
    /// `ReplaceDataObject(b, L->mGameDataType, gEmptyWideString, NULL)`.
    ConnDo,
    /// `((RoutingConnection*)a)->SendData((const ByteBuffer*)b, 0, c, NULL)`.
    ConnSendData,
    /// `WriteBuffer w(0, true); ((LobbyGame*)b)->WriteSummary(w); ((RoutingConnection*)a)->CreateDataObject(true,
    /// c, L->mGameDataType, gEmptyWideString, w.ToByteBuffer(true).get(), NULL)`.
    ConnAdvertise,
    /// `((RoutingConnection*)a)->SetClientFlags(b, c, d, e, NULL)`.
    ConnSetClientFlags,
    /// `((RoutingConnection*)a)->SetGroupFlags(b, c, d, 0, 0, NULL)`.
    ConnSetGroupFlags,
    /// `((RoutingConnection*)a)->BanClient(b & 0xffff, b >> 16 != 0, c, text d, e, NULL)`.
    ConnBan,
    /// `((RoutingConnection*)a)->MuteClient(b & 0xffff, b >> 16 != 0, c, text d, e, NULL)`.
    ConnMute,
    /// `((RoutingConnection*)a)->BanClientByWONId(b, false, 0, std::wstring(gEmptyWideString), c, NULL)`.
    ConnBanByWonId,
    /// `((RoutingConnection*)a)->MuteClientByWONId(b, false, 0, std::wstring(gEmptyWideString), c, NULL)`.
    ConnMuteByWonId,
    /// `((RoutingConnection*)a)->AcceptClient(b, c, d != 0, gEmptyWideString, NULL)`.
    ConnAccept,
    /// `((RoutingConnection*)a)->InviteClient(b, c, d != 0, text e, NULL)`.
    ConnInvite,
    /// `((RoutingConnection*)a)->SendChat(text b, c, d, NULL)`.
    ConnSendChat,
    /// `((RoutingConnection*)a)->SendChatTo(text b, c, the d ids at e (uint16_t), NULL)`.
    ConnSendChatTo,
    /// `((RoutingConnection*)a)->SendServerAlert(text b, NULL)`.
    ConnAlert,
    /// `((RoutingConnection*)a)->StartServerShutdown(text b, c, NULL)`.
    ConnStartShutdown,
    /// `new ServerConnectionConnectOp((RoutingConnection*)a, ((LobbyServer*)b)->GetAddress())`, held.
    ConnectOpNew,

    // ---- the parser, the events, the dialogs
    /// the parser's (`a`) value: 0 `GetRoomFlags()`, 1 `GetCommand()`, 2 `GetDuration()`, 3 `GetTarget()`, 4
    /// `GetClients()`.
    ParserNum = 140,
    /// `new GUIString(((ChatCommandParser*)a)->GetText())`.
    ParserText,
    /// `((ChatCommandParser*)a)->SetInput(((LobbyClient*)b)->GetName())`.
    ParserSetInput,
    /// `new LobbyClientIterator((LobbyClientList*)a)` (held: `ListFree` kind 3).
    IterNew,
    /// the iterator's `HasMore()`.
    IterHasMore,
    /// the iterator's `Next()`.
    IterNext,
    /// an event's (`a`) member: 1 the accept's `mClient.get()`, 2 its `mAccepted`, 3 the friend action's
    /// `mFriend.get()`, 4 its `mAction`, 5 its `mValue`, 6 the jump's `mGroupId`, 7 Quick Play's `mGameType`, 8 the
    /// client action's `GetRoomSpec()`, 9 its `GetClient()`, 10 its `GetAction()`, 11 the join room's `mGroupId`, 12
    /// the join friend's room `mFriend.get()`, 13 `mComponent`.
    EventNum,
    /// `L->GetServer(event a's mAddress)` (a create room, join room or query server groups event).
    EventServer,
    /// `((LobbyServer*)b)->GetAddress() == ((JumpToRoomEvent*)a)->mAddress`.
    EventAddressIs,
    /// `((LobbyGame*)a)->GetName() == ((JumpToRoomEvent*)b)->mRoomName`.
    GameNameIsEvent,
    /// the friend's (`a`) `GetAddress().IsValid()`.
    FriendAddressValid,
    /// `L->GetServer(((LobbyFriend*)a)->GetAddress())`.
    FriendServer,
    /// `LobbyDoDialog(dialog a)` (1 add friend, 2 admin action, 3 bad user, 4 create room, 5 password, 6 room).
    DialogDo,
    /// dialog a's `Reset`: create room `Reset(b != 0)`, password `Reset()`, admin action `Reset(b, (LobbyClient*)c)`,
    /// add friend `Reset()`.
    DialogReset,
    /// a dialog's text, new: 1 the create room's `GetRoomName()`, 2 its `GetPassword()`, 3 the password's
    /// `GetPassword()`, 4 the admin action's `GetExplanation()`, 5 the add friend's `GetName()`.
    DialogText,
    /// a dialog's value: 1 the admin action's `GetApplyToServer()`, 2 its `GetNumSeconds()`, 3 the room dialog's
    /// `GetParent() != NULL`.
    DialogNum,
    /// `GetLobbyWindow()->EndDialog(-1, NULL, L->mRoomDialog.get())`.
    RoomDialogEnd,
    /// `RoutingBadUserList l; ((RoutingOp*)a)->GetBadUsers(l); L->mBadUserDialog->SetUserList(l, b != 0)`.
    BadUserSet,
    /// `L->mBadUserDialog->GetIds(ids)`, each pushed to sink `a` by `ee_won_routelogic_push`.
    BadUserIds,

    // ---- the events broadcast (`LobbyBroadcastEvent(new ..., true)`)
    /// `ClientChangedEvent((LobbyClient*)a, b, c)`.
    EvClientChanged = 170,
    /// `GroupChangedEvent((LobbyGroup*)a, (LobbyServer*)b, c)`.
    EvGroupChanged,
    /// `FriendChangedEvent((LobbyFriend*)a, b)`.
    EvFriendChanged,
    /// `AnnounceFriendEvent((LobbyFriend*)a, b, c)`.
    EvAnnounceFriend,
    /// `AnnounceClientLocationEvent((LobbyClient*)a, chat group b's name, game group c's name (empty for NULL), d)`.
    EvAnnounceClient,
    /// `GameChangedEvent((LobbyGame*)a, b)`.
    EvGameChanged,
    /// `GroupInvitationEvent(4, game a's name, its group id, (LobbyClient*)b, c != 0, op d's comment, false)`.
    EvInvitationRecv,
    /// `GroupInvitationEvent(4, group a's name, its id, (LobbyClient*)b, c != 0, text d, true)`.
    EvInvitationSent,
    /// `GroupJoinAttemptEvent(6, (LobbyClient*)a, op b's comment)`.
    EvJoinAttempt,
    /// `RecvChatEvent(a, b, (LobbyClient*)c, op d's text, (LobbyClient*)e)`.
    EvRecvChat,
    /// `AlertEvent(4, a, a == 1 ? op b's time : 0, op b's text)`.
    EvAlert,
    /// `ClientModeratedEvent((LobbyClient*)a, b, the name of c (a group when d & 0x100, else a server), d & 0xff,
    /// op e's time, true, op e's comment)`.
    EvModerated,
    /// `LeftRoomEvent(a, b != 0, c != 0)`.
    EvLeftRoom,
    /// `SyncGameListEvent(L->mGameList.get())`.
    EvSyncGameList,
    /// `JoinedRoomEvent(server a's name, group b's name, c, server a's my client id, group b's clients)`.
    EvJoinedRoom,
    /// `ServerChangedEvent((LobbyServer*)a, b)`.
    EvServerChanged,
    /// `SetChatInputEvent(2, client a's name, b, false)`.
    EvSetChatInput,
    /// `LobbySendEventTo(new ChatCommandErrorEvent(LOBBY_STRING(RoutingLogic_NotCaptain_String)), (Component*)a)`.
    EvNotCaptain,

    // ---- the friends' searches, the server and room lists
    /// `LobbyGetUDPManager()`.
    UdpManager = 200,
    /// `new RoutingGetUserStatusOp(L->mServerList->AddressAt(a), (UDPManager*)b)`, held.
    StatusOpNew,
    /// `((RoutingGetUserStatusOp*)a)->AddWONUserId(b)`.
    StatusOpAddId,
    /// `((RoutingGetUserStatusOp*)a)->SetFriendStatusMap(the c ids at b, each 0)`.
    StatusOpSetMap,
    /// `L->RunOp((AsyncOp*)a, new LogicCompletion<RoutingLogic>(&RoutingLogic::GetUserStatusComplete, L), 10000)`.
    StatusOpRun,
    /// `L->mServerList->FindServer(((RoutingGetUserStatusOp*)a)->GetDestAddr())`.
    StatusOpServer,
    /// `((RoutingGetUserStatusOp*)a)->GetFriendStatusMap()`, each pair pushed to sink `b` (id, status).
    StatusOpMap,
    /// `LobbyGetAccountServers()`.
    AccountServers,
    /// the add-friend lookup: `GetUserIdFromNameOp` of text `b` on servers `a`, run behind its status box; its user
    /// id (its status in `*(int*)c`).
    UserIdLookup,
    /// `LobbyGetServerList()`.
    LobbyServerList,
    /// `((LobbyServerList*)a)->FindServer(((RustServerOp*)b)->GetAddr())`.
    ServerListFind,
    /// a connection-less group list op over `RustServerOp` b's replies, its groups into server a
    /// (`SyncGroups(m, false)`).
    SyncGroupsOfQuery,
    /// `RustServerOp::NewGroupListReq(server a's address, 0x29)` run tracked with
    /// `LogicCompletion<RoutingLogic>(&RoutingLogic::QueryServerGroupsDone, L)`.
    GroupQueryRun,
    /// `LobbyGetRoomDirServers()`, held.
    RoomDirServers,
    /// `RustServerOp::NewGetDir(context a, config's mProductDirectory, 0x04000622, {"_C", "_L"})`, held, run behind
    /// the getting-rooms status box.
    DirOpRun,
    /// op `a`'s `GetEntities(EE_WON_REQOP_DIR_LIST)` as a new list (held: `ListFree` kind 2); 1 if empty.
    DirEntities,
    /// `((ServerContext*)a)->NotifyFailed(((RustServerOp*)b)->GetCurAddr())`.
    DirNotifyFailed,
    /// `L->mServerList->UpdateFromDirectory(list a)`.
    DirUpdate,
}
use EeWonRouteLogicOp as Op;

/// The calls into the object (`ee_won_routelogic_call`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRouteLogicCall {
    /// `HostGame((LobbyGame*)a)`: 0 or 1.
    HostGame = 1,
    /// `JoinGame((LobbyGame*)a)`.
    JoinGame,
    /// `GetClientList()`.
    ClientList,
    /// `GetMyClient()`.
    MyClient,
    /// `SendToCaptain(a)`.
    SendToCaptain,
    /// `BroadcastToGame(a)`.
    BroadcastToGame,
    /// `SendToClient((LobbyClient*)a, b)`.
    SendToClient,
    /// `AdvertiseGame((LobbyGame*)a)`.
    Advertise,
    /// `RemoveGameAdvertisement`.
    RemoveAdvertisement,
    /// `KickClient((LobbyClient*)a, b != 0)`.
    KickClient,
    /// `SetGameInProgress(a != 0)`.
    SetGameInProgress,
    /// `Reset()` (the idle check).
    Reset,
    /// `LeaveGame`.
    LeaveGame,
    /// `Suspend()`.
    Suspend,
    /// `Resume()`.
    Resume,
    /// `Logout()`.
    Logout,
    /// `EnterInternetScreen(a != 0)`.
    EnterInternet,
    /// `HandleControlEvent`: a 1 change room, 2 add friend, 3 anonymous check, 4 dirty-word check, 5 refresh
    /// friends, 6 refresh the game list.
    Control,
    /// `HandleComponentEvent((ComponentEvent*)a)` of type `b`.
    Event,
    /// a routing notification `a` (1..21, `RoutingOpCompletion`'s code) on op `b`.
    Notify,
    /// `GetUserStatusComplete((AsyncOp*)a)`.
    UserStatusDone,
    /// `QueryServerGroupsDone((AsyncOp*)a)`.
    GroupsDone,
    /// `ConnectionClosed((AsyncOp*)a)`.
    ConnectionClosed,
    /// a member: 0 the current server, 1 the chat room, 2 the pending server, 3 the game room, 4 the wait op, 5
    /// suppress idle, 6 idle, 7 have the game list, 8 want to be moderator, 9 the last server refresh.
    Field,
}
use EeWonRouteLogicCall as Call;

pub type EeWonRouteLogicHostFn = unsafe extern "C" fn(
    obj: *mut c_void,
    op: EeWonRouteLogicOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    e: isize,
) -> isize;

// the room notices' codes (RoutingOpCompletion)
const N_CLIENT_JOINED_SERVER: isize = 1;
const N_CLIENT_LEFT_SERVER: isize = 2;
const N_CLIENT_JOINED_GROUP: isize = 3;
const N_CLIENT_LEFT_GROUP: isize = 4;
const N_PEER_CHAT: isize = 5;
const N_PEER_DATA: isize = 6;
const N_CLIENT_FLAGS: isize = 7;
const N_MEMBER_FLAGS: isize = 8;
const N_GROUP_CREATED: isize = 9;
const N_GROUP_DELETED: isize = 10;
const N_CAPTAIN: isize = 11;
const N_DATA_CREATED: isize = 12;
const N_DATA_DELETED: isize = 13;
const N_INVITATION: isize = 14;
const N_JOIN_ATTEMPT: isize = 15;
const N_BANNED: isize = 16;
const N_MUTED: isize = 17;
const N_ALERT: isize = 18;
const N_SHUTDOWN_STARTED: isize = 19;
const N_SHUTDOWN_ABORTED: isize = 20;
const N_GAME_LIST: isize = 21;

// ServerNum, GroupNum, ClientNum, FriendNum, GameNum, the event members
const S_CLIENTS: isize = 0;
const S_CONNECTION: isize = 1;
const S_MY_CLIENT: isize = 2;
const S_MY_CLIENT_ID: isize = 3;
const S_LAST_USED: isize = 4;
const S_NUM_CLIENTS: isize = 5;
const S_LANGUAGE: isize = 6;
const S_CONNECTED: isize = 7;
const S_GROUP_COUNT: isize = 8;
const S_QUERYING: isize = 9;
const S_LAST_PING: isize = 10;
const G_ID: isize = 0;
const G_IS_GAME: isize = 1;
const G_PUBLIC_CHAT: isize = 2;
const G_NUM_CLIENTS: isize = 3;
const G_PASSWORD: isize = 4;
const G_ASK_TO_JOIN: isize = 5;
const G_CAPTAIN_ID: isize = 6;
const G_NAME_LENGTH: isize = 7;
const G_LAST_ROOM: isize = 8;
const G_CLIENTS: isize = 9;
const C_ID: isize = 0;
const C_WON_ID: isize = 1;
const C_FRIEND: isize = 2;
const C_ANONYMOUS: isize = 3;
const C_IGNORED: isize = 4;
const C_BLOCKED: isize = 5;
const C_IN_CHAT: isize = 6;
const C_CHAT_ID: isize = 7;
const C_IN_GAME: isize = 8;
const C_GAME_ID: isize = 9;
const C_AWAY: isize = 10;
const C_MODERATOR: isize = 11;
const C_ADMIN: isize = 12;
const C_CDKEY_ID: isize = 13;
const F_FLAGS: isize = 0;
const F_LAST_LOCATE: isize = 1;
const F_USER_ID: isize = 2;
const F_ANONYMOUS: isize = 3;
const F_LAST_SEARCH: isize = 4;
const F_SEARCH_START: isize = 5;
const GM_ASK_TO_JOIN: isize = 0;
const GM_INVITE_ONLY: isize = 1;
const GM_TYPE: isize = 2;
const GM_GROUP_ID: isize = 3;
const GM_IS_HOST: isize = 4;
const GM_FILTERED: isize = 5;
const GM_PASSWORD: isize = 6;
const GM_CAN_JOIN: isize = 7;
const GM_PING: isize = 8;
const GM_SKILL: isize = 9;
const E_ACCEPT_CLIENT: isize = 1;
const E_ACCEPTED: isize = 2;
const E_FRIEND: isize = 3;
const E_FRIEND_ACTION: isize = 4;
const E_FRIEND_VALUE: isize = 5;
const E_JUMP_GROUP: isize = 6;
const E_QUICK_PLAY_TYPE: isize = 7;
const E_CA_SPEC: isize = 8;
const E_CA_CLIENT: isize = 9;
const E_CA_ACTION: isize = 10;
const E_JOIN_GROUP: isize = 11;
const E_JOIN_FRIEND: isize = 12;
const E_COMPONENT: isize = 13;

// Pref
const P_CHAT_SOUND: isize = 1;
const P_STAGING_SOUND: isize = 2;
const P_LOBBY_SOUND: isize = 3;
const P_AUTO_PING: isize = 4;
const P_LOBBY_MUSIC: isize = 5;
const P_ANONYMOUS: isize = 6;
const P_FILTER: isize = 7;
const P_INTERNET_SCREEN: isize = 8;
const P_CANCELLED: isize = 9;

// Lobby
const L_KILL_PINGS: isize = 1;
const L_GAME_DISCONNECTED: isize = 2;
const L_LEAVE_GAME: isize = 3;
const L_SHOW_LOBBY_TAB: isize = 4;
const L_HOST_GAME: isize = 5;
const L_PINGS_TO_DO: isize = 6;
const L_LOGOUT: isize = 7;
const L_SHOW_INTERNET: isize = 8;

// the dialogs
const D_ADD_FRIEND: isize = 1;
const D_ADMIN: isize = 2;
const D_BAD_USER: isize = 3;
const D_CREATE_ROOM: isize = 4;
const D_PASSWORD: isize = 5;
const D_ROOM: isize = 6;

// the routing op types
const RT_BAN_CLIENT: isize = 3;
const RT_CREATE_GROUP: isize = 12;
const RT_GET_BAD_USER_LIST: isize = 19;
const RT_GET_GROUP_LIST: isize = 21;
const RT_JOIN_GROUP: isize = 34;
const RT_MUTE_CLIENT: isize = 38;
const RT_REGISTER_CLIENT: isize = 42;

const DIALOG_OK: isize = 100001;
const AWAY: isize = 8;

fn res(r: Res) -> isize {
    r as isize
}

/// The Internet lobby's logic.
pub struct EeWonRouteLogic {
    obj: *mut c_void,
    host: EeWonRouteLogicHostFn,
    /// the model's ops answered by its Rust objects (modelobj.rs; the game's logic), not the host
    model: bool,
    last_server_refresh: Cell<i64>,
    suppress_idle: Cell<bool>,
    idle: Cell<bool>,
    have_game_list: Cell<bool>,
    want_moderator: Cell<bool>,
    wait_op: Cell<isize>,
    current: Cell<isize>,
    pending: Cell<isize>,
    chat: Cell<isize>,
    game: Cell<isize>,
}

/// A reference or text Rust holds for a scope (a `SmartPtr` local, a `GUIString` local).
struct Held<'a> {
    lg: &'a EeWonRouteLogic,
    p: isize,
    free: Op,
}

impl Drop for Held<'_> {
    fn drop(&mut self) {
        if self.p != 0 {
            self.lg.h(self.free, &[self.p]);
        }
    }
}

impl EeWonRouteLogic {
    pub fn new(obj: *mut c_void, host: EeWonRouteLogicHostFn) -> Self {
        EeWonRouteLogic {
            obj,
            host,
            model: false,
            last_server_refresh: Cell::new(0),
            suppress_idle: Cell::new(false),
            idle: Cell::new(false),
            have_game_list: Cell::new(false),
            want_moderator: Cell::new(false),
            wait_op: Cell::new(0),
            current: Cell::new(0),
            pending: Cell::new(0),
            chat: Cell::new(0),
            game: Cell::new(0),
        }
    }

    fn h(&self, op: Op, args: &[isize]) -> isize {
        let g = |i: usize| args.get(i).copied().unwrap_or(0);
        if self.model {
            if let Some(r) = crate::modelobj::route_op(op, g(0), g(1), g(2), g(3)) {
                return r;
            }
            if let Some(r) = crate::persistobj::game_route_op(op, g(0)) {
                return r;
            }
            if op == Op::EventNum {
                if let Some(r) = crate::lobbyevt::route_event_num(g(0), g(1)) {
                    return r;
                }
            }
            if let Some((kind, values)) = conn_request(op, [g(0), g(1), g(2), g(3), g(4)], &|p| self.text_utf16(p)) {
                ee_won_net::routecpp::request(g(0) as *mut c_void, kind, &values, std::ptr::null_mut());
                return 0;
            }
        }
        // SAFETY: the host is the C++ (or a test's) function given at creation, called with its own object.
        unsafe { (self.host)(self.obj, op, g(0), g(1), g(2), g(3), g(4)) }
    }
    fn b(&self, op: Op, args: &[isize]) -> bool {
        self.h(op, args) != 0
    }
    /// A host text's UTF-16 little-endian bytes (a request's text value).
    fn text_utf16(&self, p: isize) -> Vec<u8> {
        let n = self.h(Op::TextUnits, &[p, 0]).max(0) as usize;
        let mut v = vec![0u32; n];
        if n != 0 {
            self.h(Op::TextUnits, &[p, v.as_mut_ptr() as isize]);
        }
        utf16le(&v)
    }
    /// A held reference given back at the scope's end.
    fn hold(&self, p: isize) -> Held<'_> {
        Held { lg: self, p, free: Op::Release }
    }
    /// A reference taken (a `SmartPtr` copy of a raw pointer) and given back at the scope's end.
    fn take(&self, p: isize) -> Held<'_> {
        if p != 0 {
            self.h(Op::AddRef, &[p]);
        }
        self.hold(p)
    }
    fn text(&self, p: isize) -> Held<'_> {
        Held { lg: self, p, free: Op::TextFree }
    }
    /// `SmartPtr`'s assignment: the old one released, the new one taken.
    fn assign(&self, cell: &Cell<isize>, p: isize) {
        let old = cell.get();
        if old != p {
            if old != 0 {
                self.h(Op::Release, &[old]);
            }
            cell.set(p);
            if p != 0 {
                self.h(Op::AddRef, &[p]);
            }
        }
    }

    fn snum(&self, s: isize, f: isize) -> isize {
        self.h(Op::ServerNum, &[s, f])
    }
    fn gnum(&self, g: isize, f: isize) -> isize {
        self.h(Op::GroupNum, &[g, f])
    }
    fn gid(&self, g: isize) -> u16 {
        self.gnum(g, G_ID) as u16
    }
    fn cnum(&self, c: isize, f: isize) -> isize {
        self.h(Op::ClientNum, &[c, f])
    }
    fn cb(&self, c: isize, f: isize) -> bool {
        self.cnum(c, f) != 0
    }
    fn cid(&self, c: isize) -> u16 {
        self.cnum(c, C_ID) as u16
    }
    fn won_id(&self, c: isize) -> u32 {
        self.cnum(c, C_WON_ID) as u32
    }
    fn fnum(&self, f: isize, k: isize) -> isize {
        self.h(Op::FriendNum, &[f, k])
    }
    fn user_id(&self, f: isize) -> u32 {
        self.fnum(f, F_USER_ID) as u32
    }
    fn fdo(&self, f: isize, k: isize, c: isize, d: isize) {
        self.h(Op::FriendDo, &[f, k, c, d]);
    }
    fn gmnum(&self, g: isize, f: isize) -> isize {
        self.h(Op::GameNum, &[g, f])
    }
    fn gldo(&self, k: isize, b: isize, c: isize) -> isize {
        self.h(Op::GameList, &[k, b, c])
    }
    fn rnum(&self, r: isize, f: i32) -> i64 {
        self.h(Op::RNum, &[r, f as isize]) as i64
    }
    fn rgroup(&self, r: isize) -> u16 {
        self.rnum(r, EE_WON_RTF_GROUP_ID) as u16
    }
    fn rclient(&self, r: isize) -> u16 {
        self.rnum(r, EE_WON_RTF_CLIENT_ID) as u16
    }
    fn rset(&self, r: isize, f: i32, v: isize) {
        self.h(Op::RSetNum, &[r, f as isize, v]);
    }
    fn rtext(&self, r: isize, f: i32, t: isize) {
        self.h(Op::RSetText, &[r, f as isize, t]);
    }
    fn pref(&self, p: isize) -> bool {
        self.b(Op::Pref, &[p])
    }
    fn lobby(&self, k: isize, b: isize) -> isize {
        self.h(Op::Lobby, &[k, b])
    }
    fn sound(&self, r: Res) {
        self.h(Op::Sound, &[res(r)]);
    }
    fn message_box(&self, t: Res, d: Res) {
        self.h(Op::MessageBox, &[res(t), res(d)]);
    }
    fn run_status(&self, op: isize, t: Res, d: Res, mode: isize, timeout: isize) {
        self.h(Op::RunWithStatus, &[op, res(t), res(d), mode | timeout << 8]);
    }
    fn status(&self, op: isize) -> i32 {
        self.h(Op::OpStatus, &[op]) as i32
    }
    fn ev_client(&self, c: isize, spec: isize, kind: isize) {
        self.h(Op::EvClientChanged, &[c, spec, kind]);
    }
    fn ev_friend(&self, f: isize, kind: isize) {
        self.h(Op::EvFriendChanged, &[f, kind]);
    }
    fn ev_left_room(&self, spec: isize, a: bool, b: bool) {
        self.h(Op::EvLeftRoom, &[spec, a as isize, b as isize]);
    }
    fn conn_do(&self, c: isize, k: isize, b: isize) -> isize {
        self.h(Op::ConnDo, &[c, k, b])
    }
    fn set_client_flags(&self, c: isize, mask: isize, flags: isize, amask: isize, aflags: isize) {
        self.h(Op::ConnSetClientFlags, &[c, mask, flags, amask, aflags]);
    }
    fn dialog_do(&self, d: isize) -> isize {
        self.h(Op::DialogDo, &[d])
    }
    fn event_num(&self, e: isize, f: isize) -> isize {
        self.h(Op::EventNum, &[e, f])
    }

    // ---- the members

    fn clear_pending(&self) {
        let p = self.pending.get();
        if p != 0 && p != self.current.get() {
            self.snum_do(p, 1, 0);
        }
        self.assign(&self.pending, 0);
    }
    fn snum_do(&self, s: isize, k: isize, c: isize) {
        self.h(Op::ServerDo, &[s, k, c]);
    }
    fn room_spec_id(&self, id: u16) -> i32 {
        let chat = self.chat.get();
        let chat = if chat != 0 { self.gid(chat) as i32 } else { -1 };
        let game = self.game.get();
        let game = if game != 0 { self.gid(game) as i32 } else { -1 };
        let g = |v: i32| u16::try_from(v).ok();
        routing::room_spec(id, g(chat), g(game))
    }
    fn room_spec(&self, group: isize, server: isize) -> i32 {
        if server == self.current.get() && group != 0 {
            return self.room_spec_id(self.gid(group));
        }
        0
    }
    fn client_list(&self) -> isize {
        let s = self.current.get();
        if s != 0 {
            return self.snum(s, S_CLIENTS);
        }
        0
    }
    fn connection(&self) -> isize {
        let s = self.current.get();
        if s == 0 {
            return 0;
        }
        self.snum(s, S_CONNECTION)
    }
    fn group_of_spec(&self, spec: i32) -> isize {
        match routing::group_of_spec(spec) {
            1 => self.chat.get(),
            2 => self.game.get(),
            _ => 0,
        }
    }
    fn server_of_op(&self, op: isize) -> isize {
        let c = self.current.get();
        if c != 0 && self.h(Op::RConnection, &[op]) == self.snum(c, S_CONNECTION) {
            return c;
        }
        let p = self.pending.get();
        if p != 0 && self.h(Op::RConnection, &[op]) == self.snum(p, S_CONNECTION) {
            return p;
        }
        0
    }
    fn default_server(&self) -> isize {
        if !self.b(Op::HasConfig, &[]) {
            return 0;
        }
        let mut list = Vec::new();
        let mut picks = Vec::new();
        let mut i = 0;
        while i < self.h(Op::ListNum, &[0]) {
            let s = self.h(Op::ListAt, &[i]);
            let pick = routing::EeWonServerPick {
                last_used: self.snum(s, S_LAST_USED) != 0,
                clients: self.snum(s, S_NUM_CLIENTS) as i32,
                language: self.snum(s, S_LANGUAGE) != 0,
            };
            list.push(s);
            picks.push(pick);
            if pick.last_used {
                break;
            }
            i += 1;
        }
        routing::default_server(&picks).map_or(0, |i| list[i])
    }
    fn my_client(&self) -> isize {
        let s = self.current.get();
        if s == 0 {
            0
        } else {
            self.snum(s, S_MY_CLIENT)
        }
    }
    fn send_data(&self, buf: isize, to: Option<isize>, captain: bool) {
        let conn = self.connection();
        let game = self.game.get();
        if conn != 0 && game != 0 {
            let id = match to {
                Some(c) => self.cid(c) as isize,
                None if captain => self.gnum(game, G_CAPTAIN_ID) as u16 as isize,
                None => self.gid(game) as isize,
            };
            self.h(Op::ConnSendData, &[conn, buf, id]);
        }
    }
    fn advertise(&self, game: isize) {
        let conn = self.connection();
        let g = self.game.get();
        if conn == 0 || g == 0 {
            return;
        }
        self.h(Op::ConnAdvertise, &[conn, game, self.gid(g) as isize]);
    }
    fn remove_advertisement(&self) {
        let conn = self.connection();
        let g = self.game.get();
        if conn == 0 || g == 0 {
            return;
        }
        self.conn_do(conn, 10, self.gid(g) as isize);
    }
    fn kick_client(&self, client: isize, ban: bool) {
        let g = self.game.get();
        if g == 0 {
            return;
        }
        let conn = self.connection();
        if conn == 0 {
            return;
        }
        let group = self.gid(g) as isize;
        self.h(Op::ConnBan, &[conn, group | 1 << 16, !ban as isize, 0, self.cid(client) as isize]);
    }
    fn set_game_in_progress(&self, close: bool) {
        let conn = self.connection();
        let g = self.game.get();
        if conn == 0 || g == 0 {
            return;
        }
        self.h(Op::ConnSetGroupFlags, &[conn, self.gid(g) as isize, 2, if close { 2 } else { 0 }]);
    }

    // ---- the routing server's notices

    fn member_flags_changed(&self, r: isize, server: isize) {
        let id = self.rgroup(r);
        let group = self.h(Op::ServerGroup, &[server, id as isize]);
        if group == 0 {
            return;
        }
        let client = self.h(Op::GroupFindClient, &[group, self.rclient(r) as isize]);
        if client == 0 {
            return;
        }
        let flags = self.rnum(r, EE_WON_RTF_MEMBER_FLAGS) as u8;
        let is_game = self.gnum(group, G_IS_GAME);
        self.h(Op::ClientDo, &[client, 1, flags as isize, is_game]);
        if server != self.current.get() {
            return;
        }
        let spec = self.room_spec(group, server);
        if spec != 0 {
            self.ev_client(client, spec as u32 as isize, 2);
        }
    }
    fn client_flags_changed(&self, r: isize, server: isize) {
        let client = self.h(Op::ServerClient, &[server, self.rclient(r) as isize]);
        if client == 0 {
            return;
        }
        let flags = self.rnum(r, EE_WON_RTF_CLIENT_FLAGS) as u32;
        self.h(Op::ClientDo, &[client, 2, flags as isize]);
        self.ev_client(client, 4, 2);
        if !self.cb(client, C_FRIEND) {
            return;
        }
        let f = self.h(Op::FindFriend, &[self.won_id(client) as isize]);
        if f == 0 || (self.fnum(f, F_ANONYMOUS) != 0) == self.cb(client, C_ANONYMOUS) {
            return;
        }
        let anonymous = self.cnum(client, C_ANONYMOUS);
        self.fdo(f, 1, anonymous, 0);
        self.ev_friend(f, 2);
    }
    fn should_locate_friend(&self, f: isize, now: u32) -> bool {
        let locating = self.fnum(f, F_FLAGS) & 4 != 0;
        let current = self.current.get();
        let here = !locating && current != 0 && self.b(Op::FriendAtServer, &[f, current]);
        let count = self.h(Op::ListNum, &[0]) as usize;
        let last = self.fnum(f, F_LAST_LOCATE) as u32;
        routing::should_locate_friend(locating, here, count, current != 0, now, last)
    }
    fn announce_client_location(&self, client: isize, force: bool, spec: i32, by_user_id: bool) -> bool {
        let current = self.current.get();
        if current == 0 || client == 0 {
            return false;
        }
        let mut client = client;
        let mut chat_room = 0;
        let mut game_room = 0;
        'announce: {
            if by_user_id && self.won_id(client) != 0 {
                let found = self.h(Op::ServerFindByWonId, &[current, self.won_id(client) as isize]);
                if found != 0 {
                    client = found;
                } else {
                    break 'announce;
                }
            }
            if self.cb(client, C_IN_CHAT) {
                let key = self.cnum(client, C_CHAT_ID) as u16 as isize;
                let g = self.h(Op::ServerGroup, &[current, key]);
                if g != 0 {
                    chat_room = g;
                }
            }
            if self.cb(client, C_IN_GAME) {
                let key = self.cnum(client, C_GAME_ID) as u16 as isize;
                let g = self.h(Op::ServerGroup, &[current, key]);
                if g != 0 {
                    game_room = g;
                }
            }
        }
        let len = |g: isize| if g != 0 { self.gnum(g, G_NAME_LENGTH) } else { 0 };
        if len(chat_room) == 0 && len(game_room) == 0 && !force {
            return false;
        }
        self.h(Op::EvAnnounceClient, &[client, chat_room, game_room, (spec | 4) as isize]);
        true
    }
    fn peer_data(&self, r: isize, server: isize) {
        let g = self.game.get();
        if server != self.current.get() || g == 0 {
            return;
        }
        let client = self.h(Op::GroupFindClient, &[g, self.rclient(r) as isize]);
        if client == 0 || self.cid(client) == self.snum(self.current.get(), S_MY_CLIENT_ID) as u16 {
            return;
        }
        self.h(Op::HandleStaging, &[r, client]);
    }
    fn client_joined_group(&self, r: isize, server: isize) {
        let group = self.h(Op::ServerGroup, &[server, self.rgroup(r) as isize]);
        if group == 0 {
            return;
        }
        let client = self.h(Op::ServerClient, &[server, self.rclient(r) as isize]);
        if client == 0 {
            return;
        }
        let flags = self.rnum(r, EE_WON_RTF_MEMBER_FLAGS) as u8;
        self.h(Op::ServerAddClientToGroup, &[server, client, flags as isize, group]);
        let spec = self.room_spec(group, server);
        if spec != 0 {
            self.ev_client(client, spec as isize, 0);
        }
        self.h(Op::EvGroupChanged, &[group, server, 2]);
        self.update_friend(client);
    }
    fn client_left_group(&self, r: isize, server: isize) {
        let group = self.h(Op::ServerGroup, &[server, self.rgroup(r) as isize]);
        if group == 0 {
            return;
        }
        let client = self.hold(self.h(Op::ServerRemoveClientFromGroup, &[server, self.rclient(r) as isize, group]));
        if client.p == 0 {
            return;
        }
        let spec = self.room_spec(group, server);
        if spec != 0 {
            self.ev_client(client.p, spec as isize, 1);
            if spec & 2 != 0 {
                self.h(Op::ClientLeftGame, &[client.p]);
            }
        }
        self.h(Op::EvGroupChanged, &[group, server, 2]);
        self.update_friend(client.p);
    }
    fn group_created(&self, r: isize, server: isize) {
        let group = self.h(Op::ServerAddGroup, &[server, r]);
        if group != 0 {
            self.h(Op::EvGroupChanged, &[group, server, 0]);
        }
    }
    fn group_deleted(&self, r: isize, server: isize) {
        let group = self.hold(self.h(Op::ServerRemoveGroup, &[server, self.rgroup(r) as isize]));
        if group.p != 0 {
            self.h(Op::EvGroupChanged, &[group.p, server, 1]);
        }
    }
    fn update_friend(&self, client: isize) {
        if !self.cb(client, C_FRIEND) {
            return;
        }
        let f = self.h(Op::FindFriend, &[self.won_id(client) as isize]);
        if f == 0 {
            return;
        }
        self.fdo(f, 2, client, 0);
        self.ev_friend(f, 2);
    }
    fn announce_friend_location(&self, f: isize, spec: i32) {
        if f == 0 {
            return;
        }
        let current = self.current.get();
        if current != 0 && self.b(Op::FriendAtServer, &[f, current]) {
            let client = self.h(Op::ServerFindByWonId, &[current, self.user_id(f) as isize]);
            if client != 0 && self.announce_client_location(client, false, spec, false) {
                return;
            }
        }
        self.h(Op::EvAnnounceFriend, &[f, spec as isize, 2]);
    }
    fn client_joined_server(&self, r: isize, server: isize) {
        let client = self.h(Op::ServerAddClient, &[server, r]);
        let f = self.h(Op::FindFriend, &[self.won_id(client) as isize]);
        if f == 0 {
            return;
        }
        self.h(Op::ClientDo, &[client, 3, 1]);
        self.fdo(f, 3, server, 0);
        let anonymous = self.cnum(client, C_ANONYMOUS);
        self.fdo(f, 1, anonymous, 0);
        self.ev_friend(f, 2);
        if server != self.current.get() || self.cb(client, C_ANONYMOUS) {
            return;
        }
        if self.pref(P_CHAT_SOUND) {
            self.sound(Res::LobbyGlobal_FriendJoined_Sound);
        }
        self.h(Op::EvAnnounceFriend, &[f, 4, 0]);
    }
    fn client_left_server(&self, r: isize, server: isize) {
        let client = self.hold(self.h(Op::ServerRemoveClient, &[server, self.rclient(r) as isize]));
        if client.p != 0 {
            if !self.cb(client.p, C_FRIEND) {
                return;
            }
            let f = self.h(Op::FindFriend, &[self.won_id(client.p) as isize]);
            if f == 0 {
                return;
            }
            self.fdo(f, 4, 0, 0);
            self.ev_friend(f, 2);
            if server == self.current.get() && !self.cb(client.p, C_ANONYMOUS) {
                if self.pref(P_CHAT_SOUND) {
                    self.sound(Res::LobbyGlobal_FriendLeft_Sound);
                }
                self.h(Op::EvAnnounceFriend, &[f, 4, 1]);
            }
        }
    }
    fn group_invitation(&self, r: isize, server: isize) {
        let invited = self.rnum(r, EE_WON_RTF_ON) != 0;
        if server != self.current.get() {
            return;
        }
        let client = self.h(Op::ServerClient, &[server, self.rclient(r) as isize]);
        if client == 0 || self.cb(client, C_IGNORED) || self.cb(client, C_BLOCKED) {
            return;
        }
        let game = self.gldo(5, self.rgroup(r) as isize, 0);
        if game == 0 {
            return;
        }
        self.h(Op::GameDo, &[game, 3, invited as isize]);
        self.h(Op::EvGameChanged, &[game, 2]);
        if self.pref(P_STAGING_SOUND) {
            self.sound(Res::LobbyGlobal_InviteRecv_Sound);
        }
        self.h(Op::EvInvitationRecv, &[game, client, invited as isize, r]);
    }
    fn group_join_attempt(&self, r: isize, server: isize) {
        let g = self.game.get();
        if server != self.current.get() || g == 0 || self.rgroup(r) != self.gid(g) {
            return;
        }
        let client = self.h(Op::ServerClient, &[server, self.rclient(r) as isize]);
        if client == 0 || self.cb(client, C_IGNORED) || self.cb(client, C_BLOCKED) {
            return;
        }
        if self.pref(P_STAGING_SOUND) {
            self.sound(Res::LobbyGlobal_AskToJoinRecv_Sound);
        }
        self.h(Op::EvJoinAttempt, &[client, r]);
    }
    fn reset(&self) {
        if self.suppress_idle.get() || self.current.get() == 0 {
            return;
        }
        let last = self.h(Op::LastInputTick, &[]) as u32;
        if (self.h(Op::Tick, &[]) as u32).wrapping_sub(last) > 300000 {
            if self.idle.get() {
                return;
            }
            let conn = self.connection();
            if conn != 0 {
                self.set_client_flags(conn, AWAY, AWAY, 0, 0);
                self.idle.set(true);
            }
        } else if self.idle.get() {
            let conn = self.connection();
            if conn != 0 {
                self.set_client_flags(conn, AWAY, 0, 0, 0);
                self.idle.set(false);
            }
        }
    }
    fn peer_chat(&self, r: isize, server: isize) {
        let recipient = self.h(Op::RRecipient, &[r]);
        let flags = self.rnum(r, EE_WON_RTF_FLAGS16) as u16;
        if server != self.current.get() || (self.chat.get() == 0 && self.game.get() == 0) || recipient < 0 {
            return;
        }
        let recipient = recipient as u16 as i32;
        let sender = self.h(Op::ServerClient, &[server, self.rclient(r) as isize]);
        if sender == 0 || self.cb(sender, C_IGNORED) || self.cb(sender, C_BLOCKED) {
            return;
        }
        let mut target = 0;
        let mut spec: u32 = 4;
        if flags & 0x100 != 0 {
            if self.game.get() == 0 {
                return;
            }
            spec = 0x16;
        } else if recipient < 0x4002 {
            spec = self.room_spec_id(recipient as u16) as u32;
            if spec == 0 {
                return;
            }
        } else {
            target = self.h(Op::ServerClient, &[server, recipient as isize]);
            if target == 0 {
                return;
            }
        }
        if flags & 4 != 0 && target != 0 && self.cid(target) == self.snum(server, S_MY_CLIENT_ID) as u16 {
            if self.pref(P_CHAT_SOUND) {
                self.sound(Res::LobbyGlobal_WhisperRecv_Sound);
            }
            let parser = self.h(Op::Parser, &[]);
            if parser != 0 {
                self.h(Op::ParserSetInput, &[parser, sender]);
            }
        }
        if flags & 0x10 != 0 && self.pref(P_LOBBY_SOUND) {
            self.sound(Res::LobbyGlobal_Error_Sound);
        }
        self.h(Op::EvRecvChat, &[spec as isize, flags as isize, sender, r, target]);
    }
    fn alert(&self, r: isize, kind: isize) {
        if self.pref(P_LOBBY_SOUND) {
            self.sound(Res::LobbyGlobal_Error_Sound);
        }
        self.h(Op::EvAlert, &[kind, r]);
    }
    /// The room a moderation notice names: (spec, the server or group with its name, a group).
    fn moderated_room(&self, id: u16, server: isize) -> Option<(i32, isize, bool)> {
        if id == 0xffff {
            return Some((0, server, false));
        }
        let g = self.game.get();
        if g != 0 && id == self.gid(g) {
            return Some((2, g, true));
        }
        let c = self.chat.get();
        if c != 0 && id == self.gid(c) {
            return Some((1, c, true));
        }
        None
    }
    fn you_were_muted(&self, r: isize, server: isize) {
        let kind = if self.rnum(r, EE_WON_RTF_ON) != 0 { 2 } else { 3 };
        let id = self.rgroup(r);
        let Some((spec, name, group)) = self.moderated_room(id, server) else { return };
        let client = self.snum(server, S_MY_CLIENT);
        self.h(Op::EvModerated, &[client, (spec | 4) as isize, name, kind | (group as isize) << 8, r]);
    }
    fn leave_game(&self) {
        let g = self.game.get();
        if g == 0 {
            return;
        }
        if self.chat.get() == 0 {
            self.assign(&self.game, 0);
            self.leave_server();
        } else {
            let conn = self.connection();
            if conn != 0 {
                self.conn_do(conn, 8, self.gid(self.game.get()) as isize);
            }
            self.assign(&self.game, 0);
        }
        self.ev_left_room(6, false, true);
    }
    fn suspend(&self) {
        let conn = self.connection();
        if conn == 0 {
            return;
        }
        let c = self.chat.get();
        if c != 0 {
            self.conn_do(conn, 8, self.gid(c) as isize);
            self.assign(&self.chat, 0);
            self.ev_left_room(5, false, true);
        }
        self.set_client_flags(conn, 0, 0, 0xffff_ffff, 0xfffb_ffff);
    }
    fn resume(&self) {
        let conn = self.connection();
        if conn != 0 {
            self.set_client_flags(conn, 0x40, 0x40, 0, 0);
        }
        self.lobby(L_LEAVE_GAME, 0);
        self.leave_server();
        self.join_default_room(true);
    }
    fn leave_server(&self) {
        let c = self.current.get();
        if c != 0 {
            self.snum_do(c, 1, 0);
            self.lobby(L_KILL_PINGS, 0);
            self.ev_left_room(4, false, false);
        }
        let w = self.wait_op.get();
        if w != 0 {
            self.h(Op::OpKill, &[w]);
        }
        self.have_game_list.set(false);
        self.assign(&self.current, 0);
        self.assign(&self.chat, 0);
        self.gldo(1, 0, 0);
        self.h(Op::EvSyncGameList, &[]);
        if self.game.get() != 0 {
            self.assign(&self.game, 0);
            self.lobby(L_GAME_DISCONNECTED, 1);
        }
    }
    fn group_captain_changed(&self, r: isize, server: isize) {
        let group = self.h(Op::ServerGroup, &[server, self.rgroup(r) as isize]);
        if group == 0 {
            return;
        }
        let mut old: isize = 0;
        let captain = self.rnum(r, EE_WON_RTF_CAPTAIN_ID) as u16;
        let new = self.h(Op::GroupSetCaptain, &[group, captain as isize, &mut old as *mut isize as isize]);
        if server != self.current.get() {
            return;
        }
        let spec = self.room_spec(group, server);
        if spec == 0 {
            return;
        }
        if old != 0 {
            self.ev_client(old, spec as isize, 2);
        }
        if new != 0 {
            self.ev_client(new, spec as isize, 2);
        }
        if group == self.game.get() && old != 0 {
            self.lobby(L_GAME_DISCONNECTED, 1);
        }
    }
    fn data_object_deleted(&self, r: isize, server: isize) {
        if server != self.current.get() || !self.b(Op::RIsGameData, &[r]) {
            return;
        }
        let link = self.rnum(r, EE_WON_RTF_LINK_ID) as u16;
        let game = self.hold(self.gldo(7, link as isize, 0));
        if game.p != 0 {
            self.h(Op::EvGameChanged, &[game.p, 1]);
        }
    }
    fn data_object_created(&self, r: isize, server: isize) {
        if server != self.current.get() || !self.b(Op::RIsGameData, &[r]) {
            return;
        }
        let link = self.rnum(r, EE_WON_RTF_LINK_ID) as u16;
        let group = self.h(Op::ServerGroup, &[server, link as isize]);
        if group == 0 {
            return;
        }
        let mut existed = false;
        let game = self.gldo(8, self.gid(group) as isize, &mut existed as *mut bool as isize);
        if !self.b(Op::GameReadSummary, &[game, r]) {
            let _hold = self.take(game);
            drop(self.hold(self.gldo(7, self.gid(group) as isize, 0)));
            if existed {
                self.h(Op::EvGameChanged, &[game, 1]);
            }
            return;
        }
        if !existed {
            self.h(Op::GameDo, &[game, 4, group]);
            if self.game.get() != 0 {
                let mine = self.h(Op::MyGame, &[]);
                if mine != 0
                    && self.gmnum(mine, GM_IS_HOST) != 0
                    && self.gmnum(game, GM_GROUP_ID) as u16 == self.gmnum(mine, GM_GROUP_ID) as u16
                {
                    self.h(Op::GameDo, &[game, 3, 1]);
                }
            }
            if self.pref(P_AUTO_PING) {
                self.h(Op::PingGame, &[game]);
            }
        }
        self.h(Op::EvGameChanged, &[game, if existed { 2 } else { 0 }]);
    }
    fn you_were_banned(&self, r: isize, server: isize) {
        let banned = self.rnum(r, EE_WON_RTF_ON) != 0;
        let id = self.rgroup(r);
        if !banned {
            return;
        }
        let duration = self.rnum(r, EE_WON_RTF_TIME) as u32;
        let kind = (duration != 0 && duration < 10) as isize;
        let Some((spec, name, group)) = self.moderated_room(id, server) else { return };
        let mine = self.snum(server, S_MY_CLIENT);
        if (spec >> 1) & 1 != 0 {
            self.h(Op::ClientKicked, &[mine, (kind == 0) as isize]);
            return;
        }
        self.h(Op::EvModerated, &[mine, (spec | 4) as isize, name, kind | (group as isize) << 8, r]);
        if spec & 1 != 0 {
            if self.game.get() == 0 {
                self.leave_server();
            } else {
                self.assign(&self.chat, 0);
                self.ev_left_room(5, false, false);
            }
        } else {
            let had_game = self.game.get() != 0;
            if had_game {
                self.lobby(L_LEAVE_GAME, 0);
            }
            self.leave_server();
            if had_game {
                self.lobby(L_SHOW_LOBBY_TAB, 0);
            }
        }
    }

    // ---- joining servers and rooms

    /// A connect attempt's body, the pending server cleared on every exit (`PendingServerCleanup`).
    fn with_pending<T>(&self, body: impl FnOnce() -> T) -> T {
        let r = body();
        self.clear_pending();
        r
    }
    fn host_game(&self, game: isize) -> bool {
        if self.pending.get() != 0 || self.game.get() != 0 {
            return false;
        }
        let mut server = self.current.get();
        if server == 0 {
            server = self.default_server();
        }
        if server == 0 {
            return false;
        }
        self.with_pending(|| {
            if !self.connect_to_server(server, false) {
                return false;
            }
            let mut flags = 0;
            if self.gmnum(game, GM_ASK_TO_JOIN) != 0 {
                flags = 0x100;
            }
            if self.gmnum(game, GM_INVITE_ONLY) != 0 {
                flags |= 0x80;
            }
            let mut id = 0u16;
            let name = self.text(self.h(Op::GameText, &[game, 0]));
            let password = self.text(self.h(Op::GameText, &[game, 1]));
            if !self.create_group(self.pending.get(), name.p, password.p, flags, &mut id) {
                return false;
            }
            drop(password);
            drop(name);
            self.joined_group(id, true);
            if self.connection() == 0 {
                return false;
            }
            self.h(Op::GameDo, &[game, 1, id as isize]);
            self.snum_do(server, 2, 0);
            let g = self.game.get();
            if g != 0 {
                self.h(Op::GameDo, &[game, 2, self.gnum(g, G_CLIENTS)]);
            }
            true
        })
    }
    fn join_game(&self, game: isize) -> bool {
        if self.pending.get() != 0 || self.game.get() != 0 || self.current.get() == 0 || self.gmnum(game, GM_TYPE) != 2
        {
            return false;
        }
        self.with_pending(|| {
            if !self.connect_to_server(self.current.get(), false) {
                return false;
            }
            if !self.join_group(self.pending.get(), self.gmnum(game, GM_GROUP_ID) as u16 as i32) {
                return false;
            }
            self.joined_group(self.gmnum(game, GM_GROUP_ID) as u16, true);
            let g = self.game.get();
            if g != 0 {
                self.h(Op::GameDo, &[game, 2, self.gnum(g, G_CLIENTS)]);
            }
            true
        })
    }
    fn join_default_room(&self, show_status: bool) -> bool {
        let server = self.default_server();
        if server == 0 {
            return false;
        }
        self.with_pending(|| {
            if !self.connect_to_server(server, show_status) {
                return false;
            }
            let mut best = 0;
            let mut i = 0;
            while i < self.snum(server, S_GROUP_COUNT) {
                let g = self.h(Op::ServerGroupAt, &[server, i]);
                if self.gnum(g, G_PUBLIC_CHAT) != 0 {
                    if self.gnum(g, G_LAST_ROOM) != 0 {
                        best = g;
                        break;
                    }
                    if best == 0
                        || (self.gnum(g, G_NUM_CLIENTS) as i32 > self.gnum(best, G_NUM_CLIENTS) as i32
                            && (self.gnum(g, G_NUM_CLIENTS) as i32) < 50)
                    {
                        best = g;
                    }
                }
                i += 1;
            }
            let mut id = 0;
            if best != 0 {
                id = self.gid(best) as i32;
            }
            if !self.join_group(self.pending.get(), id) {
                return false;
            }
            self.joined_group(id as u16, false);
            true
        })
    }
    fn refresh_game_list(&self) {
        if !self.pref(P_INTERNET_SCREEN) {
            return;
        }
        self.gldo(2, 0, 0);
        while self.gldo(3, 0, 0) != 0 {
            let game = self.gldo(4, 0, 0);
            if self.gmnum(game, GM_FILTERED) == 0 {
                self.h(Op::PingGame, &[game]);
            }
        }
        self.h(Op::BroadcastLobby, &[100058]);
    }
    fn joined_group(&self, id: u16, is_game: bool) {
        let old = if is_game { self.game.get() } else { self.chat.get() };
        let p = self.pending.get();
        if p != 0 && p != self.current.get() {
            self.leave_server();
            self.idle.set(false);
            self.suppress_idle.set(false);
            self.have_game_list.set(false);
            self.assign(&self.current, self.pending.get());
            let conn = self.connection();
            if conn != 0 {
                self.conn_do(conn, 4, 0);
            }
        } else if old != 0 && self.current.get() != 0 && self.snum(self.current.get(), S_CONNECTION) != 0 {
            let conn = self.snum(self.current.get(), S_CONNECTION);
            self.conn_do(conn, 8, self.gid(old) as isize);
        }
        let current = self.current.get();
        if current == 0 {
            return;
        }
        let group = self.h(Op::ServerGroup, &[current, id as isize]);
        if is_game {
            self.assign(&self.game, group);
        } else {
            self.assign(&self.chat, group);
        }
        if group == 0 {
            return;
        }
        if self.gnum(group, G_PUBLIC_CHAT) != 0 && self.current.get() != 0 {
            self.h(Op::SetLastRoom, &[self.current.get(), group]);
        }
        let spec = (if is_game { 2 } else { 1 }) | 4;
        self.h(Op::EvJoinedRoom, &[self.current.get(), group, spec]);
    }
    fn read_game_list_complete(&self, r: isize, server: isize) {
        if server != self.current.get() {
            return;
        }
        self.gldo(1, 0, 0);
        let alert = self.pref(P_AUTO_PING);
        let mut count: isize = 0;
        let list = self.h(Op::RDataObjects, &[r, &mut count as *mut isize as isize]);
        for i in 0..count {
            let link = self.h(Op::DataObjectLink, &[list, i]);
            let group = self.h(Op::ServerGroup, &[server, link]);
            if group == 0 {
                continue;
            }
            let game = self.hold(self.h(Op::GameCreate, &[]));
            self.h(Op::GameDo, &[game.p, 4, group]);
            if !self.b(Op::GameReadDataObject, &[game.p, list, i]) {
                continue;
            }
            self.gldo(6, game.p, 0);
            if alert {
                self.h(Op::PingGame, &[game.p]);
            }
        }
        self.h(Op::EvSyncGameList, &[]);
        self.have_game_list.set(true);
        let w = self.wait_op.get();
        if w != 0 {
            self.h(Op::OpFinish, &[w]);
        }
        self.h(Op::ListFree, &[list, 1]);
    }
    fn accept_client(&self, event: isize) {
        let current = self.current.get();
        if current == 0 || self.game.get() == 0 {
            return;
        }
        let conn = self.snum(current, S_CONNECTION);
        if conn == 0 || self.event_num(event, E_ACCEPT_CLIENT) == 0 {
            return;
        }
        let ec = self.event_num(event, E_ACCEPT_CLIENT);
        let client = self.h(Op::ServerClient, &[current, self.cid(ec) as isize]);
        if client != self.event_num(event, E_ACCEPT_CLIENT) {
            return;
        }
        let group = self.gid(self.game.get()) as isize;
        let id = self.cid(client) as isize;
        let accepted = self.event_num(event, E_ACCEPTED);
        self.h(Op::ConnAccept, &[conn, group, id, accepted]);
    }
    fn remove_friend(&self, f: isize) {
        if f == 0 {
            return;
        }
        let current = self.current.get();
        if current != 0 {
            let client = self.h(Op::ServerFindByWonId, &[current, self.user_id(f) as isize]);
            if client != 0 {
                self.h(Op::ClientDo, &[client, 3, 0]);
                self.ev_client(client, 4, 2);
            }
        }
        self.ev_friend(f, 1);
        self.h(Op::RemoveFriend, &[self.user_id(f) as isize]);
    }
    fn friend_action(&self, event: isize) {
        let f = self.event_num(event, E_FRIEND);
        if f == 0 {
            return;
        }
        let action = self.event_num(event, E_FRIEND_ACTION) as i32;
        let mut spec = self.event_num(event, E_FRIEND_VALUE) as i32;
        if spec == 0 {
            spec = 5;
        }
        match action {
            5 | 4 => {
                if action == 5 {
                    let current = self.current.get();
                    if current != 0 {
                        let client = self.h(Op::ServerFindByWonId, &[current, self.user_id(f) as isize]);
                        if client != 0 {
                            self.show_client_options(client, spec);
                            return;
                        }
                    }
                }
                self.h(Op::FriendOptions, &[f, spec as isize]);
            }
            1 => self.locate_friend(f, spec),
            2 => self.locate_friends(),
            3 => self.remove_friend(f),
            _ => {}
        }
    }
    fn select_game(&self, game_type: i32, ping_limit: u16) -> isize {
        let config = self.h(Op::ConfigPingCeiling, &[]);
        let ceiling = routing::ping_ceiling(config >= 0, if config >= 0 { config as u16 } else { 0 });
        let mut best: Option<(u16, isize)> = None;
        self.gldo(2, 0, 0);
        while self.gldo(3, 0, 0) != 0 {
            let game = self.gldo(4, 0, 0);
            let password = self.gmnum(game, GM_PASSWORD) != 0;
            let ask = !password && self.gmnum(game, GM_ASK_TO_JOIN) != 0;
            let can_join = !password && !ask && self.gmnum(game, GM_CAN_JOIN) != 0;
            let ping = self.gmnum(game, GM_PING) as u16;
            let skill = self.gmnum(game, GM_SKILL) as i32;
            let pick = routing::quick_play_rank(password, ask, can_join, ping, skill, game_type, ping_limit, ceiling);
            if pick.action == 1 {
                let key = pick.rank as u16;
                if best.is_none_or(|(k, _)| key < k) {
                    best = Some((key, game));
                }
            } else if pick.action == 2 {
                self.h(Op::PingGame, &[game]);
            }
        }
        best.map_or(0, |(_, g)| g)
    }
    fn jump_to_room(&self, event: isize) {
        let current = self.current.get();
        if self.pending.get() != 0 || current == 0 || !self.b(Op::EventAddressIs, &[event, current]) {
            return;
        }
        let group = self.h(Op::ServerGroup, &[current, self.event_num(event, E_JUMP_GROUP)]);
        if group == 0 {
            return;
        }
        if self.gnum(group, G_IS_GAME) != 0 {
            let game = self.gldo(5, self.event_num(event, E_JUMP_GROUP), 0);
            if game != 0 && self.b(Op::GameNameIsEvent, &[game, event]) {
                self.h(Op::JoinGame, &[game]);
            }
        } else {
            self.join_chat_room(self.current.get(), self.event_num(event, E_JUMP_GROUP) as u16);
        }
    }
    fn quick_play(&self, event: isize) {
        if self.lobby(L_LEAVE_GAME, 1) == 0 {
            return;
        }
        if self.current.get() == 0 {
            self.join_default_room(false);
        }
        if self.current.get() == 0 {
            return;
        }
        if !self.have_game_list.get() {
            let w = self.hold(self.h(Op::AsyncOpNew, &[]));
            self.assign(&self.wait_op, w.p);
            drop(w);
            self.run_status(
                self.wait_op.get(),
                Res::RoutingLogic_GettingGameListTitle_String,
                Res::RoutingLogic_GettingGameListDetails_String,
                1,
                0,
            );
            self.assign(&self.wait_op, 0);
            if !self.have_game_list.get() {
                return;
            }
        }
        loop {
            if self.current.get() == 0 {
                return;
            }
            let game = self.select_game(self.event_num(event, E_QUICK_PLAY_TYPE) as i32, 1000);
            if game != 0 {
                self.h(Op::JoinGame, &[game]);
                return;
            }
            if self.lobby(L_PINGS_TO_DO, 0) as i32 > 0 {
                let w = self.hold(self.h(Op::AsyncOpNew, &[]));
                self.run_status(
                    w.p,
                    Res::RoutingLogic_CollectingPingsTitle_String,
                    Res::RoutingLogic_CollectingPingsDetails_String,
                    2,
                    1000,
                );
                if self.b(Op::OpKilled, &[w.p]) {
                    return;
                }
            } else {
                let answer = self.h(
                    Op::YesNo,
                    &[
                        res(Res::GameStagingLogic_NoQuickPlayGameTitle_String),
                        res(Res::GameStagingLogic_NoQuickPlayGameDetails_String),
                    ],
                );
                if answer == DIALOG_OK {
                    self.lobby(L_HOST_GAME, 0);
                }
                return;
            }
        }
    }
    fn client_action(&self, event: isize) {
        let current = self.current.get();
        if current == 0 {
            return;
        }
        let conn = self.snum(current, S_CONNECTION);
        if conn == 0 {
            return;
        }
        if self.event_num(event, E_CA_SPEC) & 4 == 0 {
            return;
        }
        let client = self.event_num(event, E_CA_CLIENT);
        if client == 0 {
            return;
        }
        match self.event_num(event, E_CA_ACTION) as i32 {
            18 => self.show_client_options(client, self.event_num(event, E_CA_SPEC) as i32),
            2 | 7 | 8 | 14 | 15 => {
                let action = self.event_num(event, E_CA_ACTION) as i32;
                self.do_admin_action(action, self.event_num(event, E_CA_SPEC) as i32, client)
            }
            1 => {
                if self.won_id(client) == 0 || self.cb(client, C_FRIEND) || !self.can_add_friend() {
                    return;
                }
                self.h(Op::ClientDo, &[client, 3, 1]);
                self.ev_client(client, 4, 2);
                let f = self.h(Op::AddFriendOfClient, &[client]);
                if f == 0 {
                    return;
                }
                let here = self.h(Op::ServerFindByWonId, &[self.current.get(), self.won_id(client) as isize]);
                if here != 0 {
                    self.fdo(f, 3, self.current.get(), 0);
                    self.fdo(f, 2, here, 0);
                }
                self.ev_friend(f, 0);
            }
            9 => {
                if !self.cb(client, C_FRIEND) {
                    return;
                }
                self.h(Op::ClientDo, &[client, 3, 0]);
                self.ev_client(client, 4, 2);
                let f = self.h(Op::FindFriend, &[self.won_id(client) as isize]);
                if f != 0 {
                    self.ev_friend(f, 1);
                }
                self.h(Op::RemoveFriend, &[self.won_id(client) as isize]);
            }
            3 => {
                self.h(Op::SetBlocked, &[client, 1]);
            }
            4 => {
                self.suppress_idle.set(false);
                self.set_client_flags(conn, AWAY, 0, 0, 0);
            }
            5 => {
                self.h(Op::SetIgnored, &[client, 1]);
            }
            6 => {
                self.invite_client(client, true, 0);
            }
            13 => {
                self.invite_client(client, false, 0);
            }
            10 => {
                self.suppress_idle.set(true);
                self.set_client_flags(conn, AWAY, AWAY, 0, 0);
            }
            11 => {
                self.h(Op::SetBlocked, &[client, 0]);
            }
            12 => {
                self.h(Op::SetIgnored, &[client, 0]);
            }
            16 => {
                self.h(Op::EvSetChatInput, &[client, self.event_num(event, E_CA_SPEC)]);
            }
            17 => {
                if self.cb(client, C_FRIEND) {
                    let f = self.h(Op::FindFriend, &[self.won_id(client) as isize]);
                    if f != 0 {
                        self.locate_friend(f, self.event_num(event, E_CA_SPEC) as i32);
                        return;
                    }
                }
                self.announce_client_location(client, true, self.event_num(event, E_CA_SPEC) as i32, true);
            }
            _ => {}
        }
    }
    fn invite_client(&self, client: isize, invite: bool, comment: isize) -> bool {
        if client == 0 || self.game.get() == 0 {
            return false;
        }
        let mine = self.h(Op::MyGame, &[]);
        if mine == 0 || self.gmnum(mine, GM_IS_HOST) == 0 {
            return false;
        }
        let conn = self.connection();
        if conn == 0 {
            return false;
        }
        self.h(Op::ClientDo, &[client, 4, invite as isize]);
        let group = self.gid(self.game.get()) as isize;
        let id = self.cid(client) as isize;
        self.h(Op::ConnInvite, &[conn, group, id, invite as isize, comment]);
        self.h(Op::EvInvitationSent, &[self.game.get(), client, invite as isize, comment]);
        true
    }
    // the original's switch, a command's target checked in its case
    #[allow(clippy::collapsible_match)]
    fn chat_input(&self, target: isize) {
        let current = self.current.get();
        if current == 0 {
            return;
        }
        let conn = self.snum(current, S_CONNECTION);
        if conn == 0 {
            return;
        }
        let parser = self.h(Op::Parser, &[]);
        if parser == 0 || self.h(Op::ParserNum, &[parser, 0]) & 4 == 0 {
            return;
        }
        let group = self.group_of_spec(self.h(Op::ParserNum, &[parser, 0]) as i32);
        if group == 0 {
            return;
        }
        let gid = self.gid(group) as isize;
        let who = self.h(Op::ParserNum, &[parser, 3]);
        let duration = || self.h(Op::ParserNum, &[parser, 2]) as u32 as isize;
        let text = || self.text(self.h(Op::ParserText, &[parser]));
        let room_flags = || self.h(Op::ParserNum, &[parser, 0]) as u32 as i32;
        let mute = |group: isize, on: bool| {
            let d = duration();
            let t = text();
            self.h(Op::ConnMute, &[conn, group | (on as isize) << 16, d, t.p, self.cid(who) as isize]);
        };
        let ban = |group: isize| {
            let d = duration();
            let t = text();
            self.h(Op::ConnBan, &[conn, group | 1 << 16, d, t.p, self.cid(who) as isize]);
        };
        let admin = || {
            let mine = self.snum(self.current.get(), S_MY_CLIENT);
            mine != 0 && self.cb(mine, C_ADMIN)
        };
        match self.h(Op::ParserNum, &[parser, 1]) as i32 {
            1 | 3 => {
                let flags = if self.h(Op::ParserNum, &[parser, 1]) == 3 { 2 } else { 0 };
                if self.h(Op::ParserNum, &[parser, 0]) & 0x10 != 0 {
                    let mine = self.h(Op::MyClient, &[]);
                    if mine == 0 {
                        return;
                    }
                    let mut ids: Vec<u16> = Vec::new();
                    let it = self.h(Op::IterNew, &[self.h(Op::ParserNum, &[parser, 4])]);
                    while self.h(Op::IterHasMore, &[it]) != 0 {
                        let c = self.h(Op::IterNext, &[it]);
                        if self.b(Op::ClientIsTeammate, &[mine, c]) {
                            ids.push(self.cid(c));
                        }
                    }
                    let t = text();
                    self.h(Op::ConnSendChatTo, &[conn, t.p, flags | 0x100, ids.len() as isize, ids.as_ptr() as isize]);
                    drop(t);
                    self.h(Op::ListFree, &[it, 3]);
                } else {
                    let t = text();
                    self.h(Op::ConnSendChat, &[conn, t.p, flags, gid]);
                }
            }
            2 => {
                if who != 0 {
                    let t = text();
                    self.h(Op::ConnSendChat, &[conn, t.p, 4, self.cid(who) as isize]);
                }
            }
            6 => {
                let mine = self.snum(self.current.get(), S_MY_CLIENT);
                if mine == 0 {
                    return;
                }
                self.suppress_idle.set(!self.cb(mine, C_AWAY));
                self.set_client_flags(conn, AWAY, if self.suppress_idle.get() { 8 } else { 0 }, 0, 0);
            }
            16 | 17 => {
                if who != 0 {
                    let invite = self.h(Op::ParserNum, &[parser, 1]) == 16;
                    let t = text();
                    if !self.invite_client(who, invite, t.p) {
                        drop(t);
                        self.h(Op::EvNotCaptain, &[target]);
                    }
                }
            }
            7 => {
                let mine = self.snum(self.current.get(), S_MY_CLIENT);
                if mine == 0 {
                    return;
                }
                self.want_moderator.set(!self.cb(mine, C_MODERATOR));
                self.conn_do(conn, 7, self.want_moderator.get() as isize);
            }
            10 => {
                if who != 0 {
                    mute(gid, true);
                }
            }
            8 => {
                if who != 0 {
                    mute(0xffff, true);
                }
            }
            11 => {
                if who != 0 {
                    ban(gid);
                }
            }
            9 => {
                if who != 0 {
                    ban(0xffff);
                }
            }
            12 => {
                if who != 0 {
                    mute(gid, false);
                } else {
                    self.edit_bad_user_list(true, false, room_flags());
                }
            }
            13 => {
                if who != 0 {
                    mute(0xffff, false);
                } else {
                    self.edit_bad_user_list(true, true, room_flags());
                }
            }
            5 => {
                if who != 0 {
                    self.h(Op::SetIgnored, &[who, !self.cb(who, C_IGNORED) as isize]);
                }
            }
            4 => {
                if who != 0 {
                    self.h(Op::SetBlocked, &[who, !self.cb(who, C_BLOCKED) as isize]);
                }
            }
            14 => self.edit_bad_user_list(false, false, room_flags()),
            15 => self.edit_bad_user_list(false, true, room_flags()),
            22 => {
                if admin() {
                    let t = text();
                    self.h(Op::ConnAlert, &[conn, t.p]);
                }
            }
            23 => {
                if admin() {
                    let t = text();
                    let d = duration();
                    self.h(Op::ConnStartShutdown, &[conn, t.p, d]);
                }
            }
            24 => {
                if admin() {
                    self.conn_do(conn, 6, 0);
                }
            }
            25 => {
                if who != 0 {
                    let t = text();
                    self.h(Op::ConnSendChat, &[conn, t.p, 0x10, self.cid(who) as isize]);
                }
            }
            _ => {}
        }
    }
    fn connection_closed(&self, op: isize) {
        let socket = self.h(Op::OpSocket, &[op]);
        if socket != self.connection() {
            return;
        }
        self.ev_left_room(4, true, true);
        if self.game.get() != 0 {
            self.lobby(L_GAME_DISCONNECTED, 0);
        }
        self.leave_server();
        self.message_box(Res::RoutingLogic_DisconnectedTitle_String, Res::RoutingLogic_DisconnectedDetails_String);
    }
    fn create_room(&self, event: isize) {
        self.h(Op::DialogReset, &[D_CREATE_ROOM, 0]);
        if self.dialog_do(D_CREATE_ROOM) != DIALOG_OK || self.pending.get() != 0 {
            return;
        }
        let name = self.text(self.h(Op::DialogText, &[1]));
        let password = self.text(self.h(Op::DialogText, &[2]));
        if self.h(Op::TextLength, &[name.p]) == 0 {
            return;
        }
        let mut server = self.h(Op::EventServer, &[event]);
        if server == 0 {
            server = self.current.get();
        }
        if server == 0 {
            server = self.default_server();
        }
        if server == 0 {
            return;
        }
        self.with_pending(|| {
            if !self.connect_to_server(server, false) {
                return;
            }
            let mut id = 0u16;
            if !self.create_group(self.pending.get(), name.p, password.p, 0x800, &mut id) {
                return;
            }
            self.joined_group(id, false);
            if self.h(Op::DialogNum, &[3]) != 0 {
                self.h(Op::RoomDialogEnd, &[]);
            }
        });
    }
    fn join_chat_room(&self, server: isize, id: u16) -> bool {
        if server == 0 {
            return false;
        }
        self.with_pending(|| {
            if !self.connect_to_server(server, false) {
                return false;
            }
            let chat = self.chat.get();
            if chat != 0 && server == self.current.get() && id == self.gid(chat) {
                self.message_box(Res::RoutingLogic_AlreadyJoinedTitle_String, Res::RoutingLogic_AlreadyInRoom_String);
                return false;
            }
            if !self.join_group(self.pending.get(), id as i32) {
                return false;
            }
            self.joined_group(id, false);
            true
        })
    }
    fn join_room(&self, event: isize) {
        if self.pending.get() != 0 {
            return;
        }
        let server = self.h(Op::EventServer, &[event]);
        if server == 0 {
            return;
        }
        let mut id = self.event_num(event, E_JOIN_GROUP) as u32;
        if id == 0xffff {
            id = 0;
        }
        if self.join_chat_room(server, id as u16) && self.h(Op::DialogNum, &[3]) != 0 {
            self.h(Op::RoomDialogEnd, &[]);
        }
    }
    fn join_friends_room(&self, event: isize) {
        if self.pending.get() != 0 {
            return;
        }
        let f = self.event_num(event, E_JOIN_FRIEND);
        if f == 0 {
            return;
        }
        if !self.b(Op::FriendAddressValid, &[f]) {
            return;
        }
        let server = self.h(Op::FriendServer, &[f]);
        if server == 0 {
            return;
        }
        self.with_pending(|| {
            if !self.connect_to_server(server, false) {
                return;
            }
            let client = self.h(Op::ServerFindByWonId, &[server, self.user_id(f) as isize]);
            if client == 0 {
                return;
            }
            let mut id: u16 = 0;
            if self.cb(client, C_IN_CHAT) {
                id = self.cnum(client, C_CHAT_ID) as u16;
            }
            let chat = self.chat.get();
            if chat != 0 && server == self.current.get() && id == self.gid(chat) {
                self.message_box(Res::RoutingLogic_AlreadyJoinedTitle_String, Res::RoutingLogic_AlreadyInRoom_String);
                return;
            }
            if !self.join_group(server, id as i32) {
                return;
            }
            self.joined_group(id, false);
        });
    }
    fn create_group(&self, server: isize, name: isize, password: isize, flags: i32, out: &mut u16) -> bool {
        if server != 0 {
            let conn = self.take(self.snum(server, S_CONNECTION));
            if conn.p == 0 {
                return false;
            }
            let op = self.hold(self.h(Op::RNew, &[conn.p, RT_CREATE_GROUP]));
            self.rtext(op.p, EE_WON_RTT_PASSWORD, password);
            self.rtext(op.p, EE_WON_RTT_NAME, name);
            self.rset(op.p, EE_WON_RTF_GROUP_FLAGS, flags as u32 as isize);
            self.rset(op.p, EE_WON_RTF_JOIN_FLAGS, 1);
            self.rset(op.p, EE_WON_RTF_LEAVE_ON_CANCEL, 1);
            self.run_status(
                op.p,
                Res::RoutingLogic_CreateGroupTitle_String,
                Res::RoutingLogic_CreateGroupDetails_String,
                1,
                0,
            );
            if self.status(op.p) == 0 {
                *out = self.rgroup(op.p);
                return true;
            }
        }
        false
    }
    fn ask_password(&self) -> Option<Held<'_>> {
        self.h(Op::DialogReset, &[D_PASSWORD]);
        if self.dialog_do(D_PASSWORD) != DIALOG_OK {
            return None;
        }
        Some(self.text(self.h(Op::DialogText, &[3])))
    }
    fn join_group(&self, server: isize, id: i32) -> bool {
        if server == 0 {
            return false;
        }
        let conn = self.take(self.snum(server, S_CONNECTION));
        if conn.p == 0 {
            return false;
        }
        let group = self.take(self.h(Op::ServerGroup, &[server, id as isize]));
        if group.p == 0 {
            self.h(Op::ErrorDialog, &[res(Res::RoutingLogic_JoinGroupTitle_String), -2520]);
            return false;
        }
        let mut password = self.text(0);
        if self.gnum(group.p, G_PASSWORD) != 0 {
            match self.ask_password() {
                Some(t) => password = t,
                None => return false,
            }
        }
        if self.conn_do(conn.p, 5, 0) == 0 {
            return false;
        }
        let op = self.hold(self.h(Op::RNew, &[conn.p, RT_JOIN_GROUP]));
        self.rset(op.p, EE_WON_RTF_GROUP_ID, id as u16 as isize);
        self.rtext(op.p, EE_WON_RTT_PASSWORD, password.p);
        loop {
            if group.p != 0 && self.gnum(group.p, G_ASK_TO_JOIN) != 0 {
                self.run_status(
                    op.p,
                    Res::RoutingLogic_AskingCaptainTitle_String,
                    Res::RoutingLogic_AskingCaptainDetails_String,
                    1,
                    0,
                );
            } else {
                self.run_status(
                    op.p,
                    Res::RoutingLogic_JoinGroupTitle_String,
                    Res::RoutingLogic_JoinGroupDetails_String,
                    1,
                    0,
                );
            }
            if self.status(op.p) == 0 {
                break;
            }
            if self.conn_do(conn.p, 5, 0) == 0 {
                return false;
            }
            if self.b(Op::OpKilled, &[op.p]) {
                self.conn_do(conn.p, 9, id as u16 as isize);
                return false;
            }
            if self.status(op.p) == -2524 {
                let Some(t) = self.ask_password() else { return false };
                self.rtext(op.p, EE_WON_RTT_PASSWORD, t.p);
            } else {
                return self.status(op.p) == -2504;
            }
        }
        true
    }
    fn show_client_options(&self, client: isize, spec: i32) {
        let current = self.current.get();
        if current == 0 || client == 0 {
            return;
        }
        let mine = self.snum(current, S_MY_CLIENT);
        if mine == 0 {
            return;
        }
        self.h(Op::ClientOptions, &[client, mine, spec as isize]);
    }
    fn set_moderation_target(&self, op: isize, here: isize, client: isize) {
        if here != 0 {
            self.rset(op, EE_WON_RTF_CLIENT_ID, self.cid(here) as isize);
            self.rset(op, EE_WON_RTF_BY_WON_ID, 0);
            return;
        }
        let mut id = self.cnum(client, C_CDKEY_ID) as u32;
        if id == 0 {
            id = self.won_id(client);
        }
        if id == 0 {
            return;
        }
        self.rset(op, EE_WON_RTF_WON_ID, id as isize);
        self.rset(op, EE_WON_RTF_BY_WON_ID, 1);
        self.rset(op, EE_WON_RTF_IS_CDKEY, 1);
    }
    fn do_admin_action(&self, action: i32, spec: i32, client: isize) {
        let mut conn = self.connection();
        if conn == 0 {
            return;
        }
        let mine = self.snum(self.current.get(), S_MY_CLIENT);
        if mine == 0 {
            return;
        }
        let show = self.cb(mine, C_MODERATOR) || self.cb(mine, C_ADMIN);
        if show {
            self.h(Op::DialogReset, &[D_ADMIN, action as isize, client]);
            if self.dialog_do(D_ADMIN) != DIALOG_OK {
                return;
            }
            conn = self.connection();
            if conn == 0 {
                return;
            }
        }
        let here = self.h(Op::ServerFindByWonId, &[self.current.get(), self.won_id(client) as isize]);
        let mut group: i32 = 0xffff;
        if spec & 2 != 0 {
            let g = self.game.get();
            if g == 0 {
                return;
            }
            group = self.gid(g) as i32;
        } else if spec & 1 != 0 {
            let c = self.chat.get();
            if c == 0 {
                return;
            }
            group = self.gid(c) as i32;
        }
        let mut explanation = self.text(0);
        let seconds: u32;
        if show {
            if self.h(Op::DialogNum, &[1]) != 0 {
                group = 0xffff;
            } else if group == 0xffff && action != 15 {
                return;
            }
            explanation = self.text(self.h(Op::DialogText, &[4]));
            seconds = self.h(Op::DialogNum, &[2]) as u32;
        } else {
            if group == 0xffff {
                return;
            }
            seconds = (action == 7) as u32;
        }
        let mut title = Res::RoutingLogic_BanClientTitle_String;
        let op = match action {
            2 | 7 => {
                if !show && spec & 2 != 0 {
                    self.h(Op::KickClient, &[client, (action == 2) as isize]);
                    return;
                }
                let ban = self.hold(self.h(Op::RNew, &[conn, RT_BAN_CLIENT]));
                if action == 2 {
                    self.rtext(ban.p, EE_WON_RTT_COMMENT, explanation.p);
                    self.rset(ban.p, EE_WON_RTF_TIME, seconds as isize);
                } else {
                    self.rset(ban.p, EE_WON_RTF_TIME, 1);
                    title = Res::RoutingLogic_KickClientTitle_String;
                }
                self.rset(ban.p, EE_WON_RTF_GROUP_ID, group as u16 as isize);
                self.set_moderation_target(ban.p, here, client);
                ban
            }
            8 | 14 => {
                let mute = self.hold(self.h(Op::RNew, &[conn, RT_MUTE_CLIENT]));
                self.rtext(mute.p, EE_WON_RTT_COMMENT, explanation.p);
                self.rset(mute.p, EE_WON_RTF_TIME, seconds as isize);
                self.rset(mute.p, EE_WON_RTF_GROUP_ID, group as u16 as isize);
                self.set_moderation_target(mute.p, here, client);
                if action == 14 {
                    title = Res::RoutingLogic_UnmuteClientTitle_String;
                    self.rset(mute.p, EE_WON_RTF_ON, 0);
                } else {
                    title = Res::RoutingLogic_MuteClientTitle_String;
                    self.rset(mute.p, EE_WON_RTF_ON, 1);
                }
                mute
            }
            15 => {
                if here != 0 {
                    self.h(Op::ConnSendChat, &[conn, explanation.p, 0x10, self.cid(here) as isize]);
                }
                return;
            }
            _ => return,
        };
        if show {
            self.run_status(op.p, title, Res::RoutingLogic_ModerateDetails_String, 1, 0);
        } else {
            self.h(Op::RunOp, &[op.p]);
        }
    }
    fn edit_bad_user_list(&self, muted: bool, server_wide: bool, spec: i32) {
        let mut conn = self.connection();
        if conn == 0 {
            return;
        }
        let mine = self.snum(self.current.get(), S_MY_CLIENT);
        if mine == 0 {
            return;
        }
        let moderator = self.cb(mine, C_MODERATOR) || self.cb(mine, C_ADMIN);
        let list_type: isize = if muted { 2 } else { 1 };
        let mut group: u16 = 0xffff;
        if !server_wide {
            let g = self.group_of_spec(spec);
            if g == 0 {
                return;
            }
            group = self.gid(g);
        }
        let op = self.hold(self.h(Op::RNew, &[conn, RT_GET_BAD_USER_LIST]));
        self.rset(op.p, EE_WON_RTF_LIST_TYPE, list_type);
        self.rset(op.p, EE_WON_RTF_GROUP_ID, group as isize);
        self.rset(op.p, EE_WON_RTF_FLAGS8, 0x0f);
        self.run_status(
            op.p,
            Res::RoutingLogic_GetBadUsersTitle_String,
            Res::RoutingLogic_GetBadUsersDetails_String,
            1,
            0,
        );
        if self.status(op.p) != 0 {
            return;
        }
        self.h(Op::BadUserSet, &[op.p, !moderator as isize]);
        if self.dialog_do(D_BAD_USER) != DIALOG_OK {
            return;
        }
        conn = self.connection();
        if conn == 0 {
            return;
        }
        let mut ids: Vec<u64> = Vec::new();
        self.h(Op::BadUserIds, &[&mut ids as *mut Vec<u64> as isize]);
        for id in ids {
            if list_type == 1 {
                self.h(Op::ConnBanByWonId, &[conn, group as isize, id as isize]);
            } else if list_type == 2 {
                self.h(Op::ConnMuteByWonId, &[conn, group as isize, id as isize]);
            }
        }
    }
    fn locate_friend(&self, f: isize, spec: i32) {
        if f == 0 {
            return;
        }
        if !self.should_locate_friend(f, self.h(Op::Now, &[]) as u32) {
            if self.fnum(f, F_FLAGS) & 4 == 0 {
                self.announce_friend_location(f, spec);
            }
            return;
        }
        let manager = self.h(Op::UdpManager, &[]);
        if manager == 0 {
            return;
        }
        let mut count: i32 = 0;
        let mut i = 0;
        while i < self.h(Op::ListNum, &[0]) {
            let current = self.current.get();
            if current != 0 && self.b(Op::ListAddressIs, &[i, current]) {
                i += 1;
                continue;
            }
            let op = self.hold(self.h(Op::StatusOpNew, &[i, manager]));
            self.h(Op::StatusOpAddId, &[op.p, self.user_id(f) as isize]);
            self.h(Op::StatusOpRun, &[op.p]);
            count += 1;
            i += 1;
        }
        if count <= 0 {
            return;
        }
        self.fdo(f, 5, spec as isize, 0);
        self.fdo(f, 6, count as isize, 0);
        self.ev_friend(f, 2);
    }
    fn user_status_done(&self, op: isize) {
        let server = self.h(Op::StatusOpServer, &[op]);
        let now = self.h(Op::Now, &[]) as u32;
        let last = Cell::new(0);
        let mut changed = 0;
        let mut statuses: Vec<u64> = Vec::new();
        self.h(Op::StatusOpMap, &[op, &mut statuses as *mut Vec<u64> as isize]);
        for pair in statuses.chunks(2) {
            let (id, status) = (pair[0], pair[1] as u8);
            let f = self.h(Op::FindFriend, &[id as isize]);
            if f == 0 || self.fnum(f, F_FLAGS) & 4 == 0 {
                continue;
            }
            let mut found = false;
            if status != 0 && server != 0 {
                self.fdo(f, 3, server, status as isize);
                found = true;
            }
            let searches = (self.fnum(f, F_LAST_SEARCH) as u32).wrapping_sub(1);
            self.fdo(f, 6, searches as isize, 0);
            let remaining = self.fnum(f, F_LAST_SEARCH) as u32 as i32;
            if remaining > 0 && !found {
                continue;
            }
            self.assign(&last, f);
            changed += 1;
            self.fdo(f, 7, now as isize, 0);
            if self.fnum(f, F_SEARCH_START) != 0 {
                let start = self.fnum(f, F_SEARCH_START);
                self.h(Op::EvAnnounceFriend, &[f, start, 2]);
            }
        }
        if changed > 1 {
            self.h(Op::BroadcastLobby, &[100056]);
        } else if changed == 1 && last.get() != 0 {
            self.ev_friend(last.get(), 2);
        }
        self.assign(&last, 0);
    }
    fn locate_friends(&self) {
        let manager = self.h(Op::UdpManager, &[]);
        if manager == 0 {
            return;
        }
        let now = self.h(Op::Now, &[]) as u32;
        let others = || {
            let mut list = Vec::new();
            let mut i = 0;
            while i < self.h(Op::ListNum, &[0]) {
                let current = self.current.get();
                if !(current != 0 && self.b(Op::ListAddressIs, &[i, current])) {
                    list.push(i);
                }
                i += 1;
            }
            list
        };
        let count = others().len();
        if count == 0 {
            return;
        }
        let mut requested: Vec<u32> = Vec::new();
        let mut i = 0;
        while i < self.h(Op::FriendCount, &[]) {
            let f = self.h(Op::FriendAt, &[i]);
            if self.should_locate_friend(f, now) {
                self.fdo(f, 5, 0, 0);
                self.fdo(f, 6, count as isize, 0);
                let id = self.user_id(f);
                if let Err(at) = requested.binary_search(&id) {
                    requested.insert(at, id);
                }
            }
            i += 1;
        }
        if requested.is_empty() {
            return;
        }
        let mut i = 0;
        while i < self.h(Op::ListNum, &[0]) {
            let current = self.current.get();
            if current != 0 && self.b(Op::ListAddressIs, &[i, current]) {
                i += 1;
                continue;
            }
            let op = self.hold(self.h(Op::StatusOpNew, &[i, manager]));
            self.h(Op::StatusOpSetMap, &[op.p, requested.as_ptr() as isize, requested.len() as isize]);
            self.h(Op::StatusOpRun, &[op.p]);
            i += 1;
        }
        self.h(Op::BroadcastLobby, &[100056]);
    }
    fn connect_to_server(&self, server: isize, flag: bool) -> bool {
        if server == 0 {
            return false;
        }
        self.assign(&self.pending, server);
        if server == self.current.get() && self.snum(server, S_CONNECTED) != 0 {
            return true;
        }
        if self.game.get() != 0 && self.lobby(L_LEAVE_GAME, 1) == 0 {
            return false;
        }
        let conn = self.hold(self.h(Op::ConnNew, &[]));
        self.snum_do(server, 8, conn.p);
        let auth = self.h(Op::AuthContext, &[]);
        if auth == 0 {
            return false;
        }
        self.conn_do(conn.p, 1, auth);
        self.conn_do(conn.p, 2, 0);
        let connect = self.hold(self.h(Op::ConnectOpNew, &[conn.p, server]));
        self.run_status(
            connect.p,
            Res::RoutingLogic_ConnectTitle_String,
            Res::RoutingLogic_ConnectDetails_String,
            1,
            0,
        );
        if self.status(connect.p) != 0 {
            return false;
        }
        let register_flags = if self.want_moderator.get() { 0x1d } else { 0x15 };
        let register = self.hold(self.h(Op::RNew, &[conn.p, RT_REGISTER_CLIENT]));
        self.rset(register.p, EE_WON_RTF_REGISTER_FLAGS, register_flags);
        let reconnect = self.h(Op::ReconnectId, &[server]) as u32;
        self.rset(register.p, EE_WON_RTF_RECONNECT_ID, reconnect as isize);
        let mut client_flags = 0;
        if !self.pref(P_FILTER) {
            client_flags = 4;
        }
        if self.pref(P_ANONYMOUS) || flag {
            client_flags |= 0x40;
        }
        self.rset(register.p, EE_WON_RTF_CLIENT_FLAGS, client_flags);
        self.run_status(
            register.p,
            Res::RoutingLogic_RegisterTitle_String,
            Res::RoutingLogic_RegisterDetails_String,
            1,
            0,
        );
        if self.status(register.p) != 0 {
            return false;
        }
        let reconnect = self.rnum(register.p, EE_WON_RTF_RECONNECT_ID) as u32;
        self.h(Op::SetReconnectId, &[server, reconnect as isize]);
        self.h(Op::ServerSyncClients, &[server, register.p]);
        self.snum_do(server, 7, self.rclient(register.p) as isize);
        self.conn_do(conn.p, 3, 0);
        if flag && !self.pref(P_ANONYMOUS) {
            self.set_client_flags(conn.p, 0x40, 0, 0, 0);
        }
        let groups = self.hold(self.h(Op::RNew, &[conn.p, RT_GET_GROUP_LIST]));
        self.rset(groups.p, EE_WON_RTF_FLAGS16, 0xff);
        self.run_status(
            groups.p,
            Res::RoutingLogic_QueryGroupsTitle_String,
            Res::RoutingLogic_QueryGroupsDetails_String,
            1,
            0,
        );
        if self.status(groups.p) != 0 {
            return false;
        }
        self.h(Op::ServerSyncGroups, &[server, groups.p, 1]);
        self.h(Op::EvServerChanged, &[server, 2]);
        self.snum_do(server, 3, 0);
        self.h(Op::BroadcastLobby, &[100056]);
        true
    }
    fn add_friend(&self) {
        if !self.can_add_friend() {
            return;
        }
        self.h(Op::DialogReset, &[D_ADD_FRIEND]);
        if self.dialog_do(D_ADD_FRIEND) != DIALOG_OK {
            return;
        }
        let servers = self.h(Op::AccountServers, &[]);
        if servers == 0 {
            return;
        }
        let name = self.text(self.h(Op::DialogText, &[5]));
        let mut status: i32 = 0;
        let id = self.h(Op::UserIdLookup, &[servers, name.p, &mut status as *mut i32 as isize]) as u32;
        if status != 0 || id == 0 {
            return;
        }
        let f = self.h(Op::AddFriendNamed, &[name.p, id as isize]);
        if f != 0 {
            let current = self.current.get();
            if current != 0 {
                let client = self.h(Op::ServerFindByWonId, &[current, self.user_id(f) as isize]);
                if client != 0 {
                    self.h(Op::ClientDo, &[client, 3, 1]);
                    self.fdo(f, 3, self.current.get(), 0);
                    self.fdo(f, 2, client, 0);
                    self.ev_client(client, 4, 2);
                }
            }
            self.ev_friend(f, 0);
        }
    }
    fn query_server_groups_done(&self, op: isize) {
        let servers = self.h(Op::LobbyServerList, &[]);
        if servers == 0 {
            return;
        }
        let server = self.h(Op::ServerListFind, &[servers, op]);
        if server == 0 {
            return;
        }
        self.snum_do(server, 5, 0);
        if self.status(op) != 0 {
            self.last_server_refresh.set(0);
            self.snum_do(server, 6, 0);
        }
        self.h(Op::SyncGroupsOfQuery, &[server, op]);
        self.h(Op::EvServerChanged, &[server, 2]);
    }
    fn handle_query_server_groups(&self, event: isize) {
        let server = self.h(Op::EventServer, &[event]);
        if server == 0 {
            return;
        }
        let now = self.h(Op::Now, &[]) as i64;
        if self.snum(server, S_CONNECTED) != 0
            || self.snum(server, S_QUERYING) != 0
            || ((now as u32).wrapping_sub(self.snum(server, S_LAST_PING) as u32) as i32) < 30
        {
            return;
        }
        self.snum_do(server, 4, 0);
        self.h(Op::EvServerChanged, &[server, 2]);
        self.snum_do(server, 5, 1);
        self.snum_do(server, 6, now as u32 as isize);
        self.h(Op::GroupQueryRun, &[server]);
    }
    fn refresh_server_list(&self) {
        let context = self.hold(self.h(Op::RoomDirServers, &[]));
        let config = self.b(Op::HasConfig, &[]);
        if context.p == 0 || !config {
            return;
        }
        let refresh = self.h(Op::Now, &[]) as i64;
        if ((refresh as u32).wrapping_sub(self.last_server_refresh.get() as u32) as i32) >= 300
            || self.h(Op::ListNum, &[1]) != 0
        {
            let op = self.hold(self.h(Op::DirOpRun, &[context.p]));
            if self.status(op.p) != 0 {
                return;
            }
            let mut empty: isize = 0;
            let entities = self.h(Op::DirEntities, &[op.p, &mut empty as *mut isize as isize]);
            if empty != 0 {
                self.h(Op::DirNotifyFailed, &[context.p, op.p]);
            }
            self.h(Op::DirUpdate, &[entities]);
            self.h(Op::ListFree, &[entities, 2]);
            self.last_server_refresh.set(refresh);
        }
        let now = self.h(Op::Now, &[]) as i64;
        let mut i = 0;
        while i < self.h(Op::ListNum, &[0]) {
            let s = self.h(Op::ListAt, &[i]);
            if self.snum(s, S_CONNECTED) == 0
                && ((now as u32).wrapping_sub(self.snum(s, S_LAST_PING) as u32) as i32) >= 30
            {
                self.snum_do(s, 4, 0);
            }
            i += 1;
        }
        self.h(Op::BroadcastLobby, &[100053]);
    }
    fn enter_internet_screen(&self, join: bool) {
        self.refresh_server_list();
        if self.h(Op::ListNum, &[1]) != 0 {
            self.message_box(Res::RoutingLogic_NoServersTitle_String, Res::RoutingLogic_NoServersDetails_String);
            return;
        }
        if join {
            if !self.join_default_room(false) {
                if self.pref(P_CANCELLED) {
                    self.lobby(L_LOGOUT, 0);
                    return;
                }
                self.dialog_do(D_ROOM);
            }
        } else {
            self.dialog_do(D_ROOM);
        }
        self.lobby(L_SHOW_INTERNET, 0);
        if self.pref(P_LOBBY_SOUND) && !self.pref(P_LOBBY_MUSIC) {
            self.sound(Res::LobbyGlobal_SierraLogon_Sound);
        }
    }
    fn client_flag_setting(&self, dirty_words: bool) {
        let conn = self.connection();
        if conn == 0 {
            return;
        }
        let (mask, flags) = if dirty_words {
            routing::dirty_word_flags(self.pref(P_FILTER))
        } else {
            routing::anonymous_flags(self.pref(P_ANONYMOUS))
        };
        self.set_client_flags(conn, mask as isize, flags as isize, 0, 0);
    }
    fn can_add_friend(&self) -> bool {
        if !routing::can_add_friend(self.h(Op::FriendCount, &[]) as usize) {
            self.message_box(
                Res::RoutingLogic_TooManyFriendsTitle_String,
                Res::RoutingLogic_TooManyFriendsDetails_String,
            );
            return false;
        }
        true
    }
    fn logout(&self) {
        let auth = self.h(Op::AuthContext, &[]);
        if auth != 0 {
            self.h(Op::AuthStopRefresh, &[auth]);
        }
        self.want_moderator.set(false);
        self.h(Op::DialogReset, &[D_CREATE_ROOM, 1]);
        self.clear_pending();
        self.leave_server();
    }
    fn control(&self, which: isize) {
        match which {
            1 => {
                self.refresh_server_list();
                self.dialog_do(D_ROOM);
            }
            2 => self.add_friend(),
            3 => self.client_flag_setting(false),
            4 => self.client_flag_setting(true),
            5 => self.locate_friends(),
            6 => self.refresh_game_list(),
            _ => {}
        }
    }
    fn event(&self, event: isize, kind: isize) {
        match kind {
            100002 => self.accept_client(event),
            100005 => self.client_action(event),
            100011 => self.create_room(event),
            100012 => self.chat_input(self.event_num(event, E_COMPONENT)),
            100015 => self.friend_action(event),
            100028 => self.join_room(event),
            100026 => self.join_friends_room(event),
            100031 => self.jump_to_room(event),
            100042 => self.handle_query_server_groups(event),
            100043 => self.quick_play(event),
            _ => {}
        }
    }
    fn notify(&self, code: isize, op: isize) {
        if self.b(Op::Killed, &[]) || self.b(Op::OpKilled, &[op]) {
            return;
        }
        let server = self.server_of_op(op);
        if server == 0 {
            return;
        }
        match code {
            N_CLIENT_JOINED_SERVER => self.client_joined_server(op, server),
            N_CLIENT_LEFT_SERVER => self.client_left_server(op, server),
            N_CLIENT_JOINED_GROUP => self.client_joined_group(op, server),
            N_CLIENT_LEFT_GROUP => self.client_left_group(op, server),
            N_PEER_CHAT => self.peer_chat(op, server),
            N_PEER_DATA => self.peer_data(op, server),
            N_CLIENT_FLAGS => self.client_flags_changed(op, server),
            N_MEMBER_FLAGS => self.member_flags_changed(op, server),
            N_GROUP_CREATED => self.group_created(op, server),
            N_GROUP_DELETED => self.group_deleted(op, server),
            N_CAPTAIN => self.group_captain_changed(op, server),
            N_DATA_CREATED => self.data_object_created(op, server),
            N_DATA_DELETED => self.data_object_deleted(op, server),
            N_INVITATION => self.group_invitation(op, server),
            N_JOIN_ATTEMPT => self.group_join_attempt(op, server),
            N_BANNED => self.you_were_banned(op, server),
            N_MUTED => self.you_were_muted(op, server),
            N_ALERT => self.alert(op, 0),
            N_SHUTDOWN_STARTED => self.alert(op, 1),
            N_SHUTDOWN_ABORTED => self.alert(op, 2),
            N_GAME_LIST => self.read_game_list_complete(op, server),
            _ => {}
        }
    }

    pub fn call(&self, call: Call, a: isize, b: isize) -> isize {
        match call {
            Call::HostGame => self.host_game(a) as isize,
            Call::JoinGame => self.join_game(a) as isize,
            Call::ClientList => self.client_list(),
            Call::MyClient => self.my_client(),
            Call::SendToCaptain => {
                self.send_data(a, None, true);
                0
            }
            Call::BroadcastToGame => {
                self.send_data(a, None, false);
                0
            }
            Call::SendToClient => {
                self.send_data(b, Some(a), false);
                0
            }
            Call::Advertise => {
                self.advertise(a);
                0
            }
            Call::RemoveAdvertisement => {
                self.remove_advertisement();
                0
            }
            Call::KickClient => {
                self.kick_client(a, b != 0);
                0
            }
            Call::SetGameInProgress => {
                self.set_game_in_progress(a != 0);
                0
            }
            Call::Reset => {
                self.reset();
                0
            }
            Call::LeaveGame => {
                self.leave_game();
                0
            }
            Call::Suspend => {
                self.suspend();
                0
            }
            Call::Resume => {
                self.resume();
                0
            }
            Call::Logout => {
                self.logout();
                0
            }
            Call::EnterInternet => {
                self.enter_internet_screen(a != 0);
                0
            }
            Call::Control => {
                self.control(a);
                0
            }
            Call::Event => {
                self.event(a, b);
                0
            }
            Call::Notify => {
                self.notify(a, b);
                0
            }
            Call::UserStatusDone => {
                self.user_status_done(a);
                0
            }
            Call::GroupsDone => {
                self.query_server_groups_done(a);
                0
            }
            Call::ConnectionClosed => {
                self.connection_closed(a);
                0
            }
            Call::Field => match a {
                0 => self.current.get(),
                1 => self.chat.get(),
                2 => self.pending.get(),
                3 => self.game.get(),
                4 => self.wait_op.get(),
                5 => self.suppress_idle.get() as isize,
                6 => self.idle.get() as isize,
                7 => self.have_game_list.get() as isize,
                8 => self.want_moderator.get() as isize,
                9 => self.last_server_refresh.get() as isize,
                _ => 0,
            },
        }
    }

    /// The members' references given back (the destructor's), the game room's first as the original's.
    fn release_all(&self) {
        for cell in [&self.game, &self.chat, &self.pending, &self.current, &self.wait_op] {
            self.assign(cell, 0);
        }
    }
}

/// The routing connection's requests the game's logic makes in Rust since WONLobby round 14 (RoutingLogic.cpp's
/// cases for them are gone, and with them `RoutingConnection`'s helpers): an op's request kind (`EE_WON_RTQ_*`)
/// and its values in the helper's parameter order, as the C++ case's helper built them from the host's arguments
/// (`g`: the connection and the op's four arguments; `text` reads a `GUIString*` as UTF-16 little-endian bytes, 0
/// the empty text). The buffers' ops (`ConnSendData`, `ConnAdvertise`) and the connection's own calls stay the
/// host's. An `unsigned long` is 32 bits, as in the original.
pub(crate) fn conn_request(op: Op, g: [isize; 5], text: &dyn Fn(isize) -> Vec<u8>) -> Option<(i32, Vec<Value>)> {
    use ee_won_net::routecpp::*;
    let n = |x: i64| Value { num: x, bytes: Vec::new() };
    let w = |p: isize| Value { num: 0, bytes: if p == 0 { Vec::new() } else { text(p) } };
    let ids = |v: &[u16]| Value { num: 0, bytes: v.iter().flat_map(|i| i.to_le_bytes()).collect() };
    let s = |x: isize| n(i64::from(x as u16));
    let ul = |x: isize| n(i64::from(x as u32));
    let on = |x: isize| n(i64::from(x != 0));
    let [_, b, c, d, e] = g;
    Some(match op {
        Op::ConnDo => match b {
            6 => (EE_WON_RTQ_ABORT_SHUTDOWN, vec![]),
            7 => (EE_WON_RTQ_MODERATOR, vec![on(c)]),
            8 => (EE_WON_RTQ_LEAVE, vec![s(c)]),
            9 => (EE_WON_RTQ_CANCEL_JOIN, vec![s(c), n(1)]),
            // the logic's game data type, "G" from the constructor, and an empty name
            10 => (EE_WON_RTQ_DELETE_DATA, vec![s(c), Value { num: 0, bytes: b"G".to_vec() }, w(0)]),
            _ => return None,
        },
        Op::ConnSetClientFlags => (EE_WON_RTQ_SET_CLIENT_FLAGS, vec![ul(b), ul(c), ul(d), ul(e)]),
        Op::ConnSetGroupFlags => (EE_WON_RTQ_SET_GROUP_FLAGS, vec![s(b), ul(c), ul(d), n(0), n(0)]),
        Op::ConnBan | Op::ConnMute => {
            let kind = if op == Op::ConnBan { EE_WON_RTQ_BAN } else { EE_WON_RTQ_MUTE };
            (kind, vec![s(b), on(b >> 16), ul(c), w(d), s(e)])
        }
        Op::ConnBanByWonId | Op::ConnMuteByWonId => {
            let kind = if op == Op::ConnBanByWonId { EE_WON_RTQ_BAN_WON_ID } else { EE_WON_RTQ_MUTE_WON_ID };
            (kind, vec![s(b), n(0), n(0), w(0), ul(c)])
        }
        Op::ConnAccept => (EE_WON_RTQ_ACCEPT, vec![s(b), s(c), on(d), w(0)]),
        Op::ConnInvite => (EE_WON_RTQ_INVITE, vec![s(b), s(c), on(d), w(e)]),
        // SendChat(text, flags, recipient): SendChatTo with the flags cut to 16 bits and a list of one
        Op::ConnSendChat => (EE_WON_RTQ_CHAT, vec![w(b), s(c), ids(&[d as u16])]),
        Op::ConnSendChatTo => {
            let v: Vec<u16> = if e == 0 || d <= 0 {
                Vec::new()
            } else {
                // SAFETY: the logic passes `d` ids at `e`, live for the call
                unsafe { std::slice::from_raw_parts(e as *const u16, d as usize) }.to_vec()
            };
            (EE_WON_RTQ_CHAT, vec![w(b), s(c), ids(&v)])
        }
        Op::ConnAlert => (EE_WON_RTQ_ALERT, vec![w(b)]),
        Op::ConnStartShutdown => (EE_WON_RTQ_START_SHUTDOWN, vec![w(b), ul(c)]),
        _ => return None,
    })
}

/// A `wchar_t` text's units (16-bit units on Windows, code points elsewhere) as UTF-16 little-endian bytes, as
/// `WideAppendUtf16LE` makes them.
fn utf16le(units: &[u32]) -> Vec<u8> {
    let v: Vec<u16> = if cfg!(windows) {
        units.iter().map(|&u| u as u16).collect()
    } else {
        ee_won_net::text::utf16_from_utf32(units)
    };
    ee_won_net::text::utf16_to_le_bytes(&v)
}

/// The text callback of [`ee_won_routelogic_conn_request`]: `text`'s unit count, its units written to `units` when
/// not NULL.
pub type EeWonRouteConnTextFn = unsafe extern "C" fn(ctx: *mut c_void, text: isize, units: *mut u32) -> usize;
/// The value callback of [`ee_won_routelogic_conn_request`]: one value's number and bytes, in order.
pub type EeWonRouteConnValueFn = unsafe extern "C" fn(ctx: *mut c_void, num: i64, bytes: *const u8, length: usize);

/// [`conn_request`] for the drivers (won_routeconn_req_test): whether op `op` with the connection and arguments
/// `args` (five) is a request the logic makes in Rust; if so its kind to `kind` and each value to `value`. Texts
/// (`GUIString*`s in the game) are read through `text`.
///
/// # Safety
/// `args` holds five values; the callbacks are callable with `ctx`; `kind` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routelogic_conn_request(
    op: EeWonRouteLogicOp,
    args: *const isize,
    text: EeWonRouteConnTextFn,
    value: EeWonRouteConnValueFn,
    ctx: *mut c_void,
    kind: *mut i32,
) -> bool {
    // SAFETY: the caller's contract
    let g = unsafe { [*args, *args.add(1), *args.add(2), *args.add(3), *args.add(4)] };
    let read = |p: isize| {
        // SAFETY: the caller's callback
        let n = unsafe { text(ctx, p, std::ptr::null_mut()) };
        let mut v = vec![0u32; n];
        if n != 0 {
            // SAFETY: room for `n` units
            unsafe { text(ctx, p, v.as_mut_ptr()) };
        }
        utf16le(&v)
    };
    let Some((k, values)) = conn_request(op, g, &read) else { return false };
    // SAFETY: the caller's contract
    unsafe { *kind = k };
    for x in &values {
        // SAFETY: the caller's callback; the bytes live for the call
        unsafe { value(ctx, x.num, x.bytes.as_ptr(), x.bytes.len()) };
    }
    true
}

// ---- the C ABI

/// As `ee_won_routelogic_new`, with the model's ops answered by its Rust objects (the game's).
#[no_mangle]
pub extern "C" fn ee_won_routelogic_new_model(obj: *mut c_void, host: EeWonRouteLogicHostFn) -> *mut EeWonRouteLogic {
    let mut l = EeWonRouteLogic::new(obj, host);
    l.model = true;
    Box::into_raw(Box::new(l))
}

/// A new logic for the shell `obj` (the members as the constructor sets them).
#[no_mangle]
pub extern "C" fn ee_won_routelogic_new(obj: *mut c_void, host: EeWonRouteLogicHostFn) -> *mut EeWonRouteLogic {
    Box::into_raw(Box::new(EeWonRouteLogic::new(obj, host)))
}

/// Frees a logic, giving back the references it holds.
///
/// # Safety
/// `p` is NULL or from `ee_won_routelogic_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routelogic_free(p: *mut EeWonRouteLogic) {
    if !p.is_null() {
        // SAFETY: the caller's.
        let lg = unsafe { Box::from_raw(p) };
        lg.release_all();
    }
}

/// One of the logic's methods (`EeWonRouteLogicCall`).
///
/// # Safety
/// `p` is from `ee_won_routelogic_new`; the arguments are what the call says.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routelogic_call(
    p: *const EeWonRouteLogic,
    call: EeWonRouteLogicCall,
    a: isize,
    b: isize,
) -> isize {
    // SAFETY: the caller's; the object is only read through `Cell`s, so a re-entrant call is sound.
    let lg = unsafe { &*p };
    lg.call(call, a, b)
}

/// Pushes a number to a sink an op was given (`BadUserIds`, `StatusOpMap`).
///
/// # Safety
/// `sink` is the pointer the op was given.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routelogic_push(sink: *mut c_void, value: u64) {
    // SAFETY: the caller's: the sink is a `Vec<u64>` of the op's caller.
    unsafe { (*(sink as *mut Vec<u64>)).push(value) };
}

#[cfg(test)]
#[path = "routelogic_tests.rs"]
mod tests;
