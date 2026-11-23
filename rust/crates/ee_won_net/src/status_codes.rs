//! The WON SDK's status codes, by the names the C++ uses (`WS_Success`, ...): the C++ `WONStatus` (a `long`)
//! values, read through `rust/won_net/status_codes.h` (`include/ee/wonapi/WONStatus.h` includes it; until R6 WONLobby
//! round 30 they were that header's enum). The names are WONStatusToString's (0x69deb0): status.rs's `NAMES` table,
//! printed by the C++ over every code (`golden_status.tsv`), names each of these values by its constant's name.

#![allow(non_upper_case_globals)]

pub const WS_DBProxyServ_GameSpecificErrorEnd: i32 = -2999;
pub const WS_DBProxyServ_GameSpecificWarriorNameDoesNotExist: i32 = -2903;
pub const WS_DBProxyServ_GameSpecificErrorStart: i32 = -2900;
pub const WS_DBProxyServ_DataAlreadyExists: i32 = -2817;
pub const WS_DBProxyServ_ValidNotLatest: i32 = -2816;
pub const WS_DBProxyServ_OutOfDateNoUpdate: i32 = -2815;
pub const WS_DBProxyServ_OutOfDate: i32 = -2814;
pub const WS_DBProxyServ_ItemHasDependencies: i32 = -2813;
pub const WS_DBProxyServ_AgeNotOldEnough: i32 = -2812;
pub const WS_DBProxyServ_InvalidPassword: i32 = -2811;
pub const WS_DBProxyServ_AccountCreateDisabled: i32 = -2810;
pub const WS_DBProxyServ_EMailPasswordDuplication: i32 = -2809;
pub const WS_DBProxyServ_KeyNotUsed: i32 = -2808;
pub const WS_DBProxyServ_InvalidCDKey: i32 = -2807;
pub const WS_DBProxyServ_UserDoesNotExist: i32 = -2806;
pub const WS_DBProxyServ_KeyInUse: i32 = -2805;
pub const WS_DBProxyServ_InvalidUserName: i32 = -2804;
pub const WS_DBProxyServ_UserExists: i32 = -2803;
pub const WS_DBProxyServ_NoModule: i32 = -2802;
pub const WS_DBProxyServ_NoData: i32 = -2801;
pub const WS_DBProxyServ_DBError: i32 = -2800;
pub const WS_ProfileServ_RemoveNewsLettersFailed: i32 = -2617;
pub const WS_ProfileServ_NoNewslettersProvided: i32 = -2616;
pub const WS_ProfileServ_TooManyNewsletters: i32 = -2615;
pub const WS_ProfileServ_UnfoundNewsletterName: i32 = -2614;
pub const WS_ProfileServ_OtherError: i32 = -2613;
pub const WS_ProfileServ_AccountCreationDisabled: i32 = -2612;
pub const WS_ProfileServ_InvalidWONUserName: i32 = -2611;
pub const WS_ProfileServ_DatabaseError: i32 = -2610;
pub const WS_ProfileServ_WONUserAlreadyExists: i32 = -2609;
pub const WS_ProfileServ_BadWONUserPassword: i32 = -2608;
pub const WS_ProfileServ_PartySourceEntryExists: i32 = -2607;
pub const WS_ProfileServ_NewsletterEntryExists: i32 = -2606;
pub const WS_ProfileServ_EmailAlreadyUsed: i32 = -2605;
pub const WS_ProfileServ_PartySeqAlreadySet: i32 = -2604;
pub const WS_ProfileServ_CountryNotFound: i32 = -2603;
pub const WS_ProfileServ_AgeRangeNotFound: i32 = -2602;
pub const WS_ProfileServ_UserNotFound: i32 = -2601;
pub const WS_ProfileServ_ProfileDoesNotExist: i32 = -2600;
pub const WS_RoutingServG2_JoinCanceled: i32 = -2552;
pub const WS_RoutingServG2_GroupNameExceedsMaximum: i32 = -2551;
pub const WS_RoutingServG2_TooManyRecipients: i32 = -2550;
pub const WS_RoutingServG2_TooManyMemberships: i32 = -2549;
pub const WS_RoutingServG2_Throttled: i32 = -2548;
pub const WS_RoutingServG2_SubscriptionDoesNotExists: i32 = -2547;
pub const WS_RoutingServG2_SubscriptionDoesNotExist: i32 = -2546;
pub const WS_RoutingServG2_SubscriptionAlreadyExists: i32 = -2545;
pub const WS_RoutingServG2_ShutdownTimerDoesNotExist: i32 = -2544;
pub const WS_RoutingServG2_ShutdownTimerAlreadyExists: i32 = -2543;
pub const WS_RoutingServG2_ServerFull: i32 = -2542;
pub const WS_RoutingServG2_Pending: i32 = -2541;
pub const WS_RoutingServG2_OffsetTooLarge: i32 = -2540;
pub const WS_RoutingServG2_ObserversNotAllowed: i32 = -2539;
pub const WS_RoutingServG2_ObjectDoesNotExist: i32 = -2538;
pub const WS_RoutingServG2_NoUsernameInCertificate: i32 = -2537;
pub const WS_RoutingServG2_NoPendingJoin: i32 = -2536;
pub const WS_RoutingServG2_MustBeVisible: i32 = -2535;
pub const WS_RoutingServG2_MustBeSelf: i32 = -2534;
pub const WS_RoutingServG2_MustBeModerator: i32 = -2532;
pub const WS_RoutingServG2_MustBeClient: i32 = -2531;
pub const WS_RoutingServG2_MustBeCaptainOrModerator: i32 = -2530;
pub const WS_RoutingServG2_MustBeCaptain: i32 = -2529;
pub const WS_RoutingServG2_MustBeAuthenticated: i32 = -2528;
pub const WS_RoutingServG2_MustBeAdmin: i32 = -2527;
pub const WS_RoutingServG2_LoginTypeNotSupported: i32 = -2526;
pub const WS_RoutingServG2_InvalidWONUserId: i32 = -2525;
pub const WS_RoutingServG2_InvalidPassword: i32 = -2524;
pub const WS_RoutingServG2_InvalidContentType: i32 = -2523;
pub const WS_RoutingServG2_GuestNamesReserved: i32 = -2522;
pub const WS_RoutingServG2_GroupFull: i32 = -2521;
pub const WS_RoutingServG2_GroupDoesNotExist: i32 = -2520;
pub const WS_RoutingServG2_GroupDeleted: i32 = -2519;
pub const WS_RoutingServG2_GroupClosed: i32 = -2518;
pub const WS_RoutingServG2_GroupAlreadyExists: i32 = -2517;
pub const WS_RoutingServG2_GroupAlreadyClaimed: i32 = -2516;
pub const WS_RoutingServG2_DuplicateDataObject: i32 = -2515;
pub const WS_RoutingServG2_ConnectFailure: i32 = -2514;
pub const WS_RoutingServG2_ClientNotPermitted: i32 = -2513;
pub const WS_RoutingServG2_ClientNotMuted: i32 = -2512;
pub const WS_RoutingServG2_ClientNotInvited: i32 = -2511;
pub const WS_RoutingServG2_ClientNotInGroup: i32 = -2510;
pub const WS_RoutingServG2_ClientNotBanned: i32 = -2509;
pub const WS_RoutingServG2_ClientDoesNotExist: i32 = -2508;
pub const WS_RoutingServG2_ClientBanned: i32 = -2507;
pub const WS_RoutingServG2_ClientAlreadyRegistered: i32 = -2506;
pub const WS_RoutingServG2_ClientAlreadyMuted: i32 = -2505;
pub const WS_RoutingServG2_ClientAlreadyInGroup: i32 = -2504;
pub const WS_RoutingServG2_ClientAlreadyExists: i32 = -2503;
pub const WS_RoutingServG2_ClientAlreadyBanned: i32 = -2502;
pub const WS_RoutingServG2_CaptainRejectedYou: i32 = -2501;
pub const WS_RoutingServG2_BlankNamesNotAllowed: i32 = -2500;
pub const WS_EventServ_InvalidDataType: i32 = -1902;
pub const WS_EventServ_InvalidDateTime: i32 = -1901;
pub const WS_EventServ_BufferingError: i32 = -1900;
pub const WS_EventServ_FailureStart: i32 = -1899;
pub const WS_AuthServ_SetCUDError: i32 = -1513;
pub const WS_AuthServ_UserSeqInUse: i32 = -1512;
pub const WS_AuthServ_NotInCommunity: i32 = -1511;
pub const WS_AuthServ_InvalidCDKey: i32 = -1510;
pub const WS_AuthServ_BadCommunity: i32 = -1509;
pub const WS_AuthServ_InvalidUserName: i32 = -1508;
pub const WS_AuthServ_BadPassword: i32 = -1507;
pub const WS_AuthServ_UserNotFound: i32 = -1506;
pub const WS_AuthServ_UserExists: i32 = -1505;
pub const WS_AuthServ_CRCFailed: i32 = -1504;
pub const WS_AuthServ_CDKeyInUse: i32 = -1503;
pub const WS_AuthServ_CDKeyBanned: i32 = -1502;
pub const WS_AuthServ_BadCDKey: i32 = -1501;
pub const WS_AuthServ_BadPubKeyBlock: i32 = -1500;
pub const WS_AuthServ_FailureStart: i32 = -1499;
pub const WS_DirServ_MultiGetFailedAllRequests: i32 = -1224;
pub const WS_DirServ_MultiGetPartialFailure: i32 = -1223;
pub const WS_DirServ_NoACLs: i32 = -1222;
pub const WS_DirServ_MaxPermissions: i32 = -1221;
pub const WS_DirServ_PermissionNotFound: i32 = -1220;
pub const WS_DirServ_PermissionExists: i32 = -1219;
pub const WS_DirServ_InvalidACLType: i32 = -1218;
pub const WS_DirServ_DataObjectNotFound: i32 = -1217;
pub const WS_DirServ_DataObjectExists: i32 = -1216;
pub const WS_DirServ_NoDataObjects: i32 = -1215;
pub const WS_DirServ_InvalidDataObject: i32 = -1214;
pub const WS_DirServ_InvalidLifespan: i32 = -1213;
pub const WS_DirServ_InvalidMode: i32 = -1212;
pub const WS_DirServ_InvalidKey: i32 = -1211;
pub const WS_DirServ_InvalidGetFlags: i32 = -1210;
pub const WS_DirServ_InvalidPath: i32 = -1209;
pub const WS_DirServ_BadDataOffset: i32 = -1208;
pub const WS_DirServ_MaxDataObjects: i32 = -1207;
pub const WS_DirServ_EntityTooLarge: i32 = -1206;
pub const WS_DirServ_AlreadyConnected: i32 = -1205;
pub const WS_DirServ_DirIsFull: i32 = -1204;
pub const WS_DirServ_ServiceExists: i32 = -1203;
pub const WS_DirServ_DirExists: i32 = -1202;
pub const WS_DirServ_ServiceNotFound: i32 = -1201;
pub const WS_DirServ_DirNotFound: i32 = -1200;
pub const WS_DirServ_FailureStart: i32 = -1199;
pub const WS_CommServ_StatusCommon_BannedAddress: i32 = -20;
pub const WS_CommServ_ExpiredPeerCertificate: i32 = -19;
pub const WS_CommServ_BadPeerCertificate: i32 = -18;
pub const WS_CommServ_BehindFirewall: i32 = -17;
pub const WS_CommServ_InvalidMessage: i32 = -16;
pub const WS_CommServ_InvalidSequenceNumber: i32 = -15;
pub const WS_CommServ_InvalidSessionId: i32 = -14;
pub const WS_CommServ_DecryptionFailed: i32 = -13;
pub const WS_CommServ_NotSupported: i32 = -12;
pub const WS_CommServ_TimedOut: i32 = -11;
pub const WS_CommServ_NoPortAvailable: i32 = -10;
pub const WS_CommServ_ResourcesLow: i32 = -9;
pub const WS_CommServ_NotAuthorized: i32 = -8;
pub const WS_CommServ_SessionNotFound: i32 = -7;
pub const WS_CommServ_MessageExceedsMax: i32 = -6;
pub const WS_CommServ_AlreadyExists: i32 = -5;
pub const WS_CommServ_InvalidParameters: i32 = -4;
pub const WS_CommServ_NotAllowed: i32 = -3;
pub const WS_CommServ_NotAvailable: i32 = -2;
pub const WS_CommServ_Failure: i32 = -1;
pub const WS_Success: i32 = 0;
pub const WS_WSAEINTR: i32 = 10004;
pub const WS_WSAEBADF: i32 = 10009;
pub const WS_WSAEACCES: i32 = 10013;
pub const WS_WSAEFAULT: i32 = 10014;
pub const WS_WSAEINVAL: i32 = 10022;
pub const WS_WSAEMFILE: i32 = 10024;
pub const WS_WSAEWOULDBLOCK: i32 = 10035;
pub const WS_WSAEINPROGRESS: i32 = 10036;
pub const WS_WSAEALREADY: i32 = 10037;
pub const WS_WSAENOTSOCK: i32 = 10038;
pub const WS_WSAEDESTADDRREQ: i32 = 10039;
pub const WS_WSAEMSGSIZE: i32 = 10040;
pub const WS_WSAEPROTOTYPE: i32 = 10041;
pub const WS_WSAENOPROTOOPT: i32 = 10042;
pub const WS_WSAEPROTONOSUPPORT: i32 = 10043;
pub const WS_WSAESOCKTNOSUPPORT: i32 = 10044;
pub const WS_WSAEOPNOTSUPP: i32 = 10045;
pub const WS_WSAEPFNOSUPPORT: i32 = 10046;
pub const WS_WSAEAFNOSUPPORT: i32 = 10047;
pub const WS_WSAEADDRINUSE: i32 = 10048;
pub const WS_WSAEADDRNOTAVAIL: i32 = 10049;
pub const WS_WSAENETDOWN: i32 = 10050;
pub const WS_WSAENETUNREACH: i32 = 10051;
pub const WS_WSAENETRESET: i32 = 10052;
pub const WS_WSAECONNABORTED: i32 = 10053;
pub const WS_WSAECONNRESET: i32 = 10054;
pub const WS_WSAENOBUFS: i32 = 10055;
pub const WS_WSAEISCONN: i32 = 10056;
pub const WS_WSAENOTCONN: i32 = 10057;
pub const WS_WSAESHUTDOWN: i32 = 10058;
pub const WS_WSAETOOMANYREFS: i32 = 10059;
pub const WS_WSAETIMEDOUT: i32 = 10060;
pub const WS_WSAECONNREFUSED: i32 = 10061;
pub const WS_WSAELOOP: i32 = 10062;
pub const WS_WSAENAMETOOLONG: i32 = 10063;
pub const WS_WSAEHOSTDOWN: i32 = 10064;
pub const WS_WSAEHOSTUNREACH: i32 = 10065;
pub const WS_WSAENOTEMPTY: i32 = 10066;
pub const WS_WSAEPROCLIM: i32 = 10067;
pub const WS_WSAEUSERS: i32 = 10068;
pub const WS_WSAEDQUOT: i32 = 10069;
pub const WS_WSAESTALE: i32 = 10070;
pub const WS_WSAEREMOTE: i32 = 10071;
pub const WS_WSASYSNOTREADY: i32 = 10091;
pub const WS_WSAVERNOTSUPPORTED: i32 = 10092;
pub const WS_WSANOTINITIALISED: i32 = 10093;
pub const WS_WSAEDISCON: i32 = 10101;
pub const WS_Pending: i32 = 100000;
pub const WS_TimedOut: i32 = 100001;
pub const WS_Killed: i32 = 100002;
pub const WS_Incomplete: i32 = 100003;
pub const WS_None: i32 = 100004;
pub const WS_AwaitingCompletion: i32 = 100005;
pub const WS_InvalidMessage: i32 = 100006;
pub const WS_FailedToOpenFile: i32 = 100007;
pub const WS_FailedToWriteToFile: i32 = 100008;
pub const WS_BlockMode_NotSupported: i32 = 100009;
pub const WS_AsyncMode_NotSupported: i32 = 100010;
pub const WS_InvalidPublicKey: i32 = 100011;
pub const WS_MessageUnpackFailure: i32 = 100012;
pub const WS_AuthRequired: i32 = 100013;
pub const WS_InvalidAddress: i32 = 100014;
pub const WS_NoReply: i32 = 100015;
pub const WS_AsyncSocket_InvalidSocket: i32 = 100016;
pub const WS_AsyncSocket_PartialSendTo: i32 = 100017;
pub const WS_AsyncSocket_StreamNotAllowed: i32 = 100018;
pub const WS_AsyncSocket_DatagramNotAllowed: i32 = 100019;
pub const WS_AsyncSocket_Shutdown: i32 = 100020;
pub const WS_AsyncSocket_ConnectFailed: i32 = 100021;
pub const WS_SocketOp_InvalidSocket: i32 = 100022;
pub const WS_RecvMsg_InvalidLengthFieldSize: i32 = 100023;
pub const WS_RecvMsg_InvalidMessageLength: i32 = 100024;
pub const WS_PeerAuthClient_Challenge1DecryptFailure: i32 = 100025;
pub const WS_PeerAuthClient_Challenge1InvalidSecretLen: i32 = 100026;
pub const WS_PeerAuthClient_Challenge1InvalidSecretKey: i32 = 100027;
pub const WS_PeerAuthClient_Challenge1CertificateUnpackFailure: i32 = 100028;
pub const WS_PeerAuthClient_Challenge1CertificateVerifyFailure: i32 = 100029;
pub const WS_PeerAuthClient_Challenge2EncryptFailure: i32 = 100030;
pub const WS_PeerAuthClient_CompleteDecryptFailure: i32 = 100031;
pub const WS_PeerAuthClient_CompleteInvalidSecretLen: i32 = 100032;
pub const WS_PeerAuthClient_CompleteInvalidSecretKey: i32 = 100033;
pub const WS_PeerAuthClient_InvalidServiceType: i32 = 100034;
pub const WS_PeerAuthClient_UnexpectedChallenge: i32 = 100035;
pub const WS_PeerAuthClient_InvalidMessage: i32 = 100036;
pub const WS_PeerAuthClient_MsgUnpackFailure: i32 = 100037;
pub const WS_PeerAuthServer_NotStarted: i32 = 100038;
pub const WS_PeerAuthServer_InvalidServiceType: i32 = 100039;
pub const WS_PeerAuthServer_UnexpectedRequest: i32 = 100040;
pub const WS_PeerAuthServer_UnexpectedChallenge: i32 = 100041;
pub const WS_PeerAuthServer_InvalidMessage: i32 = 100042;
pub const WS_PeerAuthServer_MsgUnpackFailure: i32 = 100043;
pub const WS_PeerAuthServer_InvalidAuthMode: i32 = 100044;
pub const WS_PeerAuthServer_InvalidEncryptType: i32 = 100045;
pub const WS_PeerAuthServer_InvalidClientCertificate: i32 = 100046;
pub const WS_PeerAuthServer_ExpiredClientCertificate: i32 = 100047;
pub const WS_PeerAuthServer_FailedToVerifyClientCertificate: i32 = 100048;
pub const WS_PeerAuthServer_FailedToEncryptWithClientPubKey: i32 = 100049;
pub const WS_PeerAuthServer_FailedToDecryptWithPrivateKey: i32 = 100050;
pub const WS_PeerAuthServer_InvalidSecretA: i32 = 100051;
pub const WS_PeerAuthServer_InvalidSecretB: i32 = 100052;
pub const WS_PeerAuth_GetCertFailure: i32 = 100053;
pub const WS_ServerReq_FailedAllServers: i32 = 100054;
pub const WS_ServerReq_NoServersSpecified: i32 = 100055;
pub const WS_ServerReq_NeedAuthContext: i32 = 100056;
pub const WS_ServerReq_TryNextServer: i32 = 100057;
pub const WS_ServerReq_Send: i32 = 100058;
pub const WS_ServerReq_Recv: i32 = 100059;
pub const WS_ServerReq_ExitCommunicationLoop: i32 = 100060;
pub const WS_ServerReq_SessionExpired: i32 = 100061;
pub const WS_ServerReq_GetCertFailure: i32 = 100062;
pub const WS_ServerReq_InvalidReplyHeader: i32 = 100063;
pub const WS_ServerReq_UnpackFailure: i32 = 100064;
pub const WS_GetCert_PubKeyEncryptFailure: i32 = 100065;
pub const WS_GetCert_InvalidPubKeyReply: i32 = 100066;
pub const WS_GetCert_InvalidLoginReply: i32 = 100067;
pub const WS_GetCert_UnableToFindHashFile: i32 = 100068;
pub const WS_GetCert_UnexpectedLoginChallenge: i32 = 100069;
pub const WS_GetCert_NeedVerifierKey: i32 = 100070;
pub const WS_GetCert_FailedToVerifyPubKeyBlock: i32 = 100071;
pub const WS_GetCert_InvalidCertificate: i32 = 100072;
pub const WS_GetCert_InvalidPubKeyBlock: i32 = 100073;
pub const WS_GetCert_DecryptFailure: i32 = 100074;
pub const WS_GetCert_InvalidPrivateKey: i32 = 100075;
pub const WS_GetCert_InvalidSecretConfirm: i32 = 100076;
pub const WS_GetCert_MissingCertificate: i32 = 100077;
pub const WS_AuthData_SetCertificateUnpackFailed: i32 = 100078;
pub const WS_AuthData_SetPubKeyBlockUnpackFailed: i32 = 100079;
pub const WS_AuthData_SetCertificateVerifyFailed: i32 = 100080;
pub const WS_AuthData_SetPrivateKeyFailed: i32 = 100081;
pub const WS_AuthSession_EncryptFailure: i32 = 100082;
pub const WS_AuthSession_DecryptSessionIdMismatch: i32 = 100083;
pub const WS_AuthSession_DecryptFailure: i32 = 100084;
pub const WS_AuthSession_DecryptBadLen: i32 = 100085;
pub const WS_AuthSession_DecryptInvalidSequenceNum: i32 = 100086;
pub const WS_AuthSession_DecryptUnpackFailure: i32 = 100087;
pub const WS_ServerConnection_RecvNotAllowedInAsyncMode: i32 = 100088;
pub const WS_FTP_InvalidResponse: i32 = 100089;
pub const WS_FTP_StatusError: i32 = 100090;
pub const WS_FTP_InvalidPasvResponse: i32 = 100091;
pub const WS_HTTP_InvalidHeader: i32 = 100092;
pub const WS_HTTP_StatusError: i32 = 100093;
pub const WS_HTTP_InvalidRedirect: i32 = 100094;
pub const WS_HTTP_TooManyRedirects: i32 = 100095;
pub const WS_HTTP_Redirected: i32 = 100096;
pub const WS_HTTP_SendSuccess: i32 = 100097;
pub const WS_GetMOTD_SysNotFound: i32 = 100098;
pub const WS_GetMOTD_GameNotFound: i32 = 100099;
pub const WS_BaseMsgRequest_NoRequest: i32 = 100100;
pub const WS_BaseMsgRequest_UnpackFailure: i32 = 100101;
pub const WS_BaseMsgRequest_BadHeaderType: i32 = 100102;
pub const WS_RoutingOp_ReplyUnpackFailure: i32 = 100103;
pub const WS_RoutingOp_DontWantReply: i32 = 100104;
pub const WS_RoutingOp_NeedMoreReplies: i32 = 100105;
pub const WS_GameSpySupport_UnhandledError: i32 = 100106;
pub const WS_GameSpySupport_WinSockError: i32 = 100107;
pub const WS_GameSpySupport_BindError: i32 = 100108;
pub const WS_GameSpySupport_DNSError: i32 = 100109;
pub const WS_GameSpySupport_ConnError: i32 = 100110;

/// Every name and value, in the header's order (the tests')
#[cfg(test)]
pub(crate) const ALL: &[(&str, i32)] = &[
    ("WS_DBProxyServ_GameSpecificErrorEnd", WS_DBProxyServ_GameSpecificErrorEnd),
    ("WS_DBProxyServ_GameSpecificWarriorNameDoesNotExist", WS_DBProxyServ_GameSpecificWarriorNameDoesNotExist),
    ("WS_DBProxyServ_GameSpecificErrorStart", WS_DBProxyServ_GameSpecificErrorStart),
    ("WS_DBProxyServ_DataAlreadyExists", WS_DBProxyServ_DataAlreadyExists),
    ("WS_DBProxyServ_ValidNotLatest", WS_DBProxyServ_ValidNotLatest),
    ("WS_DBProxyServ_OutOfDateNoUpdate", WS_DBProxyServ_OutOfDateNoUpdate),
    ("WS_DBProxyServ_OutOfDate", WS_DBProxyServ_OutOfDate),
    ("WS_DBProxyServ_ItemHasDependencies", WS_DBProxyServ_ItemHasDependencies),
    ("WS_DBProxyServ_AgeNotOldEnough", WS_DBProxyServ_AgeNotOldEnough),
    ("WS_DBProxyServ_InvalidPassword", WS_DBProxyServ_InvalidPassword),
    ("WS_DBProxyServ_AccountCreateDisabled", WS_DBProxyServ_AccountCreateDisabled),
    ("WS_DBProxyServ_EMailPasswordDuplication", WS_DBProxyServ_EMailPasswordDuplication),
    ("WS_DBProxyServ_KeyNotUsed", WS_DBProxyServ_KeyNotUsed),
    ("WS_DBProxyServ_InvalidCDKey", WS_DBProxyServ_InvalidCDKey),
    ("WS_DBProxyServ_UserDoesNotExist", WS_DBProxyServ_UserDoesNotExist),
    ("WS_DBProxyServ_KeyInUse", WS_DBProxyServ_KeyInUse),
    ("WS_DBProxyServ_InvalidUserName", WS_DBProxyServ_InvalidUserName),
    ("WS_DBProxyServ_UserExists", WS_DBProxyServ_UserExists),
    ("WS_DBProxyServ_NoModule", WS_DBProxyServ_NoModule),
    ("WS_DBProxyServ_NoData", WS_DBProxyServ_NoData),
    ("WS_DBProxyServ_DBError", WS_DBProxyServ_DBError),
    ("WS_ProfileServ_RemoveNewsLettersFailed", WS_ProfileServ_RemoveNewsLettersFailed),
    ("WS_ProfileServ_NoNewslettersProvided", WS_ProfileServ_NoNewslettersProvided),
    ("WS_ProfileServ_TooManyNewsletters", WS_ProfileServ_TooManyNewsletters),
    ("WS_ProfileServ_UnfoundNewsletterName", WS_ProfileServ_UnfoundNewsletterName),
    ("WS_ProfileServ_OtherError", WS_ProfileServ_OtherError),
    ("WS_ProfileServ_AccountCreationDisabled", WS_ProfileServ_AccountCreationDisabled),
    ("WS_ProfileServ_InvalidWONUserName", WS_ProfileServ_InvalidWONUserName),
    ("WS_ProfileServ_DatabaseError", WS_ProfileServ_DatabaseError),
    ("WS_ProfileServ_WONUserAlreadyExists", WS_ProfileServ_WONUserAlreadyExists),
    ("WS_ProfileServ_BadWONUserPassword", WS_ProfileServ_BadWONUserPassword),
    ("WS_ProfileServ_PartySourceEntryExists", WS_ProfileServ_PartySourceEntryExists),
    ("WS_ProfileServ_NewsletterEntryExists", WS_ProfileServ_NewsletterEntryExists),
    ("WS_ProfileServ_EmailAlreadyUsed", WS_ProfileServ_EmailAlreadyUsed),
    ("WS_ProfileServ_PartySeqAlreadySet", WS_ProfileServ_PartySeqAlreadySet),
    ("WS_ProfileServ_CountryNotFound", WS_ProfileServ_CountryNotFound),
    ("WS_ProfileServ_AgeRangeNotFound", WS_ProfileServ_AgeRangeNotFound),
    ("WS_ProfileServ_UserNotFound", WS_ProfileServ_UserNotFound),
    ("WS_ProfileServ_ProfileDoesNotExist", WS_ProfileServ_ProfileDoesNotExist),
    ("WS_RoutingServG2_JoinCanceled", WS_RoutingServG2_JoinCanceled),
    ("WS_RoutingServG2_GroupNameExceedsMaximum", WS_RoutingServG2_GroupNameExceedsMaximum),
    ("WS_RoutingServG2_TooManyRecipients", WS_RoutingServG2_TooManyRecipients),
    ("WS_RoutingServG2_TooManyMemberships", WS_RoutingServG2_TooManyMemberships),
    ("WS_RoutingServG2_Throttled", WS_RoutingServG2_Throttled),
    ("WS_RoutingServG2_SubscriptionDoesNotExists", WS_RoutingServG2_SubscriptionDoesNotExists),
    ("WS_RoutingServG2_SubscriptionDoesNotExist", WS_RoutingServG2_SubscriptionDoesNotExist),
    ("WS_RoutingServG2_SubscriptionAlreadyExists", WS_RoutingServG2_SubscriptionAlreadyExists),
    ("WS_RoutingServG2_ShutdownTimerDoesNotExist", WS_RoutingServG2_ShutdownTimerDoesNotExist),
    ("WS_RoutingServG2_ShutdownTimerAlreadyExists", WS_RoutingServG2_ShutdownTimerAlreadyExists),
    ("WS_RoutingServG2_ServerFull", WS_RoutingServG2_ServerFull),
    ("WS_RoutingServG2_Pending", WS_RoutingServG2_Pending),
    ("WS_RoutingServG2_OffsetTooLarge", WS_RoutingServG2_OffsetTooLarge),
    ("WS_RoutingServG2_ObserversNotAllowed", WS_RoutingServG2_ObserversNotAllowed),
    ("WS_RoutingServG2_ObjectDoesNotExist", WS_RoutingServG2_ObjectDoesNotExist),
    ("WS_RoutingServG2_NoUsernameInCertificate", WS_RoutingServG2_NoUsernameInCertificate),
    ("WS_RoutingServG2_NoPendingJoin", WS_RoutingServG2_NoPendingJoin),
    ("WS_RoutingServG2_MustBeVisible", WS_RoutingServG2_MustBeVisible),
    ("WS_RoutingServG2_MustBeSelf", WS_RoutingServG2_MustBeSelf),
    ("WS_RoutingServG2_MustBeModerator", WS_RoutingServG2_MustBeModerator),
    ("WS_RoutingServG2_MustBeClient", WS_RoutingServG2_MustBeClient),
    ("WS_RoutingServG2_MustBeCaptainOrModerator", WS_RoutingServG2_MustBeCaptainOrModerator),
    ("WS_RoutingServG2_MustBeCaptain", WS_RoutingServG2_MustBeCaptain),
    ("WS_RoutingServG2_MustBeAuthenticated", WS_RoutingServG2_MustBeAuthenticated),
    ("WS_RoutingServG2_MustBeAdmin", WS_RoutingServG2_MustBeAdmin),
    ("WS_RoutingServG2_LoginTypeNotSupported", WS_RoutingServG2_LoginTypeNotSupported),
    ("WS_RoutingServG2_InvalidWONUserId", WS_RoutingServG2_InvalidWONUserId),
    ("WS_RoutingServG2_InvalidPassword", WS_RoutingServG2_InvalidPassword),
    ("WS_RoutingServG2_InvalidContentType", WS_RoutingServG2_InvalidContentType),
    ("WS_RoutingServG2_GuestNamesReserved", WS_RoutingServG2_GuestNamesReserved),
    ("WS_RoutingServG2_GroupFull", WS_RoutingServG2_GroupFull),
    ("WS_RoutingServG2_GroupDoesNotExist", WS_RoutingServG2_GroupDoesNotExist),
    ("WS_RoutingServG2_GroupDeleted", WS_RoutingServG2_GroupDeleted),
    ("WS_RoutingServG2_GroupClosed", WS_RoutingServG2_GroupClosed),
    ("WS_RoutingServG2_GroupAlreadyExists", WS_RoutingServG2_GroupAlreadyExists),
    ("WS_RoutingServG2_GroupAlreadyClaimed", WS_RoutingServG2_GroupAlreadyClaimed),
    ("WS_RoutingServG2_DuplicateDataObject", WS_RoutingServG2_DuplicateDataObject),
    ("WS_RoutingServG2_ConnectFailure", WS_RoutingServG2_ConnectFailure),
    ("WS_RoutingServG2_ClientNotPermitted", WS_RoutingServG2_ClientNotPermitted),
    ("WS_RoutingServG2_ClientNotMuted", WS_RoutingServG2_ClientNotMuted),
    ("WS_RoutingServG2_ClientNotInvited", WS_RoutingServG2_ClientNotInvited),
    ("WS_RoutingServG2_ClientNotInGroup", WS_RoutingServG2_ClientNotInGroup),
    ("WS_RoutingServG2_ClientNotBanned", WS_RoutingServG2_ClientNotBanned),
    ("WS_RoutingServG2_ClientDoesNotExist", WS_RoutingServG2_ClientDoesNotExist),
    ("WS_RoutingServG2_ClientBanned", WS_RoutingServG2_ClientBanned),
    ("WS_RoutingServG2_ClientAlreadyRegistered", WS_RoutingServG2_ClientAlreadyRegistered),
    ("WS_RoutingServG2_ClientAlreadyMuted", WS_RoutingServG2_ClientAlreadyMuted),
    ("WS_RoutingServG2_ClientAlreadyInGroup", WS_RoutingServG2_ClientAlreadyInGroup),
    ("WS_RoutingServG2_ClientAlreadyExists", WS_RoutingServG2_ClientAlreadyExists),
    ("WS_RoutingServG2_ClientAlreadyBanned", WS_RoutingServG2_ClientAlreadyBanned),
    ("WS_RoutingServG2_CaptainRejectedYou", WS_RoutingServG2_CaptainRejectedYou),
    ("WS_RoutingServG2_BlankNamesNotAllowed", WS_RoutingServG2_BlankNamesNotAllowed),
    ("WS_EventServ_InvalidDataType", WS_EventServ_InvalidDataType),
    ("WS_EventServ_InvalidDateTime", WS_EventServ_InvalidDateTime),
    ("WS_EventServ_BufferingError", WS_EventServ_BufferingError),
    ("WS_EventServ_FailureStart", WS_EventServ_FailureStart),
    ("WS_AuthServ_SetCUDError", WS_AuthServ_SetCUDError),
    ("WS_AuthServ_UserSeqInUse", WS_AuthServ_UserSeqInUse),
    ("WS_AuthServ_NotInCommunity", WS_AuthServ_NotInCommunity),
    ("WS_AuthServ_InvalidCDKey", WS_AuthServ_InvalidCDKey),
    ("WS_AuthServ_BadCommunity", WS_AuthServ_BadCommunity),
    ("WS_AuthServ_InvalidUserName", WS_AuthServ_InvalidUserName),
    ("WS_AuthServ_BadPassword", WS_AuthServ_BadPassword),
    ("WS_AuthServ_UserNotFound", WS_AuthServ_UserNotFound),
    ("WS_AuthServ_UserExists", WS_AuthServ_UserExists),
    ("WS_AuthServ_CRCFailed", WS_AuthServ_CRCFailed),
    ("WS_AuthServ_CDKeyInUse", WS_AuthServ_CDKeyInUse),
    ("WS_AuthServ_CDKeyBanned", WS_AuthServ_CDKeyBanned),
    ("WS_AuthServ_BadCDKey", WS_AuthServ_BadCDKey),
    ("WS_AuthServ_BadPubKeyBlock", WS_AuthServ_BadPubKeyBlock),
    ("WS_AuthServ_FailureStart", WS_AuthServ_FailureStart),
    ("WS_DirServ_MultiGetFailedAllRequests", WS_DirServ_MultiGetFailedAllRequests),
    ("WS_DirServ_MultiGetPartialFailure", WS_DirServ_MultiGetPartialFailure),
    ("WS_DirServ_NoACLs", WS_DirServ_NoACLs),
    ("WS_DirServ_MaxPermissions", WS_DirServ_MaxPermissions),
    ("WS_DirServ_PermissionNotFound", WS_DirServ_PermissionNotFound),
    ("WS_DirServ_PermissionExists", WS_DirServ_PermissionExists),
    ("WS_DirServ_InvalidACLType", WS_DirServ_InvalidACLType),
    ("WS_DirServ_DataObjectNotFound", WS_DirServ_DataObjectNotFound),
    ("WS_DirServ_DataObjectExists", WS_DirServ_DataObjectExists),
    ("WS_DirServ_NoDataObjects", WS_DirServ_NoDataObjects),
    ("WS_DirServ_InvalidDataObject", WS_DirServ_InvalidDataObject),
    ("WS_DirServ_InvalidLifespan", WS_DirServ_InvalidLifespan),
    ("WS_DirServ_InvalidMode", WS_DirServ_InvalidMode),
    ("WS_DirServ_InvalidKey", WS_DirServ_InvalidKey),
    ("WS_DirServ_InvalidGetFlags", WS_DirServ_InvalidGetFlags),
    ("WS_DirServ_InvalidPath", WS_DirServ_InvalidPath),
    ("WS_DirServ_BadDataOffset", WS_DirServ_BadDataOffset),
    ("WS_DirServ_MaxDataObjects", WS_DirServ_MaxDataObjects),
    ("WS_DirServ_EntityTooLarge", WS_DirServ_EntityTooLarge),
    ("WS_DirServ_AlreadyConnected", WS_DirServ_AlreadyConnected),
    ("WS_DirServ_DirIsFull", WS_DirServ_DirIsFull),
    ("WS_DirServ_ServiceExists", WS_DirServ_ServiceExists),
    ("WS_DirServ_DirExists", WS_DirServ_DirExists),
    ("WS_DirServ_ServiceNotFound", WS_DirServ_ServiceNotFound),
    ("WS_DirServ_DirNotFound", WS_DirServ_DirNotFound),
    ("WS_DirServ_FailureStart", WS_DirServ_FailureStart),
    ("WS_CommServ_StatusCommon_BannedAddress", WS_CommServ_StatusCommon_BannedAddress),
    ("WS_CommServ_ExpiredPeerCertificate", WS_CommServ_ExpiredPeerCertificate),
    ("WS_CommServ_BadPeerCertificate", WS_CommServ_BadPeerCertificate),
    ("WS_CommServ_BehindFirewall", WS_CommServ_BehindFirewall),
    ("WS_CommServ_InvalidMessage", WS_CommServ_InvalidMessage),
    ("WS_CommServ_InvalidSequenceNumber", WS_CommServ_InvalidSequenceNumber),
    ("WS_CommServ_InvalidSessionId", WS_CommServ_InvalidSessionId),
    ("WS_CommServ_DecryptionFailed", WS_CommServ_DecryptionFailed),
    ("WS_CommServ_NotSupported", WS_CommServ_NotSupported),
    ("WS_CommServ_TimedOut", WS_CommServ_TimedOut),
    ("WS_CommServ_NoPortAvailable", WS_CommServ_NoPortAvailable),
    ("WS_CommServ_ResourcesLow", WS_CommServ_ResourcesLow),
    ("WS_CommServ_NotAuthorized", WS_CommServ_NotAuthorized),
    ("WS_CommServ_SessionNotFound", WS_CommServ_SessionNotFound),
    ("WS_CommServ_MessageExceedsMax", WS_CommServ_MessageExceedsMax),
    ("WS_CommServ_AlreadyExists", WS_CommServ_AlreadyExists),
    ("WS_CommServ_InvalidParameters", WS_CommServ_InvalidParameters),
    ("WS_CommServ_NotAllowed", WS_CommServ_NotAllowed),
    ("WS_CommServ_NotAvailable", WS_CommServ_NotAvailable),
    ("WS_CommServ_Failure", WS_CommServ_Failure),
    ("WS_Success", WS_Success),
    ("WS_WSAEINTR", WS_WSAEINTR),
    ("WS_WSAEBADF", WS_WSAEBADF),
    ("WS_WSAEACCES", WS_WSAEACCES),
    ("WS_WSAEFAULT", WS_WSAEFAULT),
    ("WS_WSAEINVAL", WS_WSAEINVAL),
    ("WS_WSAEMFILE", WS_WSAEMFILE),
    ("WS_WSAEWOULDBLOCK", WS_WSAEWOULDBLOCK),
    ("WS_WSAEINPROGRESS", WS_WSAEINPROGRESS),
    ("WS_WSAEALREADY", WS_WSAEALREADY),
    ("WS_WSAENOTSOCK", WS_WSAENOTSOCK),
    ("WS_WSAEDESTADDRREQ", WS_WSAEDESTADDRREQ),
    ("WS_WSAEMSGSIZE", WS_WSAEMSGSIZE),
    ("WS_WSAEPROTOTYPE", WS_WSAEPROTOTYPE),
    ("WS_WSAENOPROTOOPT", WS_WSAENOPROTOOPT),
    ("WS_WSAEPROTONOSUPPORT", WS_WSAEPROTONOSUPPORT),
    ("WS_WSAESOCKTNOSUPPORT", WS_WSAESOCKTNOSUPPORT),
    ("WS_WSAEOPNOTSUPP", WS_WSAEOPNOTSUPP),
    ("WS_WSAEPFNOSUPPORT", WS_WSAEPFNOSUPPORT),
    ("WS_WSAEAFNOSUPPORT", WS_WSAEAFNOSUPPORT),
    ("WS_WSAEADDRINUSE", WS_WSAEADDRINUSE),
    ("WS_WSAEADDRNOTAVAIL", WS_WSAEADDRNOTAVAIL),
    ("WS_WSAENETDOWN", WS_WSAENETDOWN),
    ("WS_WSAENETUNREACH", WS_WSAENETUNREACH),
    ("WS_WSAENETRESET", WS_WSAENETRESET),
    ("WS_WSAECONNABORTED", WS_WSAECONNABORTED),
    ("WS_WSAECONNRESET", WS_WSAECONNRESET),
    ("WS_WSAENOBUFS", WS_WSAENOBUFS),
    ("WS_WSAEISCONN", WS_WSAEISCONN),
    ("WS_WSAENOTCONN", WS_WSAENOTCONN),
    ("WS_WSAESHUTDOWN", WS_WSAESHUTDOWN),
    ("WS_WSAETOOMANYREFS", WS_WSAETOOMANYREFS),
    ("WS_WSAETIMEDOUT", WS_WSAETIMEDOUT),
    ("WS_WSAECONNREFUSED", WS_WSAECONNREFUSED),
    ("WS_WSAELOOP", WS_WSAELOOP),
    ("WS_WSAENAMETOOLONG", WS_WSAENAMETOOLONG),
    ("WS_WSAEHOSTDOWN", WS_WSAEHOSTDOWN),
    ("WS_WSAEHOSTUNREACH", WS_WSAEHOSTUNREACH),
    ("WS_WSAENOTEMPTY", WS_WSAENOTEMPTY),
    ("WS_WSAEPROCLIM", WS_WSAEPROCLIM),
    ("WS_WSAEUSERS", WS_WSAEUSERS),
    ("WS_WSAEDQUOT", WS_WSAEDQUOT),
    ("WS_WSAESTALE", WS_WSAESTALE),
    ("WS_WSAEREMOTE", WS_WSAEREMOTE),
    ("WS_WSASYSNOTREADY", WS_WSASYSNOTREADY),
    ("WS_WSAVERNOTSUPPORTED", WS_WSAVERNOTSUPPORTED),
    ("WS_WSANOTINITIALISED", WS_WSANOTINITIALISED),
    ("WS_WSAEDISCON", WS_WSAEDISCON),
    ("WS_Pending", WS_Pending),
    ("WS_TimedOut", WS_TimedOut),
    ("WS_Killed", WS_Killed),
    ("WS_Incomplete", WS_Incomplete),
    ("WS_None", WS_None),
    ("WS_AwaitingCompletion", WS_AwaitingCompletion),
    ("WS_InvalidMessage", WS_InvalidMessage),
    ("WS_FailedToOpenFile", WS_FailedToOpenFile),
    ("WS_FailedToWriteToFile", WS_FailedToWriteToFile),
    ("WS_BlockMode_NotSupported", WS_BlockMode_NotSupported),
    ("WS_AsyncMode_NotSupported", WS_AsyncMode_NotSupported),
    ("WS_InvalidPublicKey", WS_InvalidPublicKey),
    ("WS_MessageUnpackFailure", WS_MessageUnpackFailure),
    ("WS_AuthRequired", WS_AuthRequired),
    ("WS_InvalidAddress", WS_InvalidAddress),
    ("WS_NoReply", WS_NoReply),
    ("WS_AsyncSocket_InvalidSocket", WS_AsyncSocket_InvalidSocket),
    ("WS_AsyncSocket_PartialSendTo", WS_AsyncSocket_PartialSendTo),
    ("WS_AsyncSocket_StreamNotAllowed", WS_AsyncSocket_StreamNotAllowed),
    ("WS_AsyncSocket_DatagramNotAllowed", WS_AsyncSocket_DatagramNotAllowed),
    ("WS_AsyncSocket_Shutdown", WS_AsyncSocket_Shutdown),
    ("WS_AsyncSocket_ConnectFailed", WS_AsyncSocket_ConnectFailed),
    ("WS_SocketOp_InvalidSocket", WS_SocketOp_InvalidSocket),
    ("WS_RecvMsg_InvalidLengthFieldSize", WS_RecvMsg_InvalidLengthFieldSize),
    ("WS_RecvMsg_InvalidMessageLength", WS_RecvMsg_InvalidMessageLength),
    ("WS_PeerAuthClient_Challenge1DecryptFailure", WS_PeerAuthClient_Challenge1DecryptFailure),
    ("WS_PeerAuthClient_Challenge1InvalidSecretLen", WS_PeerAuthClient_Challenge1InvalidSecretLen),
    ("WS_PeerAuthClient_Challenge1InvalidSecretKey", WS_PeerAuthClient_Challenge1InvalidSecretKey),
    ("WS_PeerAuthClient_Challenge1CertificateUnpackFailure", WS_PeerAuthClient_Challenge1CertificateUnpackFailure),
    ("WS_PeerAuthClient_Challenge1CertificateVerifyFailure", WS_PeerAuthClient_Challenge1CertificateVerifyFailure),
    ("WS_PeerAuthClient_Challenge2EncryptFailure", WS_PeerAuthClient_Challenge2EncryptFailure),
    ("WS_PeerAuthClient_CompleteDecryptFailure", WS_PeerAuthClient_CompleteDecryptFailure),
    ("WS_PeerAuthClient_CompleteInvalidSecretLen", WS_PeerAuthClient_CompleteInvalidSecretLen),
    ("WS_PeerAuthClient_CompleteInvalidSecretKey", WS_PeerAuthClient_CompleteInvalidSecretKey),
    ("WS_PeerAuthClient_InvalidServiceType", WS_PeerAuthClient_InvalidServiceType),
    ("WS_PeerAuthClient_UnexpectedChallenge", WS_PeerAuthClient_UnexpectedChallenge),
    ("WS_PeerAuthClient_InvalidMessage", WS_PeerAuthClient_InvalidMessage),
    ("WS_PeerAuthClient_MsgUnpackFailure", WS_PeerAuthClient_MsgUnpackFailure),
    ("WS_PeerAuthServer_NotStarted", WS_PeerAuthServer_NotStarted),
    ("WS_PeerAuthServer_InvalidServiceType", WS_PeerAuthServer_InvalidServiceType),
    ("WS_PeerAuthServer_UnexpectedRequest", WS_PeerAuthServer_UnexpectedRequest),
    ("WS_PeerAuthServer_UnexpectedChallenge", WS_PeerAuthServer_UnexpectedChallenge),
    ("WS_PeerAuthServer_InvalidMessage", WS_PeerAuthServer_InvalidMessage),
    ("WS_PeerAuthServer_MsgUnpackFailure", WS_PeerAuthServer_MsgUnpackFailure),
    ("WS_PeerAuthServer_InvalidAuthMode", WS_PeerAuthServer_InvalidAuthMode),
    ("WS_PeerAuthServer_InvalidEncryptType", WS_PeerAuthServer_InvalidEncryptType),
    ("WS_PeerAuthServer_InvalidClientCertificate", WS_PeerAuthServer_InvalidClientCertificate),
    ("WS_PeerAuthServer_ExpiredClientCertificate", WS_PeerAuthServer_ExpiredClientCertificate),
    ("WS_PeerAuthServer_FailedToVerifyClientCertificate", WS_PeerAuthServer_FailedToVerifyClientCertificate),
    ("WS_PeerAuthServer_FailedToEncryptWithClientPubKey", WS_PeerAuthServer_FailedToEncryptWithClientPubKey),
    ("WS_PeerAuthServer_FailedToDecryptWithPrivateKey", WS_PeerAuthServer_FailedToDecryptWithPrivateKey),
    ("WS_PeerAuthServer_InvalidSecretA", WS_PeerAuthServer_InvalidSecretA),
    ("WS_PeerAuthServer_InvalidSecretB", WS_PeerAuthServer_InvalidSecretB),
    ("WS_PeerAuth_GetCertFailure", WS_PeerAuth_GetCertFailure),
    ("WS_ServerReq_FailedAllServers", WS_ServerReq_FailedAllServers),
    ("WS_ServerReq_NoServersSpecified", WS_ServerReq_NoServersSpecified),
    ("WS_ServerReq_NeedAuthContext", WS_ServerReq_NeedAuthContext),
    ("WS_ServerReq_TryNextServer", WS_ServerReq_TryNextServer),
    ("WS_ServerReq_Send", WS_ServerReq_Send),
    ("WS_ServerReq_Recv", WS_ServerReq_Recv),
    ("WS_ServerReq_ExitCommunicationLoop", WS_ServerReq_ExitCommunicationLoop),
    ("WS_ServerReq_SessionExpired", WS_ServerReq_SessionExpired),
    ("WS_ServerReq_GetCertFailure", WS_ServerReq_GetCertFailure),
    ("WS_ServerReq_InvalidReplyHeader", WS_ServerReq_InvalidReplyHeader),
    ("WS_ServerReq_UnpackFailure", WS_ServerReq_UnpackFailure),
    ("WS_GetCert_PubKeyEncryptFailure", WS_GetCert_PubKeyEncryptFailure),
    ("WS_GetCert_InvalidPubKeyReply", WS_GetCert_InvalidPubKeyReply),
    ("WS_GetCert_InvalidLoginReply", WS_GetCert_InvalidLoginReply),
    ("WS_GetCert_UnableToFindHashFile", WS_GetCert_UnableToFindHashFile),
    ("WS_GetCert_UnexpectedLoginChallenge", WS_GetCert_UnexpectedLoginChallenge),
    ("WS_GetCert_NeedVerifierKey", WS_GetCert_NeedVerifierKey),
    ("WS_GetCert_FailedToVerifyPubKeyBlock", WS_GetCert_FailedToVerifyPubKeyBlock),
    ("WS_GetCert_InvalidCertificate", WS_GetCert_InvalidCertificate),
    ("WS_GetCert_InvalidPubKeyBlock", WS_GetCert_InvalidPubKeyBlock),
    ("WS_GetCert_DecryptFailure", WS_GetCert_DecryptFailure),
    ("WS_GetCert_InvalidPrivateKey", WS_GetCert_InvalidPrivateKey),
    ("WS_GetCert_InvalidSecretConfirm", WS_GetCert_InvalidSecretConfirm),
    ("WS_GetCert_MissingCertificate", WS_GetCert_MissingCertificate),
    ("WS_AuthData_SetCertificateUnpackFailed", WS_AuthData_SetCertificateUnpackFailed),
    ("WS_AuthData_SetPubKeyBlockUnpackFailed", WS_AuthData_SetPubKeyBlockUnpackFailed),
    ("WS_AuthData_SetCertificateVerifyFailed", WS_AuthData_SetCertificateVerifyFailed),
    ("WS_AuthData_SetPrivateKeyFailed", WS_AuthData_SetPrivateKeyFailed),
    ("WS_AuthSession_EncryptFailure", WS_AuthSession_EncryptFailure),
    ("WS_AuthSession_DecryptSessionIdMismatch", WS_AuthSession_DecryptSessionIdMismatch),
    ("WS_AuthSession_DecryptFailure", WS_AuthSession_DecryptFailure),
    ("WS_AuthSession_DecryptBadLen", WS_AuthSession_DecryptBadLen),
    ("WS_AuthSession_DecryptInvalidSequenceNum", WS_AuthSession_DecryptInvalidSequenceNum),
    ("WS_AuthSession_DecryptUnpackFailure", WS_AuthSession_DecryptUnpackFailure),
    ("WS_ServerConnection_RecvNotAllowedInAsyncMode", WS_ServerConnection_RecvNotAllowedInAsyncMode),
    ("WS_FTP_InvalidResponse", WS_FTP_InvalidResponse),
    ("WS_FTP_StatusError", WS_FTP_StatusError),
    ("WS_FTP_InvalidPasvResponse", WS_FTP_InvalidPasvResponse),
    ("WS_HTTP_InvalidHeader", WS_HTTP_InvalidHeader),
    ("WS_HTTP_StatusError", WS_HTTP_StatusError),
    ("WS_HTTP_InvalidRedirect", WS_HTTP_InvalidRedirect),
    ("WS_HTTP_TooManyRedirects", WS_HTTP_TooManyRedirects),
    ("WS_HTTP_Redirected", WS_HTTP_Redirected),
    ("WS_HTTP_SendSuccess", WS_HTTP_SendSuccess),
    ("WS_GetMOTD_SysNotFound", WS_GetMOTD_SysNotFound),
    ("WS_GetMOTD_GameNotFound", WS_GetMOTD_GameNotFound),
    ("WS_BaseMsgRequest_NoRequest", WS_BaseMsgRequest_NoRequest),
    ("WS_BaseMsgRequest_UnpackFailure", WS_BaseMsgRequest_UnpackFailure),
    ("WS_BaseMsgRequest_BadHeaderType", WS_BaseMsgRequest_BadHeaderType),
    ("WS_RoutingOp_ReplyUnpackFailure", WS_RoutingOp_ReplyUnpackFailure),
    ("WS_RoutingOp_DontWantReply", WS_RoutingOp_DontWantReply),
    ("WS_RoutingOp_NeedMoreReplies", WS_RoutingOp_NeedMoreReplies),
    ("WS_GameSpySupport_UnhandledError", WS_GameSpySupport_UnhandledError),
    ("WS_GameSpySupport_WinSockError", WS_GameSpySupport_WinSockError),
    ("WS_GameSpySupport_BindError", WS_GameSpySupport_BindError),
    ("WS_GameSpySupport_DNSError", WS_GameSpySupport_DNSError),
    ("WS_GameSpySupport_ConnError", WS_GameSpySupport_ConnError),
];

#[cfg(test)]
mod tests {
    use super::ALL;

    /// Each constant is the code status.rs's table (the C++ names, printed by the original) names after it.
    #[test]
    fn every_code_is_named_after_its_constant() {
        assert_eq!(ALL.len(), 315);
        for &(name, value) in ALL {
            assert_eq!(crate::status::name(value as crate::status::Status), name, "{name}");
        }
    }

    /// The socket errors WON reports are Winsock's, and the platform layer reports them in that numbering
    /// everywhere: each of include/platform/PlatSocket.h's codes with a WSA twin (all but HOST_NOT_FOUND and
    /// OTHER) is that `WS_WSA*` constant's value, and ee_base's (AsyncOp.cpp's 47 static_asserts until R6 WON
    /// round 33).
    #[test]
    fn the_socket_errors_are_winsocks_numbering() {
        let header = include_str!("../../../../include/platform/PlatSocket.h");
        let mut checked = 0;
        for line in header.lines() {
            let Some(rest) = line.strip_prefix("#define PLAT_SOCK_") else { continue };
            let mut parts = rest.split_whitespace();
            let (name, value) = (parts.next().unwrap(), parts.next().unwrap().parse::<i32>().unwrap());
            if name == "HOST_NOT_FOUND" || name == "OTHER" {
                continue;
            }
            let ws = format!("WS_WSA{name}");
            let found = ALL.iter().find(|(n, _)| *n == ws).unwrap_or_else(|| panic!("{ws}"));
            assert_eq!(found.1, value, "{ws} against PLAT_SOCK_{name}");
            checked += 1;
        }
        assert_eq!(checked, 47);
        use ee_base::plat_sys::constants as c;
        for (ws, plat) in [
            (super::WS_WSAEINTR, c::PLAT_SOCK_EINTR),
            (super::WS_WSAEWOULDBLOCK, c::PLAT_SOCK_EWOULDBLOCK),
            (super::WS_WSAECONNREFUSED, c::PLAT_SOCK_ECONNREFUSED),
        ] {
            assert_eq!(ws, plat as i32);
        }
    }
}
