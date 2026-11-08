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
    // Inline constructor (in Duplicate 0x7e3af0, HostGame and JoinGame): BlockingSocket(0), mClient(NULL),
    // then SetLengthFieldSize(2); mLastMessageTime (0xa4) is left uninitialized.
    LobbyLanSocket() : BlockingSocket(0) { this->SetLengthFieldSize(2); }
    // Implicit destructor 0x7e3ba0: destroys the queue members inline, then calls the AsyncSocket destructor.
    virtual AsyncSocket* Duplicate() { return new LobbyLanSocket; } // 0x7e3af0; returns a new LobbyLanSocket
    void SetClient(LobbyLanClient* inClient); // 0x7dfb90; also points the client back at this socket
    SmartPtr<LobbyLanClient> mClient; // 0xa0, owned
    long mLastMessageTime; // 0xa4: time(NULL), written at 0x7e12e9 and 0x7e14da
};

// LanLogic: 0xb8 bytes (allocated at 0x762f7f); members start at 0x40.
class LanLogic : public NetLogic {
public:
    LanLogic(); // 0x7dfc00
    int GetRoomSpec() const { return 8; }
    virtual ~LanLogic(); // 0x7dfef0
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7e00a0
    virtual void HandleControlEvent(int inControlId); // 0x7e2760
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7e27c0
    virtual bool HostGame(LobbyGame* inGame);  // 0x7e37a0
    virtual bool JoinGame(LobbyGame* inGame);  // 0x7e3d20
    virtual LobbyClient* GetMyClient(); // 0x7e0180
    virtual void SendToCaptain(const ByteBuffer* inBuffer); // 0x7e1c30
    virtual void BroadcastToGame(const ByteBuffer* inBuffer); // 0x7e1cf0
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer); // 0x7e1db0
    virtual void AdvertiseGame(LobbyGame* inGame); // 0x7e45c0
    virtual void RemoveGameAdvertisement(LobbyGame* inGame); // 0x7e45e0
    virtual void KickClient(LobbyClient* inClient, bool inFlag); // 0x7e4590
    virtual void SetGameInProgress(bool inFlag); // empty: the shared one-argument stub at 0x6fa500
    virtual void LeaveGame(LobbyGame* inGame); // 0x7e45b0
    virtual void Suspend();   // empty: the shared no-argument stub at 0x8142a0
    virtual void Resume();    // 0x7e5900
    virtual void Logout();    // 0x7e5910
    unsigned short GetNextClientId();
    bool IsConnected();
    bool IsCaptain();
    LobbyClientList* GetClientList();
    void CloseConnections();
    void HandleSocketClosed(AsyncOp* inOp);
    void HandleNetworkTimer(AsyncOp* inOp);
    void StartNetworkTimer();
    void SendMessageToClient(LobbyClient* inClient, const ByteBuffer* inBuffer);
    void SendMessageToCaptain(const ByteBuffer* inBuffer);
    void BroadcastMessage(const ByteBuffer* inBuffer, bool inFlag);
    void HandleClientJoined(ReadBuffer& inBuffer);
    void HandleClientLeft(ReadBuffer& inBuffer);
    void HandleChatRequest(ReadBuffer& inBuffer, LobbyClient* inSender);
    void HandleChatMessage(ReadBuffer& inBuffer);
    void HandleStagingMessage(ReadBuffer& inBuffer, LobbyClient* inSender);
    void HandleMessage(const ByteBuffer* inBuffer, LobbyClient* inClient);
    void ChangeLanName();
    void ChangeLanPort();
    void StartDiscovery();
    void HandleChangeDirectConnect(ChangeDirectConnectEvent* inEvent);
    bool CanBroadcastGame(LobbyGame* inGame);
    void BroadcastGameSummary(LobbyGame* inGame, const IPAddr& inAddress);
    void BroadcastGameKeepAlive(LobbyGame* inGame);
    void BroadcastGameRemoved(LobbyGame* inGame);
    void HandleDiscoveryQuery(ReadBuffer&, const IPAddr&);
    void HandleGameSummary(ReadBuffer&, const IPAddr&);
    void HandleGameKeepAlive(ReadBuffer&, const IPAddr&);
    void HandleGameRemoved(ReadBuffer&, const IPAddr&);
    void HandleDiscoveryTimer(AsyncOp* inOp);
    void StartDiscoveryTimer();
    void StopDiscovery();

    void HandleClientAction(ClientActionEvent*);
    void SendChat(const GUIString&,unsigned short,unsigned short);
    void HandleChatCommand(Component*);
    void RefreshGames();
    void EnterLanScreen();
    void HandleRecvMsg(AsyncOp*); // receive completion
    void HandleNewSocketMessage(const ByteBuffer*,LobbyLanSocket*);
    void HandleJoinRequest(ReadBuffer&,LobbyLanSocket*);
    int ReadJoinReply(const ByteBuffer*,LobbyGame*);
    void ShowClientOptions(LobbyClient*,int);
    void AddPendingSocket(LobbyLanSocket*);
    bool RefreshCertificate();
    void HandlePeerAuthDone(AsyncOp*); // 0x7e1600; peer authentication completion
    void HandleAccept(AsyncOp*); // accept completion
    void HandleDiscoveryRecv(AsyncOp*); // discovery receive completion

    SmartPtr<DirectConnectDialog> mDirectConnectDialog; // 0x40
    SmartPtr<LanNameDialog> mLanNameDialog; // 0x44
    SmartPtr<LanPortDialog> mLanPortDialog; // 0x48
    SmartPtr<PasswordDialog> mPasswordDialog; // 0x4c
    SmartPtr<LobbyGameList> mLanGames; // 0x50, LobbyGameList(1) (0x2c bytes, constructor 0x7a7c50)
    SmartPtr<LobbyGameList> mDirectGames; // 0x54, LobbyGameList(3)
    std::set<SmartPtr<LobbyLanSocket> > mPendingSockets; // 0x58
    SmartPtr<LobbyLanSocket> mListenSocket; // 0x64, 0xa8 bytes, vtable 0x8559ec
    SmartPtr<LobbyLanSocket> mServerSocket; // 0x68, 0xa8 bytes, vtable 0x8559ec
    long mLastServerPing; // 0x6c, time(NULL); compared in the network timer 0x7e04e0
    AsyncOpPtr mNetworkTimer; // 0x70, timer operation (constructor 0x69ef80), callback 0x7e04e0
    bool mRequirePeerAuth; // 0x74, set for direct-connect games (game kind 3) (inferred)
    SmartPtr<QueueSocket> mDiscoverySocket; // 0x78, QueueSocket (0xa0 bytes, constructor 0x6b3930, type 1)
    AsyncOpPtr mDiscoveryTimer; // 0x7c, timer operation, callback 0x7e4be0
    IPAddr mDiscoveryAddress; // 0x80
    long mLastBroadcastTime; // 0xa8, time(NULL), compared signed; not initialized by the constructor
    SmartPtr<LobbyClientList> mClients; // 0xac
    SmartPtr<LobbyLanClient> mMyClient; // 0xb0, LobbyLanClient (vtable 0x8559d8)
    unsigned short mNextClientId; // 0xb4, wraps at 16 bits
    bool mIsCaptain; // 0xb6
};
}
#endif
