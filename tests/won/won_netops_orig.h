// The host cases R6 WONLobby round 28 answered in Rust (ee_won_lobby netops.rs), as they were: LanLogicHost.cpp's
// on the ops, socket ops, queue sockets and byte buffers, EELobbyGameHost.cpp's saved game transfer's and
// RoutingLogic.cpp's, copied from the units (round 28's first commit) for won_netops_test. The lobby's classes the
// cases cast to are their network bases here (a LobbyLanSocket is a BlockingSocket, a LobbyClient a RefCount: the
// cases call only those bases' members). Requires ee/wonapi/WONRouting.h and the lobby host's, transfer's and
// routing logic's headers.
#ifndef WON_NETOPS_ORIG_H
#define WON_NETOPS_ORIG_H
namespace WonNetopsOrig {
using namespace WONAPI;
typedef BlockingSocket LobbyLanSocket;
typedef RefCount LobbyClient;

static IPAddr Addr(void* inValue) { return IPAddr::FromValue(ee_won_ipaddr_clone(static_cast<const EeWonIpAddr*>(inValue))); }
static const ByteBuffer* LanMessageOf(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

// LanLogicHostCall's deleted cases (-1 for another op)
static intptr_t Lan(EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD, bool* outHandled)
{
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyLanSocket* sock = static_cast<LobbyLanSocket*>(static_cast<RefCount*>(inA));
    AsyncOp* op = static_cast<AsyncOp*>(static_cast<RefCount*>(inA));
    ByteBufferPtr aHold;
    *outHandled = true;
    switch (inOp) {
    case EeWonLobbyOp::LanOpSocket: return (intptr_t)static_cast<LobbyLanSocket*>(static_cast<SocketOp*>(op)->GetSocket());
    case EeWonLobbyOp::LanSocketClose: if (sock != NULL) sock->QueueClose((long)inC); return 0;
    case EeWonLobbyOp::LanSocketSend:
        static_cast<LobbyLanSocket*>(static_cast<RefCount*>(inB))->QueueOp(new SocketOp(EeWonCoreSocketOp::SendMsg, NULL, LanMessageOf(inA, inC, inD, aHold)), -1);
        return 0;
    case EeWonLobbyOp::ClientHold: cl->AddRef(); return (intptr_t)cl;
    case EeWonLobbyOp::ClientRelease: cl->Release(); return 0;
    case EeWonLobbyOp::LanRecvOk: return op->GetStatus() == WS_Success && !op->IsKilled();
    case EeWonLobbyOp::LanRecvMsgTake: {
        const ByteBuffer* aMsg = static_cast<SocketOp*>(op)->GetMsg().get();
        if (aMsg != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return (intptr_t)aMsg;
    }
    case EeWonLobbyOp::LanDiscoveryTake: {
        const ByteBuffer* aBytes = static_cast<SocketOp*>(op)->GetBytes().get();
        if (aBytes != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aBytes->data()), aBytes->length());
        return 0;
    }
    case EeWonLobbyOp::LanAcceptedSocket:
        if (op->GetStatus() != WS_Success || op->IsKilled()) return 0;
        return (intptr_t)static_cast<LobbyLanSocket*>(static_cast<SocketOp*>(op)->GetAcceptedSocket());
    case EeWonLobbyOp::RefHold: static_cast<RefCount*>(inA)->AddRef(); return 0;
    case EeWonLobbyOp::RefRelease: static_cast<RefCount*>(inA)->Release(); return 0;
    case EeWonLobbyOp::LanSocketKill: static_cast<QueueSocket*>(static_cast<RefCount*>(inA))->Kill(); return 0;
    case EeWonLobbyOp::LanTimerKill: op->Kill(true); return 0;
    case EeWonLobbyOp::LanRunTimer: op->RunAsTimer((DWORD)inC); return 0;
    case EeWonLobbyOp::LanSendTo: {
        IPAddr aTo = Addr(inB);
        static_cast<QueueSocket*>(static_cast<RefCount*>(inA))->QueueOp(new SocketOp(EeWonCoreSocketOp::SendTo, NULL, LanMessageOf(NULL, inC, inD, aHold), aTo.GetValue()), -1);
        return 0;
    }
    case EeWonLobbyOp::LanOpOnSocket: {
        SocketOp* anOp = static_cast<SocketOp*>(op);
        return (void*)static_cast<RefCount*>(anOp->GetSocket()) == inB && anOp->Succeeded();
    }
    case EeWonLobbyOp::LanRecvFromAddr:
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inB), static_cast<SocketOp*>(op)->GetAddr().GetValue());
        return 0;
    default: *outHandled = false; return 0;
    }
}

// EELobbyTransferHost's deleted cases
static intptr_t Xfer(EeWonEeXferOp inOp, intptr_t a, bool* outHandled)
{
    typedef EeWonEeXferOp Op;
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    QueueSocket* aSocket = static_cast<QueueSocket*>(reinterpret_cast<AsyncSocket*>(a));
    *outHandled = true;
    switch (inOp) {
    case Op::Succeeded: return anOp->Succeeded() ? 1 : 0;
    case Op::OpSocket: return (intptr_t) static_cast<SocketOp*>(anOp)->GetSocket();
    case Op::AcceptedSocket: return (intptr_t) static_cast<SocketOp*>(anOp)->GetAcceptedSocket();
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::Kill: reinterpret_cast<AsyncSocket*>(a)->Kill(); return 0;
    case Op::Listen: return aSocket->Listen(1000);
    default: *outHandled = false; return 0;
    }
}

// RoutingHost's deleted cases (the logic's game data type is "G" from its constructor)
static intptr_t Route(EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, bool* outHandled)
{
    typedef EeWonRouteLogicOp O;
    static const std::string kGameDataType = "G";
    RoutingOp* RO_a = reinterpret_cast<RoutingOp*>(a);
    AsyncOp* AO_a = reinterpret_cast<AsyncOp*>(a);
    *outHandled = true;
    switch (inOp) {
    case O::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::OpStatus: return (int)AO_a->GetStatus();
    case O::OpKilled: return AO_a->IsKilled();
    case O::OpKill: AO_a->Kill(true); return 0;
    case O::OpFinish: AO_a->Finish(WS_Success); return 0;
    case O::AsyncOpNew: { AsyncOp* anOp = new AsyncOp; anOp->AddRef(); return (intptr_t)anOp; }
    case O::Now: return (intptr_t)time(NULL);
    case O::Tick: return (intptr_t)WONTickCount();
    case O::RNum: return (intptr_t)RO_a->Num((int)b);
    case O::RIsGameData: return RO_a->DataType() == kGameDataType;
    case O::RRecipient: {
        std::list<unsigned short> aRecipients = RO_a->Recipients();
        return aRecipients.empty() ? -1 : (intptr_t)aRecipients.front();
    }
    case O::OpSocket: return (intptr_t) static_cast<RefCount*>(static_cast<SocketOp*>(AO_a)->GetSocket());
    // R6 WON lobby and network round 40's (the routing connection's a socket here: IsConnected is AsyncSocket's)
    case O::RSetNum: RO_a->SetNum((int)b, (long long)c); return 0;
    case O::ConnIsConnected: return reinterpret_cast<AsyncSocket*>(a)->IsConnected();
    case O::StatusOpRust: return (intptr_t)UserStatusOf(AO_a);
    default: *outHandled = false; return 0;
    }
}
}
#endif
