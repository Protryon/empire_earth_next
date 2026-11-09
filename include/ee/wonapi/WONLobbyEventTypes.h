// ee/wonapi/WONLobbyEventTypes.h. Named integer constants; names PROVISIONAL unless the comment says T1.
#ifndef EE_WONLOBBYEVENTTYPES_H
#define EE_WONLOBBYEVENTTYPES_H

// the lobby's event ids (ComponentEvent::mType >= 100001, Event_User_Start + n); names = the event class that passes the id to its LobbyEvent/LobbyChangeEvent base (RTTI class names). Ids without an own class (posted as a bare LobbyEvent: LobbyFn359120) are not named
enum LobbyEventType {
    LobbyEvent_Alert = 100001,                 // T1 RTTI AlertEvent: its ctor passes 100001 to LobbyEvent
    LobbyEvent_AcceptClient = 100002,          // T1 RTTI AcceptClientEvent: its ctor passes 100002 to LobbyEvent
    LobbyEvent_AnnounceFriend = 100003,        // T1 RTTI AnnounceFriendEvent: its ctor passes 100003 to LobbyChangeEvent
    LobbyEvent_AnnounceClientLocation = 100004,  // T1 RTTI AnnounceClientLocationEvent: its ctor passes 100004 to LobbyEvent
    LobbyEvent_ClientAction = 100005,          // T1 RTTI ClientActionEvent: its ctor passes 100005 to LobbyEvent
    LobbyEvent_ClientChanged = 100006,         // T1 RTTI ClientChangedEvent: its ctor passes 100006 to LobbyChangeEvent
    LobbyEvent_ClientModerated = 100007,       // T1 RTTI ClientModeratedEvent: its ctor passes 100007 to LobbyEvent
    LobbyEvent_ChangeDirectConnect = 100008,   // T1 RTTI ChangeDirectConnectEvent: its ctor passes 100008 to LobbyChangeEvent
    LobbyEvent_ChatCommandError = 100009,      // T1 RTTI ChatCommandErrorEvent: its ctor passes 100009 to LobbyEvent
    LobbyEvent_CreateAccount = 100010,         // T1 RTTI CreateAccountEvent: its ctor passes 100010 to LobbyEvent
    LobbyEvent_CreateRoom = 100011,            // T1 RTTI CreateRoomEvent: its ctor passes 100011 to LobbyEvent
    LobbyEvent_FriendAction = 100015,          // T1 RTTI FriendActionEvent: its ctor passes 100015 to LobbyEvent
    LobbyEvent_FriendChanged = 100016,         // T1 RTTI FriendChangedEvent: its ctor passes 100016 to LobbyChangeEvent
    LobbyEvent_GameAction = 100017,            // T1 RTTI GameActionEvent: its ctor passes 100017 to LobbyEvent
    LobbyEvent_GetLostPassword = 100018,       // T1 RTTI GetLostPasswordEvent: its ctor passes 100018 to LobbyEvent
    LobbyEvent_GetLostUserName = 100019,       // T1 RTTI GetLostUserNameEvent: its ctor passes 100019 to LobbyEvent
    LobbyEvent_GameChanged = 100020,           // T1 RTTI GameChangedEvent: its ctor passes 100020 to LobbyChangeEvent
    LobbyEvent_GroupChanged = 100021,          // T1 RTTI GroupChangedEvent: its ctor passes 100021 to LobbyChangeEvent
    LobbyEvent_GroupInvitation = 100022,       // T1 RTTI GroupInvitationEvent: its ctor passes 100022 to LobbyEvent
    LobbyEvent_GroupJoinAttempt = 100023,      // T1 RTTI GroupJoinAttemptEvent: its ctor passes 100023 to LobbyEvent
    LobbyEvent_GotHTTPDocument = 100024,       // T1 RTTI GotHTTPDocumentEvent: its ctor passes 100024 to LobbyEvent
    LobbyEvent_JoinFriendsRoom = 100026,       // T1 RTTI JoinFriendsRoomEvent: its ctor passes 100026 to LobbyEvent
    LobbyEvent_JoinGame = 100027,              // T1 RTTI JoinGameEvent: its ctor passes 100027 to LobbyEvent
    LobbyEvent_JoinRoom = 100028,              // T1 RTTI JoinRoomEvent: its ctor passes 100028 to LobbyEvent
    LobbyEvent_JoinedRoom = 100030,            // T1 RTTI JoinedRoomEvent: its ctor passes 100030 to LobbyEvent
    LobbyEvent_JumpToRoom = 100031,            // T1 RTTI JumpToRoomEvent: its ctor passes 100031 to LobbyEvent
    LobbyEvent_LeftRoom = 100033,              // T1 RTTI LeftRoomEvent: its ctor passes 100033 to LobbyEvent
    LobbyEvent_Login = 100035,                 // T1 RTTI LoginEvent: its ctor passes 100035 to LobbyEvent
    LobbyEvent_LoginInitStatus = 100036,       // T1 RTTI LoginInitStatusEvent: its ctor passes 100036 to LobbyEvent
    LobbyEvent_PingGame = 100037,              // T1 RTTI PingGameEvent: its ctor passes 100037 to LobbyEvent
    LobbyEvent_PlayerChanged = 100038,         // T1 RTTI PlayerChangedEvent: its ctor passes 100038 to LobbyChangeEvent
    LobbyEvent_PlayerTeamChanged = 100039,     // T1 RTTI PlayerTeamChangedEvent: its ctor passes 100039 to LobbyChangeEvent
    LobbyEvent_QueryAccountResult = 100041,    // T1 RTTI QueryAccountResultEvent: its ctor passes 100041 to LobbyEvent
    LobbyEvent_QueryServerGroups = 100042,     // T1 RTTI QueryServerGroupsEvent: its ctor passes 100042 to LobbyEvent
    LobbyEvent_QuickPlay = 100043,             // T1 RTTI QuickPlayEvent: its ctor passes 100043 to LobbyEvent
    LobbyEvent_RecvChat = 100044,              // T1 RTTI RecvChatEvent: its ctor passes 100044 to LobbyEvent
    LobbyEvent_ServerChanged = 100047,         // T1 RTTI ServerChangedEvent: its ctor passes 100047 to LobbyChangeEvent
    LobbyEvent_SetChatInput = 100048,          // T1 RTTI SetChatInputEvent: its ctor passes 100048 to LobbyEvent
    LobbyEvent_SetCrossPromotion = 100049,     // T1 RTTI SetCrossPromotionEvent: its ctor passes 100049 to LobbyEvent
    LobbyEvent_SetLoginFields = 100051,        // T1 RTTI SetLoginFieldsEvent: its ctor passes 100051 to LobbyEvent
    LobbyEvent_SetRoomSpecFlags = 100054,      // T1 RTTI SetRoomSpecFlagsEvent: its ctor passes 100054 to LobbyEvent
    LobbyEvent_SyncGameList = 100057,          // T1 RTTI SyncGameListEvent: its ctor passes 100057 to LobbyEvent
    LobbyEvent_UpdateAccount = 100060,         // T1 RTTI UpdateAccountEvent: its ctor passes 100060 to LobbyEvent
    LobbyEvent_EELobbySyncOption = 100062,     // T1 RTTI EELobbySyncOptionEvent: its ctor passes 100062 to LobbyEvent
    LobbyEvent_EELobbySyncPlayer = 100063,     // T1 RTTI EELobbySyncPlayerEvent: its ctor passes 100063 to LobbyEvent
};

#endif // EE_WONLOBBYEVENTTYPES_H
