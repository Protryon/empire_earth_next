//! The routing op type ids, by the names the C++ uses (`RoutingOp_JoinGroup`, ...): the C++ `RoutingOpType` (a
//! `long`) values, read through `rust/won_net/routeop_types.h` (until R6 WONLobby round 30 they were
//! WONRoutingOpTypes.h's enum; the header went in R6 WON round 39). The names are RoutingOpTypeToString's (0x4cd4f9), each
//! the type its op class's GetType returned, numbered from 0 in that order.

#![allow(non_upper_case_globals)]

pub const RoutingOp_Undefined: i32 = 0;
pub const RoutingOp_AbortServerShutdown: i32 = 1;
pub const RoutingOp_AcceptClient: i32 = 2;
pub const RoutingOp_BanClient: i32 = 3;
pub const RoutingOp_BecomeModerator: i32 = 4;
pub const RoutingOp_CancelJoinGroup: i32 = 5;
pub const RoutingOp_ClientFlagsChanged: i32 = 6;
pub const RoutingOp_ClientJoinedGroup: i32 = 7;
pub const RoutingOp_ClientJoinedServer: i32 = 8;
pub const RoutingOp_ClientLeftGroup: i32 = 9;
pub const RoutingOp_ClientLeftServer: i32 = 10;
pub const RoutingOp_CreateDataObject: i32 = 11;
pub const RoutingOp_CreateGroup: i32 = 12;
pub const RoutingOp_DataObjectCreated: i32 = 13;
pub const RoutingOp_DataObjectDeleted: i32 = 14;
pub const RoutingOp_DataObjectModified: i32 = 15;
pub const RoutingOp_DeleteDataObject: i32 = 16;
pub const RoutingOp_DetectFirewall: i32 = 17;
pub const RoutingOp_DisconnectClient: i32 = 18;
pub const RoutingOp_GetBadUserList: i32 = 19;
pub const RoutingOp_GetClientList: i32 = 20;
pub const RoutingOp_GetGroupList: i32 = 21;
pub const RoutingOp_GetMembersOfGroup: i32 = 22;
pub const RoutingOp_GroupCaptainChanged: i32 = 23;
pub const RoutingOp_GroupCreated: i32 = 24;
pub const RoutingOp_GroupDeleted: i32 = 25;
pub const RoutingOp_GroupFlagsChanged: i32 = 26;
pub const RoutingOp_GroupInvitation: i32 = 27;
pub const RoutingOp_GroupJoinAttempt: i32 = 28;
pub const RoutingOp_GroupMaxPlayersChanged: i32 = 29;
pub const RoutingOp_GroupMemberCount: i32 = 30;
pub const RoutingOp_GroupNameChanged: i32 = 31;
pub const RoutingOp_GroupObserverCount: i32 = 32;
pub const RoutingOp_InviteClient: i32 = 33;
pub const RoutingOp_JoinGroup: i32 = 34;
pub const RoutingOp_LeaveGroup: i32 = 35;
pub const RoutingOp_MemberFlagsChanged: i32 = 36;
pub const RoutingOp_ModifyDataObject: i32 = 37;
pub const RoutingOp_MuteClient: i32 = 38;
pub const RoutingOp_PeerChat: i32 = 39;
pub const RoutingOp_PeerData: i32 = 40;
pub const RoutingOp_ReadDataObject: i32 = 41;
pub const RoutingOp_RegisterClient: i32 = 42;
pub const RoutingOp_RelinquishCaptaincy: i32 = 43;
pub const RoutingOp_SendChat: i32 = 44;
pub const RoutingOp_SendComplaint: i32 = 45;
pub const RoutingOp_SendData: i32 = 46;
pub const RoutingOp_SendServerAlert: i32 = 47;
pub const RoutingOp_ServerAlert: i32 = 48;
pub const RoutingOp_ServerShutdownAborted: i32 = 49;
pub const RoutingOp_ServerShutdownStarted: i32 = 50;
pub const RoutingOp_SetClientFlags: i32 = 51;
pub const RoutingOp_SetGroupFlags: i32 = 52;
pub const RoutingOp_SetGroupMaxPlayers: i32 = 53;
pub const RoutingOp_SetGroupName: i32 = 54;
pub const RoutingOp_SetGroupPassword: i32 = 55;
pub const RoutingOp_StartServerShutdown: i32 = 56;
pub const RoutingOp_UnsubscribeDataObject: i32 = 57;
pub const RoutingOp_YouWereBanned: i32 = 58;
pub const RoutingOp_YouWereMuted: i32 = 59;
pub const RoutingOp_DerivedServerOp: i32 = 60;

/// Every name and value, in the header's order (the tests')
#[cfg(test)]
pub(crate) const ALL: &[(&str, i32)] = &[
    ("RoutingOp_Undefined", RoutingOp_Undefined),
    ("RoutingOp_AbortServerShutdown", RoutingOp_AbortServerShutdown),
    ("RoutingOp_AcceptClient", RoutingOp_AcceptClient),
    ("RoutingOp_BanClient", RoutingOp_BanClient),
    ("RoutingOp_BecomeModerator", RoutingOp_BecomeModerator),
    ("RoutingOp_CancelJoinGroup", RoutingOp_CancelJoinGroup),
    ("RoutingOp_ClientFlagsChanged", RoutingOp_ClientFlagsChanged),
    ("RoutingOp_ClientJoinedGroup", RoutingOp_ClientJoinedGroup),
    ("RoutingOp_ClientJoinedServer", RoutingOp_ClientJoinedServer),
    ("RoutingOp_ClientLeftGroup", RoutingOp_ClientLeftGroup),
    ("RoutingOp_ClientLeftServer", RoutingOp_ClientLeftServer),
    ("RoutingOp_CreateDataObject", RoutingOp_CreateDataObject),
    ("RoutingOp_CreateGroup", RoutingOp_CreateGroup),
    ("RoutingOp_DataObjectCreated", RoutingOp_DataObjectCreated),
    ("RoutingOp_DataObjectDeleted", RoutingOp_DataObjectDeleted),
    ("RoutingOp_DataObjectModified", RoutingOp_DataObjectModified),
    ("RoutingOp_DeleteDataObject", RoutingOp_DeleteDataObject),
    ("RoutingOp_DetectFirewall", RoutingOp_DetectFirewall),
    ("RoutingOp_DisconnectClient", RoutingOp_DisconnectClient),
    ("RoutingOp_GetBadUserList", RoutingOp_GetBadUserList),
    ("RoutingOp_GetClientList", RoutingOp_GetClientList),
    ("RoutingOp_GetGroupList", RoutingOp_GetGroupList),
    ("RoutingOp_GetMembersOfGroup", RoutingOp_GetMembersOfGroup),
    ("RoutingOp_GroupCaptainChanged", RoutingOp_GroupCaptainChanged),
    ("RoutingOp_GroupCreated", RoutingOp_GroupCreated),
    ("RoutingOp_GroupDeleted", RoutingOp_GroupDeleted),
    ("RoutingOp_GroupFlagsChanged", RoutingOp_GroupFlagsChanged),
    ("RoutingOp_GroupInvitation", RoutingOp_GroupInvitation),
    ("RoutingOp_GroupJoinAttempt", RoutingOp_GroupJoinAttempt),
    ("RoutingOp_GroupMaxPlayersChanged", RoutingOp_GroupMaxPlayersChanged),
    ("RoutingOp_GroupMemberCount", RoutingOp_GroupMemberCount),
    ("RoutingOp_GroupNameChanged", RoutingOp_GroupNameChanged),
    ("RoutingOp_GroupObserverCount", RoutingOp_GroupObserverCount),
    ("RoutingOp_InviteClient", RoutingOp_InviteClient),
    ("RoutingOp_JoinGroup", RoutingOp_JoinGroup),
    ("RoutingOp_LeaveGroup", RoutingOp_LeaveGroup),
    ("RoutingOp_MemberFlagsChanged", RoutingOp_MemberFlagsChanged),
    ("RoutingOp_ModifyDataObject", RoutingOp_ModifyDataObject),
    ("RoutingOp_MuteClient", RoutingOp_MuteClient),
    ("RoutingOp_PeerChat", RoutingOp_PeerChat),
    ("RoutingOp_PeerData", RoutingOp_PeerData),
    ("RoutingOp_ReadDataObject", RoutingOp_ReadDataObject),
    ("RoutingOp_RegisterClient", RoutingOp_RegisterClient),
    ("RoutingOp_RelinquishCaptaincy", RoutingOp_RelinquishCaptaincy),
    ("RoutingOp_SendChat", RoutingOp_SendChat),
    ("RoutingOp_SendComplaint", RoutingOp_SendComplaint),
    ("RoutingOp_SendData", RoutingOp_SendData),
    ("RoutingOp_SendServerAlert", RoutingOp_SendServerAlert),
    ("RoutingOp_ServerAlert", RoutingOp_ServerAlert),
    ("RoutingOp_ServerShutdownAborted", RoutingOp_ServerShutdownAborted),
    ("RoutingOp_ServerShutdownStarted", RoutingOp_ServerShutdownStarted),
    ("RoutingOp_SetClientFlags", RoutingOp_SetClientFlags),
    ("RoutingOp_SetGroupFlags", RoutingOp_SetGroupFlags),
    ("RoutingOp_SetGroupMaxPlayers", RoutingOp_SetGroupMaxPlayers),
    ("RoutingOp_SetGroupName", RoutingOp_SetGroupName),
    ("RoutingOp_SetGroupPassword", RoutingOp_SetGroupPassword),
    ("RoutingOp_StartServerShutdown", RoutingOp_StartServerShutdown),
    ("RoutingOp_UnsubscribeDataObject", RoutingOp_UnsubscribeDataObject),
    ("RoutingOp_YouWereBanned", RoutingOp_YouWereBanned),
    ("RoutingOp_YouWereMuted", RoutingOp_YouWereMuted),
    ("RoutingOp_DerivedServerOp", RoutingOp_DerivedServerOp),
];

#[cfg(test)]
mod tests {
    use super::ALL;

    /// The ids are 0..=60 in order, each named once.
    #[test]
    fn the_ids_count_from_zero() {
        assert_eq!(ALL.len(), 61);
        for (i, &(name, value)) in ALL.iter().enumerate() {
            assert_eq!(value, i as i32, "{name}");
            assert!(name.starts_with("RoutingOp_"));
        }
    }
}
