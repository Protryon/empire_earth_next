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
#include "ee/wonapi/WONLobbyStaging.h"
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

// ---- LanLogic: the RustLogic of kind Lan (its virtuals, completions and its four dialogs' lookup: logicshell.rs)

namespace WONAPI {
// The LAN logic's Rust object with an empty client list and empty LAN and direct-connect game lists (its own)
void* LanLogicMake(RustLogic* inSelf)
{
    return ee_won_lan_new_model(inSelf, ee_won_lobbycpp_call, new LobbyClientList, new LobbyGameList(1), new LobbyGameList(3));
}
}

// ---- the LanLogic ops (lobbyhost.rs documents each)

// the third and fourth bytes of an address's six-byte form (the fourth times 256)
static intptr_t LanSubnet(const IPAddr& inAddr) { return (intptr_t)((uint32_t)(unsigned char)inAddr.GetSixByte()[2] | (uint32_t)(unsigned char)inAddr.GetSixByte()[3] << 8); }
static void LanStagingBytes(ReadBuffer inBuffer, LobbyClient* inClient) { LobbyHandleStagingReadBuffer(inBuffer, inClient); }
static bool LanGameReadSummary(ReadBuffer inBuffer, LobbyGame* inGame) { return inGame->ReadSummary(inBuffer); }

// a connect op to the address (the op copies it)
static SocketOp* LanNewConnectOp(LobbyLanSocket* inSocket, IPAddr inAddress) { return new SocketOp(EeWonCoreSocketOp::Connect, inSocket, NULL, &inAddress); }

intptr_t LanLogicHostCall(RustLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD)
{
    RustLogic* l = inLan;
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
    // LanSubnets is lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::LanPeerSubnet: return LanSubnet(sock->GetDestAddr());
    case EeWonLobbyOp::LanLocalSubnet: return LanSubnet(IPAddr::GetLocalAddr());
    case EeWonLobbyOp::LanGamePasswordIs: return LanWide(inC, inD) == std::wstring(g->GetPassword());
    // LanNewClient, LanPortDialogRun and LanNewTimer are lobbyops.rs's over these (R6 WON round 32)
    case EeWonLobbyOp::LanClientAlloc: return Held(new LobbyLanClient);
    case EeWonLobbyOp::LanClientSetName: lc->SetName(GUIString(new GUIStringData(LanWide(inC, inD)))); return 0;
    case EeWonLobbyOp::LanPortSetGamePort: l->mDialog[kLanPort]->LCall(EeWonLobbyDlgCall::SetGamePort, (unsigned short)LobbyGetLobbyPort()); return 0;
    case EeWonLobbyOp::LanPortSetPort: l->mDialog[kLanPort]->LCall(EeWonLobbyDlgCall::SetPort, (unsigned short)(int)inC); return 0;
    case EeWonLobbyOp::LanPortReset: l->mDialog[kLanPort]->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case EeWonLobbyOp::LanPortDo: return LobbyDoDialog(l->mDialog[kLanPort].get());
    case EeWonLobbyOp::LanTimerAlloc: return Held(new AsyncOp);
    case EeWonLobbyOp::LanTimerSetCompletion: op->SetCompletion(new ShellCompletion(l, inC == 0 ? EeWonLogicShellDone::LanNetworkTimer : EeWonLogicShellDone::LanDiscoveryTimer)); return 0;
    case EeWonLobbyOp::ClientNameTake: Take(inB, LanUtf16(cl->GetName())); return 0;
    case EeWonLobbyOp::LanClientChanged:
        LobbyBroadcastEvent(ClientChangedEvent(cl, 8, (int)inC), true);
        return 0;
    case EeWonLobbyOp::ClientLeftGame: LobbyClientLeftGame(cl); return 0;
    case EeWonLobbyOp::LanSocketStamp: sock->SetLastMessageTime(time(NULL)); return 0;
    case EeWonLobbyOp::LanClientSocket: return (intptr_t)lc->mSocket;
    case EeWonLobbyOp::LanClientSocketSend: static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB))->mSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::SendMsg, NULL, LobbyHostMessage(inA, inC, inD, aHold)), -1); return 0;
    case EeWonLobbyOp::LanRecvChat: LobbyBroadcastEvent(RecvChatEvent(8, (unsigned long)(inD >> 32), cl, LanWide(inC, inD & 0xffffffff), static_cast<LobbyClient*>(inB)), true); return 0;
    // LanStagingRead is lobbyops.rs's over this (R6 WON round 36)
    case EeWonLobbyOp::LanStagingBytes: LanStagingBytes(ReadBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD), static_cast<LobbyClient*>(inB)); return 0;
    case EeWonLobbyOp::LanNameDialogRun: l->mDialog[kLanName]->LCall(EeWonLobbyDlgCall::Reset); return LobbyDoDialog(l->mDialog[kLanName].get());
    case EeWonLobbyOp::LanNameDialogLength: return (intptr_t)l->mDialog[kLanName]->InputText(0).length();
    case EeWonLobbyOp::LanBroadcastPort: return LobbyGetLanBroadcastPort();
    case EeWonLobbyOp::LanPortDialogPort: return (unsigned short)l->mDialog[kLanPort]->LCall(EeWonLobbyDlgCall::GetPort);
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
    // LanSummaryTake is lobbyops.rs's over these and WbufNew / WbufFree (R6 WON round 36)
    case EeWonLobbyOp::GameWriteSummary: g->WriteSummary(*static_cast<WriteBuffer*>(inB)); return 0;
    case EeWonLobbyOp::WbufSink: ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(static_cast<WriteBuffer*>(inA)->data()), static_cast<WriteBuffer*>(inA)->length()); return 0;
    case EeWonLobbyOp::LanPasswordDialog: l->mDialog[kLanPassword]->LCall(EeWonLobbyDlgCall::Reset); return LobbyDoDialog(l->mDialog[kLanPassword].get());
    case EeWonLobbyOp::LanPasswordTake: Take(inB, LanUtf16(l->mDialog[kLanPassword]->InputText(0))); return 0;
    case EeWonLobbyOp::LanPingGame: LobbyPingGame(g, false); return 0;
    case EeWonLobbyOp::LanDirectAddressAt:
        ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), LobbyGetDirectConnectAddressAt((size_t)inC).GetValue());
        return 0;
    case EeWonLobbyOp::LanDirectNameAt: g->SetName(LobbyGetDirectConnectNameAt((size_t)inC)); return 0;
    case EeWonLobbyOp::LanRemoveSavedDirect: LobbyRemoveDirectConnect(g->GetAddress()); return 0;
    // LanDirectAsk and LanReadSummary are lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::LanDirectReset: l->mDialog[kLanDirectConnect]->LCall(EeWonLobbyDlgCall::Reset); return 0;
    case EeWonLobbyOp::LanDirectDo: return LobbyDoDialog(l->mDialog[kLanDirectConnect].get());
    // (the configured port moved by the harness slot's offset, as LobbyBindUDPPort's: TEST ONLY)
    case EeWonLobbyOp::ConfigLobbyPortTest: return static_cast<LobbyConfig*>(inA)->mLobbyPort + LobbyTestPortOffset();
    case EeWonLobbyOp::LobbyPort: return LobbyGetLobbyPort();
    case EeWonLobbyOp::LanDirectHostTake: Take(inB, std::string(l->mDialog[kLanDirectConnect]->InputText(1))); return 0;
    case EeWonLobbyOp::LanDirectDnsHeld: return Held(NewDnsTestOp(l->mDialog[kLanDirectConnect]->InputText(1)));
    case EeWonLobbyOp::DnsOpAddrAssign: ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), static_cast<RustServerOp*>(static_cast<AsyncOp*>(static_cast<RefCount*>(inB)))->AddrOf(EE_WON_DNSTEST_ADDR).GetValue()); return 0;
    case EeWonLobbyOp::LanGameReadSummary: return LobbyReadGuard([&] { return LanGameReadSummary(ReadBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD), static_cast<LobbyGame*>(inB)); });
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
    case EeWonLobbyOp::GetCertNew: return Held(RustServerOp::NewGetCert(static_cast<AuthContext*>(inA)));
    case EeWonLobbyOp::LanRunPeerAuth: l->RunTrackedOp(RustServerOpPtr(RustServerOp::NewPeerAuthServer(static_cast<AuthContext*>(inB), sock)).get(), new ShellCompletion(l, EeWonLogicShellDone::LanPeerAuthDone), 30000);
        return 0;
    case EeWonLobbyOp::PeerAuthSocket: return (intptr_t) static_cast<LobbyLanSocket*>(static_cast<RustServerOp*>(op)->GetSocket());
    case EeWonLobbyOp::PeerAuthPermits: return static_cast<RustServerOp*>(op)->GetNum(EE_WON_PEEROP_CLIENT_PERMITS, (DWORD)inC) != 0;
    case EeWonLobbyOp::ChatSoundEffects: return LobbyGetChatSoundEffects() ? 1 : 0;
    case EeWonLobbyOp::PlayWhisperSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_WhisperRecv_Sound).get());
        return 0;
    case EeWonLobbyOp::ParserSetInputName: parser->SetInput(static_cast<LobbyClient*>(inB)->GetName());
        return 0;
    case EeWonLobbyOp::LanSetLanNameFromDialog: LobbySetLanName(l->mDialog[kLanName]->InputText(0));
        return 0;
    case EeWonLobbyOp::LanSetBroadcastPortTo: LobbySetLanBroadcastPort((int)inC);
        return 0;
    case EeWonLobbyOp::LanBindLobbyPort: return sock->Fn2B0D90(LobbyGetLobbyPort(), 0);
    case EeWonLobbyOp::ErrorDialogRow: LobbyDoErrorDialog(gLobbyResources[inC].mString, (WONStatus)inD);
        return 0;
    case EeWonLobbyOp::LanArmAcceptCompletion: sock->SetRepeatCompletion(new ShellCompletion(l, EeWonLogicShellDone::LanAccept));
        return 0;
    case EeWonLobbyOp::LanArmAcceptOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept));
        return 0;
    case EeWonLobbyOp::LanBroadcastAddrAssign: ee_won_ipaddr_assign(static_cast<EeWonIpAddr*>(inA), IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort()).GetValue());
        return 0;
    case EeWonLobbyOp::LanNewQueueSocket: return Held(new QueueSocket(1));
    case EeWonLobbyOp::LanBindBroadcast: return static_cast<QueueSocket*>(inA)->Fn2B0D90(LobbyGetLanBroadcastPort(), 1);
    case EeWonLobbyOp::LanArmDiscoveryCompletion: static_cast<QueueSocket*>(inA)->SetRepeatCompletion(new ShellCompletion(l, EeWonLogicShellDone::LanDiscoveryRecv));
        return 0;
    case EeWonLobbyOp::LanArmDiscoveryOp: static_cast<QueueSocket*>(inA)->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvFrom, NULL, NULL, NULL, 1024));
        return 0;
    case EeWonLobbyOp::LanNewPeerAuthConnect: return Held(RustServerOp::NewPeerAuth(g->GetAddress(), reinterpret_cast<AuthContext*>(inC), AUTH_TYPE_PERSISTENT, static_cast<LobbyLanSocket*>(inB), true));
    case EeWonLobbyOp::LanNewConnect: return Held(LanNewConnectOp(static_cast<LobbyLanSocket*>(inB), g->GetAddress()));
    case EeWonLobbyOp::LanNewRecvMsg: return Held(new SocketOp(EeWonCoreSocketOp::RecvMsg, sock));
    case EeWonLobbyOp::LanArmCloseCompletion: sock->SetCloseCompletion(new ShellCompletion(l, EeWonLogicShellDone::LanSocketClosed));
        return 0;
    case EeWonLobbyOp::LanArmRecvCompletion: sock->SetRepeatCompletion(new ShellCompletion(l, EeWonLogicShellDone::LanRecvMsg));
        return 0;
    case EeWonLobbyOp::LanArmRecvOp: sock->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::RecvMsg));
        return 0;
    case EeWonLobbyOp::LanDirectInputLength: return (intptr_t)l->mDialog[kLanDirectConnect]->InputText(0).length();
    case EeWonLobbyOp::GameSetDirectName: g->SetName(l->mDialog[kLanDirectConnect]->InputText((int)inC));
        return 0;
    case EeWonLobbyOp::AddDirectConnectNamed: LobbyAddDirectConnect(Addr(inB), l->mDialog[kLanDirectConnect]->InputText((int)inC));
        return 0;
    default: return 0;
    }
}
