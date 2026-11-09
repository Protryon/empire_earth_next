// ee/wonapi/WONRoutingOpTypes.h. Named integer constants; names PROVISIONAL unless the comment says T1.
#ifndef EE_WONROUTINGOPTYPES_H
#define EE_WONROUTINGOPTYPES_H

// the routing op type ids (RoutingOpType itself is `typedef long`, the type the mangled names use); names = RoutingOpTypeToString 0x4cd4f9's strings
enum RoutingOpTypeCode {
    RoutingOp_Undefined = 0,              // T1 RoutingOpTypeToString 0x4cd4f9
    RoutingOp_AbortServerShutdown = 1,    // T1 RoutingOpTypeToString 0x4cd4f9; RoutingAbortServerShutdownOp::GetType returns it
    RoutingOp_AcceptClient = 2,           // T1 RoutingOpTypeToString 0x4cd4f9; RoutingAcceptClientOp::GetType returns it
    RoutingOp_BanClient = 3,              // T1 RoutingOpTypeToString 0x4cd4f9; RoutingBanClientOp::GetType returns it
    RoutingOp_BecomeModerator = 4,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingBecomeModeratorOp::GetType returns it
    RoutingOp_CancelJoinGroup = 5,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingCancelJoinGroupOp::GetType returns it
    RoutingOp_ClientFlagsChanged = 6,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingClientFlagsChangedOp::GetType returns it
    RoutingOp_ClientJoinedGroup = 7,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingClientJoinedGroupOp::GetType returns it
    RoutingOp_ClientJoinedServer = 8,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingClientJoinedServerOp::GetType returns it
    RoutingOp_ClientLeftGroup = 9,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingClientLeftGroupOp::GetType returns it
    RoutingOp_ClientLeftServer = 10,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingClientLeftServerOp::GetType returns it
    RoutingOp_CreateDataObject = 11,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingCreateDataObjectOp::GetType returns it
    RoutingOp_CreateGroup = 12,           // T1 RoutingOpTypeToString 0x4cd4f9; RoutingCreateGroupOp::GetType returns it
    RoutingOp_DataObjectCreated = 13,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDataObjectCreatedOp::GetType returns it
    RoutingOp_DataObjectDeleted = 14,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDataObjectDeletedOp::GetType returns it
    RoutingOp_DataObjectModified = 15,    // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDataObjectModifiedOp::GetType returns it
    RoutingOp_DeleteDataObject = 16,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDeleteDataObjectOp::GetType returns it
    RoutingOp_DetectFirewall = 17,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDetectFirewallOp::GetType returns it
    RoutingOp_DisconnectClient = 18,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDisconnectClientOp::GetType returns it
    RoutingOp_GetBadUserList = 19,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGetBadUserListOp::GetType returns it
    RoutingOp_GetClientList = 20,         // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGetClientListOp::GetType returns it
    RoutingOp_GetGroupList = 21,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGetGroupListOp::GetType returns it
    RoutingOp_GetMembersOfGroup = 22,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGetMembersOfGroupOp::GetType returns it
    RoutingOp_GroupCaptainChanged = 23,   // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupCaptainChangedOp::GetType returns it
    RoutingOp_GroupCreated = 24,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupCreatedOp::GetType returns it
    RoutingOp_GroupDeleted = 25,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupDeletedOp::GetType returns it
    RoutingOp_GroupFlagsChanged = 26,     // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupFlagsChangedOp::GetType returns it
    RoutingOp_GroupInvitation = 27,       // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupInvitationOp::GetType returns it
    RoutingOp_GroupJoinAttempt = 28,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupJoinAttemptOp::GetType returns it
    RoutingOp_GroupMaxPlayersChanged = 29,  // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupMaxPlayersChangedOp::GetType returns it
    RoutingOp_GroupMemberCount = 30,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupMemberCountOp::GetType returns it
    RoutingOp_GroupNameChanged = 31,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupNameChangedOp::GetType returns it
    RoutingOp_GroupObserverCount = 32,    // T1 RoutingOpTypeToString 0x4cd4f9; RoutingGroupObserverCountOp::GetType returns it
    RoutingOp_InviteClient = 33,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingInviteClientOp::GetType returns it
    RoutingOp_JoinGroup = 34,             // T1 RoutingOpTypeToString 0x4cd4f9; RoutingJoinGroupOp::GetType returns it
    RoutingOp_LeaveGroup = 35,            // T1 RoutingOpTypeToString 0x4cd4f9; RoutingLeaveGroupOp::GetType returns it
    RoutingOp_MemberFlagsChanged = 36,    // T1 RoutingOpTypeToString 0x4cd4f9; RoutingMemberFlagsChangedOp::GetType returns it
    RoutingOp_ModifyDataObject = 37,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingModifyDataObjectOp::GetType returns it
    RoutingOp_MuteClient = 38,            // T1 RoutingOpTypeToString 0x4cd4f9; RoutingMuteClientOp::GetType returns it
    RoutingOp_PeerChat = 39,              // T1 RoutingOpTypeToString 0x4cd4f9; RoutingPeerChatOp::GetType returns it
    RoutingOp_PeerData = 40,              // T1 RoutingOpTypeToString 0x4cd4f9; RoutingPeerDataOp::GetType returns it
    RoutingOp_ReadDataObject = 41,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingReadDataObjectOp::GetType returns it
    RoutingOp_RegisterClient = 42,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingRegisterClientOp::GetType returns it
    RoutingOp_RelinquishCaptaincy = 43,   // T1 RoutingOpTypeToString 0x4cd4f9; RoutingRelinquishCaptaincyOp::GetType returns it
    RoutingOp_SendChat = 44,              // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSendChatOp::GetType returns it
    RoutingOp_SendComplaint = 45,         // T1 RoutingOpTypeToString 0x4cd4f9
    RoutingOp_SendData = 46,              // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSendDataOp::GetType returns it
    RoutingOp_SendServerAlert = 47,       // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSendServerAlertOp::GetType returns it
    RoutingOp_ServerAlert = 48,           // T1 RoutingOpTypeToString 0x4cd4f9; RoutingServerAlertOp::GetType returns it
    RoutingOp_ServerShutdownAborted = 49,  // T1 RoutingOpTypeToString 0x4cd4f9; RoutingServerShutdownAbortedOp::GetType returns it
    RoutingOp_ServerShutdownStarted = 50,  // T1 RoutingOpTypeToString 0x4cd4f9; RoutingServerShutdownStartedOp::GetType returns it
    RoutingOp_SetClientFlags = 51,        // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSetClientFlagsOp::GetType returns it
    RoutingOp_SetGroupFlags = 52,         // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSetGroupFlagsOp::GetType returns it
    RoutingOp_SetGroupMaxPlayers = 53,    // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSetGroupMaxPlayersOp::GetType returns it
    RoutingOp_SetGroupName = 54,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSetGroupNameOp::GetType returns it
    RoutingOp_SetGroupPassword = 55,      // T1 RoutingOpTypeToString 0x4cd4f9; RoutingSetGroupPasswordOp::GetType returns it
    RoutingOp_StartServerShutdown = 56,   // T1 RoutingOpTypeToString 0x4cd4f9; RoutingStartServerShutdownOp::GetType returns it
    RoutingOp_UnsubscribeDataObject = 57,  // T1 RoutingOpTypeToString 0x4cd4f9; RoutingUnsubscribeDataObjectOp::GetType returns it
    RoutingOp_YouWereBanned = 58,         // T1 RoutingOpTypeToString 0x4cd4f9; RoutingYouWereBannedOp::GetType returns it
    RoutingOp_YouWereMuted = 59,          // T1 RoutingOpTypeToString 0x4cd4f9; RoutingYouWereMutedOp::GetType returns it
    RoutingOp_DerivedServerOp = 60,       // T1 RoutingOpTypeToString 0x4cd4f9; RoutingDerivedServerOp::GetType returns it
};

#endif // EE_WONROUTINGOPTYPES_H
