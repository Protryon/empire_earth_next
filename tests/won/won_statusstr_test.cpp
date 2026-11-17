// GetWONStatusString's choice in Rust (R6 WONLobby round 7: ee_won statusstr.rs): the original switch
// (WONStatusString.cpp before the port, its LOBBY_STRING(X) the row X of ee_won's resource table and
// GeneralStatusError(text, status) a general text) against ee_won_status_lobby_text for every status from -200,000 to
// 200,000 and the ends of the range. Every WS_ label of the switch is inside that range.
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include "ee/wonapi/WONStatus.h"
#include "rust/won_lobby/resource.h"
#include "rust/won_lobby/statusstr.h"

struct Text { int mRes; bool mGeneral; };

static Text OrigStatusText(int status)
{
    switch(status) {
    case -2803:
        return Text{(int)EeWonLobbyRes::WONStatus_UserExists_String, false};
    case -2804:
        return Text{(int)EeWonLobbyRes::WONStatus_InvalidUserName_String, false};
    case -2805:
        return Text{(int)EeWonLobbyRes::WONStatus_CDKeyInUse_String, false};
    case -2806:
        return Text{(int)EeWonLobbyRes::WONStatus_UserNotFound_String, false};
    case -2808:
        return Text{(int)EeWonLobbyRes::WONStatus_KeyNotUsed_String, false};
    case -2809:
        return Text{(int)EeWonLobbyRes::WONStatus_EMailPasswordDuplication_String, false};
    case -2810:
        return Text{(int)EeWonLobbyRes::WONStatus_AccountCreateDisabled_String, false};
    case -2811:
        return Text{(int)EeWonLobbyRes::WONStatus_BadPassword_String, false};
    case -2812:
        return Text{(int)EeWonLobbyRes::WONStatus_AgeNotOldEnough_String, false};
    case -2813:
        return Text{(int)EeWonLobbyRes::WONStatus_ItemHasDependencies_String, false};
    case -2814:
        return Text{(int)EeWonLobbyRes::WONStatus_OutOfDate_String, false};
    case -2815:
        return Text{(int)EeWonLobbyRes::WONStatus_OutOfDateNoUpdate_String, false};
    case -2816:
        return Text{(int)EeWonLobbyRes::WONStatus_ValidNotLatest_String, false};
    case -2817:
        return Text{(int)EeWonLobbyRes::WONStatus_DataAlreadyExists_String, false};
    case -2800:
        return Text{(int)EeWonLobbyRes::WONStatus_DBError_String, false};
    case -2802:
    case -2801:
        return Text{(int)EeWonLobbyRes::WONStatus_DatabaseFailure_String, true};
    case -2552:
        return Text{(int)EeWonLobbyRes::WONStatus_JoinCanceled_String, false};
    case -2542:
        return Text{(int)EeWonLobbyRes::WONStatus_ServerFull_String, false};
    case -2551:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupNameExceedsMaximum_String, false};
    case -2548:
        return Text{(int)EeWonLobbyRes::WONStatus_Throttled_String, false};
    case -2532:
        return Text{(int)EeWonLobbyRes::WONStatus_MustBeModerator_String, false};
    case -2500:
        return Text{(int)EeWonLobbyRes::WONStatus_BlankNamesNotAllowed_String, false};
    case -2501:
        return Text{(int)EeWonLobbyRes::WONStatus_CaptainRejectedYou_String, false};
    case -2502:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientAlreadyBanned_String, false};
    case -2504:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientAlreadyInGroup_String, false};
    case -2505:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientAlreadyMuted_String, false};
    case -2507:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientBanned_String, false};
    case -2508:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientDoesNotExist_String, false};
    case -2509:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientNotBanned_String, false};
    case -2510:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientNotInGroup_String, false};
    case -2511:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientNotInvited_String, false};
    case -2512:
        return Text{(int)EeWonLobbyRes::WONStatus_ClientNotMuted_String, false};
    case -2514:
        return Text{(int)EeWonLobbyRes::WONStatus_ConnectFailure_String, false};
    case -2516:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupAlreadyClaimed_String, false};
    case -2517:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupAlreadyExists_String, false};
    case -2518:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupClosed_String, false};
    case -2519:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupDeleted_String, false};
    case -2520:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupDoesNotExist_String, false};
    case -2521:
        return Text{(int)EeWonLobbyRes::WONStatus_GroupFull_String, false};
    case -2522:
        return Text{(int)EeWonLobbyRes::WONStatus_GuestNamesReserved_String, false};
    case -2524:
        return Text{(int)EeWonLobbyRes::WONStatus_InvalidPassword_String, false};
    case -2527:
        return Text{(int)EeWonLobbyRes::WONStatus_MustBeAdmin_String, false};
    case -2529:
        return Text{(int)EeWonLobbyRes::WONStatus_MustBeCaptain_String, false};
    case -2530:
        return Text{(int)EeWonLobbyRes::WONStatus_MustBeCaptainOrModerator_String, false};
    case -2550:
    case -2549:
    case -2547:
    case -2546:
    case -2545:
    case -2544:
    case -2543:
    case -2541:
    case -2540:
    case -2539:
    case -2538:
    case -2537:
    case -2536:
    case -2535:
    case -2534:
    case -2531:
    case -2528:
    case -2526:
    case -2525:
    case -2523:
    case -2515:
    case -2513:
    case -2506:
    case -2503:
        return Text{(int)EeWonLobbyRes::WONStatus_ChatFailure_String, true};
    case -2807:
    case -1510:
    case -1501:
        return Text{(int)EeWonLobbyRes::WONStatus_InvalidCDKey_String, false};
    case -1502:
        return Text{(int)EeWonLobbyRes::WONStatus_CDKeyBanned_String, false};
    case -1503:
        return Text{(int)EeWonLobbyRes::WONStatus_CDKeyInUse_String, false};
    case -1504:
        return Text{(int)EeWonLobbyRes::WONStatus_CRCFailed_String, false};
    case -1505:
        return Text{(int)EeWonLobbyRes::WONStatus_UserExists_String, false};
    case -1506:
        return Text{(int)EeWonLobbyRes::WONStatus_UserNotFound_String, false};
    case -1507:
        return Text{(int)EeWonLobbyRes::WONStatus_BadPassword_String, false};
    case -1508:
        return Text{(int)EeWonLobbyRes::WONStatus_InvalidUserName_String, false};
    case -1509:
        return Text{(int)EeWonLobbyRes::WONStatus_BadCommunity_String, false};
    case -1511:
        return Text{(int)EeWonLobbyRes::WONStatus_NotInCommunity_String, false};
    case -1512:
        return Text{(int)EeWonLobbyRes::WONStatus_UserSeqInUse_String, false};
    case -1513:
    case -1500:
        return Text{(int)EeWonLobbyRes::WONStatus_AuthFailure_String, true};
    case -1224:
    case -1223:
    case -1222:
    case -1221:
    case -1220:
    case -1219:
    case -1218:
    case -1217:
    case -1216:
    case -1215:
    case -1214:
    case -1213:
    case -1212:
    case -1211:
    case -1210:
    case -1209:
    case -1208:
    case -1207:
    case -1206:
    case -1205:
    case -1204:
    case -1203:
    case -1202:
    case -1201:
    case -1200:
        return Text{(int)EeWonLobbyRes::WONStatus_DirFailure_String, true};
    case -8:
        return Text{(int)EeWonLobbyRes::WONStatus_NotAuthorized_String, false};
    case -17:
        return Text{(int)EeWonLobbyRes::WONStatus_BehindFirewall_String, false};
    case -19:
    case -18:
        return Text{(int)EeWonLobbyRes::WONStatus_ExpiredCertificate_String, false};
    case -20:
    case -16:
    case -15:
    case -14:
    case -13:
    case -12:
    case -11:
    case -10:
    case -9:
    case -7:
    case -6:
    case -5:
    case -4:
    case -2:
    case -1:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case -3:
        return Text{(int)EeWonLobbyRes::WONStatus_NotAllowed_String, false};
    case WS_Success:
        return Text{(int)EeWonLobbyRes::WONStatus_Success_String, false};
    case WS_WSAEINTR:
    case WS_WSAEBADF:
    case WS_WSAEACCES:
    case WS_WSAEFAULT:
    case WS_WSAEINVAL:
    case WS_WSAEMFILE:
    case WS_WSAEWOULDBLOCK:
    case WS_WSAEINPROGRESS:
    case WS_WSAEALREADY:
    case WS_WSAENOTSOCK:
    case WS_WSAEDESTADDRREQ:
    case WS_WSAEMSGSIZE:
    case WS_WSAEPROTOTYPE:
    case WS_WSAENOPROTOOPT:
    case WS_WSAEPROTONOSUPPORT:
    case WS_WSAESOCKTNOSUPPORT:
    case WS_WSAEOPNOTSUPP:
    case WS_WSAEPFNOSUPPORT:
    case WS_WSAEAFNOSUPPORT:
    case WS_WSAEADDRINUSE:
    case WS_WSAEADDRNOTAVAIL:
    case WS_WSAENETDOWN:
    case WS_WSAENETUNREACH:
    case WS_WSAENETRESET:
    case WS_WSAECONNABORTED:
    case WS_WSAECONNRESET:
    case WS_WSAENOBUFS:
    case WS_WSAEISCONN:
    case WS_WSAENOTCONN:
    case WS_WSAESHUTDOWN:
    case WS_WSAETOOMANYREFS:
    case WS_WSAETIMEDOUT:
    case WS_WSAELOOP:
    case WS_WSAENAMETOOLONG:
    case WS_WSAEHOSTDOWN:
    case WS_WSAEHOSTUNREACH:
    case WS_WSAENOTEMPTY:
    case WS_WSAEPROCLIM:
    case WS_WSAEUSERS:
    case WS_WSAEDQUOT:
    case WS_WSAESTALE:
    case WS_WSAEREMOTE:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_WSAECONNREFUSED:
        return Text{(int)EeWonLobbyRes::WONStatus_ConnRefused_String, false};
    case WS_TimedOut:
        return Text{(int)EeWonLobbyRes::WONStatus_TimedOut_String, false};
    case WS_Killed:
        return Text{(int)EeWonLobbyRes::WONStatus_Killed_String, false};
    case WS_Pending:
    case WS_Incomplete:
    case WS_None:
    case WS_AwaitingCompletion:
    case WS_InvalidMessage:
    case WS_FailedToOpenFile:
    case WS_FailedToWriteToFile:
    case WS_BlockMode_NotSupported:
    case WS_AsyncMode_NotSupported:
    case WS_InvalidPublicKey:
    case WS_MessageUnpackFailure:
    case WS_AuthRequired:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_InvalidAddress:
        return Text{(int)EeWonLobbyRes::WONStatus_InvalidAddress_String, false};
    case WS_NoReply:
    case WS_AsyncSocket_InvalidSocket:
    case WS_AsyncSocket_PartialSendTo:
    case WS_AsyncSocket_StreamNotAllowed:
    case WS_AsyncSocket_DatagramNotAllowed:
    case WS_AsyncSocket_Shutdown:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_AsyncSocket_ConnectFailed:
        return Text{(int)EeWonLobbyRes::WONStatus_ConnRefused_String, false};
    case WS_SocketOp_InvalidSocket:
    case WS_RecvMsg_InvalidLengthFieldSize:
    case WS_RecvMsg_InvalidMessageLength:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_PeerAuthClient_Challenge1DecryptFailure:
    case WS_PeerAuthClient_Challenge1InvalidSecretLen:
    case WS_PeerAuthClient_Challenge1InvalidSecretKey:
    case WS_PeerAuthClient_Challenge1CertificateUnpackFailure:
    case WS_PeerAuthClient_Challenge1CertificateVerifyFailure:
    case WS_PeerAuthClient_Challenge2EncryptFailure:
    case WS_PeerAuthClient_CompleteDecryptFailure:
    case WS_PeerAuthClient_CompleteInvalidSecretLen:
    case WS_PeerAuthClient_CompleteInvalidSecretKey:
    case WS_PeerAuthClient_InvalidServiceType:
    case WS_PeerAuthClient_UnexpectedChallenge:
    case WS_PeerAuthClient_InvalidMessage:
    case WS_PeerAuthClient_MsgUnpackFailure:
    case WS_PeerAuthServer_NotStarted:
    case WS_PeerAuthServer_InvalidServiceType:
    case WS_PeerAuthServer_UnexpectedRequest:
    case WS_PeerAuthServer_UnexpectedChallenge:
    case WS_PeerAuthServer_InvalidMessage:
    case WS_PeerAuthServer_MsgUnpackFailure:
    case WS_PeerAuthServer_InvalidAuthMode:
    case WS_PeerAuthServer_InvalidEncryptType:
    case WS_PeerAuthServer_InvalidClientCertificate:
    case WS_PeerAuthServer_ExpiredClientCertificate:
    case WS_PeerAuthServer_FailedToVerifyClientCertificate:
    case WS_PeerAuthServer_FailedToEncryptWithClientPubKey:
    case WS_PeerAuthServer_FailedToDecryptWithPrivateKey:
    case WS_PeerAuthServer_InvalidSecretA:
    case WS_PeerAuthServer_InvalidSecretB:
    case WS_PeerAuth_GetCertFailure:
        return Text{(int)EeWonLobbyRes::WONStatus_AuthFailure_String, true};
    case WS_ServerReq_FailedAllServers:
    case WS_ServerReq_NoServersSpecified:
    case WS_ServerReq_NeedAuthContext:
    case WS_ServerReq_TryNextServer:
    case WS_ServerReq_Send:
    case WS_ServerReq_Recv:
    case WS_ServerReq_ExitCommunicationLoop:
    case WS_ServerReq_SessionExpired:
    case WS_ServerReq_GetCertFailure:
    case WS_ServerReq_InvalidReplyHeader:
    case WS_ServerReq_UnpackFailure:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_GetCert_PubKeyEncryptFailure:
    case WS_GetCert_InvalidPubKeyReply:
    case WS_GetCert_InvalidLoginReply:
    case WS_GetCert_UnableToFindHashFile:
    case WS_GetCert_UnexpectedLoginChallenge:
    case WS_GetCert_NeedVerifierKey:
    case WS_GetCert_FailedToVerifyPubKeyBlock:
    case WS_GetCert_InvalidCertificate:
    case WS_GetCert_InvalidPubKeyBlock:
    case WS_GetCert_DecryptFailure:
    case WS_GetCert_InvalidPrivateKey:
    case WS_GetCert_InvalidSecretConfirm:
    case WS_GetCert_MissingCertificate:
    case WS_AuthData_SetCertificateUnpackFailed:
    case WS_AuthData_SetPubKeyBlockUnpackFailed:
    case WS_AuthData_SetCertificateVerifyFailed:
    case WS_AuthData_SetPrivateKeyFailed:
    case WS_AuthSession_EncryptFailure:
    case WS_AuthSession_DecryptSessionIdMismatch:
    case WS_AuthSession_DecryptFailure:
    case WS_AuthSession_DecryptBadLen:
    case WS_AuthSession_DecryptInvalidSequenceNum:
    case WS_AuthSession_DecryptUnpackFailure:
        return Text{(int)EeWonLobbyRes::WONStatus_AuthFailure_String, true};
    case WS_ServerConnection_RecvNotAllowedInAsyncMode:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_FTP_InvalidResponse:
    case WS_FTP_StatusError:
    case WS_FTP_InvalidPasvResponse:
        return Text{(int)EeWonLobbyRes::WONStatus_FTPFailure_String, true};
    case WS_HTTP_InvalidHeader:
    case WS_HTTP_StatusError:
    case WS_HTTP_InvalidRedirect:
    case WS_HTTP_TooManyRedirects:
    case WS_HTTP_Redirected:
    case WS_HTTP_SendSuccess:
        return Text{(int)EeWonLobbyRes::WONStatus_HTTPFailure_String, true};
    case WS_GetMOTD_SysNotFound:
    case WS_GetMOTD_GameNotFound:
        return Text{(int)EeWonLobbyRes::WONStatus_MOTDFailure_String, true};
    case WS_BaseMsgRequest_NoRequest:
    case WS_BaseMsgRequest_UnpackFailure:
    case WS_BaseMsgRequest_BadHeaderType:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    case WS_RoutingOp_ReplyUnpackFailure:
    case WS_RoutingOp_DontWantReply:
    case WS_RoutingOp_NeedMoreReplies:
        return Text{(int)EeWonLobbyRes::WONStatus_RoutingFailure_String, true};
    case WS_GameSpySupport_UnhandledError:
    case WS_GameSpySupport_WinSockError:
    case WS_GameSpySupport_BindError:
    case WS_GameSpySupport_DNSError:
    case WS_GameSpySupport_ConnError:
        return Text{(int)EeWonLobbyRes::WONStatus_GameSpyFailure_String, true};
    default:
        return Text{(int)EeWonLobbyRes::WONStatus_GeneralFailure_String, true};
    }
}

int main()
{
    long long aChecked = 0, aGeneral = 0;
    int aFailures = 0;
    for (long long s = -200001; s <= 200001; s++) {
        int aStatus = s == -200001 ? INT_MIN : s == 200001 ? INT_MAX : (int)s;
        Text a = OrigStatusText(aStatus);
        EeWonStatusLobbyText b = ee_won_status_lobby_text(aStatus);
        aChecked++;
        if (a.mGeneral) aGeneral++;
        if (a.mRes != (int)b.res || a.mGeneral != b.general) {
            if (++aFailures <= 10)
                printf("FAIL status %d: C++ %d %d Rust %d %d\n", aStatus, a.mRes, (int)a.mGeneral, (int)b.res,
                       (int)b.general);
        }
    }
    printf("won_statusstr_test: %lld statuses (%lld general texts), %d failures\n", aChecked, aGeneral, aFailures);
    return aFailures ? 1 : 0;
}
