//! The player-facing text of a WON status (R6 WONLobby round 7; the original `GetWONStatusString`,
//! WONStatusString.cpp): which lobby resource string a status shows, and whether it is a general failure text
//! the status's own description is formatted into (`LobbyFormatString(text, WONStatusToString(status))`). The C++
//! keeps the formatting; this is the original's switch, its labels as numbers (WONStatus.h's values; the names of
//! each group in its comment). `won_statusstr_test` compares it with the original switch over every status from
//! -200,000 to 200,000.

use crate::resource::EeWonLobbyRes;
use crate::resource::EeWonLobbyRes::*;

/// A status's text: the resource string, and whether it is a general text formatted with the status's
/// description.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeWonStatusLobbyText {
    pub res: EeWonLobbyRes,
    pub general: bool,
}

/// The original switch: `(resource, general)` for `status` (its labels kept one by one).
#[allow(clippy::manual_range_patterns)]
pub fn status_text(status: i32) -> (EeWonLobbyRes, bool) {
    match status {
        -2803 => (WONStatus_UserExists_String, false),
        -2804 => (WONStatus_InvalidUserName_String, false),
        -2805 => (WONStatus_CDKeyInUse_String, false),
        -2806 => (WONStatus_UserNotFound_String, false),
        -2808 => (WONStatus_KeyNotUsed_String, false),
        -2809 => (WONStatus_EMailPasswordDuplication_String, false),
        -2810 => (WONStatus_AccountCreateDisabled_String, false),
        -2811 => (WONStatus_BadPassword_String, false),
        -2812 => (WONStatus_AgeNotOldEnough_String, false),
        -2813 => (WONStatus_ItemHasDependencies_String, false),
        -2814 => (WONStatus_OutOfDate_String, false),
        -2815 => (WONStatus_OutOfDateNoUpdate_String, false),
        -2816 => (WONStatus_ValidNotLatest_String, false),
        -2817 => (WONStatus_DataAlreadyExists_String, false),
        -2800 => (WONStatus_DBError_String, false),
        -2802 | -2801 => (WONStatus_DatabaseFailure_String, true),
        -2552 => (WONStatus_JoinCanceled_String, false),
        -2542 => (WONStatus_ServerFull_String, false),
        -2551 => (WONStatus_GroupNameExceedsMaximum_String, false),
        -2548 => (WONStatus_Throttled_String, false),
        -2532 => (WONStatus_MustBeModerator_String, false),
        -2500 => (WONStatus_BlankNamesNotAllowed_String, false),
        -2501 => (WONStatus_CaptainRejectedYou_String, false),
        -2502 => (WONStatus_ClientAlreadyBanned_String, false),
        -2504 => (WONStatus_ClientAlreadyInGroup_String, false),
        -2505 => (WONStatus_ClientAlreadyMuted_String, false),
        -2507 => (WONStatus_ClientBanned_String, false),
        -2508 => (WONStatus_ClientDoesNotExist_String, false),
        -2509 => (WONStatus_ClientNotBanned_String, false),
        -2510 => (WONStatus_ClientNotInGroup_String, false),
        -2511 => (WONStatus_ClientNotInvited_String, false),
        -2512 => (WONStatus_ClientNotMuted_String, false),
        -2514 => (WONStatus_ConnectFailure_String, false),
        -2516 => (WONStatus_GroupAlreadyClaimed_String, false),
        -2517 => (WONStatus_GroupAlreadyExists_String, false),
        -2518 => (WONStatus_GroupClosed_String, false),
        -2519 => (WONStatus_GroupDeleted_String, false),
        -2520 => (WONStatus_GroupDoesNotExist_String, false),
        -2521 => (WONStatus_GroupFull_String, false),
        -2522 => (WONStatus_GuestNamesReserved_String, false),
        -2524 => (WONStatus_InvalidPassword_String, false),
        -2527 => (WONStatus_MustBeAdmin_String, false),
        -2529 => (WONStatus_MustBeCaptain_String, false),
        -2530 => (WONStatus_MustBeCaptainOrModerator_String, false),
        -2550 | -2549 | -2547 | -2546 | -2545 | -2544 | -2543 | -2541 | -2540 | -2539 | -2538 | -2537 | -2536
        | -2535 | -2534 | -2531 | -2528 | -2526 | -2525 | -2523 | -2515 | -2513 | -2506 | -2503 => {
            (WONStatus_ChatFailure_String, true)
        }
        -2807 | -1510 | -1501 => (WONStatus_InvalidCDKey_String, false),
        -1502 => (WONStatus_CDKeyBanned_String, false),
        -1503 => (WONStatus_CDKeyInUse_String, false),
        -1504 => (WONStatus_CRCFailed_String, false),
        -1505 => (WONStatus_UserExists_String, false),
        -1506 => (WONStatus_UserNotFound_String, false),
        -1507 => (WONStatus_BadPassword_String, false),
        -1508 => (WONStatus_InvalidUserName_String, false),
        -1509 => (WONStatus_BadCommunity_String, false),
        -1511 => (WONStatus_NotInCommunity_String, false),
        -1512 => (WONStatus_UserSeqInUse_String, false),
        -1513 | -1500 => (WONStatus_AuthFailure_String, true),
        -1224 | -1223 | -1222 | -1221 | -1220 | -1219 | -1218 | -1217 | -1216 | -1215 | -1214 | -1213 | -1212
        | -1211 | -1210 | -1209 | -1208 | -1207 | -1206 | -1205 | -1204 | -1203 | -1202 | -1201 | -1200 => {
            (WONStatus_DirFailure_String, true)
        }
        -8 => (WONStatus_NotAuthorized_String, false),
        -17 => (WONStatus_BehindFirewall_String, false),
        -19 | -18 => (WONStatus_ExpiredCertificate_String, false),
        -20 | -16 | -15 | -14 | -13 | -12 | -11 | -10 | -9 | -7 | -6 | -5 | -4 | -2 | -1 => {
            (WONStatus_GeneralFailure_String, true)
        }
        -3 => (WONStatus_NotAllowed_String, false),
        // WS_Success
        0 => (WONStatus_Success_String, false),
        // WS_WSAEINTR .. WS_WSAEREMOTE
        10004 | 10009 | 10013 | 10014 | 10022 | 10024 | 10035 | 10036 | 10037 | 10038 | 10039 | 10040 | 10041
        | 10042 | 10043 | 10044 | 10045 | 10046 | 10047 | 10048 | 10049 | 10050 | 10051 | 10052 | 10053 | 10054
        | 10055 | 10056 | 10057 | 10058 | 10059 | 10060 | 10062 | 10063 | 10064 | 10065 | 10066 | 10067 | 10068
        | 10069 | 10070 | 10071 => (WONStatus_GeneralFailure_String, true),
        // WS_WSAECONNREFUSED
        10061 => (WONStatus_ConnRefused_String, false),
        // WS_TimedOut
        100001 => (WONStatus_TimedOut_String, false),
        // WS_Killed
        100002 => (WONStatus_Killed_String, false),
        // WS_Pending .. WS_AuthRequired
        100000 | 100003 | 100004 | 100005 | 100006 | 100007 | 100008 | 100009 | 100010 | 100011 | 100012 | 100013 => {
            (WONStatus_GeneralFailure_String, true)
        }
        // WS_InvalidAddress
        100014 => (WONStatus_InvalidAddress_String, false),
        // WS_NoReply .. WS_AsyncSocket_Shutdown
        100015 | 100016 | 100017 | 100018 | 100019 | 100020 => (WONStatus_GeneralFailure_String, true),
        // WS_AsyncSocket_ConnectFailed
        100021 => (WONStatus_ConnRefused_String, false),
        // WS_SocketOp_InvalidSocket, WS_RecvMsg_InvalidLengthFieldSize, WS_RecvMsg_InvalidMessageLength
        100022 | 100023 | 100024 => (WONStatus_GeneralFailure_String, true),
        // WS_PeerAuthClient_Challenge1DecryptFailure .. WS_PeerAuth_GetCertFailure
        100025 | 100026 | 100027 | 100028 | 100029 | 100030 | 100031 | 100032 | 100033 | 100034 | 100035 | 100036
        | 100037 | 100038 | 100039 | 100040 | 100041 | 100042 | 100043 | 100044 | 100045 | 100046 | 100047 | 100048
        | 100049 | 100050 | 100051 | 100052 | 100053 => (WONStatus_AuthFailure_String, true),
        // WS_ServerReq_FailedAllServers .. WS_ServerReq_UnpackFailure
        100054 | 100055 | 100056 | 100057 | 100058 | 100059 | 100060 | 100061 | 100062 | 100063 | 100064 => {
            (WONStatus_GeneralFailure_String, true)
        }
        // WS_GetCert_PubKeyEncryptFailure .. WS_AuthSession_DecryptUnpackFailure
        100065 | 100066 | 100067 | 100068 | 100069 | 100070 | 100071 | 100072 | 100073 | 100074 | 100075 | 100076
        | 100077 | 100078 | 100079 | 100080 | 100081 | 100082 | 100083 | 100084 | 100085 | 100086 | 100087 => {
            (WONStatus_AuthFailure_String, true)
        }
        // WS_ServerConnection_RecvNotAllowedInAsyncMode
        100088 => (WONStatus_GeneralFailure_String, true),
        // WS_FTP_InvalidResponse, WS_FTP_StatusError, WS_FTP_InvalidPasvResponse
        100089 | 100090 | 100091 => (WONStatus_FTPFailure_String, true),
        // WS_HTTP_InvalidHeader .. WS_HTTP_SendSuccess
        100092 | 100093 | 100094 | 100095 | 100096 | 100097 => (WONStatus_HTTPFailure_String, true),
        // WS_GetMOTD_SysNotFound, WS_GetMOTD_GameNotFound
        100098 | 100099 => (WONStatus_MOTDFailure_String, true),
        // WS_BaseMsgRequest_NoRequest, WS_BaseMsgRequest_UnpackFailure, WS_BaseMsgRequest_BadHeaderType
        100100 | 100101 | 100102 => (WONStatus_GeneralFailure_String, true),
        // WS_RoutingOp_ReplyUnpackFailure, WS_RoutingOp_DontWantReply, WS_RoutingOp_NeedMoreReplies
        100103 | 100104 | 100105 => (WONStatus_RoutingFailure_String, true),
        // WS_GameSpySupport_UnhandledError .. WS_GameSpySupport_ConnError
        100106 | 100107 | 100108 | 100109 | 100110 => (WONStatus_GameSpyFailure_String, true),
        _ => (WONStatus_GeneralFailure_String, true),
    }
}

/// `GetWONStatusString`'s choice for `status`.
#[no_mangle]
pub extern "C" fn ee_won_status_lobby_text(status: i32) -> EeWonStatusLobbyText {
    let (res, general) = status_text(status);
    EeWonStatusLobbyText { res, general }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn specific_and_general_texts() {
        assert_eq!(status_text(0), (WONStatus_Success_String, false));
        assert_eq!(status_text(100001), (WONStatus_TimedOut_String, false));
        assert_eq!(status_text(-2803), (WONStatus_UserExists_String, false));
        assert_eq!(status_text(10061), (WONStatus_ConnRefused_String, false));
        assert_eq!(status_text(10004), (WONStatus_GeneralFailure_String, true));
        assert_eq!(status_text(-1), (WONStatus_GeneralFailure_String, true));
        assert_eq!(status_text(-1200), (WONStatus_DirFailure_String, true));
        assert_eq!(status_text(123_456_789), (WONStatus_GeneralFailure_String, true));
        assert_eq!(status_text(i32::MIN), (WONStatus_GeneralFailure_String, true));
        let t = ee_won_status_lobby_text(100002);
        assert_eq!((t.res, t.general), (WONStatus_Killed_String, false));
    }
}
