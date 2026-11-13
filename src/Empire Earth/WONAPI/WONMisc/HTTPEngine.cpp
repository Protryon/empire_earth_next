// HTTPSession and HTTPConnection: per-host keep-alive connections with request pipelining
// (file grouping inferred)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <time.h>
#include <list>
#include <vector>
#include <map>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {

// HTTPSession's map and HTTPConnection's state machine are ee_won's (rust/crates/ee_won/src/httpengine.rs); these
// classes keep the locks and are the RefCounts the ops' completions hold. The hooks hand Rust the C++ objects as
// RefCounts.
struct HTTPEngineHooks {
    static RefCount* Obj(void* p) { return static_cast<RefCount*>(p); }
    static RustServerOp* Op(void* p) { return static_cast<RustServerOp*>(static_cast<AsyncOp*>(Obj(p))); }
    static void* Ref(RefCount* p) { return p; }
    static void ConnLock(void* c) { static_cast<HTTPConnection*>(c)->mDataCrit.Enter(); }
    static void ConnUnlock(void* c) { static_cast<HTTPConnection*>(c)->mDataCrit.Leave(); }
    static void SessionLock(void* c) { static_cast<HTTPSession*>(c)->mDataCrit.Enter(); }
    static void SessionUnlock(void* c) { static_cast<HTTPSession*>(c)->mDataCrit.Leave(); }
    static void AddRef(void* p) { Obj(p)->AddRef(); }
    static void Release(void* p) { Obj(p)->Release(); }
    static long Status(void* p) { return Op(p)->GetStatus(); }
    static void Kill(void* p) { Op(p)->Kill(true); }
    static void ForceFinish(void* p, long s) { Op(p)->ForceFinish(s); }
    static void* Completion(void* p)
    {
        CompletionBase<AsyncOpPtr>* c = Op(p)->GetCompletion();
        if (c == NULL) return NULL;
        c->AddRef();
        return Ref(c);
    }
    static void SetCompletion(void* p, void* c)
    {
        Op(p)->SetCompletion(c != NULL ? static_cast<CompletionBase<AsyncOpPtr>*>(Obj(c)) : NULL);
    }
    static void SetConnCompletion(void* p, void* c)
    {
        Op(p)->SetCompletion(new ParamCompletion<AsyncOpPtr,SmartPtr<RefCount> >(
            HTTPConnection::StaticOpCallback,SmartPtr<RefCount>(static_cast<HTTPConnection*>(c))));
    }
    static void Prepare(void* p, void* s)
    {
        RustServerOp* op = Op(p);
        op->SetNum(EE_WON_HTTPGET_CLOSE, 0);
        op->SetNum(EE_WON_HTTPGET_RANGE_RESUME, 0);
        op->Set(EE_WON_HTTPGET_SOCKET, s != NULL ? Obj(s) : NULL);
    }
    static void RunAsync(void* p) { Op(p)->RunAsync(Op(p)->GetTimeout()); }
    static void* Socket(void* p)
    {
        BlockingSocket* s = Op(p)->GetHttpSocket();
        if (s == NULL) return NULL;
        s->AddRef();
        return Ref(s);
    }
    static void SetMode(void* p, int m)
    {
        Op(p)->SetNum(EE_WON_HTTPGET_MODE, m == 2 ? 2 : m == 0 ? 0 : 1); // only receive, only send, both
    }
    static void Complete(void* c, void* p)
    {
        static_cast<CompletionBase<AsyncOpPtr>*>(Obj(c))->Complete(AsyncOpPtr(static_cast<AsyncOp*>(Op(p))));
    }
    static void SocketKill(void* s) { static_cast<BlockingSocket*>(Obj(s))->Kill(); }
    static int64_t Now(void*) { return (int64_t)time(NULL); }
    static void Host(void* p, EeWonByteSink sink, void* sc)
    {
        std::string h = Op(p)->GetBytes(EE_WON_HTTPGET_HOST);
        sink(sc, reinterpret_cast<const uint8_t*>(h.data()), h.size());
    }
    static void* NewConn(void*)
    {
        HTTPConnection* c = new HTTPConnection(NULL);
        c->AddRef();
        return Ref(c);
    }
    static void ConnQueue(void* c, void* p) { static_cast<HTTPConnection*>(Obj(c))->QueueOp(Op(p)); }
    static void ConnKill(void* c) { static_cast<HTTPConnection*>(Obj(c))->Kill(); }
    static int ConnNumOps(void* c) { return static_cast<HTTPConnection*>(Obj(c))->GetNumOps(); }

    static EeWonHttpConn* NewConnImpl(HTTPConnection* c)
    {
        EeWonHttpConnHooks h;
        h.context = c;
        h.lock = ConnLock;
        h.unlock = ConnUnlock;
        h.add_ref = AddRef;
        h.release = Release;
        h.op_status = Status;
        h.op_kill = Kill;
        h.op_force_finish = ForceFinish;
        h.op_completion = Completion;
        h.op_set_completion = SetCompletion;
        h.op_set_conn_completion = SetConnCompletion;
        h.op_prepare = Prepare;
        h.op_run_async = RunAsync;
        h.op_socket = Socket;
        h.op_set_mode = SetMode;
        h.complete = Complete;
        h.socket_kill = SocketKill;
        h.now = Now;
        return ee_won_http_conn_new(&h);
    }
    static EeWonHttpSession* NewSessionImpl(HTTPSession* s)
    {
        EeWonHttpSessionHooks h;
        h.context = s;
        h.lock = SessionLock;
        h.unlock = SessionUnlock;
        h.add_ref = AddRef;
        h.release = Release;
        h.op_host = Host;
        h.new_conn = NewConn;
        h.conn_queue_op = ConnQueue;
        h.conn_kill = ConnKill;
        h.conn_num_ops = ConnNumOps;
        return ee_won_http_session_new(&h);
    }
};

// FUNCTION: WON 0x006d28f0
// Queues an HTTP get on the connection for its host, creating the connection on first use
void HTTPSession::AddOp(RustServerOp* inOp)
{
    ee_won_http_session_add_op(this->mImpl, HTTPEngineHooks::Ref(static_cast<AsyncOp*>(inOp)));
}

// FUNCTION: WON 0x006d2bc0
// Kills every connection's running and queued requests
void HTTPSession::KillAll()
{
    ee_won_http_session_kill_all(this->mImpl);
}

// FUNCTION: WON 0x006d2c80
// Creates an idle connection with pipelining on
HTTPConnection::HTTPConnection(HTTPSession* inSession)
{
    this->mSession = inSession;
    this->mImpl = HTTPEngineHooks::NewConnImpl(this);
}
// TEMPLATE: WON 0x006d2d50
// Destroys the connection: its socket, queues and saved completions
HTTPConnection::~HTTPConnection()
{
    ee_won_http_conn_free(this->mImpl);
}

// FUNCTION: WON 0x006d3600
// Kills the running and queued requests (finishing queued ones as killed) and drops the socket
void HTTPConnection::Kill()
{
    ee_won_http_conn_kill(this->mImpl);
}

// FUNCTION: WON 0x006d33a0
// Queues a request as send-only on the pipeline queue, or as a full request when pipelining is off
void HTTPConnection::QueueOp(RustServerOp* inOp)
{
    ee_won_http_conn_queue_op(this->mImpl, HTTPEngineHooks::Ref(static_cast<AsyncOp*>(inOp)));
}
// FUNCTION: WON 0x006d3290
// Appends a request to the send (0) or receive (1) queue and starts it when the queue was empty
void HTTPConnection::QueueOp(RustServerOp* inOp,int inId)
{
    ee_won_http_conn_queue_op_id(this->mImpl, HTTPEngineHooks::Ref(static_cast<AsyncOp*>(inOp)), inId);
}

// HTTPConnection is 0x64 bytes, HTTPSession 0x2c bytes

HTTPSession::HTTPSession()
{
    this->mImpl = HTTPEngineHooks::NewSessionImpl(this);
}
// TEMPLATE: WON 0x006d2870
// Destroys the session: its connections
HTTPSession::~HTTPSession()
{
    ee_won_http_session_free(this->mImpl);
}
// FUNCTION: WON 0x006d3430
// Starts the next request of a queue on the shared socket, routing its completion back to the connection
void HTTPConnection::RunOp(int inId)
{
    ee_won_http_conn_run_op(this->mImpl, inId);
}

// FUNCTION: WON 0x006d2df0
// Completion trampoline: passes a finished request to its connection's OpCallback
void HTTPConnection::StaticOpCallback(AsyncOpPtr inOp,SmartPtr<RefCount> inConnection)
{
    static_cast<HTTPConnection*>(inConnection.get())->OpCallback(
        static_cast<RustServerOp*>(inOp.get()));
}

// FUNCTION: WON 0x006d2e80
// Handles a finished request: queues a sent one for its reply or completes it; requeues all when the pipeline drops
void HTTPConnection::OpCallback(RustServerOp* inOp)
{
    ee_won_http_conn_op_callback(this->mImpl, HTTPEngineHooks::Ref(static_cast<AsyncOp*>(inOp)));
}

// No original address identified: WON SDK request counting
int HTTPSession::GetNumOps()
{
    return ee_won_http_session_num_ops(this->mImpl);
}

int HTTPConnection::GetNumOps()
{
    return ee_won_http_conn_num_ops(this->mImpl);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2d30
// (public: virtual void * __thiscall WONAPI::HTTPConnection::`scalar deleting destructor'(unsigned int)    [??_GHTTPConnection@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d27b0
// (public: __thiscall WONAPI::HTTPSession::HTTPSession(void)    [??0HTTPSession@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006d2870
// (public: virtual __thiscall WONAPI::HTTPSession::~HTTPSession(void)    [??1HTTPSession@WONAPI@@UAE@XZ])

// TEMPLATE: WON 0x006d2850
// (HTTPSession scalar deleting destructor)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006d2b30
// (public: __thiscall std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WONAPI::SmartPtr<cla)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006dad40
// (public: class std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp> > >::iter)
// TEMPLATE: WON 0x006dc9b0
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>)
// TEMPLATE: WON 0x006d9c10
// (public: __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp> > >:)
// TEMPLATE: WON 0x006daa30
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_)
// TEMPLATE: WON 0x006dc680
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class s)
// TEMPLATE: WON 0x006dc6c0
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTTPGetOp>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTTPGet)
// TEMPLATE: WON 0x006da980
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006dc160
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct std::pair<class std::basic_str)
// TEMPLATE: WON 0x006dcac0
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
