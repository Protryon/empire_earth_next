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
extern wchar_t gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in WONGUI/GUIEmptyWideString.cpp)
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
    this->mRust = ee_won_lan_new_model(this, ee_won_lobbycpp_call, new LobbyClientList, new LobbyGameList(1), new LobbyGameList(3));
}

LanLogic::~LanLogic() { ee_won_lan_free(this->mRust); }

// Looks up the direct-connect, LAN name, LAN port and password dialogs from the lobby layout
void LanLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetDialog(inConfig, this->mDirectConnectDialog, EeWonLobbyDlgKind::DirectConnect, "DirectConnectDialog", "LanLogic.cpp", 105);
    SafeGetDialog(inConfig, this->mLanNameDialog, EeWonLobbyDlgKind::LanName, "LanNameDialog", "LanLogic.cpp", 106);
    SafeGetDialog(inConfig, this->mLanPortDialog, EeWonLobbyDlgKind::LanPort, "LanPortDialog", "LanLogic.cpp", 107);
    SafeGetDialog(inConfig, this->mPasswordDialog, EeWonLobbyDlgKind::Password, "PasswordDialog", "LanLogic.cpp", 108);
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

// Host: accepts a peer-authenticated socket whose certificate has the lobby's community, else closes it (the Rust's:
// lanobj.rs PeerAuthDone over lobbyops.rs's checks)
void LanLogic::HandlePeerAuthDone(AsyncOp* inOp) { this->Call(EeWonLanCall::PeerAuthDone, inOp); }

// ---- the LanLogic ops (lobbyhost.rs documents each)

static const ByteBuffer* LanMessageOf(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

// a connect op to the address (the op copies it)
static SocketOp* LanNewConnectOp(LobbyLanSocket* inSocket, IPAddr inAddress) { return new SocketOp(EeWonCoreSocketOp::Connect, inSocket, NULL, &inAddress); }

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
    case EeWonLobbyOp::LanSocketClient: return (intptr_t)static_cast<LobbyClient*>(sock->GetClient());
    case EeWonLobbyOp::LanSocketSetClient:
        sock->SetClient(static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB)));
        return 0;
    case EeWonLobbyOp::LanSocketLastTime: return sock->GetLastMessageTime();
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
    // LanNewClient, LanPortDialogRun and LanNewTimer are lobbyops.rs's over these (R6 WON round 32)
    case EeWonLobbyOp::LanClientAlloc: return Held(new LobbyLanClient);
    case EeWonLobbyOp::LanClientSetName: lc->SetName(GUIString(new GUIStringData(LanWide(inC, inD)))); return 0;
    case EeWonLobbyOp::LanPortSetGamePort: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetGamePort, (unsigned short)LobbyGetLobbyPort()); return 0;
    case EeWonLobbyOp::LanPortSetPort: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::SetPort, (unsigned short)(int)inC); return 0;
    case EeWonLobbyOp::LanPortReset: l->mLanPortDialog->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case EeWonLobbyOp::LanPortDo: return LobbyDoDialog(l->mLanPortDialog.get());
    case EeWonLobbyOp::LanTimerAlloc: return Held(new AsyncOp);
    case EeWonLobbyOp::LanTimerSetCompletion: op->SetCompletion(new LogicCompletion<LanLogic>(inC == 0 ? &LanLogic::HandleNetworkTimer : &LanLogic::HandleDiscoveryTimer, l)); return 0;
    case EeWonLobbyOp::ClientNameTake: Take(inB, LanUtf16(cl->GetName())); return 0;
    case EeWonLobbyOp::LanClientChanged:
        LobbyBroadcastEvent(ClientChangedEvent(cl, l->GetRoomSpec(), (int)inC), true);
        return 0;
    case EeWonLobbyOp::ClientLeftGame: LobbyClientLeftGame(cl); return 0;
    case EeWonLobbyOp::LanSocketStamp: sock->SetLastMessageTime(time(NULL)); return 0;
    case EeWonLobbyOp::LanClientSocket: return (intptr_t)lc->mSocket;
    case EeWonLobbyOp::LanClientSocketSend: static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB))->mSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::SendMsg, NULL, LanMessageOf(inA, inC, inD, aHold)), -1); return 0;
    case EeWonLobbyOp::LanRecvChat: LobbyBroadcastEvent(RecvChatEvent(8, (unsigned long)(inD >> 32), cl, LanWide(inC, inD & 0xffffffff), static_cast<LobbyClient*>(inB)), true); return 0;
    case EeWonLobbyOp::LanStagingRead: {
        ReadBuffer aBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD);
        try {
            aBuffer.ReadByte();
            LobbyHandleStagingReadBuffer(aBuffer, static_cast<LobbyClient*>(inB));
        }
        catch (ReadBufferException&) {}
        return 0;
    }
    case EeWonLobbyOp::LanNameDialogRun: l->mLanNameDialog->LCall(EeWonLobbyDlgCall::Reset); return LobbyDoDialog(l->mLanNameDialog.get());
    case EeWonLobbyOp::LanNameDialogLength: return (intptr_t)l->mLanNameDialog->InputText(0).length();
    case EeWonLobbyOp::LanBroadcastPort: return LobbyGetLanBroadcastPort();
    case EeWonLobbyOp::LanPortDialogPort: return (unsigned short)l->mLanPortDialog->LCall(EeWonLobbyDlgCall::GetPort);
    case EeWonLobbyOp::EventComponent: return (intptr_t)static_cast<ComponentEvent*>(inA)->mComponent;
    case EeWonLobbyOp::SetClientIgnored: LobbySetClientIgnored(cl, inC != 0, (int)inD); return 0;
    case EeWonLobbyOp::LanSetChatInput:
        LobbyBroadcastEvent(SetChatInputEvent(2, cl->GetName(), (int)inC, false), true);
        return 0;
    case EeWonLobbyOp::KickClientFromLobby: LobbyKickClient(cl, false); return 0;
    case EeWonLobbyOp::ChatParser: return (intptr_t)LobbyGetChatCommandParser();
    case EeWonLobbyOp::ParserRoomFlags: return parser->GetRoomFlags();
    case EeWonLobbyOp::ParserTarget: return (intptr_t)parser->GetTarget();
    case EeWonLobbyOp::ParserCommand: return parser->GetCommand();
    case EeWonLobbyOp::ParserTextTake: Take(inB, LanUtf16(parser->GetText())); return 0;
    case EeWonLobbyOp::LanChatCommandError:
        LobbySendEventTo(ChatCommandErrorEvent(LOBBY_STRING(RoutingLogic_InvalidCommand_String)), static_cast<Component*>(inA));
        return 0;
    case EeWonLobbyOp::ShowLanScreen: LobbyShowLanScreen(); return 0;
    case EeWonLobbyOp::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case EeWonLobbyOp::KillFirewallTest: LobbyKillFirewallTest(); return 0;
    case EeWonLobbyOp::LanNameTake: Take(inB, LanUtf16(LobbyGetLanName())); return 0;
    // the C++ calls of the cases over the members (lanown.rs)
    case EeWonLobbyOp::LanGameDisconnected: LobbyGameDisconnected(true); return 0;
    case EeWonLobbyOp::LanSyncGameList: LobbyBroadcastEvent(SyncGameListEvent(static_cast<LobbyGameList*>(inA)), true); return 0;
    case EeWonLobbyOp::LanGameChanged: LobbyBroadcastEvent(GameChangedEvent(g, (int)inC), true); return 0;
    case EeWonLobbyOp::LanNewSocket: return Held(new LobbyLanSocket);
    case EeWonLobbyOp::LanJoinedRoom:
        LobbyBroadcastEvent(JoinedRoomEvent("", g->GetName(), 10, (unsigned short)inC, g->GetClients().get()), true);
        return 0;
    case EeWonLobbyOp::LanSummaryTake: {
        WriteBuffer aBuffer(0, true);
        g->WriteSummary(aBuffer);
        ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aBuffer.data()), aBuffer.length());
        return 0;
    }
    case EeWonLobbyOp::LanPasswordDialog: l->mPasswordDialog->LCall(EeWonLobbyDlgCall::Reset); return LobbyDoDialog(l->mPasswordDialog.get());
    case EeWonLobbyOp::LanPasswordTake: Take(inB, LanUtf16(l->mPasswordDialog->InputText(0))); return 0;
    case EeWonLobbyOp::LanPingGame: LobbyPingGame(g, false); return 0;
    case EeWonLobbyOp::LanDirectAddressAt:
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), LobbyGetDirectConnectAddressAt((size_t)inC).GetValue());
        return 0;
    case EeWonLobbyOp::LanDirectNameAt: g->SetName(LobbyGetDirectConnectNameAt((size_t)inC)); return 0;
    case EeWonLobbyOp::LanRemoveSavedDirect: LobbyRemoveDirectConnect(g->GetAddress()); return 0;
    case EeWonLobbyOp::LanDirectAsk: {
        l->mDirectConnectDialog->LCall(EeWonLobbyDlgCall::Reset);
        if (LobbyDoDialog(l->mDirectConnectDialog.get()) != 0x186a1)
            return 0;
        IPAddr anAddress;
        LobbyConfig* aConfig = GetLobbyConfig();
        // (the configured port moved by the harness slot's offset, as LobbyBindUDPPort's: TEST ONLY)
        int aDefaultPort = ee_won_lanlogic_direct_default_port(aConfig != NULL,
                                                               aConfig != NULL ? aConfig->mLobbyPort + LobbyTestPortOffset() : 0,
                                                               LobbyGetLobbyPort());
        anAddress.SetWithDefaultPortNoDNS(l->mDirectConnectDialog->InputText(1), aDefaultPort);
        if (!anAddress.IsValid()) {
            SmartPtr<DNSTestOp> anOp = new DNSTestOp(l->mDirectConnectDialog->InputText(1));
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
    case EeWonLobbyOp::LanReadSummary: {
        // the game's summary follows the type and the port (LobbyGame::ReadSummary)
        const ByteBuffer* aBytes = static_cast<SocketOp*>(op)->GetBytes().get();
        ReadBuffer aBuffer(aBytes->data(), aBytes->length());
        try {
            aBuffer.ReadByte();
            aBuffer.ReadShort();
            return static_cast<LobbyGame*>(inB)->ReadSummary(aBuffer);
        }
        catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
    }
    case EeWonLobbyOp::LanClientOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Client, (intptr_t)cl, (intptr_t)inB, (int)inC);
        return 0;
    }
    // the statements of the cases lobbyops.rs answers (R6 WONLobby round 29: the certificate, the peer
    // authentication, the listen, the discovery socket, the connect and register, the sockets' completions, the
    // whisper, the LAN name and port, the direct connections)
    case EeWonLobbyOp::AuthGet: return (intptr_t)GetLobbyAuthContext();
    case EeWonLobbyOp::AuthSetAutoRefresh: static_cast<AuthContext*>(inA)->SetDoAutoRefresh(inC != 0);
        return 0;
    case EeWonLobbyOp::AuthIsExpired: return static_cast<AuthContext*>(inA)->IsExpired((time_t)inC) ? 1 : 0;
    case EeWonLobbyOp::AuthClearUserName: static_cast<AuthContext*>(inA)->SetUserName(std::wstring(gEmptyWideString));
        return 0;
    case EeWonLobbyOp::AuthClearPassword: static_cast<AuthContext*>(inA)->SetPassword(std::wstring(gEmptyWideString));
        return 0;
    case EeWonLobbyOp::AuthCommunity: return (intptr_t)static_cast<AuthContext*>(inA)->GetPeerData()->GetFirstCommunity();
    case EeWonLobbyOp::CdKeyValid: return LobbyIsCDKeyValid() ? 1 : 0;
    case EeWonLobbyOp::GetCertNew: return Held(new GetCertOp(static_cast<AuthContext*>(inA)));
    case EeWonLobbyOp::LanRunPeerAuth: l->RunTrackedOp(SmartPtr<PeerAuthServerOp>(new PeerAuthServerOp(static_cast<AuthContext*>(inB), sock)).get(), new LogicCompletion<LanLogic>(&LanLogic::HandlePeerAuthDone, l), 30000);
        return 0;
    case EeWonLobbyOp::PeerAuthSocket: return (intptr_t) static_cast<LobbyLanSocket*>(static_cast<PeerAuthServerOp*>(op)->GetSocket());
    case EeWonLobbyOp::PeerAuthPermits: return static_cast<PeerAuthServerOp*>(op)->ClientPermits((DWORD)inC) ? 1 : 0;
    case EeWonLobbyOp::ChatSoundEffects: return LobbyGetChatSoundEffects() ? 1 : 0;
    case EeWonLobbyOp::PlayWhisperSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_WhisperRecv_Sound).get());
        return 0;
    case EeWonLobbyOp::ParserSetInputName: parser->SetInput(static_cast<LobbyClient*>(inB)->GetName());
        return 0;
    case EeWonLobbyOp::LanSetLanNameFromDialog: LobbySetLanName(l->mLanNameDialog->InputText(0));
        return 0;
    case EeWonLobbyOp::LanSetBroadcastPortTo: LobbySetLanBroadcastPort((int)inC);
        return 0;
    case EeWonLobbyOp::LanBindLobbyPort: return sock->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case EeWonLobbyOp::ErrorDialogRow: LobbyDoErrorDialog(gLobbyResources[inC].mString, (WONStatus)inD);
        return 0;
    case EeWonLobbyOp::LanArmAcceptCompletion: sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleAccept, l));
        return 0;
    case EeWonLobbyOp::LanArmAcceptOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept));
        return 0;
    case EeWonLobbyOp::LanBroadcastAddrAssign: ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort()).GetValue());
        return 0;
    case EeWonLobbyOp::LanNewQueueSocket: return Held(new QueueSocket(1));
    case EeWonLobbyOp::LanBindBroadcast: return static_cast<QueueSocket*>(inA)->Fn2B0D90(LobbyGetLanBroadcastPort(), 1);
    case EeWonLobbyOp::LanArmDiscoveryCompletion: static_cast<QueueSocket*>(inA)->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryRecv, l));
        return 0;
    case EeWonLobbyOp::LanArmDiscoveryOp: static_cast<QueueSocket*>(inA)->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvFrom, NULL, NULL, NULL, 1024));
        return 0;
    case EeWonLobbyOp::LanNewPeerAuthConnect: return Held(new PeerAuthOp(g->GetAddress(), reinterpret_cast<AuthContext*>(inC), AUTH_TYPE_PERSISTENT, static_cast<LobbyLanSocket*>(inB), true));
    case EeWonLobbyOp::LanNewConnect: return Held(LanNewConnectOp(static_cast<LobbyLanSocket*>(inB), g->GetAddress()));
    case EeWonLobbyOp::LanNewRecvMsg: return Held(new SocketOp(EeWonCoreSocketOp::RecvMsg, sock));
    case EeWonLobbyOp::LanArmCloseCompletion: sock->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, l));
        return 0;
    case EeWonLobbyOp::LanArmRecvCompletion: sock->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, l));
        return 0;
    case EeWonLobbyOp::LanArmRecvOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvMsg));
        return 0;
    case EeWonLobbyOp::LanDirectInputLength: return (intptr_t)l->mDirectConnectDialog->InputText(0).length();
    case EeWonLobbyOp::GameSetDirectName: g->SetName(l->mDirectConnectDialog->InputText((int)inC));
        return 0;
    case EeWonLobbyOp::AddDirectConnectNamed: LobbyAddDirectConnect(Addr(inB), l->mDirectConnectDialog->InputText((int)inC));
        return 0;
    default: return 0;
    }
}
