// LAN and direct-connect play: LobbyLanClient, LobbyLanSocket and LanLogic
#ifndef EE_WONAPI_WONLOBBYLAN_H
#define EE_WONAPI_WONLOBBYLAN_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
class DirectConnectDialog;
class LanNameDialog;
class LanPortDialog;
class PasswordDialog;
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
    // A Rust object since round 14 (rust/crates/ee_won_net/src/qsock.rs EeWonLanSocket): the client (0xa0, a reference
    // held) and the last message's time (0xa4, time(NULL); 0 until the first)
    EeWonLanSocket* mLan;
};

// LanLogic: 0xb8 bytes in the original (allocated at 0x762f7f); vtable slots as NetLogic's.
// A Rust object since round 20 (rust/crates/ee_won_lobby/src/lanobj.rs): its methods and state are Rust's (since WONLobby
// round 11 also the members the shell held: the client list, the LAN and direct-connect games, this player's
// client, the sockets and their set, the timers, the discovery address, the join password: lanown.rs), its calls out
// the host's ops (WONLobby/LanLogicHost.cpp); this shell keeps the dialogs, the virtual slots and completions.
class LanLogic : public NetLogic {
public:
    LanLogic();
    int GetRoomSpec() const { return 8; }
    virtual ~LanLogic();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    virtual bool HostGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::HostGame, inGame) != 0; }
    virtual bool JoinGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::JoinGame, inGame) != 0; }
    virtual LobbyClient* GetMyClient() { return reinterpret_cast<LobbyClient*>(this->Call(EeWonLanCall::MyClient)); }
    virtual void SendToCaptain(const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::SendToCaptain, inBuffer, NULL); }
    virtual void BroadcastToGame(const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::BroadcastToGame, inBuffer, NULL); }
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::SendToClient, inBuffer, inClient); }
    virtual void AdvertiseGame(LobbyGame* inGame) { this->Call(EeWonLanCall::AdvertiseGame, inGame); }
    virtual void RemoveGameAdvertisement(LobbyGame* inGame)
    { this->Call(EeWonLanCall::RemoveGameAdvertisement, inGame); }
    virtual void KickClient(LobbyClient* inClient, bool) { this->Call(EeWonLanCall::KickClient, NULL, inClient); }
    virtual void SetGameInProgress(bool) {} // empty: the shared one-argument stub at 0x6fa500
    virtual void LeaveGame(LobbyGame*) { this->Call(EeWonLanCall::LeaveGame); }
    virtual void Suspend() {} // empty: the shared no-argument stub at 0x8142a0
    virtual void Resume();
    virtual void Logout() { this->Call(EeWonLanCall::Logout); }
    LobbyClientList* GetClientList() { return reinterpret_cast<LobbyClientList*>(this->Call(EeWonLanCall::ClientList)); }
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
    void HandlePeerAuthDone(AsyncOp* inOp); // the certificate's check (C++), then the Rust's AddPending

    SmartPtr<DirectConnectDialog> mDirectConnectDialog; // 0x40
    SmartPtr<LanNameDialog> mLanNameDialog; // 0x44
    SmartPtr<LanPortDialog> mLanPortDialog; // 0x48
    SmartPtr<PasswordDialog> mPasswordDialog; // 0x4c
private:
    EeWonLan* mRust;
};
}
#endif
