// LAN and direct-connect play: LobbyLanClient, LobbyLanSocket and LanLogic
#ifndef EE_WONAPI_WONLOBBYLAN_H
#define EE_WONAPI_WONLOBBYLAN_H
#include <set>
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
class DirectConnectDialog;
class LanNameDialog;
class LanPortDialog;
class PasswordDialog;
class ChangeDirectConnectEvent;
class ClientActionEvent;
class LobbyGameList;
class QueueSocket;
class LobbyLanSocket;

// RTTI 0x8559d8; 0x30 bytes (allocated at 0x7e0ada and 0x7e37f6); LobbyClient is 0x2c bytes.
// mSocket does not own the socket; the socket owns this client.
class LobbyLanClient : public LobbyClient {
public:
    LobbyLanClient() : mSocket(NULL) {}
    // Implicit destructor (0x7e0f80): tail-calls the base without writing its own vtable pointer.
    LobbyLanSocket* mSocket; // 0x2c, not owned; the socket owns this client at 0xa0
};

class LobbyLanSocket : public BlockingSocket {
public:
    // The original's inline constructor (in Duplicate 0x7e3af0, HostGame and JoinGame): BlockingSocket(0),
    // mClient(NULL), then SetLengthFieldSize(2); LanLogic.cpp's now (the Rust object)
    LobbyLanSocket();
    virtual ~LobbyLanSocket(); // the Rust object (the original's was implicit, 0x7e3ba0)
    virtual AsyncSocket* Duplicate() { return new LobbyLanSocket; } // 0x7e3af0; returns a new LobbyLanSocket
    void SetClient(LobbyLanClient* inClient); // 0x7dfb90; also points the client back at this socket
    LobbyLanClient* GetClient() const { return static_cast<LobbyLanClient*>(ee_won_lansocket_client(this->mLan)); }
    long GetLastMessageTime() const { return static_cast<long>(ee_won_lansocket_last_message_time(this->mLan)); }
    void SetLastMessageTime(long inTime) { ee_won_lansocket_set_last_message_time(this->mLan,inTime); }
private:
    // A Rust object since round 14 (rust/crates/ee_won/src/qsock.rs EeWonLanSocket): the client (0xa0, a reference
    // held) and the last message's time (0xa4, time(NULL); 0 until the first)
    EeWonLanSocket* mLan;
};

// LanLogic: 0xb8 bytes in the original (allocated at 0x762f7f); vtable slots as NetLogic's.
// A Rust object since round 20 (rust/crates/ee_won/src/lanobj.rs): its methods and scalar state (the next client
// id, the captain and peer-auth flags, the last host ping and broadcast times) are Rust's, its calls out the host's
// ops (WONLobby/LanLogicHost.cpp); this shell keeps the SmartPtrs, the socket set, the discovery address, the
// virtual slots and completions, and the steps that are C++ alone.
class LanLogic : public NetLogic {
public:
    LanLogic(); // 0x7dfc00
    int GetRoomSpec() const { return 8; }
    virtual ~LanLogic(); // 0x7dfef0
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7e00a0
    virtual void HandleControlEvent(int inControlId); // 0x7e2760
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7e27c0
    virtual bool HostGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::HostGame, inGame) != 0; } // 0x7e37a0
    virtual bool JoinGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::JoinGame, inGame) != 0; } // 0x7e3d20
    virtual LobbyClient* GetMyClient() { return this->mMyClient.get(); } // 0x7e0180
    virtual void SendToCaptain(const ByteBuffer* inBuffer) // 0x7e1c30
    { this->CallMessage(EeWonLanCall::SendToCaptain, inBuffer, NULL); }
    virtual void BroadcastToGame(const ByteBuffer* inBuffer) // 0x7e1cf0
    { this->CallMessage(EeWonLanCall::BroadcastToGame, inBuffer, NULL); }
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer) // 0x7e1db0
    { this->CallMessage(EeWonLanCall::SendToClient, inBuffer, inClient); }
    virtual void AdvertiseGame(LobbyGame* inGame) { this->Call(EeWonLanCall::AdvertiseGame, inGame); } // 0x7e45c0
    virtual void RemoveGameAdvertisement(LobbyGame* inGame) // 0x7e45e0
    { this->Call(EeWonLanCall::RemoveGameAdvertisement, inGame); }
    virtual void KickClient(LobbyClient* inClient, bool) { this->Call(EeWonLanCall::KickClient, NULL, inClient); } // 0x7e4590
    virtual void SetGameInProgress(bool) {} // empty: the shared one-argument stub at 0x6fa500
    virtual void LeaveGame(LobbyGame*) { this->Call(EeWonLanCall::LeaveGame); } // 0x7e45b0
    virtual void Suspend() {} // empty: the shared no-argument stub at 0x8142a0
    virtual void Resume(); // 0x7e5900
    virtual void Logout() { this->Call(EeWonLanCall::Logout); } // 0x7e5910
    LobbyClientList* GetClientList() { return this->mClients.get(); }
    intptr_t Call(EeWonLanCall inCall, void* inA = NULL, void* inB = NULL, intptr_t inC = 0)
    { return ee_won_lan_call(this->mRust, inCall, inA, inB, inC, 0); }
    void CallMessage(EeWonLanCall inCall, const ByteBuffer* inMessage, LobbyClient* inClient);
    // the completions (Rust's)
    void HandleSocketClosed(AsyncOp* inOp) { this->Call(EeWonLanCall::SocketClosed, inOp); }
    void HandleNetworkTimer(AsyncOp* inOp) { this->Call(EeWonLanCall::NetworkTimer, inOp); }
    void HandleRecvMsg(AsyncOp* inOp) { this->Call(EeWonLanCall::RecvMsg, inOp); }
    void HandleDiscoveryTimer(AsyncOp* inOp) { this->Call(EeWonLanCall::DiscoveryTimer, inOp); }
    void HandleDiscoveryRecv(AsyncOp* inOp) { this->Call(EeWonLanCall::DiscoveryRecv, inOp); }
    void HandleAccept(AsyncOp* inOp) { this->Call(EeWonLanCall::Accept, inOp); }
    // the steps that are C++ alone (the host's ops)
    void HandlePeerAuthDone(AsyncOp* inOp); // 0x7e1600
    void AddPendingSocket(LobbyLanSocket* inSocket);
    void StartNetworkTimer();
    void StartDiscoveryTimer();
    void StopDiscovery();
    bool RefreshCertificate();
    void HandleGameSummary(unsigned short inPort, ReadBuffer& inSummary, const IPAddr& inAddress);
    void HandleGameKeepAlive(unsigned short inPort, const IPAddr& inAddress);
    void HandleGameRemoved(unsigned short inPort, const IPAddr& inAddress);
    void AddDirectConnect();

    SmartPtr<DirectConnectDialog> mDirectConnectDialog; // 0x40
    SmartPtr<LanNameDialog> mLanNameDialog; // 0x44
    SmartPtr<LanPortDialog> mLanPortDialog; // 0x48
    SmartPtr<PasswordDialog> mPasswordDialog; // 0x4c
    SmartPtr<LobbyGameList> mLanGames; // 0x50, LobbyGameList(1)
    SmartPtr<LobbyGameList> mDirectGames; // 0x54, LobbyGameList(3)
    std::set<SmartPtr<LobbyLanSocket> > mPendingSockets; // 0x58
    SmartPtr<LobbyLanSocket> mListenSocket; // 0x64
    SmartPtr<LobbyLanSocket> mServerSocket; // 0x68
    AsyncOpPtr mNetworkTimer; // 0x70, callback HandleNetworkTimer
    SmartPtr<QueueSocket> mDiscoverySocket; // 0x78, QueueSocket type 1
    AsyncOpPtr mDiscoveryTimer; // 0x7c, callback HandleDiscoveryTimer
    IPAddr mDiscoveryAddress; // 0x80
    SmartPtr<LobbyClientList> mClients; // 0xac
    SmartPtr<LobbyLanClient> mMyClient; // 0xb0
    GUIString mJoinPassword; // JoinGame's password (the original's loop local)
private:
    EeWonLan* mRust;
};
}
#endif
