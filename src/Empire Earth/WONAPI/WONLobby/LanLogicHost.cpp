// LanLogic's C++ host (NEW, no original address: units.EXTRA_UNITS): the shell of LanLogic, a Rust object since
// round 20 (rust/crates/ee_won_lobby/src/lanobj.rs; its members, the lists, sockets and timers it held, Rust's since
// WONLobby round 11: lanown.rs), LobbyLanSocket, and the LanLogic ops of ee_won_lobbycpp_call (LobbyLogicHost.cpp;
// lobbyhost.rs EeWonLobbyOp from LanMyClient on): the C++ calls on the sockets, ops, dialogs and events. It
// replaces LanLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <stdint.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/EELobbyGameCode.h" // LobbyMessageBox
#include "rust/won_lobby/lanlogic.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_net/ipobj.h"
#include "rust/won_net/qsock.h"
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in EEWonNet.cpp)
namespace WONAPI {
AuthContext* GetLobbyAuthContext(); // the lobby's authentication context (cdecl)
}
using namespace WONAPI;

extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD);

// a wide string as the UTF-16 LE bytes the Rust side takes
static std::string LanUtf16(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}
// UTF-16 units from the Rust side as a wide string
static std::wstring LanWide(intptr_t inUnits, intptr_t inCount)
{
    std::wstring anOut;
    const uint16_t* aUnits = reinterpret_cast<const uint16_t*>(inUnits);
    for (intptr_t i = 0; i < inCount; i++)
        WideAppendUtf16Unit(anOut, aUnits[i]);
    return anOut;
}
static void Take(void* inSink, const std::string& inBytes)
{
    ee_won_lobby_sink(inSink, reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.length());
}
static intptr_t Held(RefCount* inObj)
{
    inObj->AddRef();
    return (intptr_t)inObj;
}
static IPAddr Addr(void* inValue) { return IPAddr::FromValue(ee_won_ipaddr_clone(static_cast<const EeWonIpAddr*>(inValue))); }

// ---- LobbyLanSocket: its client and time are a Rust object (qsock.rs EeWonLanSocket); its calls back

static void LanClientAddRef(void* inClient) { static_cast<LobbyLanClient*>(inClient)->AddRef(); }
static void LanClientRelease(void* inClient) { static_cast<LobbyLanClient*>(inClient)->Release(); }
static void LanClientSetSocket(void* inClient, void* inSocket)
{
    static_cast<LobbyLanClient*>(inClient)->mSocket = static_cast<LobbyLanSocket*>(inSocket);
}
static const EeWonLanSocketHooks kLanSocketHooks = {LanClientAddRef, LanClientRelease, LanClientSetSocket};

// A stream socket with a 2-byte length field and no client
LobbyLanSocket::LobbyLanSocket() : BlockingSocket(0)
{
    this->mLan = ee_won_lansocket_new(static_cast<void*>(this), &kLanSocketHooks);
    this->SetLengthFieldSize(2);
}
LobbyLanSocket::~LobbyLanSocket() { ee_won_lansocket_free(this->mLan); }
void LobbyLanSocket::SetClient(LobbyLanClient* inClient) { ee_won_lansocket_set_client(this->mLan, inClient); }

// ---- LanLogic's shell

// Creates the LAN logic with an empty client list and empty LAN and direct-connect game lists (the Rust object's)
LanLogic::LanLogic() : mRust(NULL)
{
    LobbyClientList* aClients = new LobbyClientList;
    LobbyGameList* aLanGames = new LobbyGameList(1);
    LobbyGameList* aDirectGames = new LobbyGameList(3);
    this->mRust = ee_won_lan_new_model(this, ee_won_lobbycpp_call, aClients, aLanGames, aDirectGames);
}

LanLogic::~LanLogic() { ee_won_lan_free(this->mRust); }

// Looks up the direct-connect, LAN name, LAN port and password dialogs from the lobby layout
void LanLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mDirectConnectDialog, "DirectConnectDialog", "LanLogic.cpp", 105);
    SafeGetComponent(inConfig, this->mLanNameDialog, "LanNameDialog", "LanLogic.cpp", 106);
    SafeGetComponent(inConfig, this->mLanPortDialog, "LanPortDialog", "LanLogic.cpp", 107);
    SafeGetComponent(inConfig, this->mPasswordDialog, "PasswordDialog", "LanLogic.cpp", 108);
}

// The LAN name, LAN port, refresh game list and show LAN screen commands
void LanLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_AddLANName ? 1 : inControlId == ID_ChangeLANBroadcast ? 2
               : inControlId == ID_RefreshGameList ? 3 : inControlId == ID_ShowLanScreen ? 4 : 0;
    if (aWhich != 0) this->Call(EeWonLanCall::Control, NULL, NULL, aWhich);
}

void LanLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->Call(EeWonLanCall::ComponentEvent, inEvent, NULL, inEvent->mType);
}

void LanLogic::CallMessage(EeWonLanCall inCall, const ByteBuffer* inMessage, LobbyClient* inClient)
{
    ee_won_lan_call(this->mRust, inCall, const_cast<ByteBuffer*>(inMessage), inClient,
                    inMessage != NULL ? (intptr_t)inMessage->data() : 0,
                    inMessage != NULL ? (intptr_t)inMessage->length() : 0);
}

// Leaves the current game when the lobby resumes
void LanLogic::Resume() { LobbyLeaveGame(0); }

// Host: accepts a peer-authenticated socket whose certificate has the lobby's community, else closes it
void LanLogic::HandlePeerAuthDone(AsyncOp* inOp)
{
    PeerAuthServerOp* op = static_cast<PeerAuthServerOp*>(inOp);
    LobbyLanSocket* socket = static_cast<LobbyLanSocket*>(op->GetSocket());
    if (!op->Succeeded()) {
        socket->QueueClose(1000);
        return;
    }
    AuthContext* auth = GetLobbyAuthContext();
    if (!auth) return;
    DWORD community = auth->GetPeerData()->GetFirstCommunity();
    if (!op->ClientPermits(community)) {
        socket->QueueClose(1000);
        return;
    }
    this->Call(EeWonLanCall::AddPending, socket);
}

// ---- the LanLogic ops (lobbyhost.rs documents each)

static const ByteBuffer* LanMessageOf(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

intptr_t LanLogicHostCall(LanLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    LanLogic* l = inLan;
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyLanClient* lc = static_cast<LobbyLanClient*>(cl);
    LobbyLanSocket* sock = static_cast<LobbyLanSocket*>(inA);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    AsyncOp* op = static_cast<AsyncOp*>(inA);
    ChatCommandParser* parser = static_cast<ChatCommandParser*>(inA);
    ByteBufferPtr aHold;
    switch (inOp) {
    case EeWonLobbyOp::LanAuthAutoRefreshOff: {
        AuthContext* anAuthContext = GetLobbyAuthContext();
        if (anAuthContext != NULL)
            anAuthContext->SetDoAutoRefresh(false);
        return 0;
    }
    case EeWonLobbyOp::LanOpSocket: return (intptr_t)static_cast<LobbyLanSocket*>(static_cast<SocketOp*>(op)->GetSocket());
    case EeWonLobbyOp::LanSocketClient: return (intptr_t)static_cast<LobbyClient*>(sock->GetClient());
    case EeWonLobbyOp::LanSocketSetClient:
        sock->SetClient(static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB)));
        return 0;
    case EeWonLobbyOp::LanSocketLastTime: return sock->GetLastMessageTime();
    case EeWonLobbyOp::LanSocketClose: if (sock != NULL) sock->QueueClose((long)inC); return 0;
    case EeWonLobbyOp::LanMyGame: return (intptr_t)GetMyLobbyGame();
    case EeWonLobbyOp::LanSubnets: {
        IPAddr aLocalAddr = IPAddr::GetLocalAddr();
        IPAddr aPeerAddr = sock->GetDestAddr();   // a copy: the bytes point into it
        const char* aPeerBytes = aPeerAddr.GetSixByte();
        const char* aLocalBytes = aLocalAddr.GetSixByte();
        return (intptr_t)((uint32_t)(unsigned char)aPeerBytes[2] | (uint32_t)(unsigned char)aPeerBytes[3] << 8 |
                          (uint32_t)(unsigned char)aLocalBytes[2] << 16 | (uint32_t)(unsigned char)aLocalBytes[3] << 24);
    }
    case EeWonLobbyOp::LanGamePasswordIs: return LanWide(inC, inD) == std::wstring(g->GetPassword());
    case EeWonLobbyOp::LanSocketSend:
        static_cast<LobbyLanSocket*>(inB)->QueueOp(new SendMsgOp(LanMessageOf(inA, inC, inD, aHold)), -1);
        return 0;
    case EeWonLobbyOp::LanNewClient: {
        LobbyLanClient* aClient = new LobbyLanClient;
        aClient->SetName(GUIString(new GUIStringData(LanWide(inC, inD))));
        return Held(aClient);
    }
    case EeWonLobbyOp::ClientHold: cl->AddRef(); return (intptr_t)cl;
    case EeWonLobbyOp::ClientRelease: cl->Release(); return 0;
    case EeWonLobbyOp::ClientNameTake: Take(inB, LanUtf16(cl->GetName())); return 0;
    case EeWonLobbyOp::LanClientChanged:
        LobbyBroadcastEvent(new ClientChangedEvent(cl, l->GetRoomSpec(), (int)inC), true);
        return 0;
    case EeWonLobbyOp::ClientLeftGame: LobbyClientLeftGame(cl); return 0;
    case EeWonLobbyOp::LanRecvOk: return op->GetStatus() == WS_Success && !op->IsKilled();
    case EeWonLobbyOp::LanSocketStamp: sock->SetLastMessageTime(time(NULL)); return 0;
    case EeWonLobbyOp::LanRecvMsgTake: {
        const ByteBuffer* aMsg = static_cast<RecvMsgOp*>(op)->GetMsg().get();
        if (aMsg != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return (intptr_t)aMsg;
    }
    case EeWonLobbyOp::LanClientSocket: return (intptr_t)lc->mSocket;
    case EeWonLobbyOp::LanClientSocketSend:
        static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB))->mSocket->QueueOp(
            new SendMsgOp(LanMessageOf(inA, inC, inD, aHold)), -1);
        return 0;
    case EeWonLobbyOp::LanWhisperReceived: {
        if (LobbyGetChatSoundEffects())
            LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_WhisperRecv_Sound).get());
        ChatCommandParser* aParser = LobbyGetChatCommandParser();
        if (aParser != NULL)
            aParser->SetInput(cl->GetName());
        return 0;
    }
    case EeWonLobbyOp::LanRecvChat:
        LobbyBroadcastEvent(new RecvChatEvent(8, (unsigned long)(inD >> 32), cl, LanWide(inC, inD & 0xffffffff),
                                              static_cast<LobbyClient*>(inB)), true);
        return 0;
    case EeWonLobbyOp::LanStagingRead: {
        ReadBuffer aBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD);
        try {
            aBuffer.ReadByte();
            LobbyHandleStagingReadBuffer(aBuffer, static_cast<LobbyClient*>(inB));
        }
        catch (ReadBufferException&) {}
        return 0;
    }
    case EeWonLobbyOp::LanNameDialogRun: l->mLanNameDialog->Reset(); return LobbyDoDialog(l->mLanNameDialog.get());
    case EeWonLobbyOp::LanNameDialogLength: return (intptr_t)l->mLanNameDialog->GetName().length();
    case EeWonLobbyOp::LanNameDialogSave:
        LobbySetLanName(l->mLanNameDialog->GetName());
        LobbyBroadcastLobbyEvent(0x186d2, true);
        return 0;
    case EeWonLobbyOp::LanBroadcastPort: return LobbyGetLanBroadcastPort();
    case EeWonLobbyOp::LanPortDialogRun:
        l->mLanPortDialog->SetGamePort(LobbyGetLobbyPort());
        l->mLanPortDialog->SetPort((int)inC);
        l->mLanPortDialog->Reset();
        return LobbyDoDialog(l->mLanPortDialog.get());
    case EeWonLobbyOp::LanPortDialogPort: return l->mLanPortDialog->GetPort();
    case EeWonLobbyOp::LanSetBroadcastPort:
        LobbySetLanBroadcastPort((int)inC);
        LobbyBroadcastLobbyEvent(0x186d2, true);
        return 0;
    case EeWonLobbyOp::EventComponent: return (intptr_t)static_cast<ComponentEvent*>(inA)->mComponent;
    case EeWonLobbyOp::EventDirectIsDelete: return static_cast<ChangeDirectConnectEvent*>(inA)->IsDelete();
    case EeWonLobbyOp::EventDirectIsAdd: return static_cast<ChangeDirectConnectEvent*>(inA)->IsAdd();
    case EeWonLobbyOp::EventDirectGame: return (intptr_t)static_cast<ChangeDirectConnectEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventClientActionClient: return (intptr_t)static_cast<ClientActionEvent*>(inA)->mClient.get();
    case EeWonLobbyOp::EventClientActionRoomSpec: return static_cast<ClientActionEvent*>(inA)->mRoomSpec;
    case EeWonLobbyOp::EventClientActionAction: return static_cast<ClientActionEvent*>(inA)->mAction;
    case EeWonLobbyOp::SetClientIgnored: LobbySetClientIgnored(cl, inC != 0, (int)inD); return 0;
    case EeWonLobbyOp::LanSetChatInput:
        LobbyBroadcastEvent(new SetChatInputEvent(2, cl->GetName(), (int)inC, false), true);
        return 0;
    case EeWonLobbyOp::KickClientFromLobby: LobbyKickClient(cl, false); return 0;
    case EeWonLobbyOp::ChatParser: return (intptr_t)LobbyGetChatCommandParser();
    case EeWonLobbyOp::ParserRoomFlags: return parser->GetRoomFlags();
    case EeWonLobbyOp::ParserTarget: return (intptr_t)parser->GetTarget();
    case EeWonLobbyOp::ParserCommand: return parser->GetCommand();
    case EeWonLobbyOp::ParserTextTake: Take(inB, LanUtf16(parser->GetText())); return 0;
    case EeWonLobbyOp::LanChatCommandError:
        LobbySendEventTo(new ChatCommandErrorEvent(LOBBY_STRING(RoutingLogic_InvalidCommand_String)), static_cast<Component*>(inA));
        return 0;
    case EeWonLobbyOp::ShowLanScreen: LobbyShowLanScreen(); return 0;
    case EeWonLobbyOp::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case EeWonLobbyOp::KillFirewallTest: LobbyKillFirewallTest(); return 0;
    case EeWonLobbyOp::LanRefreshCertificate: {
        // a fresh login certificate (anonymous) unless the current one is valid; false on bad CD key or failure
        AuthContext* auth = GetLobbyAuthContext();
        if (!auth) return false;
        if (!LobbyIsCDKeyValid()) {
            LobbyMessageBox(LOBBY_STRING(AccountLogic_InvalidCDKeyTitle_String), LOBBY_STRING(AccountLogic_InvalidCDKeyDetails_String));
            return false;
        }
        if (!auth->IsExpired(120)) {
            auth->SetDoAutoRefresh(true);
            return true;
        }
        auth->SetUserName(std::wstring(gEmptyWideString));
        auth->SetPassword(std::wstring(gEmptyWideString));
        SmartPtr<GetCertOp> anOp = new GetCertOp(auth);
        LobbyRunOpWithStatus(anOp.get(), LOBBY_STRING(LobbyContainer_LoginTitle_String), LOBBY_STRING(LobbyContainer_LoginDetails_String), 1, 0);
        if (anOp->GetStatus() != WS_Success) return false;
        auth->SetDoAutoRefresh(true);
        return true;
    }
    case EeWonLobbyOp::LanDiscoveryTake: {
        const ByteBuffer* aBytes = static_cast<RecvBytesFromOp*>(op)->GetBytes().get();
        if (aBytes != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aBytes->data()), aBytes->length());
        return 0;
    }
    case EeWonLobbyOp::LanAcceptedSocket:
        if (op->GetStatus() != WS_Success || op->IsKilled()) return 0;
        return (intptr_t)static_cast<LobbyLanSocket*>(static_cast<AcceptOp*>(op)->GetAcceptedSocket());
    case EeWonLobbyOp::LanStartPeerAuth: {
        AuthContext* auth = GetLobbyAuthContext();
        if (!auth) return 0;
        SmartPtr<PeerAuthServerOp> server = new PeerAuthServerOp(auth, sock);
        l->RunTrackedOp(server.get(), new LogicCompletion<LanLogic>(&LanLogic::HandlePeerAuthDone, l), 30000);
        return 0;
    }
    case EeWonLobbyOp::LanNameTake: Take(inB, LanUtf16(LobbyGetLanName())); return 0;
    // the C++ calls of the cases over the members (lanown.rs)
    case EeWonLobbyOp::RefHold: static_cast<RefCount*>(inA)->AddRef(); return 0;
    case EeWonLobbyOp::RefRelease: static_cast<RefCount*>(inA)->Release(); return 0;
    case EeWonLobbyOp::LanSocketKill: static_cast<QueueSocket*>(inA)->Kill(); return 0;
    case EeWonLobbyOp::LanTimerKill: op->Kill(true); return 0;
    case EeWonLobbyOp::LanNewTimer: {
        AsyncOp* aTimer = new AsyncOp;
        aTimer->SetCompletion(new LogicCompletion<LanLogic>(inC == 0 ? &LanLogic::HandleNetworkTimer : &LanLogic::HandleDiscoveryTimer, l));
        return Held(aTimer);
    }
    case EeWonLobbyOp::LanRunTimer: op->RunAsTimer((DWORD)inC); return 0;
    case EeWonLobbyOp::LanGameDisconnected: LobbyGameDisconnected(true); return 0;
    case EeWonLobbyOp::LanSyncGameList: LobbyBroadcastEvent(new SyncGameListEvent(static_cast<LobbyGameList*>(inA)), true); return 0;
    case EeWonLobbyOp::LanGameChanged: LobbyBroadcastEvent(new GameChangedEvent(g, (int)inC), true); return 0;
    case EeWonLobbyOp::LanNewSocket: return Held(new LobbyLanSocket);
    case EeWonLobbyOp::LanListen: {
        WONStatus aStatus = sock->Fn2B0D90(LobbyGetLobbyPort(), 0);
        if (aStatus != 0) {
            LobbyDoErrorDialog(LOBBY_STRING(LANLogic_SocketBindError_String), aStatus);
            return 0;
        }
        aStatus = sock->Listen(1000);
        if (aStatus != 0) {
            LobbyDoErrorDialog(LOBBY_STRING(LANLogic_SocketListenError_String), aStatus);
            return 0;
        }
        sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleAccept, l));
        sock->SetRepeatOp(new AcceptOp(NULL));
        return 1;
    }
    case EeWonLobbyOp::LanJoinedRoom:
        LobbyBroadcastEvent(new JoinedRoomEvent("", g->GetName(), 10, (unsigned short)inC, g->GetClients().get()), true);
        return 0;
    case EeWonLobbyOp::LanNewDiscovery: {
        // port: the discovery port is bound with address reuse (the original bound it exclusively), so that two
        // lobbies on one machine both receive the broadcasts
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort()).GetValue());
        QueueSocket* aSocket = new QueueSocket(1);
        Held(aSocket);
        *static_cast<intptr_t*>(inB) = aSocket->Fn2B0D90(LobbyGetLanBroadcastPort(), 1);
        return (intptr_t)aSocket;
    }
    case EeWonLobbyOp::LanArmDiscovery:
        static_cast<QueueSocket*>(inA)->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryRecv, l));
        static_cast<QueueSocket*>(inA)->SetRepeatOp(new RecvBytesFromOp(1024, NULL));
        return 0;
    case EeWonLobbyOp::LanSummaryTake: {
        WriteBuffer aBuffer(0, true);
        g->WriteSummary(aBuffer);
        ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aBuffer.data()), aBuffer.length());
        return 0;
    }
    case EeWonLobbyOp::LanSendTo:
        static_cast<QueueSocket*>(inA)->QueueOp(new SendBytesToOp(LanMessageOf(NULL, inC, inD, aHold), Addr(inB)), -1);
        return 0;
    case EeWonLobbyOp::LanOpOnSocket: {
        RecvBytesFromOp* anOp = static_cast<RecvBytesFromOp*>(op);
        return (void*)anOp->GetSocket() == inB && anOp->Succeeded();
    }
    case EeWonLobbyOp::LanConnectTo: {
        SmartPtr<AsyncOp> connect;
        if (inC != 0) {
            SmartPtr<AuthContext> auth = GetLobbyAuthContext();
            connect = new PeerAuthOp(g->GetAddress(), auth.get(), AUTH_TYPE_PERSISTENT, static_cast<LobbyLanSocket*>(inB), true);
        } else
            connect = new ConnectOp(g->GetAddress(), static_cast<LobbyLanSocket*>(inB));
        LobbyRunOpWithStatus(connect.get(), LOBBY_STRING(LANLogic_ConnectToGameTitle_String), LOBBY_STRING(LANLogic_ConnectToGameDetails_String), 1, 0);
        return connect->GetStatus() == WS_Success;
    }
    case EeWonLobbyOp::LanRecvReply: {
        SmartPtr<RecvMsgOp> reply = new RecvMsgOp(sock);
        LobbyRunOpWithStatus(reply.get(), LOBBY_STRING(LANLogic_RegisterTitle_String), LOBBY_STRING(LANLogic_RegisterDetails_String), 1, 0);
        if (reply->GetStatus() != WS_Success) return -1;
        const ByteBuffer* aMsg = reply->GetMsg().get();
        if (aMsg == NULL) return 0;
        ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return 1;
    }
    case EeWonLobbyOp::LanArmSocket:
        sock->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, l));
        sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, l));
        sock->SetRepeatOp(new RecvMsgOp);
        return 0;
    case EeWonLobbyOp::LanPasswordDialog: l->mPasswordDialog->Reset(); return LobbyDoDialog(l->mPasswordDialog.get());
    case EeWonLobbyOp::LanPasswordTake: Take(inB, LanUtf16(l->mPasswordDialog->GetPassword())); return 0;
    case EeWonLobbyOp::LanPingGame: LobbyPingGame(g, false); return 0;
    case EeWonLobbyOp::LanNoCurrentUser: LobbySetCurrentUser(NULL); return 0;
    case EeWonLobbyOp::LanDirectCount: return (intptr_t)LobbyGetDirectConnectCount();
    case EeWonLobbyOp::LanDirectAddressAt:
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), LobbyGetDirectConnectAddressAt((size_t)inC).GetValue());
        return 0;
    case EeWonLobbyOp::LanDirectNameAt: g->SetName(LobbyGetDirectConnectNameAt((size_t)inC)); return 0;
    case EeWonLobbyOp::LanRemoveSavedDirect: LobbyRemoveDirectConnect(g->GetAddress()); return 0;
    case EeWonLobbyOp::LanDirectAsk: {
        l->mDirectConnectDialog->Reset();
        if (LobbyDoDialog(l->mDirectConnectDialog.get()) != 0x186a1)
            return 0;
        IPAddr anAddress;
        LobbyConfig* aConfig = GetLobbyConfig();
        // (the configured port moved by the harness slot's offset, as LobbyBindUDPPort's: TEST ONLY)
        int aDefaultPort = ee_won_lanlogic_direct_default_port(aConfig != NULL,
                                                               aConfig != NULL ? aConfig->mLobbyPort + LobbyTestPortOffset() : 0,
                                                               LobbyGetLobbyPort());
        anAddress.SetWithDefaultPortNoDNS(l->mDirectConnectDialog->GetAddress(), aDefaultPort);
        if (!anAddress.IsValid()) {
            SmartPtr<DNSTestOp> anOp = new DNSTestOp(l->mDirectConnectDialog->GetAddress());
            LobbyRunOpWithStatus(anOp.get(), LOBBY_STRING(LANLogic_CheckingAddressTitle_String), LOBBY_STRING(LANLogic_CheckingAddressDetails_String), 1, 0);
            if (!anOp->Succeeded())
                return 0;
            anAddress = anOp->GetHostAddr();
            if (anAddress.GetPort() == 0)
                anAddress.SetPort(aDefaultPort);
        }
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), anAddress.GetValue());
        return 1;
    }
    case EeWonLobbyOp::LanDirectSave: {
        GUIString aName = l->mDirectConnectDialog->GetName();
        if (ee_won_lanlogic_direct_name_is_address(aName.length()))
            aName = l->mDirectConnectDialog->GetAddress();
        g->SetName(aName);
        LobbyAddDirectConnect(Addr(inB), aName);
        return 0;
    }
    case EeWonLobbyOp::LanReadSummary: {
        // the game's summary follows the type and the port (LobbyGame::ReadSummary)
        const ByteBuffer* aBytes = static_cast<RecvBytesFromOp*>(op)->GetBytes().get();
        ReadBuffer aBuffer(aBytes->data(), aBytes->length());
        try {
            aBuffer.ReadByte();
            aBuffer.ReadShort();
            return static_cast<LobbyGame*>(inB)->ReadSummary(aBuffer);
        }
        catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    }
    case EeWonLobbyOp::LanRecvFromAddr:
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inB), static_cast<RecvBytesFromOp*>(op)->GetAddr().GetValue());
        return 0;
    case EeWonLobbyOp::LanClientOptions: {
        SmartPtr<ClientOptionsPopup> aPopup = new ClientOptionsPopup;
        aPopup->Init(cl, static_cast<LobbyClient*>(inB), (int)inC);
        LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
        return 0;
    }
    default: return 0;
    }
}
