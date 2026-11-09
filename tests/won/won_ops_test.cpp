// WON's socket ops and async machinery over 127.0.0.1: BlockingSocket's blocking ops (Connect, SendMsg / RecvMsg
// with 1-, 2- and 4-byte length fields and the 2-byte 0xffff escape, bad lengths, RecvCRMsg, RecvBytes, SendBytes of
// a large buffer to a reader thread, SendBytesTo / RecvBytesFrom, a receive that times out), then the same ops run
// asynchronously through WONAPICore (the op queue, the socket thread, the timer thread: RunAsTimer, a timeout, Kill)
// and WONStatusToString. Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <thread>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatSocket.h"
#include "platform/PlatTime.h"

using namespace WONAPI;

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define CHECK_STATUS(s, want) do { WONStatus s_ = (s); if (s_ != (want)) { sFailures++; \
    printf("FAIL line %d: %s -> %ld (%s), want %s\n", __LINE__, #s, (long)s_, WONStatusToString(s_), #want); } } while (0)

static ByteBufferPtr Bytes(const void* inData, unsigned long inLength)
{
    return new ByteBuffer(inData, inLength, false);
}

static bool Same(const ByteBuffer* inBuffer, const void* inData, unsigned long inLength)
{
    return inBuffer != NULL && inBuffer->length() == inLength && memcmp(inBuffer->data(), inData, inLength) == 0;
}

// a connected pair of blocking stream sockets: the client and the accepted socket
struct Pair {
    SmartPtr<BlockingSocket> mListener, mClient;
    SmartPtr<AsyncSocket> mServer;
    bool Make()
    {
        this->mListener = new BlockingSocket(0);
        if (this->mListener->Bind(IPAddr("127.0.0.1", 0), true) != 0 || this->mListener->Listen(5) != 0)
            return false;
        this->mClient = new BlockingSocket(0);
        if (this->mClient->Connect(IPAddr("127.0.0.1", this->mListener->GetLocalPort()), 2000) != WS_Success)
            return false;
        this->mListener->WaitForAccept(2000);
        return this->mListener->Accept(this->mServer) == 0 && this->mServer.get() != NULL;
    }
    BlockingSocket* Server() { return static_cast<BlockingSocket*>(this->mServer.get()); }
};

// counts completions and keeps the last op's status
class Counter : public CompletionBase<AsyncOpPtr> {
public:
    Counter() : mCount(0), mLast(WS_None) {}
    virtual void Complete(AsyncOpPtr inOp) { this->mCount++; this->mLast = inOp->GetStatus(); this->mOp = inOp; }
    int mCount;
    WONStatus mLast;
    AsyncOpPtr mOp;
};

static void PumpUntil(WONAPICore& inCore, Counter* inDone, int inCount, unsigned long inMaxMs)
{
    uint64_t theEnd = PlatTimeMilliseconds64() + inMaxMs;
    while (inDone->mCount < inCount && PlatTimeMilliseconds64() < theEnd)
        inCore.Pump(10);
}

static void BlockingOps()
{
    Pair aPair;
    CHECK(aPair.Make());
    if (aPair.mServer.get() == NULL)
        return;
    BlockingSocket* aServer = aPair.Server();
    BlockingSocket* aClient = aPair.mClient.get();
    CHECK(aServer->IsConnected() && aServer->GetType() == AsyncSocket::TCP);

    // a 4-byte length field (the default) counts itself
    const unsigned char aMsg4[] = {9, 0, 0, 0, 'h', 'e', 'l', 'l', 'o'};
    CHECK_STATUS(aClient->SendMsg(Bytes(aMsg4, sizeof(aMsg4)), 1000), WS_Success);
    ByteBufferPtr aGot;
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_Success);
    CHECK(Same(aGot, "hello", 5));

    // 2 bytes, and the 2-byte escape 0xffff followed by a 4-byte length
    aServer->SetLengthFieldSize(2);
    const unsigned char aMsg2[] = {5, 0, 'a', 'b', 'c'};
    CHECK_STATUS(aClient->SendMsg(Bytes(aMsg2, sizeof(aMsg2)), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_Success);
    CHECK(Same(aGot, "abc", 3));
    const unsigned char aMsgEsc[] = {0xff, 0xff, 7, 0, 0, 0, 'x', 'y', 'z'};
    CHECK_STATUS(aClient->SendMsg(Bytes(aMsgEsc, sizeof(aMsgEsc)), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_Success);
    CHECK(Same(aGot, "xyz", 3));

    // 1 byte
    aServer->SetLengthFieldSize(1);
    const unsigned char aMsg1[] = {3, 'q', 'r'};
    CHECK_STATUS(aClient->SendMsg(Bytes(aMsg1, sizeof(aMsg1)), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_Success);
    CHECK(Same(aGot, "qr", 2));

    // a length not above the field's size, a length of 256000, a field size of 3
    aServer->SetLengthFieldSize(4);
    const unsigned char aBad[] = {4, 0, 0, 0};
    CHECK_STATUS(aClient->SendBytes(Bytes(aBad, sizeof(aBad)), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_RecvMsg_InvalidMessageLength);
    const unsigned char aHuge[] = {0x00, 0xe8, 0x03, 0};   // 256000
    CHECK_STATUS(aClient->SendBytes(Bytes(aHuge, sizeof(aHuge)), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_RecvMsg_InvalidMessageLength);
    aServer->SetLengthFieldSize(3);
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_RecvMsg_InvalidLengthFieldSize);
    aServer->SetLengthFieldSize(4);

    // nothing queued: the receive times out
    uint64_t aStart = PlatTimeMilliseconds64();
    CHECK_STATUS(aServer->RecvMsg(aGot, 150), WS_TimedOut);
    uint64_t aWaited = PlatTimeMilliseconds64() - aStart;
    CHECK(aWaited >= 100 && aWaited < 2000);

    // a CR-LF line (the CR becomes a NUL, the LF is dropped); a lone LF does not end it
    const char aLines[] = "a\nb\r\nnext";
    CHECK_STATUS(aClient->SendBytes(Bytes(aLines, sizeof(aLines) - 1), 1000), WS_Success);
    CHECK_STATUS(aServer->RecvCRMsg(aGot, 1000), WS_Success);
    CHECK(Same(aGot, "a\nb\0", 4));
    CHECK_STATUS(aServer->RecvBytes(aGot, 4, 1000), WS_Success);
    CHECK(Same(aGot, "next", 4));

    // a large send while another thread reads it
    const unsigned long aBig = 3 * 1024 * 1024 + 17;
    char* aData = new char[aBig];
    for (unsigned long i = 0; i < aBig; i++)
        aData[i] = (char)(i * 7 + (i >> 11));
    ByteBufferPtr aReceived;
    WONStatus aRecvStatus = WS_None;
    std::thread aReader([&] { aRecvStatus = aServer->RecvBytes(aReceived, aBig, 10000); });
    CHECK_STATUS(aClient->SendBytes(Bytes(aData, aBig), 10000), WS_Success);
    aReader.join();
    CHECK_STATUS(aRecvStatus, WS_Success);
    CHECK(Same(aReceived, aData, aBig));
    delete[] aData;

    // the peer closes: a receive reports Shutdown; RecvBytes(0) reads until then
    const char aTail[] = "tail";
    CHECK_STATUS(aClient->SendBytes(Bytes(aTail, 4), 1000), WS_Success);
    aClient->Close();
    CHECK_STATUS(aServer->RecvBytes(aGot, 0, 1000), WS_Success);
    CHECK(Same(aGot, "tail", 4));
    CHECK_STATUS(aServer->RecvMsg(aGot, 1000), WS_AsyncSocket_Shutdown);

    // ops on a closed socket
    CHECK_STATUS(aClient->SendMsg(Bytes(aMsg4, sizeof(aMsg4)), 100), WS_AsyncSocket_InvalidSocket);

    // datagrams
    SmartPtr<BlockingSocket> aUdpA = new BlockingSocket(1), aUdpB = new BlockingSocket(1);
    CHECK(aUdpA->Bind(IPAddr("127.0.0.1", 0), false) == 0 && aUdpB->Bind(IPAddr("127.0.0.1", 0), false) == 0);
    CHECK_STATUS(aUdpA->SendBytesTo(Bytes("ping", 4), IPAddr("127.0.0.1", aUdpB->GetLocalPort()), 1000), WS_Success);
    IPAddr aFrom;
    CHECK_STATUS(aUdpB->RecvBytesFrom(aGot, 512, aFrom, 1000), WS_Success);
    CHECK(Same(aGot, "ping", 4));
    CHECK(aFrom.GetPort() == aUdpA->GetLocalPort() && aFrom.GetHost() == 0x7f000001);
    CHECK_STATUS(aUdpB->RecvBytesFrom(aGot, 512, aFrom, 100), WS_TimedOut);
    // a stream op on a datagram socket and the reverse
    CHECK_STATUS(aUdpA->Listen(1), WS_AsyncSocket_DatagramNotAllowed);
    CHECK_STATUS(aServer->SendTo("x", 1, aFrom), WS_AsyncSocket_StreamNotAllowed);
    aUdpA->Close();
    aUdpB->Close();
    aServer->Close();
    unsigned short aClosedPort = aPair.mListener->GetLocalPort();
    aPair.mListener->Close();

    // connecting to a port nobody listens on (the listener's, closed), and to an invalid address
    SmartPtr<BlockingSocket> aRefused = new BlockingSocket(0);
    CHECK_STATUS(aRefused->Connect(IPAddr("127.0.0.1", aClosedPort), 3000), PLAT_SOCK_ECONNREFUSED);
    CHECK_STATUS(aRefused->Connect(IPAddr(), 100), WS_InvalidAddress);
}

static void AsyncOps(WONAPICore& inCore)
{
    Pair aPair;
    CHECK(aPair.Make());
    if (aPair.mServer.get() == NULL)
        return;

    // a receive started before the data arrives: the socket thread finishes it, the pump completes it
    SmartPtr<Counter> aDone = new Counter;
    SmartPtr<RecvMsgOp> aRecv = new RecvMsgOp(aPair.mServer.get());
    aRecv->SetCompletion(aDone);
    aRecv->RunAsync(2000);
    CHECK_STATUS(aRecv->GetStatus(), WS_Pending);
    inCore.Pump(30);
    CHECK(aDone->mCount == 0);
    const unsigned char aMsg[] = {8, 0, 0, 0, 'a', 's', 'y', 'n'};
    SmartPtr<SendMsgOp> aSend = new SendMsgOp(Bytes(aMsg, sizeof(aMsg)), aPair.mClient.get());
    SmartPtr<Counter> aSent = new Counter;
    aSend->SetCompletion(aSent);
    aSend->RunAsync(2000);
    PumpUntil(inCore, aDone, 1, 3000);
    PumpUntil(inCore, aSent, 1, 3000);
    CHECK(aDone->mCount == 1 && aSent->mCount == 1);
    CHECK_STATUS(aDone->mLast, WS_Success);
    CHECK_STATUS(aSent->mLast, WS_Success);
    CHECK(Same(aRecv->GetMsg(), "asyn", 4));

    // an async receive that times out (the timer thread finishes it)
    SmartPtr<Counter> aTimedOut = new Counter;
    SmartPtr<RecvMsgOp> aLate = new RecvMsgOp(aPair.mServer.get());
    aLate->SetCompletion(aTimedOut);
    aLate->RunAsync(100);
    PumpUntil(inCore, aTimedOut, 1, 3000);
    CHECK(aTimedOut->mCount == 1);
    CHECK_STATUS(aTimedOut->mLast, WS_TimedOut);

    // a killed op completes once with WS_Killed; running it again while pending says so
    SmartPtr<Counter> aKilled = new Counter;
    SmartPtr<RecvMsgOp> aKill = new RecvMsgOp(aPair.mServer.get());
    aKill->SetCompletion(aKilled);
    aKill->RunAsync(5000);
    inCore.Pump(20);
    CHECK_STATUS(aKill->Run(OP_MODE_ASYNC, 5000), WS_Pending);
    aKill->Kill(true);
    PumpUntil(inCore, aKilled, 1, 3000);
    inCore.Pump(20);
    CHECK(aKilled->mCount == 1);
    CHECK_STATUS(aKilled->mLast, WS_Killed);

    // a pure timer
    SmartPtr<Counter> aTimer = new Counter;
    AsyncOpPtr aTimerOp = new AsyncOp;
    aTimerOp->SetCompletion(aTimer);
    uint64_t aStart = PlatTimeMilliseconds64();
    aTimerOp->RunAsTimer(60);
    PumpUntil(inCore, aTimer, 1, 3000);
    uint64_t aWaited = PlatTimeMilliseconds64() - aStart;
    CHECK(aTimer->mCount == 1 && aWaited >= 50);
    CHECK_STATUS(aTimer->mLast, WS_TimedOut);

    // a blocking op's time left counts down; an async op has none
    AsyncOpPtr anIdle = new AsyncOp;
    CHECK(anIdle->TimeLeft() == 0);

    aPair.mClient->Close();
    aPair.mServer->Close();
    aPair.mListener->Close();
}

int main(int argc, char** argv)
{
    CHECK(AsyncSocket::StartWinsock() == 0);

    CHECK(strcmp(WONStatusToString(WS_TimedOut), "WS_TimedOut") == 0);
    CHECK(strcmp(WONStatusToString(-2950), "DBProxyServ_GameSpecificError-2950") == 0);
    CHECK(strcmp(WONStatusToString(123456), "123456") == 0);
    CHECK(strcmp(WONStatusToString(PLAT_SOCK_ECONNREFUSED), "WS_WSAECONNREFUSED") == 0);

    BlockingOps();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(".");   // _wonapiversion.txt
        aCore.Startup();
        AsyncOps(aCore);
        aCore.Shutdown();
    }

    CHECK(AsyncSocket::StopWinsock() == 0);
    printf("%s (%d failed)\n", sFailures ? "FAIL" : "PASS", sFailures);
    return sFailures;
}
