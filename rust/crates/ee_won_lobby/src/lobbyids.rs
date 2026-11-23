//! The lobby's named numbers, by the names the C++ uses (R6 WON lobby and network round 35): the lobby events'
//! types (`LobbyEvent_*`, until then WONLobbyEventTypes.h's enum `LobbyEventType`: each event class's constructor's
//! type), the chat parser's commands (`LobbyChatCommand_*`, WONLobbyChatCommands.h's enum: the commands the parser
//! registers and the ids it stores) and the lobby's control numbers (`ID_*`, WONLobbyResourceGlobals.h's enum
//! `LobbyResourceCtrlId`, from 200001). The C++ reads them through `rust/won_lobby/lobbyids.h` (those three headers
//! include it); the Rust's choices on control numbers read them here.

#![allow(non_upper_case_globals)]

// ---- the lobby events' types (LobbyEventType)
/// T1 RTTI AlertEvent: its ctor passes 100001 to LobbyEvent
pub const LobbyEvent_Alert: i32 = 100001;
/// T1 RTTI AcceptClientEvent: its ctor passes 100002 to LobbyEvent
pub const LobbyEvent_AcceptClient: i32 = 100002;
/// T1 RTTI AnnounceFriendEvent: its ctor passes 100003 to LobbyChangeEvent
pub const LobbyEvent_AnnounceFriend: i32 = 100003;
/// T1 RTTI AnnounceClientLocationEvent: its ctor passes 100004 to LobbyEvent
pub const LobbyEvent_AnnounceClientLocation: i32 = 100004;
/// T1 RTTI ClientActionEvent: its ctor passes 100005 to LobbyEvent
pub const LobbyEvent_ClientAction: i32 = 100005;
/// T1 RTTI ClientChangedEvent: its ctor passes 100006 to LobbyChangeEvent
pub const LobbyEvent_ClientChanged: i32 = 100006;
/// T1 RTTI ClientModeratedEvent: its ctor passes 100007 to LobbyEvent
pub const LobbyEvent_ClientModerated: i32 = 100007;
/// T1 RTTI ChangeDirectConnectEvent: its ctor passes 100008 to LobbyChangeEvent
pub const LobbyEvent_ChangeDirectConnect: i32 = 100008;
/// T1 RTTI ChatCommandErrorEvent: its ctor passes 100009 to LobbyEvent
pub const LobbyEvent_ChatCommandError: i32 = 100009;
/// T1 RTTI CreateAccountEvent: its ctor passes 100010 to LobbyEvent
pub const LobbyEvent_CreateAccount: i32 = 100010;
/// T1 RTTI CreateRoomEvent: its ctor passes 100011 to LobbyEvent
pub const LobbyEvent_CreateRoom: i32 = 100011;
/// T1 RTTI FriendActionEvent: its ctor passes 100015 to LobbyEvent
pub const LobbyEvent_FriendAction: i32 = 100015;
/// T1 RTTI FriendChangedEvent: its ctor passes 100016 to LobbyChangeEvent
pub const LobbyEvent_FriendChanged: i32 = 100016;
/// T1 RTTI GameActionEvent: its ctor passes 100017 to LobbyEvent
pub const LobbyEvent_GameAction: i32 = 100017;
/// T1 RTTI GetLostPasswordEvent: its ctor passes 100018 to LobbyEvent
pub const LobbyEvent_GetLostPassword: i32 = 100018;
/// T1 RTTI GetLostUserNameEvent: its ctor passes 100019 to LobbyEvent
pub const LobbyEvent_GetLostUserName: i32 = 100019;
/// T1 RTTI GameChangedEvent: its ctor passes 100020 to LobbyChangeEvent
pub const LobbyEvent_GameChanged: i32 = 100020;
/// T1 RTTI GroupChangedEvent: its ctor passes 100021 to LobbyChangeEvent
pub const LobbyEvent_GroupChanged: i32 = 100021;
/// T1 RTTI GroupInvitationEvent: its ctor passes 100022 to LobbyEvent
pub const LobbyEvent_GroupInvitation: i32 = 100022;
/// T1 RTTI GroupJoinAttemptEvent: its ctor passes 100023 to LobbyEvent
pub const LobbyEvent_GroupJoinAttempt: i32 = 100023;
/// T1 RTTI GotHTTPDocumentEvent: its ctor passes 100024 to LobbyEvent
pub const LobbyEvent_GotHTTPDocument: i32 = 100024;
/// T1 RTTI JoinFriendsRoomEvent: its ctor passes 100026 to LobbyEvent
pub const LobbyEvent_JoinFriendsRoom: i32 = 100026;
/// T1 RTTI JoinGameEvent: its ctor passes 100027 to LobbyEvent
pub const LobbyEvent_JoinGame: i32 = 100027;
/// T1 RTTI JoinRoomEvent: its ctor passes 100028 to LobbyEvent
pub const LobbyEvent_JoinRoom: i32 = 100028;
/// T1 RTTI JoinedRoomEvent: its ctor passes 100030 to LobbyEvent
pub const LobbyEvent_JoinedRoom: i32 = 100030;
/// T1 RTTI JumpToRoomEvent: its ctor passes 100031 to LobbyEvent
pub const LobbyEvent_JumpToRoom: i32 = 100031;
/// T1 RTTI LeftRoomEvent: its ctor passes 100033 to LobbyEvent
pub const LobbyEvent_LeftRoom: i32 = 100033;
/// T1 RTTI LoginEvent: its ctor passes 100035 to LobbyEvent
pub const LobbyEvent_Login: i32 = 100035;
/// T1 RTTI LoginInitStatusEvent: its ctor passes 100036 to LobbyEvent
pub const LobbyEvent_LoginInitStatus: i32 = 100036;
/// T1 RTTI PingGameEvent: its ctor passes 100037 to LobbyEvent
pub const LobbyEvent_PingGame: i32 = 100037;
/// T1 RTTI PlayerChangedEvent: its ctor passes 100038 to LobbyChangeEvent
pub const LobbyEvent_PlayerChanged: i32 = 100038;
/// T1 RTTI PlayerTeamChangedEvent: its ctor passes 100039 to LobbyChangeEvent
pub const LobbyEvent_PlayerTeamChanged: i32 = 100039;
/// T1 RTTI QueryAccountResultEvent: its ctor passes 100041 to LobbyEvent
pub const LobbyEvent_QueryAccountResult: i32 = 100041;
/// T1 RTTI QueryServerGroupsEvent: its ctor passes 100042 to LobbyEvent
pub const LobbyEvent_QueryServerGroups: i32 = 100042;
/// T1 RTTI QuickPlayEvent: its ctor passes 100043 to LobbyEvent
pub const LobbyEvent_QuickPlay: i32 = 100043;
/// T1 RTTI RecvChatEvent: its ctor passes 100044 to LobbyEvent
pub const LobbyEvent_RecvChat: i32 = 100044;
/// T1 RTTI ServerChangedEvent: its ctor passes 100047 to LobbyChangeEvent
pub const LobbyEvent_ServerChanged: i32 = 100047;
/// T1 RTTI SetChatInputEvent: its ctor passes 100048 to LobbyEvent
pub const LobbyEvent_SetChatInput: i32 = 100048;
/// T1 RTTI SetCrossPromotionEvent: its ctor passes 100049 to LobbyEvent
pub const LobbyEvent_SetCrossPromotion: i32 = 100049;
/// T1 RTTI SetLoginFieldsEvent: its ctor passes 100051 to LobbyEvent
pub const LobbyEvent_SetLoginFields: i32 = 100051;
/// T1 RTTI SetRoomSpecFlagsEvent: its ctor passes 100054 to LobbyEvent
pub const LobbyEvent_SetRoomSpecFlags: i32 = 100054;
/// T1 RTTI SyncGameListEvent: its ctor passes 100057 to LobbyEvent
pub const LobbyEvent_SyncGameList: i32 = 100057;
/// T1 RTTI UpdateAccountEvent: its ctor passes 100060 to LobbyEvent
pub const LobbyEvent_UpdateAccount: i32 = 100060;
/// T1 RTTI EELobbySyncOptionEvent: its ctor passes 100062 to LobbyEvent
pub const LobbyEvent_EELobbySyncOption: i32 = 100062;
/// T1 RTTI EELobbySyncPlayerEvent: its ctor passes 100063 to LobbyEvent
pub const LobbyEvent_EELobbySyncPlayer: i32 = 100063;

// ---- the chat parser's commands (LobbyChatCommand)
/// T3 PROVISIONAL: ParseCommand stores 0 for an unknown command (mError 1)
pub const LobbyChatCommand_None: i32 = 0;
/// T3 PROVISIONAL: HandleInput stores 1 for text that is not a /command (plain chat)
pub const LobbyChatCommand_Broadcast: i32 = 1;
/// T1 resource string ChatCommandLogic_Whisper_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Whisper: i32 = 2;
/// T1 resource string ChatCommandLogic_Emote_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Emote: i32 = 3;
/// T1 resource string ChatCommandLogic_Block_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Block: i32 = 4;
/// T1 resource string ChatCommandLogic_Ignore_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Ignore: i32 = 5;
/// T1 resource string ChatCommandLogic_Away_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Away: i32 = 6;
/// T1 resource string ChatCommandLogic_Moderator_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Moderator: i32 = 7;
/// T1 resource string ChatCommandLogic_ServerMute_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_ServerMute: i32 = 8;
/// T1 resource string ChatCommandLogic_ServerBan_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_ServerBan: i32 = 9;
/// T1 resource string ChatCommandLogic_Mute_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Mute: i32 = 10;
/// T1 resource string ChatCommandLogic_Ban_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Ban: i32 = 11;
/// T1 resource string ChatCommandLogic_Unmute_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Unmute: i32 = 12;
/// T1 resource string ChatCommandLogic_ServerUnmute_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_ServerUnmute: i32 = 13;
/// T1 resource string ChatCommandLogic_Unban_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Unban: i32 = 14;
/// T1 resource string ChatCommandLogic_ServerUnban_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_ServerUnban: i32 = 15;
/// T1 resource string ChatCommandLogic_Invite_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Invite: i32 = 16;
/// T1 resource string ChatCommandLogic_Uninvite_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Uninvite: i32 = 17;
/// T1 resource string ChatCommandLogic_Reply_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Reply: i32 = 18;
/// T1 resource string ChatCommandLogic_Clear_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Clear: i32 = 19;
/// T1 resource string ChatCommandLogic_Help_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Help: i32 = 20;
/// T1 resource string ChatCommandLogic_ShowTeam_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_ShowTeam: i32 = 21;
/// T1 resource string ChatCommandLogic_Alert_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Alert: i32 = 22;
/// T1 GetCommandName 23 -> "startshutdown"
pub const LobbyChatCommand_StartShutdown: i32 = 23;
/// T1 GetCommandName 24 -> "abortshutdown"
pub const LobbyChatCommand_AbortShutdown: i32 = 24;
/// T1 resource string ChatCommandLogic_Warn_String (ChatCommandParser ctor AddCommands)
pub const LobbyChatCommand_Warn: i32 = 25;

// ---- the lobby's control numbers (LobbyResourceCtrlId; LobbyResourceCtrlId_Min is ControlId_UserMin)
/// ControlId_UserMin; the first control ID is 200001
pub const LobbyResourceCtrlId_Min: i32 = 200000;
pub const ID_ShowCreateAccount: i32 = 200001;
pub const ID_ShowLostPassword: i32 = 200002;
pub const ID_ShowLostUserName: i32 = 200003;
pub const ID_ShowMOTD: i32 = 200004;
pub const ID_ShowTOU: i32 = 200005;
pub const ID_LoginScreenBack: i32 = 200006;
pub const ID_ShowLoginHelp: i32 = 200007;
pub const ID_ShowInternetScreen: i32 = 200008;
pub const ID_ShowLanScreen: i32 = 200009;
pub const ID_Logout: i32 = 200010;
pub const ID_HostGame: i32 = 200011;
pub const ID_ChangeRoom: i32 = 200012;
pub const ID_JoinGame: i32 = 200013;
pub const ID_LeaveGame: i32 = 200014;
pub const ID_StartGame: i32 = 200015;
pub const ID_RefreshGameList: i32 = 200016;
pub const ID_AddFriend: i32 = 200017;
pub const ID_RefreshFriends: i32 = 200018;
pub const ID_AddLANName: i32 = 200019;
pub const ID_ChangeLANBroadcast: i32 = 200020;
pub const ID_FilterDirtyWordsCheck: i32 = 200021;
pub const ID_AnonymousToFriendsCheck: i32 = 200022;
pub const ID_ChooseNetworkAdapter: i32 = 200023;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_numbers_are_the_original_enums_values() {
        // the events' types run from Alert's 100001 to EELobbySyncPlayer's 100063, with the original's gaps
        assert_eq!(LobbyEvent_Alert, 100001);
        assert_eq!(LobbyEvent_CreateRoom, 100011);
        assert_eq!(LobbyEvent_FriendAction, 100015);
        assert_eq!(LobbyEvent_Login, 100035);
        assert_eq!(LobbyEvent_EELobbySyncPlayer, 100063);
        // the commands count from 0 without gaps
        assert_eq!(LobbyChatCommand_None, 0);
        assert_eq!(LobbyChatCommand_Help, 20);
        assert_eq!(LobbyChatCommand_Warn, 25);
        // the controls follow ControlId_UserMin
        assert_eq!(LobbyResourceCtrlId_Min, 200000);
        assert_eq!(ID_ShowCreateAccount, 200001);
        assert_eq!(ID_ChooseNetworkAdapter, 200023);
    }
}
