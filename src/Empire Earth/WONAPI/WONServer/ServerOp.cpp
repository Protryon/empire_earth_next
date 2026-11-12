// ServerOp: base of the server operations (per-step connect/send/receive time limits)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONPing.h" // the MultiPingOp helpers over RustServerOp
namespace WONAPI {
// FUNCTION: WON 0x006acc10
// Creates a server op with 60-second connect, send and receive limits and an 8-byte key size
ServerOp::ServerOp()
{
    this->mMaxConnectTime=60000;
    this->mMaxSendTime=60000;
    this->mMaxRecvTime=60000;
    this->mClientKeySize=8;
    this->mUseClientKey=false;
}
// FUNCTION: WON 0x006ace20
// Sets the connect, send and receive time limits
void ServerOp::SetMaxTimes(int inConnect,int inSend,int inRecv)
{
    this->mMaxConnectTime=inConnect;
    this->mMaxSendTime=inSend;
    this->mMaxRecvTime=inRecv;
}
// FUNCTION: WON 0x006ace50
// Returns the connect time limit, capped by the time left when blocking
DWORD ServerOp::ConnectTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxConnectTime;
    return ee_won_server_step_time(this->mMaxConnectTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006ace80
// Returns the send time limit, capped by the time left when blocking
DWORD ServerOp::SendTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxSendTime;
    return ee_won_server_step_time(this->mMaxSendTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006aceb0
// Returns the receive time limit, capped by the time left when blocking
DWORD ServerOp::RecvTime()
{
    // Rust's (ee_won server.rs); the time left is read only when blocking, as before
    if(this->GetMode()==OP_MODE_ASYNC) return this->mMaxRecvTime;
    return ee_won_server_step_time(this->mMaxRecvTime,false,this->TimeLeft());
}
// FUNCTION: WON 0x006acee0
// Copies the time limits of another server op
void ServerOp::CopyMaxTimes(ServerOp* inSource)
{
    this->SetMaxTimes(inSource->mMaxConnectTime,inSource->mMaxSendTime,inSource->mMaxRecvTime);
}
}

// No original address identified: WON SDK methods
namespace WONAPI {
void ServerOp::SendMsgAsync(ByteBufferPtr theMsg, int theTrackId)
{
	SmartPtr<SendMsgOp> anOp = new SendMsgOp(theMsg, this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}

void ServerOp::RecvMsgAsync(int theTrackId)
{
	SmartPtr<RecvMsgOp> anOp = new RecvMsgOp(this->mSocket);
	this->Track(anOp,theTrackId);
	this->mSocket->QueueOp(anOp,-1);
}
}

// ---- RustServerOp: an op of the Rust op base (ee_won hostop.rs), the Rust op's calls answered on the C++ objects

#include <time.h>
#include <stdlib.h>
#include <vector>
#include <sys/stat.h>
#include "platform/PlatFile.h"
namespace WONAPI {
namespace {
// the param of a completion this op made: the op (a reference, as the original's completions held theirs) and the id
struct HostParam : public RefCount {
    SmartPtr<RustServerOp> mOp;
    int mId;
    HostParam(RustServerOp* inOp, int inId) : mOp(inOp), mId(inId) {}
};
void HostCompletion(AsyncOpPtr inOp, SmartPtr<RefCount> inParam)
{
    HostParam* aParam = static_cast<HostParam*>(inParam.get());
    aParam->mOp->Completed(inOp.get(), aParam->mId);
}
CompletionBase<AsyncOpPtr>* NewHostCompletion(RustServerOp* inOp, int inId)
{
    return new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(HostCompletion, new HostParam(inOp, inId));
}
RefCount* Obj(void* inObj) { return static_cast<RefCount*>(inObj); }
BlockingSocket* Sock(void* inObj) { return inObj ? static_cast<BlockingSocket*>(Obj(inObj)) : NULL; }
AsyncOp* Op(void* inObj) { return static_cast<AsyncOp*>(Obj(inObj)); }
void* Out(RefCount* inObj) { inObj->AddRef(); return inObj; } // a new object: the Rust op's reference
IPAddr Addr(const EeWonHostCall* c) { IPAddr a; a.SetSixByte(c->addr); return a; }
void GiveAddr(EeWonHostCall* c, const IPAddr& inAddr) { memcpy(c->out_addr, inAddr.GetSixByte(), 6); }
ByteBufferPtr Buf(const EeWonHostCall* c) { return new ByteBuffer(c->data, (unsigned long)c->len, false); }
void Give(EeWonHostCall* c, const void* inData, size_t inLength)
{
    c->out_sink(c->out_context, static_cast<const uint8_t*>(inData), inLength);
}
void Give(EeWonHostCall* c, const ByteBuffer* inBytes)
{
    if (inBytes != NULL) Give(c, inBytes->data(), inBytes->length());
}
SocketOp* NewSocketOp(const EeWonHostCall* c)
{
    AsyncSocket* s = Sock(c->obj[0]);
    switch (c->num[0]) {
    case EE_WON_HOST_OP_CONNECT: return new ConnectOp(Addr(c), s);
    case EE_WON_HOST_OP_SEND_BYTES: return new SendBytesOp(Buf(c), s);
    case EE_WON_HOST_OP_SEND_BYTES_TO: return new SendBytesToOp(Buf(c), Addr(c), s);
    case EE_WON_HOST_OP_SEND_MSG: return new SendMsgOp(Buf(c), s);
    case EE_WON_HOST_OP_RECV_BYTES: return new RecvBytesOp((unsigned long)c->num[1], s);
    case EE_WON_HOST_OP_RECV_MSG: return new RecvMsgOp(s);
    case EE_WON_HOST_OP_RECV_CR_MSG: return new RecvCRMsgOp(s);
    default: return new RecvBytesFromOp((unsigned short)c->num[1], s);
    }
}
}

RustServerOp::RustServerOp(EeWonHostOp* inRustOp) : mRustOp(inRustOp), mFile(NULL)
{
    this->mLengthFieldSize = 4;
    this->mAuthType = AUTH_TYPE_NONE;
    ee_won_hostop_bind(inRustOp, RustServerOp::HostCall, this);
}
RustServerOp::~RustServerOp()
{
    ee_won_hostop_free(this->mRustOp); // its objects released through HostCall
    if (this->mFile != NULL) fclose(this->mFile);
}
void RustServerOp::RunHook() { ee_won_hostop_run(this->mRustOp); }
void RustServerOp::CleanupHook() { ee_won_hostop_cleanup(this->mRustOp); }
bool RustServerOp::CallbackHook(AsyncOp* inOp, int inParam)
{
    return ee_won_hostop_callback(this->mRustOp, inParam, inOp);
}
void RustServerOp::HostCall(void* inOp, EeWonHostCall* c)
{
    RustServerOp* o = static_cast<RustServerOp*>(inOp);
    void* o0 = c->obj[0];
    long long n0 = c->num[0], n1 = c->num[1];
    switch (c->code) {
    case EE_WON_HOST_STATUS: c->out_num = o->GetStatus(); break;
    case EE_WON_HOST_MODE: c->out_num = o->GetMode(); break;
    case EE_WON_HOST_TIME_LEFT: c->out_num = (DWORD)o->TimeLeft(); break;
    case EE_WON_HOST_TICK: c->out_num = (DWORD)WONTickCount(); break;
    case EE_WON_HOST_RAND: c->out_num = rand(); break;
    case EE_WON_HOST_SRAND: srand((unsigned)time(NULL)); break;
    case EE_WON_HOST_LFS: c->out_num = o->mLengthFieldSize; break;
    case EE_WON_HOST_SEND_TIME: c->out_num = o->SendTime(); break;
    case EE_WON_HOST_RECV_TIME: c->out_num = o->RecvTime(); break;
    case EE_WON_HOST_NEW_SOCKET: c->out_obj = Out(new BlockingSocket(n0 ? AsyncSocket::UDP : AsyncSocket::TCP)); break;
    case EE_WON_HOST_NEW_OP: c->out_obj = Out(NewSocketOp(c)); break;
    case EE_WON_HOST_NEW_TIMER: {
        AsyncOp* aTimer = new AsyncOp;
        aTimer->SetCompletion(NewHostCompletion(o, (int)n0));
        c->out_obj = Out(aTimer);
        break;
    }
    case EE_WON_HOST_NEW_PEER_AUTH: {
        PeerAuthOp* anOp = new PeerAuthOp(Addr(c), o->mAuthContext, o->mAuthType, Sock(o0), n0 != 0);
        anOp->CopyMaxTimes(o);
        c->out_obj = Out(anOp);
        break;
    }
    case EE_WON_HOST_OP_STATUS: c->out_num = Op(o0)->GetStatus(); break;
    case EE_WON_HOST_OP_BYTES: {
        AsyncOp* anOp = Op(o0);
        if (RecvBytesFromOp* f = dynamic_cast<RecvBytesFromOp*>(anOp)) Give(c, f->GetBytes().get());
        else if (RecvMsgOp* m = dynamic_cast<RecvMsgOp*>(anOp)) Give(c, m->GetMsg().get());
        else if (RecvCRMsgOp* l = dynamic_cast<RecvCRMsgOp*>(anOp)) Give(c, l->GetMsg().get());
        else if (RecvBytesOp* b = dynamic_cast<RecvBytesOp*>(anOp)) Give(c, b->GetBytes().get());
        break;
    }
    case EE_WON_HOST_OP_ADDR:
        if (RecvBytesFromOp* f = dynamic_cast<RecvBytesFromOp*>(Op(o0))) GiveAddr(c, f->GetAddr());
        break;
    case EE_WON_HOST_OP_SOCKET:
        if (SocketOp* s = dynamic_cast<SocketOp*>(Op(o0))) c->out_obj = s->GetSocket() ? Obj(s->GetSocket()) : NULL;
        break;
    case EE_WON_HOST_PEER_AUTH_KEY: {
        AuthSession* aSession = static_cast<PeerAuthOp*>(Op(o0))->GetSession();
        if (aSession != NULL) Give(c, aSession->GetKey().GetKey(), aSession->GetKey().GetKeyLen());
        break;
    }
    case EE_WON_HOST_OP_KILLED: c->out_num = Op(o0)->IsKilled() ? 1 : 0; break;
    case EE_WON_HOST_RESOLVE: {
        IPAddr anAddr;
        c->out_num = anAddr.SetWithDefaultPort(std::string(reinterpret_cast<const char*>(c->data), c->len),
                                               (unsigned short)n0) ? 1 : 0;
        GiveAddr(c, anAddr);
        break;
    }
    case EE_WON_HOST_RECV_PROGRESS:
        static_cast<RecvBytesOp*>(Op(o0))->SetProgressCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(Obj(c->obj[1])),
                                                                 (long)n0);
        break;
    case EE_WON_HOST_RECV_FILE: {
        RecvBytesOp* anOp = static_cast<RecvBytesOp*>(Op(o0));
        anOp->SetFilePath(std::string(reinterpret_cast<const char*>(c->data), c->len));
        anOp->SetAppendFile(n0 != 0);
        break;
    }
    case EE_WON_HOST_CONNECT: c->out_num = Sock(o0)->Connect(Addr(c), (unsigned long)n0); break;
    case EE_WON_HOST_FILE_SIZE: {
        std::string aPath(reinterpret_cast<const char*>(c->data), c->len);
        struct stat anInfo;
        c->out_num = stat(aPath.c_str(), &anInfo) == 0 ? (long long)anInfo.st_size : -1;
        break;
    }
    case EE_WON_HOST_SOCKET_BIND: c->out_num = Sock(o0)->Fn2B0D90((unsigned short)n0, n1 != 0); break;
    case EE_WON_HOST_SOCKET_SET_LFS: Sock(o0)->SetLengthFieldSize((unsigned char)n0); break;
    case EE_WON_HOST_SEND_BYTES_TO: c->out_num = Sock(o0)->SendBytesTo(Buf(c), Addr(c), (unsigned long)n0); break;
    case EE_WON_HOST_RECV_BYTES_FROM: {
        ByteBufferPtr aBytes;
        IPAddr anAddr;
        c->out_num = Sock(o0)->RecvBytesFrom(aBytes, (unsigned short)n0, anAddr, (unsigned long)n1);
        Give(c, aBytes.get());
        GiveAddr(c, anAddr);
        break;
    }
    case EE_WON_HOST_SEND_BYTES: c->out_num = Sock(o0)->SendBytes(Buf(c), (unsigned long)n0); break;
    case EE_WON_HOST_SEND_MSG: c->out_num = Sock(o0)->SendMsg(Buf(c), (unsigned long)n0); break;
    case EE_WON_HOST_RECV_BYTES: {
        ByteBufferPtr aBytes;
        c->out_num = Sock(o0)->RecvBytes(aBytes, (DWORD)n0, (DWORD)n1);
        Give(c, aBytes.get());
        break;
    }
    case EE_WON_HOST_RECV_MSG: {
        ByteBufferPtr aBytes;
        c->out_num = Sock(o0)->RecvMsg(aBytes, (unsigned long)n0);
        Give(c, aBytes.get());
        break;
    }
    case EE_WON_HOST_RUN_BLOCK: c->out_num = Op(o0)->RunBlock((unsigned long)n0) ? 1 : 0; break;
    case EE_WON_HOST_FILE_OPEN:
        if (o->mFile != NULL) fclose(o->mFile);
        o->mFile = PlatFOpen(std::string(reinterpret_cast<const char*>(c->data), c->len).c_str(), "rb");
        c->out_num = o->mFile != NULL;
        break;
    case EE_WON_HOST_FILE_READ: {
        std::vector<char> aBuf((size_t)n0 + 1);
        size_t aLen = o->mFile != NULL ? fread(&aBuf[0], 1, (size_t)n0, o->mFile) : 0;
        Give(c, &aBuf[0], aLen);
        break;
    }
    case EE_WON_HOST_FILE_CLOSE:
        if (o->mFile != NULL) fclose(o->mFile);
        o->mFile = NULL;
        break;
    case EE_WON_HOST_FINISH: o->Finish((WONStatus)n0); break;
    case EE_WON_HOST_KILL: (o0 != NULL ? Op(o0) : o)->Kill(n0 != 0); break;
    case EE_WON_HOST_TRACK: o->Track(Op(o0), (int)n0); break;
    case EE_WON_HOST_QUEUE: Sock(o0)->QueueOp(static_cast<SocketOp*>(Op(c->obj[1])), (long)n0); break;
    case EE_WON_HOST_RUN_ASYNC: Op(o0)->RunAsync((long)n0); break;
    case EE_WON_HOST_CLOSE: Sock(o0)->Close(); break;
    case EE_WON_HOST_SET_REPEAT:
        Sock(o0)->SetRepeatCompletion(NewHostCompletion(o, (int)n0));
        Sock(o0)->SetRepeatOp(static_cast<SocketOp*>(Op(c->obj[1])));
        break;
    case EE_WON_HOST_COMPLETE: static_cast<CompletionBase<AsyncOpPtr>*>(Obj(o0))->Complete(o); break;
    case EE_WON_HOST_BASE_CLEANUP: o->AsyncOpWithTracker::CleanupHook(); break;
    case EE_WON_HOST_RELEASE: Obj(o0)->Release(); break;
    case EE_WON_HOST_ADD_REF: Obj(o0)->AddRef(); break;
    default: break;
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accd0
// (public: virtual void * __thiscall WONAPI::ServerOp::`scalar deleting destructor'(unsigned int)    [??_GServerOp@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006accf0
// (public: virtual __thiscall WONAPI::ServerOp::~ServerOp(void)    [??1ServerOp@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ae9f0
// (public: __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,clas)
