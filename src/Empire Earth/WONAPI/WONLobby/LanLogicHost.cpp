// LanLogic's C++ host (NEW, no original address: units.EXTRA_UNITS): the shell of LanLogic, a Rust object since
// round 20 (rust/crates/ee_won/src/lanobj.rs), its steps that are C++ alone, LobbyLanSocket, and the LanLogic ops
// of ee_won_lobbycpp_call (LobbyLogicHost.cpp; lobbyhost.rs EeWonLobbyOp from LanMyClient on). It replaces
// LanLogic.cpp.
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
#include "rust/ee_won.h"
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
// FUNCTION: WON 0x007dfb90
void LobbyLanSocket::SetClient(LobbyLanClient* inClient) { ee_won_lansocket_set_client(this->mLan, inClient); }

// ---- LanLogic's shell

// FUNCTION: WON 0x007dfc00
// Creates the LAN logic with an empty client list and empty LAN and direct-connect game lists
LanLogic::LanLogic() : mJoinPassword(GetEmptyGUIStringData()), mRust(ee_won_lan_new(this, ee_won_lobbycpp_call))
{
    this->mClients = new LobbyClientList;
    this->mLanGames = new LobbyGameList(1);
    this->mDirectGames = new LobbyGameList(3);
}

// FUNCTION: WON 0x007dfef0
LanLogic::~LanLogic() { ee_won_lan_free(this->mRust); }

// FUNCTION: WON 0x007e00a0
// Looks up the direct-connect, LAN name, LAN port and password dialogs from the lobby layout
void LanLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mDirectConnectDialog, "DirectConnectDialog", "LanLogic.cpp", 105);
    SafeGetComponent(inConfig, this->mLanNameDialog, "LanNameDialog", "LanLogic.cpp", 106);
    SafeGetComponent(inConfig, this->mLanPortDialog, "LanPortDialog", "LanLogic.cpp", 107);
    SafeGetComponent(inConfig, this->mPasswordDialog, "PasswordDialog", "LanLogic.cpp", 108);
}

// FUNCTION: WON 0x007e2760
// The LAN name, LAN port, refresh game list and show LAN screen commands
void LanLogic::HandleControlEvent(int inControlId)
{
    int aWhich = inControlId == ID_AddLANName ? 1 : inControlId == ID_ChangeLANBroadcast ? 2
               : inControlId == ID_RefreshGameList ? 3 : inControlId == ID_ShowLanScreen ? 4 : 0;
    if (aWhich != 0) this->Call(EeWonLanCall::Control, NULL, NULL, aWhich);
}

// FUNCTION: WON 0x007e27c0
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

// FUNCTION: WON 0x007e5900
// Leaves the current game when the lobby resumes
void LanLogic::Resume() { LobbyLeaveGame(0); }

// FUNCTION: WON 0x007e1600
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
    this->AddPendingSocket(socket);
}

// FUNCTION: WON 0x007e13b0
// Host: starts receiving messages on an accepted socket and keeps it in the socket set
void LanLogic::AddPendingSocket(LobbyLanSocket* inSocket)
{
    inSocket->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, this));
    inSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, this));
    inSocket->SetRepeatOp(new RecvMsgOp);
    inSocket->SetLastMessageTime(time(NULL));
    this->mPendingSockets.insert(inSocket);
}

// FUNCTION: WON 0x007e0620
// Starts (creating if needed) the 2-minute network timer
void LanLogic::StartNetworkTimer()
{
    if (this->mNetworkTimer.get() == NULL) {
        this->mNetworkTimer = new AsyncOp;
        this->mNetworkTimer->SetCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleNetworkTimer, this));
    }
    this->mNetworkTimer->RunAsTimer(120000);
}

// FUNCTION: WON 0x007e53a0
// Starts (creating if needed) the 15-second discovery timer
void LanLogic::StartDiscoveryTimer()
{
    if (this->mDiscoveryTimer.get() == NULL) {
        this->mDiscoveryTimer = new AsyncOp;
        this->mDiscoveryTimer->SetCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryTimer, this));
    }
    this->mDiscoveryTimer->RunAsTimer(15000);
}

// FUNCTION: WON 0x007e5810
// Clears the LAN games and closes the discovery timer and socket
void LanLogic::StopDiscovery()
{
    if (!this->mLanGames->IsEmpty()) {
        this->mLanGames->Clear();
        LobbyBroadcastEvent(new SyncGameListEvent(this->mLanGames.get()), true);
    }
    if (this->mDiscoveryTimer.get() != NULL) {
        this->mDiscoveryTimer->Kill(true);
        this->mDiscoveryTimer = NULL;
    }
    if (this->mDiscoverySocket.get() != NULL) {
        this->mDiscoverySocket->Kill();
        this->mDiscoverySocket = NULL;
    }
}

// FUNCTION: WON 0x007e35a0
// Gets a fresh login certificate (anonymous) unless the current one is valid; false on bad CD key or failure
bool LanLogic::RefreshCertificate()
{
    AuthContext* auth = GetLobbyAuthContext();
    if (!auth) return false;
    if (!LobbyIsCDKeyValid()) {
        LobbyMessageBox(::AccountLogic_InvalidCDKeyTitle_String, ::AccountLogic_InvalidCDKeyDetails_String);
        return false;
    }
    if (!auth->IsExpired(120)) {
        auth->SetDoAutoRefresh(true);
        return true;
    }
    auth->SetUserName(std::wstring(gEmptyWideString));
    auth->SetPassword(std::wstring(gEmptyWideString));
    SmartPtr<GetCertOp> op = new GetCertOp(auth);
    LobbyRunOpWithStatus(op.get(), ::LobbyContainer_LoginTitle_String, ::LobbyContainer_LoginDetails_String, 1, 0);
    if (op->GetStatus() == WS_Success) {
        auth->SetDoAutoRefresh(true);
        return true;
    }
    return false;
}

// FUNCTION: WON 0x007e4cb0
// Adds or updates a LAN game from its summary; an unreadable one is removed but reported as modified
void LanLogic::HandleGameSummary(unsigned short inPort, ReadBuffer& inBuffer, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inPort);
    bool aFlag = false;
    LobbyGame* aGame = this->mLanGames->GetOrCreateGame(anAddress, &aFlag);
    EeWonLanSummary anOutcome = ee_won_lanlogic_summary(aGame->ReadSummary(inBuffer), aFlag);
    SmartPtr<LobbyGame> aHeldGame = aGame;
    if (anOutcome.keep)
        aGame->SetLastSeenTime(time(NULL));
    else
        this->mLanGames->RemoveGame(anAddress);
    if (anOutcome.event >= 0)
        LobbyBroadcastEvent(new GameChangedEvent(aGame, anOutcome.event), true);
}

// FUNCTION: WON 0x007e4ec0
// Marks a known LAN game as just seen
void LanLogic::HandleGameKeepAlive(unsigned short inPort, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inPort);
    long anIP = inAddress.GetHost();
    if (anAddress.GetHost() != anIP)
        return;
    LobbyGame* aGame = this->mLanGames->GetGame(anAddress);
    if (aGame == NULL)
        return;
    aGame->SetLastSeenTime(time(NULL));
}

// FUNCTION: WON 0x007e5050
// Removes a LAN game its host announced as gone
void LanLogic::HandleGameRemoved(unsigned short inPort, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inPort);
    long aBroadcast = inAddress.GetHost();
    if (anAddress.GetHost() != aBroadcast)
        return;
    SmartPtr<LobbyGame> aGame = this->mLanGames->RemoveGame(anAddress);
    if (aGame.get() != NULL)
        LobbyBroadcastEvent(new GameChangedEvent(aGame.get(), 1), true);
}

// FUNCTION: WON 0x007e28d0
// HandleChangeDirectConnect's add: asks for an address (resolving DNS) and adds or renames a saved game
void LanLogic::AddDirectConnect()
{
    this->mDirectConnectDialog->Reset();
    if (LobbyDoDialog(this->mDirectConnectDialog.get()) != 0x186a1)
        return;
    IPAddr anAddress;
    LobbyConfig* aConfig = GetLobbyConfig();
    int aDefaultPort = ee_won_lanlogic_direct_default_port(aConfig != NULL, aConfig != NULL ? aConfig->mLobbyPort : 0,
                                                           LobbyGetLobbyPort());
    anAddress.SetWithDefaultPortNoDNS(this->mDirectConnectDialog->GetAddress(), aDefaultPort);
    if (!anAddress.IsValid()) {
        SmartPtr<DNSTestOp> anOp = new DNSTestOp(this->mDirectConnectDialog->GetAddress());
        LobbyRunOpWithStatus(anOp.get(), ::LANLogic_CheckingAddressTitle_String, ::LANLogic_CheckingAddressDetails_String, 1, 0);
        if (!anOp->Succeeded())
            return;
        anAddress = anOp->GetHostAddr();
        if (anAddress.GetPort() == 0)
            anAddress.SetPort(aDefaultPort);
    }
    GUIString aName = this->mDirectConnectDialog->GetName();
    if (ee_won_lanlogic_direct_name_is_address(aName.length()))
        aName = this->mDirectConnectDialog->GetAddress();
    bool aFlag = false;
    LobbyGame* aGame = this->mDirectGames->GetOrCreateGame(anAddress, &aFlag);
    aGame->SetName(aName);
    LobbyAddDirectConnect(anAddress, aName);
    LobbyBroadcastEvent(new GameChangedEvent(aGame, ee_won_lanlogic_direct_event(aFlag)), true);
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
    case EeWonLobbyOp::LanMyClient: return (intptr_t)static_cast<LobbyClient*>(l->mMyClient.get());
    case EeWonLobbyOp::LanSetMyClient: l->mMyClient = lc; return 0;
    case EeWonLobbyOp::LanListenSocket: return (intptr_t)l->mListenSocket.get();
    case EeWonLobbyOp::LanServerSocket: return (intptr_t)l->mServerSocket.get();
    case EeWonLobbyOp::LanNetworkTimer: return (intptr_t)l->mNetworkTimer.get();
    case EeWonLobbyOp::LanDiscoveryTimer: return (intptr_t)l->mDiscoveryTimer.get();
    case EeWonLobbyOp::LanDiscoverySocket: return (intptr_t)l->mDiscoverySocket.get();
    case EeWonLobbyOp::LanDiscoveryAddress: return (intptr_t)&l->mDiscoveryAddress;
    case EeWonLobbyOp::LanAuthAutoRefreshOff: {
        AuthContext* anAuthContext = GetLobbyAuthContext();
        if (anAuthContext != NULL)
            anAuthContext->SetDoAutoRefresh(false);
        return 0;
    }
    case EeWonLobbyOp::LanCloseSockets:
        if (l->mListenSocket.get() != NULL) {
            l->mListenSocket->Kill();
            l->mListenSocket = NULL;
        }
        if (l->mServerSocket.get() != NULL) {
            l->mServerSocket->QueueClose(-1);
            l->mServerSocket = NULL;
        }
        if (l->mNetworkTimer.get() != NULL) {
            l->mNetworkTimer->Kill(true);
            l->mNetworkTimer = NULL;
        }
        for (std::set<SmartPtr<LobbyLanSocket> >::iterator anItr = l->mPendingSockets.begin();
             anItr != l->mPendingSockets.end(); ++anItr)
            (*anItr)->QueueClose(-1);
        l->mMyClient = NULL;
        l->mPendingSockets.clear();
        l->mClients->Clear();
        return 0;
    case EeWonLobbyOp::LanOpSocket: return (intptr_t)static_cast<LobbyLanSocket*>(static_cast<SocketOp*>(op)->GetSocket());
    case EeWonLobbyOp::LanSocketClient: return (intptr_t)static_cast<LobbyClient*>(sock->GetClient());
    case EeWonLobbyOp::LanSocketSetClient:
        sock->SetClient(static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB)));
        return 0;
    case EeWonLobbyOp::LanForgetSocket: l->mPendingSockets.erase(sock); return 0;
    case EeWonLobbyOp::LanLoseHost:
        l->mServerSocket->Kill();
        l->mServerSocket = NULL;
        LobbyGameDisconnected(true);
        return 0;
    case EeWonLobbyOp::LanPendingCount: return (intptr_t)l->mPendingSockets.size();
    case EeWonLobbyOp::LanPendingAt: {
        std::set<SmartPtr<LobbyLanSocket> >::iterator anItr = l->mPendingSockets.begin();
        for (intptr_t i = 0; i < inC && anItr != l->mPendingSockets.end(); ++i) ++anItr;
        return anItr != l->mPendingSockets.end() ? (intptr_t)anItr->get() : 0;
    }
    case EeWonLobbyOp::LanSocketLastTime: return sock->GetLastMessageTime();
    case EeWonLobbyOp::LanSocketClose: if (sock != NULL) sock->QueueClose((long)inC); return 0;
    case EeWonLobbyOp::LanStartNetworkTimer: l->StartNetworkTimer(); return 0;
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
        aClient->AddRef();
        return (intptr_t)static_cast<LobbyClient*>(aClient);
    }
    case EeWonLobbyOp::LanAddClient: cl->SetClientId((unsigned short)inC); l->mClients->AddClient(cl); return 0;
    case EeWonLobbyOp::ClientHold: cl->AddRef(); return (intptr_t)cl;
    case EeWonLobbyOp::ClientRelease: cl->Release(); return 0;
    case EeWonLobbyOp::ClientNameTake: Take(inB, LanUtf16(cl->GetName())); return 0;
    case EeWonLobbyOp::LanClientCount: return (intptr_t)l->mClients->GetCount();
    case EeWonLobbyOp::LanClientAt: return (intptr_t)l->mClients->ClientAt((size_t)inC);
    case EeWonLobbyOp::LanFindClient: return (intptr_t)l->mClients->FindClient((unsigned short)inC);
    case EeWonLobbyOp::LanRemoveClient: {
        SmartPtr<LobbyClient> aClient = l->mClients->RemoveClient((unsigned short)inC);
        if (aClient.get() != NULL) aClient->AddRef();
        return (intptr_t)aClient.get();
    }
    case EeWonLobbyOp::LanClientChanged:
        LobbyBroadcastEvent(new ClientChangedEvent(cl, l->GetRoomSpec(), (int)inC), true);
        return 0;
    case EeWonLobbyOp::ClientLeftGame: LobbyClientLeftGame(cl); return 0;
    case EeWonLobbyOp::GameSetCaptainId: g->SetCaptainId((unsigned short)inC); return 0;
    case EeWonLobbyOp::LanServerClientIsCaptain:
        l->mServerSocket->SetClient(lc);
        lc->SetCaptain(true, true);
        return 0;
    case EeWonLobbyOp::LanRecvOk: return op->GetStatus() == WS_Success && !op->IsKilled();
    case EeWonLobbyOp::LanSocketStamp: sock->SetLastMessageTime(time(NULL)); return 0;
    case EeWonLobbyOp::LanRecvMsgTake: {
        const ByteBuffer* aMsg = static_cast<RecvMsgOp*>(op)->GetMsg().get();
        if (aMsg != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return (intptr_t)aMsg;
    }
    case EeWonLobbyOp::LanAddPending: l->AddPendingSocket(sock); return 0;
    case EeWonLobbyOp::LanClientSocket: return (intptr_t)lc->mSocket;
    case EeWonLobbyOp::LanClientSocketSend:
        static_cast<LobbyLanClient*>(static_cast<LobbyClient*>(inB))->mSocket->QueueOp(
            new SendMsgOp(LanMessageOf(inA, inC, inD, aHold)), -1);
        return 0;
    case EeWonLobbyOp::LanServerSend: l->mServerSocket->QueueOp(new SendMsgOp(LanMessageOf(inA, inC, inD, aHold)), -1); return 0;
    case EeWonLobbyOp::ClientIsIgnored: return cl->IsIgnored();
    case EeWonLobbyOp::LanWhisperReceived: {
        if (LobbyGetChatSoundEffects())
            LobbyPlaySound(::LobbyGlobal_WhisperRecv_Sound.get());
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
    case EeWonLobbyOp::LanDirectRemove:
        LobbyRemoveDirectConnect(g->GetAddress());
        l->mDirectGames->RemoveGame(g->GetAddress());
        LobbyBroadcastEvent(new GameChangedEvent(g, 1), true);
        return 0;
    case EeWonLobbyOp::LanDirectAdd: l->AddDirectConnect(); return 0;
    case EeWonLobbyOp::LanShowClientOptions: {
        SmartPtr<ClientOptionsPopup> aPopup = new ClientOptionsPopup;
        aPopup->Init(cl, l->mMyClient.get(), (int)inC);
        LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
        return 0;
    }
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
        LobbySendEventTo(new ChatCommandErrorEvent(::RoutingLogic_InvalidCommand_String), static_cast<Component*>(inA));
        return 0;
    case EeWonLobbyOp::LanClearLanGames:
        l->mLanGames->Clear();
        LobbyBroadcastEvent(new SyncGameListEvent(l->mLanGames.get()), true);
        return 0;
    case EeWonLobbyOp::LanPingDirectGames:
        l->mDirectGames->Rewind();
        while (l->mDirectGames->HasMore()) {
            LobbyGame* aGame = l->mDirectGames->Next();
            if (!aGame->GetIsFiltered())
                LobbyPingGame(aGame, false);
        }
        LobbyBroadcastLobbyEvent(0x186da, true);
        return 0;
    case EeWonLobbyOp::LanEnterScreenLists:
        LobbySetCurrentUser(NULL);
        l->mLanGames->Clear();
        l->mDirectGames->Clear();
        for (size_t anItr = 0; anItr < LobbyGetDirectConnectCount(); ++anItr) {
            LobbyGame* aGame = l->mDirectGames->GetOrCreateGame(LobbyGetDirectConnectAddressAt(anItr), NULL);
            aGame->SetName(LobbyGetDirectConnectNameAt(anItr));
        }
        LobbyBroadcastEvent(new SyncGameListEvent(l->mDirectGames.get()), true);
        return 0;
    case EeWonLobbyOp::ShowLanScreen: LobbyShowLanScreen(); return 0;
    case EeWonLobbyOp::DoCDKeyCheck: return LobbyDoCDKeyCheck();
    case EeWonLobbyOp::KillFirewallTest: LobbyKillFirewallTest(); return 0;
    case EeWonLobbyOp::LanRefreshCertificate: return l->RefreshCertificate();
    case EeWonLobbyOp::LanHostMyClient:
        l->mMyClient = new LobbyLanClient;
        l->mMyClient->SetClientId((unsigned short)inC);
        l->mMyClient->SetName(LobbyGetLanName());
        l->mMyClient->SetCaptain(true, true);
        l->mClients->AddClient(l->mMyClient.get());
        g->SetClients(l->mClients.get());
        return 0;
    case EeWonLobbyOp::LanHostListen: {
        l->mListenSocket = new LobbyLanSocket;
        WONStatus aStatus = l->mListenSocket->Fn2B0D90(LobbyGetLobbyPort(), 0);
        if (aStatus != 0) {
            LobbyDoErrorDialog(::LANLogic_SocketBindError_String, aStatus);
            return 0;
        }
        aStatus = l->mListenSocket->Listen(1000);
        if (aStatus != 0) {
            LobbyDoErrorDialog(::LANLogic_SocketListenError_String, aStatus);
            return 0;
        }
        l->mListenSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleAccept, l));
        l->mListenSocket->SetRepeatOp(new AcceptOp(NULL));
        LobbyBroadcastEvent(new JoinedRoomEvent("", g->GetName(), 10, l->mMyClient->GetClientId(),
                                                g->GetClients().get()), true);
        return 1;
    }
    case EeWonLobbyOp::GameAddressPort: return g->GetAddress().GetPort();
    case EeWonLobbyOp::LanSendSummary: {
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendBytes(reinterpret_cast<const void*>(inC), (unsigned long)inD);
        g->WriteSummary(aBuffer);
        l->mDiscoverySocket->QueueOp(new SendBytesToOp(aBuffer.ToByteBuffer(true).get(), *static_cast<IPAddr*>(inB)), -1);
        return 0;
    }
    case EeWonLobbyOp::LanDiscoverySend:
        l->mDiscoverySocket->QueueOp(new SendBytesToOp(LanMessageOf(inA, inC, inD, aHold), *static_cast<IPAddr*>(inB)), -1);
        return 0;
    case EeWonLobbyOp::LanRemoveStaleGames:
        if (l->mLanGames->RemoveStaleGames(30))
            LobbyBroadcastEvent(new SyncGameListEvent(l->mLanGames.get()), true);
        return 0;
    case EeWonLobbyOp::LanStartDiscoveryTimer: l->StartDiscoveryTimer(); return 0;
    case EeWonLobbyOp::LanDiscoveryOk: {
        RecvBytesFromOp* anOp = static_cast<RecvBytesFromOp*>(op);
        return anOp->GetSocket() == l->mDiscoverySocket.get() && anOp->Succeeded();
    }
    case EeWonLobbyOp::LanDiscoveryTake: {
        const ByteBuffer* aBytes = static_cast<RecvBytesFromOp*>(op)->GetBytes().get();
        if (aBytes != NULL) ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aBytes->data()), aBytes->length());
        return 0;
    }
    case EeWonLobbyOp::LanGameSummary: {
        // the game's summary follows the type and the port (LobbyGame::ReadSummary)
        RecvBytesFromOp* anOp = static_cast<RecvBytesFromOp*>(op);
        const ByteBuffer* aBytes = anOp->GetBytes().get();
        ReadBuffer aBuffer(aBytes->data(), aBytes->length());
        try {
            aBuffer.ReadByte();
            aBuffer.ReadShort();
            l->HandleGameSummary((unsigned short)inC, aBuffer, anOp->GetAddr());
        }
        catch (ReadBufferException&) {}
        return 0;
    }
    case EeWonLobbyOp::LanGameKeepAlive:
        l->HandleGameKeepAlive((unsigned short)inC, static_cast<RecvBytesFromOp*>(op)->GetAddr());
        return 0;
    case EeWonLobbyOp::LanGameRemoved:
        l->HandleGameRemoved((unsigned short)inC, static_cast<RecvBytesFromOp*>(op)->GetAddr());
        return 0;
    case EeWonLobbyOp::LanRecvFrom: return (intptr_t)&static_cast<RecvBytesFromOp*>(op)->GetAddr();
    case EeWonLobbyOp::LanOpenDiscovery:
        l->mDiscoveryAddress = IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort());
        l->mDiscoverySocket = new QueueSocket(1);
        // port: the discovery port is bound with address reuse (the original bound it exclusively), so that two
        // lobbies on one machine both receive the broadcasts
        return l->mDiscoverySocket->Fn2B0D90(LobbyGetLanBroadcastPort(), 1);
    case EeWonLobbyOp::LanDiscoveryArm:
        l->mDiscoverySocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryRecv, l));
        l->mDiscoverySocket->SetRepeatOp(new RecvBytesFromOp(1024, NULL));
        return 0;
    case EeWonLobbyOp::LanStopDiscovery: l->StopDiscovery(); return 0;
    case EeWonLobbyOp::LanClearDirectGames: l->mDirectGames->Clear(); return 0;
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
    case EeWonLobbyOp::LanNewServerSocket: l->mServerSocket = new LobbyLanSocket; return 0;
    case EeWonLobbyOp::GameHasPassword: return g->GetHasPassword();
    case EeWonLobbyOp::LanJoinPasswordClear: l->mJoinPassword = GUIString(GetEmptyGUIStringData()); return 0;
    case EeWonLobbyOp::LanPasswordDialogRun: {
        l->mPasswordDialog->Reset();
        int aResult = LobbyDoDialog(l->mPasswordDialog.get());
        if (aResult == 100001) l->mJoinPassword = l->mPasswordDialog->GetPassword();
        return aResult;
    }
    case EeWonLobbyOp::LanConnect: {
        SmartPtr<AsyncOp> connect;
        if (inC != 0) {
            SmartPtr<AuthContext> auth = GetLobbyAuthContext();
            connect = new PeerAuthOp(g->GetAddress(), auth.get(), AUTH_TYPE_PERSISTENT, l->mServerSocket.get(), true);
        } else
            connect = new ConnectOp(g->GetAddress(), l->mServerSocket.get());
        LobbyRunOpWithStatus(connect.get(), ::LANLogic_ConnectToGameTitle_String, ::LANLogic_ConnectToGameDetails_String, 1, 0);
        return connect->GetStatus() == WS_Success;
    }
    case EeWonLobbyOp::LanNameTake: Take(inB, LanUtf16(LobbyGetLanName())); return 0;
    case EeWonLobbyOp::LanJoinPasswordTake: Take(inB, LanUtf16(l->mJoinPassword)); return 0;
    case EeWonLobbyOp::LanRecvReplyTake: {
        SmartPtr<RecvMsgOp> reply = new RecvMsgOp(l->mServerSocket.get());
        LobbyRunOpWithStatus(reply.get(), ::LANLogic_RegisterTitle_String, ::LANLogic_RegisterDetails_String, 1, 0);
        if (reply->GetStatus() != WS_Success) return -1;
        const ByteBuffer* aMsg = reply->GetMsg().get();
        if (aMsg == NULL) return 0;
        ee_won_lobby_sink(inB, reinterpret_cast<const uint8_t*>(aMsg->data()), aMsg->length());
        return 1;
    }
    case EeWonLobbyOp::LanJoinArm:
        l->mServerSocket->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, l));
        l->mServerSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, l));
        l->mServerSocket->SetRepeatOp(new RecvMsgOp);
        l->StartNetworkTimer();
        g->SetClients(l->mClients.get());
        LobbyBroadcastEvent(new JoinedRoomEvent(GUIString(new GUIStringData("")), g->GetName(), 10,
                                                l->mMyClient->GetClientId(), g->GetClients().get()), true);
        return 0;
    default: return 0;
    }
}
