// SocketOp run step: start, then hand to the socket thread or finish in place
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/wonapi/WONStatus.h"
#include "platform/PlatFile.h"
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif
namespace WONAPI {
// The op's base state and RunHook are a Rust object (rust/crates/ee_won/src/sockop.rs EeWonSocketOp); these are its
// calls back: the virtual Start and Continue, Finish, the run mode, the socket thread's add, the socket's references
// and its Rust object
static long HookStart(void* inOp) { return static_cast<SocketOp*>(inOp)->Start(); }
static long HookContinue(void* inOp) { return static_cast<SocketOp*>(inOp)->Continue(); }
static void HookFinish(void* inOp, long inStatus) { static_cast<SocketOp*>(inOp)->Finish(inStatus); }
static int32_t HookMode(void* inOp) { return static_cast<int32_t>(static_cast<SocketOp*>(inOp)->GetMode()); }
static void HookSocketAddRef(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->AddRef(); }
static void HookSocketRelease(void* inSocket) { static_cast<AsyncSocket*>(inSocket)->Release(); }
static EeWonAsyncSocket* HookSocketObject(void* inSocket) { return static_cast<AsyncSocket*>(inSocket)->GetSocketObject(); }
void SocketOp::HookAddToSocketThread(void* inOp) { WONAPICore::AddToSocketThread(static_cast<SocketOp*>(inOp)); }

// The ops' C++ side (rust/crates/ee_won/src/sockops.rs): the objects they hold are RefCount*s (byte buffers, the
// progress completion, the accepted socket), each with a reference
static void HookRefAdd(void* inObject) { static_cast<RefCount*>(inObject)->AddRef(); }
static void HookRefRelease(void* inObject) { static_cast<RefCount*>(inObject)->Release(); }
static const EeWonByteBuffer* HookBytesOf(void* inBuffer)
{ return static_cast<const ByteBuffer*>(static_cast<RefCount*>(inBuffer))->mBytes; }
static void* HookBytesAdopt(EeWonByteBuffer* inBytes)
{
    RefCount* aBuffer = new ByteBuffer(inBytes);
    aBuffer->AddRef();
    return aBuffer;
}
// the socket's transform chain on the message in *ioMessage (a reference held; the result's replaces it)
static long HookTransform(void* inSocket, void** ioMessage, bool inSend)
{
    ByteBufferPtr aMessage = static_cast<const ByteBuffer*>(static_cast<RefCount*>(*ioMessage));
    AsyncSocket* aSocket = static_cast<AsyncSocket*>(inSocket);
    WONStatus aStatus = inSend ? aSocket->TransformSendMsg(aMessage) : aSocket->TransformRecvMsg(aMessage);
    RefCount* aResult = const_cast<ByteBuffer*>(aMessage.get());
    if (aResult != NULL)
        aResult->AddRef();
    if (*ioMessage != NULL)
        static_cast<RefCount*>(*ioMessage)->Release();
    *ioMessage = aResult;
    return aStatus;
}
static void* HookFileOpen(const uint8_t* inPath, size_t inLength, bool inBinary, bool inAppend)
{
    std::string aPath(reinterpret_cast<const char*>(inPath), inLength);
    if (inBinary)
        return PlatFOpen(aPath.c_str(), inAppend ? "a+b" : "wb");
    return PlatFOpen(aPath.c_str(), inAppend ? "a+" : "w");
}
static size_t HookFileWrite(void* inFile, const uint8_t* inData, size_t inLength)
{ return fwrite(inData, 1, inLength, static_cast<FILE*>(inFile)); }
static int32_t HookFileClose(void* inFile) { return fclose(static_cast<FILE*>(inFile)); }
static void HookFileSetTime(const uint8_t* inPath, size_t inLength, int64_t inTime)
{
    std::string aPath(reinterpret_cast<const char*>(inPath), inLength);
#ifdef _WIN32
    struct _utimbuf aTime;
#else
    struct utimbuf aTime;   // POSIX's utime: the same fields
#endif
    aTime.actime = static_cast<time_t>(inTime);
    aTime.modtime = static_cast<time_t>(inTime);
#ifdef _WIN32
    _utime(aPath.c_str(), &aTime);
#else
    utime(aPath.c_str(), &aTime);
#endif
}
// a progress chunk: a RecvChunkOp of the receiving op with its completion, run in the op's mode
static void HookProgress(void* inOp, void* inCompletion)
{
    RecvBytesOp* anOp = static_cast<RecvBytesOp*>(static_cast<SocketOp*>(inOp));
    SmartPtr<RecvChunkOp> aChunk = new RecvChunkOp(anOp);
    aChunk->SetCompletion(static_cast<CompletionBase<AsyncOpPtr>*>(static_cast<RefCount*>(inCompletion)));
    long aMode = anOp->GetMode();
    aChunk->Run(aMode, 0);
}
// AsyncSocket::Accept (the virtual Duplicate): the accepted socket in *outSocket with a reference, on success only
static long HookAccept(void* inSocket, void** outSocket)
{
    SmartPtr<AsyncSocket> anAccepted;
    WONStatus aStatus = static_cast<AsyncSocket*>(inSocket)->Accept(anAccepted);
    if (aStatus == 0) {
        RefCount* aResult = anAccepted.get();
        if (aResult != NULL)
            aResult->AddRef();
        *outSocket = aResult;
    }
    return aStatus;
}

const EeWonSocketOpBaseHooks SocketOp::kHooks={HookStart,HookContinue,HookFinish,HookMode,
                                               &SocketOp::HookAddToSocketThread,HookSocketAddRef,
                                               HookSocketRelease,HookSocketObject,HookRefAdd,HookRefRelease,
                                               HookBytesOf,HookBytesAdopt,HookTransform,HookFileOpen,HookFileWrite,
                                               HookFileClose,HookFileSetTime,HookProgress,HookAccept};

// Creates an op on the socket (a reference held): no socket thread, no waits, the event indices -1
SocketOp::SocketOp(AsyncSocket* inSocket)
{
    this->mSockOp=ee_won_socketop_new(static_cast<void*>(this),&SocketOp::kHooks);
    this->SetSocket(inSocket);
    this->SetRunAsyncImmediately(SocketOp::mRunAsyncImmediatelyDef);
}
// Destroys the op: the socket's reference goes with the Rust object
SocketOp::~SocketOp() { ee_won_socketop_free(this->mSockOp); }

// FUNCTION: WON 0x006b6070
// Starts the op, first finishing it with InvalidSocket when it has no socket (and starting anyway); a pending
// start is queued on the socket thread (async) or continued and finished (blocking), any other result finishes it
void SocketOp::RunHook() { ee_won_socketop_run_hook(this->mSockOp); }
}

// SDK SocketOp static definition; in the original a zero byte at 565f08
namespace WONAPI {
bool SocketOp::mRunAsyncImmediatelyDef;
}
