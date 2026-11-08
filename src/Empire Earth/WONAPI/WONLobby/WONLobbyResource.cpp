// The lobby's resources: global storage, registration, the table by id, load and unload.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceGlobals.h"
using namespace WONAPI;

GUIString LobbyGlobal_Cancel_String; // 0x566944
GUIString LobbyGlobal_Lan_String; // 0x5669e0
GUIString LobbyGlobal_No_String; // 0x566a58
GUIString LobbyGlobal_OK_String; // 0x566468
GUIString LobbyGlobal_Yes_String; // 0x566a30
GUIString LobbyGlobal_QuickPlay_String; // 0x566758
SmartPtr<Image> LobbyGlobal_Searching_Image; // 0x5668ac
SmartPtr<Image> LobbyGlobal_Firewall_Image; // 0x566414
SmartPtr<Sound> LobbyGlobal_ButtonClick_Sound; // 0x566770
SmartPtr<Sound> LobbyGlobal_LobbyMusic_Sound; // 0x5664d0
SmartPtr<Sound> LobbyGlobal_AllReady_Sound; // 0x5667d0
SmartPtr<Sound> LobbyGlobal_AllNotReady_Sound; // 0x566438
SmartPtr<Sound> LobbyGlobal_AskToJoinRecv_Sound; // 0x566694
SmartPtr<Sound> LobbyGlobal_Error_Sound; // 0x566638
SmartPtr<Sound> LobbyGlobal_FriendJoined_Sound; // 0x56646c
SmartPtr<Sound> LobbyGlobal_FriendLeft_Sound; // 0x566a9c
SmartPtr<Sound> LobbyGlobal_InviteRecv_Sound; // 0x566714
SmartPtr<Sound> LobbyGlobal_PlayerJoined_Sound; // 0x566500
SmartPtr<Sound> LobbyGlobal_SierraLogon_Sound; // 0x566a2c
SmartPtr<Sound> LobbyGlobal_WhisperRecv_Sound; // 0x5665dc
GUIString AccountLogic_InvalidCDKeyTitle_String; // 0x5666f4
GUIString AccountLogic_InvalidCDKeyDetails_String; // 0x56682c
GUIString AddFriendDialog_StatusTitle_String; // 0x566598
GUIString AddFriendDialog_StatusDetails_String; // 0x566740
GUIString AdminActionDialog_Warn_String; // 0x56640c
GUIString AdminActionDialog_Mute_String; // 0x5664a0
GUIString AdminActionDialog_UnMute_String; // 0x566a04
GUIString AdminActionDialog_Kick_String; // 0x566604
GUIString AdminActionDialog_Ban_String; // 0x566444
GUIString AdminActionDialog_ExplanationOpt_String; // 0x566818
GUIString AdminActionDialog_ExplanationReq_String; // 0x566548
GUIString BadUserDialog_OneDay_String; // 0x5664cc
GUIString BadUserDialog_OneHour_String; // 0x5665b4
GUIString BadUserDialog_OneMinute_String; // 0x56675c
GUIString BadUserDialog_OneSecond_String; // 0x5668dc
GUIString BadUserDialog_XDays_String; // 0x566418
GUIString BadUserDialog_XHours_String; // 0x566760
GUIString BadUserDialog_XMinutes_String; // 0x5667b4
GUIString BadUserDialog_XSeconds_String; // 0x566530
GUIString BadUserDialog_Infinite_String; // 0x566828
GUIString ChatCommandLogic_Whisper_String; // 0x5664bc
GUIString ChatCommandLogic_Block_String; // 0x5666a0
GUIString ChatCommandLogic_Emote_String; // 0x566a18
GUIString ChatCommandLogic_Ignore_String; // 0x56641c
GUIString ChatCommandLogic_Away_String; // 0x5668c8
GUIString ChatCommandLogic_Invite_String; // 0x5668c4
GUIString ChatCommandLogic_Uninvite_String; // 0x566708
GUIString ChatCommandLogic_Reply_String; // 0x5665ac
GUIString ChatCommandLogic_Clear_String; // 0x566a74
GUIString ChatCommandLogic_Help_String; // 0x5666cc
GUIString ChatCommandLogic_ShowTeam_String; // 0x5667a0
GUIString ChatCommandLogic_Moderator_String; // 0x56642c
GUIString ChatCommandLogic_ServerMute_String; // 0x566744
GUIString ChatCommandLogic_ServerBan_String; // 0x56676c
GUIString ChatCommandLogic_Mute_String; // 0x566648
GUIString ChatCommandLogic_Ban_String; // 0x56689c
GUIString ChatCommandLogic_Unmute_String; // 0x56679c
GUIString ChatCommandLogic_ServerUnmute_String; // 0x5666e4
GUIString ChatCommandLogic_Unban_String; // 0x566794
GUIString ChatCommandLogic_ServerUnban_String; // 0x5663f4
GUIString ChatCommandLogic_Alert_String; // 0x5665d0
GUIString ChatCommandLogic_Warn_String; // 0x566a48
GUIString ChatCommandLogic_HelpHeader_String; // 0x5668d8
GUIString ChatCommandLogic_HelpHelp_String; // 0x566498
GUIString ChatCommandLogic_ShowTeamHelp_String; // 0x5668b0
GUIString ChatCommandLogic_WhisperHelp_String; // 0x56688c
GUIString ChatCommandLogic_BlockHelp_String; // 0x566580
GUIString ChatCommandLogic_IgnoreHelp_String; // 0x5663c4
GUIString ChatCommandLogic_ReplyHelp_String; // 0x566620
GUIString ChatCommandLogic_EmoteHelp_String; // 0x5667c0
GUIString ChatCommandLogic_AwayHelp_String; // 0x5665f8
GUIString ChatCommandLogic_ClearHelp_String; // 0x566874
GUIString ChatCommandLogic_InviteHelp_String; // 0x566a98
GUIString ChatCommandLogic_UninviteHelp_String; // 0x566820
GUIString ChatCommandLogic_MuteHelp_String; // 0x566674
GUIString ChatCommandLogic_UnmuteHelp_String; // 0x566658
GUIString ChatCommandLogic_BanHelp_String; // 0x566984
GUIString ChatCommandLogic_UnbanHelp_String; // 0x566920
GUIString ChatCommandLogic_ModeratorHelp_String; // 0x566958
GUIString ChatCommandLogic_ServerMuteHelp_String; // 0x5669f4
GUIString ChatCommandLogic_ServerUnmuteHelp_String; // 0x5665cc
GUIString ChatCommandLogic_ServerBanHelp_String; // 0x566ab0
GUIString ChatCommandLogic_ServerUnbanHelp_String; // 0x56686c
GUIString ChatCommandLogic_AlertHelp_String; // 0x566860
GUIString ChatCommandLogic_WarnHelp_String; // 0x56652c
GUIString ChatCtrl_NormalChat_String; // 0x5667e4
GUIString ChatCtrl_Emote_String; // 0x5668b8
GUIString ChatCtrl_TeamChat_String; // 0x56692c
GUIString ChatCtrl_TeamEmote_String; // 0x5666e0
GUIString ChatCtrl_YouWhisperToYourself_String; // 0x566718
GUIString ChatCtrl_YouWhisper_String; // 0x566900
GUIString ChatCtrl_SomeoneWhispersToYou_String; // 0x566398
GUIString ChatCtrl_YouWarnSomeone_String; // 0x5666d8
GUIString ChatCtrl_YouWarnYourself_String; // 0x566a00
GUIString ChatCtrl_SomeoneWarnsYou_String; // 0x5669b0
GUIString ChatCtrl_SomeoneWarnsSomeone_String; // 0x566a70
GUIString ChatCtrl_EnteringRoom_String; // 0x566410
GUIString ChatCtrl_LeavingRoom_String; // 0x5669c8
GUIString ChatCtrl_Disconnected_String; // 0x566664
GUIString ChatCtrl_YouWereKicked_String; // 0x566908
GUIString ChatCtrl_YouWereBannedMinute_String; // 0x56694c
GUIString ChatCtrl_YouWereBannedMinutes_String; // 0x566634
GUIString ChatCtrl_YouWereBannedHour_String; // 0x566428
GUIString ChatCtrl_YouWereBannedHours_String; // 0x566a0c
GUIString ChatCtrl_YouWereBannedDay_String; // 0x566600
GUIString ChatCtrl_YouWereBannedDays_String; // 0x566ad0
GUIString ChatCtrl_YouWereBannedIndefinitely_String; // 0x56697c
GUIString ChatCtrl_SomeoneWasKicked_String; // 0x5664c8
GUIString ChatCtrl_SomeoneWasBannedMinute_String; // 0x5668a4
GUIString ChatCtrl_SomeoneWasBannedMinutes_String; // 0x5663bc
GUIString ChatCtrl_SomeoneWasBannedHour_String; // 0x5667d8
GUIString ChatCtrl_SomeoneWasBannedHours_String; // 0x566684
GUIString ChatCtrl_SomeoneWasBannedDay_String; // 0x566ac4
GUIString ChatCtrl_SomeoneWasBannedDays_String; // 0x56660c
GUIString ChatCtrl_SomeoneWasBannedIndefinitely_String; // 0x5664b8
GUIString ChatCtrl_YouWereUnmuted_String; // 0x5663d4
GUIString ChatCtrl_YouWereMutedMinute_String; // 0x5664e4
GUIString ChatCtrl_YouWereMutedMinutes_String; // 0x5666ac
GUIString ChatCtrl_YouWereMutedHour_String; // 0x5663cc
GUIString ChatCtrl_YouWereMutedHours_String; // 0x566ae4
GUIString ChatCtrl_YouWereMutedDay_String; // 0x566a34
GUIString ChatCtrl_YouWereMutedDays_String; // 0x5665e4
GUIString ChatCtrl_YouWereMutedIndefinitely_String; // 0x56649c
GUIString ChatCtrl_SomeoneWasUnmuted_String; // 0x566960
GUIString ChatCtrl_SomeoneWasMutedMinute_String; // 0x566424
GUIString ChatCtrl_SomeoneWasMutedMinutes_String; // 0x5667a8
GUIString ChatCtrl_SomeoneWasMutedHour_String; // 0x5663e8
GUIString ChatCtrl_SomeoneWasMutedHours_String; // 0x566588
GUIString ChatCtrl_SomeoneWasMutedDay_String; // 0x566690
GUIString ChatCtrl_SomeoneWasMutedDays_String; // 0x5668e4
GUIString ChatCtrl_SomeoneWasMutedIndefinitely_String; // 0x566440
GUIString ChatCtrl_ModeratorComment_String; // 0x566494
GUIString ChatCtrl_AlertComment_String; // 0x566898
GUIString ChatCtrl_ServerAlert_String; // 0x5664b4
GUIString ChatCtrl_ServerShutdownStartedMinute_String; // 0x56669c
GUIString ChatCtrl_ServerShutdownStartedMinutes_String; // 0x5668d4
GUIString ChatCtrl_ServerShutdownStartedHour_String; // 0x5665a4
GUIString ChatCtrl_ServerShutdownStartedHours_String; // 0x566914
GUIString ChatCtrl_ServerShutdownStartedDay_String; // 0x5663d0
GUIString ChatCtrl_ServerShutdownStartedDays_String; // 0x5668e0
GUIString ChatCtrl_ServerShutdownAborted_String; // 0x566608
GUIString ChatCtrl_SomeoneInvitesYou_String; // 0x566940
GUIString ChatCtrl_SomeoneUninvitesYou_String; // 0x566934
GUIString ChatCtrl_YouInviteSomeone_String; // 0x566644
GUIString ChatCtrl_YouUninviteSomeone_String; // 0x566478
GUIString ChatCtrl_SomeoneInvitesYouExpl_String; // 0x566980
GUIString ChatCtrl_SomeoneUninvitesYouExpl_String; // 0x566974
GUIString ChatCtrl_YouInviteSomeoneExpl_String; // 0x5663e4
GUIString ChatCtrl_YouUninviteSomeoneExpl_String; // 0x566878
GUIString ChatCtrl_Accept_String; // 0x566ad4
GUIString ChatCtrl_Reject_String; // 0x566574
GUIString ChatCtrl_SomeoneAsksToJoin_String; // 0x5669bc
GUIString ChatCtrl_SomeoneAsksToJoinExpl_String; // 0x566734
GUIString ChatCtrl_ClientNotOnServer_String; // 0x5669a0
GUIString ChatCtrl_ClientInGame_String; // 0x5669f8
GUIString ChatCtrl_ClientInChat_String; // 0x5669dc
GUIString ChatCtrl_ClientInGameAndChat_String; // 0x56693c
GUIString ChatCtrl_ClientIsAnonymous_String; // 0x5664ec
GUIString ChatCtrl_FriendIsChatting_String; // 0x56674c
GUIString ChatCtrl_FriendIsInGameStaging_String; // 0x566768
GUIString ChatCtrl_FriendIsPlaying_String; // 0x566acc
GUIString ChatCtrl_FriendIsOn_String; // 0x566460
GUIString ChatCtrl_FriendIsNotOnline_String; // 0x566a28
GUIString ChatCtrl_PlayerKicked_String; // 0x566450
GUIString ChatCtrl_PlayerBanned_String; // 0x566780
GUIString ChatCtrl_FriendJoined_String; // 0x56661c
GUIString ChatCtrl_FriendLeft_String; // 0x566ad8
GUIString ChatCtrl_TeamMemberJoined_String; // 0x566954
GUIString ChatCtrl_TeamMemberLeft_String; // 0x5666a8
GUIString ChatCtrl_TeamMembers_String; // 0x566528
GUIString ChatCtrl_IgnoreList_String; // 0x566a64
GUIString ChooseNetworkAdapterDialog_Default_String; // 0x56681c
GUIString ChooseNetworkAdapterDialog_Unknown_String; // 0x566594
GUIString ClientOptionsCtrl_Whisper_String; // 0x566968
GUIString ClientOptionsCtrl_Invite_String; // 0x566710
GUIString ClientOptionsCtrl_Uninvite_String; // 0x566814
GUIString ClientOptionsCtrl_AddToFriends_String; // 0x5667d4
GUIString ClientOptionsCtrl_RemoveFromFriends_String; // 0x5665f0
GUIString ClientOptionsCtrl_ClearAFK_String; // 0x566964
GUIString ClientOptionsCtrl_SetAFK_String; // 0x5668f4
GUIString ClientOptionsCtrl_Ignore_String; // 0x5668c0
GUIString ClientOptionsCtrl_Unignore_String; // 0x56656c
GUIString ClientOptionsCtrl_Block_String; // 0x5667bc
GUIString ClientOptionsCtrl_Unblock_String; // 0x566400
GUIString ClientOptionsCtrl_Warn_String; // 0x56653c
GUIString ClientOptionsCtrl_Mute_String; // 0x566998
GUIString ClientOptionsCtrl_UnMute_String; // 0x566890
GUIString ClientOptionsCtrl_Kick_String; // 0x5664a8
GUIString ClientOptionsCtrl_Ban_String; // 0x5667fc
GUIString DirectConnectionOptionsCtrl_Connect_String; // 0x5666a4
GUIString DirectConnectionOptionsCtrl_Refresh_String; // 0x5667c4
GUIString DirectConnectionOptionsCtrl_Remove_String; // 0x566614
GUIString FriendOptionsCtrl_Locate_String; // 0x5664e8
GUIString FriendOptionsCtrl_RefreshList_String; // 0x56683c
GUIString FriendOptionsCtrl_RemoveFromFriends_String; // 0x566a5c
SmartPtr<Image> FriendsListCtrl_Unknown_Image; // 0x566aa4
SmartPtr<Image> FriendsListCtrl_NotFound_Image; // 0x56677c
SmartPtr<Image> FriendsListCtrl_InChat_Image; // 0x5663ec
SmartPtr<Image> FriendsListCtrl_InGame_Image; // 0x566858
GUIString GameOptionsPopup_Join_String; // 0x56647c
GUIString GameOptionsPopup_Ping_String; // 0x566988
GUIString GameOptionsPopup_QueryDetails_String; // 0x5665c8
GUIString GameOptionsPopup_HideDetails_String; // 0x566790
GUIString GameOptionsPopup_ShowDetails_String; // 0x566a90
GUIString GameOptionsPopup_Remove_String; // 0x5668fc
GUIString GameStagingCtrl_Ready_String; // 0x566748
GUIString GameStagingCtrl_NotReady_String; // 0x56650c
GUIString GameStagingCtrl_StartGame_String; // 0x566558
GUIString GameStagingCtrl_LeaveGame_String; // 0x566a08
GUIString GameStagingCtrl_DissolveGame_String; // 0x566730
GUIString GameStagingLogic_KickedTitle_String; // 0x5669fc
GUIString GameStagingLogic_KickedDetails_String; // 0x5669a4
GUIString GameStagingLogic_BannedTitle_String; // 0x566704
GUIString GameStagingLogic_BannedDetails_String; // 0x566970
GUIString GameStagingLogic_ConnectWarningTitle1_String; // 0x5665a8
GUIString GameStagingLogic_ConnectWarningDetails1_String; // 0x566458
GUIString GameStagingLogic_ConnectWarningTitle2_String; // 0x566448
GUIString GameStagingLogic_ConnectWarningDetails2_String; // 0x5667f0
GUIString GameStagingLogic_ConnectWarningTitle3_String; // 0x5669b4
GUIString GameStagingLogic_ConnectWarningDetails3_String; // 0x5666c8
GUIString GameStagingLogic_NoPingReplyError_String; // 0x56667c
GUIString GameStagingLogic_GettingGameInfoTitle_String; // 0x566654
GUIString GameStagingLogic_GettingGameInfoDetails_String; // 0x566480
GUIString GameStagingLogic_JoinGameErrorTitle_String; // 0x566938
GUIString GameStagingLogic_VerifyingPingTitle_String; // 0x566a14
GUIString GameStagingLogic_VerifyingPingDetails_String; // 0x566ac8
GUIString GameStagingLogic_DissolveGame_String; // 0x566800
GUIString GameStagingLogic_DissolveGameConfirm_String; // 0x566700
GUIString GameStagingLogic_LeaveGame_String; // 0x56662c
GUIString GameStagingLogic_LeaveGameConfirm_String; // 0x566568
GUIString GameStagingLogic_NoQuickPlayGameTitle_String; // 0x56654c
GUIString GameStagingLogic_NoQuickPlayGameDetails_String; // 0x56684c
GUIString GameUserListCtrl_Ban_String; // 0x56691c
GUIString GameUserListCtrl_Kick_String; // 0x566430
SmartPtr<Image> GameUserListCtrl_Ready_Image; // 0x5665c0
GUIString InternetQuickPlay_Welcome_String; // 0x566ab8
GUIString InternetQuickPlay_UserWelcome_String; // 0x566544
GUIString JoinResponse_Joined_String; // 0x5669d0
GUIString JoinResponse_ServerFull_String; // 0x5665b8
GUIString JoinResponse_InviteOnly_String; // 0x566464
GUIString JoinResponse_AskToJoin_String; // 0x5667b8
GUIString JoinResponse_GameInProgress_String; // 0x56668c
GUIString JoinResponse_InvalidPassword_String; // 0x566a50
GUIString JoinResponse_NotOnLAN_String; // 0x5663f8
GUIString JoinResponse_Blocked_String; // 0x5666f8
GUIString JoinResponse_Unknown_String; // 0x566848
GUIString LANLogic_SocketBindError_String; // 0x5663d8
GUIString LANLogic_SocketListenError_String; // 0x566808
GUIString LANLogic_ConnectToGameTitle_String; // 0x566904
GUIString LANLogic_ConnectToGameDetails_String; // 0x5666dc
GUIString LANLogic_CheckingAddressTitle_String; // 0x56690c
GUIString LANLogic_CheckingAddressDetails_String; // 0x5668a0
GUIString LANLogic_RegisterTitle_String; // 0x56699c
GUIString LANLogic_RegisterDetails_String; // 0x5663fc
GUIString LANLogic_JoinFailure_String; // 0x566a78
GUIString LANLogic_JoinReplyError_String; // 0x566a44
GUIString LANLogic_InvalidSubnet_String; // 0x5666c4
GUIString LobbyContainer_InitStart_String; // 0x566888
GUIString LobbyContainer_InitFailed_String; // 0x5667b0
GUIString LobbyContainer_CheckingVersion_String; // 0x5668d0
GUIString LobbyContainer_QueryingServers_String; // 0x5669ac
GUIString LobbyContainer_NoConnection_String; // 0x5664a4
GUIString LobbyContainer_DNSLookupFailed_String; // 0x56663c
GUIString LobbyContainer_DirLookupFailure_String; // 0x5665fc
GUIString LobbyContainer_VersionCheckFailure_String; // 0x56639c
GUIString LobbyContainer_InitComplete_String; // 0x5669c0
GUIString LobbyContainer_NoServers_String; // 0x566aac
GUIString LobbyContainer_AuthServers_String; // 0x566788
GUIString LobbyContainer_AccountServers_String; // 0x566660
GUIString LobbyContainer_VersionServers_String; // 0x566724
GUIString LobbyContainer_CreateAccountTitle_String; // 0x566924
GUIString LobbyContainer_CreateAccountDetails_String; // 0x5669a8
GUIString LobbyContainer_RetrievePasswordTitle_String; // 0x566720
GUIString LobbyContainer_RetrievePasswordDetails_String; // 0x5663a0
GUIString LobbyContainer_RetrieveUsernameTitle_String; // 0x566804
GUIString LobbyContainer_RetrieveUsernameDetails_String; // 0x566824
GUIString LobbyContainer_QueryAccountTitle_String; // 0x566510
GUIString LobbyContainer_QueryAccountDetails_String; // 0x56664c
GUIString LobbyContainer_UpdateAccountTitle_String; // 0x5665ec
GUIString LobbyContainer_UpdateAccountDetails_String; // 0x566ae0
GUIString LobbyContainer_LoginTitle_String; // 0x5667f4
GUIString LobbyContainer_LoginDetails_String; // 0x566a10
GUIString LobbyContainer_GettingRoomsTitle_String; // 0x566610
GUIString LobbyContainer_GettingRoomsDetails_String; // 0x56696c
GUIString LobbyContainer_VersionOutOfDataTitle_String; // 0x5668f0
GUIString LobbyContainer_VersionOutOfDataDetails_String; // 0x5668ec
GUIString LobbyGameStatus_Success_String; // 0x566550
GUIString LobbyGameStatus_GameFull_String; // 0x566578
GUIString LobbyGameStatus_GameInProgress_String; // 0x566810
GUIString LobbyGameStatus_UnpackFailure_String; // 0x566870
GUIString LobbyGameStatus_NotInvited_String; // 0x5665d8
GUIString LobbyGameStatus_CaptainRejectedYou_String; // 0x566640
GUIString LobbyGameStatus_InvalidPassword_String; // 0x56659c
GUIString LobbyGameStatus_DuplicateName_String; // 0x56648c
GUIString LobbyOptions_DefaultName_String; // 0x566488
GUIString LobbyOptions_CheckingAddress_String; // 0x5666f0
GUIString LobbyOptions_AddressUnavailable_String; // 0x566628
GUIString RoomCtrl_QueryingServer_String; // 0x566838
SmartPtr<Image> RoomCtrl_Password_Image; // 0x5664f4
GUIString RoomLabelCtrl_LabelPrefix_String; // 0x5663b8
GUIString RoomLabelCtrl_LabelServer_String; // 0x5664b0
GUIString RoomLabelCtrl_LabelUser_String; // 0x566688
GUIString RoomLabelCtrl_LabelUsers_String; // 0x5669ec
GUIString RoomLabelCtrl_LabelDisconnected_String; // 0x566764
GUIString RoutingLogic_GetClientListTitle_String; // 0x5669e4
GUIString RoutingLogic_GetClientListDetails_String; // 0x566a3c
GUIString RoutingLogic_GetGroupListTitle_String; // 0x566ab4
GUIString RoutingLogic_GetGroupListDetails_String; // 0x566a1c
GUIString RoutingLogic_GameDissolvedTitle_String; // 0x566470
GUIString RoutingLogic_GameDissolvedDetails_String; // 0x566994
GUIString RoutingLogic_NoServersTitle_String; // 0x566534
GUIString RoutingLogic_NoServersDetails_String; // 0x5664e0
GUIString RoutingLogic_TooManyFriendsTitle_String; // 0x5666b8
GUIString RoutingLogic_TooManyFriendsDetails_String; // 0x566678
GUIString RoutingLogic_AlreadyJoinedTitle_String; // 0x5667ac
GUIString RoutingLogic_AlreadyInGame_String; // 0x566504
GUIString RoutingLogic_AlreadyInRoom_String; // 0x56695c
GUIString RoutingLogic_JoinGroupTitle_String; // 0x5667dc
GUIString RoutingLogic_JoinGroupDetails_String; // 0x5663c0
GUIString RoutingLogic_ConnectTitle_String; // 0x566490
GUIString RoutingLogic_ConnectDetails_String; // 0x5664fc
GUIString RoutingLogic_RegisterTitle_String; // 0x5669d8
GUIString RoutingLogic_RegisterDetails_String; // 0x566618
GUIString RoutingLogic_QueryGroupsTitle_String; // 0x566854
GUIString RoutingLogic_QueryGroupsDetails_String; // 0x566650
GUIString RoutingLogic_CreateGroupTitle_String; // 0x56651c
GUIString RoutingLogic_CreateGroupDetails_String; // 0x5667a4
GUIString RoutingLogic_GetBadUsersTitle_String; // 0x5664d8
GUIString RoutingLogic_GetBadUsersDetails_String; // 0x566abc
GUIString RoutingLogic_DisconnectedTitle_String; // 0x5666d0
GUIString RoutingLogic_DisconnectedDetails_String; // 0x566a6c
GUIString RoutingLogic_AskingCaptainTitle_String; // 0x5665a0
GUIString RoutingLogic_AskingCaptainDetails_String; // 0x566a84
GUIString RoutingLogic_CollectingPingsTitle_String; // 0x566884
GUIString RoutingLogic_CollectingPingsDetails_String; // 0x566778
GUIString RoutingLogic_GettingGameListTitle_String; // 0x56665c
GUIString RoutingLogic_GettingGameListDetails_String; // 0x56673c
GUIString RoutingLogic_MuteClientTitle_String; // 0x5667e8
GUIString RoutingLogic_UnmuteClientTitle_String; // 0x566844
GUIString RoutingLogic_BanClientTitle_String; // 0x566880
GUIString RoutingLogic_KickClientTitle_String; // 0x5663c8
GUIString RoutingLogic_ModerateDetails_String; // 0x566408
GUIString RoutingLogic_FriendNotFoundTitle_String; // 0x5666c0
GUIString RoutingLogic_FriendNotFoundDetails_String; // 0x566738
GUIString RoutingLogic_ClientNotFound_String; // 0x566784
GUIString RoutingLogic_ClientAmbiguous_String; // 0x566518
GUIString RoutingLogic_InvalidCommand_String; // 0x5669b8
GUIString RoutingLogic_RequireName_String; // 0x566590
GUIString RoutingLogic_BadTimeSpec_String; // 0x566a24
GUIString RoutingLogic_Error_String; // 0x56678c
GUIString RoutingLogic_NotCaptain_String; // 0x566750
GUIString ServerListCtrl_Pinging_String; // 0x56657c
GUIString ServerListCtrl_PingFailed_String; // 0x56687c
GUIString ServerListCtrl_Querying_String; // 0x566918
GUIString ServerListCtrl_QueryFailed_String; // 0x566404
GUIString ServerListCtrl_ListSummary_String; // 0x5665c4
GUIString ServerListCtrl_ListPingSummary_String; // 0x566950
GUIString ServerListCtrl_RefreshList_String; // 0x566868
GUIString ServerListCtrl_StopRefresh_String; // 0x5664dc
GUIString ServerListCtrl_NoFilter_String; // 0x56671c
GUIString ServerListCtrl_AddRemove_String; // 0x566540
GUIString ServerListCtrl_Add_String; // 0x566474
GUIString ServerListCtrl_Remove_String; // 0x566670
GUIString ServerListCtrl_FullFilter_String; // 0x566a38
GUIString ServerListCtrl_NotFullFilter_String; // 0x566894
GUIString ServerListCtrl_OpenFilter_String; // 0x5669cc
SmartPtr<Image> ServerListCtrl_PingFailed_Image; // 0x5666bc
SmartPtr<Image> ServerListCtrl_GameExpanded_Image; // 0x5664f8
SmartPtr<Image> ServerListCtrl_GameNotExpanded_Image; // 0x566930
SmartPtr<Image> ServerListCtrl_GameAreInvited_Image; // 0x5669e8
SmartPtr<Image> ServerListCtrl_GameNone_Image; // 0x566a80
SmartPtr<Image> ServerListCtrl_GameAskToJoin_Image; // 0x56645c
SmartPtr<Image> ServerListCtrl_GameHasPassword_Image; // 0x5663e0
SmartPtr<Image> ServerListCtrl_GameNotInvited_Image; // 0x5666fc
SmartPtr<Image> ServerListCtrl_GameInProgress_Image; // 0x566830
SmartPtr<Image> ServerListCtrl_SkillOpen_Image; // 0x566a7c
SmartPtr<Image> ServerListCtrl_SkillNovice_Image; // 0x5663dc
SmartPtr<Image> ServerListCtrl_SkillIntermediate_Image; // 0x56658c
SmartPtr<Image> ServerListCtrl_SkillAdvanced_Image; // 0x5667cc
SmartPtr<Image> ServerListCtrl_SkillExpert_Image; // 0x5668e8
GUIString ServerOptionsCtrl_Connect_String; // 0x566524
GUIString ServerOptionsCtrl_RefreshServer_String; // 0x566990
GUIString ServerOptionsCtrl_HideDetails_String; // 0x56685c
GUIString ServerOptionsCtrl_ShowDetails_String; // 0x5663ac
GUIString ServerOptionsCtrl_ResfreshList_String; // 0x5666b4
GUIString StagingLogic_DisconnectedTitle_String; // 0x5663b4
GUIString StagingLogic_DisconnectedDetails_String; // 0x5669d4
GUIString StagingLogic_BannedTitle_String; // 0x566434
GUIString StagingLogic_BannedDetails_String; // 0x566850
GUIString StagingLogic_KickedTitle_String; // 0x5667c8
GUIString StagingLogic_KickedDetails_String; // 0x566624
GUIString UserListCtrl_Ignore_String; // 0x56644c
GUIString UserListCtrl_Unignore_String; // 0x566a40
SmartPtr<Image> UserListCtrl_Normal_Image; // 0x566698
SmartPtr<Image> UserListCtrl_Away_Image; // 0x566560
SmartPtr<Image> UserListCtrl_Admin_Image; // 0x566aa8
SmartPtr<Image> UserListCtrl_Moderator_Image; // 0x5665f4
SmartPtr<Image> UserListCtrl_Captain_Image; // 0x566aa0
SmartPtr<Image> UserListCtrl_Muted_Image; // 0x5665e8
SmartPtr<Image> UserListCtrl_Ignored_Image; // 0x566a20
SmartPtr<Image> UserListCtrl_Blocked_Image; // 0x5668cc
GUIString WONStatus_Success_String; // 0x566584
GUIString WONStatus_Killed_String; // 0x566a68
GUIString WONStatus_TimedOut_String; // 0x566630
GUIString WONStatus_InvalidAddress_String; // 0x566538
GUIString WONStatus_GeneralFailure_String; // 0x5663b0
GUIString WONStatus_AuthFailure_String; // 0x566a4c
GUIString WONStatus_FTPFailure_String; // 0x56666c
GUIString WONStatus_HTTPFailure_String; // 0x5665b0
GUIString WONStatus_MOTDFailure_String; // 0x566a88
GUIString WONStatus_RoutingFailure_String; // 0x5668b4
GUIString WONStatus_GameSpyFailure_String; // 0x566554
GUIString WONStatus_DirFailure_String; // 0x566754
GUIString WONStatus_ChatFailure_String; // 0x5669c4
GUIString WONStatus_DatabaseFailure_String; // 0x5668a8
GUIString WONStatus_NotAllowed_String; // 0x5666d4
GUIString WONStatus_NotAuthorized_String; // 0x566484
GUIString WONStatus_BehindFirewall_String; // 0x566ac0
GUIString WONStatus_ExpiredCertificate_String; // 0x5667f8
GUIString WONStatus_InvalidCDKey_String; // 0x5663a4
GUIString WONStatus_CDKeyBanned_String; // 0x566520
GUIString WONStatus_CDKeyInUse_String; // 0x566514
GUIString WONStatus_CRCFailed_String; // 0x56670c
GUIString WONStatus_UserExists_String; // 0x566680
GUIString WONStatus_UserNotFound_String; // 0x566978
GUIString WONStatus_BadPassword_String; // 0x5667e0
GUIString WONStatus_InvalidUserName_String; // 0x5664ac
GUIString WONStatus_BadCommunity_String; // 0x566a60
GUIString WONStatus_NotInCommunity_String; // 0x566454
GUIString WONStatus_UserSeqInUse_String; // 0x56698c
GUIString WONStatus_BlankNamesNotAllowed_String; // 0x566a94
GUIString WONStatus_CaptainRejectedYou_String; // 0x566a8c
GUIString WONStatus_ClientAlreadyBanned_String; // 0x5663a8
GUIString WONStatus_ClientAlreadyInGroup_String; // 0x5665bc
GUIString WONStatus_ClientAlreadyMuted_String; // 0x566420
GUIString WONStatus_ClientBanned_String; // 0x566840
GUIString WONStatus_ClientDoesNotExist_String; // 0x5663f0
GUIString WONStatus_ClientNotBanned_String; // 0x5665e0
GUIString WONStatus_ClientNotInGroup_String; // 0x566834
GUIString WONStatus_ClientNotInvited_String; // 0x566508
GUIString WONStatus_ClientNotMuted_String; // 0x566864
GUIString WONStatus_ConnectFailure_String; // 0x5666ec
GUIString WONStatus_GroupAlreadyClaimed_String; // 0x5664c4
GUIString WONStatus_GroupAlreadyExists_String; // 0x5667ec
GUIString WONStatus_GroupClosed_String; // 0x5665d4
GUIString WONStatus_GroupDeleted_String; // 0x566910
GUIString WONStatus_GroupDoesNotExist_String; // 0x56680c
GUIString WONStatus_GroupFull_String; // 0x566798
GUIString WONStatus_GuestNamesReserved_String; // 0x566570
GUIString WONStatus_InvalidPassword_String; // 0x5669f0
GUIString WONStatus_MustBeAdmin_String; // 0x5668bc
GUIString WONStatus_MustBeCaptain_String; // 0x566ae8
GUIString WONStatus_MustBeCaptainOrModerator_String; // 0x56643c
GUIString WONStatus_MustBeModerator_String; // 0x566668
GUIString WONStatus_ServerFull_String; // 0x566a54
GUIString WONStatus_GroupNameExceedsMaximum_String; // 0x5664c0
GUIString WONStatus_JoinCanceled_String; // 0x5664f0
GUIString WONStatus_Throttled_String; // 0x566adc
GUIString WONStatus_KeyNotUsed_String; // 0x5666e8
GUIString WONStatus_EMailPasswordDuplication_String; // 0x56672c
GUIString WONStatus_AccountCreateDisabled_String; // 0x5666b0
GUIString WONStatus_AgeNotOldEnough_String; // 0x56655c
GUIString WONStatus_ItemHasDependencies_String; // 0x566948
GUIString WONStatus_OutOfDate_String; // 0x566928
GUIString WONStatus_OutOfDateNoUpdate_String; // 0x566774
GUIString WONStatus_ValidNotLatest_String; // 0x566728
GUIString WONStatus_DataAlreadyExists_String; // 0x566564
GUIString WONStatus_DBError_String; // 0x5664d4
GUIString WONStatus_ConnRefused_String; // 0x5668f8

// SYNTHETIC: WON 0x770130
// SYNTHETIC: WON 0x770140
// SYNTHETIC: WON 0x770150
// SYNTHETIC: WON 0x770160
// SYNTHETIC: WON 0x770170
// SYNTHETIC: WON 0x770180
// SYNTHETIC: WON 0x770190
// SYNTHETIC: WON 0x7701a0
// SYNTHETIC: WON 0x7701b0
// SYNTHETIC: WON 0x7701c0
// SYNTHETIC: WON 0x7701d0
// SYNTHETIC: WON 0x7701e0
// SYNTHETIC: WON 0x7701f0
// SYNTHETIC: WON 0x770200
// SYNTHETIC: WON 0x770210
// SYNTHETIC: WON 0x770220
// SYNTHETIC: WON 0x770230
// SYNTHETIC: WON 0x770240
// SYNTHETIC: WON 0x770250
// SYNTHETIC: WON 0x770260
// SYNTHETIC: WON 0x770270
// SYNTHETIC: WON 0x770280
// SYNTHETIC: WON 0x770290
// SYNTHETIC: WON 0x7702a0
// SYNTHETIC: WON 0x7702b0
// SYNTHETIC: WON 0x7702c0
// SYNTHETIC: WON 0x7702d0
// SYNTHETIC: WON 0x7702e0
// SYNTHETIC: WON 0x7702f0
// SYNTHETIC: WON 0x770300
// SYNTHETIC: WON 0x770310
// SYNTHETIC: WON 0x770320
// SYNTHETIC: WON 0x770330
// SYNTHETIC: WON 0x770340
// SYNTHETIC: WON 0x770350
// SYNTHETIC: WON 0x770360
// SYNTHETIC: WON 0x770370
// SYNTHETIC: WON 0x770380
// SYNTHETIC: WON 0x770390
// SYNTHETIC: WON 0x7703a0
// SYNTHETIC: WON 0x7703b0
// SYNTHETIC: WON 0x7703c0
// SYNTHETIC: WON 0x7703d0
// SYNTHETIC: WON 0x7703e0
// SYNTHETIC: WON 0x7703f0
// SYNTHETIC: WON 0x770400
// SYNTHETIC: WON 0x770410
// SYNTHETIC: WON 0x770420
// SYNTHETIC: WON 0x770430
// SYNTHETIC: WON 0x770440
// SYNTHETIC: WON 0x770450
// SYNTHETIC: WON 0x770460
// SYNTHETIC: WON 0x770470
// SYNTHETIC: WON 0x770480
// SYNTHETIC: WON 0x770490
// SYNTHETIC: WON 0x7704a0
// SYNTHETIC: WON 0x7704b0
// SYNTHETIC: WON 0x7704c0
// SYNTHETIC: WON 0x7704d0
// SYNTHETIC: WON 0x7704e0
// SYNTHETIC: WON 0x7704f0
// SYNTHETIC: WON 0x770500
// SYNTHETIC: WON 0x770510
// SYNTHETIC: WON 0x770520
// SYNTHETIC: WON 0x770530
// SYNTHETIC: WON 0x770540
// SYNTHETIC: WON 0x770550
// SYNTHETIC: WON 0x770560
// SYNTHETIC: WON 0x770570
// SYNTHETIC: WON 0x770580
// SYNTHETIC: WON 0x770590
// SYNTHETIC: WON 0x7705a0
// SYNTHETIC: WON 0x7705b0
// SYNTHETIC: WON 0x7705c0
// SYNTHETIC: WON 0x7705d0
// SYNTHETIC: WON 0x7705e0
// SYNTHETIC: WON 0x7705f0
// SYNTHETIC: WON 0x770600
// SYNTHETIC: WON 0x770610
// SYNTHETIC: WON 0x770620
// SYNTHETIC: WON 0x770630
// SYNTHETIC: WON 0x770640
// SYNTHETIC: WON 0x770650
// SYNTHETIC: WON 0x770660
// SYNTHETIC: WON 0x770670
// SYNTHETIC: WON 0x770680
// SYNTHETIC: WON 0x770690
// SYNTHETIC: WON 0x7706a0
// SYNTHETIC: WON 0x7706b0
// SYNTHETIC: WON 0x7706c0
// SYNTHETIC: WON 0x7706d0
// SYNTHETIC: WON 0x7706e0
// SYNTHETIC: WON 0x7706f0
// SYNTHETIC: WON 0x770700
// SYNTHETIC: WON 0x770710
// SYNTHETIC: WON 0x770720
// SYNTHETIC: WON 0x770730
// SYNTHETIC: WON 0x770740
// SYNTHETIC: WON 0x770750
// SYNTHETIC: WON 0x770760
// SYNTHETIC: WON 0x770770
// SYNTHETIC: WON 0x770780
// SYNTHETIC: WON 0x770790
// SYNTHETIC: WON 0x7707a0
// SYNTHETIC: WON 0x7707b0
// SYNTHETIC: WON 0x7707c0
// SYNTHETIC: WON 0x7707d0
// SYNTHETIC: WON 0x7707e0
// SYNTHETIC: WON 0x7707f0
// SYNTHETIC: WON 0x770800
// SYNTHETIC: WON 0x770810
// SYNTHETIC: WON 0x770820
// SYNTHETIC: WON 0x770830
// SYNTHETIC: WON 0x770840
// SYNTHETIC: WON 0x770850
// SYNTHETIC: WON 0x770860
// SYNTHETIC: WON 0x770870
// SYNTHETIC: WON 0x770880
// SYNTHETIC: WON 0x770890
// SYNTHETIC: WON 0x7708a0
// SYNTHETIC: WON 0x7708b0
// SYNTHETIC: WON 0x7708c0
// SYNTHETIC: WON 0x7708d0
// SYNTHETIC: WON 0x7708e0
// SYNTHETIC: WON 0x7708f0
// SYNTHETIC: WON 0x770900
// SYNTHETIC: WON 0x770910
// SYNTHETIC: WON 0x770920
// SYNTHETIC: WON 0x770930
// SYNTHETIC: WON 0x770940
// SYNTHETIC: WON 0x770950
// SYNTHETIC: WON 0x770960
// SYNTHETIC: WON 0x770970
// SYNTHETIC: WON 0x770980
// SYNTHETIC: WON 0x770990
// SYNTHETIC: WON 0x7709a0
// SYNTHETIC: WON 0x7709b0
// SYNTHETIC: WON 0x7709c0
// SYNTHETIC: WON 0x7709d0
// SYNTHETIC: WON 0x7709e0
// SYNTHETIC: WON 0x7709f0
// SYNTHETIC: WON 0x770a00
// SYNTHETIC: WON 0x770a10
// SYNTHETIC: WON 0x770a20
// SYNTHETIC: WON 0x770a30
// SYNTHETIC: WON 0x770a40
// SYNTHETIC: WON 0x770a50
// SYNTHETIC: WON 0x770a60
// SYNTHETIC: WON 0x770a70
// SYNTHETIC: WON 0x770a80
// SYNTHETIC: WON 0x770a90
// SYNTHETIC: WON 0x770aa0
// SYNTHETIC: WON 0x770ab0
// SYNTHETIC: WON 0x770ac0
// SYNTHETIC: WON 0x770ad0
// SYNTHETIC: WON 0x770ae0
// SYNTHETIC: WON 0x770af0
// SYNTHETIC: WON 0x770b00
// SYNTHETIC: WON 0x770b10
// SYNTHETIC: WON 0x770b20
// SYNTHETIC: WON 0x770b30
// SYNTHETIC: WON 0x770b40
// SYNTHETIC: WON 0x770b50
// SYNTHETIC: WON 0x770b60
// SYNTHETIC: WON 0x770b70
// SYNTHETIC: WON 0x770b80
// SYNTHETIC: WON 0x770b90
// SYNTHETIC: WON 0x770ba0
// SYNTHETIC: WON 0x770bb0
// SYNTHETIC: WON 0x770bc0
// SYNTHETIC: WON 0x770bd0
// SYNTHETIC: WON 0x770be0
// SYNTHETIC: WON 0x770bf0
// SYNTHETIC: WON 0x770c00
// SYNTHETIC: WON 0x770c10
// SYNTHETIC: WON 0x770c20
// SYNTHETIC: WON 0x770c30
// SYNTHETIC: WON 0x770c40
// SYNTHETIC: WON 0x770c50
// SYNTHETIC: WON 0x770c60
// SYNTHETIC: WON 0x770c70
// SYNTHETIC: WON 0x770c80
// SYNTHETIC: WON 0x770c90
// SYNTHETIC: WON 0x770ca0
// SYNTHETIC: WON 0x770cb0
// SYNTHETIC: WON 0x770cc0
// SYNTHETIC: WON 0x770cd0
// SYNTHETIC: WON 0x770ce0
// SYNTHETIC: WON 0x770cf0
// SYNTHETIC: WON 0x770d00
// SYNTHETIC: WON 0x770d10
// SYNTHETIC: WON 0x770d20
// SYNTHETIC: WON 0x770d30
// SYNTHETIC: WON 0x770d40
// SYNTHETIC: WON 0x770d50
// SYNTHETIC: WON 0x770d60
// SYNTHETIC: WON 0x770d70
// SYNTHETIC: WON 0x770d80
// SYNTHETIC: WON 0x770d90
// SYNTHETIC: WON 0x770da0
// SYNTHETIC: WON 0x770db0
// SYNTHETIC: WON 0x770dc0
// SYNTHETIC: WON 0x770dd0
// SYNTHETIC: WON 0x770de0
// SYNTHETIC: WON 0x770df0
// SYNTHETIC: WON 0x770e00
// SYNTHETIC: WON 0x770e10
// SYNTHETIC: WON 0x770e20
// SYNTHETIC: WON 0x770e30
// SYNTHETIC: WON 0x770e40
// SYNTHETIC: WON 0x770e50
// SYNTHETIC: WON 0x770e60
// SYNTHETIC: WON 0x770e70
// SYNTHETIC: WON 0x770e80
// SYNTHETIC: WON 0x770e90
// SYNTHETIC: WON 0x770ea0
// SYNTHETIC: WON 0x770eb0
// SYNTHETIC: WON 0x770ec0
// SYNTHETIC: WON 0x770ed0
// SYNTHETIC: WON 0x770ee0
// SYNTHETIC: WON 0x770ef0
// SYNTHETIC: WON 0x770f00
// SYNTHETIC: WON 0x770f10
// SYNTHETIC: WON 0x770f20
// SYNTHETIC: WON 0x770f30
// SYNTHETIC: WON 0x770f40
// SYNTHETIC: WON 0x770f50
// SYNTHETIC: WON 0x770f60
// SYNTHETIC: WON 0x770f70
// SYNTHETIC: WON 0x770f80
// SYNTHETIC: WON 0x770f90
// SYNTHETIC: WON 0x770fa0
// SYNTHETIC: WON 0x770fb0
// SYNTHETIC: WON 0x770fc0
// SYNTHETIC: WON 0x770fd0
// SYNTHETIC: WON 0x770fe0
// SYNTHETIC: WON 0x770ff0
// SYNTHETIC: WON 0x771000
// SYNTHETIC: WON 0x771010
// SYNTHETIC: WON 0x771020
// SYNTHETIC: WON 0x771030
// SYNTHETIC: WON 0x771040
// SYNTHETIC: WON 0x771050
// SYNTHETIC: WON 0x771060
// SYNTHETIC: WON 0x771070
// SYNTHETIC: WON 0x771080
// SYNTHETIC: WON 0x771090
// SYNTHETIC: WON 0x7710a0
// SYNTHETIC: WON 0x7710b0
// SYNTHETIC: WON 0x7710c0
// SYNTHETIC: WON 0x7710d0
// SYNTHETIC: WON 0x7710e0
// SYNTHETIC: WON 0x7710f0
// SYNTHETIC: WON 0x771100
// SYNTHETIC: WON 0x771110
// SYNTHETIC: WON 0x771120
// SYNTHETIC: WON 0x771130
// SYNTHETIC: WON 0x771140
// SYNTHETIC: WON 0x771150
// SYNTHETIC: WON 0x771160
// SYNTHETIC: WON 0x771170
// SYNTHETIC: WON 0x771180
// SYNTHETIC: WON 0x771190
// SYNTHETIC: WON 0x7711a0
// SYNTHETIC: WON 0x7711b0
// SYNTHETIC: WON 0x7711c0
// SYNTHETIC: WON 0x7711d0
// SYNTHETIC: WON 0x7711e0
// SYNTHETIC: WON 0x7711f0
// SYNTHETIC: WON 0x771200
// SYNTHETIC: WON 0x771210
// SYNTHETIC: WON 0x771220
// SYNTHETIC: WON 0x771230
// SYNTHETIC: WON 0x771240
// SYNTHETIC: WON 0x771250
// SYNTHETIC: WON 0x771260
// SYNTHETIC: WON 0x771270
// SYNTHETIC: WON 0x771280
// SYNTHETIC: WON 0x771290
// SYNTHETIC: WON 0x7712a0
// SYNTHETIC: WON 0x7712b0
// SYNTHETIC: WON 0x7712c0
// SYNTHETIC: WON 0x7712d0
// SYNTHETIC: WON 0x7712e0
// SYNTHETIC: WON 0x7712f0
// SYNTHETIC: WON 0x771300
// SYNTHETIC: WON 0x771310
// SYNTHETIC: WON 0x771320
// SYNTHETIC: WON 0x771330
// SYNTHETIC: WON 0x771340
// SYNTHETIC: WON 0x771350
// SYNTHETIC: WON 0x771360
// SYNTHETIC: WON 0x771370
// SYNTHETIC: WON 0x771380
// SYNTHETIC: WON 0x771390
// SYNTHETIC: WON 0x7713a0
// SYNTHETIC: WON 0x7713b0
// SYNTHETIC: WON 0x7713c0
// SYNTHETIC: WON 0x7713d0
// SYNTHETIC: WON 0x7713e0
// SYNTHETIC: WON 0x7713f0
// SYNTHETIC: WON 0x771400
// SYNTHETIC: WON 0x771410
// SYNTHETIC: WON 0x771420
// SYNTHETIC: WON 0x771430
// SYNTHETIC: WON 0x771440
// SYNTHETIC: WON 0x771450
// SYNTHETIC: WON 0x771460
// SYNTHETIC: WON 0x771470
// SYNTHETIC: WON 0x771480
// SYNTHETIC: WON 0x771490
// SYNTHETIC: WON 0x7714a0
// SYNTHETIC: WON 0x7714b0
// SYNTHETIC: WON 0x7714c0
// SYNTHETIC: WON 0x7714d0
// SYNTHETIC: WON 0x7714e0
// SYNTHETIC: WON 0x7714f0
// SYNTHETIC: WON 0x771500
// SYNTHETIC: WON 0x771510
// SYNTHETIC: WON 0x771520
// SYNTHETIC: WON 0x771530
// SYNTHETIC: WON 0x771540
// SYNTHETIC: WON 0x771550
// SYNTHETIC: WON 0x771560
// SYNTHETIC: WON 0x771570
// SYNTHETIC: WON 0x771580
// SYNTHETIC: WON 0x771590
// SYNTHETIC: WON 0x7715a0
// SYNTHETIC: WON 0x7715b0
// SYNTHETIC: WON 0x7715c0
// SYNTHETIC: WON 0x7715d0
// SYNTHETIC: WON 0x7715e0
// SYNTHETIC: WON 0x7715f0
// SYNTHETIC: WON 0x771600
// SYNTHETIC: WON 0x771610
// SYNTHETIC: WON 0x771620
// SYNTHETIC: WON 0x771630
// SYNTHETIC: WON 0x771640
// SYNTHETIC: WON 0x771650
// SYNTHETIC: WON 0x771660
// SYNTHETIC: WON 0x771670
// SYNTHETIC: WON 0x771680
// SYNTHETIC: WON 0x771690
// SYNTHETIC: WON 0x7716a0
// SYNTHETIC: WON 0x7716b0
// SYNTHETIC: WON 0x7716c0
// SYNTHETIC: WON 0x7716d0
// SYNTHETIC: WON 0x7716e0
// SYNTHETIC: WON 0x7716f0
// SYNTHETIC: WON 0x771700
// SYNTHETIC: WON 0x771710
// SYNTHETIC: WON 0x771720
// SYNTHETIC: WON 0x771730
// SYNTHETIC: WON 0x771740
// SYNTHETIC: WON 0x771750
// SYNTHETIC: WON 0x771760
// SYNTHETIC: WON 0x771770
// SYNTHETIC: WON 0x771780
// SYNTHETIC: WON 0x771790
// SYNTHETIC: WON 0x7717a0
// SYNTHETIC: WON 0x7717b0
// SYNTHETIC: WON 0x7717c0
// SYNTHETIC: WON 0x7717d0
// SYNTHETIC: WON 0x7717e0
// SYNTHETIC: WON 0x7717f0
// SYNTHETIC: WON 0x771800
// SYNTHETIC: WON 0x771810
// SYNTHETIC: WON 0x771820
// SYNTHETIC: WON 0x771830
// SYNTHETIC: WON 0x771840
// SYNTHETIC: WON 0x771850
// SYNTHETIC: WON 0x771860
// SYNTHETIC: WON 0x771870
// SYNTHETIC: WON 0x771880
// SYNTHETIC: WON 0x771890
// SYNTHETIC: WON 0x7718a0
// SYNTHETIC: WON 0x7718b0
// SYNTHETIC: WON 0x7718c0
// SYNTHETIC: WON 0x7718d0
// SYNTHETIC: WON 0x7718e0
// SYNTHETIC: WON 0x7718f0
// SYNTHETIC: WON 0x771900
// SYNTHETIC: WON 0x771910
// SYNTHETIC: WON 0x771920
// SYNTHETIC: WON 0x771930
// SYNTHETIC: WON 0x771940
// SYNTHETIC: WON 0x771950
// SYNTHETIC: WON 0x771960
// SYNTHETIC: WON 0x771970
// SYNTHETIC: WON 0x771980
// SYNTHETIC: WON 0x771990
// SYNTHETIC: WON 0x7719a0
// SYNTHETIC: WON 0x7719b0
// SYNTHETIC: WON 0x7719c0
// SYNTHETIC: WON 0x7719d0
// SYNTHETIC: WON 0x7719e0
// SYNTHETIC: WON 0x7719f0
// SYNTHETIC: WON 0x771a00
// SYNTHETIC: WON 0x771a10
// SYNTHETIC: WON 0x771a20
// SYNTHETIC: WON 0x771a30
// SYNTHETIC: WON 0x771a40
// SYNTHETIC: WON 0x771a50
// SYNTHETIC: WON 0x771a60
// SYNTHETIC: WON 0x771a70
// SYNTHETIC: WON 0x771a80
// SYNTHETIC: WON 0x771a90
// SYNTHETIC: WON 0x771aa0
// SYNTHETIC: WON 0x771ab0
// SYNTHETIC: WON 0x771ac0
// SYNTHETIC: WON 0x771ad0
// SYNTHETIC: WON 0x771ae0
// SYNTHETIC: WON 0x771af0
// SYNTHETIC: WON 0x771b00
// SYNTHETIC: WON 0x771b10
// SYNTHETIC: WON 0x771b20
// SYNTHETIC: WON 0x771b30
// SYNTHETIC: WON 0x771b40
// SYNTHETIC: WON 0x771b50
// SYNTHETIC: WON 0x771b60
// SYNTHETIC: WON 0x771b70
// SYNTHETIC: WON 0x771b80
// SYNTHETIC: WON 0x771b90
// SYNTHETIC: WON 0x771ba0
// SYNTHETIC: WON 0x771bb0
// SYNTHETIC: WON 0x771bc0
// SYNTHETIC: WON 0x771bd0
// SYNTHETIC: WON 0x771be0
// SYNTHETIC: WON 0x771bf0
// SYNTHETIC: WON 0x771c00
// SYNTHETIC: WON 0x771c10
// SYNTHETIC: WON 0x771c20
// SYNTHETIC: WON 0x771c30
// SYNTHETIC: WON 0x771c40
// SYNTHETIC: WON 0x771c50
// SYNTHETIC: WON 0x771c60
// SYNTHETIC: WON 0x771c70
// SYNTHETIC: WON 0x771c80
// SYNTHETIC: WON 0x771c90
// SYNTHETIC: WON 0x771ca0
// SYNTHETIC: WON 0x771cb0
// SYNTHETIC: WON 0x771cc0
// SYNTHETIC: WON 0x771cd0
// SYNTHETIC: WON 0x771ce0
// SYNTHETIC: WON 0x771cf0
// SYNTHETIC: WON 0x771d00
// SYNTHETIC: WON 0x771d10
// SYNTHETIC: WON 0x771d20
// SYNTHETIC: WON 0x771d30
// SYNTHETIC: WON 0x771d40
// SYNTHETIC: WON 0x771d50
// SYNTHETIC: WON 0x771d60
// SYNTHETIC: WON 0x771d70
// SYNTHETIC: WON 0x771d80
// SYNTHETIC: WON 0x771d90
// SYNTHETIC: WON 0x771da0
// SYNTHETIC: WON 0x771db0
// SYNTHETIC: WON 0x771dc0
// SYNTHETIC: WON 0x771dd0
// SYNTHETIC: WON 0x771de0
// SYNTHETIC: WON 0x771df0
// SYNTHETIC: WON 0x771e00
// SYNTHETIC: WON 0x771e10
// SYNTHETIC: WON 0x771e20
// SYNTHETIC: WON 0x771e30
// SYNTHETIC: WON 0x771e40
// SYNTHETIC: WON 0x771e50
// SYNTHETIC: WON 0x771e60
// SYNTHETIC: WON 0x771e70
// SYNTHETIC: WON 0x771e80
// SYNTHETIC: WON 0x771e90
// SYNTHETIC: WON 0x771ea0
// SYNTHETIC: WON 0x771eb0
// SYNTHETIC: WON 0x771ec0
// SYNTHETIC: WON 0x771ed0
// SYNTHETIC: WON 0x771ee0
// SYNTHETIC: WON 0x771ef0
// SYNTHETIC: WON 0x771f00
// SYNTHETIC: WON 0x771f10
// SYNTHETIC: WON 0x771f20
// SYNTHETIC: WON 0x771f30
// SYNTHETIC: WON 0x771f40
// SYNTHETIC: WON 0x771f50
// SYNTHETIC: WON 0x771f60
// SYNTHETIC: WON 0x771f70
// SYNTHETIC: WON 0x771f80
// SYNTHETIC: WON 0x771f90
// SYNTHETIC: WON 0x771fa0
// SYNTHETIC: WON 0x771fb0
// SYNTHETIC: WON 0x771fc0
// SYNTHETIC: WON 0x771fd0
// SYNTHETIC: WON 0x771fe0
// SYNTHETIC: WON 0x771ff0
// SYNTHETIC: WON 0x772000
// SYNTHETIC: WON 0x772010
// SYNTHETIC: WON 0x772020
// SYNTHETIC: WON 0x772030
// SYNTHETIC: WON 0x772040
// SYNTHETIC: WON 0x772050
// SYNTHETIC: WON 0x772060
// SYNTHETIC: WON 0x772070
// SYNTHETIC: WON 0x772080
// SYNTHETIC: WON 0x772090
// SYNTHETIC: WON 0x7720a0
// SYNTHETIC: WON 0x7720b0
// SYNTHETIC: WON 0x7720c0
// SYNTHETIC: WON 0x7720d0
// SYNTHETIC: WON 0x7720e0
// SYNTHETIC: WON 0x7720f0
// SYNTHETIC: WON 0x772100
// SYNTHETIC: WON 0x772110
// SYNTHETIC: WON 0x772120
// SYNTHETIC: WON 0x772130
// SYNTHETIC: WON 0x772140
// SYNTHETIC: WON 0x772150
// SYNTHETIC: WON 0x772160
// SYNTHETIC: WON 0x772170
// SYNTHETIC: WON 0x772180
// SYNTHETIC: WON 0x772190
// SYNTHETIC: WON 0x7721a0
// SYNTHETIC: WON 0x7721b0
// SYNTHETIC: WON 0x7721c0
// SYNTHETIC: WON 0x7721d0
// SYNTHETIC: WON 0x7721e0
// SYNTHETIC: WON 0x7721f0
// SYNTHETIC: WON 0x772200
// SYNTHETIC: WON 0x772210
// SYNTHETIC: WON 0x772220
// SYNTHETIC: WON 0x772230
// SYNTHETIC: WON 0x772240
// SYNTHETIC: WON 0x772250
// SYNTHETIC: WON 0x772260
// SYNTHETIC: WON 0x772270
// SYNTHETIC: WON 0x772280
// SYNTHETIC: WON 0x772290
// SYNTHETIC: WON 0x7722a0
// SYNTHETIC: WON 0x7722b0
// SYNTHETIC: WON 0x7722c0
// SYNTHETIC: WON 0x7722d0
// SYNTHETIC: WON 0x7722e0
// SYNTHETIC: WON 0x7722f0
// SYNTHETIC: WON 0x772300
// SYNTHETIC: WON 0x772310
// SYNTHETIC: WON 0x772320
// SYNTHETIC: WON 0x772330
// SYNTHETIC: WON 0x772340
// SYNTHETIC: WON 0x772350
// SYNTHETIC: WON 0x772360
// SYNTHETIC: WON 0x772370
// SYNTHETIC: WON 0x772380
// SYNTHETIC: WON 0x772390
// SYNTHETIC: WON 0x7723a0
// SYNTHETIC: WON 0x7723b0
// SYNTHETIC: WON 0x7723c0
// SYNTHETIC: WON 0x7723d0
// SYNTHETIC: WON 0x7723e0
// SYNTHETIC: WON 0x7723f0
// SYNTHETIC: WON 0x772400
// SYNTHETIC: WON 0x772410
// SYNTHETIC: WON 0x772420
// SYNTHETIC: WON 0x772430
// SYNTHETIC: WON 0x772440
// SYNTHETIC: WON 0x772450
// SYNTHETIC: WON 0x772460
// SYNTHETIC: WON 0x772470
// SYNTHETIC: WON 0x772480
// SYNTHETIC: WON 0x772490
// SYNTHETIC: WON 0x7724a0
// SYNTHETIC: WON 0x7724b0
// SYNTHETIC: WON 0x7724c0
// SYNTHETIC: WON 0x7724d0
// SYNTHETIC: WON 0x7724e0
// SYNTHETIC: WON 0x7724f0
// SYNTHETIC: WON 0x772500
// SYNTHETIC: WON 0x772510
// SYNTHETIC: WON 0x772520
// SYNTHETIC: WON 0x772530
// SYNTHETIC: WON 0x772540
// SYNTHETIC: WON 0x772550
// SYNTHETIC: WON 0x772560
// SYNTHETIC: WON 0x772570
// SYNTHETIC: WON 0x772580
// SYNTHETIC: WON 0x772590
// SYNTHETIC: WON 0x7725a0
// SYNTHETIC: WON 0x7725b0
// SYNTHETIC: WON 0x7725c0
// SYNTHETIC: WON 0x7725d0
// SYNTHETIC: WON 0x7725e0
// SYNTHETIC: WON 0x7725f0
// SYNTHETIC: WON 0x772600
// SYNTHETIC: WON 0x772610
// SYNTHETIC: WON 0x772620
// SYNTHETIC: WON 0x772630
// SYNTHETIC: WON 0x772640
// SYNTHETIC: WON 0x772650
// SYNTHETIC: WON 0x772660
// SYNTHETIC: WON 0x772670
// SYNTHETIC: WON 0x772680
// SYNTHETIC: WON 0x772690
// SYNTHETIC: WON 0x7726a0
// SYNTHETIC: WON 0x7726b0
// SYNTHETIC: WON 0x7726c0
// SYNTHETIC: WON 0x7726d0
// SYNTHETIC: WON 0x7726e0
// SYNTHETIC: WON 0x7726f0
// SYNTHETIC: WON 0x772700
// SYNTHETIC: WON 0x772710
// SYNTHETIC: WON 0x772720
// SYNTHETIC: WON 0x772730
// SYNTHETIC: WON 0x772740
// SYNTHETIC: WON 0x772750
// SYNTHETIC: WON 0x772760
// SYNTHETIC: WON 0x772770
// SYNTHETIC: WON 0x772780
// SYNTHETIC: WON 0x772790
// SYNTHETIC: WON 0x7727a0
// SYNTHETIC: WON 0x7727b0
// SYNTHETIC: WON 0x7727c0
// SYNTHETIC: WON 0x7727d0
// SYNTHETIC: WON 0x7727e0
// SYNTHETIC: WON 0x7727f0
// SYNTHETIC: WON 0x772800
// SYNTHETIC: WON 0x772810
// SYNTHETIC: WON 0x772820
// SYNTHETIC: WON 0x772830
// SYNTHETIC: WON 0x772840
// SYNTHETIC: WON 0x772850
// SYNTHETIC: WON 0x772860
// SYNTHETIC: WON 0x772870
// SYNTHETIC: WON 0x772880
// SYNTHETIC: WON 0x772890
// SYNTHETIC: WON 0x7728a0
// SYNTHETIC: WON 0x7728b0
// SYNTHETIC: WON 0x7728c0
// SYNTHETIC: WON 0x7728d0
// SYNTHETIC: WON 0x7728e0
// SYNTHETIC: WON 0x7728f0
// SYNTHETIC: WON 0x772900
// SYNTHETIC: WON 0x772910
// SYNTHETIC: WON 0x772920
// SYNTHETIC: WON 0x772930
// SYNTHETIC: WON 0x772940
// SYNTHETIC: WON 0x772950
// SYNTHETIC: WON 0x772960
// SYNTHETIC: WON 0x772970
// SYNTHETIC: WON 0x772980
// SYNTHETIC: WON 0x772990
// SYNTHETIC: WON 0x7729a0
// SYNTHETIC: WON 0x7729b0
// SYNTHETIC: WON 0x7729c0
// SYNTHETIC: WON 0x7729d0
// SYNTHETIC: WON 0x7729e0
// SYNTHETIC: WON 0x7729f0
// SYNTHETIC: WON 0x772a00
// SYNTHETIC: WON 0x772a10
// SYNTHETIC: WON 0x772a20
// SYNTHETIC: WON 0x772a30
// SYNTHETIC: WON 0x772a40
// SYNTHETIC: WON 0x772a50
// SYNTHETIC: WON 0x772a60
// SYNTHETIC: WON 0x772a70
// SYNTHETIC: WON 0x772a80
// SYNTHETIC: WON 0x772a90
// SYNTHETIC: WON 0x772aa0
// SYNTHETIC: WON 0x772ab0
// SYNTHETIC: WON 0x772ac0
// SYNTHETIC: WON 0x772ad0
// SYNTHETIC: WON 0x772ae0
// SYNTHETIC: WON 0x772af0
// SYNTHETIC: WON 0x772b00
// SYNTHETIC: WON 0x772b10
// SYNTHETIC: WON 0x772b20
// SYNTHETIC: WON 0x772b30
// SYNTHETIC: WON 0x772b40
// SYNTHETIC: WON 0x772b50
// SYNTHETIC: WON 0x772b60
// SYNTHETIC: WON 0x772b70
// SYNTHETIC: WON 0x772b80
// SYNTHETIC: WON 0x772b90
// SYNTHETIC: WON 0x772ba0
// SYNTHETIC: WON 0x772bb0
// SYNTHETIC: WON 0x772bc0
// SYNTHETIC: WON 0x772bd0
// SYNTHETIC: WON 0x772be0
// SYNTHETIC: WON 0x772bf0
// SYNTHETIC: WON 0x772c00
// SYNTHETIC: WON 0x772c10
// SYNTHETIC: WON 0x772c20
// SYNTHETIC: WON 0x772c30
// SYNTHETIC: WON 0x772c40
// SYNTHETIC: WON 0x772c50
// SYNTHETIC: WON 0x772c60
// SYNTHETIC: WON 0x772c70
// SYNTHETIC: WON 0x772c80
// SYNTHETIC: WON 0x772c90
// SYNTHETIC: WON 0x772ca0
// SYNTHETIC: WON 0x772cb0
// SYNTHETIC: WON 0x772cc0
// SYNTHETIC: WON 0x772cd0
// SYNTHETIC: WON 0x772ce0
// SYNTHETIC: WON 0x772cf0
// SYNTHETIC: WON 0x772d00
// SYNTHETIC: WON 0x772d10
// SYNTHETIC: WON 0x772d20
// SYNTHETIC: WON 0x772d30
// SYNTHETIC: WON 0x772d40
// SYNTHETIC: WON 0x772d50
// SYNTHETIC: WON 0x772d60
// SYNTHETIC: WON 0x772d70
// SYNTHETIC: WON 0x772d80
// SYNTHETIC: WON 0x772d90
// SYNTHETIC: WON 0x772da0
// SYNTHETIC: WON 0x772db0
// SYNTHETIC: WON 0x772dc0
// SYNTHETIC: WON 0x772dd0
// SYNTHETIC: WON 0x772de0
// SYNTHETIC: WON 0x772df0
// SYNTHETIC: WON 0x772e00
// SYNTHETIC: WON 0x772e10
// SYNTHETIC: WON 0x772e20
// SYNTHETIC: WON 0x772e30
// SYNTHETIC: WON 0x772e40
// SYNTHETIC: WON 0x772e50
// SYNTHETIC: WON 0x772e60
// SYNTHETIC: WON 0x772e70
// SYNTHETIC: WON 0x772e80
// SYNTHETIC: WON 0x772e90
// SYNTHETIC: WON 0x772ea0
// SYNTHETIC: WON 0x772eb0
// SYNTHETIC: WON 0x772ec0
// SYNTHETIC: WON 0x772ed0
// SYNTHETIC: WON 0x772ee0
// SYNTHETIC: WON 0x772ef0
// SYNTHETIC: WON 0x772f00
// SYNTHETIC: WON 0x772f10
// SYNTHETIC: WON 0x772f20
// SYNTHETIC: WON 0x772f30
// SYNTHETIC: WON 0x772f40
// SYNTHETIC: WON 0x772f50
// SYNTHETIC: WON 0x772f60
// SYNTHETIC: WON 0x772f70
// SYNTHETIC: WON 0x772f80
// SYNTHETIC: WON 0x772f90
// SYNTHETIC: WON 0x772fa0
// SYNTHETIC: WON 0x772fb0
// SYNTHETIC: WON 0x772fc0
// SYNTHETIC: WON 0x772fd0
// SYNTHETIC: WON 0x772fe0
// SYNTHETIC: WON 0x772ff0
// SYNTHETIC: WON 0x773000
// SYNTHETIC: WON 0x773010
// SYNTHETIC: WON 0x773020
// SYNTHETIC: WON 0x773030
// SYNTHETIC: WON 0x773040
// SYNTHETIC: WON 0x773050
// SYNTHETIC: WON 0x773060
// SYNTHETIC: WON 0x773070
// SYNTHETIC: WON 0x773080
// SYNTHETIC: WON 0x773090
// SYNTHETIC: WON 0x7730a0
// SYNTHETIC: WON 0x7730b0
// SYNTHETIC: WON 0x7730c0
// SYNTHETIC: WON 0x7730d0
// SYNTHETIC: WON 0x7730e0
// SYNTHETIC: WON 0x7730f0
// SYNTHETIC: WON 0x773100
// SYNTHETIC: WON 0x773110
// SYNTHETIC: WON 0x773120
// SYNTHETIC: WON 0x773130
// SYNTHETIC: WON 0x773140
// SYNTHETIC: WON 0x773150
// SYNTHETIC: WON 0x773160
// SYNTHETIC: WON 0x773170
// SYNTHETIC: WON 0x773180
// SYNTHETIC: WON 0x773190
// SYNTHETIC: WON 0x7731a0
// SYNTHETIC: WON 0x7731b0
// SYNTHETIC: WON 0x7731c0
// SYNTHETIC: WON 0x7731d0
// SYNTHETIC: WON 0x7731e0
// SYNTHETIC: WON 0x7731f0
// SYNTHETIC: WON 0x773200
// SYNTHETIC: WON 0x773210
// SYNTHETIC: WON 0x773220
// SYNTHETIC: WON 0x773230
// SYNTHETIC: WON 0x773240
// SYNTHETIC: WON 0x773250
// SYNTHETIC: WON 0x773260
// SYNTHETIC: WON 0x773270
// SYNTHETIC: WON 0x773280
// SYNTHETIC: WON 0x773290
// SYNTHETIC: WON 0x7732a0
// SYNTHETIC: WON 0x7732b0
// SYNTHETIC: WON 0x7732c0
// SYNTHETIC: WON 0x7732d0
// SYNTHETIC: WON 0x7732e0
// SYNTHETIC: WON 0x7732f0
// SYNTHETIC: WON 0x773300
// SYNTHETIC: WON 0x773310
// SYNTHETIC: WON 0x773320
// SYNTHETIC: WON 0x773330
// SYNTHETIC: WON 0x773340
// SYNTHETIC: WON 0x773350
// SYNTHETIC: WON 0x773360
// SYNTHETIC: WON 0x773370
// SYNTHETIC: WON 0x773380
// SYNTHETIC: WON 0x773390
// SYNTHETIC: WON 0x7733a0
// SYNTHETIC: WON 0x7733b0
// SYNTHETIC: WON 0x7733c0
// SYNTHETIC: WON 0x7733d0
// SYNTHETIC: WON 0x7733e0
// SYNTHETIC: WON 0x7733f0
// SYNTHETIC: WON 0x773400
// SYNTHETIC: WON 0x773410
// SYNTHETIC: WON 0x773420
// SYNTHETIC: WON 0x773430
// SYNTHETIC: WON 0x773440
// SYNTHETIC: WON 0x773450
// SYNTHETIC: WON 0x773460
// SYNTHETIC: WON 0x773470
// SYNTHETIC: WON 0x773480
// SYNTHETIC: WON 0x773490
// SYNTHETIC: WON 0x7734a0
// SYNTHETIC: WON 0x7734b0
// SYNTHETIC: WON 0x7734c0
// SYNTHETIC: WON 0x7734d0
// SYNTHETIC: WON 0x7734e0
// SYNTHETIC: WON 0x7734f0
// SYNTHETIC: WON 0x773500
// SYNTHETIC: WON 0x773510
// SYNTHETIC: WON 0x773520
// SYNTHETIC: WON 0x773530
// SYNTHETIC: WON 0x773540
// SYNTHETIC: WON 0x773550
// SYNTHETIC: WON 0x773560
// SYNTHETIC: WON 0x773570
// SYNTHETIC: WON 0x773580
// SYNTHETIC: WON 0x773590
// SYNTHETIC: WON 0x7735a0
// SYNTHETIC: WON 0x7735b0
// SYNTHETIC: WON 0x7735c0
// SYNTHETIC: WON 0x7735d0
// SYNTHETIC: WON 0x7735e0
// SYNTHETIC: WON 0x7735f0
// SYNTHETIC: WON 0x773600
// SYNTHETIC: WON 0x773610
// SYNTHETIC: WON 0x773620
// SYNTHETIC: WON 0x773630
// SYNTHETIC: WON 0x773640
// SYNTHETIC: WON 0x773650
// SYNTHETIC: WON 0x773660
// SYNTHETIC: WON 0x773670
// SYNTHETIC: WON 0x773680
// SYNTHETIC: WON 0x773690
// SYNTHETIC: WON 0x7736a0
// SYNTHETIC: WON 0x7736b0
// SYNTHETIC: WON 0x7736c0
// SYNTHETIC: WON 0x7736d0
// SYNTHETIC: WON 0x7736e0
// SYNTHETIC: WON 0x7736f0
// SYNTHETIC: WON 0x773700
// SYNTHETIC: WON 0x773710
// SYNTHETIC: WON 0x773720
// SYNTHETIC: WON 0x773730
// SYNTHETIC: WON 0x773740
// SYNTHETIC: WON 0x773750
// SYNTHETIC: WON 0x773760
// SYNTHETIC: WON 0x773770
// SYNTHETIC: WON 0x773780
// SYNTHETIC: WON 0x773790
// SYNTHETIC: WON 0x7737a0
// SYNTHETIC: WON 0x7737b0
// SYNTHETIC: WON 0x7737c0
// SYNTHETIC: WON 0x7737d0
// SYNTHETIC: WON 0x7737e0
// SYNTHETIC: WON 0x7737f0
// SYNTHETIC: WON 0x773800
// SYNTHETIC: WON 0x773810
// SYNTHETIC: WON 0x773820
// SYNTHETIC: WON 0x773830
// SYNTHETIC: WON 0x773840
// SYNTHETIC: WON 0x773850
// SYNTHETIC: WON 0x773860
// SYNTHETIC: WON 0x773870
// SYNTHETIC: WON 0x773880
// SYNTHETIC: WON 0x773890
// SYNTHETIC: WON 0x7738a0
// SYNTHETIC: WON 0x7738b0
// SYNTHETIC: WON 0x7738c0
// SYNTHETIC: WON 0x7738d0
// SYNTHETIC: WON 0x7738e0
// SYNTHETIC: WON 0x7738f0
// SYNTHETIC: WON 0x773900
// SYNTHETIC: WON 0x773910
// SYNTHETIC: WON 0x773920
// SYNTHETIC: WON 0x773930
// SYNTHETIC: WON 0x773940
// SYNTHETIC: WON 0x773950
// SYNTHETIC: WON 0x773960
// SYNTHETIC: WON 0x773970
// SYNTHETIC: WON 0x773980
// SYNTHETIC: WON 0x773990
// SYNTHETIC: WON 0x7739a0
// SYNTHETIC: WON 0x7739b0
// SYNTHETIC: WON 0x7739c0
// SYNTHETIC: WON 0x7739d0
// SYNTHETIC: WON 0x7739e0
// SYNTHETIC: WON 0x7739f0
// SYNTHETIC: WON 0x773a00
// SYNTHETIC: WON 0x773a10
// SYNTHETIC: WON 0x773a20
// SYNTHETIC: WON 0x773a30
// SYNTHETIC: WON 0x773a40
// SYNTHETIC: WON 0x773a50
// SYNTHETIC: WON 0x773a60
// SYNTHETIC: WON 0x773a70
// SYNTHETIC: WON 0x773a80
// SYNTHETIC: WON 0x773a90
// SYNTHETIC: WON 0x773aa0
// SYNTHETIC: WON 0x773ab0
// SYNTHETIC: WON 0x773ac0
// SYNTHETIC: WON 0x773ad0
// SYNTHETIC: WON 0x773ae0
// SYNTHETIC: WON 0x773af0
// SYNTHETIC: WON 0x773b00
// SYNTHETIC: WON 0x773b10
// SYNTHETIC: WON 0x773b20
// SYNTHETIC: WON 0x773b30
// SYNTHETIC: WON 0x773b40
// SYNTHETIC: WON 0x773b50
// SYNTHETIC: WON 0x773b60
// SYNTHETIC: WON 0x773b70
// SYNTHETIC: WON 0x773b80
// SYNTHETIC: WON 0x773b90
// SYNTHETIC: WON 0x773ba0
// SYNTHETIC: WON 0x773bb0
// SYNTHETIC: WON 0x773bc0
// SYNTHETIC: WON 0x773bd0
// SYNTHETIC: WON 0x773be0
// SYNTHETIC: WON 0x773bf0
// SYNTHETIC: WON 0x773c00
// SYNTHETIC: WON 0x773c10
// SYNTHETIC: WON 0x773c20
// SYNTHETIC: WON 0x773c30
// SYNTHETIC: WON 0x773c40
// SYNTHETIC: WON 0x773c50
// SYNTHETIC: WON 0x773c60
// SYNTHETIC: WON 0x773c70
// SYNTHETIC: WON 0x773c80
// SYNTHETIC: WON 0x773c90
// SYNTHETIC: WON 0x773ca0
// SYNTHETIC: WON 0x773cb0
// SYNTHETIC: WON 0x773cc0
// SYNTHETIC: WON 0x773cd0
// SYNTHETIC: WON 0x773ce0
// SYNTHETIC: WON 0x773cf0
// SYNTHETIC: WON 0x773d00
// SYNTHETIC: WON 0x773d10
// SYNTHETIC: WON 0x773d20
// SYNTHETIC: WON 0x773d30
// SYNTHETIC: WON 0x773d40
// SYNTHETIC: WON 0x773d50
// SYNTHETIC: WON 0x773d60
// SYNTHETIC: WON 0x773d70
// SYNTHETIC: WON 0x773d80
// SYNTHETIC: WON 0x773d90
// SYNTHETIC: WON 0x773da0
// SYNTHETIC: WON 0x773db0
// SYNTHETIC: WON 0x773dc0
// SYNTHETIC: WON 0x773dd0
// SYNTHETIC: WON 0x773de0
// SYNTHETIC: WON 0x773df0
// SYNTHETIC: WON 0x773e00
// SYNTHETIC: WON 0x773e10
// SYNTHETIC: WON 0x773e20
// SYNTHETIC: WON 0x773e30
// SYNTHETIC: WON 0x773e40
// SYNTHETIC: WON 0x773e50
// SYNTHETIC: WON 0x773e60
// SYNTHETIC: WON 0x773e70
// SYNTHETIC: WON 0x773e80
// SYNTHETIC: WON 0x773e90
// SYNTHETIC: WON 0x773ea0
// SYNTHETIC: WON 0x773eb0
// SYNTHETIC: WON 0x773ec0
// SYNTHETIC: WON 0x773ed0
// SYNTHETIC: WON 0x773ee0
// SYNTHETIC: WON 0x773ef0
// SYNTHETIC: WON 0x773f00
// SYNTHETIC: WON 0x773f10
// SYNTHETIC: WON 0x773f20
// SYNTHETIC: WON 0x773f30
// SYNTHETIC: WON 0x773f40
// SYNTHETIC: WON 0x773f50
// SYNTHETIC: WON 0x773f60
// SYNTHETIC: WON 0x773f70
// SYNTHETIC: WON 0x773f80
// SYNTHETIC: WON 0x773f90
// SYNTHETIC: WON 0x773fa0
// SYNTHETIC: WON 0x773fb0
// SYNTHETIC: WON 0x773fc0
// SYNTHETIC: WON 0x773fd0
// SYNTHETIC: WON 0x773fe0
// SYNTHETIC: WON 0x773ff0
// SYNTHETIC: WON 0x774000
// SYNTHETIC: WON 0x774010
// SYNTHETIC: WON 0x774020
// SYNTHETIC: WON 0x774030
// SYNTHETIC: WON 0x774040
// SYNTHETIC: WON 0x774050
// SYNTHETIC: WON 0x774060
// SYNTHETIC: WON 0x774070
// SYNTHETIC: WON 0x774080
// SYNTHETIC: WON 0x774090
// SYNTHETIC: WON 0x7740a0
// SYNTHETIC: WON 0x7740b0
// SYNTHETIC: WON 0x7740c0
// SYNTHETIC: WON 0x7740d0
// SYNTHETIC: WON 0x7740e0
// SYNTHETIC: WON 0x7740f0
// SYNTHETIC: WON 0x774100
// SYNTHETIC: WON 0x774110
// SYNTHETIC: WON 0x774120
// SYNTHETIC: WON 0x774130
// SYNTHETIC: WON 0x774140
// SYNTHETIC: WON 0x774150
// SYNTHETIC: WON 0x774160
// SYNTHETIC: WON 0x774170
// SYNTHETIC: WON 0x774180
// SYNTHETIC: WON 0x774190
// SYNTHETIC: WON 0x7741a0
// SYNTHETIC: WON 0x7741b0
// SYNTHETIC: WON 0x7741c0
// SYNTHETIC: WON 0x7741d0
// SYNTHETIC: WON 0x7741e0
// SYNTHETIC: WON 0x7741f0
// SYNTHETIC: WON 0x774200
// SYNTHETIC: WON 0x774210
// SYNTHETIC: WON 0x774220
// SYNTHETIC: WON 0x774230
// SYNTHETIC: WON 0x774240
// SYNTHETIC: WON 0x774250
// SYNTHETIC: WON 0x774260
// SYNTHETIC: WON 0x774270
// SYNTHETIC: WON 0x774280
// SYNTHETIC: WON 0x774290
// SYNTHETIC: WON 0x7742a0
// SYNTHETIC: WON 0x7742b0
// SYNTHETIC: WON 0x7742c0
// SYNTHETIC: WON 0x7742d0
// SYNTHETIC: WON 0x7742e0
// SYNTHETIC: WON 0x7742f0
// SYNTHETIC: WON 0x774300
// SYNTHETIC: WON 0x774310
// SYNTHETIC: WON 0x774320
// SYNTHETIC: WON 0x774330
// SYNTHETIC: WON 0x774340
// SYNTHETIC: WON 0x774350
// SYNTHETIC: WON 0x774360
// SYNTHETIC: WON 0x774370
// SYNTHETIC: WON 0x774380
// SYNTHETIC: WON 0x774390
// SYNTHETIC: WON 0x7743a0
// SYNTHETIC: WON 0x7743b0
// SYNTHETIC: WON 0x7743c0
// SYNTHETIC: WON 0x7743d0
// SYNTHETIC: WON 0x7743e0
// SYNTHETIC: WON 0x7743f0
// SYNTHETIC: WON 0x774400
// SYNTHETIC: WON 0x774410
// SYNTHETIC: WON 0x774420
// SYNTHETIC: WON 0x774430
// SYNTHETIC: WON 0x774440
// SYNTHETIC: WON 0x774450
// SYNTHETIC: WON 0x774460
// SYNTHETIC: WON 0x774470
// SYNTHETIC: WON 0x774480
// SYNTHETIC: WON 0x774490
// SYNTHETIC: WON 0x7744a0
// SYNTHETIC: WON 0x7744b0
// SYNTHETIC: WON 0x7744c0
// SYNTHETIC: WON 0x7744d0
// SYNTHETIC: WON 0x7744e0
// SYNTHETIC: WON 0x7744f0
// SYNTHETIC: WON 0x774500
// SYNTHETIC: WON 0x774510
// SYNTHETIC: WON 0x774520
// SYNTHETIC: WON 0x774530
// SYNTHETIC: WON 0x774540
// SYNTHETIC: WON 0x774550
// SYNTHETIC: WON 0x774560
// SYNTHETIC: WON 0x774570
// SYNTHETIC: WON 0x774580
// SYNTHETIC: WON 0x774590
// SYNTHETIC: WON 0x7745a0
// SYNTHETIC: WON 0x7745b0
// SYNTHETIC: WON 0x7745c0
// SYNTHETIC: WON 0x7745d0
// SYNTHETIC: WON 0x7745e0
// SYNTHETIC: WON 0x7745f0
// SYNTHETIC: WON 0x774600
// SYNTHETIC: WON 0x774610
// SYNTHETIC: WON 0x774620
// SYNTHETIC: WON 0x774630
// SYNTHETIC: WON 0x774640
// SYNTHETIC: WON 0x774650
// SYNTHETIC: WON 0x774660
// SYNTHETIC: WON 0x774670
// SYNTHETIC: WON 0x774680
// SYNTHETIC: WON 0x774690
// SYNTHETIC: WON 0x7746a0
// SYNTHETIC: WON 0x7746b0
// SYNTHETIC: WON 0x7746c0
// SYNTHETIC: WON 0x7746d0
// SYNTHETIC: WON 0x7746e0
// SYNTHETIC: WON 0x7746f0
// SYNTHETIC: WON 0x774700
// SYNTHETIC: WON 0x774710
// SYNTHETIC: WON 0x774720
// SYNTHETIC: WON 0x774730
// SYNTHETIC: WON 0x774740
// SYNTHETIC: WON 0x774750
// SYNTHETIC: WON 0x774760
// SYNTHETIC: WON 0x774770
// SYNTHETIC: WON 0x774780
// SYNTHETIC: WON 0x774790
// SYNTHETIC: WON 0x7747a0
// SYNTHETIC: WON 0x7747b0
// SYNTHETIC: WON 0x7747c0
// SYNTHETIC: WON 0x7747d0
// SYNTHETIC: WON 0x7747e0
// SYNTHETIC: WON 0x7747f0
// SYNTHETIC: WON 0x774800
// SYNTHETIC: WON 0x774810
// SYNTHETIC: WON 0x774820
// SYNTHETIC: WON 0x774830
// SYNTHETIC: WON 0x774840
// SYNTHETIC: WON 0x774850
// SYNTHETIC: WON 0x774860
// SYNTHETIC: WON 0x774870
// SYNTHETIC: WON 0x774880
// SYNTHETIC: WON 0x774890
// SYNTHETIC: WON 0x7748a0
// SYNTHETIC: WON 0x7748b0
// SYNTHETIC: WON 0x7748c0
// SYNTHETIC: WON 0x7748d0
// SYNTHETIC: WON 0x7748e0
// SYNTHETIC: WON 0x7748f0
// SYNTHETIC: WON 0x774900
// SYNTHETIC: WON 0x774910
// SYNTHETIC: WON 0x774920
// SYNTHETIC: WON 0x774930
// SYNTHETIC: WON 0x774940
// SYNTHETIC: WON 0x774950
// SYNTHETIC: WON 0x774960
// SYNTHETIC: WON 0x774970
// SYNTHETIC: WON 0x774980
// SYNTHETIC: WON 0x774990
// SYNTHETIC: WON 0x7749a0
// SYNTHETIC: WON 0x7749b0
// SYNTHETIC: WON 0x7749c0
// SYNTHETIC: WON 0x7749d0
// SYNTHETIC: WON 0x7749e0
// SYNTHETIC: WON 0x7749f0
// SYNTHETIC: WON 0x774a00
// SYNTHETIC: WON 0x774a10
// SYNTHETIC: WON 0x774a20
// SYNTHETIC: WON 0x774a30
// SYNTHETIC: WON 0x774a40
// SYNTHETIC: WON 0x774a50
// SYNTHETIC: WON 0x774a60
// SYNTHETIC: WON 0x774a70
// SYNTHETIC: WON 0x774a80
// SYNTHETIC: WON 0x774a90
// SYNTHETIC: WON 0x774aa0
// SYNTHETIC: WON 0x774ab0
// SYNTHETIC: WON 0x774ac0
// SYNTHETIC: WON 0x774ad0
// SYNTHETIC: WON 0x774ae0
// SYNTHETIC: WON 0x774af0
// SYNTHETIC: WON 0x774b00
// SYNTHETIC: WON 0x774b10
// SYNTHETIC: WON 0x774b20
// SYNTHETIC: WON 0x774b30
// SYNTHETIC: WON 0x774b40
// SYNTHETIC: WON 0x774b50
// SYNTHETIC: WON 0x774b60
// SYNTHETIC: WON 0x774b70
// SYNTHETIC: WON 0x774b80
// SYNTHETIC: WON 0x774b90
// SYNTHETIC: WON 0x774ba0
// SYNTHETIC: WON 0x774bb0
// SYNTHETIC: WON 0x774bc0
// SYNTHETIC: WON 0x774bd0
// SYNTHETIC: WON 0x774be0
// SYNTHETIC: WON 0x774bf0
// SYNTHETIC: WON 0x774c00
// SYNTHETIC: WON 0x774c10
// SYNTHETIC: WON 0x774c20
// SYNTHETIC: WON 0x774c30
// SYNTHETIC: WON 0x774c40
// SYNTHETIC: WON 0x774c50
// SYNTHETIC: WON 0x774c60
// SYNTHETIC: WON 0x774c70
// SYNTHETIC: WON 0x774c80
// SYNTHETIC: WON 0x774c90
// SYNTHETIC: WON 0x774ca0
// SYNTHETIC: WON 0x774cb0
// SYNTHETIC: WON 0x774cc0
// SYNTHETIC: WON 0x774cd0
// SYNTHETIC: WON 0x774ce0
// SYNTHETIC: WON 0x774cf0
// SYNTHETIC: WON 0x774d00
// SYNTHETIC: WON 0x774d10
// SYNTHETIC: WON 0x774d20
// SYNTHETIC: WON 0x774d30
// SYNTHETIC: WON 0x774d40
// SYNTHETIC: WON 0x774d50
// SYNTHETIC: WON 0x774d60
// SYNTHETIC: WON 0x774d70
// SYNTHETIC: WON 0x774d80
// SYNTHETIC: WON 0x774d90
// SYNTHETIC: WON 0x774da0
// SYNTHETIC: WON 0x774db0
// SYNTHETIC: WON 0x774dc0
// SYNTHETIC: WON 0x774dd0
// SYNTHETIC: WON 0x774de0
// SYNTHETIC: WON 0x774df0
// SYNTHETIC: WON 0x774e00
// SYNTHETIC: WON 0x774e10
// SYNTHETIC: WON 0x774e20
// SYNTHETIC: WON 0x774e30
// SYNTHETIC: WON 0x774e40
// SYNTHETIC: WON 0x774e50
// SYNTHETIC: WON 0x774e60
// SYNTHETIC: WON 0x774e70
// SYNTHETIC: WON 0x774e80
// SYNTHETIC: WON 0x774e90
// SYNTHETIC: WON 0x774ea0
// SYNTHETIC: WON 0x774eb0
// SYNTHETIC: WON 0x774ec0
// SYNTHETIC: WON 0x774ed0
// SYNTHETIC: WON 0x774ee0
// SYNTHETIC: WON 0x774ef0
// SYNTHETIC: WON 0x774f00
// SYNTHETIC: WON 0x774f10
// SYNTHETIC: WON 0x774f20
// SYNTHETIC: WON 0x774f30
// SYNTHETIC: WON 0x774f40
// SYNTHETIC: WON 0x774f50
// SYNTHETIC: WON 0x774f60
// SYNTHETIC: WON 0x774f70
// SYNTHETIC: WON 0x774f80
// SYNTHETIC: WON 0x774f90
// SYNTHETIC: WON 0x774fa0
// SYNTHETIC: WON 0x774fb0
// SYNTHETIC: WON 0x774fc0
// SYNTHETIC: WON 0x774fd0
// SYNTHETIC: WON 0x774fe0
// SYNTHETIC: WON 0x774ff0
// SYNTHETIC: WON 0x775000
// SYNTHETIC: WON 0x775010
// SYNTHETIC: WON 0x775020
// SYNTHETIC: WON 0x775030
// SYNTHETIC: WON 0x775040
// SYNTHETIC: WON 0x775050
// SYNTHETIC: WON 0x775060
// SYNTHETIC: WON 0x775070
// SYNTHETIC: WON 0x775080
// SYNTHETIC: WON 0x775090
// SYNTHETIC: WON 0x7750a0
// SYNTHETIC: WON 0x7750b0
// SYNTHETIC: WON 0x7750c0
// SYNTHETIC: WON 0x7750d0
// SYNTHETIC: WON 0x7750e0
// SYNTHETIC: WON 0x7750f0
// SYNTHETIC: WON 0x775100
// SYNTHETIC: WON 0x775110
// SYNTHETIC: WON 0x775120
// SYNTHETIC: WON 0x775130
// SYNTHETIC: WON 0x775140
// SYNTHETIC: WON 0x775150
// SYNTHETIC: WON 0x775160
// SYNTHETIC: WON 0x775170
// SYNTHETIC: WON 0x775180
// SYNTHETIC: WON 0x775190
// SYNTHETIC: WON 0x7751a0
// SYNTHETIC: WON 0x7751b0
// SYNTHETIC: WON 0x7751c0
// SYNTHETIC: WON 0x7751d0
// SYNTHETIC: WON 0x7751e0
// SYNTHETIC: WON 0x7751f0
// SYNTHETIC: WON 0x775200
// SYNTHETIC: WON 0x775210
// SYNTHETIC: WON 0x775220
// SYNTHETIC: WON 0x775230
// SYNTHETIC: WON 0x775240
// SYNTHETIC: WON 0x775250
// SYNTHETIC: WON 0x775260
// SYNTHETIC: WON 0x775270
// SYNTHETIC: WON 0x775280
// SYNTHETIC: WON 0x775290
// SYNTHETIC: WON 0x7752a0
// SYNTHETIC: WON 0x7752b0
// SYNTHETIC: WON 0x7752c0
// SYNTHETIC: WON 0x7752d0
// SYNTHETIC: WON 0x7752e0
// SYNTHETIC: WON 0x7752f0
// SYNTHETIC: WON 0x775300
// SYNTHETIC: WON 0x775310
// SYNTHETIC: WON 0x775320
// SYNTHETIC: WON 0x775330
// SYNTHETIC: WON 0x775340
// SYNTHETIC: WON 0x775350
// SYNTHETIC: WON 0x775360
// SYNTHETIC: WON 0x775370
// SYNTHETIC: WON 0x775380
// SYNTHETIC: WON 0x775390
// SYNTHETIC: WON 0x7753a0
// SYNTHETIC: WON 0x7753b0
// SYNTHETIC: WON 0x7753c0
// SYNTHETIC: WON 0x7753d0
// SYNTHETIC: WON 0x7753e0
// SYNTHETIC: WON 0x7753f0
// SYNTHETIC: WON 0x775400
// SYNTHETIC: WON 0x775410
// SYNTHETIC: WON 0x775420
// SYNTHETIC: WON 0x775430
// SYNTHETIC: WON 0x775440
// SYNTHETIC: WON 0x775450
// SYNTHETIC: WON 0x775460
// SYNTHETIC: WON 0x775470
// SYNTHETIC: WON 0x775480
// SYNTHETIC: WON 0x775490
// SYNTHETIC: WON 0x7754a0
// SYNTHETIC: WON 0x7754b0
// SYNTHETIC: WON 0x7754c0
// SYNTHETIC: WON 0x7754d0
// SYNTHETIC: WON 0x7754e0
// SYNTHETIC: WON 0x7754f0
// SYNTHETIC: WON 0x775500
// SYNTHETIC: WON 0x775510
// SYNTHETIC: WON 0x775520
// SYNTHETIC: WON 0x775530
// SYNTHETIC: WON 0x775540
// SYNTHETIC: WON 0x775550
// SYNTHETIC: WON 0x775560
// SYNTHETIC: WON 0x775570
// SYNTHETIC: WON 0x775580
// SYNTHETIC: WON 0x775590
// SYNTHETIC: WON 0x7755a0
// SYNTHETIC: WON 0x7755b0
// SYNTHETIC: WON 0x7755c0
// SYNTHETIC: WON 0x7755d0
// SYNTHETIC: WON 0x7755e0
// SYNTHETIC: WON 0x7755f0
// SYNTHETIC: WON 0x775600
// SYNTHETIC: WON 0x775610
// SYNTHETIC: WON 0x775620
// SYNTHETIC: WON 0x775630
// SYNTHETIC: WON 0x775640
// SYNTHETIC: WON 0x775650
// SYNTHETIC: WON 0x775660
// SYNTHETIC: WON 0x775670
// SYNTHETIC: WON 0x775680
// SYNTHETIC: WON 0x775690
// SYNTHETIC: WON 0x7756a0
// SYNTHETIC: WON 0x7756b0
// SYNTHETIC: WON 0x7756c0
// SYNTHETIC: WON 0x7756d0
// SYNTHETIC: WON 0x7756e0
// SYNTHETIC: WON 0x7756f0
// SYNTHETIC: WON 0x775700
// SYNTHETIC: WON 0x775710
// SYNTHETIC: WON 0x775720
// SYNTHETIC: WON 0x775730
// SYNTHETIC: WON 0x775740
// SYNTHETIC: WON 0x775750
// SYNTHETIC: WON 0x775760
// SYNTHETIC: WON 0x775770
// SYNTHETIC: WON 0x775780
// SYNTHETIC: WON 0x775790
// SYNTHETIC: WON 0x7757a0
// SYNTHETIC: WON 0x7757b0
// SYNTHETIC: WON 0x7757c0
// SYNTHETIC: WON 0x7757d0
// SYNTHETIC: WON 0x7757e0
// SYNTHETIC: WON 0x7757f0
// SYNTHETIC: WON 0x775800
// SYNTHETIC: WON 0x775810
// SYNTHETIC: WON 0x775820
// SYNTHETIC: WON 0x775830
// SYNTHETIC: WON 0x775840
// SYNTHETIC: WON 0x775850
// SYNTHETIC: WON 0x775860
// SYNTHETIC: WON 0x775870
// SYNTHETIC: WON 0x775880
// SYNTHETIC: WON 0x775890
// SYNTHETIC: WON 0x7758a0
// SYNTHETIC: WON 0x7758b0
// SYNTHETIC: WON 0x7758c0
// SYNTHETIC: WON 0x7758d0
// SYNTHETIC: WON 0x7758e0
// SYNTHETIC: WON 0x7758f0
// SYNTHETIC: WON 0x775900
// SYNTHETIC: WON 0x775910
// SYNTHETIC: WON 0x775920
// SYNTHETIC: WON 0x775930
// SYNTHETIC: WON 0x775940
// SYNTHETIC: WON 0x775950
// SYNTHETIC: WON 0x775960
// SYNTHETIC: WON 0x775970
// SYNTHETIC: WON 0x775980
// SYNTHETIC: WON 0x775990
// SYNTHETIC: WON 0x7759a0
// SYNTHETIC: WON 0x7759b0
// SYNTHETIC: WON 0x7759c0
// SYNTHETIC: WON 0x7759d0
// SYNTHETIC: WON 0x7759e0
// SYNTHETIC: WON 0x7759f0
// SYNTHETIC: WON 0x775a00
// SYNTHETIC: WON 0x775a10
// SYNTHETIC: WON 0x775a20
// SYNTHETIC: WON 0x775a30
// SYNTHETIC: WON 0x775a40
// SYNTHETIC: WON 0x775a50
// SYNTHETIC: WON 0x775a60
// SYNTHETIC: WON 0x775a70
// SYNTHETIC: WON 0x775a80
// SYNTHETIC: WON 0x775a90
// SYNTHETIC: WON 0x775aa0
// SYNTHETIC: WON 0x775ab0
// SYNTHETIC: WON 0x775ac0
// SYNTHETIC: WON 0x775ad0
// SYNTHETIC: WON 0x775ae0
// SYNTHETIC: WON 0x775af0
// SYNTHETIC: WON 0x775b00
// SYNTHETIC: WON 0x775b10
// SYNTHETIC: WON 0x775b20
// SYNTHETIC: WON 0x775b30
// SYNTHETIC: WON 0x775b40
// SYNTHETIC: WON 0x775b50
// SYNTHETIC: WON 0x775b60
// SYNTHETIC: WON 0x775b70
// SYNTHETIC: WON 0x775b80
// SYNTHETIC: WON 0x775b90
// SYNTHETIC: WON 0x775ba0
// SYNTHETIC: WON 0x775bb0
// SYNTHETIC: WON 0x775bc0
// SYNTHETIC: WON 0x775bd0
// SYNTHETIC: WON 0x775be0
// SYNTHETIC: WON 0x775bf0
// SYNTHETIC: WON 0x775c00
// SYNTHETIC: WON 0x775c10
// SYNTHETIC: WON 0x775c20
// SYNTHETIC: WON 0x775c30
// SYNTHETIC: WON 0x775c40
// SYNTHETIC: WON 0x775c50
// SYNTHETIC: WON 0x775c60
// SYNTHETIC: WON 0x775c70
// SYNTHETIC: WON 0x775c80
// SYNTHETIC: WON 0x775c90
// SYNTHETIC: WON 0x775ca0
// SYNTHETIC: WON 0x775cb0
// SYNTHETIC: WON 0x775cc0
// SYNTHETIC: WON 0x775cd0
// SYNTHETIC: WON 0x775ce0
// SYNTHETIC: WON 0x775cf0
// SYNTHETIC: WON 0x775d00
// SYNTHETIC: WON 0x775d10
// SYNTHETIC: WON 0x775d20
// SYNTHETIC: WON 0x775d30
// SYNTHETIC: WON 0x775d40
// SYNTHETIC: WON 0x775d50
// SYNTHETIC: WON 0x775d60
// SYNTHETIC: WON 0x775d70
// SYNTHETIC: WON 0x775d80
// SYNTHETIC: WON 0x775d90
// SYNTHETIC: WON 0x775da0
// SYNTHETIC: WON 0x775db0
// SYNTHETIC: WON 0x775dc0
// SYNTHETIC: WON 0x775dd0
// SYNTHETIC: WON 0x775de0
// SYNTHETIC: WON 0x775df0
// SYNTHETIC: WON 0x775e00
// SYNTHETIC: WON 0x775e10
// SYNTHETIC: WON 0x775e20
// SYNTHETIC: WON 0x775e30
// SYNTHETIC: WON 0x775e40
// SYNTHETIC: WON 0x775e50
// SYNTHETIC: WON 0x775e60
// SYNTHETIC: WON 0x775e70
// SYNTHETIC: WON 0x775e80
// SYNTHETIC: WON 0x775e90
// SYNTHETIC: WON 0x775ea0
// SYNTHETIC: WON 0x775eb0
// SYNTHETIC: WON 0x775ec0
// SYNTHETIC: WON 0x775ed0
// SYNTHETIC: WON 0x775ee0
// SYNTHETIC: WON 0x775ef0
// SYNTHETIC: WON 0x775f00
// SYNTHETIC: WON 0x775f10
// SYNTHETIC: WON 0x775f20
// SYNTHETIC: WON 0x775f30
// SYNTHETIC: WON 0x775f40
// SYNTHETIC: WON 0x775f50
// SYNTHETIC: WON 0x775f60
// SYNTHETIC: WON 0x775f70
// SYNTHETIC: WON 0x775f80
// SYNTHETIC: WON 0x775f90
// SYNTHETIC: WON 0x775fa0
// SYNTHETIC: WON 0x775fb0
// SYNTHETIC: WON 0x775fc0
// SYNTHETIC: WON 0x775fd0
// SYNTHETIC: WON 0x775fe0
// SYNTHETIC: WON 0x775ff0
// SYNTHETIC: WON 0x776000
// SYNTHETIC: WON 0x776010
// SYNTHETIC: WON 0x776020
// SYNTHETIC: WON 0x776030
// SYNTHETIC: WON 0x776040
// SYNTHETIC: WON 0x776050
// SYNTHETIC: WON 0x776060
// SYNTHETIC: WON 0x776070
// SYNTHETIC: WON 0x776080
// SYNTHETIC: WON 0x776090
// SYNTHETIC: WON 0x7760a0
// SYNTHETIC: WON 0x7760b0
// SYNTHETIC: WON 0x7760c0
// SYNTHETIC: WON 0x7760d0
// SYNTHETIC: WON 0x7760e0
// SYNTHETIC: WON 0x7760f0
// SYNTHETIC: WON 0x776100
// SYNTHETIC: WON 0x776110
// SYNTHETIC: WON 0x776120
// SYNTHETIC: WON 0x776130
// SYNTHETIC: WON 0x776140
// SYNTHETIC: WON 0x776150
// SYNTHETIC: WON 0x776160
// SYNTHETIC: WON 0x776170
// SYNTHETIC: WON 0x776180
// SYNTHETIC: WON 0x776190
// SYNTHETIC: WON 0x7761a0
// SYNTHETIC: WON 0x7761b0
// SYNTHETIC: WON 0x7761c0
// SYNTHETIC: WON 0x7761d0
// SYNTHETIC: WON 0x7761e0
// SYNTHETIC: WON 0x7761f0
// SYNTHETIC: WON 0x776200
// SYNTHETIC: WON 0x776210
// SYNTHETIC: WON 0x776220
// SYNTHETIC: WON 0x776230
// SYNTHETIC: WON 0x776240
// SYNTHETIC: WON 0x776250
// SYNTHETIC: WON 0x776260
// SYNTHETIC: WON 0x776270
// SYNTHETIC: WON 0x776280
// SYNTHETIC: WON 0x776290
// SYNTHETIC: WON 0x7762a0
// SYNTHETIC: WON 0x7762b0
// SYNTHETIC: WON 0x7762c0
// SYNTHETIC: WON 0x7762d0
// SYNTHETIC: WON 0x7762e0
// SYNTHETIC: WON 0x7762f0
// SYNTHETIC: WON 0x776300
// SYNTHETIC: WON 0x776310
// SYNTHETIC: WON 0x776320
// SYNTHETIC: WON 0x776330
// SYNTHETIC: WON 0x776340
// SYNTHETIC: WON 0x776350
// SYNTHETIC: WON 0x776360
// SYNTHETIC: WON 0x776370
// SYNTHETIC: WON 0x776380
// SYNTHETIC: WON 0x776390
// SYNTHETIC: WON 0x7763a0
// SYNTHETIC: WON 0x7763b0
// SYNTHETIC: WON 0x7763c0
// SYNTHETIC: WON 0x7763d0
// SYNTHETIC: WON 0x7763e0
// SYNTHETIC: WON 0x7763f0
// SYNTHETIC: WON 0x776400
// SYNTHETIC: WON 0x776410
// SYNTHETIC: WON 0x776420
// SYNTHETIC: WON 0x776430
// SYNTHETIC: WON 0x776440
// SYNTHETIC: WON 0x776450
// SYNTHETIC: WON 0x776460
// SYNTHETIC: WON 0x776470
// SYNTHETIC: WON 0x776480
// SYNTHETIC: WON 0x776490
// SYNTHETIC: WON 0x7764a0
// SYNTHETIC: WON 0x7764b0
// SYNTHETIC: WON 0x7764c0
// SYNTHETIC: WON 0x7764d0
// SYNTHETIC: WON 0x7764e0
// SYNTHETIC: WON 0x7764f0
// SYNTHETIC: WON 0x776500
// SYNTHETIC: WON 0x776510
// SYNTHETIC: WON 0x776520
// SYNTHETIC: WON 0x776530
// SYNTHETIC: WON 0x776540
// SYNTHETIC: WON 0x776550
// SYNTHETIC: WON 0x776560
// SYNTHETIC: WON 0x776570
// SYNTHETIC: WON 0x776580
// SYNTHETIC: WON 0x776590
// SYNTHETIC: WON 0x7765a0
// SYNTHETIC: WON 0x7765b0
// SYNTHETIC: WON 0x7765c0
// SYNTHETIC: WON 0x7765d0
// SYNTHETIC: WON 0x7765e0
// SYNTHETIC: WON 0x7765f0
// SYNTHETIC: WON 0x776600
// SYNTHETIC: WON 0x776610
// SYNTHETIC: WON 0x776620
// SYNTHETIC: WON 0x776630
// SYNTHETIC: WON 0x776640
// SYNTHETIC: WON 0x776650
// SYNTHETIC: WON 0x776660
// SYNTHETIC: WON 0x776670
// SYNTHETIC: WON 0x776680
// SYNTHETIC: WON 0x776690
// SYNTHETIC: WON 0x7766a0
// SYNTHETIC: WON 0x7766b0
// SYNTHETIC: WON 0x7766c0
// SYNTHETIC: WON 0x7766d0
// SYNTHETIC: WON 0x7766e0
// SYNTHETIC: WON 0x7766f0
// SYNTHETIC: WON 0x776700
// SYNTHETIC: WON 0x776710
// SYNTHETIC: WON 0x776720
// SYNTHETIC: WON 0x776730
// SYNTHETIC: WON 0x776740
// SYNTHETIC: WON 0x776750
// SYNTHETIC: WON 0x776760
// SYNTHETIC: WON 0x776770
// SYNTHETIC: WON 0x776780
// SYNTHETIC: WON 0x776790
// SYNTHETIC: WON 0x7767a0
// SYNTHETIC: WON 0x7767b0
// SYNTHETIC: WON 0x7767c0
// SYNTHETIC: WON 0x7767d0
// SYNTHETIC: WON 0x7767e0
// SYNTHETIC: WON 0x7767f0
// SYNTHETIC: WON 0x776800
// SYNTHETIC: WON 0x776810
// SYNTHETIC: WON 0x776820
// SYNTHETIC: WON 0x776830
// SYNTHETIC: WON 0x776840
// SYNTHETIC: WON 0x776850
// SYNTHETIC: WON 0x776860
// SYNTHETIC: WON 0x776870
// SYNTHETIC: WON 0x776880
// SYNTHETIC: WON 0x776890
// SYNTHETIC: WON 0x7768a0
// SYNTHETIC: WON 0x7768b0
// SYNTHETIC: WON 0x7768c0
// SYNTHETIC: WON 0x7768d0
// SYNTHETIC: WON 0x7768e0
// SYNTHETIC: WON 0x7768f0
// SYNTHETIC: WON 0x776900
// SYNTHETIC: WON 0x776910
// SYNTHETIC: WON 0x776920
// SYNTHETIC: WON 0x776930
// SYNTHETIC: WON 0x776940
// SYNTHETIC: WON 0x776950
// SYNTHETIC: WON 0x776960
// SYNTHETIC: WON 0x776970
// SYNTHETIC: WON 0x776980
// SYNTHETIC: WON 0x776990
// SYNTHETIC: WON 0x7769a0
// SYNTHETIC: WON 0x7769b0
// SYNTHETIC: WON 0x7769c0
// SYNTHETIC: WON 0x7769d0
// SYNTHETIC: WON 0x7769e0
// SYNTHETIC: WON 0x7769f0
// SYNTHETIC: WON 0x776a00
// SYNTHETIC: WON 0x776a10
// SYNTHETIC: WON 0x776a20
// SYNTHETIC: WON 0x776a30
// SYNTHETIC: WON 0x776a40
// SYNTHETIC: WON 0x776a50
// SYNTHETIC: WON 0x776a60
// SYNTHETIC: WON 0x776a70
// SYNTHETIC: WON 0x776a80
// SYNTHETIC: WON 0x776a90
// SYNTHETIC: WON 0x776aa0
// SYNTHETIC: WON 0x776ab0
// SYNTHETIC: WON 0x776ac0
// SYNTHETIC: WON 0x776ad0
// SYNTHETIC: WON 0x776ae0
// SYNTHETIC: WON 0x776af0
// SYNTHETIC: WON 0x776b00
// SYNTHETIC: WON 0x776b10
// SYNTHETIC: WON 0x776b20
// SYNTHETIC: WON 0x776b30
// SYNTHETIC: WON 0x776b40
// SYNTHETIC: WON 0x776b50
// SYNTHETIC: WON 0x776b60
// SYNTHETIC: WON 0x776b70
// SYNTHETIC: WON 0x776b80
// SYNTHETIC: WON 0x776b90
// SYNTHETIC: WON 0x776ba0
// SYNTHETIC: WON 0x776bb0
// SYNTHETIC: WON 0x776bc0
// SYNTHETIC: WON 0x776bd0
// SYNTHETIC: WON 0x776be0
// SYNTHETIC: WON 0x776bf0
// SYNTHETIC: WON 0x776c00
// SYNTHETIC: WON 0x776c10
// SYNTHETIC: WON 0x776c20
// SYNTHETIC: WON 0x776c30
// SYNTHETIC: WON 0x776c40
// SYNTHETIC: WON 0x776c50
// SYNTHETIC: WON 0x776c60
// SYNTHETIC: WON 0x776c70
// SYNTHETIC: WON 0x776c80
// SYNTHETIC: WON 0x776c90
// SYNTHETIC: WON 0x776ca0
// SYNTHETIC: WON 0x776cb0
// SYNTHETIC: WON 0x776cc0
// SYNTHETIC: WON 0x776cd0
// SYNTHETIC: WON 0x776ce0
// SYNTHETIC: WON 0x776cf0
// SYNTHETIC: WON 0x776d00
// SYNTHETIC: WON 0x776d10
// SYNTHETIC: WON 0x776d20
// SYNTHETIC: WON 0x776d30
// SYNTHETIC: WON 0x776d40
// SYNTHETIC: WON 0x776d50
// SYNTHETIC: WON 0x776d60
// SYNTHETIC: WON 0x776d70
// SYNTHETIC: WON 0x776d80
// SYNTHETIC: WON 0x776d90
// SYNTHETIC: WON 0x776da0
// SYNTHETIC: WON 0x776db0
// SYNTHETIC: WON 0x776dc0
// SYNTHETIC: WON 0x776dd0
// SYNTHETIC: WON 0x776de0
// SYNTHETIC: WON 0x776df0
// SYNTHETIC: WON 0x776e00
// SYNTHETIC: WON 0x776e10
// SYNTHETIC: WON 0x776e20
// SYNTHETIC: WON 0x776e30
// SYNTHETIC: WON 0x776e40
// SYNTHETIC: WON 0x776e50
// SYNTHETIC: WON 0x776e60
// SYNTHETIC: WON 0x776e70
// SYNTHETIC: WON 0x776e80
// SYNTHETIC: WON 0x776e90
// SYNTHETIC: WON 0x776ea0
// SYNTHETIC: WON 0x776eb0
// SYNTHETIC: WON 0x776ec0
// SYNTHETIC: WON 0x776ed0
// SYNTHETIC: WON 0x776ee0
// SYNTHETIC: WON 0x776ef0
// SYNTHETIC: WON 0x776f00
// SYNTHETIC: WON 0x776f10
// SYNTHETIC: WON 0x776f20
// SYNTHETIC: WON 0x776f30
// SYNTHETIC: WON 0x776f40
// SYNTHETIC: WON 0x776f50
// SYNTHETIC: WON 0x776f60
// SYNTHETIC: WON 0x776f70
// SYNTHETIC: WON 0x776f80
// SYNTHETIC: WON 0x776f90
// SYNTHETIC: WON 0x776fa0
// SYNTHETIC: WON 0x776fb0
// SYNTHETIC: WON 0x776fc0
// SYNTHETIC: WON 0x776fd0
// SYNTHETIC: WON 0x776fe0
// SYNTHETIC: WON 0x776ff0
// SYNTHETIC: WON 0x777000
// SYNTHETIC: WON 0x777010
// SYNTHETIC: WON 0x777020
// SYNTHETIC: WON 0x777030
// SYNTHETIC: WON 0x777040
// SYNTHETIC: WON 0x777050
// SYNTHETIC: WON 0x777060
// SYNTHETIC: WON 0x777070
// SYNTHETIC: WON 0x777080
// SYNTHETIC: WON 0x777090
// SYNTHETIC: WON 0x7770a0
// SYNTHETIC: WON 0x7770b0
// SYNTHETIC: WON 0x7770c0
// SYNTHETIC: WON 0x7770d0
// SYNTHETIC: WON 0x7770e0
// SYNTHETIC: WON 0x7770f0
// SYNTHETIC: WON 0x777100
// SYNTHETIC: WON 0x777110
// SYNTHETIC: WON 0x777120
// SYNTHETIC: WON 0x777130
// SYNTHETIC: WON 0x777140
// SYNTHETIC: WON 0x777150
// SYNTHETIC: WON 0x777160
// SYNTHETIC: WON 0x777170
// SYNTHETIC: WON 0x777180
// SYNTHETIC: WON 0x777190
// SYNTHETIC: WON 0x7771a0
// SYNTHETIC: WON 0x7771b0
// SYNTHETIC: WON 0x7771c0
// SYNTHETIC: WON 0x7771d0
// SYNTHETIC: WON 0x7771e0
// SYNTHETIC: WON 0x7771f0
// SYNTHETIC: WON 0x777200
// SYNTHETIC: WON 0x777210
// SYNTHETIC: WON 0x777220
// SYNTHETIC: WON 0x777230
// SYNTHETIC: WON 0x777240
// SYNTHETIC: WON 0x777250
// SYNTHETIC: WON 0x777260
// SYNTHETIC: WON 0x777270
// SYNTHETIC: WON 0x777280
// SYNTHETIC: WON 0x777290
// SYNTHETIC: WON 0x7772a0
// SYNTHETIC: WON 0x7772b0
// SYNTHETIC: WON 0x7772c0
// SYNTHETIC: WON 0x7772d0
// SYNTHETIC: WON 0x7772e0
// SYNTHETIC: WON 0x7772f0
// SYNTHETIC: WON 0x777300
// SYNTHETIC: WON 0x777310
// SYNTHETIC: WON 0x777320
// SYNTHETIC: WON 0x777330
// SYNTHETIC: WON 0x777340
// SYNTHETIC: WON 0x777350
// SYNTHETIC: WON 0x777360
// SYNTHETIC: WON 0x777370
// SYNTHETIC: WON 0x777380
// SYNTHETIC: WON 0x777390
// SYNTHETIC: WON 0x7773a0
// SYNTHETIC: WON 0x7773b0
// SYNTHETIC: WON 0x7773c0
// SYNTHETIC: WON 0x7773d0
// SYNTHETIC: WON 0x7773e0
// SYNTHETIC: WON 0x7773f0
// SYNTHETIC: WON 0x777400
// SYNTHETIC: WON 0x777410
// SYNTHETIC: WON 0x777420
// SYNTHETIC: WON 0x777430
// SYNTHETIC: WON 0x777440
// SYNTHETIC: WON 0x777450
// SYNTHETIC: WON 0x777460
// SYNTHETIC: WON 0x777470
// SYNTHETIC: WON 0x777480
// SYNTHETIC: WON 0x777490
// SYNTHETIC: WON 0x7774a0
// SYNTHETIC: WON 0x7774b0
// SYNTHETIC: WON 0x7774c0
// SYNTHETIC: WON 0x7774d0
// SYNTHETIC: WON 0x7774e0
// SYNTHETIC: WON 0x7774f0
// SYNTHETIC: WON 0x777500
// SYNTHETIC: WON 0x777510
// SYNTHETIC: WON 0x777520
// SYNTHETIC: WON 0x777530
// SYNTHETIC: WON 0x777540
// SYNTHETIC: WON 0x777550
// SYNTHETIC: WON 0x777560
// SYNTHETIC: WON 0x777570
// SYNTHETIC: WON 0x777580
// SYNTHETIC: WON 0x777590
// SYNTHETIC: WON 0x7775a0
// SYNTHETIC: WON 0x7775b0
// SYNTHETIC: WON 0x7775c0
// SYNTHETIC: WON 0x7775d0
// SYNTHETIC: WON 0x7775e0
// SYNTHETIC: WON 0x7775f0
// SYNTHETIC: WON 0x777600
// SYNTHETIC: WON 0x777610
// SYNTHETIC: WON 0x777620
// SYNTHETIC: WON 0x777630
// SYNTHETIC: WON 0x777640
// SYNTHETIC: WON 0x777650
// SYNTHETIC: WON 0x777660

// FUNCTION: WON 0x00777670
// Registers the lobby's resource loader and control ids on the given (or a new) resource table and returns it
ResourceConfigTablePtr LobbyResource_Init(ResourceConfigTable* theTable)
{
    ResourceConfigTablePtr aTable=theTable;
    if(theTable==NULL)
        aTable=theTable=new ResourceConfigTable;
    theTable->RegisterGlueResourceFunc(LobbyResource_SafeGlueResources);
    theTable->RegisterControlId("ShowCreateAccount",ID_ShowCreateAccount);
    theTable->RegisterControlId("ShowLostPassword",ID_ShowLostPassword);
    theTable->RegisterControlId("ShowLostUserName",ID_ShowLostUserName);
    theTable->RegisterControlId("ShowMOTD",ID_ShowMOTD);
    theTable->RegisterControlId("ShowTOU",ID_ShowTOU);
    theTable->RegisterControlId("LoginScreenBack",ID_LoginScreenBack);
    theTable->RegisterControlId("ShowLoginHelp",ID_ShowLoginHelp);
    theTable->RegisterControlId("ShowInternetScreen",ID_ShowInternetScreen);
    theTable->RegisterControlId("ShowLanScreen",ID_ShowLanScreen);
    theTable->RegisterControlId("Logout",ID_Logout);
    theTable->RegisterControlId("HostGame",ID_HostGame);
    theTable->RegisterControlId("ChangeRoom",ID_ChangeRoom);
    theTable->RegisterControlId("JoinGame",ID_JoinGame);
    theTable->RegisterControlId("LeaveGame",ID_LeaveGame);
    theTable->RegisterControlId("StartGame",ID_StartGame);
    theTable->RegisterControlId("RefreshGameList",ID_RefreshGameList);
    theTable->RegisterControlId("AddFriend",ID_AddFriend);
    theTable->RegisterControlId("RefreshFriends",ID_RefreshFriends);
    theTable->RegisterControlId("AddLANName",ID_AddLANName);
    theTable->RegisterControlId("ChangeLANBroadcast",ID_ChangeLANBroadcast);
    theTable->RegisterControlId("FilterDirtyWordsCheck",ID_FilterDirtyWordsCheck);
    theTable->RegisterControlId("AnonymousToFriendsCheck",ID_AnonymousToFriendsCheck);
    theTable->RegisterControlId("ChooseNetworkAdapter",ID_ChooseNetworkAdapter);
    return aTable;
}

static void* gResources[] = {
    &LobbyGlobal_Cancel_String,
    &LobbyGlobal_Lan_String,
    &LobbyGlobal_No_String,
    &LobbyGlobal_OK_String,
    &LobbyGlobal_Yes_String,
    &LobbyGlobal_QuickPlay_String,
    &LobbyGlobal_Searching_Image,
    &LobbyGlobal_Firewall_Image,
    &LobbyGlobal_ButtonClick_Sound,
    &LobbyGlobal_LobbyMusic_Sound,
    &LobbyGlobal_AllReady_Sound,
    &LobbyGlobal_AllNotReady_Sound,
    &LobbyGlobal_AskToJoinRecv_Sound,
    &LobbyGlobal_Error_Sound,
    &LobbyGlobal_FriendJoined_Sound,
    &LobbyGlobal_FriendLeft_Sound,
    &LobbyGlobal_InviteRecv_Sound,
    &LobbyGlobal_PlayerJoined_Sound,
    &LobbyGlobal_SierraLogon_Sound,
    &LobbyGlobal_WhisperRecv_Sound,
    &AccountLogic_InvalidCDKeyTitle_String,
    &AccountLogic_InvalidCDKeyDetails_String,
    &AddFriendDialog_StatusTitle_String,
    &AddFriendDialog_StatusDetails_String,
    &AdminActionDialog_Warn_String,
    &AdminActionDialog_Mute_String,
    &AdminActionDialog_UnMute_String,
    &AdminActionDialog_Kick_String,
    &AdminActionDialog_Ban_String,
    &AdminActionDialog_ExplanationOpt_String,
    &AdminActionDialog_ExplanationReq_String,
    &BadUserDialog_OneDay_String,
    &BadUserDialog_OneHour_String,
    &BadUserDialog_OneMinute_String,
    &BadUserDialog_OneSecond_String,
    &BadUserDialog_XDays_String,
    &BadUserDialog_XHours_String,
    &BadUserDialog_XMinutes_String,
    &BadUserDialog_XSeconds_String,
    &BadUserDialog_Infinite_String,
    &ChatCommandLogic_Whisper_String,
    &ChatCommandLogic_Block_String,
    &ChatCommandLogic_Emote_String,
    &ChatCommandLogic_Ignore_String,
    &ChatCommandLogic_Away_String,
    &ChatCommandLogic_Invite_String,
    &ChatCommandLogic_Uninvite_String,
    &ChatCommandLogic_Reply_String,
    &ChatCommandLogic_Clear_String,
    &ChatCommandLogic_Help_String,
    &ChatCommandLogic_ShowTeam_String,
    &ChatCommandLogic_Moderator_String,
    &ChatCommandLogic_ServerMute_String,
    &ChatCommandLogic_ServerBan_String,
    &ChatCommandLogic_Mute_String,
    &ChatCommandLogic_Ban_String,
    &ChatCommandLogic_Unmute_String,
    &ChatCommandLogic_ServerUnmute_String,
    &ChatCommandLogic_Unban_String,
    &ChatCommandLogic_ServerUnban_String,
    &ChatCommandLogic_Alert_String,
    &ChatCommandLogic_Warn_String,
    &ChatCommandLogic_HelpHeader_String,
    &ChatCommandLogic_HelpHelp_String,
    &ChatCommandLogic_ShowTeamHelp_String,
    &ChatCommandLogic_WhisperHelp_String,
    &ChatCommandLogic_BlockHelp_String,
    &ChatCommandLogic_IgnoreHelp_String,
    &ChatCommandLogic_ReplyHelp_String,
    &ChatCommandLogic_EmoteHelp_String,
    &ChatCommandLogic_AwayHelp_String,
    &ChatCommandLogic_ClearHelp_String,
    &ChatCommandLogic_InviteHelp_String,
    &ChatCommandLogic_UninviteHelp_String,
    &ChatCommandLogic_MuteHelp_String,
    &ChatCommandLogic_UnmuteHelp_String,
    &ChatCommandLogic_BanHelp_String,
    &ChatCommandLogic_UnbanHelp_String,
    &ChatCommandLogic_ModeratorHelp_String,
    &ChatCommandLogic_ServerMuteHelp_String,
    &ChatCommandLogic_ServerUnmuteHelp_String,
    &ChatCommandLogic_ServerBanHelp_String,
    &ChatCommandLogic_ServerUnbanHelp_String,
    &ChatCommandLogic_AlertHelp_String,
    &ChatCommandLogic_WarnHelp_String,
    &ChatCtrl_NormalChat_String,
    &ChatCtrl_Emote_String,
    &ChatCtrl_TeamChat_String,
    &ChatCtrl_TeamEmote_String,
    &ChatCtrl_YouWhisperToYourself_String,
    &ChatCtrl_YouWhisper_String,
    &ChatCtrl_SomeoneWhispersToYou_String,
    &ChatCtrl_YouWarnSomeone_String,
    &ChatCtrl_YouWarnYourself_String,
    &ChatCtrl_SomeoneWarnsYou_String,
    &ChatCtrl_SomeoneWarnsSomeone_String,
    &ChatCtrl_EnteringRoom_String,
    &ChatCtrl_LeavingRoom_String,
    &ChatCtrl_Disconnected_String,
    &ChatCtrl_YouWereKicked_String,
    &ChatCtrl_YouWereBannedMinute_String,
    &ChatCtrl_YouWereBannedMinutes_String,
    &ChatCtrl_YouWereBannedHour_String,
    &ChatCtrl_YouWereBannedHours_String,
    &ChatCtrl_YouWereBannedDay_String,
    &ChatCtrl_YouWereBannedDays_String,
    &ChatCtrl_YouWereBannedIndefinitely_String,
    &ChatCtrl_SomeoneWasKicked_String,
    &ChatCtrl_SomeoneWasBannedMinute_String,
    &ChatCtrl_SomeoneWasBannedMinutes_String,
    &ChatCtrl_SomeoneWasBannedHour_String,
    &ChatCtrl_SomeoneWasBannedHours_String,
    &ChatCtrl_SomeoneWasBannedDay_String,
    &ChatCtrl_SomeoneWasBannedDays_String,
    &ChatCtrl_SomeoneWasBannedIndefinitely_String,
    &ChatCtrl_YouWereUnmuted_String,
    &ChatCtrl_YouWereMutedMinute_String,
    &ChatCtrl_YouWereMutedMinutes_String,
    &ChatCtrl_YouWereMutedHour_String,
    &ChatCtrl_YouWereMutedHours_String,
    &ChatCtrl_YouWereMutedDay_String,
    &ChatCtrl_YouWereMutedDays_String,
    &ChatCtrl_YouWereMutedIndefinitely_String,
    &ChatCtrl_SomeoneWasUnmuted_String,
    &ChatCtrl_SomeoneWasMutedMinute_String,
    &ChatCtrl_SomeoneWasMutedMinutes_String,
    &ChatCtrl_SomeoneWasMutedHour_String,
    &ChatCtrl_SomeoneWasMutedHours_String,
    &ChatCtrl_SomeoneWasMutedDay_String,
    &ChatCtrl_SomeoneWasMutedDays_String,
    &ChatCtrl_SomeoneWasMutedIndefinitely_String,
    &ChatCtrl_ModeratorComment_String,
    &ChatCtrl_AlertComment_String,
    &ChatCtrl_ServerAlert_String,
    &ChatCtrl_ServerShutdownStartedMinute_String,
    &ChatCtrl_ServerShutdownStartedMinutes_String,
    &ChatCtrl_ServerShutdownStartedHour_String,
    &ChatCtrl_ServerShutdownStartedHours_String,
    &ChatCtrl_ServerShutdownStartedDay_String,
    &ChatCtrl_ServerShutdownStartedDays_String,
    &ChatCtrl_ServerShutdownAborted_String,
    &ChatCtrl_SomeoneInvitesYou_String,
    &ChatCtrl_SomeoneUninvitesYou_String,
    &ChatCtrl_YouInviteSomeone_String,
    &ChatCtrl_YouUninviteSomeone_String,
    &ChatCtrl_SomeoneInvitesYouExpl_String,
    &ChatCtrl_SomeoneUninvitesYouExpl_String,
    &ChatCtrl_YouInviteSomeoneExpl_String,
    &ChatCtrl_YouUninviteSomeoneExpl_String,
    &ChatCtrl_Accept_String,
    &ChatCtrl_Reject_String,
    &ChatCtrl_SomeoneAsksToJoin_String,
    &ChatCtrl_SomeoneAsksToJoinExpl_String,
    &ChatCtrl_ClientNotOnServer_String,
    &ChatCtrl_ClientInGame_String,
    &ChatCtrl_ClientInChat_String,
    &ChatCtrl_ClientInGameAndChat_String,
    &ChatCtrl_ClientIsAnonymous_String,
    &ChatCtrl_FriendIsChatting_String,
    &ChatCtrl_FriendIsInGameStaging_String,
    &ChatCtrl_FriendIsPlaying_String,
    &ChatCtrl_FriendIsOn_String,
    &ChatCtrl_FriendIsNotOnline_String,
    &ChatCtrl_PlayerKicked_String,
    &ChatCtrl_PlayerBanned_String,
    &ChatCtrl_FriendJoined_String,
    &ChatCtrl_FriendLeft_String,
    &ChatCtrl_TeamMemberJoined_String,
    &ChatCtrl_TeamMemberLeft_String,
    &ChatCtrl_TeamMembers_String,
    &ChatCtrl_IgnoreList_String,
    &ChooseNetworkAdapterDialog_Default_String,
    &ChooseNetworkAdapterDialog_Unknown_String,
    &ClientOptionsCtrl_Whisper_String,
    &ClientOptionsCtrl_Invite_String,
    &ClientOptionsCtrl_Uninvite_String,
    &ClientOptionsCtrl_AddToFriends_String,
    &ClientOptionsCtrl_RemoveFromFriends_String,
    &ClientOptionsCtrl_ClearAFK_String,
    &ClientOptionsCtrl_SetAFK_String,
    &ClientOptionsCtrl_Ignore_String,
    &ClientOptionsCtrl_Unignore_String,
    &ClientOptionsCtrl_Block_String,
    &ClientOptionsCtrl_Unblock_String,
    &ClientOptionsCtrl_Warn_String,
    &ClientOptionsCtrl_Mute_String,
    &ClientOptionsCtrl_UnMute_String,
    &ClientOptionsCtrl_Kick_String,
    &ClientOptionsCtrl_Ban_String,
    &DirectConnectionOptionsCtrl_Connect_String,
    &DirectConnectionOptionsCtrl_Refresh_String,
    &DirectConnectionOptionsCtrl_Remove_String,
    &FriendOptionsCtrl_Locate_String,
    &FriendOptionsCtrl_RefreshList_String,
    &FriendOptionsCtrl_RemoveFromFriends_String,
    &FriendsListCtrl_Unknown_Image,
    &FriendsListCtrl_NotFound_Image,
    &FriendsListCtrl_InChat_Image,
    &FriendsListCtrl_InGame_Image,
    &GameOptionsPopup_Join_String,
    &GameOptionsPopup_Ping_String,
    &GameOptionsPopup_QueryDetails_String,
    &GameOptionsPopup_HideDetails_String,
    &GameOptionsPopup_ShowDetails_String,
    &GameOptionsPopup_Remove_String,
    &GameStagingCtrl_Ready_String,
    &GameStagingCtrl_NotReady_String,
    &GameStagingCtrl_StartGame_String,
    &GameStagingCtrl_LeaveGame_String,
    &GameStagingCtrl_DissolveGame_String,
    &GameStagingLogic_KickedTitle_String,
    &GameStagingLogic_KickedDetails_String,
    &GameStagingLogic_BannedTitle_String,
    &GameStagingLogic_BannedDetails_String,
    &GameStagingLogic_ConnectWarningTitle1_String,
    &GameStagingLogic_ConnectWarningDetails1_String,
    &GameStagingLogic_ConnectWarningTitle2_String,
    &GameStagingLogic_ConnectWarningDetails2_String,
    &GameStagingLogic_ConnectWarningTitle3_String,
    &GameStagingLogic_ConnectWarningDetails3_String,
    &GameStagingLogic_NoPingReplyError_String,
    &GameStagingLogic_GettingGameInfoTitle_String,
    &GameStagingLogic_GettingGameInfoDetails_String,
    &GameStagingLogic_JoinGameErrorTitle_String,
    &GameStagingLogic_VerifyingPingTitle_String,
    &GameStagingLogic_VerifyingPingDetails_String,
    &GameStagingLogic_DissolveGame_String,
    &GameStagingLogic_DissolveGameConfirm_String,
    &GameStagingLogic_LeaveGame_String,
    &GameStagingLogic_LeaveGameConfirm_String,
    &GameStagingLogic_NoQuickPlayGameTitle_String,
    &GameStagingLogic_NoQuickPlayGameDetails_String,
    &GameUserListCtrl_Ban_String,
    &GameUserListCtrl_Kick_String,
    &GameUserListCtrl_Ready_Image,
    &InternetQuickPlay_Welcome_String,
    &InternetQuickPlay_UserWelcome_String,
    &JoinResponse_Joined_String,
    &JoinResponse_ServerFull_String,
    &JoinResponse_InviteOnly_String,
    &JoinResponse_AskToJoin_String,
    &JoinResponse_GameInProgress_String,
    &JoinResponse_InvalidPassword_String,
    &JoinResponse_NotOnLAN_String,
    &JoinResponse_Blocked_String,
    &JoinResponse_Unknown_String,
    &LANLogic_SocketBindError_String,
    &LANLogic_SocketListenError_String,
    &LANLogic_ConnectToGameTitle_String,
    &LANLogic_ConnectToGameDetails_String,
    &LANLogic_CheckingAddressTitle_String,
    &LANLogic_CheckingAddressDetails_String,
    &LANLogic_RegisterTitle_String,
    &LANLogic_RegisterDetails_String,
    &LANLogic_JoinFailure_String,
    &LANLogic_JoinReplyError_String,
    &LANLogic_InvalidSubnet_String,
    &LobbyContainer_InitStart_String,
    &LobbyContainer_InitFailed_String,
    &LobbyContainer_CheckingVersion_String,
    &LobbyContainer_QueryingServers_String,
    &LobbyContainer_NoConnection_String,
    &LobbyContainer_DNSLookupFailed_String,
    &LobbyContainer_DirLookupFailure_String,
    &LobbyContainer_VersionCheckFailure_String,
    &LobbyContainer_InitComplete_String,
    &LobbyContainer_NoServers_String,
    &LobbyContainer_AuthServers_String,
    &LobbyContainer_AccountServers_String,
    &LobbyContainer_VersionServers_String,
    &LobbyContainer_CreateAccountTitle_String,
    &LobbyContainer_CreateAccountDetails_String,
    &LobbyContainer_RetrievePasswordTitle_String,
    &LobbyContainer_RetrievePasswordDetails_String,
    &LobbyContainer_RetrieveUsernameTitle_String,
    &LobbyContainer_RetrieveUsernameDetails_String,
    &LobbyContainer_QueryAccountTitle_String,
    &LobbyContainer_QueryAccountDetails_String,
    &LobbyContainer_UpdateAccountTitle_String,
    &LobbyContainer_UpdateAccountDetails_String,
    &LobbyContainer_LoginTitle_String,
    &LobbyContainer_LoginDetails_String,
    &LobbyContainer_GettingRoomsTitle_String,
    &LobbyContainer_GettingRoomsDetails_String,
    &LobbyContainer_VersionOutOfDataTitle_String,
    &LobbyContainer_VersionOutOfDataDetails_String,
    &LobbyGameStatus_Success_String,
    &LobbyGameStatus_GameFull_String,
    &LobbyGameStatus_GameInProgress_String,
    &LobbyGameStatus_UnpackFailure_String,
    &LobbyGameStatus_NotInvited_String,
    &LobbyGameStatus_CaptainRejectedYou_String,
    &LobbyGameStatus_InvalidPassword_String,
    &LobbyGameStatus_DuplicateName_String,
    &LobbyOptions_DefaultName_String,
    &LobbyOptions_CheckingAddress_String,
    &LobbyOptions_AddressUnavailable_String,
    &RoomCtrl_QueryingServer_String,
    &RoomCtrl_Password_Image,
    &RoomLabelCtrl_LabelPrefix_String,
    &RoomLabelCtrl_LabelServer_String,
    &RoomLabelCtrl_LabelUser_String,
    &RoomLabelCtrl_LabelUsers_String,
    &RoomLabelCtrl_LabelDisconnected_String,
    &RoutingLogic_GetClientListTitle_String,
    &RoutingLogic_GetClientListDetails_String,
    &RoutingLogic_GetGroupListTitle_String,
    &RoutingLogic_GetGroupListDetails_String,
    &RoutingLogic_GameDissolvedTitle_String,
    &RoutingLogic_GameDissolvedDetails_String,
    &RoutingLogic_NoServersTitle_String,
    &RoutingLogic_NoServersDetails_String,
    &RoutingLogic_TooManyFriendsTitle_String,
    &RoutingLogic_TooManyFriendsDetails_String,
    &RoutingLogic_AlreadyJoinedTitle_String,
    &RoutingLogic_AlreadyInGame_String,
    &RoutingLogic_AlreadyInRoom_String,
    &RoutingLogic_JoinGroupTitle_String,
    &RoutingLogic_JoinGroupDetails_String,
    &RoutingLogic_ConnectTitle_String,
    &RoutingLogic_ConnectDetails_String,
    &RoutingLogic_RegisterTitle_String,
    &RoutingLogic_RegisterDetails_String,
    &RoutingLogic_QueryGroupsTitle_String,
    &RoutingLogic_QueryGroupsDetails_String,
    &RoutingLogic_CreateGroupTitle_String,
    &RoutingLogic_CreateGroupDetails_String,
    &RoutingLogic_GetBadUsersTitle_String,
    &RoutingLogic_GetBadUsersDetails_String,
    &RoutingLogic_DisconnectedTitle_String,
    &RoutingLogic_DisconnectedDetails_String,
    &RoutingLogic_AskingCaptainTitle_String,
    &RoutingLogic_AskingCaptainDetails_String,
    &RoutingLogic_CollectingPingsTitle_String,
    &RoutingLogic_CollectingPingsDetails_String,
    &RoutingLogic_GettingGameListTitle_String,
    &RoutingLogic_GettingGameListDetails_String,
    &RoutingLogic_MuteClientTitle_String,
    &RoutingLogic_UnmuteClientTitle_String,
    &RoutingLogic_BanClientTitle_String,
    &RoutingLogic_KickClientTitle_String,
    &RoutingLogic_ModerateDetails_String,
    &RoutingLogic_FriendNotFoundTitle_String,
    &RoutingLogic_FriendNotFoundDetails_String,
    &RoutingLogic_ClientNotFound_String,
    &RoutingLogic_ClientAmbiguous_String,
    &RoutingLogic_InvalidCommand_String,
    &RoutingLogic_RequireName_String,
    &RoutingLogic_BadTimeSpec_String,
    &RoutingLogic_Error_String,
    &RoutingLogic_NotCaptain_String,
    &ServerListCtrl_Pinging_String,
    &ServerListCtrl_PingFailed_String,
    &ServerListCtrl_Querying_String,
    &ServerListCtrl_QueryFailed_String,
    &ServerListCtrl_ListSummary_String,
    &ServerListCtrl_ListPingSummary_String,
    &ServerListCtrl_RefreshList_String,
    &ServerListCtrl_StopRefresh_String,
    &ServerListCtrl_NoFilter_String,
    &ServerListCtrl_AddRemove_String,
    &ServerListCtrl_Add_String,
    &ServerListCtrl_Remove_String,
    &ServerListCtrl_FullFilter_String,
    &ServerListCtrl_NotFullFilter_String,
    &ServerListCtrl_OpenFilter_String,
    &ServerListCtrl_PingFailed_Image,
    &ServerListCtrl_GameExpanded_Image,
    &ServerListCtrl_GameNotExpanded_Image,
    &ServerListCtrl_GameAreInvited_Image,
    &ServerListCtrl_GameNone_Image,
    &ServerListCtrl_GameAskToJoin_Image,
    &ServerListCtrl_GameHasPassword_Image,
    &ServerListCtrl_GameNotInvited_Image,
    &ServerListCtrl_GameInProgress_Image,
    &ServerListCtrl_SkillOpen_Image,
    &ServerListCtrl_SkillNovice_Image,
    &ServerListCtrl_SkillIntermediate_Image,
    &ServerListCtrl_SkillAdvanced_Image,
    &ServerListCtrl_SkillExpert_Image,
    &ServerOptionsCtrl_Connect_String,
    &ServerOptionsCtrl_RefreshServer_String,
    &ServerOptionsCtrl_HideDetails_String,
    &ServerOptionsCtrl_ShowDetails_String,
    &ServerOptionsCtrl_ResfreshList_String,
    &StagingLogic_DisconnectedTitle_String,
    &StagingLogic_DisconnectedDetails_String,
    &StagingLogic_BannedTitle_String,
    &StagingLogic_BannedDetails_String,
    &StagingLogic_KickedTitle_String,
    &StagingLogic_KickedDetails_String,
    &UserListCtrl_Ignore_String,
    &UserListCtrl_Unignore_String,
    &UserListCtrl_Normal_Image,
    &UserListCtrl_Away_Image,
    &UserListCtrl_Admin_Image,
    &UserListCtrl_Moderator_Image,
    &UserListCtrl_Captain_Image,
    &UserListCtrl_Muted_Image,
    &UserListCtrl_Ignored_Image,
    &UserListCtrl_Blocked_Image,
    &WONStatus_Success_String,
    &WONStatus_Killed_String,
    &WONStatus_TimedOut_String,
    &WONStatus_InvalidAddress_String,
    &WONStatus_GeneralFailure_String,
    &WONStatus_AuthFailure_String,
    &WONStatus_FTPFailure_String,
    &WONStatus_HTTPFailure_String,
    &WONStatus_MOTDFailure_String,
    &WONStatus_RoutingFailure_String,
    &WONStatus_GameSpyFailure_String,
    &WONStatus_DirFailure_String,
    &WONStatus_ChatFailure_String,
    &WONStatus_DatabaseFailure_String,
    &WONStatus_NotAllowed_String,
    &WONStatus_NotAuthorized_String,
    &WONStatus_BehindFirewall_String,
    &WONStatus_ExpiredCertificate_String,
    &WONStatus_InvalidCDKey_String,
    &WONStatus_CDKeyBanned_String,
    &WONStatus_CDKeyInUse_String,
    &WONStatus_CRCFailed_String,
    &WONStatus_UserExists_String,
    &WONStatus_UserNotFound_String,
    &WONStatus_BadPassword_String,
    &WONStatus_InvalidUserName_String,
    &WONStatus_BadCommunity_String,
    &WONStatus_NotInCommunity_String,
    &WONStatus_UserSeqInUse_String,
    &WONStatus_BlankNamesNotAllowed_String,
    &WONStatus_CaptainRejectedYou_String,
    &WONStatus_ClientAlreadyBanned_String,
    &WONStatus_ClientAlreadyInGroup_String,
    &WONStatus_ClientAlreadyMuted_String,
    &WONStatus_ClientBanned_String,
    &WONStatus_ClientDoesNotExist_String,
    &WONStatus_ClientNotBanned_String,
    &WONStatus_ClientNotInGroup_String,
    &WONStatus_ClientNotInvited_String,
    &WONStatus_ClientNotMuted_String,
    &WONStatus_ConnectFailure_String,
    &WONStatus_GroupAlreadyClaimed_String,
    &WONStatus_GroupAlreadyExists_String,
    &WONStatus_GroupClosed_String,
    &WONStatus_GroupDeleted_String,
    &WONStatus_GroupDoesNotExist_String,
    &WONStatus_GroupFull_String,
    &WONStatus_GuestNamesReserved_String,
    &WONStatus_InvalidPassword_String,
    &WONStatus_MustBeAdmin_String,
    &WONStatus_MustBeCaptain_String,
    &WONStatus_MustBeCaptainOrModerator_String,
    &WONStatus_MustBeModerator_String,
    &WONStatus_ServerFull_String,
    &WONStatus_GroupNameExceedsMaximum_String,
    &WONStatus_JoinCanceled_String,
    &WONStatus_Throttled_String,
    &WONStatus_KeyNotUsed_String,
    &WONStatus_EMailPasswordDuplication_String,
    &WONStatus_AccountCreateDisabled_String,
    &WONStatus_AgeNotOldEnough_String,
    &WONStatus_ItemHasDependencies_String,
    &WONStatus_OutOfDate_String,
    &WONStatus_OutOfDateNoUpdate_String,
    &WONStatus_ValidNotLatest_String,
    &WONStatus_DataAlreadyExists_String,
    &WONStatus_DBError_String,
    &WONStatus_ConnRefused_String,
    NULL
};

// The string and background getters fold to one body, the pointer and value getters to another.
// FUNCTION: WON 0x00777f40
// Returns the lobby string resource with the given id
const GUIString& LobbyResource_GetString(LobbyResourceId id) { return *(GUIString*)gResources[id]; }
// FUNCTION: WON 0x00777f50
// Returns the lobby image resource with the given id (also serves sounds, fonts, colours and ints)
Image* LobbyResource_GetImage(LobbyResourceId id) { return ((SmartPtr<Image>*)gResources[id])->get(); }
Sound* LobbyResource_GetSound(LobbyResourceId id) { return ((SmartPtr<Sound>*)gResources[id])->get(); }
Font* LobbyResource_GetFont(LobbyResourceId id) { return ((SmartPtr<Font>*)gResources[id])->get(); }
const Background& LobbyResource_GetBackground(LobbyResourceId id) { return *(Background*)gResources[id]; }
DWORD LobbyResource_GetColor(LobbyResourceId id) { return *(DWORD*)gResources[id]; }
int LobbyResource_GetInt(LobbyResourceId id) { return *(int*)gResources[id]; }

// FUNCTION: WON 0x00777f60
// Loads every lobby string, image and sound from its section of the resource table
void LobbyResource_SafeGlueResources(ResourceConfigTable* theTable)
{
    ResourceConfig* aConfig;
    aConfig=theTable->SafeGetConfig("LobbyGlobal");
    LobbyGlobal_Cancel_String=aConfig->SafeGetString("Cancel");
    LobbyGlobal_Lan_String=aConfig->SafeGetString("Lan");
    LobbyGlobal_No_String=aConfig->SafeGetString("No");
    LobbyGlobal_OK_String=aConfig->SafeGetString("OK");
    LobbyGlobal_Yes_String=aConfig->SafeGetString("Yes");
    LobbyGlobal_QuickPlay_String=aConfig->SafeGetString("QuickPlay");
    LobbyGlobal_Searching_Image=aConfig->SafeGetImage("Searching");
    LobbyGlobal_Firewall_Image=aConfig->SafeGetImage("Firewall");
    LobbyGlobal_ButtonClick_Sound=aConfig->SafeGetSound("ButtonClick");
    LobbyGlobal_LobbyMusic_Sound=aConfig->SafeGetSound("LobbyMusic");
    LobbyGlobal_AllReady_Sound=aConfig->SafeGetSound("AllReady");
    LobbyGlobal_AllNotReady_Sound=aConfig->SafeGetSound("AllNotReady");
    LobbyGlobal_AskToJoinRecv_Sound=aConfig->SafeGetSound("AskToJoinRecv");
    LobbyGlobal_Error_Sound=aConfig->SafeGetSound("Error");
    LobbyGlobal_FriendJoined_Sound=aConfig->SafeGetSound("FriendJoined");
    LobbyGlobal_FriendLeft_Sound=aConfig->SafeGetSound("FriendLeft");
    LobbyGlobal_InviteRecv_Sound=aConfig->SafeGetSound("InviteRecv");
    LobbyGlobal_PlayerJoined_Sound=aConfig->SafeGetSound("PlayerJoined");
    LobbyGlobal_SierraLogon_Sound=aConfig->SafeGetSound("SierraLogon");
    LobbyGlobal_WhisperRecv_Sound=aConfig->SafeGetSound("WhisperRecv");
    aConfig=theTable->SafeGetConfig("AccountLogic");
    AccountLogic_InvalidCDKeyTitle_String=aConfig->SafeGetString("InvalidCDKeyTitle");
    AccountLogic_InvalidCDKeyDetails_String=aConfig->SafeGetString("InvalidCDKeyDetails");
    aConfig=theTable->SafeGetConfig("AddFriendDialog");
    AddFriendDialog_StatusTitle_String=aConfig->SafeGetString("StatusTitle");
    AddFriendDialog_StatusDetails_String=aConfig->SafeGetString("StatusDetails");
    aConfig=theTable->SafeGetConfig("AdminActionDialog");
    AdminActionDialog_Warn_String=aConfig->SafeGetString("Warn");
    AdminActionDialog_Mute_String=aConfig->SafeGetString("Mute");
    AdminActionDialog_UnMute_String=aConfig->SafeGetString("UnMute");
    AdminActionDialog_Kick_String=aConfig->SafeGetString("Kick");
    AdminActionDialog_Ban_String=aConfig->SafeGetString("Ban");
    AdminActionDialog_ExplanationOpt_String=aConfig->SafeGetString("ExplanationOpt");
    AdminActionDialog_ExplanationReq_String=aConfig->SafeGetString("ExplanationReq");
    aConfig=theTable->SafeGetConfig("BadUserDialog");
    BadUserDialog_OneDay_String=aConfig->SafeGetString("OneDay");
    BadUserDialog_OneHour_String=aConfig->SafeGetString("OneHour");
    BadUserDialog_OneMinute_String=aConfig->SafeGetString("OneMinute");
    BadUserDialog_OneSecond_String=aConfig->SafeGetString("OneSecond");
    BadUserDialog_XDays_String=aConfig->SafeGetString("XDays");
    BadUserDialog_XHours_String=aConfig->SafeGetString("XHours");
    BadUserDialog_XMinutes_String=aConfig->SafeGetString("XMinutes");
    BadUserDialog_XSeconds_String=aConfig->SafeGetString("XSeconds");
    BadUserDialog_Infinite_String=aConfig->SafeGetString("Infinite");
    aConfig=theTable->SafeGetConfig("ChatCommandLogic");
    ChatCommandLogic_Whisper_String=aConfig->SafeGetString("Whisper");
    ChatCommandLogic_Block_String=aConfig->SafeGetString("Block");
    ChatCommandLogic_Emote_String=aConfig->SafeGetString("Emote");
    ChatCommandLogic_Ignore_String=aConfig->SafeGetString("Ignore");
    ChatCommandLogic_Away_String=aConfig->SafeGetString("Away");
    ChatCommandLogic_Invite_String=aConfig->SafeGetString("Invite");
    ChatCommandLogic_Uninvite_String=aConfig->SafeGetString("Uninvite");
    ChatCommandLogic_Reply_String=aConfig->SafeGetString("Reply");
    ChatCommandLogic_Clear_String=aConfig->SafeGetString("Clear");
    ChatCommandLogic_Help_String=aConfig->SafeGetString("Help");
    ChatCommandLogic_ShowTeam_String=aConfig->SafeGetString("ShowTeam");
    ChatCommandLogic_Moderator_String=aConfig->SafeGetString("Moderator");
    ChatCommandLogic_ServerMute_String=aConfig->SafeGetString("ServerMute");
    ChatCommandLogic_ServerBan_String=aConfig->SafeGetString("ServerBan");
    ChatCommandLogic_Mute_String=aConfig->SafeGetString("Mute");
    ChatCommandLogic_Ban_String=aConfig->SafeGetString("Ban");
    ChatCommandLogic_Unmute_String=aConfig->SafeGetString("Unmute");
    ChatCommandLogic_ServerUnmute_String=aConfig->SafeGetString("ServerUnmute");
    ChatCommandLogic_Unban_String=aConfig->SafeGetString("Unban");
    ChatCommandLogic_ServerUnban_String=aConfig->SafeGetString("ServerUnban");
    ChatCommandLogic_Alert_String=aConfig->SafeGetString("Alert");
    ChatCommandLogic_Warn_String=aConfig->SafeGetString("Warn");
    ChatCommandLogic_HelpHeader_String=aConfig->SafeGetString("HelpHeader");
    ChatCommandLogic_HelpHelp_String=aConfig->SafeGetString("HelpHelp");
    ChatCommandLogic_ShowTeamHelp_String=aConfig->SafeGetString("ShowTeamHelp");
    ChatCommandLogic_WhisperHelp_String=aConfig->SafeGetString("WhisperHelp");
    ChatCommandLogic_BlockHelp_String=aConfig->SafeGetString("BlockHelp");
    ChatCommandLogic_IgnoreHelp_String=aConfig->SafeGetString("IgnoreHelp");
    ChatCommandLogic_ReplyHelp_String=aConfig->SafeGetString("ReplyHelp");
    ChatCommandLogic_EmoteHelp_String=aConfig->SafeGetString("EmoteHelp");
    ChatCommandLogic_AwayHelp_String=aConfig->SafeGetString("AwayHelp");
    ChatCommandLogic_ClearHelp_String=aConfig->SafeGetString("ClearHelp");
    ChatCommandLogic_InviteHelp_String=aConfig->SafeGetString("InviteHelp");
    ChatCommandLogic_UninviteHelp_String=aConfig->SafeGetString("UninviteHelp");
    ChatCommandLogic_MuteHelp_String=aConfig->SafeGetString("MuteHelp");
    ChatCommandLogic_UnmuteHelp_String=aConfig->SafeGetString("UnmuteHelp");
    ChatCommandLogic_BanHelp_String=aConfig->SafeGetString("BanHelp");
    ChatCommandLogic_UnbanHelp_String=aConfig->SafeGetString("UnbanHelp");
    ChatCommandLogic_ModeratorHelp_String=aConfig->SafeGetString("ModeratorHelp");
    ChatCommandLogic_ServerMuteHelp_String=aConfig->SafeGetString("ServerMuteHelp");
    ChatCommandLogic_ServerUnmuteHelp_String=aConfig->SafeGetString("ServerUnmuteHelp");
    ChatCommandLogic_ServerBanHelp_String=aConfig->SafeGetString("ServerBanHelp");
    ChatCommandLogic_ServerUnbanHelp_String=aConfig->SafeGetString("ServerUnbanHelp");
    ChatCommandLogic_AlertHelp_String=aConfig->SafeGetString("AlertHelp");
    ChatCommandLogic_WarnHelp_String=aConfig->SafeGetString("WarnHelp");
    aConfig=theTable->SafeGetConfig("ChatCtrl");
    ChatCtrl_NormalChat_String=aConfig->SafeGetString("NormalChat");
    ChatCtrl_Emote_String=aConfig->SafeGetString("Emote");
    ChatCtrl_TeamChat_String=aConfig->SafeGetString("TeamChat");
    ChatCtrl_TeamEmote_String=aConfig->SafeGetString("TeamEmote");
    ChatCtrl_YouWhisperToYourself_String=aConfig->SafeGetString("YouWhisperToYourself");
    ChatCtrl_YouWhisper_String=aConfig->SafeGetString("YouWhisper");
    ChatCtrl_SomeoneWhispersToYou_String=aConfig->SafeGetString("SomeoneWhispersToYou");
    ChatCtrl_YouWarnSomeone_String=aConfig->SafeGetString("YouWarnSomeone");
    ChatCtrl_YouWarnYourself_String=aConfig->SafeGetString("YouWarnYourself");
    ChatCtrl_SomeoneWarnsYou_String=aConfig->SafeGetString("SomeoneWarnsYou");
    ChatCtrl_SomeoneWarnsSomeone_String=aConfig->SafeGetString("SomeoneWarnsSomeone");
    ChatCtrl_EnteringRoom_String=aConfig->SafeGetString("EnteringRoom");
    ChatCtrl_LeavingRoom_String=aConfig->SafeGetString("LeavingRoom");
    ChatCtrl_Disconnected_String=aConfig->SafeGetString("Disconnected");
    ChatCtrl_YouWereKicked_String=aConfig->SafeGetString("YouWereKicked");
    ChatCtrl_YouWereBannedMinute_String=aConfig->SafeGetString("YouWereBannedMinute");
    ChatCtrl_YouWereBannedMinutes_String=aConfig->SafeGetString("YouWereBannedMinutes");
    ChatCtrl_YouWereBannedHour_String=aConfig->SafeGetString("YouWereBannedHour");
    ChatCtrl_YouWereBannedHours_String=aConfig->SafeGetString("YouWereBannedHours");
    ChatCtrl_YouWereBannedDay_String=aConfig->SafeGetString("YouWereBannedDay");
    ChatCtrl_YouWereBannedDays_String=aConfig->SafeGetString("YouWereBannedDays");
    ChatCtrl_YouWereBannedIndefinitely_String=aConfig->SafeGetString("YouWereBannedIndefinitely");
    ChatCtrl_SomeoneWasKicked_String=aConfig->SafeGetString("SomeoneWasKicked");
    ChatCtrl_SomeoneWasBannedMinute_String=aConfig->SafeGetString("SomeoneWasBannedMinute");
    ChatCtrl_SomeoneWasBannedMinutes_String=aConfig->SafeGetString("SomeoneWasBannedMinutes");
    ChatCtrl_SomeoneWasBannedHour_String=aConfig->SafeGetString("SomeoneWasBannedHour");
    ChatCtrl_SomeoneWasBannedHours_String=aConfig->SafeGetString("SomeoneWasBannedHours");
    ChatCtrl_SomeoneWasBannedDay_String=aConfig->SafeGetString("SomeoneWasBannedDay");
    ChatCtrl_SomeoneWasBannedDays_String=aConfig->SafeGetString("SomeoneWasBannedDays");
    ChatCtrl_SomeoneWasBannedIndefinitely_String=aConfig->SafeGetString("SomeoneWasBannedIndefinitely");
    ChatCtrl_YouWereUnmuted_String=aConfig->SafeGetString("YouWereUnmuted");
    ChatCtrl_YouWereMutedMinute_String=aConfig->SafeGetString("YouWereMutedMinute");
    ChatCtrl_YouWereMutedMinutes_String=aConfig->SafeGetString("YouWereMutedMinutes");
    ChatCtrl_YouWereMutedHour_String=aConfig->SafeGetString("YouWereMutedHour");
    ChatCtrl_YouWereMutedHours_String=aConfig->SafeGetString("YouWereMutedHours");
    ChatCtrl_YouWereMutedDay_String=aConfig->SafeGetString("YouWereMutedDay");
    ChatCtrl_YouWereMutedDays_String=aConfig->SafeGetString("YouWereMutedDays");
    ChatCtrl_YouWereMutedIndefinitely_String=aConfig->SafeGetString("YouWereMutedIndefinitely");
    ChatCtrl_SomeoneWasUnmuted_String=aConfig->SafeGetString("SomeoneWasUnmuted");
    ChatCtrl_SomeoneWasMutedMinute_String=aConfig->SafeGetString("SomeoneWasMutedMinute");
    ChatCtrl_SomeoneWasMutedMinutes_String=aConfig->SafeGetString("SomeoneWasMutedMinutes");
    ChatCtrl_SomeoneWasMutedHour_String=aConfig->SafeGetString("SomeoneWasMutedHour");
    ChatCtrl_SomeoneWasMutedHours_String=aConfig->SafeGetString("SomeoneWasMutedHours");
    ChatCtrl_SomeoneWasMutedDay_String=aConfig->SafeGetString("SomeoneWasMutedDay");
    ChatCtrl_SomeoneWasMutedDays_String=aConfig->SafeGetString("SomeoneWasMutedDays");
    ChatCtrl_SomeoneWasMutedIndefinitely_String=aConfig->SafeGetString("SomeoneWasMutedIndefinitely");
    ChatCtrl_ModeratorComment_String=aConfig->SafeGetString("ModeratorComment");
    ChatCtrl_AlertComment_String=aConfig->SafeGetString("AlertComment");
    ChatCtrl_ServerAlert_String=aConfig->SafeGetString("ServerAlert");
    ChatCtrl_ServerShutdownStartedMinute_String=aConfig->SafeGetString("ServerShutdownStartedMinute");
    ChatCtrl_ServerShutdownStartedMinutes_String=aConfig->SafeGetString("ServerShutdownStartedMinutes");
    ChatCtrl_ServerShutdownStartedHour_String=aConfig->SafeGetString("ServerShutdownStartedHour");
    ChatCtrl_ServerShutdownStartedHours_String=aConfig->SafeGetString("ServerShutdownStartedHours");
    ChatCtrl_ServerShutdownStartedDay_String=aConfig->SafeGetString("ServerShutdownStartedDay");
    ChatCtrl_ServerShutdownStartedDays_String=aConfig->SafeGetString("ServerShutdownStartedDays");
    ChatCtrl_ServerShutdownAborted_String=aConfig->SafeGetString("ServerShutdownAborted");
    ChatCtrl_SomeoneInvitesYou_String=aConfig->SafeGetString("SomeoneInvitesYou");
    ChatCtrl_SomeoneUninvitesYou_String=aConfig->SafeGetString("SomeoneUninvitesYou");
    ChatCtrl_YouInviteSomeone_String=aConfig->SafeGetString("YouInviteSomeone");
    ChatCtrl_YouUninviteSomeone_String=aConfig->SafeGetString("YouUninviteSomeone");
    ChatCtrl_SomeoneInvitesYouExpl_String=aConfig->SafeGetString("SomeoneInvitesYouExpl");
    ChatCtrl_SomeoneUninvitesYouExpl_String=aConfig->SafeGetString("SomeoneUninvitesYouExpl");
    ChatCtrl_YouInviteSomeoneExpl_String=aConfig->SafeGetString("YouInviteSomeoneExpl");
    ChatCtrl_YouUninviteSomeoneExpl_String=aConfig->SafeGetString("YouUninviteSomeoneExpl");
    ChatCtrl_Accept_String=aConfig->SafeGetString("Accept");
    ChatCtrl_Reject_String=aConfig->SafeGetString("Reject");
    ChatCtrl_SomeoneAsksToJoin_String=aConfig->SafeGetString("SomeoneAsksToJoin");
    ChatCtrl_SomeoneAsksToJoinExpl_String=aConfig->SafeGetString("SomeoneAsksToJoinExpl");
    ChatCtrl_ClientNotOnServer_String=aConfig->SafeGetString("ClientNotOnServer");
    ChatCtrl_ClientInGame_String=aConfig->SafeGetString("ClientInGame");
    ChatCtrl_ClientInChat_String=aConfig->SafeGetString("ClientInChat");
    ChatCtrl_ClientInGameAndChat_String=aConfig->SafeGetString("ClientInGameAndChat");
    ChatCtrl_ClientIsAnonymous_String=aConfig->SafeGetString("ClientIsAnonymous");
    ChatCtrl_FriendIsChatting_String=aConfig->SafeGetString("FriendIsChatting");
    ChatCtrl_FriendIsInGameStaging_String=aConfig->SafeGetString("FriendIsInGameStaging");
    ChatCtrl_FriendIsPlaying_String=aConfig->SafeGetString("FriendIsPlaying");
    ChatCtrl_FriendIsOn_String=aConfig->SafeGetString("FriendIsOn");
    ChatCtrl_FriendIsNotOnline_String=aConfig->SafeGetString("FriendIsNotOnline");
    ChatCtrl_PlayerKicked_String=aConfig->SafeGetString("PlayerKicked");
    ChatCtrl_PlayerBanned_String=aConfig->SafeGetString("PlayerBanned");
    ChatCtrl_FriendJoined_String=aConfig->SafeGetString("FriendJoined");
    ChatCtrl_FriendLeft_String=aConfig->SafeGetString("FriendLeft");
    ChatCtrl_TeamMemberJoined_String=aConfig->SafeGetString("TeamMemberJoined");
    ChatCtrl_TeamMemberLeft_String=aConfig->SafeGetString("TeamMemberLeft");
    ChatCtrl_TeamMembers_String=aConfig->SafeGetString("TeamMembers");
    ChatCtrl_IgnoreList_String=aConfig->SafeGetString("IgnoreList");
    aConfig=theTable->SafeGetConfig("ChooseNetworkAdapterDialog");
    ChooseNetworkAdapterDialog_Default_String=aConfig->SafeGetString("Default");
    ChooseNetworkAdapterDialog_Unknown_String=aConfig->SafeGetString("Unknown");
    aConfig=theTable->SafeGetConfig("ClientOptionsCtrl");
    ClientOptionsCtrl_Whisper_String=aConfig->SafeGetString("Whisper");
    ClientOptionsCtrl_Invite_String=aConfig->SafeGetString("Invite");
    ClientOptionsCtrl_Uninvite_String=aConfig->SafeGetString("Uninvite");
    ClientOptionsCtrl_AddToFriends_String=aConfig->SafeGetString("AddToFriends");
    ClientOptionsCtrl_RemoveFromFriends_String=aConfig->SafeGetString("RemoveFromFriends");
    ClientOptionsCtrl_ClearAFK_String=aConfig->SafeGetString("ClearAFK");
    ClientOptionsCtrl_SetAFK_String=aConfig->SafeGetString("SetAFK");
    ClientOptionsCtrl_Ignore_String=aConfig->SafeGetString("Ignore");
    ClientOptionsCtrl_Unignore_String=aConfig->SafeGetString("Unignore");
    ClientOptionsCtrl_Block_String=aConfig->SafeGetString("Block");
    ClientOptionsCtrl_Unblock_String=aConfig->SafeGetString("Unblock");
    ClientOptionsCtrl_Warn_String=aConfig->SafeGetString("Warn");
    ClientOptionsCtrl_Mute_String=aConfig->SafeGetString("Mute");
    ClientOptionsCtrl_UnMute_String=aConfig->SafeGetString("UnMute");
    ClientOptionsCtrl_Kick_String=aConfig->SafeGetString("Kick");
    ClientOptionsCtrl_Ban_String=aConfig->SafeGetString("Ban");
    aConfig=theTable->SafeGetConfig("DirectConnectionOptionsCtrl");
    DirectConnectionOptionsCtrl_Connect_String=aConfig->SafeGetString("Connect");
    DirectConnectionOptionsCtrl_Refresh_String=aConfig->SafeGetString("Refresh");
    DirectConnectionOptionsCtrl_Remove_String=aConfig->SafeGetString("Remove");
    aConfig=theTable->SafeGetConfig("FriendOptionsCtrl");
    FriendOptionsCtrl_Locate_String=aConfig->SafeGetString("Locate");
    FriendOptionsCtrl_RefreshList_String=aConfig->SafeGetString("RefreshList");
    FriendOptionsCtrl_RemoveFromFriends_String=aConfig->SafeGetString("RemoveFromFriends");
    aConfig=theTable->SafeGetConfig("FriendsListCtrl");
    FriendsListCtrl_Unknown_Image=aConfig->SafeGetImage("Unknown");
    FriendsListCtrl_NotFound_Image=aConfig->SafeGetImage("NotFound");
    FriendsListCtrl_InChat_Image=aConfig->SafeGetImage("InChat");
    FriendsListCtrl_InGame_Image=aConfig->SafeGetImage("InGame");
    aConfig=theTable->SafeGetConfig("GameOptionsPopup");
    GameOptionsPopup_Join_String=aConfig->SafeGetString("Join");
    GameOptionsPopup_Ping_String=aConfig->SafeGetString("Ping");
    GameOptionsPopup_QueryDetails_String=aConfig->SafeGetString("QueryDetails");
    GameOptionsPopup_HideDetails_String=aConfig->SafeGetString("HideDetails");
    GameOptionsPopup_ShowDetails_String=aConfig->SafeGetString("ShowDetails");
    GameOptionsPopup_Remove_String=aConfig->SafeGetString("Remove");
    aConfig=theTable->SafeGetConfig("GameStagingCtrl");
    GameStagingCtrl_Ready_String=aConfig->SafeGetString("Ready");
    GameStagingCtrl_NotReady_String=aConfig->SafeGetString("NotReady");
    GameStagingCtrl_StartGame_String=aConfig->SafeGetString("StartGame");
    GameStagingCtrl_LeaveGame_String=aConfig->SafeGetString("LeaveGame");
    GameStagingCtrl_DissolveGame_String=aConfig->SafeGetString("DissolveGame");
    aConfig=theTable->SafeGetConfig("GameStagingLogic");
    GameStagingLogic_KickedTitle_String=aConfig->SafeGetString("KickedTitle");
    GameStagingLogic_KickedDetails_String=aConfig->SafeGetString("KickedDetails");
    GameStagingLogic_BannedTitle_String=aConfig->SafeGetString("BannedTitle");
    GameStagingLogic_BannedDetails_String=aConfig->SafeGetString("BannedDetails");
    GameStagingLogic_ConnectWarningTitle1_String=aConfig->SafeGetString("ConnectWarningTitle1");
    GameStagingLogic_ConnectWarningDetails1_String=aConfig->SafeGetString("ConnectWarningDetails1");
    GameStagingLogic_ConnectWarningTitle2_String=aConfig->SafeGetString("ConnectWarningTitle2");
    GameStagingLogic_ConnectWarningDetails2_String=aConfig->SafeGetString("ConnectWarningDetails2");
    GameStagingLogic_ConnectWarningTitle3_String=aConfig->SafeGetString("ConnectWarningTitle3");
    GameStagingLogic_ConnectWarningDetails3_String=aConfig->SafeGetString("ConnectWarningDetails3");
    GameStagingLogic_NoPingReplyError_String=aConfig->SafeGetString("NoPingReplyError");
    GameStagingLogic_GettingGameInfoTitle_String=aConfig->SafeGetString("GettingGameInfoTitle");
    GameStagingLogic_GettingGameInfoDetails_String=aConfig->SafeGetString("GettingGameInfoDetails");
    GameStagingLogic_JoinGameErrorTitle_String=aConfig->SafeGetString("JoinGameErrorTitle");
    GameStagingLogic_VerifyingPingTitle_String=aConfig->SafeGetString("VerifyingPingTitle");
    GameStagingLogic_VerifyingPingDetails_String=aConfig->SafeGetString("VerifyingPingDetails");
    GameStagingLogic_DissolveGame_String=aConfig->SafeGetString("DissolveGame");
    GameStagingLogic_DissolveGameConfirm_String=aConfig->SafeGetString("DissolveGameConfirm");
    GameStagingLogic_LeaveGame_String=aConfig->SafeGetString("LeaveGame");
    GameStagingLogic_LeaveGameConfirm_String=aConfig->SafeGetString("LeaveGameConfirm");
    GameStagingLogic_NoQuickPlayGameTitle_String=aConfig->SafeGetString("NoQuickPlayGameTitle");
    GameStagingLogic_NoQuickPlayGameDetails_String=aConfig->SafeGetString("NoQuickPlayGameDetails");
    aConfig=theTable->SafeGetConfig("GameUserListCtrl");
    GameUserListCtrl_Ban_String=aConfig->SafeGetString("Ban");
    GameUserListCtrl_Kick_String=aConfig->SafeGetString("Kick");
    GameUserListCtrl_Ready_Image=aConfig->SafeGetImage("Ready");
    aConfig=theTable->SafeGetConfig("InternetQuickPlay");
    InternetQuickPlay_Welcome_String=aConfig->SafeGetString("Welcome");
    InternetQuickPlay_UserWelcome_String=aConfig->SafeGetString("UserWelcome");
    aConfig=theTable->SafeGetConfig("JoinResponse");
    JoinResponse_Joined_String=aConfig->SafeGetString("Joined");
    JoinResponse_ServerFull_String=aConfig->SafeGetString("ServerFull");
    JoinResponse_InviteOnly_String=aConfig->SafeGetString("InviteOnly");
    JoinResponse_AskToJoin_String=aConfig->SafeGetString("AskToJoin");
    JoinResponse_GameInProgress_String=aConfig->SafeGetString("GameInProgress");
    JoinResponse_InvalidPassword_String=aConfig->SafeGetString("InvalidPassword");
    JoinResponse_NotOnLAN_String=aConfig->SafeGetString("NotOnLAN");
    JoinResponse_Blocked_String=aConfig->SafeGetString("Blocked");
    JoinResponse_Unknown_String=aConfig->SafeGetString("Unknown");
    aConfig=theTable->SafeGetConfig("LANLogic");
    LANLogic_SocketBindError_String=aConfig->SafeGetString("SocketBindError");
    LANLogic_SocketListenError_String=aConfig->SafeGetString("SocketListenError");
    LANLogic_ConnectToGameTitle_String=aConfig->SafeGetString("ConnectToGameTitle");
    LANLogic_ConnectToGameDetails_String=aConfig->SafeGetString("ConnectToGameDetails");
    LANLogic_CheckingAddressTitle_String=aConfig->SafeGetString("CheckingAddressTitle");
    LANLogic_CheckingAddressDetails_String=aConfig->SafeGetString("CheckingAddressDetails");
    LANLogic_RegisterTitle_String=aConfig->SafeGetString("RegisterTitle");
    LANLogic_RegisterDetails_String=aConfig->SafeGetString("RegisterDetails");
    LANLogic_JoinFailure_String=aConfig->SafeGetString("JoinFailure");
    LANLogic_JoinReplyError_String=aConfig->SafeGetString("JoinReplyError");
    LANLogic_InvalidSubnet_String=aConfig->SafeGetString("InvalidSubnet");
    aConfig=theTable->SafeGetConfig("LobbyContainer");
    LobbyContainer_InitStart_String=aConfig->SafeGetString("InitStart");
    LobbyContainer_InitFailed_String=aConfig->SafeGetString("InitFailed");
    LobbyContainer_CheckingVersion_String=aConfig->SafeGetString("CheckingVersion");
    LobbyContainer_QueryingServers_String=aConfig->SafeGetString("QueryingServers");
    LobbyContainer_NoConnection_String=aConfig->SafeGetString("NoConnection");
    LobbyContainer_DNSLookupFailed_String=aConfig->SafeGetString("DNSLookupFailed");
    LobbyContainer_DirLookupFailure_String=aConfig->SafeGetString("DirLookupFailure");
    LobbyContainer_VersionCheckFailure_String=aConfig->SafeGetString("VersionCheckFailure");
    LobbyContainer_InitComplete_String=aConfig->SafeGetString("InitComplete");
    LobbyContainer_NoServers_String=aConfig->SafeGetString("NoServers");
    LobbyContainer_AuthServers_String=aConfig->SafeGetString("AuthServers");
    LobbyContainer_AccountServers_String=aConfig->SafeGetString("AccountServers");
    LobbyContainer_VersionServers_String=aConfig->SafeGetString("VersionServers");
    LobbyContainer_CreateAccountTitle_String=aConfig->SafeGetString("CreateAccountTitle");
    LobbyContainer_CreateAccountDetails_String=aConfig->SafeGetString("CreateAccountDetails");
    LobbyContainer_RetrievePasswordTitle_String=aConfig->SafeGetString("RetrievePasswordTitle");
    LobbyContainer_RetrievePasswordDetails_String=aConfig->SafeGetString("RetrievePasswordDetails");
    LobbyContainer_RetrieveUsernameTitle_String=aConfig->SafeGetString("RetrieveUsernameTitle");
    LobbyContainer_RetrieveUsernameDetails_String=aConfig->SafeGetString("RetrieveUsernameDetails");
    LobbyContainer_QueryAccountTitle_String=aConfig->SafeGetString("QueryAccountTitle");
    LobbyContainer_QueryAccountDetails_String=aConfig->SafeGetString("QueryAccountDetails");
    LobbyContainer_UpdateAccountTitle_String=aConfig->SafeGetString("UpdateAccountTitle");
    LobbyContainer_UpdateAccountDetails_String=aConfig->SafeGetString("UpdateAccountDetails");
    LobbyContainer_LoginTitle_String=aConfig->SafeGetString("LoginTitle");
    LobbyContainer_LoginDetails_String=aConfig->SafeGetString("LoginDetails");
    LobbyContainer_GettingRoomsTitle_String=aConfig->SafeGetString("GettingRoomsTitle");
    LobbyContainer_GettingRoomsDetails_String=aConfig->SafeGetString("GettingRoomsDetails");
    LobbyContainer_VersionOutOfDataTitle_String=aConfig->SafeGetString("VersionOutOfDataTitle");
    LobbyContainer_VersionOutOfDataDetails_String=aConfig->SafeGetString("VersionOutOfDataDetails");
    aConfig=theTable->SafeGetConfig("LobbyGameStatus");
    LobbyGameStatus_Success_String=aConfig->SafeGetString("Success");
    LobbyGameStatus_GameFull_String=aConfig->SafeGetString("GameFull");
    LobbyGameStatus_GameInProgress_String=aConfig->SafeGetString("GameInProgress");
    LobbyGameStatus_UnpackFailure_String=aConfig->SafeGetString("UnpackFailure");
    LobbyGameStatus_NotInvited_String=aConfig->SafeGetString("NotInvited");
    LobbyGameStatus_CaptainRejectedYou_String=aConfig->SafeGetString("CaptainRejectedYou");
    LobbyGameStatus_InvalidPassword_String=aConfig->SafeGetString("InvalidPassword");
    LobbyGameStatus_DuplicateName_String=aConfig->SafeGetString("DuplicateName");
    aConfig=theTable->SafeGetConfig("LobbyOptions");
    LobbyOptions_DefaultName_String=aConfig->SafeGetString("DefaultName");
    LobbyOptions_CheckingAddress_String=aConfig->SafeGetString("CheckingAddress");
    LobbyOptions_AddressUnavailable_String=aConfig->SafeGetString("AddressUnavailable");
    aConfig=theTable->SafeGetConfig("RoomCtrl");
    RoomCtrl_QueryingServer_String=aConfig->SafeGetString("QueryingServer");
    RoomCtrl_Password_Image=aConfig->SafeGetImage("Password");
    aConfig=theTable->SafeGetConfig("RoomLabelCtrl");
    RoomLabelCtrl_LabelPrefix_String=aConfig->SafeGetString("LabelPrefix");
    RoomLabelCtrl_LabelServer_String=aConfig->SafeGetString("LabelServer");
    RoomLabelCtrl_LabelUser_String=aConfig->SafeGetString("LabelUser");
    RoomLabelCtrl_LabelUsers_String=aConfig->SafeGetString("LabelUsers");
    RoomLabelCtrl_LabelDisconnected_String=aConfig->SafeGetString("LabelDisconnected");
    aConfig=theTable->SafeGetConfig("RoutingLogic");
    RoutingLogic_GetClientListTitle_String=aConfig->SafeGetString("GetClientListTitle");
    RoutingLogic_GetClientListDetails_String=aConfig->SafeGetString("GetClientListDetails");
    RoutingLogic_GetGroupListTitle_String=aConfig->SafeGetString("GetGroupListTitle");
    RoutingLogic_GetGroupListDetails_String=aConfig->SafeGetString("GetGroupListDetails");
    RoutingLogic_GameDissolvedTitle_String=aConfig->SafeGetString("GameDissolvedTitle");
    RoutingLogic_GameDissolvedDetails_String=aConfig->SafeGetString("GameDissolvedDetails");
    RoutingLogic_NoServersTitle_String=aConfig->SafeGetString("NoServersTitle");
    RoutingLogic_NoServersDetails_String=aConfig->SafeGetString("NoServersDetails");
    RoutingLogic_TooManyFriendsTitle_String=aConfig->SafeGetString("TooManyFriendsTitle");
    RoutingLogic_TooManyFriendsDetails_String=aConfig->SafeGetString("TooManyFriendsDetails");
    RoutingLogic_AlreadyJoinedTitle_String=aConfig->SafeGetString("AlreadyJoinedTitle");
    RoutingLogic_AlreadyInGame_String=aConfig->SafeGetString("AlreadyInGame");
    RoutingLogic_AlreadyInRoom_String=aConfig->SafeGetString("AlreadyInRoom");
    RoutingLogic_JoinGroupTitle_String=aConfig->SafeGetString("JoinGroupTitle");
    RoutingLogic_JoinGroupDetails_String=aConfig->SafeGetString("JoinGroupDetails");
    RoutingLogic_ConnectTitle_String=aConfig->SafeGetString("ConnectTitle");
    RoutingLogic_ConnectDetails_String=aConfig->SafeGetString("ConnectDetails");
    RoutingLogic_RegisterTitle_String=aConfig->SafeGetString("RegisterTitle");
    RoutingLogic_RegisterDetails_String=aConfig->SafeGetString("RegisterDetails");
    RoutingLogic_QueryGroupsTitle_String=aConfig->SafeGetString("QueryGroupsTitle");
    RoutingLogic_QueryGroupsDetails_String=aConfig->SafeGetString("QueryGroupsDetails");
    RoutingLogic_CreateGroupTitle_String=aConfig->SafeGetString("CreateGroupTitle");
    RoutingLogic_CreateGroupDetails_String=aConfig->SafeGetString("CreateGroupDetails");
    RoutingLogic_GetBadUsersTitle_String=aConfig->SafeGetString("GetBadUsersTitle");
    RoutingLogic_GetBadUsersDetails_String=aConfig->SafeGetString("GetBadUsersDetails");
    RoutingLogic_DisconnectedTitle_String=aConfig->SafeGetString("DisconnectedTitle");
    RoutingLogic_DisconnectedDetails_String=aConfig->SafeGetString("DisconnectedDetails");
    RoutingLogic_AskingCaptainTitle_String=aConfig->SafeGetString("AskingCaptainTitle");
    RoutingLogic_AskingCaptainDetails_String=aConfig->SafeGetString("AskingCaptainDetails");
    RoutingLogic_CollectingPingsTitle_String=aConfig->SafeGetString("CollectingPingsTitle");
    RoutingLogic_CollectingPingsDetails_String=aConfig->SafeGetString("CollectingPingsDetails");
    RoutingLogic_GettingGameListTitle_String=aConfig->SafeGetString("GettingGameListTitle");
    RoutingLogic_GettingGameListDetails_String=aConfig->SafeGetString("GettingGameListDetails");
    RoutingLogic_MuteClientTitle_String=aConfig->SafeGetString("MuteClientTitle");
    RoutingLogic_UnmuteClientTitle_String=aConfig->SafeGetString("UnmuteClientTitle");
    RoutingLogic_BanClientTitle_String=aConfig->SafeGetString("BanClientTitle");
    RoutingLogic_KickClientTitle_String=aConfig->SafeGetString("KickClientTitle");
    RoutingLogic_ModerateDetails_String=aConfig->SafeGetString("ModerateDetails");
    RoutingLogic_FriendNotFoundTitle_String=aConfig->SafeGetString("FriendNotFoundTitle");
    RoutingLogic_FriendNotFoundDetails_String=aConfig->SafeGetString("FriendNotFoundDetails");
    RoutingLogic_ClientNotFound_String=aConfig->SafeGetString("ClientNotFound");
    RoutingLogic_ClientAmbiguous_String=aConfig->SafeGetString("ClientAmbiguous");
    RoutingLogic_InvalidCommand_String=aConfig->SafeGetString("InvalidCommand");
    RoutingLogic_RequireName_String=aConfig->SafeGetString("RequireName");
    RoutingLogic_BadTimeSpec_String=aConfig->SafeGetString("BadTimeSpec");
    RoutingLogic_Error_String=aConfig->SafeGetString("Error");
    RoutingLogic_NotCaptain_String=aConfig->SafeGetString("NotCaptain");
    aConfig=theTable->SafeGetConfig("ServerListCtrl");
    ServerListCtrl_Pinging_String=aConfig->SafeGetString("Pinging");
    ServerListCtrl_PingFailed_String=aConfig->SafeGetString("PingFailed");
    ServerListCtrl_Querying_String=aConfig->SafeGetString("Querying");
    ServerListCtrl_QueryFailed_String=aConfig->SafeGetString("QueryFailed");
    ServerListCtrl_ListSummary_String=aConfig->SafeGetString("ListSummary");
    ServerListCtrl_ListPingSummary_String=aConfig->SafeGetString("ListPingSummary");
    ServerListCtrl_RefreshList_String=aConfig->SafeGetString("RefreshList");
    ServerListCtrl_StopRefresh_String=aConfig->SafeGetString("StopRefresh");
    ServerListCtrl_NoFilter_String=aConfig->SafeGetString("NoFilter");
    ServerListCtrl_AddRemove_String=aConfig->SafeGetString("AddRemove");
    ServerListCtrl_Add_String=aConfig->SafeGetString("Add");
    ServerListCtrl_Remove_String=aConfig->SafeGetString("Remove");
    ServerListCtrl_FullFilter_String=aConfig->SafeGetString("FullFilter");
    ServerListCtrl_NotFullFilter_String=aConfig->SafeGetString("NotFullFilter");
    ServerListCtrl_OpenFilter_String=aConfig->SafeGetString("OpenFilter");
    ServerListCtrl_PingFailed_Image=aConfig->SafeGetImage("PingFailed");
    ServerListCtrl_GameExpanded_Image=aConfig->SafeGetImage("GameExpanded");
    ServerListCtrl_GameNotExpanded_Image=aConfig->SafeGetImage("GameNotExpanded");
    ServerListCtrl_GameAreInvited_Image=aConfig->SafeGetImage("GameAreInvited");
    ServerListCtrl_GameNone_Image=aConfig->SafeGetImage("GameNone");
    ServerListCtrl_GameAskToJoin_Image=aConfig->SafeGetImage("GameAskToJoin");
    ServerListCtrl_GameHasPassword_Image=aConfig->SafeGetImage("GameHasPassword");
    ServerListCtrl_GameNotInvited_Image=aConfig->SafeGetImage("GameNotInvited");
    ServerListCtrl_GameInProgress_Image=aConfig->SafeGetImage("GameInProgress");
    ServerListCtrl_SkillOpen_Image=aConfig->SafeGetImage("SkillOpen");
    ServerListCtrl_SkillNovice_Image=aConfig->SafeGetImage("SkillNovice");
    ServerListCtrl_SkillIntermediate_Image=aConfig->SafeGetImage("SkillIntermediate");
    ServerListCtrl_SkillAdvanced_Image=aConfig->SafeGetImage("SkillAdvanced");
    ServerListCtrl_SkillExpert_Image=aConfig->SafeGetImage("SkillExpert");
    aConfig=theTable->SafeGetConfig("ServerOptionsCtrl");
    ServerOptionsCtrl_Connect_String=aConfig->SafeGetString("Connect");
    ServerOptionsCtrl_RefreshServer_String=aConfig->SafeGetString("RefreshServer");
    ServerOptionsCtrl_HideDetails_String=aConfig->SafeGetString("HideDetails");
    ServerOptionsCtrl_ShowDetails_String=aConfig->SafeGetString("ShowDetails");
    ServerOptionsCtrl_ResfreshList_String=aConfig->SafeGetString("ResfreshList");
    aConfig=theTable->SafeGetConfig("StagingLogic");
    StagingLogic_DisconnectedTitle_String=aConfig->SafeGetString("DisconnectedTitle");
    StagingLogic_DisconnectedDetails_String=aConfig->SafeGetString("DisconnectedDetails");
    StagingLogic_BannedTitle_String=aConfig->SafeGetString("BannedTitle");
    StagingLogic_BannedDetails_String=aConfig->SafeGetString("BannedDetails");
    StagingLogic_KickedTitle_String=aConfig->SafeGetString("KickedTitle");
    StagingLogic_KickedDetails_String=aConfig->SafeGetString("KickedDetails");
    aConfig=theTable->SafeGetConfig("UserListCtrl");
    UserListCtrl_Ignore_String=aConfig->SafeGetString("Ignore");
    UserListCtrl_Unignore_String=aConfig->SafeGetString("Unignore");
    UserListCtrl_Normal_Image=aConfig->SafeGetImage("Normal");
    UserListCtrl_Away_Image=aConfig->SafeGetImage("Away");
    UserListCtrl_Admin_Image=aConfig->SafeGetImage("Admin");
    UserListCtrl_Moderator_Image=aConfig->SafeGetImage("Moderator");
    UserListCtrl_Captain_Image=aConfig->SafeGetImage("Captain");
    UserListCtrl_Muted_Image=aConfig->SafeGetImage("Muted");
    UserListCtrl_Ignored_Image=aConfig->SafeGetImage("Ignored");
    UserListCtrl_Blocked_Image=aConfig->SafeGetImage("Blocked");
    aConfig=theTable->SafeGetConfig("WONStatus");
    WONStatus_Success_String=aConfig->SafeGetString("Success");
    WONStatus_Killed_String=aConfig->SafeGetString("Killed");
    WONStatus_TimedOut_String=aConfig->SafeGetString("TimedOut");
    WONStatus_InvalidAddress_String=aConfig->SafeGetString("InvalidAddress");
    WONStatus_GeneralFailure_String=aConfig->SafeGetString("GeneralFailure");
    WONStatus_AuthFailure_String=aConfig->SafeGetString("AuthFailure");
    WONStatus_FTPFailure_String=aConfig->SafeGetString("FTPFailure");
    WONStatus_HTTPFailure_String=aConfig->SafeGetString("HTTPFailure");
    WONStatus_MOTDFailure_String=aConfig->SafeGetString("MOTDFailure");
    WONStatus_RoutingFailure_String=aConfig->SafeGetString("RoutingFailure");
    WONStatus_GameSpyFailure_String=aConfig->SafeGetString("GameSpyFailure");
    WONStatus_DirFailure_String=aConfig->SafeGetString("DirFailure");
    WONStatus_ChatFailure_String=aConfig->SafeGetString("ChatFailure");
    WONStatus_DatabaseFailure_String=aConfig->SafeGetString("DatabaseFailure");
    WONStatus_NotAllowed_String=aConfig->SafeGetString("NotAllowed");
    WONStatus_NotAuthorized_String=aConfig->SafeGetString("NotAuthorized");
    WONStatus_BehindFirewall_String=aConfig->SafeGetString("BehindFirewall");
    WONStatus_ExpiredCertificate_String=aConfig->SafeGetString("ExpiredCertificate");
    WONStatus_InvalidCDKey_String=aConfig->SafeGetString("InvalidCDKey");
    WONStatus_CDKeyBanned_String=aConfig->SafeGetString("CDKeyBanned");
    WONStatus_CDKeyInUse_String=aConfig->SafeGetString("CDKeyInUse");
    WONStatus_CRCFailed_String=aConfig->SafeGetString("CRCFailed");
    WONStatus_UserExists_String=aConfig->SafeGetString("UserExists");
    WONStatus_UserNotFound_String=aConfig->SafeGetString("UserNotFound");
    WONStatus_BadPassword_String=aConfig->SafeGetString("BadPassword");
    WONStatus_InvalidUserName_String=aConfig->SafeGetString("InvalidUserName");
    WONStatus_BadCommunity_String=aConfig->SafeGetString("BadCommunity");
    WONStatus_NotInCommunity_String=aConfig->SafeGetString("NotInCommunity");
    WONStatus_UserSeqInUse_String=aConfig->SafeGetString("UserSeqInUse");
    WONStatus_BlankNamesNotAllowed_String=aConfig->SafeGetString("BlankNamesNotAllowed");
    WONStatus_CaptainRejectedYou_String=aConfig->SafeGetString("CaptainRejectedYou");
    WONStatus_ClientAlreadyBanned_String=aConfig->SafeGetString("ClientAlreadyBanned");
    WONStatus_ClientAlreadyInGroup_String=aConfig->SafeGetString("ClientAlreadyInGroup");
    WONStatus_ClientAlreadyMuted_String=aConfig->SafeGetString("ClientAlreadyMuted");
    WONStatus_ClientBanned_String=aConfig->SafeGetString("ClientBanned");
    WONStatus_ClientDoesNotExist_String=aConfig->SafeGetString("ClientDoesNotExist");
    WONStatus_ClientNotBanned_String=aConfig->SafeGetString("ClientNotBanned");
    WONStatus_ClientNotInGroup_String=aConfig->SafeGetString("ClientNotInGroup");
    WONStatus_ClientNotInvited_String=aConfig->SafeGetString("ClientNotInvited");
    WONStatus_ClientNotMuted_String=aConfig->SafeGetString("ClientNotMuted");
    WONStatus_ConnectFailure_String=aConfig->SafeGetString("ConnectFailure");
    WONStatus_GroupAlreadyClaimed_String=aConfig->SafeGetString("GroupAlreadyClaimed");
    WONStatus_GroupAlreadyExists_String=aConfig->SafeGetString("GroupAlreadyExists");
    WONStatus_GroupClosed_String=aConfig->SafeGetString("GroupClosed");
    WONStatus_GroupDeleted_String=aConfig->SafeGetString("GroupDeleted");
    WONStatus_GroupDoesNotExist_String=aConfig->SafeGetString("GroupDoesNotExist");
    WONStatus_GroupFull_String=aConfig->SafeGetString("GroupFull");
    WONStatus_GuestNamesReserved_String=aConfig->SafeGetString("GuestNamesReserved");
    WONStatus_InvalidPassword_String=aConfig->SafeGetString("InvalidPassword");
    WONStatus_MustBeAdmin_String=aConfig->SafeGetString("MustBeAdmin");
    WONStatus_MustBeCaptain_String=aConfig->SafeGetString("MustBeCaptain");
    WONStatus_MustBeCaptainOrModerator_String=aConfig->SafeGetString("MustBeCaptainOrModerator");
    WONStatus_MustBeModerator_String=aConfig->SafeGetString("MustBeModerator");
    WONStatus_ServerFull_String=aConfig->SafeGetString("ServerFull");
    WONStatus_GroupNameExceedsMaximum_String=aConfig->SafeGetString("GroupNameExceedsMaximum");
    WONStatus_JoinCanceled_String=aConfig->SafeGetString("JoinCanceled");
    WONStatus_Throttled_String=aConfig->SafeGetString("Throttled");
    WONStatus_KeyNotUsed_String=aConfig->SafeGetString("KeyNotUsed");
    WONStatus_EMailPasswordDuplication_String=aConfig->SafeGetString("EMailPasswordDuplication");
    WONStatus_AccountCreateDisabled_String=aConfig->SafeGetString("AccountCreateDisabled");
    WONStatus_AgeNotOldEnough_String=aConfig->SafeGetString("AgeNotOldEnough");
    WONStatus_ItemHasDependencies_String=aConfig->SafeGetString("ItemHasDependencies");
    WONStatus_OutOfDate_String=aConfig->SafeGetString("OutOfDate");
    WONStatus_OutOfDateNoUpdate_String=aConfig->SafeGetString("OutOfDateNoUpdate");
    WONStatus_ValidNotLatest_String=aConfig->SafeGetString("ValidNotLatest");
    WONStatus_DataAlreadyExists_String=aConfig->SafeGetString("DataAlreadyExists");
    WONStatus_DBError_String=aConfig->SafeGetString("DBError");
    WONStatus_ConnRefused_String=aConfig->SafeGetString("ConnRefused");
}

// FUNCTION: WON 0x0077fb50
// Releases every lobby image and sound and empties every lobby string
void LobbyResource_UnloadResources()
{
    LobbyGlobal_Cancel_String.erase();
    LobbyGlobal_Lan_String.erase();
    LobbyGlobal_No_String.erase();
    LobbyGlobal_OK_String.erase();
    LobbyGlobal_Yes_String.erase();
    LobbyGlobal_QuickPlay_String.erase();
    LobbyGlobal_Searching_Image=NULL;
    LobbyGlobal_Firewall_Image=NULL;
    LobbyGlobal_ButtonClick_Sound=NULL;
    LobbyGlobal_LobbyMusic_Sound=NULL;
    LobbyGlobal_AllReady_Sound=NULL;
    LobbyGlobal_AllNotReady_Sound=NULL;
    LobbyGlobal_AskToJoinRecv_Sound=NULL;
    LobbyGlobal_Error_Sound=NULL;
    LobbyGlobal_FriendJoined_Sound=NULL;
    LobbyGlobal_FriendLeft_Sound=NULL;
    LobbyGlobal_InviteRecv_Sound=NULL;
    LobbyGlobal_PlayerJoined_Sound=NULL;
    LobbyGlobal_SierraLogon_Sound=NULL;
    LobbyGlobal_WhisperRecv_Sound=NULL;
    AccountLogic_InvalidCDKeyTitle_String.erase();
    AccountLogic_InvalidCDKeyDetails_String.erase();
    AddFriendDialog_StatusTitle_String.erase();
    AddFriendDialog_StatusDetails_String.erase();
    AdminActionDialog_Warn_String.erase();
    AdminActionDialog_Mute_String.erase();
    AdminActionDialog_UnMute_String.erase();
    AdminActionDialog_Kick_String.erase();
    AdminActionDialog_Ban_String.erase();
    AdminActionDialog_ExplanationOpt_String.erase();
    AdminActionDialog_ExplanationReq_String.erase();
    BadUserDialog_OneDay_String.erase();
    BadUserDialog_OneHour_String.erase();
    BadUserDialog_OneMinute_String.erase();
    BadUserDialog_OneSecond_String.erase();
    BadUserDialog_XDays_String.erase();
    BadUserDialog_XHours_String.erase();
    BadUserDialog_XMinutes_String.erase();
    BadUserDialog_XSeconds_String.erase();
    BadUserDialog_Infinite_String.erase();
    ChatCommandLogic_Whisper_String.erase();
    ChatCommandLogic_Block_String.erase();
    ChatCommandLogic_Emote_String.erase();
    ChatCommandLogic_Ignore_String.erase();
    ChatCommandLogic_Away_String.erase();
    ChatCommandLogic_Invite_String.erase();
    ChatCommandLogic_Uninvite_String.erase();
    ChatCommandLogic_Reply_String.erase();
    ChatCommandLogic_Clear_String.erase();
    ChatCommandLogic_Help_String.erase();
    ChatCommandLogic_ShowTeam_String.erase();
    ChatCommandLogic_Moderator_String.erase();
    ChatCommandLogic_ServerMute_String.erase();
    ChatCommandLogic_ServerBan_String.erase();
    ChatCommandLogic_Mute_String.erase();
    ChatCommandLogic_Ban_String.erase();
    ChatCommandLogic_Unmute_String.erase();
    ChatCommandLogic_ServerUnmute_String.erase();
    ChatCommandLogic_Unban_String.erase();
    ChatCommandLogic_ServerUnban_String.erase();
    ChatCommandLogic_Alert_String.erase();
    ChatCommandLogic_Warn_String.erase();
    ChatCommandLogic_HelpHeader_String.erase();
    ChatCommandLogic_HelpHelp_String.erase();
    ChatCommandLogic_ShowTeamHelp_String.erase();
    ChatCommandLogic_WhisperHelp_String.erase();
    ChatCommandLogic_BlockHelp_String.erase();
    ChatCommandLogic_IgnoreHelp_String.erase();
    ChatCommandLogic_ReplyHelp_String.erase();
    ChatCommandLogic_EmoteHelp_String.erase();
    ChatCommandLogic_AwayHelp_String.erase();
    ChatCommandLogic_ClearHelp_String.erase();
    ChatCommandLogic_InviteHelp_String.erase();
    ChatCommandLogic_UninviteHelp_String.erase();
    ChatCommandLogic_MuteHelp_String.erase();
    ChatCommandLogic_UnmuteHelp_String.erase();
    ChatCommandLogic_BanHelp_String.erase();
    ChatCommandLogic_UnbanHelp_String.erase();
    ChatCommandLogic_ModeratorHelp_String.erase();
    ChatCommandLogic_ServerMuteHelp_String.erase();
    ChatCommandLogic_ServerUnmuteHelp_String.erase();
    ChatCommandLogic_ServerBanHelp_String.erase();
    ChatCommandLogic_ServerUnbanHelp_String.erase();
    ChatCommandLogic_AlertHelp_String.erase();
    ChatCommandLogic_WarnHelp_String.erase();
    ChatCtrl_NormalChat_String.erase();
    ChatCtrl_Emote_String.erase();
    ChatCtrl_TeamChat_String.erase();
    ChatCtrl_TeamEmote_String.erase();
    ChatCtrl_YouWhisperToYourself_String.erase();
    ChatCtrl_YouWhisper_String.erase();
    ChatCtrl_SomeoneWhispersToYou_String.erase();
    ChatCtrl_YouWarnSomeone_String.erase();
    ChatCtrl_YouWarnYourself_String.erase();
    ChatCtrl_SomeoneWarnsYou_String.erase();
    ChatCtrl_SomeoneWarnsSomeone_String.erase();
    ChatCtrl_EnteringRoom_String.erase();
    ChatCtrl_LeavingRoom_String.erase();
    ChatCtrl_Disconnected_String.erase();
    ChatCtrl_YouWereKicked_String.erase();
    ChatCtrl_YouWereBannedMinute_String.erase();
    ChatCtrl_YouWereBannedMinutes_String.erase();
    ChatCtrl_YouWereBannedHour_String.erase();
    ChatCtrl_YouWereBannedHours_String.erase();
    ChatCtrl_YouWereBannedDay_String.erase();
    ChatCtrl_YouWereBannedDays_String.erase();
    ChatCtrl_YouWereBannedIndefinitely_String.erase();
    ChatCtrl_SomeoneWasKicked_String.erase();
    ChatCtrl_SomeoneWasBannedMinute_String.erase();
    ChatCtrl_SomeoneWasBannedMinutes_String.erase();
    ChatCtrl_SomeoneWasBannedHour_String.erase();
    ChatCtrl_SomeoneWasBannedHours_String.erase();
    ChatCtrl_SomeoneWasBannedDay_String.erase();
    ChatCtrl_SomeoneWasBannedDays_String.erase();
    ChatCtrl_SomeoneWasBannedIndefinitely_String.erase();
    ChatCtrl_YouWereUnmuted_String.erase();
    ChatCtrl_YouWereMutedMinute_String.erase();
    ChatCtrl_YouWereMutedMinutes_String.erase();
    ChatCtrl_YouWereMutedHour_String.erase();
    ChatCtrl_YouWereMutedHours_String.erase();
    ChatCtrl_YouWereMutedDay_String.erase();
    ChatCtrl_YouWereMutedDays_String.erase();
    ChatCtrl_YouWereMutedIndefinitely_String.erase();
    ChatCtrl_SomeoneWasUnmuted_String.erase();
    ChatCtrl_SomeoneWasMutedMinute_String.erase();
    ChatCtrl_SomeoneWasMutedMinutes_String.erase();
    ChatCtrl_SomeoneWasMutedHour_String.erase();
    ChatCtrl_SomeoneWasMutedHours_String.erase();
    ChatCtrl_SomeoneWasMutedDay_String.erase();
    ChatCtrl_SomeoneWasMutedDays_String.erase();
    ChatCtrl_SomeoneWasMutedIndefinitely_String.erase();
    ChatCtrl_ModeratorComment_String.erase();
    ChatCtrl_AlertComment_String.erase();
    ChatCtrl_ServerAlert_String.erase();
    ChatCtrl_ServerShutdownStartedMinute_String.erase();
    ChatCtrl_ServerShutdownStartedMinutes_String.erase();
    ChatCtrl_ServerShutdownStartedHour_String.erase();
    ChatCtrl_ServerShutdownStartedHours_String.erase();
    ChatCtrl_ServerShutdownStartedDay_String.erase();
    ChatCtrl_ServerShutdownStartedDays_String.erase();
    ChatCtrl_ServerShutdownAborted_String.erase();
    ChatCtrl_SomeoneInvitesYou_String.erase();
    ChatCtrl_SomeoneUninvitesYou_String.erase();
    ChatCtrl_YouInviteSomeone_String.erase();
    ChatCtrl_YouUninviteSomeone_String.erase();
    ChatCtrl_SomeoneInvitesYouExpl_String.erase();
    ChatCtrl_SomeoneUninvitesYouExpl_String.erase();
    ChatCtrl_YouInviteSomeoneExpl_String.erase();
    ChatCtrl_YouUninviteSomeoneExpl_String.erase();
    ChatCtrl_Accept_String.erase();
    ChatCtrl_Reject_String.erase();
    ChatCtrl_SomeoneAsksToJoin_String.erase();
    ChatCtrl_SomeoneAsksToJoinExpl_String.erase();
    ChatCtrl_ClientNotOnServer_String.erase();
    ChatCtrl_ClientInGame_String.erase();
    ChatCtrl_ClientInChat_String.erase();
    ChatCtrl_ClientInGameAndChat_String.erase();
    ChatCtrl_ClientIsAnonymous_String.erase();
    ChatCtrl_FriendIsChatting_String.erase();
    ChatCtrl_FriendIsInGameStaging_String.erase();
    ChatCtrl_FriendIsPlaying_String.erase();
    ChatCtrl_FriendIsOn_String.erase();
    ChatCtrl_FriendIsNotOnline_String.erase();
    ChatCtrl_PlayerKicked_String.erase();
    ChatCtrl_PlayerBanned_String.erase();
    ChatCtrl_FriendJoined_String.erase();
    ChatCtrl_FriendLeft_String.erase();
    ChatCtrl_TeamMemberJoined_String.erase();
    ChatCtrl_TeamMemberLeft_String.erase();
    ChatCtrl_TeamMembers_String.erase();
    ChatCtrl_IgnoreList_String.erase();
    ChooseNetworkAdapterDialog_Default_String.erase();
    ChooseNetworkAdapterDialog_Unknown_String.erase();
    ClientOptionsCtrl_Whisper_String.erase();
    ClientOptionsCtrl_Invite_String.erase();
    ClientOptionsCtrl_Uninvite_String.erase();
    ClientOptionsCtrl_AddToFriends_String.erase();
    ClientOptionsCtrl_RemoveFromFriends_String.erase();
    ClientOptionsCtrl_ClearAFK_String.erase();
    ClientOptionsCtrl_SetAFK_String.erase();
    ClientOptionsCtrl_Ignore_String.erase();
    ClientOptionsCtrl_Unignore_String.erase();
    ClientOptionsCtrl_Block_String.erase();
    ClientOptionsCtrl_Unblock_String.erase();
    ClientOptionsCtrl_Warn_String.erase();
    ClientOptionsCtrl_Mute_String.erase();
    ClientOptionsCtrl_UnMute_String.erase();
    ClientOptionsCtrl_Kick_String.erase();
    ClientOptionsCtrl_Ban_String.erase();
    DirectConnectionOptionsCtrl_Connect_String.erase();
    DirectConnectionOptionsCtrl_Refresh_String.erase();
    DirectConnectionOptionsCtrl_Remove_String.erase();
    FriendOptionsCtrl_Locate_String.erase();
    FriendOptionsCtrl_RefreshList_String.erase();
    FriendOptionsCtrl_RemoveFromFriends_String.erase();
    FriendsListCtrl_Unknown_Image=NULL;
    FriendsListCtrl_NotFound_Image=NULL;
    FriendsListCtrl_InChat_Image=NULL;
    FriendsListCtrl_InGame_Image=NULL;
    GameOptionsPopup_Join_String.erase();
    GameOptionsPopup_Ping_String.erase();
    GameOptionsPopup_QueryDetails_String.erase();
    GameOptionsPopup_HideDetails_String.erase();
    GameOptionsPopup_ShowDetails_String.erase();
    GameOptionsPopup_Remove_String.erase();
    GameStagingCtrl_Ready_String.erase();
    GameStagingCtrl_NotReady_String.erase();
    GameStagingCtrl_StartGame_String.erase();
    GameStagingCtrl_LeaveGame_String.erase();
    GameStagingCtrl_DissolveGame_String.erase();
    GameStagingLogic_KickedTitle_String.erase();
    GameStagingLogic_KickedDetails_String.erase();
    GameStagingLogic_BannedTitle_String.erase();
    GameStagingLogic_BannedDetails_String.erase();
    GameStagingLogic_ConnectWarningTitle1_String.erase();
    GameStagingLogic_ConnectWarningDetails1_String.erase();
    GameStagingLogic_ConnectWarningTitle2_String.erase();
    GameStagingLogic_ConnectWarningDetails2_String.erase();
    GameStagingLogic_ConnectWarningTitle3_String.erase();
    GameStagingLogic_ConnectWarningDetails3_String.erase();
    GameStagingLogic_NoPingReplyError_String.erase();
    GameStagingLogic_GettingGameInfoTitle_String.erase();
    GameStagingLogic_GettingGameInfoDetails_String.erase();
    GameStagingLogic_JoinGameErrorTitle_String.erase();
    GameStagingLogic_VerifyingPingTitle_String.erase();
    GameStagingLogic_VerifyingPingDetails_String.erase();
    GameStagingLogic_DissolveGame_String.erase();
    GameStagingLogic_DissolveGameConfirm_String.erase();
    GameStagingLogic_LeaveGame_String.erase();
    GameStagingLogic_LeaveGameConfirm_String.erase();
    GameStagingLogic_NoQuickPlayGameTitle_String.erase();
    GameStagingLogic_NoQuickPlayGameDetails_String.erase();
    GameUserListCtrl_Ban_String.erase();
    GameUserListCtrl_Kick_String.erase();
    GameUserListCtrl_Ready_Image=NULL;
    InternetQuickPlay_Welcome_String.erase();
    InternetQuickPlay_UserWelcome_String.erase();
    JoinResponse_Joined_String.erase();
    JoinResponse_ServerFull_String.erase();
    JoinResponse_InviteOnly_String.erase();
    JoinResponse_AskToJoin_String.erase();
    JoinResponse_GameInProgress_String.erase();
    JoinResponse_InvalidPassword_String.erase();
    JoinResponse_NotOnLAN_String.erase();
    JoinResponse_Blocked_String.erase();
    JoinResponse_Unknown_String.erase();
    LANLogic_SocketBindError_String.erase();
    LANLogic_SocketListenError_String.erase();
    LANLogic_ConnectToGameTitle_String.erase();
    LANLogic_ConnectToGameDetails_String.erase();
    LANLogic_CheckingAddressTitle_String.erase();
    LANLogic_CheckingAddressDetails_String.erase();
    LANLogic_RegisterTitle_String.erase();
    LANLogic_RegisterDetails_String.erase();
    LANLogic_JoinFailure_String.erase();
    LANLogic_JoinReplyError_String.erase();
    LANLogic_InvalidSubnet_String.erase();
    LobbyContainer_InitStart_String.erase();
    LobbyContainer_InitFailed_String.erase();
    LobbyContainer_CheckingVersion_String.erase();
    LobbyContainer_QueryingServers_String.erase();
    LobbyContainer_NoConnection_String.erase();
    LobbyContainer_DNSLookupFailed_String.erase();
    LobbyContainer_DirLookupFailure_String.erase();
    LobbyContainer_VersionCheckFailure_String.erase();
    LobbyContainer_InitComplete_String.erase();
    LobbyContainer_NoServers_String.erase();
    LobbyContainer_AuthServers_String.erase();
    LobbyContainer_AccountServers_String.erase();
    LobbyContainer_VersionServers_String.erase();
    LobbyContainer_CreateAccountTitle_String.erase();
    LobbyContainer_CreateAccountDetails_String.erase();
    LobbyContainer_RetrievePasswordTitle_String.erase();
    LobbyContainer_RetrievePasswordDetails_String.erase();
    LobbyContainer_RetrieveUsernameTitle_String.erase();
    LobbyContainer_RetrieveUsernameDetails_String.erase();
    LobbyContainer_QueryAccountTitle_String.erase();
    LobbyContainer_QueryAccountDetails_String.erase();
    LobbyContainer_UpdateAccountTitle_String.erase();
    LobbyContainer_UpdateAccountDetails_String.erase();
    LobbyContainer_LoginTitle_String.erase();
    LobbyContainer_LoginDetails_String.erase();
    LobbyContainer_GettingRoomsTitle_String.erase();
    LobbyContainer_GettingRoomsDetails_String.erase();
    LobbyContainer_VersionOutOfDataTitle_String.erase();
    LobbyContainer_VersionOutOfDataDetails_String.erase();
    LobbyGameStatus_Success_String.erase();
    LobbyGameStatus_GameFull_String.erase();
    LobbyGameStatus_GameInProgress_String.erase();
    LobbyGameStatus_UnpackFailure_String.erase();
    LobbyGameStatus_NotInvited_String.erase();
    LobbyGameStatus_CaptainRejectedYou_String.erase();
    LobbyGameStatus_InvalidPassword_String.erase();
    LobbyGameStatus_DuplicateName_String.erase();
    LobbyOptions_DefaultName_String.erase();
    LobbyOptions_CheckingAddress_String.erase();
    LobbyOptions_AddressUnavailable_String.erase();
    RoomCtrl_QueryingServer_String.erase();
    RoomCtrl_Password_Image=NULL;
    RoomLabelCtrl_LabelPrefix_String.erase();
    RoomLabelCtrl_LabelServer_String.erase();
    RoomLabelCtrl_LabelUser_String.erase();
    RoomLabelCtrl_LabelUsers_String.erase();
    RoomLabelCtrl_LabelDisconnected_String.erase();
    RoutingLogic_GetClientListTitle_String.erase();
    RoutingLogic_GetClientListDetails_String.erase();
    RoutingLogic_GetGroupListTitle_String.erase();
    RoutingLogic_GetGroupListDetails_String.erase();
    RoutingLogic_GameDissolvedTitle_String.erase();
    RoutingLogic_GameDissolvedDetails_String.erase();
    RoutingLogic_NoServersTitle_String.erase();
    RoutingLogic_NoServersDetails_String.erase();
    RoutingLogic_TooManyFriendsTitle_String.erase();
    RoutingLogic_TooManyFriendsDetails_String.erase();
    RoutingLogic_AlreadyJoinedTitle_String.erase();
    RoutingLogic_AlreadyInGame_String.erase();
    RoutingLogic_AlreadyInRoom_String.erase();
    RoutingLogic_JoinGroupTitle_String.erase();
    RoutingLogic_JoinGroupDetails_String.erase();
    RoutingLogic_ConnectTitle_String.erase();
    RoutingLogic_ConnectDetails_String.erase();
    RoutingLogic_RegisterTitle_String.erase();
    RoutingLogic_RegisterDetails_String.erase();
    RoutingLogic_QueryGroupsTitle_String.erase();
    RoutingLogic_QueryGroupsDetails_String.erase();
    RoutingLogic_CreateGroupTitle_String.erase();
    RoutingLogic_CreateGroupDetails_String.erase();
    RoutingLogic_GetBadUsersTitle_String.erase();
    RoutingLogic_GetBadUsersDetails_String.erase();
    RoutingLogic_DisconnectedTitle_String.erase();
    RoutingLogic_DisconnectedDetails_String.erase();
    RoutingLogic_AskingCaptainTitle_String.erase();
    RoutingLogic_AskingCaptainDetails_String.erase();
    RoutingLogic_CollectingPingsTitle_String.erase();
    RoutingLogic_CollectingPingsDetails_String.erase();
    RoutingLogic_GettingGameListTitle_String.erase();
    RoutingLogic_GettingGameListDetails_String.erase();
    RoutingLogic_MuteClientTitle_String.erase();
    RoutingLogic_UnmuteClientTitle_String.erase();
    RoutingLogic_BanClientTitle_String.erase();
    RoutingLogic_KickClientTitle_String.erase();
    RoutingLogic_ModerateDetails_String.erase();
    RoutingLogic_FriendNotFoundTitle_String.erase();
    RoutingLogic_FriendNotFoundDetails_String.erase();
    RoutingLogic_ClientNotFound_String.erase();
    RoutingLogic_ClientAmbiguous_String.erase();
    RoutingLogic_InvalidCommand_String.erase();
    RoutingLogic_RequireName_String.erase();
    RoutingLogic_BadTimeSpec_String.erase();
    RoutingLogic_Error_String.erase();
    RoutingLogic_NotCaptain_String.erase();
    ServerListCtrl_Pinging_String.erase();
    ServerListCtrl_PingFailed_String.erase();
    ServerListCtrl_Querying_String.erase();
    ServerListCtrl_QueryFailed_String.erase();
    ServerListCtrl_ListSummary_String.erase();
    ServerListCtrl_ListPingSummary_String.erase();
    ServerListCtrl_RefreshList_String.erase();
    ServerListCtrl_StopRefresh_String.erase();
    ServerListCtrl_NoFilter_String.erase();
    ServerListCtrl_AddRemove_String.erase();
    ServerListCtrl_Add_String.erase();
    ServerListCtrl_Remove_String.erase();
    ServerListCtrl_FullFilter_String.erase();
    ServerListCtrl_NotFullFilter_String.erase();
    ServerListCtrl_OpenFilter_String.erase();
    ServerListCtrl_PingFailed_Image=NULL;
    ServerListCtrl_GameExpanded_Image=NULL;
    ServerListCtrl_GameNotExpanded_Image=NULL;
    ServerListCtrl_GameAreInvited_Image=NULL;
    ServerListCtrl_GameNone_Image=NULL;
    ServerListCtrl_GameAskToJoin_Image=NULL;
    ServerListCtrl_GameHasPassword_Image=NULL;
    ServerListCtrl_GameNotInvited_Image=NULL;
    ServerListCtrl_GameInProgress_Image=NULL;
    ServerListCtrl_SkillOpen_Image=NULL;
    ServerListCtrl_SkillNovice_Image=NULL;
    ServerListCtrl_SkillIntermediate_Image=NULL;
    ServerListCtrl_SkillAdvanced_Image=NULL;
    ServerListCtrl_SkillExpert_Image=NULL;
    ServerOptionsCtrl_Connect_String.erase();
    ServerOptionsCtrl_RefreshServer_String.erase();
    ServerOptionsCtrl_HideDetails_String.erase();
    ServerOptionsCtrl_ShowDetails_String.erase();
    ServerOptionsCtrl_ResfreshList_String.erase();
    StagingLogic_DisconnectedTitle_String.erase();
    StagingLogic_DisconnectedDetails_String.erase();
    StagingLogic_BannedTitle_String.erase();
    StagingLogic_BannedDetails_String.erase();
    StagingLogic_KickedTitle_String.erase();
    StagingLogic_KickedDetails_String.erase();
    UserListCtrl_Ignore_String.erase();
    UserListCtrl_Unignore_String.erase();
    UserListCtrl_Normal_Image=NULL;
    UserListCtrl_Away_Image=NULL;
    UserListCtrl_Admin_Image=NULL;
    UserListCtrl_Moderator_Image=NULL;
    UserListCtrl_Captain_Image=NULL;
    UserListCtrl_Muted_Image=NULL;
    UserListCtrl_Ignored_Image=NULL;
    UserListCtrl_Blocked_Image=NULL;
    WONStatus_Success_String.erase();
    WONStatus_Killed_String.erase();
    WONStatus_TimedOut_String.erase();
    WONStatus_InvalidAddress_String.erase();
    WONStatus_GeneralFailure_String.erase();
    WONStatus_AuthFailure_String.erase();
    WONStatus_FTPFailure_String.erase();
    WONStatus_HTTPFailure_String.erase();
    WONStatus_MOTDFailure_String.erase();
    WONStatus_RoutingFailure_String.erase();
    WONStatus_GameSpyFailure_String.erase();
    WONStatus_DirFailure_String.erase();
    WONStatus_ChatFailure_String.erase();
    WONStatus_DatabaseFailure_String.erase();
    WONStatus_NotAllowed_String.erase();
    WONStatus_NotAuthorized_String.erase();
    WONStatus_BehindFirewall_String.erase();
    WONStatus_ExpiredCertificate_String.erase();
    WONStatus_InvalidCDKey_String.erase();
    WONStatus_CDKeyBanned_String.erase();
    WONStatus_CDKeyInUse_String.erase();
    WONStatus_CRCFailed_String.erase();
    WONStatus_UserExists_String.erase();
    WONStatus_UserNotFound_String.erase();
    WONStatus_BadPassword_String.erase();
    WONStatus_InvalidUserName_String.erase();
    WONStatus_BadCommunity_String.erase();
    WONStatus_NotInCommunity_String.erase();
    WONStatus_UserSeqInUse_String.erase();
    WONStatus_BlankNamesNotAllowed_String.erase();
    WONStatus_CaptainRejectedYou_String.erase();
    WONStatus_ClientAlreadyBanned_String.erase();
    WONStatus_ClientAlreadyInGroup_String.erase();
    WONStatus_ClientAlreadyMuted_String.erase();
    WONStatus_ClientBanned_String.erase();
    WONStatus_ClientDoesNotExist_String.erase();
    WONStatus_ClientNotBanned_String.erase();
    WONStatus_ClientNotInGroup_String.erase();
    WONStatus_ClientNotInvited_String.erase();
    WONStatus_ClientNotMuted_String.erase();
    WONStatus_ConnectFailure_String.erase();
    WONStatus_GroupAlreadyClaimed_String.erase();
    WONStatus_GroupAlreadyExists_String.erase();
    WONStatus_GroupClosed_String.erase();
    WONStatus_GroupDeleted_String.erase();
    WONStatus_GroupDoesNotExist_String.erase();
    WONStatus_GroupFull_String.erase();
    WONStatus_GuestNamesReserved_String.erase();
    WONStatus_InvalidPassword_String.erase();
    WONStatus_MustBeAdmin_String.erase();
    WONStatus_MustBeCaptain_String.erase();
    WONStatus_MustBeCaptainOrModerator_String.erase();
    WONStatus_MustBeModerator_String.erase();
    WONStatus_ServerFull_String.erase();
    WONStatus_GroupNameExceedsMaximum_String.erase();
    WONStatus_JoinCanceled_String.erase();
    WONStatus_Throttled_String.erase();
    WONStatus_KeyNotUsed_String.erase();
    WONStatus_EMailPasswordDuplication_String.erase();
    WONStatus_AccountCreateDisabled_String.erase();
    WONStatus_AgeNotOldEnough_String.erase();
    WONStatus_ItemHasDependencies_String.erase();
    WONStatus_OutOfDate_String.erase();
    WONStatus_OutOfDateNoUpdate_String.erase();
    WONStatus_ValidNotLatest_String.erase();
    WONStatus_DataAlreadyExists_String.erase();
    WONStatus_DBError_String.erase();
    WONStatus_ConnRefused_String.erase();
}

// TEMPLATE: WON 0x00781810
// (GUIString::erase)
