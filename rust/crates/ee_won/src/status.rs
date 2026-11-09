//! WONStatus: the WON library's status codes (`include/ee/wonapi/WONStatus.h`) and their names
//! (`WONStatusToString`, `src/Empire Earth/WONAPI/WONCommon/WONStatus.cpp`). The table was printed by the C++ over
//! every code from -40000 to 120000 (`golden_status.tsv`, the test's reference).

use std::borrow::Cow;
use std::ffi::CStr;

/// A WON status (the C++ `WONStatus`, a `long`).
pub type Status = i64;

pub const WS_SUCCESS: Status = 0;
pub const WS_PENDING: Status = 100000;
pub const WS_TIMED_OUT: Status = 100001;
pub const WS_KILLED: Status = 100002;
pub const WS_NONE: Status = 100004;
pub const WS_AWAITING_COMPLETION: Status = 100005;
pub const WS_FAILED_TO_OPEN_FILE: Status = 100007;
pub const WS_FAILED_TO_WRITE_TO_FILE: Status = 100008;
pub const WS_INVALID_ADDRESS: Status = 100014;
pub const WS_ASYNC_SOCKET_INVALID_SOCKET: Status = 100016;
pub const WS_ASYNC_SOCKET_PARTIAL_SEND_TO: Status = 100017;
pub const WS_ASYNC_SOCKET_STREAM_NOT_ALLOWED: Status = 100018;
pub const WS_ASYNC_SOCKET_DATAGRAM_NOT_ALLOWED: Status = 100019;
pub const WS_ASYNC_SOCKET_SHUTDOWN: Status = 100020;
pub const WS_ASYNC_SOCKET_CONNECT_FAILED: Status = 100021;
pub const WS_RECV_MSG_INVALID_LENGTH_FIELD_SIZE: Status = 100023;
pub const WS_RECV_MSG_INVALID_MESSAGE_LENGTH: Status = 100024;
/// The DB proxy's game-specific error range, named by number.
pub const WS_DB_PROXY_SERV_GAME_SPECIFIC_ERROR_START: Status = -2900;
pub const WS_DB_PROXY_SERV_GAME_SPECIFIC_ERROR_END: Status = -2999;

/// Every named status, by code.
pub static NAMES: &[(Status, &CStr)] = &[
    (-2999, c"WS_DBProxyServ_GameSpecificErrorEnd"),
    (-2903, c"WS_DBProxyServ_GameSpecificWarriorNameDoesNotExist"),
    (-2900, c"WS_DBProxyServ_GameSpecificErrorStart"),
    (-2817, c"WS_DBProxyServ_DataAlreadyExists"),
    (-2816, c"WS_DBProxyServ_ValidNotLatest"),
    (-2815, c"WS_DBProxyServ_OutOfDateNoUpdate"),
    (-2814, c"WS_DBProxyServ_OutOfDate"),
    (-2813, c"WS_DBProxyServ_ItemHasDependencies"),
    (-2812, c"WS_DBProxyServ_AgeNotOldEnough"),
    (-2811, c"WS_DBProxyServ_InvalidPassword"),
    (-2810, c"WS_DBProxyServ_AccountCreateDisabled"),
    (-2809, c"WS_DBProxyServ_EMailPasswordDuplication"),
    (-2808, c"WS_DBProxyServ_KeyNotUsed"),
    (-2807, c"WS_DBProxyServ_InvalidCDKey"),
    (-2806, c"WS_DBProxyServ_UserDoesNotExist"),
    (-2805, c"WS_DBProxyServ_KeyInUse"),
    (-2804, c"WS_DBProxyServ_InvalidUserName"),
    (-2803, c"WS_DBProxyServ_UserExists"),
    (-2802, c"WS_DBProxyServ_NoModule"),
    (-2801, c"WS_DBProxyServ_NoData"),
    (-2800, c"WS_DBProxyServ_DBError"),
    (-2617, c"WS_ProfileServ_RemoveNewsLettersFailed"),
    (-2616, c"WS_ProfileServ_NoNewslettersProvided"),
    (-2615, c"WS_ProfileServ_TooManyNewsletters"),
    (-2614, c"WS_ProfileServ_UnfoundNewsletterName"),
    (-2613, c"WS_ProfileServ_OtherError"),
    (-2612, c"WS_ProfileServ_AccountCreationDisabled"),
    (-2611, c"WS_ProfileServ_InvalidWONUserName"),
    (-2610, c"WS_ProfileServ_DatabaseError"),
    (-2609, c"WS_ProfileServ_WONUserAlreadyExists"),
    (-2608, c"WS_ProfileServ_BadWONUserPassword"),
    (-2607, c"WS_ProfileServ_PartySourceEntryExists"),
    (-2606, c"WS_ProfileServ_NewsletterEntryExists"),
    (-2605, c"WS_ProfileServ_EmailAlreadyUsed"),
    (-2604, c"WS_ProfileServ_PartySeqAlreadySet"),
    (-2603, c"WS_ProfileServ_CountryNotFound"),
    (-2602, c"WS_ProfileServ_AgeRangeNotFound"),
    (-2601, c"WS_ProfileServ_UserNotFound"),
    (-2600, c"WS_ProfileServ_ProfileDoesNotExist"),
    (-2552, c"WS_RoutingServG2_JoinCanceled"),
    (-2551, c"WS_RoutingServG2_GroupNameExceedsMaximum"),
    (-2550, c"WS_RoutingServG2_TooManyRecipients"),
    (-2549, c"WS_RoutingServG2_TooManyMemberships"),
    (-2548, c"WS_RoutingServG2_Throttled"),
    (-2547, c"WS_RoutingServG2_SubscriptionDoesNotExists"),
    (-2546, c"WS_RoutingServG2_SubscriptionDoesNotExist"),
    (-2545, c"WS_RoutingServG2_SubscriptionAlreadyExists"),
    (-2544, c"WS_RoutingServG2_ShutdownTimerDoesNotExist"),
    (-2543, c"WS_RoutingServG2_ShutdownTimerAlreadyExists"),
    (-2542, c"WS_RoutingServG2_ServerFull"),
    (-2541, c"WS_RoutingServG2_Pending"),
    (-2540, c"WS_RoutingServG2_OffsetTooLarge"),
    (-2539, c"WS_RoutingServG2_ObserversNotAllowed"),
    (-2538, c"WS_RoutingServG2_ObjectDoesNotExist"),
    (-2537, c"WS_RoutingServG2_NoUsernameInCertificate"),
    (-2536, c"WS_RoutingServG2_NoPendingJoin"),
    (-2535, c"WS_RoutingServG2_MustBeVisible"),
    (-2534, c"WS_RoutingServG2_MustBeSelf"),
    (-2532, c"WS_RoutingServG2_MustBeModerator"),
    (-2531, c"WS_RoutingServG2_MustBeClient"),
    (-2530, c"WS_RoutingServG2_MustBeCaptainOrModerator"),
    (-2529, c"WS_RoutingServG2_MustBeCaptain"),
    (-2528, c"WS_RoutingServG2_MustBeAuthenticated"),
    (-2527, c"WS_RoutingServG2_MustBeAdmin"),
    (-2526, c"WS_RoutingServG2_LoginTypeNotSupported"),
    (-2525, c"WS_RoutingServG2_InvalidWONUserId"),
    (-2524, c"WS_RoutingServG2_InvalidPassword"),
    (-2523, c"WS_RoutingServG2_InvalidContentType"),
    (-2522, c"WS_RoutingServG2_GuestNamesReserved"),
    (-2521, c"WS_RoutingServG2_GroupFull"),
    (-2520, c"WS_RoutingServG2_GroupDoesNotExist"),
    (-2519, c"WS_RoutingServG2_GroupDeleted"),
    (-2518, c"WS_RoutingServG2_GroupClosed"),
    (-2517, c"WS_RoutingServG2_GroupAlreadyExists"),
    (-2516, c"WS_RoutingServG2_GroupAlreadyClaimed"),
    (-2515, c"WS_RoutingServG2_DuplicateDataObject"),
    (-2514, c"WS_RoutingServG2_ConnectFailure"),
    (-2513, c"WS_RoutingServG2_ClientNotPermitted"),
    (-2512, c"WS_RoutingServG2_ClientNotMuted"),
    (-2511, c"WS_RoutingServG2_ClientNotInvited"),
    (-2510, c"WS_RoutingServG2_ClientNotInGroup"),
    (-2509, c"WS_RoutingServG2_ClientNotBanned"),
    (-2508, c"WS_RoutingServG2_ClientDoesNotExist"),
    (-2507, c"WS_RoutingServG2_ClientBanned"),
    (-2506, c"WS_RoutingServG2_ClientAlreadyRegistered"),
    (-2505, c"WS_RoutingServG2_ClientAlreadyMuted"),
    (-2504, c"WS_RoutingServG2_ClientAlreadyInGroup"),
    (-2503, c"WS_RoutingServG2_ClientAlreadyExists"),
    (-2502, c"WS_RoutingServG2_ClientAlreadyBanned"),
    (-2501, c"WS_RoutingServG2_CaptainRejectedYou"),
    (-2500, c"WS_RoutingServG2_BlankNamesNotAllowed"),
    (-1902, c"WS_EventServ_InvalidDataType"),
    (-1901, c"WS_EventServ_InvalidDateTime"),
    (-1900, c"WS_EventServ_BufferingError"),
    (-1899, c"WS_EventServ_FailureStart"),
    (-1513, c"WS_AuthServ_SetCUDError"),
    (-1512, c"WS_AuthServ_UserSeqInUse"),
    (-1511, c"WS_AuthServ_NotInCommunity"),
    (-1510, c"WS_AuthServ_InvalidCDKey"),
    (-1509, c"WS_AuthServ_BadCommunity"),
    (-1508, c"WS_AuthServ_InvalidUserName"),
    (-1507, c"WS_AuthServ_BadPassword"),
    (-1506, c"WS_AuthServ_UserNotFound"),
    (-1505, c"WS_AuthServ_UserExists"),
    (-1504, c"WS_AuthServ_CRCFailed"),
    (-1503, c"WS_AuthServ_CDKeyInUse"),
    (-1502, c"WS_AuthServ_CDKeyBanned"),
    (-1501, c"WS_AuthServ_BadCDKey"),
    (-1500, c"WS_AuthServ_BadPubKeyBlock"),
    (-1499, c"WS_AuthServ_FailureStart"),
    (-1224, c"WS_DirServ_MultiGetFailedAllRequests"),
    (-1223, c"WS_DirServ_MultiGetPartialFailure"),
    (-1222, c"WS_DirServ_NoACLs"),
    (-1221, c"WS_DirServ_MaxPermissions"),
    (-1220, c"WS_DirServ_PermissionNotFound"),
    (-1219, c"WS_DirServ_PermissionExists"),
    (-1218, c"WS_DirServ_InvalidACLType"),
    (-1217, c"WS_DirServ_DataObjectNotFound"),
    (-1216, c"WS_DirServ_DataObjectExists"),
    (-1215, c"WS_DirServ_NoDataObjects"),
    (-1214, c"WS_DirServ_InvalidDataObject"),
    (-1213, c"WS_DirServ_InvalidLifespan"),
    (-1212, c"WS_DirServ_InvalidMode"),
    (-1211, c"WS_DirServ_InvalidKey"),
    (-1210, c"WS_DirServ_InvalidGetFlags"),
    (-1209, c"WS_DirServ_InvalidPath"),
    (-1208, c"WS_DirServ_BadDataOffset"),
    (-1207, c"WS_DirServ_MaxDataObjects"),
    (-1206, c"WS_DirServ_EntityTooLarge"),
    (-1205, c"WS_DirServ_AlreadyConnected"),
    (-1204, c"WS_DirServ_DirIsFull"),
    (-1203, c"WS_DirServ_ServiceExists"),
    (-1202, c"WS_DirServ_DirExists"),
    (-1201, c"WS_DirServ_ServiceNotFound"),
    (-1200, c"WS_DirServ_DirNotFound"),
    (-1199, c"WS_DirServ_FailureStart"),
    (-20, c"WS_CommServ_StatusCommon_BannedAddress"),
    (-19, c"WS_CommServ_ExpiredPeerCertificate"),
    (-18, c"WS_CommServ_BadPeerCertificate"),
    (-17, c"WS_CommServ_BehindFirewall"),
    (-16, c"WS_CommServ_InvalidMessage"),
    (-15, c"WS_CommServ_InvalidSequenceNumber"),
    (-14, c"WS_CommServ_InvalidSessionId"),
    (-13, c"WS_CommServ_DecryptionFailed"),
    (-12, c"WS_CommServ_NotSupported"),
    (-11, c"WS_CommServ_TimedOut"),
    (-10, c"WS_CommServ_NoPortAvailable"),
    (-9, c"WS_CommServ_ResourcesLow"),
    (-8, c"WS_CommServ_NotAuthorized"),
    (-7, c"WS_CommServ_SessionNotFound"),
    (-6, c"WS_CommServ_MessageExceedsMax"),
    (-5, c"WS_CommServ_AlreadyExists"),
    (-4, c"WS_CommServ_InvalidParameters"),
    (-3, c"WS_CommServ_NotAllowed"),
    (-2, c"WS_CommServ_NotAvailable"),
    (-1, c"WS_CommServ_Failure"),
    (0, c"WS_Success"),
    (10004, c"WS_WSAEINTR"),
    (10009, c"WS_WSAEBADF"),
    (10013, c"WS_WSAEACCES"),
    (10014, c"WS_WSAEFAULT"),
    (10022, c"WS_WSAEINVAL"),
    (10024, c"WS_WSAEMFILE"),
    (10035, c"WS_WSAEWOULDBLOCK"),
    (10036, c"WS_WSAEINPROGRESS"),
    (10037, c"WS_WSAEALREADY"),
    (10038, c"WS_WSAENOTSOCK"),
    (10039, c"WS_WSAEDESTADDRREQ"),
    (10040, c"WS_WSAEMSGSIZE"),
    (10041, c"WS_WSAEPROTOTYPE"),
    (10042, c"WS_WSAENOPROTOOPT"),
    (10043, c"WS_WSAEPROTONOSUPPORT"),
    (10044, c"WS_WSAESOCKTNOSUPPORT"),
    (10045, c"WS_WSAEOPNOTSUPP"),
    (10046, c"WS_WSAEPFNOSUPPORT"),
    (10047, c"WS_WSAEAFNOSUPPORT"),
    (10048, c"WS_WSAEADDRINUSE"),
    (10049, c"WS_WSAEADDRNOTAVAIL"),
    (10050, c"WS_WSAENETDOWN"),
    (10051, c"WS_WSAENETUNREACH"),
    (10052, c"WS_WSAENETRESET"),
    (10053, c"WS_WSAECONNABORTED"),
    (10054, c"WS_WSAECONNRESET"),
    (10055, c"WS_WSAENOBUFS"),
    (10056, c"WS_WSAEISCONN"),
    (10057, c"WS_WSAENOTCONN"),
    (10058, c"WS_WSAESHUTDOWN"),
    (10059, c"WS_WSAETOOMANYREFS"),
    (10060, c"WS_WSAETIMEDOUT"),
    (10061, c"WS_WSAECONNREFUSED"),
    (10062, c"WS_WSAELOOP"),
    (10063, c"WS_WSAENAMETOOLONG"),
    (10064, c"WS_WSAEHOSTDOWN"),
    (10065, c"WS_WSAEHOSTUNREACH"),
    (10066, c"WS_WSAENOTEMPTY"),
    (10067, c"WS_WSAEPROCLIM"),
    (10068, c"WS_WSAEUSERS"),
    (10069, c"WS_WSAEDQUOT"),
    (10070, c"WS_WSAESTALE"),
    (10071, c"WS_WSAEREMOTE"),
    (10091, c"WS_WSASYSNOTREADY"),
    (10092, c"WS_WSAVERNOTSUPPORTED"),
    (10093, c"WS_WSANOTINITIALISED"),
    (10101, c"WS_WSAEDISCON"),
    (100000, c"WS_Pending"),
    (100001, c"WS_TimedOut"),
    (100002, c"WS_Killed"),
    (100003, c"WS_Incomplete"),
    (100004, c"WS_None"),
    (100005, c"WS_AwaitingCompletion"),
    (100006, c"WS_InvalidMessage"),
    (100007, c"WS_FailedToOpenFile"),
    (100008, c"WS_FailedToWriteToFile"),
    (100009, c"WS_BlockMode_NotSupported"),
    (100010, c"WS_AsyncMode_NotSupported"),
    (100011, c"WS_InvalidPublicKey"),
    (100012, c"WS_MessageUnpackFailure"),
    (100013, c"WS_AuthRequired"),
    (100014, c"WS_InvalidAddress"),
    (100015, c"WS_NoReply"),
    (100016, c"WS_AsyncSocket_InvalidSocket"),
    (100017, c"WS_AsyncSocket_PartialSendTo"),
    (100018, c"WS_AsyncSocket_StreamNotAllowed"),
    (100019, c"WS_AsyncSocket_DatagramNotAllowed"),
    (100020, c"WS_AsyncSocket_Shutdown"),
    (100021, c"WS_AsyncSocket_ConnectFailed"),
    (100022, c"WS_SocketOp_InvalidSocket"),
    (100023, c"WS_RecvMsg_InvalidLengthFieldSize"),
    (100024, c"WS_RecvMsg_InvalidMessageLength"),
    (100025, c"WS_PeerAuthClient_Challenge1DecryptFailure"),
    (100026, c"WS_PeerAuthClient_Challenge1InvalidSecretLen"),
    (100027, c"WS_PeerAuthClient_Challenge1InvalidSecretKey"),
    (100028, c"WS_PeerAuthClient_Challenge1CertificateUnpackFailure"),
    (100029, c"WS_PeerAuthClient_Challenge1CertificateVerifyFailure"),
    (100030, c"WS_PeerAuthClient_Challenge2EncryptFailure"),
    (100031, c"WS_PeerAuthClient_CompleteDecryptFailure"),
    (100032, c"WS_PeerAuthClient_CompleteInvalidSecretLen"),
    (100033, c"WS_PeerAuthClient_CompleteInvalidSecretKey"),
    (100034, c"WS_PeerAuthClient_InvalidServiceType"),
    (100035, c"WS_PeerAuthClient_UnexpectedChallenge"),
    (100036, c"WS_PeerAuthClient_InvalidMessage"),
    (100037, c"WS_PeerAuthClient_MsgUnpackFailure"),
    (100038, c"WS_PeerAuthServer_NotStarted"),
    (100039, c"WS_PeerAuthServer_InvalidServiceType"),
    (100040, c"WS_PeerAuthServer_UnexpectedRequest"),
    (100041, c"WS_PeerAuthServer_UnexpectedChallenge"),
    (100042, c"WS_PeerAuthServer_InvalidMessage"),
    (100043, c"WS_PeerAuthServer_MsgUnpackFailure"),
    (100044, c"WS_PeerAuthServer_InvalidAuthMode"),
    (100045, c"WS_PeerAuthServer_InvalidEncryptType"),
    (100046, c"WS_PeerAuthServer_InvalidClientCertificate"),
    (100047, c"WS_PeerAuthServer_ExpiredClientCertificate"),
    (100048, c"WS_PeerAuthServer_FailedToVerifyClientCertificate"),
    (100049, c"WS_PeerAuthServer_FailedToEncryptWithClientPubKey"),
    (100050, c"WS_PeerAuthServer_FailedToDecryptWithPrivateKey"),
    (100051, c"WS_PeerAuthServer_InvalidSecretA"),
    (100052, c"WS_PeerAuthServer_InvalidSecretB"),
    (100053, c"WS_PeerAuth_GetCertFailure"),
    (100054, c"WS_ServerReq_FailedAllServers"),
    (100055, c"WS_ServerReq_NoServersSpecified"),
    (100056, c"WS_ServerReq_NeedAuthContext"),
    (100057, c"WS_ServerReq_TryNextServer"),
    (100058, c"WS_ServerReq_Send"),
    (100059, c"WS_ServerReq_Recv"),
    (100060, c"WS_ServerReq_ExitCommunicationLoop"),
    (100061, c"WS_ServerReq_SessionExpired"),
    (100062, c"WS_ServerReq_GetCertFailure"),
    (100063, c"WS_ServerReq_InvalidReplyHeader"),
    (100064, c"WS_ServerReq_UnpackFailure"),
    (100065, c"WS_GetCert_PubKeyEncryptFailure"),
    (100066, c"WS_GetCert_InvalidPubKeyReply"),
    (100067, c"WS_GetCert_InvalidLoginReply"),
    (100068, c"WS_GetCert_UnableToFindHashFile"),
    (100069, c"WS_GetCert_UnexpectedLoginChallenge"),
    (100070, c"WS_GetCert_NeedVerifierKey"),
    (100071, c"WS_GetCert_FailedToVerifyPubKeyBlock"),
    (100072, c"WS_GetCert_InvalidCertificate"),
    (100073, c"WS_GetCert_InvalidPubKeyBlock"),
    (100074, c"WS_GetCert_DecryptFailure"),
    (100075, c"WS_GetCert_InvalidPrivateKey"),
    (100076, c"WS_GetCert_InvalidSecretConfirm"),
    (100077, c"WS_GetCert_MissingCertificate"),
    (100078, c"WS_AuthData_SetCertificateUnpackFailed"),
    (100079, c"WS_AuthData_SetPubKeyBlockUnpackFailed"),
    (100080, c"WS_AuthData_SetCertificateVerifyFailed"),
    (100081, c"WS_AuthData_SetPrivateKeyFailed"),
    (100082, c"WS_AuthSession_EncryptFailure"),
    (100083, c"WS_AuthSession_DecryptSessionIdMismatch"),
    (100084, c"WS_AuthSession_DecryptFailure"),
    (100085, c"WS_AuthSession_DecryptBadLen"),
    (100086, c"WS_AuthSession_DecryptInvalidSequenceNum"),
    (100087, c"WS_AuthSession_DecryptUnpackFailure"),
    (100088, c"WS_ServerConnection_RecvNotAllowedInAsyncMode"),
    (100089, c"WS_FTP_InvalidResponse"),
    (100090, c"WS_FTP_StatusError"),
    (100091, c"WS_FTP_InvalidPasvResponse"),
    (100092, c"WS_HTTP_InvalidHeader"),
    (100093, c"WS_HTTP_StatusError"),
    (100094, c"WS_HTTP_InvalidRedirect"),
    (100095, c"WS_HTTP_TooManyRedirects"),
    (100096, c"WS_HTTP_Redirected"),
    (100097, c"WS_HTTP_SendSuccess"),
    (100098, c"WS_GetMOTD_SysNotFound"),
    (100099, c"WS_GetMOTD_GameNotFound"),
    (100100, c"WS_BaseMsgRequest_NoRequest"),
    (100101, c"WS_BaseMsgRequest_UnpackFailure"),
    (100102, c"WS_BaseMsgRequest_BadHeaderType"),
    (100103, c"WS_RoutingOp_ReplyUnpackFailure"),
    (100104, c"WS_RoutingOp_DontWantReply"),
    (100105, c"WS_RoutingOp_NeedMoreReplies"),
    (100106, c"WS_GameSpySupport_UnhandledError"),
    (100107, c"WS_GameSpySupport_WinSockError"),
    (100108, c"WS_GameSpySupport_BindError"),
    (100109, c"WS_GameSpySupport_DNSError"),
    (100110, c"WS_GameSpySupport_ConnError"),
];

/// The status's name: its `WS_` name, `DBProxyServ_GameSpecificError<n>` in the DB proxy's game-specific range, else
/// the number.
pub fn name(status: Status) -> Cow<'static, str> {
    if let Some(n) = named(status) {
        return Cow::Borrowed(n.to_str().unwrap_or(""));
    }
    Cow::Owned(unnamed(status))
}

/// The `WS_` name of a status that has one.
pub fn named(status: Status) -> Option<&'static CStr> {
    NAMES.binary_search_by_key(&status, |e| e.0).ok().map(|i| NAMES[i].1)
}

/// The name of a status without a `WS_` name (WONStatusToString's `sprintf` cases).
pub fn unnamed(status: Status) -> String {
    if (WS_DB_PROXY_SERV_GAME_SPECIFIC_ERROR_END..=WS_DB_PROXY_SERV_GAME_SPECIFIC_ERROR_START).contains(&status) {
        format!("DBProxyServ_GameSpecificError{status}")
    } else {
        status.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    #[test]
    fn names_equal_the_cpp_table() {
        let golden: BTreeMap<Status, &str> = include_str!("golden_status.tsv")
            .lines()
            .map(|l| {
                let (c, n) = l.split_once('\t').unwrap();
                (c.parse().unwrap(), n)
            })
            .collect();
        assert_eq!(golden.len(), 412);
        for c in -40000..=120000 {
            let want = golden.get(&c).map(|s| s.to_string()).unwrap_or_else(|| c.to_string());
            assert_eq!(name(c), want, "status {c}");
        }
        assert_eq!(name(i32::MIN as Status), "-2147483648");
    }

    #[test]
    fn table_is_sorted_and_unique() {
        assert!(NAMES.windows(2).all(|w| w[0].0 < w[1].0));
        assert_eq!(named(WS_TIMED_OUT).unwrap().to_str().unwrap(), "WS_TimedOut");
        assert_eq!(named(WS_ASYNC_SOCKET_SHUTDOWN).unwrap().to_str().unwrap(), "WS_AsyncSocket_Shutdown");
        assert_eq!(
            named(WS_RECV_MSG_INVALID_MESSAGE_LENGTH).unwrap().to_str().unwrap(),
            "WS_RecvMsg_InvalidMessageLength"
        );
    }
}
