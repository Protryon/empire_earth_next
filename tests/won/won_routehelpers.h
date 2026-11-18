// RoutingConnection's request helpers as they were in include/ee/wonapi/WONRouting.h before WONLobby round 14
// (each one a RoutingRequestValues run: routecpp.rs's ee_won_routeconn_request), for the drivers that run every
// helper over a connection (won_routeops_test). The game's callers make their requests themselves since then:
// RoutingLogic in Rust (routelogic.rs conn_request), EEWonNet.cpp through its own few. `RoutingRequests r(conn);`
// then `r.JoinGroup(...)`, or `->` through a pointer to it.
#ifndef EE_TESTS_WON_ROUTEHELPERS_H
#define EE_TESTS_WON_ROUTEHELPERS_H
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
class RoutingRequests {
public:
    explicit RoutingRequests(RoutingConnection* inConn) : mConn(inConn) {}
    RoutingRequests* operator->() { return this; }
    void SubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(1).N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_READ_DATA, c); } // 2bf1d0
    void UnsubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_UNSUBSCRIBE_DATA, c); } // 2bf2e0
    void Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inReconnectId,
                  unsigned long inAsyncFlags, unsigned long inRegisterFlags, unsigned long inFlags, RouteCompletion* c)
    {
        RoutingRequestValues().W(inName).W(inPassword).N(inReconnectId).N(inAsyncFlags).N(inRegisterFlags).N(inFlags)
            .Run(mConn, EE_WON_RTQ_REGISTER, c);
    } // 2bcc30
    void CreateGroup(unsigned short inGroupId, const std::wstring& inName, const std::wstring& inPassword,
                     unsigned short inMaxPlayers, unsigned long inGroupFlags, unsigned char inJoinFlags,
                     unsigned long inAsyncFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inGroupId).W(inName).W(inPassword).N(inMaxPlayers).N(inGroupFlags).N(inJoinFlags)
            .N(inAsyncFlags).Run(mConn, EE_WON_RTQ_CREATE_GROUP, c);
    } // 2bd430
    void CreateDataObject(unsigned char inFlags, unsigned short inLinkId, const std::string& inDataType,
                          const std::wstring& inName, const ByteBuffer* inData, RouteCompletion* c)
    { RoutingRequestValues().N(inFlags).N(inLinkId).S(inDataType).W(inName).D(inData).Run(mConn, EE_WON_RTQ_CREATE_DATA, c); } // 2becd0
    // 2bee30: the delete request
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName, RouteCompletion* c)
    { RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).Run(mConn, EE_WON_RTQ_DELETE_DATA, c); }
    void GetGroupList(unsigned char inFlags, RouteCompletion* c)
    { RoutingRequestValues().N(inFlags).Run(mConn, EE_WON_RTQ_GROUP_LIST, c); } // 2bcf90
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inRecipient, RouteCompletion* c)
    { this->SendChatTo(inText, (unsigned short)inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be540
    void SendChatTo(const std::wstring& inText, unsigned short inFlags, const std::list<unsigned short>& inClients,
                    RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inFlags).L(inClients).Run(mConn, EE_WON_RTQ_CHAT, c); } // 2be6a0
    void ModifyDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                          unsigned short inOffset, bool inInsert, const ByteBuffer* inData, RouteCompletion* c)
    {
        RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).N(inOffset).N(inInsert).D(inData)
            .Run(mConn, EE_WON_RTQ_MODIFY_DATA, c);
    } // 2bef50
    void JoinGroup(unsigned short inId, const std::wstring& inPassword, const std::wstring& inComment,
                   unsigned char inJoinFlags, unsigned char inMemberFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inId).W(inPassword).W(inComment).N(inJoinFlags).N(inMemberFlags)
            .Run(mConn, EE_WON_RTQ_JOIN_GROUP, c);
    } // 2bd580
    void CancelJoinGroup(unsigned short inId, bool inLeave, RouteCompletion* c)
    { RoutingRequestValues().N(inId).N(inLeave).Run(mConn, EE_WON_RTQ_CANCEL_JOIN, c); } // 2bd6b0
    void LeaveGroup(unsigned short inId, RouteCompletion* c) { RoutingRequestValues().N(inId).Run(mConn, EE_WON_RTQ_LEAVE, c); } // 2bd850
    void ReadDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, RouteCompletion* c)
    { RoutingRequestValues().N(0).N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_READ_DATA, c); } // 2bf0c0
    void GetClientList(RouteCompletion* c) { RoutingRequestValues().Run(mConn, EE_WON_RTQ_CLIENT_LIST, c); } // 2bcd40
    void GetMembersOfGroup(unsigned short inId, RouteCompletion* c)
    { RoutingRequestValues().N(inId).Run(mConn, EE_WON_RTQ_MEMBERS, c); } // 2bd1e0
    // 2be390: the client mask cut to 16 bits
    void SetClientFlags(unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask, unsigned long inAsyncFlags,
                        RouteCompletion* c)
    { RoutingRequestValues().N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags).Run(mConn, EE_WON_RTQ_SET_CLIENT_FLAGS, c); }
    void SetGroupFlags(unsigned short inId, unsigned long inMask, unsigned long inFlags, unsigned long inAsyncMask,
                       unsigned long inAsyncFlags, RouteCompletion* c)
    {
        RoutingRequestValues().N(inId).N(inMask).N(inFlags).N(inAsyncMask).N(inAsyncFlags)
            .Run(mConn, EE_WON_RTQ_SET_GROUP_FLAGS, c);
    } // 2bdc10
    void SendData(const ByteBuffer* inData, unsigned char inFlags, unsigned short inRecipient, RouteCompletion* c)
    { this->SendData(inData, inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be910
    void SendData(const ByteBuffer* inData, unsigned char inFlags, const std::list<unsigned short>& inRecipients,
                  RouteCompletion* c)
    { RoutingRequestValues().D(inData).N(inFlags).L(inRecipients).Run(mConn, EE_WON_RTQ_DATA, c); } // 2bea70
    // a ban or a mute: the group, on, the seconds, the explanation, the client (by client id, WON id or CD key id)
    void BanClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                   unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf590
    void BanClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                          unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf6b0
    void BanClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                            const std::wstring& inExplanation, unsigned long inCDKeyId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_BAN_CDKEY_ID, inGroupId, inOn, inSeconds, inExplanation, inCDKeyId, c); } // WON SDK
    void MuteClient(unsigned short inGroupId, bool inOn, unsigned long inSeconds, const std::wstring& inExplanation,
                    unsigned short inClientId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE, inGroupId, inOn, inSeconds, inExplanation, inClientId, c); } // 2bf7d0
    void MuteClientByWONId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                           const std::wstring& inExplanation, unsigned long inWONId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE_WON_ID, inGroupId, inOn, inSeconds, inExplanation, inWONId, c); } // 2bf8f0
    void MuteClientByCDKeyId(unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                             const std::wstring& inExplanation, unsigned long inCDKeyId, RouteCompletion* c)
    { this->Moderate(EE_WON_RTQ_MUTE_CDKEY_ID, inGroupId, inOn, inSeconds, inExplanation, inCDKeyId, c); } // WON SDK
    // 2bd9f0 (accept), 2bdb00 (invite): the answer to a client's join attempt, an invitation
    void AcceptClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(mConn, EE_WON_RTQ_ACCEPT, c); }
    void InviteClient(unsigned short inGroupId, unsigned short inClientId, bool inOn, const std::wstring& inExplanation,
                      RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inClientId).N(inOn).W(inExplanation).Run(mConn, EE_WON_RTQ_INVITE, c); }
    void BecomeModerator(bool inOn, RouteCompletion* c) { RoutingRequestValues().N(inOn).Run(mConn, EE_WON_RTQ_MODERATOR, c); } // 2bf3f0
    void SendServerAlert(const std::wstring& inText, RouteCompletion* c)
    { RoutingRequestValues().W(inText).Run(mConn, EE_WON_RTQ_ALERT, c); } // 2bddd0
    void StartServerShutdown(const std::wstring& inText, unsigned long inSeconds, RouteCompletion* c)
    { RoutingRequestValues().W(inText).N(inSeconds).Run(mConn, EE_WON_RTQ_START_SHUTDOWN, c); } // 2be010
    void AbortServerShutdown(RouteCompletion* c) { RoutingRequestValues().Run(mConn, EE_WON_RTQ_ABORT_SHUTDOWN, c); } // 2be200

    // Further WON SDK requests; no original address identified.
    void DisconnectClient(RouteCompletion* c) { RoutingRequestValues().Run(mConn, EE_WON_RTQ_DISCONNECT, c); }
    // the _KeyId data objects (link 0xffff, flags 0)
    void GetClientCDKeyIdList(RouteCompletion* c, bool inSubscribe)
    { RoutingRequestValues().N(inSubscribe).N(0xffff).S("_KeyId").N(0).Run(mConn, EE_WON_RTQ_READ_DATA, c); }
    void GetClientListEx(RouteCompletion* c) { RoutingRequestValues().Run(mConn, EE_WON_RTQ_CLIENT_LIST_EX, c); }
    void SetGroupName(unsigned short inGroupId, const std::wstring& inName, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).W(inName).Run(mConn, EE_WON_RTQ_SET_GROUP_NAME, c); }
    void SetGroupMaxPlayers(unsigned short inGroupId, unsigned short inMaxPlayers, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inMaxPlayers).Run(mConn, EE_WON_RTQ_SET_GROUP_MAX_PLAYERS, c); }
    void RelinquishCaptaincy(unsigned short inGroupId, unsigned short inNewCaptainId, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inNewCaptainId).Run(mConn, EE_WON_RTQ_RELINQUISH, c); }
    // nothing for an empty list
    void SendServerAlertWithRecipientList(const std::wstring& inText, std::list<unsigned short>& inRecipients,
                                          RouteCompletion* c)
    { RoutingRequestValues().W(inText).L(inRecipients).Run(mConn, EE_WON_RTQ_ALERT_TO, c); }
    // a RoutingDetectFirewallOp
    void DetectFirewall(unsigned short inListenPort, unsigned long inMaxConnectSeconds, bool inDoListen, RouteCompletion* c)
    { RoutingRequestValues().N(inListenPort).N(inMaxConnectSeconds).N(inDoListen).Run(mConn, EE_WON_RTQ_DETECT_FIREWALL, c); }
    void GetBadUserList(RoutingBadUserListType inListType, unsigned short inGroupId, unsigned char inFlags, RouteCompletion* c)
    { RoutingRequestValues().N(inListType).N(inGroupId).N(inFlags).Run(mConn, EE_WON_RTQ_BAD_USERS, c); }
private:
    void Moderate(int inKind, unsigned short inGroupId, bool inOn, unsigned long inSeconds,
                  const std::wstring& inExplanation, unsigned long inId, RouteCompletion* c)
    { RoutingRequestValues().N(inGroupId).N(inOn).N(inSeconds).W(inExplanation).N(inId).Run(mConn, inKind, c); }
    RoutingConnection* mConn;
};
}
#endif
