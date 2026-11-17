// ServerConnection: persistent authenticated server socket with keep-alive and message callbacks. A Rust object
// since net round 16 (rust/crates/ee_won_net/src/servconn.rs EeWonServerConn: the auth, the peer authentication op, the
// connect completion, the keep-alive timer and period, and every step); the C++ class keeps the objects' calls the
// Rust object makes (the hooks below)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"
namespace WONAPI {
static RefCount* Ref(void* inObject) { return static_cast<RefCount*>(inObject); }
static ServerConnection* Conn(void* inConn) { return static_cast<ServerConnection*>(inConn); }
// what only the C++ answers (the rest is Rust's: ee_won cppcore.rs NATIVE_SERVCONN)
static void* HookNewPeerAuth(void* inConn,const void* inAddr,void* inContext,int32_t inAuthType)
{
    PeerAuthOp* anOp=new PeerAuthOp(*static_cast<const IPAddr*>(inAddr),static_cast<AuthContext*>(Ref(inContext)),
                                    static_cast<AuthType>(inAuthType),Conn(inConn),true);
    return static_cast<RefCount*>(anOp);
}
static void HookMsgCallback(void* inConn,void* inMessage)
{
    Conn(inConn)->MsgCallback(static_cast<const ByteBuffer*>(Ref(inMessage)));
}
static const EeWonServerConnCpp kCpp={HookNewPeerAuth,HookMsgCallback};

// Creates an unauthenticated server connection with a 300-second keep-alive
ServerConnection::ServerConnection() : BlockingSocket(0)
{
    this->mConn=ee_won_servconn_new(static_cast<void*>(this),WonCore(),&kCpp,static_cast<void*>(this->GetDataLock()));
}
}
