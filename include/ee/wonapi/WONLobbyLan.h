// LAN and direct-connect play: LobbyLanClient, LobbyLanSocket and LanLogic
#ifndef EE_WONAPI_WONLOBBYLAN_H
#define EE_WONAPI_WONLOBBYLAN_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
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
// the host's ops (WONLobby/LanLogicHost.cpp). Since R6 WON round 36 it is the RustLogic of kind Lan
// (ee/wonapi/WONLobbyLogic.h): its methods are the shell's NetLogic calls and Call(EeWonLanCall, ..), its dialogs
// the shell's slots, named here. (Its room spec, GetRoomSpec(), was 8.)
enum { kLanDirectConnect, kLanName, kLanPort, kLanPassword }; // mDialog: 0x40 .. 0x4c
}
#endif
