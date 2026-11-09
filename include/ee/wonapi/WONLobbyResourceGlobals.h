// Lobby resources (strings, images, sounds), their IDs and the control IDs; global scope as in the SDK
#ifndef EE_WONAPI_WONLOBBYRESOURCEGLOBALS_H
#define EE_WONAPI_WONLOBBYRESOURCEGLOBALS_H
#include "ee/wonapi/WONLobbyResource.h"

extern WONAPI::GUIString LobbyGlobal_Cancel_String; // 0x966944
extern WONAPI::GUIString LobbyGlobal_Lan_String; // 0x9669e0
extern WONAPI::GUIString LobbyGlobal_No_String; // 0x966a58
extern WONAPI::GUIString LobbyGlobal_OK_String; // 0x966468
extern WONAPI::GUIString LobbyGlobal_Yes_String; // 0x966a30
extern WONAPI::GUIString LobbyGlobal_QuickPlay_String; // 0x966758
extern WONAPI::SmartPtr<WONAPI::Image> LobbyGlobal_Searching_Image; // 0x9668ac
extern WONAPI::SmartPtr<WONAPI::Image> LobbyGlobal_Firewall_Image; // 0x966414
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_ButtonClick_Sound; // 0x966770
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_LobbyMusic_Sound; // 0x9664d0
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_AllReady_Sound; // 0x9667d0
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_AllNotReady_Sound; // 0x966438
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_AskToJoinRecv_Sound; // 0x966694
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_Error_Sound; // 0x966638
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_FriendJoined_Sound; // 0x96646c
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_FriendLeft_Sound; // 0x966a9c
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_InviteRecv_Sound; // 0x966714
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_PlayerJoined_Sound; // 0x966500
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_SierraLogon_Sound; // 0x966a2c
extern WONAPI::SmartPtr<WONAPI::Sound> LobbyGlobal_WhisperRecv_Sound; // 0x9665dc
extern WONAPI::GUIString AccountLogic_InvalidCDKeyTitle_String; // 0x9666f4
extern WONAPI::GUIString AccountLogic_InvalidCDKeyDetails_String; // 0x96682c
extern WONAPI::GUIString AddFriendDialog_StatusTitle_String; // 0x966598
extern WONAPI::GUIString AddFriendDialog_StatusDetails_String; // 0x966740
extern WONAPI::GUIString AdminActionDialog_Warn_String; // 0x96640c
extern WONAPI::GUIString AdminActionDialog_Mute_String; // 0x9664a0
extern WONAPI::GUIString AdminActionDialog_UnMute_String; // 0x966a04
extern WONAPI::GUIString AdminActionDialog_Kick_String; // 0x966604
extern WONAPI::GUIString AdminActionDialog_Ban_String; // 0x966444
extern WONAPI::GUIString AdminActionDialog_ExplanationOpt_String; // 0x966818
extern WONAPI::GUIString AdminActionDialog_ExplanationReq_String; // 0x966548
extern WONAPI::GUIString BadUserDialog_OneDay_String; // 0x9664cc
extern WONAPI::GUIString BadUserDialog_OneHour_String; // 0x9665b4
extern WONAPI::GUIString BadUserDialog_OneMinute_String; // 0x96675c
extern WONAPI::GUIString BadUserDialog_OneSecond_String; // 0x9668dc
extern WONAPI::GUIString BadUserDialog_XDays_String; // 0x966418
extern WONAPI::GUIString BadUserDialog_XHours_String; // 0x966760
extern WONAPI::GUIString BadUserDialog_XMinutes_String; // 0x9667b4
extern WONAPI::GUIString BadUserDialog_XSeconds_String; // 0x966530
extern WONAPI::GUIString BadUserDialog_Infinite_String; // 0x966828
extern WONAPI::GUIString ChatCommandLogic_Whisper_String; // 0x9664bc
extern WONAPI::GUIString ChatCommandLogic_Block_String; // 0x9666a0
extern WONAPI::GUIString ChatCommandLogic_Emote_String; // 0x966a18
extern WONAPI::GUIString ChatCommandLogic_Ignore_String; // 0x96641c
extern WONAPI::GUIString ChatCommandLogic_Away_String; // 0x9668c8
extern WONAPI::GUIString ChatCommandLogic_Invite_String; // 0x9668c4
extern WONAPI::GUIString ChatCommandLogic_Uninvite_String; // 0x966708
extern WONAPI::GUIString ChatCommandLogic_Reply_String; // 0x9665ac
extern WONAPI::GUIString ChatCommandLogic_Clear_String; // 0x966a74
extern WONAPI::GUIString ChatCommandLogic_Help_String; // 0x9666cc
extern WONAPI::GUIString ChatCommandLogic_ShowTeam_String; // 0x9667a0
extern WONAPI::GUIString ChatCommandLogic_Moderator_String; // 0x96642c
extern WONAPI::GUIString ChatCommandLogic_ServerMute_String; // 0x966744
extern WONAPI::GUIString ChatCommandLogic_ServerBan_String; // 0x96676c
extern WONAPI::GUIString ChatCommandLogic_Mute_String; // 0x966648
extern WONAPI::GUIString ChatCommandLogic_Ban_String; // 0x96689c
extern WONAPI::GUIString ChatCommandLogic_Unmute_String; // 0x96679c
extern WONAPI::GUIString ChatCommandLogic_ServerUnmute_String; // 0x9666e4
extern WONAPI::GUIString ChatCommandLogic_Unban_String; // 0x966794
extern WONAPI::GUIString ChatCommandLogic_ServerUnban_String; // 0x9663f4
extern WONAPI::GUIString ChatCommandLogic_Alert_String; // 0x9665d0
extern WONAPI::GUIString ChatCommandLogic_Warn_String; // 0x966a48
extern WONAPI::GUIString ChatCommandLogic_HelpHeader_String; // 0x9668d8
extern WONAPI::GUIString ChatCommandLogic_HelpHelp_String; // 0x966498
extern WONAPI::GUIString ChatCommandLogic_ShowTeamHelp_String; // 0x9668b0
extern WONAPI::GUIString ChatCommandLogic_WhisperHelp_String; // 0x96688c
extern WONAPI::GUIString ChatCommandLogic_BlockHelp_String; // 0x966580
extern WONAPI::GUIString ChatCommandLogic_IgnoreHelp_String; // 0x9663c4
extern WONAPI::GUIString ChatCommandLogic_ReplyHelp_String; // 0x966620
extern WONAPI::GUIString ChatCommandLogic_EmoteHelp_String; // 0x9667c0
extern WONAPI::GUIString ChatCommandLogic_AwayHelp_String; // 0x9665f8
extern WONAPI::GUIString ChatCommandLogic_ClearHelp_String; // 0x966874
extern WONAPI::GUIString ChatCommandLogic_InviteHelp_String; // 0x966a98
extern WONAPI::GUIString ChatCommandLogic_UninviteHelp_String; // 0x966820
extern WONAPI::GUIString ChatCommandLogic_MuteHelp_String; // 0x966674
extern WONAPI::GUIString ChatCommandLogic_UnmuteHelp_String; // 0x966658
extern WONAPI::GUIString ChatCommandLogic_BanHelp_String; // 0x966984
extern WONAPI::GUIString ChatCommandLogic_UnbanHelp_String; // 0x966920
extern WONAPI::GUIString ChatCommandLogic_ModeratorHelp_String; // 0x966958
extern WONAPI::GUIString ChatCommandLogic_ServerMuteHelp_String; // 0x9669f4
extern WONAPI::GUIString ChatCommandLogic_ServerUnmuteHelp_String; // 0x9665cc
extern WONAPI::GUIString ChatCommandLogic_ServerBanHelp_String; // 0x966ab0
extern WONAPI::GUIString ChatCommandLogic_ServerUnbanHelp_String; // 0x96686c
extern WONAPI::GUIString ChatCommandLogic_AlertHelp_String; // 0x966860
extern WONAPI::GUIString ChatCommandLogic_WarnHelp_String; // 0x96652c
extern WONAPI::GUIString ChatCtrl_NormalChat_String; // 0x9667e4
extern WONAPI::GUIString ChatCtrl_Emote_String; // 0x9668b8
extern WONAPI::GUIString ChatCtrl_TeamChat_String; // 0x96692c
extern WONAPI::GUIString ChatCtrl_TeamEmote_String; // 0x9666e0
extern WONAPI::GUIString ChatCtrl_YouWhisperToYourself_String; // 0x966718
extern WONAPI::GUIString ChatCtrl_YouWhisper_String; // 0x966900
extern WONAPI::GUIString ChatCtrl_SomeoneWhispersToYou_String; // 0x966398
extern WONAPI::GUIString ChatCtrl_YouWarnSomeone_String; // 0x9666d8
extern WONAPI::GUIString ChatCtrl_YouWarnYourself_String; // 0x966a00
extern WONAPI::GUIString ChatCtrl_SomeoneWarnsYou_String; // 0x9669b0
extern WONAPI::GUIString ChatCtrl_SomeoneWarnsSomeone_String; // 0x966a70
extern WONAPI::GUIString ChatCtrl_EnteringRoom_String; // 0x966410
extern WONAPI::GUIString ChatCtrl_LeavingRoom_String; // 0x9669c8
extern WONAPI::GUIString ChatCtrl_Disconnected_String; // 0x966664
extern WONAPI::GUIString ChatCtrl_YouWereKicked_String; // 0x966908
extern WONAPI::GUIString ChatCtrl_YouWereBannedMinute_String; // 0x96694c
extern WONAPI::GUIString ChatCtrl_YouWereBannedMinutes_String; // 0x966634
extern WONAPI::GUIString ChatCtrl_YouWereBannedHour_String; // 0x966428
extern WONAPI::GUIString ChatCtrl_YouWereBannedHours_String; // 0x966a0c
extern WONAPI::GUIString ChatCtrl_YouWereBannedDay_String; // 0x966600
extern WONAPI::GUIString ChatCtrl_YouWereBannedDays_String; // 0x966ad0
extern WONAPI::GUIString ChatCtrl_YouWereBannedIndefinitely_String; // 0x96697c
extern WONAPI::GUIString ChatCtrl_SomeoneWasKicked_String; // 0x9664c8
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedMinute_String; // 0x9668a4
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedMinutes_String; // 0x9663bc
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedHour_String; // 0x9667d8
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedHours_String; // 0x966684
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedDay_String; // 0x966ac4
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedDays_String; // 0x96660c
extern WONAPI::GUIString ChatCtrl_SomeoneWasBannedIndefinitely_String; // 0x9664b8
extern WONAPI::GUIString ChatCtrl_YouWereUnmuted_String; // 0x9663d4
extern WONAPI::GUIString ChatCtrl_YouWereMutedMinute_String; // 0x9664e4
extern WONAPI::GUIString ChatCtrl_YouWereMutedMinutes_String; // 0x9666ac
extern WONAPI::GUIString ChatCtrl_YouWereMutedHour_String; // 0x9663cc
extern WONAPI::GUIString ChatCtrl_YouWereMutedHours_String; // 0x966ae4
extern WONAPI::GUIString ChatCtrl_YouWereMutedDay_String; // 0x966a34
extern WONAPI::GUIString ChatCtrl_YouWereMutedDays_String; // 0x9665e4
extern WONAPI::GUIString ChatCtrl_YouWereMutedIndefinitely_String; // 0x96649c
extern WONAPI::GUIString ChatCtrl_SomeoneWasUnmuted_String; // 0x966960
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedMinute_String; // 0x966424
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedMinutes_String; // 0x9667a8
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedHour_String; // 0x9663e8
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedHours_String; // 0x966588
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedDay_String; // 0x966690
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedDays_String; // 0x9668e4
extern WONAPI::GUIString ChatCtrl_SomeoneWasMutedIndefinitely_String; // 0x966440
extern WONAPI::GUIString ChatCtrl_ModeratorComment_String; // 0x966494
extern WONAPI::GUIString ChatCtrl_AlertComment_String; // 0x966898
extern WONAPI::GUIString ChatCtrl_ServerAlert_String; // 0x9664b4
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedMinute_String; // 0x96669c
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedMinutes_String; // 0x9668d4
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedHour_String; // 0x9665a4
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedHours_String; // 0x966914
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedDay_String; // 0x9663d0
extern WONAPI::GUIString ChatCtrl_ServerShutdownStartedDays_String; // 0x9668e0
extern WONAPI::GUIString ChatCtrl_ServerShutdownAborted_String; // 0x966608
extern WONAPI::GUIString ChatCtrl_SomeoneInvitesYou_String; // 0x966940
extern WONAPI::GUIString ChatCtrl_SomeoneUninvitesYou_String; // 0x966934
extern WONAPI::GUIString ChatCtrl_YouInviteSomeone_String; // 0x966644
extern WONAPI::GUIString ChatCtrl_YouUninviteSomeone_String; // 0x966478
extern WONAPI::GUIString ChatCtrl_SomeoneInvitesYouExpl_String; // 0x966980
extern WONAPI::GUIString ChatCtrl_SomeoneUninvitesYouExpl_String; // 0x966974
extern WONAPI::GUIString ChatCtrl_YouInviteSomeoneExpl_String; // 0x9663e4
extern WONAPI::GUIString ChatCtrl_YouUninviteSomeoneExpl_String; // 0x966878
extern WONAPI::GUIString ChatCtrl_Accept_String; // 0x966ad4
extern WONAPI::GUIString ChatCtrl_Reject_String; // 0x966574
extern WONAPI::GUIString ChatCtrl_SomeoneAsksToJoin_String; // 0x9669bc
extern WONAPI::GUIString ChatCtrl_SomeoneAsksToJoinExpl_String; // 0x966734
extern WONAPI::GUIString ChatCtrl_ClientNotOnServer_String; // 0x9669a0
extern WONAPI::GUIString ChatCtrl_ClientInGame_String; // 0x9669f8
extern WONAPI::GUIString ChatCtrl_ClientInChat_String; // 0x9669dc
extern WONAPI::GUIString ChatCtrl_ClientInGameAndChat_String; // 0x96693c
extern WONAPI::GUIString ChatCtrl_ClientIsAnonymous_String; // 0x9664ec
extern WONAPI::GUIString ChatCtrl_FriendIsChatting_String; // 0x96674c
extern WONAPI::GUIString ChatCtrl_FriendIsInGameStaging_String; // 0x966768
extern WONAPI::GUIString ChatCtrl_FriendIsPlaying_String; // 0x966acc
extern WONAPI::GUIString ChatCtrl_FriendIsOn_String; // 0x966460
extern WONAPI::GUIString ChatCtrl_FriendIsNotOnline_String; // 0x966a28
extern WONAPI::GUIString ChatCtrl_PlayerKicked_String; // 0x966450
extern WONAPI::GUIString ChatCtrl_PlayerBanned_String; // 0x966780
extern WONAPI::GUIString ChatCtrl_FriendJoined_String; // 0x96661c
extern WONAPI::GUIString ChatCtrl_FriendLeft_String; // 0x966ad8
extern WONAPI::GUIString ChatCtrl_TeamMemberJoined_String; // 0x966954
extern WONAPI::GUIString ChatCtrl_TeamMemberLeft_String; // 0x9666a8
extern WONAPI::GUIString ChatCtrl_TeamMembers_String; // 0x966528
extern WONAPI::GUIString ChatCtrl_IgnoreList_String; // 0x966a64
extern WONAPI::GUIString ChooseNetworkAdapterDialog_Default_String; // 0x96681c
extern WONAPI::GUIString ChooseNetworkAdapterDialog_Unknown_String; // 0x966594
extern WONAPI::GUIString ClientOptionsCtrl_Whisper_String; // 0x966968
extern WONAPI::GUIString ClientOptionsCtrl_Invite_String; // 0x966710
extern WONAPI::GUIString ClientOptionsCtrl_Uninvite_String; // 0x966814
extern WONAPI::GUIString ClientOptionsCtrl_AddToFriends_String; // 0x9667d4
extern WONAPI::GUIString ClientOptionsCtrl_RemoveFromFriends_String; // 0x9665f0
extern WONAPI::GUIString ClientOptionsCtrl_ClearAFK_String; // 0x966964
extern WONAPI::GUIString ClientOptionsCtrl_SetAFK_String; // 0x9668f4
extern WONAPI::GUIString ClientOptionsCtrl_Ignore_String; // 0x9668c0
extern WONAPI::GUIString ClientOptionsCtrl_Unignore_String; // 0x96656c
extern WONAPI::GUIString ClientOptionsCtrl_Block_String; // 0x9667bc
extern WONAPI::GUIString ClientOptionsCtrl_Unblock_String; // 0x966400
extern WONAPI::GUIString ClientOptionsCtrl_Warn_String; // 0x96653c
extern WONAPI::GUIString ClientOptionsCtrl_Mute_String; // 0x966998
extern WONAPI::GUIString ClientOptionsCtrl_UnMute_String; // 0x966890
extern WONAPI::GUIString ClientOptionsCtrl_Kick_String; // 0x9664a8
extern WONAPI::GUIString ClientOptionsCtrl_Ban_String; // 0x9667fc
extern WONAPI::GUIString DirectConnectionOptionsCtrl_Connect_String; // 0x9666a4
extern WONAPI::GUIString DirectConnectionOptionsCtrl_Refresh_String; // 0x9667c4
extern WONAPI::GUIString DirectConnectionOptionsCtrl_Remove_String; // 0x966614
extern WONAPI::GUIString FriendOptionsCtrl_Locate_String; // 0x9664e8
extern WONAPI::GUIString FriendOptionsCtrl_RefreshList_String; // 0x96683c
extern WONAPI::GUIString FriendOptionsCtrl_RemoveFromFriends_String; // 0x966a5c
extern WONAPI::SmartPtr<WONAPI::Image> FriendsListCtrl_Unknown_Image; // 0x966aa4
extern WONAPI::SmartPtr<WONAPI::Image> FriendsListCtrl_NotFound_Image; // 0x96677c
extern WONAPI::SmartPtr<WONAPI::Image> FriendsListCtrl_InChat_Image; // 0x9663ec
extern WONAPI::SmartPtr<WONAPI::Image> FriendsListCtrl_InGame_Image; // 0x966858
extern WONAPI::GUIString GameOptionsPopup_Join_String; // 0x96647c
extern WONAPI::GUIString GameOptionsPopup_Ping_String; // 0x966988
extern WONAPI::GUIString GameOptionsPopup_QueryDetails_String; // 0x9665c8
extern WONAPI::GUIString GameOptionsPopup_HideDetails_String; // 0x966790
extern WONAPI::GUIString GameOptionsPopup_ShowDetails_String; // 0x966a90
extern WONAPI::GUIString GameOptionsPopup_Remove_String; // 0x9668fc
extern WONAPI::GUIString GameStagingCtrl_Ready_String; // 0x966748
extern WONAPI::GUIString GameStagingCtrl_NotReady_String; // 0x96650c
extern WONAPI::GUIString GameStagingCtrl_StartGame_String; // 0x966558
extern WONAPI::GUIString GameStagingCtrl_LeaveGame_String; // 0x966a08
extern WONAPI::GUIString GameStagingCtrl_DissolveGame_String; // 0x966730
extern WONAPI::GUIString GameStagingLogic_KickedTitle_String; // 0x9669fc
extern WONAPI::GUIString GameStagingLogic_KickedDetails_String; // 0x9669a4
extern WONAPI::GUIString GameStagingLogic_BannedTitle_String; // 0x966704
extern WONAPI::GUIString GameStagingLogic_BannedDetails_String; // 0x966970
extern WONAPI::GUIString GameStagingLogic_ConnectWarningTitle1_String; // 0x9665a8
extern WONAPI::GUIString GameStagingLogic_ConnectWarningDetails1_String; // 0x966458
extern WONAPI::GUIString GameStagingLogic_ConnectWarningTitle2_String; // 0x966448
extern WONAPI::GUIString GameStagingLogic_ConnectWarningDetails2_String; // 0x9667f0
extern WONAPI::GUIString GameStagingLogic_ConnectWarningTitle3_String; // 0x9669b4
extern WONAPI::GUIString GameStagingLogic_ConnectWarningDetails3_String; // 0x9666c8
extern WONAPI::GUIString GameStagingLogic_NoPingReplyError_String; // 0x96667c
extern WONAPI::GUIString GameStagingLogic_GettingGameInfoTitle_String; // 0x966654
extern WONAPI::GUIString GameStagingLogic_GettingGameInfoDetails_String; // 0x966480
extern WONAPI::GUIString GameStagingLogic_JoinGameErrorTitle_String; // 0x966938
extern WONAPI::GUIString GameStagingLogic_VerifyingPingTitle_String; // 0x966a14
extern WONAPI::GUIString GameStagingLogic_VerifyingPingDetails_String; // 0x966ac8
extern WONAPI::GUIString GameStagingLogic_DissolveGame_String; // 0x966800
extern WONAPI::GUIString GameStagingLogic_DissolveGameConfirm_String; // 0x966700
extern WONAPI::GUIString GameStagingLogic_LeaveGame_String; // 0x96662c
extern WONAPI::GUIString GameStagingLogic_LeaveGameConfirm_String; // 0x966568
extern WONAPI::GUIString GameStagingLogic_NoQuickPlayGameTitle_String; // 0x96654c
extern WONAPI::GUIString GameStagingLogic_NoQuickPlayGameDetails_String; // 0x96684c
extern WONAPI::GUIString GameUserListCtrl_Ban_String; // 0x96691c
extern WONAPI::GUIString GameUserListCtrl_Kick_String; // 0x966430
extern WONAPI::SmartPtr<WONAPI::Image> GameUserListCtrl_Ready_Image; // 0x9665c0
extern WONAPI::GUIString InternetQuickPlay_Welcome_String; // 0x966ab8
extern WONAPI::GUIString InternetQuickPlay_UserWelcome_String; // 0x966544
extern WONAPI::GUIString JoinResponse_Joined_String; // 0x9669d0
extern WONAPI::GUIString JoinResponse_ServerFull_String; // 0x9665b8
extern WONAPI::GUIString JoinResponse_InviteOnly_String; // 0x966464
extern WONAPI::GUIString JoinResponse_AskToJoin_String; // 0x9667b8
extern WONAPI::GUIString JoinResponse_GameInProgress_String; // 0x96668c
extern WONAPI::GUIString JoinResponse_InvalidPassword_String; // 0x966a50
extern WONAPI::GUIString JoinResponse_NotOnLAN_String; // 0x9663f8
extern WONAPI::GUIString JoinResponse_Blocked_String; // 0x9666f8
extern WONAPI::GUIString JoinResponse_Unknown_String; // 0x966848
extern WONAPI::GUIString LANLogic_SocketBindError_String; // 0x9663d8
extern WONAPI::GUIString LANLogic_SocketListenError_String; // 0x966808
extern WONAPI::GUIString LANLogic_ConnectToGameTitle_String; // 0x966904
extern WONAPI::GUIString LANLogic_ConnectToGameDetails_String; // 0x9666dc
extern WONAPI::GUIString LANLogic_CheckingAddressTitle_String; // 0x96690c
extern WONAPI::GUIString LANLogic_CheckingAddressDetails_String; // 0x9668a0
extern WONAPI::GUIString LANLogic_RegisterTitle_String; // 0x96699c
extern WONAPI::GUIString LANLogic_RegisterDetails_String; // 0x9663fc
extern WONAPI::GUIString LANLogic_JoinFailure_String; // 0x966a78
extern WONAPI::GUIString LANLogic_JoinReplyError_String; // 0x966a44
extern WONAPI::GUIString LANLogic_InvalidSubnet_String; // 0x9666c4
extern WONAPI::GUIString LobbyContainer_InitStart_String; // 0x966888
extern WONAPI::GUIString LobbyContainer_InitFailed_String; // 0x9667b0
extern WONAPI::GUIString LobbyContainer_CheckingVersion_String; // 0x9668d0
extern WONAPI::GUIString LobbyContainer_QueryingServers_String; // 0x9669ac
extern WONAPI::GUIString LobbyContainer_NoConnection_String; // 0x9664a4
extern WONAPI::GUIString LobbyContainer_DNSLookupFailed_String; // 0x96663c
extern WONAPI::GUIString LobbyContainer_DirLookupFailure_String; // 0x9665fc
extern WONAPI::GUIString LobbyContainer_VersionCheckFailure_String; // 0x96639c
extern WONAPI::GUIString LobbyContainer_InitComplete_String; // 0x9669c0
extern WONAPI::GUIString LobbyContainer_NoServers_String; // 0x966aac
extern WONAPI::GUIString LobbyContainer_AuthServers_String; // 0x966788
extern WONAPI::GUIString LobbyContainer_AccountServers_String; // 0x966660
extern WONAPI::GUIString LobbyContainer_VersionServers_String; // 0x966724
extern WONAPI::GUIString LobbyContainer_CreateAccountTitle_String; // 0x966924
extern WONAPI::GUIString LobbyContainer_CreateAccountDetails_String; // 0x9669a8
extern WONAPI::GUIString LobbyContainer_RetrievePasswordTitle_String; // 0x966720
extern WONAPI::GUIString LobbyContainer_RetrievePasswordDetails_String; // 0x9663a0
extern WONAPI::GUIString LobbyContainer_RetrieveUsernameTitle_String; // 0x966804
extern WONAPI::GUIString LobbyContainer_RetrieveUsernameDetails_String; // 0x966824
extern WONAPI::GUIString LobbyContainer_QueryAccountTitle_String; // 0x966510
extern WONAPI::GUIString LobbyContainer_QueryAccountDetails_String; // 0x96664c
extern WONAPI::GUIString LobbyContainer_UpdateAccountTitle_String; // 0x9665ec
extern WONAPI::GUIString LobbyContainer_UpdateAccountDetails_String; // 0x966ae0
extern WONAPI::GUIString LobbyContainer_LoginTitle_String; // 0x9667f4
extern WONAPI::GUIString LobbyContainer_LoginDetails_String; // 0x966a10
extern WONAPI::GUIString LobbyContainer_GettingRoomsTitle_String; // 0x966610
extern WONAPI::GUIString LobbyContainer_GettingRoomsDetails_String; // 0x96696c
extern WONAPI::GUIString LobbyContainer_VersionOutOfDataTitle_String; // 0x9668f0
extern WONAPI::GUIString LobbyContainer_VersionOutOfDataDetails_String; // 0x9668ec
extern WONAPI::GUIString LobbyGameStatus_Success_String; // 0x966550
extern WONAPI::GUIString LobbyGameStatus_GameFull_String; // 0x966578
extern WONAPI::GUIString LobbyGameStatus_GameInProgress_String; // 0x966810
extern WONAPI::GUIString LobbyGameStatus_UnpackFailure_String; // 0x966870
extern WONAPI::GUIString LobbyGameStatus_NotInvited_String; // 0x9665d8
extern WONAPI::GUIString LobbyGameStatus_CaptainRejectedYou_String; // 0x966640
extern WONAPI::GUIString LobbyGameStatus_InvalidPassword_String; // 0x96659c
extern WONAPI::GUIString LobbyGameStatus_DuplicateName_String; // 0x96648c
extern WONAPI::GUIString LobbyOptions_DefaultName_String; // 0x966488
extern WONAPI::GUIString LobbyOptions_CheckingAddress_String; // 0x9666f0
extern WONAPI::GUIString LobbyOptions_AddressUnavailable_String; // 0x966628
extern WONAPI::GUIString RoomCtrl_QueryingServer_String; // 0x966838
extern WONAPI::SmartPtr<WONAPI::Image> RoomCtrl_Password_Image; // 0x9664f4
extern WONAPI::GUIString RoomLabelCtrl_LabelPrefix_String; // 0x9663b8
extern WONAPI::GUIString RoomLabelCtrl_LabelServer_String; // 0x9664b0
extern WONAPI::GUIString RoomLabelCtrl_LabelUser_String; // 0x966688
extern WONAPI::GUIString RoomLabelCtrl_LabelUsers_String; // 0x9669ec
extern WONAPI::GUIString RoomLabelCtrl_LabelDisconnected_String; // 0x966764
extern WONAPI::GUIString RoutingLogic_GetClientListTitle_String; // 0x9669e4
extern WONAPI::GUIString RoutingLogic_GetClientListDetails_String; // 0x966a3c
extern WONAPI::GUIString RoutingLogic_GetGroupListTitle_String; // 0x966ab4
extern WONAPI::GUIString RoutingLogic_GetGroupListDetails_String; // 0x966a1c
extern WONAPI::GUIString RoutingLogic_GameDissolvedTitle_String; // 0x966470
extern WONAPI::GUIString RoutingLogic_GameDissolvedDetails_String; // 0x966994
extern WONAPI::GUIString RoutingLogic_NoServersTitle_String; // 0x966534
extern WONAPI::GUIString RoutingLogic_NoServersDetails_String; // 0x9664e0
extern WONAPI::GUIString RoutingLogic_TooManyFriendsTitle_String; // 0x9666b8
extern WONAPI::GUIString RoutingLogic_TooManyFriendsDetails_String; // 0x966678
extern WONAPI::GUIString RoutingLogic_AlreadyJoinedTitle_String; // 0x9667ac
extern WONAPI::GUIString RoutingLogic_AlreadyInGame_String; // 0x966504
extern WONAPI::GUIString RoutingLogic_AlreadyInRoom_String; // 0x96695c
extern WONAPI::GUIString RoutingLogic_JoinGroupTitle_String; // 0x9667dc
extern WONAPI::GUIString RoutingLogic_JoinGroupDetails_String; // 0x9663c0
extern WONAPI::GUIString RoutingLogic_ConnectTitle_String; // 0x966490
extern WONAPI::GUIString RoutingLogic_ConnectDetails_String; // 0x9664fc
extern WONAPI::GUIString RoutingLogic_RegisterTitle_String; // 0x9669d8
extern WONAPI::GUIString RoutingLogic_RegisterDetails_String; // 0x966618
extern WONAPI::GUIString RoutingLogic_QueryGroupsTitle_String; // 0x966854
extern WONAPI::GUIString RoutingLogic_QueryGroupsDetails_String; // 0x966650
extern WONAPI::GUIString RoutingLogic_CreateGroupTitle_String; // 0x96651c
extern WONAPI::GUIString RoutingLogic_CreateGroupDetails_String; // 0x9667a4
extern WONAPI::GUIString RoutingLogic_GetBadUsersTitle_String; // 0x9664d8
extern WONAPI::GUIString RoutingLogic_GetBadUsersDetails_String; // 0x966abc
extern WONAPI::GUIString RoutingLogic_DisconnectedTitle_String; // 0x9666d0
extern WONAPI::GUIString RoutingLogic_DisconnectedDetails_String; // 0x966a6c
extern WONAPI::GUIString RoutingLogic_AskingCaptainTitle_String; // 0x9665a0
extern WONAPI::GUIString RoutingLogic_AskingCaptainDetails_String; // 0x966a84
extern WONAPI::GUIString RoutingLogic_CollectingPingsTitle_String; // 0x966884
extern WONAPI::GUIString RoutingLogic_CollectingPingsDetails_String; // 0x966778
extern WONAPI::GUIString RoutingLogic_GettingGameListTitle_String; // 0x96665c
extern WONAPI::GUIString RoutingLogic_GettingGameListDetails_String; // 0x96673c
extern WONAPI::GUIString RoutingLogic_MuteClientTitle_String; // 0x9667e8
extern WONAPI::GUIString RoutingLogic_UnmuteClientTitle_String; // 0x966844
extern WONAPI::GUIString RoutingLogic_BanClientTitle_String; // 0x966880
extern WONAPI::GUIString RoutingLogic_KickClientTitle_String; // 0x9663c8
extern WONAPI::GUIString RoutingLogic_ModerateDetails_String; // 0x966408
extern WONAPI::GUIString RoutingLogic_FriendNotFoundTitle_String; // 0x9666c0
extern WONAPI::GUIString RoutingLogic_FriendNotFoundDetails_String; // 0x966738
extern WONAPI::GUIString RoutingLogic_ClientNotFound_String; // 0x966784
extern WONAPI::GUIString RoutingLogic_ClientAmbiguous_String; // 0x966518
extern WONAPI::GUIString RoutingLogic_InvalidCommand_String; // 0x9669b8
extern WONAPI::GUIString RoutingLogic_RequireName_String; // 0x966590
extern WONAPI::GUIString RoutingLogic_BadTimeSpec_String; // 0x966a24
extern WONAPI::GUIString RoutingLogic_Error_String; // 0x96678c
extern WONAPI::GUIString RoutingLogic_NotCaptain_String; // 0x966750
extern WONAPI::GUIString ServerListCtrl_Pinging_String; // 0x96657c
extern WONAPI::GUIString ServerListCtrl_PingFailed_String; // 0x96687c
extern WONAPI::GUIString ServerListCtrl_Querying_String; // 0x966918
extern WONAPI::GUIString ServerListCtrl_QueryFailed_String; // 0x966404
extern WONAPI::GUIString ServerListCtrl_ListSummary_String; // 0x9665c4
extern WONAPI::GUIString ServerListCtrl_ListPingSummary_String; // 0x966950
extern WONAPI::GUIString ServerListCtrl_RefreshList_String; // 0x966868
extern WONAPI::GUIString ServerListCtrl_StopRefresh_String; // 0x9664dc
extern WONAPI::GUIString ServerListCtrl_NoFilter_String; // 0x96671c
extern WONAPI::GUIString ServerListCtrl_AddRemove_String; // 0x966540
extern WONAPI::GUIString ServerListCtrl_Add_String; // 0x966474
extern WONAPI::GUIString ServerListCtrl_Remove_String; // 0x966670
extern WONAPI::GUIString ServerListCtrl_FullFilter_String; // 0x966a38
extern WONAPI::GUIString ServerListCtrl_NotFullFilter_String; // 0x966894
extern WONAPI::GUIString ServerListCtrl_OpenFilter_String; // 0x9669cc
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_PingFailed_Image; // 0x9666bc
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameExpanded_Image; // 0x9664f8
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameNotExpanded_Image; // 0x966930
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameAreInvited_Image; // 0x9669e8
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameNone_Image; // 0x966a80
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameAskToJoin_Image; // 0x96645c
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameHasPassword_Image; // 0x9663e0
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameNotInvited_Image; // 0x9666fc
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_GameInProgress_Image; // 0x966830
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_SkillOpen_Image; // 0x966a7c
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_SkillNovice_Image; // 0x9663dc
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_SkillIntermediate_Image; // 0x96658c
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_SkillAdvanced_Image; // 0x9667cc
extern WONAPI::SmartPtr<WONAPI::Image> ServerListCtrl_SkillExpert_Image; // 0x9668e8
extern WONAPI::GUIString ServerOptionsCtrl_Connect_String; // 0x966524
extern WONAPI::GUIString ServerOptionsCtrl_RefreshServer_String; // 0x966990
extern WONAPI::GUIString ServerOptionsCtrl_HideDetails_String; // 0x96685c
extern WONAPI::GUIString ServerOptionsCtrl_ShowDetails_String; // 0x9663ac
extern WONAPI::GUIString ServerOptionsCtrl_ResfreshList_String; // 0x9666b4
extern WONAPI::GUIString StagingLogic_DisconnectedTitle_String; // 0x9663b4
extern WONAPI::GUIString StagingLogic_DisconnectedDetails_String; // 0x9669d4
extern WONAPI::GUIString StagingLogic_BannedTitle_String; // 0x966434
extern WONAPI::GUIString StagingLogic_BannedDetails_String; // 0x966850
extern WONAPI::GUIString StagingLogic_KickedTitle_String; // 0x9667c8
extern WONAPI::GUIString StagingLogic_KickedDetails_String; // 0x966624
extern WONAPI::GUIString UserListCtrl_Ignore_String; // 0x96644c
extern WONAPI::GUIString UserListCtrl_Unignore_String; // 0x966a40
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Normal_Image; // 0x966698
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Away_Image; // 0x966560
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Admin_Image; // 0x966aa8
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Moderator_Image; // 0x9665f4
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Captain_Image; // 0x966aa0
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Muted_Image; // 0x9665e8
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Ignored_Image; // 0x966a20
extern WONAPI::SmartPtr<WONAPI::Image> UserListCtrl_Blocked_Image; // 0x9668cc
extern WONAPI::GUIString WONStatus_Success_String; // 0x966584
extern WONAPI::GUIString WONStatus_Killed_String; // 0x966a68
extern WONAPI::GUIString WONStatus_TimedOut_String; // 0x966630
extern WONAPI::GUIString WONStatus_InvalidAddress_String; // 0x966538
extern WONAPI::GUIString WONStatus_GeneralFailure_String; // 0x9663b0
extern WONAPI::GUIString WONStatus_AuthFailure_String; // 0x966a4c
extern WONAPI::GUIString WONStatus_FTPFailure_String; // 0x96666c
extern WONAPI::GUIString WONStatus_HTTPFailure_String; // 0x9665b0
extern WONAPI::GUIString WONStatus_MOTDFailure_String; // 0x966a88
extern WONAPI::GUIString WONStatus_RoutingFailure_String; // 0x9668b4
extern WONAPI::GUIString WONStatus_GameSpyFailure_String; // 0x966554
extern WONAPI::GUIString WONStatus_DirFailure_String; // 0x966754
extern WONAPI::GUIString WONStatus_ChatFailure_String; // 0x9669c4
extern WONAPI::GUIString WONStatus_DatabaseFailure_String; // 0x9668a8
extern WONAPI::GUIString WONStatus_NotAllowed_String; // 0x9666d4
extern WONAPI::GUIString WONStatus_NotAuthorized_String; // 0x966484
extern WONAPI::GUIString WONStatus_BehindFirewall_String; // 0x966ac0
extern WONAPI::GUIString WONStatus_ExpiredCertificate_String; // 0x9667f8
extern WONAPI::GUIString WONStatus_InvalidCDKey_String; // 0x9663a4
extern WONAPI::GUIString WONStatus_CDKeyBanned_String; // 0x966520
extern WONAPI::GUIString WONStatus_CDKeyInUse_String; // 0x966514
extern WONAPI::GUIString WONStatus_CRCFailed_String; // 0x96670c
extern WONAPI::GUIString WONStatus_UserExists_String; // 0x966680
extern WONAPI::GUIString WONStatus_UserNotFound_String; // 0x966978
extern WONAPI::GUIString WONStatus_BadPassword_String; // 0x9667e0
extern WONAPI::GUIString WONStatus_InvalidUserName_String; // 0x9664ac
extern WONAPI::GUIString WONStatus_BadCommunity_String; // 0x966a60
extern WONAPI::GUIString WONStatus_NotInCommunity_String; // 0x966454
extern WONAPI::GUIString WONStatus_UserSeqInUse_String; // 0x96698c
extern WONAPI::GUIString WONStatus_BlankNamesNotAllowed_String; // 0x966a94
extern WONAPI::GUIString WONStatus_CaptainRejectedYou_String; // 0x966a8c
extern WONAPI::GUIString WONStatus_ClientAlreadyBanned_String; // 0x9663a8
extern WONAPI::GUIString WONStatus_ClientAlreadyInGroup_String; // 0x9665bc
extern WONAPI::GUIString WONStatus_ClientAlreadyMuted_String; // 0x966420
extern WONAPI::GUIString WONStatus_ClientBanned_String; // 0x966840
extern WONAPI::GUIString WONStatus_ClientDoesNotExist_String; // 0x9663f0
extern WONAPI::GUIString WONStatus_ClientNotBanned_String; // 0x9665e0
extern WONAPI::GUIString WONStatus_ClientNotInGroup_String; // 0x966834
extern WONAPI::GUIString WONStatus_ClientNotInvited_String; // 0x966508
extern WONAPI::GUIString WONStatus_ClientNotMuted_String; // 0x966864
extern WONAPI::GUIString WONStatus_ConnectFailure_String; // 0x9666ec
extern WONAPI::GUIString WONStatus_GroupAlreadyClaimed_String; // 0x9664c4
extern WONAPI::GUIString WONStatus_GroupAlreadyExists_String; // 0x9667ec
extern WONAPI::GUIString WONStatus_GroupClosed_String; // 0x9665d4
extern WONAPI::GUIString WONStatus_GroupDeleted_String; // 0x966910
extern WONAPI::GUIString WONStatus_GroupDoesNotExist_String; // 0x96680c
extern WONAPI::GUIString WONStatus_GroupFull_String; // 0x966798
extern WONAPI::GUIString WONStatus_GuestNamesReserved_String; // 0x966570
extern WONAPI::GUIString WONStatus_InvalidPassword_String; // 0x9669f0
extern WONAPI::GUIString WONStatus_MustBeAdmin_String; // 0x9668bc
extern WONAPI::GUIString WONStatus_MustBeCaptain_String; // 0x966ae8
extern WONAPI::GUIString WONStatus_MustBeCaptainOrModerator_String; // 0x96643c
extern WONAPI::GUIString WONStatus_MustBeModerator_String; // 0x966668
extern WONAPI::GUIString WONStatus_ServerFull_String; // 0x966a54
extern WONAPI::GUIString WONStatus_GroupNameExceedsMaximum_String; // 0x9664c0
extern WONAPI::GUIString WONStatus_JoinCanceled_String; // 0x9664f0
extern WONAPI::GUIString WONStatus_Throttled_String; // 0x966adc
extern WONAPI::GUIString WONStatus_KeyNotUsed_String; // 0x9666e8
extern WONAPI::GUIString WONStatus_EMailPasswordDuplication_String; // 0x96672c
extern WONAPI::GUIString WONStatus_AccountCreateDisabled_String; // 0x9666b0
extern WONAPI::GUIString WONStatus_AgeNotOldEnough_String; // 0x96655c
extern WONAPI::GUIString WONStatus_ItemHasDependencies_String; // 0x966948
extern WONAPI::GUIString WONStatus_OutOfDate_String; // 0x966928
extern WONAPI::GUIString WONStatus_OutOfDateNoUpdate_String; // 0x966774
extern WONAPI::GUIString WONStatus_ValidNotLatest_String; // 0x966728
extern WONAPI::GUIString WONStatus_DataAlreadyExists_String; // 0x966564
extern WONAPI::GUIString WONStatus_DBError_String; // 0x9664d4
extern WONAPI::GUIString WONStatus_ConnRefused_String; // 0x9668f8

enum LobbyResourceCtrlId {
    LobbyResourceCtrlId_Min=200000, // ControlId_UserMin; the first control ID is 200001
    ID_ShowCreateAccount,
    ID_ShowLostPassword,
    ID_ShowLostUserName,
    ID_ShowMOTD,
    ID_ShowTOU,
    ID_LoginScreenBack,
    ID_ShowLoginHelp,
    ID_ShowInternetScreen,
    ID_ShowLanScreen,
    ID_Logout,
    ID_HostGame,
    ID_ChangeRoom,
    ID_JoinGame,
    ID_LeaveGame,
    ID_StartGame,
    ID_RefreshGameList,
    ID_AddFriend,
    ID_RefreshFriends,
    ID_AddLANName,
    ID_ChangeLANBroadcast,
    ID_FilterDirtyWordsCheck,
    ID_AnonymousToFriendsCheck,
    ID_ChooseNetworkAdapter,
};

enum LobbyResourceId {
    LobbyGlobal_Cancel_String_Id,
    LobbyGlobal_Lan_String_Id,
    LobbyGlobal_No_String_Id,
    LobbyGlobal_OK_String_Id,
    LobbyGlobal_Yes_String_Id,
    LobbyGlobal_QuickPlay_String_Id,
    LobbyGlobal_Searching_Image_Id,
    LobbyGlobal_Firewall_Image_Id,
    LobbyGlobal_ButtonClick_Sound_Id,
    LobbyGlobal_LobbyMusic_Sound_Id,
    LobbyGlobal_AllReady_Sound_Id,
    LobbyGlobal_AllNotReady_Sound_Id,
    LobbyGlobal_AskToJoinRecv_Sound_Id,
    LobbyGlobal_Error_Sound_Id,
    LobbyGlobal_FriendJoined_Sound_Id,
    LobbyGlobal_FriendLeft_Sound_Id,
    LobbyGlobal_InviteRecv_Sound_Id,
    LobbyGlobal_PlayerJoined_Sound_Id,
    LobbyGlobal_SierraLogon_Sound_Id,
    LobbyGlobal_WhisperRecv_Sound_Id,
    AccountLogic_InvalidCDKeyTitle_String_Id,
    AccountLogic_InvalidCDKeyDetails_String_Id,
    AddFriendDialog_StatusTitle_String_Id,
    AddFriendDialog_StatusDetails_String_Id,
    AdminActionDialog_Warn_String_Id,
    AdminActionDialog_Mute_String_Id,
    AdminActionDialog_UnMute_String_Id,
    AdminActionDialog_Kick_String_Id,
    AdminActionDialog_Ban_String_Id,
    AdminActionDialog_ExplanationOpt_String_Id,
    AdminActionDialog_ExplanationReq_String_Id,
    BadUserDialog_OneDay_String_Id,
    BadUserDialog_OneHour_String_Id,
    BadUserDialog_OneMinute_String_Id,
    BadUserDialog_OneSecond_String_Id,
    BadUserDialog_XDays_String_Id,
    BadUserDialog_XHours_String_Id,
    BadUserDialog_XMinutes_String_Id,
    BadUserDialog_XSeconds_String_Id,
    BadUserDialog_Infinite_String_Id,
    ChatCommandLogic_Whisper_String_Id,
    ChatCommandLogic_Block_String_Id,
    ChatCommandLogic_Emote_String_Id,
    ChatCommandLogic_Ignore_String_Id,
    ChatCommandLogic_Away_String_Id,
    ChatCommandLogic_Invite_String_Id,
    ChatCommandLogic_Uninvite_String_Id,
    ChatCommandLogic_Reply_String_Id,
    ChatCommandLogic_Clear_String_Id,
    ChatCommandLogic_Help_String_Id,
    ChatCommandLogic_ShowTeam_String_Id,
    ChatCommandLogic_Moderator_String_Id,
    ChatCommandLogic_ServerMute_String_Id,
    ChatCommandLogic_ServerBan_String_Id,
    ChatCommandLogic_Mute_String_Id,
    ChatCommandLogic_Ban_String_Id,
    ChatCommandLogic_Unmute_String_Id,
    ChatCommandLogic_ServerUnmute_String_Id,
    ChatCommandLogic_Unban_String_Id,
    ChatCommandLogic_ServerUnban_String_Id,
    ChatCommandLogic_Alert_String_Id,
    ChatCommandLogic_Warn_String_Id,
    ChatCommandLogic_HelpHeader_String_Id,
    ChatCommandLogic_HelpHelp_String_Id,
    ChatCommandLogic_ShowTeamHelp_String_Id,
    ChatCommandLogic_WhisperHelp_String_Id,
    ChatCommandLogic_BlockHelp_String_Id,
    ChatCommandLogic_IgnoreHelp_String_Id,
    ChatCommandLogic_ReplyHelp_String_Id,
    ChatCommandLogic_EmoteHelp_String_Id,
    ChatCommandLogic_AwayHelp_String_Id,
    ChatCommandLogic_ClearHelp_String_Id,
    ChatCommandLogic_InviteHelp_String_Id,
    ChatCommandLogic_UninviteHelp_String_Id,
    ChatCommandLogic_MuteHelp_String_Id,
    ChatCommandLogic_UnmuteHelp_String_Id,
    ChatCommandLogic_BanHelp_String_Id,
    ChatCommandLogic_UnbanHelp_String_Id,
    ChatCommandLogic_ModeratorHelp_String_Id,
    ChatCommandLogic_ServerMuteHelp_String_Id,
    ChatCommandLogic_ServerUnmuteHelp_String_Id,
    ChatCommandLogic_ServerBanHelp_String_Id,
    ChatCommandLogic_ServerUnbanHelp_String_Id,
    ChatCommandLogic_AlertHelp_String_Id,
    ChatCommandLogic_WarnHelp_String_Id,
    ChatCtrl_NormalChat_String_Id,
    ChatCtrl_Emote_String_Id,
    ChatCtrl_TeamChat_String_Id,
    ChatCtrl_TeamEmote_String_Id,
    ChatCtrl_YouWhisperToYourself_String_Id,
    ChatCtrl_YouWhisper_String_Id,
    ChatCtrl_SomeoneWhispersToYou_String_Id,
    ChatCtrl_YouWarnSomeone_String_Id,
    ChatCtrl_YouWarnYourself_String_Id,
    ChatCtrl_SomeoneWarnsYou_String_Id,
    ChatCtrl_SomeoneWarnsSomeone_String_Id,
    ChatCtrl_EnteringRoom_String_Id,
    ChatCtrl_LeavingRoom_String_Id,
    ChatCtrl_Disconnected_String_Id,
    ChatCtrl_YouWereKicked_String_Id,
    ChatCtrl_YouWereBannedMinute_String_Id,
    ChatCtrl_YouWereBannedMinutes_String_Id,
    ChatCtrl_YouWereBannedHour_String_Id,
    ChatCtrl_YouWereBannedHours_String_Id,
    ChatCtrl_YouWereBannedDay_String_Id,
    ChatCtrl_YouWereBannedDays_String_Id,
    ChatCtrl_YouWereBannedIndefinitely_String_Id,
    ChatCtrl_SomeoneWasKicked_String_Id,
    ChatCtrl_SomeoneWasBannedMinute_String_Id,
    ChatCtrl_SomeoneWasBannedMinutes_String_Id,
    ChatCtrl_SomeoneWasBannedHour_String_Id,
    ChatCtrl_SomeoneWasBannedHours_String_Id,
    ChatCtrl_SomeoneWasBannedDay_String_Id,
    ChatCtrl_SomeoneWasBannedDays_String_Id,
    ChatCtrl_SomeoneWasBannedIndefinitely_String_Id,
    ChatCtrl_YouWereUnmuted_String_Id,
    ChatCtrl_YouWereMutedMinute_String_Id,
    ChatCtrl_YouWereMutedMinutes_String_Id,
    ChatCtrl_YouWereMutedHour_String_Id,
    ChatCtrl_YouWereMutedHours_String_Id,
    ChatCtrl_YouWereMutedDay_String_Id,
    ChatCtrl_YouWereMutedDays_String_Id,
    ChatCtrl_YouWereMutedIndefinitely_String_Id,
    ChatCtrl_SomeoneWasUnmuted_String_Id,
    ChatCtrl_SomeoneWasMutedMinute_String_Id,
    ChatCtrl_SomeoneWasMutedMinutes_String_Id,
    ChatCtrl_SomeoneWasMutedHour_String_Id,
    ChatCtrl_SomeoneWasMutedHours_String_Id,
    ChatCtrl_SomeoneWasMutedDay_String_Id,
    ChatCtrl_SomeoneWasMutedDays_String_Id,
    ChatCtrl_SomeoneWasMutedIndefinitely_String_Id,
    ChatCtrl_ModeratorComment_String_Id,
    ChatCtrl_AlertComment_String_Id,
    ChatCtrl_ServerAlert_String_Id,
    ChatCtrl_ServerShutdownStartedMinute_String_Id,
    ChatCtrl_ServerShutdownStartedMinutes_String_Id,
    ChatCtrl_ServerShutdownStartedHour_String_Id,
    ChatCtrl_ServerShutdownStartedHours_String_Id,
    ChatCtrl_ServerShutdownStartedDay_String_Id,
    ChatCtrl_ServerShutdownStartedDays_String_Id,
    ChatCtrl_ServerShutdownAborted_String_Id,
    ChatCtrl_SomeoneInvitesYou_String_Id,
    ChatCtrl_SomeoneUninvitesYou_String_Id,
    ChatCtrl_YouInviteSomeone_String_Id,
    ChatCtrl_YouUninviteSomeone_String_Id,
    ChatCtrl_SomeoneInvitesYouExpl_String_Id,
    ChatCtrl_SomeoneUninvitesYouExpl_String_Id,
    ChatCtrl_YouInviteSomeoneExpl_String_Id,
    ChatCtrl_YouUninviteSomeoneExpl_String_Id,
    ChatCtrl_Accept_String_Id,
    ChatCtrl_Reject_String_Id,
    ChatCtrl_SomeoneAsksToJoin_String_Id,
    ChatCtrl_SomeoneAsksToJoinExpl_String_Id,
    ChatCtrl_ClientNotOnServer_String_Id,
    ChatCtrl_ClientInGame_String_Id,
    ChatCtrl_ClientInChat_String_Id,
    ChatCtrl_ClientInGameAndChat_String_Id,
    ChatCtrl_ClientIsAnonymous_String_Id,
    ChatCtrl_FriendIsChatting_String_Id,
    ChatCtrl_FriendIsInGameStaging_String_Id,
    ChatCtrl_FriendIsPlaying_String_Id,
    ChatCtrl_FriendIsOn_String_Id,
    ChatCtrl_FriendIsNotOnline_String_Id,
    ChatCtrl_PlayerKicked_String_Id,
    ChatCtrl_PlayerBanned_String_Id,
    ChatCtrl_FriendJoined_String_Id,
    ChatCtrl_FriendLeft_String_Id,
    ChatCtrl_TeamMemberJoined_String_Id,
    ChatCtrl_TeamMemberLeft_String_Id,
    ChatCtrl_TeamMembers_String_Id,
    ChatCtrl_IgnoreList_String_Id,
    ChooseNetworkAdapterDialog_Default_String_Id,
    ChooseNetworkAdapterDialog_Unknown_String_Id,
    ClientOptionsCtrl_Whisper_String_Id,
    ClientOptionsCtrl_Invite_String_Id,
    ClientOptionsCtrl_Uninvite_String_Id,
    ClientOptionsCtrl_AddToFriends_String_Id,
    ClientOptionsCtrl_RemoveFromFriends_String_Id,
    ClientOptionsCtrl_ClearAFK_String_Id,
    ClientOptionsCtrl_SetAFK_String_Id,
    ClientOptionsCtrl_Ignore_String_Id,
    ClientOptionsCtrl_Unignore_String_Id,
    ClientOptionsCtrl_Block_String_Id,
    ClientOptionsCtrl_Unblock_String_Id,
    ClientOptionsCtrl_Warn_String_Id,
    ClientOptionsCtrl_Mute_String_Id,
    ClientOptionsCtrl_UnMute_String_Id,
    ClientOptionsCtrl_Kick_String_Id,
    ClientOptionsCtrl_Ban_String_Id,
    DirectConnectionOptionsCtrl_Connect_String_Id,
    DirectConnectionOptionsCtrl_Refresh_String_Id,
    DirectConnectionOptionsCtrl_Remove_String_Id,
    FriendOptionsCtrl_Locate_String_Id,
    FriendOptionsCtrl_RefreshList_String_Id,
    FriendOptionsCtrl_RemoveFromFriends_String_Id,
    FriendsListCtrl_Unknown_Image_Id,
    FriendsListCtrl_NotFound_Image_Id,
    FriendsListCtrl_InChat_Image_Id,
    FriendsListCtrl_InGame_Image_Id,
    GameOptionsPopup_Join_String_Id,
    GameOptionsPopup_Ping_String_Id,
    GameOptionsPopup_QueryDetails_String_Id,
    GameOptionsPopup_HideDetails_String_Id,
    GameOptionsPopup_ShowDetails_String_Id,
    GameOptionsPopup_Remove_String_Id,
    GameStagingCtrl_Ready_String_Id,
    GameStagingCtrl_NotReady_String_Id,
    GameStagingCtrl_StartGame_String_Id,
    GameStagingCtrl_LeaveGame_String_Id,
    GameStagingCtrl_DissolveGame_String_Id,
    GameStagingLogic_KickedTitle_String_Id,
    GameStagingLogic_KickedDetails_String_Id,
    GameStagingLogic_BannedTitle_String_Id,
    GameStagingLogic_BannedDetails_String_Id,
    GameStagingLogic_ConnectWarningTitle1_String_Id,
    GameStagingLogic_ConnectWarningDetails1_String_Id,
    GameStagingLogic_ConnectWarningTitle2_String_Id,
    GameStagingLogic_ConnectWarningDetails2_String_Id,
    GameStagingLogic_ConnectWarningTitle3_String_Id,
    GameStagingLogic_ConnectWarningDetails3_String_Id,
    GameStagingLogic_NoPingReplyError_String_Id,
    GameStagingLogic_GettingGameInfoTitle_String_Id,
    GameStagingLogic_GettingGameInfoDetails_String_Id,
    GameStagingLogic_JoinGameErrorTitle_String_Id,
    GameStagingLogic_VerifyingPingTitle_String_Id,
    GameStagingLogic_VerifyingPingDetails_String_Id,
    GameStagingLogic_DissolveGame_String_Id,
    GameStagingLogic_DissolveGameConfirm_String_Id,
    GameStagingLogic_LeaveGame_String_Id,
    GameStagingLogic_LeaveGameConfirm_String_Id,
    GameStagingLogic_NoQuickPlayGameTitle_String_Id,
    GameStagingLogic_NoQuickPlayGameDetails_String_Id,
    GameUserListCtrl_Ban_String_Id,
    GameUserListCtrl_Kick_String_Id,
    GameUserListCtrl_Ready_Image_Id,
    InternetQuickPlay_Welcome_String_Id,
    InternetQuickPlay_UserWelcome_String_Id,
    JoinResponse_Joined_String_Id,
    JoinResponse_ServerFull_String_Id,
    JoinResponse_InviteOnly_String_Id,
    JoinResponse_AskToJoin_String_Id,
    JoinResponse_GameInProgress_String_Id,
    JoinResponse_InvalidPassword_String_Id,
    JoinResponse_NotOnLAN_String_Id,
    JoinResponse_Blocked_String_Id,
    JoinResponse_Unknown_String_Id,
    LANLogic_SocketBindError_String_Id,
    LANLogic_SocketListenError_String_Id,
    LANLogic_ConnectToGameTitle_String_Id,
    LANLogic_ConnectToGameDetails_String_Id,
    LANLogic_CheckingAddressTitle_String_Id,
    LANLogic_CheckingAddressDetails_String_Id,
    LANLogic_RegisterTitle_String_Id,
    LANLogic_RegisterDetails_String_Id,
    LANLogic_JoinFailure_String_Id,
    LANLogic_JoinReplyError_String_Id,
    LANLogic_InvalidSubnet_String_Id,
    LobbyContainer_InitStart_String_Id,
    LobbyContainer_InitFailed_String_Id,
    LobbyContainer_CheckingVersion_String_Id,
    LobbyContainer_QueryingServers_String_Id,
    LobbyContainer_NoConnection_String_Id,
    LobbyContainer_DNSLookupFailed_String_Id,
    LobbyContainer_DirLookupFailure_String_Id,
    LobbyContainer_VersionCheckFailure_String_Id,
    LobbyContainer_InitComplete_String_Id,
    LobbyContainer_NoServers_String_Id,
    LobbyContainer_AuthServers_String_Id,
    LobbyContainer_AccountServers_String_Id,
    LobbyContainer_VersionServers_String_Id,
    LobbyContainer_CreateAccountTitle_String_Id,
    LobbyContainer_CreateAccountDetails_String_Id,
    LobbyContainer_RetrievePasswordTitle_String_Id,
    LobbyContainer_RetrievePasswordDetails_String_Id,
    LobbyContainer_RetrieveUsernameTitle_String_Id,
    LobbyContainer_RetrieveUsernameDetails_String_Id,
    LobbyContainer_QueryAccountTitle_String_Id,
    LobbyContainer_QueryAccountDetails_String_Id,
    LobbyContainer_UpdateAccountTitle_String_Id,
    LobbyContainer_UpdateAccountDetails_String_Id,
    LobbyContainer_LoginTitle_String_Id,
    LobbyContainer_LoginDetails_String_Id,
    LobbyContainer_GettingRoomsTitle_String_Id,
    LobbyContainer_GettingRoomsDetails_String_Id,
    LobbyContainer_VersionOutOfDataTitle_String_Id,
    LobbyContainer_VersionOutOfDataDetails_String_Id,
    LobbyGameStatus_Success_String_Id,
    LobbyGameStatus_GameFull_String_Id,
    LobbyGameStatus_GameInProgress_String_Id,
    LobbyGameStatus_UnpackFailure_String_Id,
    LobbyGameStatus_NotInvited_String_Id,
    LobbyGameStatus_CaptainRejectedYou_String_Id,
    LobbyGameStatus_InvalidPassword_String_Id,
    LobbyGameStatus_DuplicateName_String_Id,
    LobbyOptions_DefaultName_String_Id,
    LobbyOptions_CheckingAddress_String_Id,
    LobbyOptions_AddressUnavailable_String_Id,
    RoomCtrl_QueryingServer_String_Id,
    RoomCtrl_Password_Image_Id,
    RoomLabelCtrl_LabelPrefix_String_Id,
    RoomLabelCtrl_LabelServer_String_Id,
    RoomLabelCtrl_LabelUser_String_Id,
    RoomLabelCtrl_LabelUsers_String_Id,
    RoomLabelCtrl_LabelDisconnected_String_Id,
    RoutingLogic_GetClientListTitle_String_Id,
    RoutingLogic_GetClientListDetails_String_Id,
    RoutingLogic_GetGroupListTitle_String_Id,
    RoutingLogic_GetGroupListDetails_String_Id,
    RoutingLogic_GameDissolvedTitle_String_Id,
    RoutingLogic_GameDissolvedDetails_String_Id,
    RoutingLogic_NoServersTitle_String_Id,
    RoutingLogic_NoServersDetails_String_Id,
    RoutingLogic_TooManyFriendsTitle_String_Id,
    RoutingLogic_TooManyFriendsDetails_String_Id,
    RoutingLogic_AlreadyJoinedTitle_String_Id,
    RoutingLogic_AlreadyInGame_String_Id,
    RoutingLogic_AlreadyInRoom_String_Id,
    RoutingLogic_JoinGroupTitle_String_Id,
    RoutingLogic_JoinGroupDetails_String_Id,
    RoutingLogic_ConnectTitle_String_Id,
    RoutingLogic_ConnectDetails_String_Id,
    RoutingLogic_RegisterTitle_String_Id,
    RoutingLogic_RegisterDetails_String_Id,
    RoutingLogic_QueryGroupsTitle_String_Id,
    RoutingLogic_QueryGroupsDetails_String_Id,
    RoutingLogic_CreateGroupTitle_String_Id,
    RoutingLogic_CreateGroupDetails_String_Id,
    RoutingLogic_GetBadUsersTitle_String_Id,
    RoutingLogic_GetBadUsersDetails_String_Id,
    RoutingLogic_DisconnectedTitle_String_Id,
    RoutingLogic_DisconnectedDetails_String_Id,
    RoutingLogic_AskingCaptainTitle_String_Id,
    RoutingLogic_AskingCaptainDetails_String_Id,
    RoutingLogic_CollectingPingsTitle_String_Id,
    RoutingLogic_CollectingPingsDetails_String_Id,
    RoutingLogic_GettingGameListTitle_String_Id,
    RoutingLogic_GettingGameListDetails_String_Id,
    RoutingLogic_MuteClientTitle_String_Id,
    RoutingLogic_UnmuteClientTitle_String_Id,
    RoutingLogic_BanClientTitle_String_Id,
    RoutingLogic_KickClientTitle_String_Id,
    RoutingLogic_ModerateDetails_String_Id,
    RoutingLogic_FriendNotFoundTitle_String_Id,
    RoutingLogic_FriendNotFoundDetails_String_Id,
    RoutingLogic_ClientNotFound_String_Id,
    RoutingLogic_ClientAmbiguous_String_Id,
    RoutingLogic_InvalidCommand_String_Id,
    RoutingLogic_RequireName_String_Id,
    RoutingLogic_BadTimeSpec_String_Id,
    RoutingLogic_Error_String_Id,
    RoutingLogic_NotCaptain_String_Id,
    ServerListCtrl_Pinging_String_Id,
    ServerListCtrl_PingFailed_String_Id,
    ServerListCtrl_Querying_String_Id,
    ServerListCtrl_QueryFailed_String_Id,
    ServerListCtrl_ListSummary_String_Id,
    ServerListCtrl_ListPingSummary_String_Id,
    ServerListCtrl_RefreshList_String_Id,
    ServerListCtrl_StopRefresh_String_Id,
    ServerListCtrl_NoFilter_String_Id,
    ServerListCtrl_AddRemove_String_Id,
    ServerListCtrl_Add_String_Id,
    ServerListCtrl_Remove_String_Id,
    ServerListCtrl_FullFilter_String_Id,
    ServerListCtrl_NotFullFilter_String_Id,
    ServerListCtrl_OpenFilter_String_Id,
    ServerListCtrl_PingFailed_Image_Id,
    ServerListCtrl_GameExpanded_Image_Id,
    ServerListCtrl_GameNotExpanded_Image_Id,
    ServerListCtrl_GameAreInvited_Image_Id,
    ServerListCtrl_GameNone_Image_Id,
    ServerListCtrl_GameAskToJoin_Image_Id,
    ServerListCtrl_GameHasPassword_Image_Id,
    ServerListCtrl_GameNotInvited_Image_Id,
    ServerListCtrl_GameInProgress_Image_Id,
    ServerListCtrl_SkillOpen_Image_Id,
    ServerListCtrl_SkillNovice_Image_Id,
    ServerListCtrl_SkillIntermediate_Image_Id,
    ServerListCtrl_SkillAdvanced_Image_Id,
    ServerListCtrl_SkillExpert_Image_Id,
    ServerOptionsCtrl_Connect_String_Id,
    ServerOptionsCtrl_RefreshServer_String_Id,
    ServerOptionsCtrl_HideDetails_String_Id,
    ServerOptionsCtrl_ShowDetails_String_Id,
    ServerOptionsCtrl_ResfreshList_String_Id,
    StagingLogic_DisconnectedTitle_String_Id,
    StagingLogic_DisconnectedDetails_String_Id,
    StagingLogic_BannedTitle_String_Id,
    StagingLogic_BannedDetails_String_Id,
    StagingLogic_KickedTitle_String_Id,
    StagingLogic_KickedDetails_String_Id,
    UserListCtrl_Ignore_String_Id,
    UserListCtrl_Unignore_String_Id,
    UserListCtrl_Normal_Image_Id,
    UserListCtrl_Away_Image_Id,
    UserListCtrl_Admin_Image_Id,
    UserListCtrl_Moderator_Image_Id,
    UserListCtrl_Captain_Image_Id,
    UserListCtrl_Muted_Image_Id,
    UserListCtrl_Ignored_Image_Id,
    UserListCtrl_Blocked_Image_Id,
    WONStatus_Success_String_Id,
    WONStatus_Killed_String_Id,
    WONStatus_TimedOut_String_Id,
    WONStatus_InvalidAddress_String_Id,
    WONStatus_GeneralFailure_String_Id,
    WONStatus_AuthFailure_String_Id,
    WONStatus_FTPFailure_String_Id,
    WONStatus_HTTPFailure_String_Id,
    WONStatus_MOTDFailure_String_Id,
    WONStatus_RoutingFailure_String_Id,
    WONStatus_GameSpyFailure_String_Id,
    WONStatus_DirFailure_String_Id,
    WONStatus_ChatFailure_String_Id,
    WONStatus_DatabaseFailure_String_Id,
    WONStatus_NotAllowed_String_Id,
    WONStatus_NotAuthorized_String_Id,
    WONStatus_BehindFirewall_String_Id,
    WONStatus_ExpiredCertificate_String_Id,
    WONStatus_InvalidCDKey_String_Id,
    WONStatus_CDKeyBanned_String_Id,
    WONStatus_CDKeyInUse_String_Id,
    WONStatus_CRCFailed_String_Id,
    WONStatus_UserExists_String_Id,
    WONStatus_UserNotFound_String_Id,
    WONStatus_BadPassword_String_Id,
    WONStatus_InvalidUserName_String_Id,
    WONStatus_BadCommunity_String_Id,
    WONStatus_NotInCommunity_String_Id,
    WONStatus_UserSeqInUse_String_Id,
    WONStatus_BlankNamesNotAllowed_String_Id,
    WONStatus_CaptainRejectedYou_String_Id,
    WONStatus_ClientAlreadyBanned_String_Id,
    WONStatus_ClientAlreadyInGroup_String_Id,
    WONStatus_ClientAlreadyMuted_String_Id,
    WONStatus_ClientBanned_String_Id,
    WONStatus_ClientDoesNotExist_String_Id,
    WONStatus_ClientNotBanned_String_Id,
    WONStatus_ClientNotInGroup_String_Id,
    WONStatus_ClientNotInvited_String_Id,
    WONStatus_ClientNotMuted_String_Id,
    WONStatus_ConnectFailure_String_Id,
    WONStatus_GroupAlreadyClaimed_String_Id,
    WONStatus_GroupAlreadyExists_String_Id,
    WONStatus_GroupClosed_String_Id,
    WONStatus_GroupDeleted_String_Id,
    WONStatus_GroupDoesNotExist_String_Id,
    WONStatus_GroupFull_String_Id,
    WONStatus_GuestNamesReserved_String_Id,
    WONStatus_InvalidPassword_String_Id,
    WONStatus_MustBeAdmin_String_Id,
    WONStatus_MustBeCaptain_String_Id,
    WONStatus_MustBeCaptainOrModerator_String_Id,
    WONStatus_MustBeModerator_String_Id,
    WONStatus_ServerFull_String_Id,
    WONStatus_GroupNameExceedsMaximum_String_Id,
    WONStatus_JoinCanceled_String_Id,
    WONStatus_Throttled_String_Id,
    WONStatus_KeyNotUsed_String_Id,
    WONStatus_EMailPasswordDuplication_String_Id,
    WONStatus_AccountCreateDisabled_String_Id,
    WONStatus_AgeNotOldEnough_String_Id,
    WONStatus_ItemHasDependencies_String_Id,
    WONStatus_OutOfDate_String_Id,
    WONStatus_OutOfDateNoUpdate_String_Id,
    WONStatus_ValidNotLatest_String_Id,
    WONStatus_DataAlreadyExists_String_Id,
    WONStatus_DBError_String_Id,
    WONStatus_ConnRefused_String_Id,
};

WONAPI::ResourceConfigTablePtr LobbyResource_Init(WONAPI::ResourceConfigTable* = NULL);
const WONAPI::GUIString& LobbyResource_GetString(LobbyResourceId);
WONAPI::Image* LobbyResource_GetImage(LobbyResourceId);
WONAPI::Sound* LobbyResource_GetSound(LobbyResourceId);
WONAPI::Font* LobbyResource_GetFont(LobbyResourceId);
const WONAPI::Background& LobbyResource_GetBackground(LobbyResourceId);
DWORD LobbyResource_GetColor(LobbyResourceId);
int LobbyResource_GetInt(LobbyResourceId);
void LobbyResource_SafeGlueResources(WONAPI::ResourceConfigTable*);
void LobbyResource_UnloadResources();

#endif
