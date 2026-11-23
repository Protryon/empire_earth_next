// The logics' host cases R6 WONLobby round 29 moved to Rust (ee_won_lobby lobbyops.rs), as they were (copied by
// script from LobbyLogicHost.cpp and LanLogicHost.cpp at c01702f3f), for won_lobbyops_test over
// won_lobbyops_world.h's stand-ins. The two functions they called that the world takes over are renamed.
#ifndef WON_LOBBYOPS_ORIG_H
#define WON_LOBBYOPS_ORIG_H
#define ee_won_ipaddr_assign LoWorld::IpAssign
#define ee_won_lobby_sink LoWorld::Sink
namespace LoOrig {
using namespace LoWorld;
static intptr_t Held(RefCount* inObj)
{
    inObj->AddRef();
    return (intptr_t)inObj;
}

// ee_won_lobbycpp_call's (StagingLogic's)
static intptr_t St(StagingLogic* st, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    (void)inB; (void)inD;
    switch (inOp) {
    case EeWonLobbyOp::StJoinOpFinish: if (st->mJoinOp.get() != NULL) st->mJoinOp->Finish(WS_Success); return 0;
    case EeWonLobbyOp::StJoinOpRun:
        LobbyRunOpWithStatus(st->mJoinOp.get(), LOBBY_STRING(GameStagingLogic_GettingGameInfoTitle_String),
                             LOBBY_STRING(GameStagingLogic_GettingGameInfoDetails_String), 1, 0);
        st->mJoinOp = NULL;
        return 0;
    case EeWonLobbyOp::StJoinOpKill:
        if (st->mJoinOp.get() != NULL) { st->mJoinOp->Kill(true); st->mJoinOp = NULL; }
        return 0;
    case EeWonLobbyOp::StLogoutTail:
        st->mCreateGameDialog->LCall(EeWonLobbyDlgCall::Reset, true);
        LobbyNetLogout();
        LobbyKillPings(0);
        LobbyShowLoginScreen();
        LobbySaveGlobalPrefs(false);
        LobbySaveUserPrefs(false);
        LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop();
        return 0;
    case EeWonLobbyOp::GameLeaveNet:
        g->ClearPlayers();
        st->mMyClient = NULL;
        g->PreNetLeaveGame();
        LobbyNetLeaveGame(g);
        g->PostNetLeaveGame();
        return 0;
    case EeWonLobbyOp::GameSetInProgressNet:
        g->SetInProgress(inC != 0);
        LobbyNetSetGameInProgress(inC != 0);
        LobbyNetAdvertiseGame();
        return 0;
    case EeWonLobbyOp::GamePingAndNotify:
        LobbyPingGame(g, true);
        LobbyBroadcastEvent(GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::GameShowDetails:
        LobbyQueryGameDetails(g);
        g->SetShowDetails(true);
        LobbyBroadcastEvent(GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::GameHideDetails:
        g->SetShowDetails(false);
        LobbyBroadcastEvent(GameChangedEvent(g, 2), true);
        return 0;
    case EeWonLobbyOp::AllowJoinWithoutPing: {
        LobbyConfig* aConfig = GetLobbyConfig();
        return aConfig == NULL ? -1 : aConfig->mAllowJoinGameWithoutPing;
    }
    default: return INTPTR_MIN;
    }
}

// LanLogicHostCall's
static intptr_t Lan(LanLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    LanLogic* l = inLan;
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyLanSocket* sock = static_cast<LobbyLanSocket*>(inA);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    (void)inD;
    switch (inOp) {
    // R6 WON round 32's (LanLogicHost.cpp at af02fb9a5; lobbyops.rs composes it)
    case EeWonLobbyOp::LanPortDialogRun:
        l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetGamePort, (unsigned short)LobbyGetLobbyPort());
        l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetPort, (unsigned short)(int)inC);
        l->mLanPortDialog->LCall(EeWonLobbyDlgCall::Reset);
        return LobbyDoDialog(l->mLanPortDialog.get());
    case EeWonLobbyOp::LanAuthAutoRefreshOff: {
        AuthContext* anAuthContext = GetLobbyAuthContext();
        if (anAuthContext != NULL)
            anAuthContext->SetDoAutoRefresh(false);
        return 0;
    }
    case EeWonLobbyOp::LanWhisperReceived: {
        if (LobbyGetChatSoundEffects())
            LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_WhisperRecv_Sound).get());
        ChatCommandParser* aParser = LobbyGetChatCommandParser();
        if (aParser != NULL)
            aParser->SetInput(cl->GetName());
        return 0;
    }
    case EeWonLobbyOp::LanNameDialogSave:
        LobbySetLanName(l->mLanNameDialog->InputText(0));
        LobbyBroadcastLobbyEvent(0x186d2, true);
        return 0;
    case EeWonLobbyOp::LanSetBroadcastPort:
        LobbySetLanBroadcastPort((int)inC);
        LobbyBroadcastLobbyEvent(0x186d2, true);
        return 0;
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
    case EeWonLobbyOp::LanStartPeerAuth: {
        AuthContext* auth = GetLobbyAuthContext();
        if (!auth) return 0;
        SmartPtr<PeerAuthServerOp> server = new PeerAuthServerOp(auth, sock);
        l->RunTrackedOp(server.get(), new LogicCompletion<LanLogic>(&LanLogic::HandlePeerAuthDone, l), 30000);
        return 0;
    }
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
        sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept));
        return 1;
    }
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
        static_cast<QueueSocket*>(inA)->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvFrom, NULL, NULL, NULL, 1024));
        return 0;
    case EeWonLobbyOp::LanConnectTo: {
        SmartPtr<AsyncOp> connect;
        if (inC != 0) {
            SmartPtr<AuthContext> auth = GetLobbyAuthContext();
            connect = new PeerAuthOp(g->GetAddress(), auth.get(), AUTH_TYPE_PERSISTENT, static_cast<LobbyLanSocket*>(inB), true);
        } else {
            IPAddr anAddress = g->GetAddress();
            connect = new SocketOp(EeWonCoreSocketOp::Connect, static_cast<LobbyLanSocket*>(inB), NULL, &anAddress);
        }
        LobbyRunOpWithStatus(connect.get(), LOBBY_STRING(LANLogic_ConnectToGameTitle_String), LOBBY_STRING(LANLogic_ConnectToGameDetails_String), 1, 0);
        return connect->GetStatus() == WS_Success;
    }
    case EeWonLobbyOp::LanRecvReply: {
        SmartPtr<SocketOp> reply = new SocketOp(EeWonCoreSocketOp::RecvMsg, sock);
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
        sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvMsg));
        return 0;
    case EeWonLobbyOp::LanDirectSave: {
        GUIString aName = l->mDirectConnectDialog->InputText(0);
        if (ee_won_lanlogic_direct_name_is_address(aName.length()))
            aName = l->mDirectConnectDialog->InputText(1);
        g->SetName(aName);
        LobbyAddDirectConnect(Addr(inB), aName);
        return 0;
    }
    default: return INTPTR_MIN;
    }
}
}
namespace LoWorld {
// LanLogic::HandlePeerAuthDone, as it was
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
}
#undef ee_won_ipaddr_assign
#undef ee_won_lobby_sink
#endif
