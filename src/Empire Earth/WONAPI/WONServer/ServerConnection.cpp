// ServerConnection: persistent authenticated server socket with keep-alive and message callbacks. A Rust object
// since net round 16 (rust/crates/ee_won/src/servconn.rs EeWonServerConn: the auth, the peer authentication op, the
// connect completion, the keep-alive timer and period, and every step); the C++ class keeps the objects' calls the
// Rust object makes (the hooks below) and ServerConnectionConnectOp
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
typedef ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> > ConnCompletion;
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

// FUNCTION: WON 0x006aab10
// Creates an unauthenticated server connection with a 300-second keep-alive
ServerConnection::ServerConnection() : BlockingSocket(0)
{
    this->mConn=ee_won_servconn_new(static_cast<void*>(this),WonCore(),&kCpp,static_cast<void*>(this->GetDataLock()));
}
// FUNCTION: WON 0x006abfc0
// Creates an op that connects a server connection to an address
ServerConnectionConnectOp::ServerConnectionConnectOp(ServerConnection* inConnection,const IPAddr& inAddr)
    : mConnection(inConnection),mAddr(inAddr)
{
}
// FUNCTION: WON 0x006ac240
// Forwards the connect completion to its op
void ServerConnectionConnectOp::StaticConnectCompletion(AsyncOpPtr inOp,SmartPtr<RefCount> inParam)
{
    ((ServerConnectionConnectOp*)inParam.get())->ConnectCompletion(inOp.get());
}
// FUNCTION: WON 0x006ac2d0
// Finishes the pending op with the connect status
void ServerConnectionConnectOp::ConnectCompletion(AsyncOp* inOp)
{
    if(this->GetStatus()!=WS_Pending) return;
    this->Finish(inOp->GetStatus());
}
// FUNCTION: WON 0x006ac2f0
// Starts an asynchronous connect, completing the op when it finishes
void ServerConnectionConnectOp::RunHook()
{
    this->mConnection->SetConnectCompletion(new ConnCompletion(ServerConnectionConnectOp::StaticConnectCompletion,
                                                               SmartPtr<RefCount>(this)));
    this->mConnection->ConnectAsync(this->mAddr,this->GetTimeout());
}
// FUNCTION: WON 0x006ac380
// Removes the op's connect completion from the connection
void ServerConnectionConnectOp::CleanupHook()
{
    this->mConnection->SetConnectCompletion(NULL);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ac1a0
// (public: virtual __thiscall WONAPI::ServerConnectionConnectOp::~ServerConnectionConnectOp(void)    [??1ServerConnectionConnectOp@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006aeab0
// (public: virtual void * __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount>)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aabc0
// (public: virtual void * __thiscall WONAPI::ServerConnection::`scalar deleting destructor'(unsigned int)    [??_GServerConnection@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006aabe0
// (public: virtual __thiscall WONAPI::ServerConnection::~ServerConnection(void)    [??1ServerConnection@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006abab0
// (public: virtual void * __thiscall WONAPI::SendBytesOp::`scalar deleting destructor'(unsigned int)    [??_GSendBytesOp@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006abb60
// (public: virtual void * __thiscall WONAPI::SendMsgOp::`scalar deleting destructor'(unsigned int)    [??_GSendMsgOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ac180
// (public: virtual void * __thiscall WONAPI::ServerConnectionConnectOp::`scalar deleting destructor'(unsigned int)    [??_GServerConnectionConnectOp@WONA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae7e0
// (public: __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >::ParamComple)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae840
// (public: virtual void __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >)
// TEMPLATE: WON 0x006aead0
// (public: virtual __thiscall WONAPI::ParamCompletion<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,class WONAPI::SmartPtr<class WONAPI::RefCount> >::~Pa)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006aad90
// (public: virtual __thiscall WONAPI::BlockingSocket::~BlockingSocket(void)    [??1BlockingSocket@WONAPI@@UAE@XZ])
