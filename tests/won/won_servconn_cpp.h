// ServerConnection.cpp's C++ hooks, kept for the drivers (R6 WONLobby round 12: the unit is deleted; the game's
// connections make their peer authentication op and reach RoutingConnection's MsgCallback in Rust, ee_won cppcore.rs).
// A driver with a stand-in PeerAuthOp (its own constructor) or a connection subclass overriding MsgCallback binds
// these once at start (BindServerConnectionCpp), as every connection bound them in the unit's constructor; the
// routing drivers bind the maker alone, so their messages take the game's path.
#ifndef EE_TESTS_WON_SERVCONN_CPP_H
#define EE_TESTS_WON_SERVCONN_CPP_H
#include "rust/won_net/hostop.h"
#include "rust/won_net/servconn.h"
namespace WONAPI {
namespace ServerConnectionCpp {
inline RefCount* Ref(void* inObject) { return static_cast<RefCount*>(inObject); }
inline ServerConnection* Conn(void* inConn) { return static_cast<ServerConnection*>(inConn); }
// a driver's own peer authentication op maker (NULL: the real RustServerOp::NewPeerAuth); RustServerOp is
// header-only since R6 WON lobby and network round 37, so a stand-in is a class of its own
typedef AsyncOp* (*PeerAuthMaker)(const IPAddr& inAddr, AuthContext* inContext, AuthType inType, BlockingSocket* inSocket);
inline PeerAuthMaker gPeerAuthMaker = NULL;
inline void* HookNewPeerAuth(void* inConn,const void* inAddr,void* inContext,int32_t inAuthType)
{
    if (gPeerAuthMaker != NULL)
        return static_cast<RefCount*>(gPeerAuthMaker(*static_cast<const IPAddr*>(inAddr), static_cast<AuthContext*>(Ref(inContext)),
                                                     static_cast<AuthType>(inAuthType), Conn(inConn)));
    return static_cast<RefCount*>(RustServerOp::NewPeerAuth(*static_cast<const IPAddr*>(inAddr),static_cast<AuthContext*>(Ref(inContext)),
                                                            static_cast<AuthType>(inAuthType),Conn(inConn),true));
}
inline void HookMsgCallback(void* inConn,void* inMessage)
{
    Conn(inConn)->MsgCallback(static_cast<const ByteBuffer*>(Ref(inMessage)));
}
}
// inMsgCallback false: the messages go the game's way (RoutingConnection's MsgCallback answered in Rust)
inline void BindServerConnectionCpp(bool inMsgCallback = true)
{
    static const EeWonServerConnCpp kCpp={ServerConnectionCpp::HookNewPeerAuth,ServerConnectionCpp::HookMsgCallback};
    static const EeWonServerConnCpp kPeerOnly={ServerConnectionCpp::HookNewPeerAuth,NULL};
    ee_won_servconn_bind_cpp(inMsgCallback ? &kCpp : &kPeerOnly);
}
}
#endif
