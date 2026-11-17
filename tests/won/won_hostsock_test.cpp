// RustServerOp's socket and op host calls (ServerOp.cpp's HostCall: the sockets' bind, listen, port, length field,
// connected flag, status, close and kill; the blocking connect, sends and receives; the socket ops' bytes, address,
// socket and kind, their receive file, skip and progress settings; the ops' status, killed flag, kill and runs; the
// queue), over real sockets on 127.0.0.1, one line per call, compared with rust/crates/ee_won_net/src/golden_hostsock.txt
// (recorded over ServerOp.cpp's HostCall before these calls moved into the Rust op base, net round 23). Every call
// goes through an op's own call path (ee_won_hostop_call), so the same lines are the C++ side's answers before the
// move and the Rust op's after it. Waits are on events (a blocking call returns when its data arrives; an async op
// is pumped until it is no longer pending), never on elapsed time; the only time-outs taken are of calls that can
// get no data. Objects are named by the answer that returned them. `--print` prints the lines instead, `--trace`
// each line as it is logged. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatFile.h"
#include "rust/won_net/hostop.h"

using namespace WONAPI;
using std::string;

static std::vector<string> sLines;
static bool sTrace;
static void Log(const string& inLine)
{
    sLines.push_back(inLine);
    if (sTrace) fprintf(stderr, "%s\n", inLine.c_str());
}
static string N(long long inValue) { return std::to_string(inValue); }
static string Hex(const string& inBytes, size_t inMax = 48)
{
    static const char kDigits[] = "0123456789abcdef";
    string s = N((long long)inBytes.size()) + ":";
    for (size_t i = 0; i < inBytes.size() && i < inMax; i++) {
        unsigned char c = static_cast<unsigned char>(inBytes[i]);
        s += kDigits[c >> 4];
        s += kDigits[c & 15];
    }
    return s;
}

// ---- one call through the op's own path

struct Answer {
    bool mHasBytes = false;
    string mBytes;
    EeWonHostCall mCall;
};
static void Sink(void* inContext, const uint8_t* inData, size_t inLength)
{
    Answer* a = static_cast<Answer*>(inContext);
    a->mHasBytes = true;
    a->mBytes.assign(reinterpret_cast<const char*>(inData), inLength);
}
static RustServerOp* sOp;
static Answer Call(int inCode, void* inObj0 = NULL, void* inObj1 = NULL, long long inNum0 = 0, long long inNum1 = 0,
                   const string& inData = string(), const IPAddr* inAddr = NULL)
{
    Answer a;
    memset(&a.mCall, 0, sizeof(a.mCall));
    a.mCall.code = inCode;
    a.mCall.obj[0] = inObj0;
    a.mCall.obj[1] = inObj1;
    a.mCall.num[0] = inNum0;
    a.mCall.num[1] = inNum1;
    a.mCall.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    a.mCall.len = inData.size();
    if (inAddr != NULL) memcpy(a.mCall.addr, inAddr->GetSixByte(), 6);
    a.mCall.out_sink = Sink;
    a.mCall.out_context = &a;
    ee_won_hostop_call(sOp->GetRustOp(), &a.mCall);
    return a;
}
static long long Num(int inCode, void* inObj0 = NULL, long long inNum0 = 0, long long inNum1 = 0)
{
    return Call(inCode, inObj0, NULL, inNum0, inNum1).mCall.out_num;
}
static string Bytes(const Answer& a) { return a.mHasBytes ? Hex(a.mBytes) : string("none"); }
// an answered address: whether it is 127.0.0.1 and whether its port is the given one (ports differ per run)
static string AddrOf(const Answer& a, unsigned short inPort)
{
    IPAddr anAddr;
    anAddr.SetSixByte(a.mCall.out_addr);
    return string(anAddr.GetHost() == 0x7f000001 ? "loopback" : (anAddr.GetHost() == 0 ? "zero" : "other"))
        + (anAddr.GetPort() == inPort ? " port" : (anAddr.GetPort() == 0 ? " no port" : " other port"));
}

// the objects the calls answered, named in the order they were made (never by address: a freed object's address
// comes back for a later one)
static std::vector<std::pair<void*, string> > sObjects;
static string Fresh(void* inObj)
{
    if (inObj == NULL) return "null";
    for (size_t i = 0; i < sObjects.size(); i++)
        if (sObjects[i].first == inObj) sObjects[i].first = NULL;
    string aName = "o" + N((long long)sObjects.size());
    sObjects.push_back(std::make_pair(inObj, aName));
    return aName;
}
static string Name(void* inObj)
{
    if (inObj == NULL) return "null";
    for (size_t i = 0; i < sObjects.size(); i++)
        if (sObjects[i].first == inObj) return sObjects[i].second;
    return "unknown";
}
static void* NewSocket(int inUdp)
{
    void* s = Call(EE_WON_HOST_NEW_SOCKET, NULL, NULL, inUdp).mCall.out_obj;
    Log("new socket " + N(inUdp) + " " + Fresh(s));
    return s;
}
static void* NewOp(long long inKind, void* inSocket, long long inLength = 0, const string& inData = string(),
                   const IPAddr* inAddr = NULL)
{
    void* o = Call(EE_WON_HOST_NEW_OP, inSocket, NULL, inKind, inLength, inData, inAddr).mCall.out_obj;
    Log("new op " + N(inKind) + " " + Fresh(o));
    return o;
}
static WONAPICore* sCore;
// pumps until the op is no longer pending (an event: its data, its completion), at most about ten seconds
static void PumpUntilDone(void* inOp)
{
    for (int i = 0; i < 1000 && Num(EE_WON_HOST_OP_STATUS, inOp) == WS_Pending; i++) sCore->Pump(10);
}
// a message with the socket's 4-byte length field in front (the length with the field)
static string Framed(const string& inMsg)
{
    unsigned int aLength = (unsigned int)inMsg.size() + 4;
    string aMsg;
    for (int i = 0; i < 4; i++) aMsg += (char)((aLength >> (8 * i)) & 0xff);
    return aMsg + inMsg;
}

// the progress completion's calls: the bytes the receiving op had read at each
static std::vector<unsigned long> sProgress;
static void ProgressCallback(AsyncOpPtr inOp, SmartPtr<RefCount>)
{
    RecvChunkOp* aChunk = dynamic_cast<RecvChunkOp*>(inOp.get());
    sProgress.push_back(aChunk != NULL && aChunk->GetRecvOp() != NULL ? aChunk->GetRecvOp()->GetBytesRead() : 99999);
}

int main(int argc, char** argv)
{
    sTrace = argc > 1 && strcmp(argv[argc - 1], "--trace") == 0;
    string aExe = argv[0];
    size_t aSlash = aExe.find_last_of("/\\");
    string aDir = (aSlash == string::npos ? string(".") : aExe.substr(0, aSlash)) + "/hostsock_files/";
    PlatCreateDirectory(aDir.c_str());
    string aFile = aDir + "received.bin";
    remove(aFile.c_str());
    AsyncSocket::StartWinsock();
    {
        WONAPICoreEx aCore;
        aCore.SetDefaultFileDirectory(aDir.c_str());
        aCore.Startup();
        sCore = &aCore;
        SmartPtr<RustServerOp> anOp = RustServerOp::NewHttpGet("127.0.0.1", "/hostsock");
        sOp = anOp.get();

        // a listener: bind (the local host, any port), its port, listen, the length field
        void* l = NewSocket(0);
        Log("listener connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, l)) + " status "
            + N(Num(EE_WON_HOST_SOCKET_STATUS, l)) + " port " + N(Num(EE_WON_HOST_SOCKET_PORT, l)));
        Log("bind " + N(Num(EE_WON_HOST_SOCKET_BIND, l, 0, 1)));
        unsigned short aPort = (unsigned short)Num(EE_WON_HOST_SOCKET_PORT, l);
        Log("bound port nonzero " + N(aPort != 0));
        Log("listen " + N(Num(EE_WON_HOST_SOCKET_LISTEN, l)));
        Log("lfs " + N(Num(EE_WON_HOST_SOCKET_LFS, l)));
        Call(EE_WON_HOST_SOCKET_SET_LFS, l, NULL, 2);
        Log("lfs after 2 " + N(Num(EE_WON_HOST_SOCKET_LFS, l)));
        Call(EE_WON_HOST_SOCKET_SET_LFS, l, NULL, 1);
        Log("lfs after 1 " + N(Num(EE_WON_HOST_SOCKET_LFS, l)));
        Call(EE_WON_HOST_SOCKET_SET_LFS, l, NULL, 4);
        Log("lfs after 4 " + N(Num(EE_WON_HOST_SOCKET_LFS, l)));
        Log("listener connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, l)) + " status "
            + N(Num(EE_WON_HOST_SOCKET_STATUS, l)));
        IPAddr aListen("127.0.0.1", aPort);

        // a client: connect through the call (blocking), the server side accepted by an accept op (queued on the
        // listener, pumped until it is done)
        void* c = NewSocket(0);
        Log("client connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, c)));
        void* acc = NULL;
        SmartPtr<AsyncSocket> aServer;
        {
            SmartPtr<AcceptOp> anAccept = new AcceptOp(static_cast<AsyncSocket*>(static_cast<RefCount*>(l)));
            acc = static_cast<RefCount*>(anAccept.get());
            Log("accept op " + Fresh(acc));
            Call(EE_WON_HOST_QUEUE, l, acc, 10000);
            Log("connect " + N(Call(EE_WON_HOST_CONNECT, c, NULL, 5000, 0, string(), &aListen).mCall.out_num));
            PumpUntilDone(acc);
            Log("accept status " + N(Num(EE_WON_HOST_OP_STATUS, acc)) + " killed " + N(Num(EE_WON_HOST_OP_KILLED, acc))
                + " socket " + Name(Call(EE_WON_HOST_OP_SOCKET, acc).mCall.out_obj) + " is recv msg "
                + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, acc)) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, acc)));
            aServer = anAccept->GetAcceptedSocket();
        }
        void* s = static_cast<RefCount*>(aServer.get());
        Log("server accepted " + Fresh(s));
        if (s == NULL) return 1;
        Log("client connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, c)) + " status "
            + N(Num(EE_WON_HOST_SOCKET_STATUS, c)) + " server connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, s)));

        // the blocking calls: messages, bytes, a line, each way
        Log("send msg " + N(Call(EE_WON_HOST_SEND_MSG, c, NULL, 2000, 0, Framed("hello")).mCall.out_num));
        {
            Answer a = Call(EE_WON_HOST_RECV_MSG, s, NULL, 2000);
            Log("recv msg " + N(a.mCall.out_num) + " " + Bytes(a));
        }
        Log("send msg " + N(Call(EE_WON_HOST_SEND_MSG, s, NULL, 2000, 0, Framed(string(300, 'y'))).mCall.out_num));
        {
            Answer a = Call(EE_WON_HOST_RECV_MSG, c, NULL, 2000);
            Log("recv msg " + N(a.mCall.out_num) + " " + Bytes(a));
        }
        Log("send bytes " + N(Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "abcdefghij").mCall.out_num));
        {
            Answer a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 4, 2000);
            Log("recv bytes 4 " + N(a.mCall.out_num) + " " + Bytes(a));
            a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 6, 2000);
            Log("recv bytes 6 " + N(a.mCall.out_num) + " " + Bytes(a));
        }
        Log("send bytes " + N(Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "HTTP/1.0 200 OK\r\nnext").mCall.out_num));
        {
            Answer a = Call(EE_WON_HOST_RECV_CR, s, NULL, 2000);
            Log("recv line " + N(a.mCall.out_num) + " " + Bytes(a));
            a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 4, 2000);
            Log("recv rest " + N(a.mCall.out_num) + " " + Bytes(a));
        }
        // nothing to read: the time-outs (no data can come)
        {
            Answer a = Call(EE_WON_HOST_RECV_MSG, s, NULL, 50);
            Log("recv msg none " + N(a.mCall.out_num) + " " + Bytes(a));
            a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 3, 50);
            Log("recv bytes none " + N(a.mCall.out_num) + " " + Bytes(a));
            a = Call(EE_WON_HOST_RECV_CR, s, NULL, 50);
            Log("recv line none " + N(a.mCall.out_num) + " " + Bytes(a));
        }

        // socket ops made by the calls: run blocking, run, run async, queued, killed
        {
            void* r = NewOp(EE_WON_HOST_OP_RECV_MSG, s);
            Log("recv msg op socket " + Name(Call(EE_WON_HOST_OP_SOCKET, r).mCall.out_obj) + " is recv msg "
                + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, r)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, r)) + " killed "
                + N(Num(EE_WON_HOST_OP_KILLED, r)) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, r)));
            Call(EE_WON_HOST_SEND_MSG, c, NULL, 2000, 0, Framed("by op"));
            long long aRun = Num(EE_WON_HOST_RUN_BLOCK, r, 2000);
            Log("run block " + N(aRun) + " status " + N(Num(EE_WON_HOST_OP_STATUS, r))
                + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, r)));
            Call(EE_WON_HOST_RELEASE, r);

            void* b = NewOp(EE_WON_HOST_OP_RECV_BYTES, s, 5);
            Log("recv bytes op is recv msg " + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, b)) + " bytes "
                + Bytes(Call(EE_WON_HOST_OP_BYTES, b)));
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "12345");
            aRun = Num(EE_WON_HOST_RUN, b, 2, 2000);
            Log("run 2 " + N(aRun) + " status " + N(Num(EE_WON_HOST_OP_STATUS, b))
                + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, b)));
            Call(EE_WON_HOST_RELEASE, b);

            void* m = NewOp(EE_WON_HOST_OP_RECV_MSG, s);
            Call(EE_WON_HOST_RUN_ASYNC, m, NULL, 10000);
            Log("run async status " + N(Num(EE_WON_HOST_OP_STATUS, m)));
            Call(EE_WON_HOST_SEND_MSG, c, NULL, 2000, 0, Framed("async"));
            PumpUntilDone(m);
            Log("run async done " + N(Num(EE_WON_HOST_OP_STATUS, m)) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, m)));
            Call(EE_WON_HOST_RELEASE, m);

            void* q = NewOp(EE_WON_HOST_OP_RECV_MSG, s);
            Call(EE_WON_HOST_QUEUE, s, q, 10000);
            Log("queued status " + N(Num(EE_WON_HOST_OP_STATUS, q)));
            Call(EE_WON_HOST_SEND_MSG, c, NULL, 2000, 0, Framed("queued"));
            PumpUntilDone(q);
            Log("queued done " + N(Num(EE_WON_HOST_OP_STATUS, q)) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, q)));
            Call(EE_WON_HOST_RELEASE, q);

            void* k = NewOp(EE_WON_HOST_OP_RECV_MSG, s);
            Call(EE_WON_HOST_RUN_ASYNC, k, NULL, 10000);
            Call(EE_WON_HOST_KILL, k, NULL, 0);
            Log("killed no finish " + N(Num(EE_WON_HOST_OP_KILLED, k)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, k)));
            Call(EE_WON_HOST_KILL, k, NULL, 1);
            Log("killed finish " + N(Num(EE_WON_HOST_OP_KILLED, k)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, k)));
            Call(EE_WON_HOST_RELEASE, k);

            void* lc = NewOp(EE_WON_HOST_OP_RECV_CR_MSG, s);
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "a line\r\n");
            aRun = Num(EE_WON_HOST_RUN_BLOCK, lc, 2000);
            Log("line op " + N(aRun) + " is recv msg "
                + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, lc)) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, lc)));
            Call(EE_WON_HOST_RELEASE, lc);

            void* sb = NewOp(EE_WON_HOST_OP_SEND_BYTES, c, 0, "xyz");
            aRun = Num(EE_WON_HOST_RUN_BLOCK, sb, 2000);
            Log("send op " + N(aRun) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, sb))
                + " addr " + AddrOf(Call(EE_WON_HOST_OP_ADDR, sb), 0));
            Answer a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 3, 2000);
            Log("sent by op " + N(a.mCall.out_num) + " " + Bytes(a));
            Call(EE_WON_HOST_RELEASE, sb);

            void* sm = NewOp(EE_WON_HOST_OP_SEND_MSG, s, 0, Framed("op msg"));
            aRun = Num(EE_WON_HOST_RUN_BLOCK, sm, 2000);
            Log("send msg op " + N(aRun) + " status " + N(Num(EE_WON_HOST_OP_STATUS, sm)));
            a = Call(EE_WON_HOST_RECV_MSG, c, NULL, 2000);
            Log("sent msg by op " + N(a.mCall.out_num) + " " + Bytes(a));
            Call(EE_WON_HOST_RELEASE, sm);

            // a connect op (no bytes, no address answered), and a socket op without a socket
            void* co = NewOp(EE_WON_HOST_OP_CONNECT, NULL, 0, string(), &aListen);
            Log("connect op socket " + Name(Call(EE_WON_HOST_OP_SOCKET, co).mCall.out_obj) + " bytes "
                + Bytes(Call(EE_WON_HOST_OP_BYTES, co)) + " addr " + AddrOf(Call(EE_WON_HOST_OP_ADDR, co), aPort)
                + " is recv msg " + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, co)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, co)));
            aRun = Num(EE_WON_HOST_RUN_BLOCK, co, 1000);
            Log("connect op run without socket " + N(aRun) + " status "
                + N(Num(EE_WON_HOST_OP_STATUS, co)));
            Call(EE_WON_HOST_RELEASE, co);
        }

        // a timer (a plain AsyncOp): status, killed, kill, its socket, bytes and kind
        {
            void* t = Call(EE_WON_HOST_NEW_TIMER, NULL, NULL, 7).mCall.out_obj;
            Log("timer " + Fresh(t));
            Log("timer status " + N(Num(EE_WON_HOST_OP_STATUS, t)) + " killed "
                + N(Num(EE_WON_HOST_OP_KILLED, t)) + " socket " + Name(Call(EE_WON_HOST_OP_SOCKET, t).mCall.out_obj)
                + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, t)) + " addr " + AddrOf(Call(EE_WON_HOST_OP_ADDR, t), 0)
                + " is recv msg " + N(Num(EE_WON_HOST_OP_IS_RECV_MSG, t)));
            Call(EE_WON_HOST_KILL, t, NULL, 0);
            Log("timer killed " + N(Num(EE_WON_HOST_OP_KILLED, t)) + " status " + N(Num(EE_WON_HOST_OP_STATUS, t)));
            Call(EE_WON_HOST_RELEASE, t);
        }

        // a receive into a file (binary, its modify time set), a skipped receive, a receive with progress
        {
            void* f = NewOp(EE_WON_HOST_OP_RECV_BYTES, s, 6);
            Call(EE_WON_HOST_RECV_FILE, f, NULL, 0, 0, aFile);
            Call(EE_WON_HOST_RECV_BINARY, f, NULL, 1);
            Call(EE_WON_HOST_RECV_MODIFY_TIME, f, NULL, 1000000000);
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, string("bin\r\n\0", 6));
            long long aRun = Num(EE_WON_HOST_RUN_BLOCK, f, 2000);
            Log("file op " + N(aRun) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, f)));
            Call(EE_WON_HOST_RELEASE, f);
            FILE* aRead = fopen(aFile.c_str(), "rb");
            string aGot;
            if (aRead != NULL) {
                char aBuf[64];
                size_t n = fread(aBuf, 1, sizeof(aBuf), aRead);
                aGot.assign(aBuf, n);
                fclose(aRead);
            }
            struct stat aStat;
            long long aTime = stat(aFile.c_str(), &aStat) == 0 ? (long long)aStat.st_mtime : -1;
            Log("file " + Hex(aGot) + " modify time " + N(aTime));
            // appended, as text
            void* g = NewOp(EE_WON_HOST_OP_RECV_BYTES, s, 4);
            Call(EE_WON_HOST_RECV_FILE, g, NULL, 1, 0, aFile);
            Call(EE_WON_HOST_RECV_BINARY, g, NULL, 0);
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "t\nx\n");
            Log("text file op " + N(Num(EE_WON_HOST_RUN_BLOCK, g, 2000)));
            Call(EE_WON_HOST_RELEASE, g);
            aRead = fopen(aFile.c_str(), "rb");
            aGot.clear();
            if (aRead != NULL) {
                char aBuf[64];
                size_t n = fread(aBuf, 1, sizeof(aBuf), aRead);
                aGot.assign(aBuf, n);
                fclose(aRead);
            }
            Log("appended file " + Hex(aGot));

            void* k = NewOp(EE_WON_HOST_OP_RECV_BYTES, s, 4);
            Call(EE_WON_HOST_RECV_SKIP, k);
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "skip");
            aRun = Num(EE_WON_HOST_RUN_BLOCK, k, 2000);
            Log("skip op " + N(aRun) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, k)));
            Call(EE_WON_HOST_RELEASE, k);
            Answer a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 1, 50);
            Log("after skip " + N(a.mCall.out_num) + " " + Bytes(a));

            SmartPtr<CompletionBase<AsyncOpPtr> > aProgress =
                new ParamCompletion<AsyncOpPtr, SmartPtr<RefCount> >(ProgressCallback, NULL);
            void* p = NewOp(EE_WON_HOST_OP_RECV_BYTES, s, 10);
            Call(EE_WON_HOST_RECV_PROGRESS, p, static_cast<RefCount*>(aProgress.get()), 3);
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "0123456789");
            aRun = Num(EE_WON_HOST_RUN_BLOCK, p, 2000);
            Log("progress op " + N(aRun) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, p)));
            Call(EE_WON_HOST_RELEASE, p);
            Log("progress called " + N(!sProgress.empty()) + " last "
                + (sProgress.empty() ? string("none") : N((long long)sProgress.back())));
        }

        // datagrams: bind, send to, receive from (bytes and sender), a receive op's address, nothing to receive
        {
            void* u = NewSocket(1);
            void* v = NewSocket(1);
            Log("udp bind " + N(Num(EE_WON_HOST_SOCKET_BIND, u, 0, 0)));
            Log("udp bind reuse " + N(Num(EE_WON_HOST_SOCKET_BIND, v, 0, 1)));
            unsigned short aUPort = (unsigned short)Num(EE_WON_HOST_SOCKET_PORT, u);
            unsigned short aVPort = (unsigned short)Num(EE_WON_HOST_SOCKET_PORT, v);
            Log("udp ports nonzero " + N(aUPort != 0) + " " + N(aVPort != 0) + " connected "
                + N(Num(EE_WON_HOST_SOCKET_CONNECTED, u)));
            IPAddr aV("127.0.0.1", aVPort);
            Log("send to " + N(Call(EE_WON_HOST_SEND_BYTES_TO, u, NULL, 2000, 0, "datagram", &aV).mCall.out_num));
            Answer a = Call(EE_WON_HOST_RECV_BYTES_FROM, v, NULL, 512, 2000);
            Log("recv from " + N(a.mCall.out_num) + " " + Bytes(a) + " " + AddrOf(a, aUPort));
            a = Call(EE_WON_HOST_RECV_BYTES_FROM, v, NULL, 512, 50);
            Log("recv from none " + N(a.mCall.out_num) + " " + Bytes(a) + " " + AddrOf(a, aUPort));
            void* d = NewOp(EE_WON_HOST_OP_RECV_BYTES_FROM, v, 512);
            Call(EE_WON_HOST_SEND_BYTES_TO, u, NULL, 2000, 0, "to the op", &aV);
            long long aRun = Num(EE_WON_HOST_RUN_BLOCK, d, 2000);
            Log("recv from op " + N(aRun) + " bytes " + Bytes(Call(EE_WON_HOST_OP_BYTES, d))
                + " addr " + AddrOf(Call(EE_WON_HOST_OP_ADDR, d), aUPort) + " socket "
                + Name(Call(EE_WON_HOST_OP_SOCKET, d).mCall.out_obj));
            Call(EE_WON_HOST_RELEASE, d);
            void* st = NewOp(EE_WON_HOST_OP_SEND_BYTES_TO, u, 0, "op datagram", &aV);
            aRun = Num(EE_WON_HOST_RUN_BLOCK, st, 2000);
            Log("send to op " + N(aRun) + " addr "
                + AddrOf(Call(EE_WON_HOST_OP_ADDR, st), aVPort));
            a = Call(EE_WON_HOST_RECV_BYTES_FROM, v, NULL, 512, 2000);
            Log("sent to by op " + N(a.mCall.out_num) + " " + Bytes(a));
            Call(EE_WON_HOST_RELEASE, st);
            Call(EE_WON_HOST_CLOSE, u);
            Log("udp closed connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, u)) + " status "
                + N(Num(EE_WON_HOST_SOCKET_STATUS, u)));
            Log("send to reopened " + N(Call(EE_WON_HOST_SEND_BYTES_TO, u, NULL, 2000, 0, "reopened", &aV).mCall.out_num));
            a = Call(EE_WON_HOST_RECV_BYTES_FROM, v, NULL, 512, 2000);
            Log("recv after reopen " + N(a.mCall.out_num) + " " + Bytes(a));
            Call(EE_WON_HOST_SOCKET_KILL, v);
            a = Call(EE_WON_HOST_RECV_BYTES_FROM, v, NULL, 512, 50);
            Log("recv from killed " + N(a.mCall.out_num) + " " + Bytes(a));
            Log("killed status " + N(Num(EE_WON_HOST_SOCKET_STATUS, v)));
            Call(EE_WON_HOST_RELEASE, u);
            Call(EE_WON_HOST_RELEASE, v);
        }

        // the client closed: the server reads to the end, then a closed socket's calls
        {
            Call(EE_WON_HOST_SEND_BYTES, c, NULL, 2000, 0, "last");
            Call(EE_WON_HOST_CLOSE, c);
            Log("closed connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, c)) + " status "
                + N(Num(EE_WON_HOST_SOCKET_STATUS, c)) + " lfs " + N(Num(EE_WON_HOST_SOCKET_LFS, c)));
            Answer a = Call(EE_WON_HOST_RECV_BYTES, s, NULL, 0, 2000);
            Log("recv to close " + N(a.mCall.out_num) + " " + Bytes(a));
            a = Call(EE_WON_HOST_RECV_MSG, s, NULL, 2000);
            Log("recv msg after close " + N(a.mCall.out_num) + " " + Bytes(a));
            Log("send closed " + N(Call(EE_WON_HOST_SEND_MSG, c, NULL, 100, 0, Framed("x")).mCall.out_num));
            Call(EE_WON_HOST_SOCKET_KILL, s);
            Log("server killed connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, s)) + " status "
                + N(Num(EE_WON_HOST_SOCKET_STATUS, s)));
            // a refused connect, an invalid address
            void* x = NewSocket(0);
            Call(EE_WON_HOST_CLOSE, l);
            Log("listener closed port " + N(Num(EE_WON_HOST_SOCKET_PORT, l)));
            Log("connect refused " + N(Call(EE_WON_HOST_CONNECT, x, NULL, 5000, 0, string(), &aListen).mCall.out_num));
            Log("refused connected " + N(Num(EE_WON_HOST_SOCKET_CONNECTED, x)) + " status "
                + N(Num(EE_WON_HOST_SOCKET_STATUS, x)));
            Call(EE_WON_HOST_RELEASE, x);
        }

        // the op itself (obj NULL): killed flag, kill without and with finish, its status
        {
            SmartPtr<RustServerOp> aSelf = RustServerOp::NewHttpGet("127.0.0.1", "/self");
            sOp = aSelf.get();
            Log("self killed " + N(Num(EE_WON_HOST_OP_KILLED)) + " status " + N(Num(EE_WON_HOST_STATUS)));
            Call(EE_WON_HOST_KILL, NULL, NULL, 0);
            Log("self killed " + N(Num(EE_WON_HOST_OP_KILLED)) + " status " + N(Num(EE_WON_HOST_STATUS)));
            Call(EE_WON_HOST_KILL, NULL, NULL, 1);
            Log("self killed " + N(Num(EE_WON_HOST_OP_KILLED)) + " status " + N(Num(EE_WON_HOST_STATUS)));
            Log("op as obj killed " + N(Num(EE_WON_HOST_OP_KILLED, static_cast<RefCount*>(anOp.get()))) + " status "
                + N(Num(EE_WON_HOST_OP_STATUS, static_cast<RefCount*>(anOp.get()))));
            sOp = anOp.get();
        }
        Call(EE_WON_HOST_RELEASE, c);
        Call(EE_WON_HOST_RELEASE, l);
        aServer = NULL;
        sOp = NULL;
        anOp = NULL;
        aCore.Pump(20);
        aCore.Shutdown();
    }
    AsyncSocket::StopWinsock();
    Log("done");
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    string aPath = __FILE__;
    aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == string::npos ? 0 : aSlash + 1);
    aPath += "../../rust/crates/ee_won_net/src/golden_hostsock.txt";
    FILE* aGolden = fopen(aPath.c_str(), "rb");
    if (aGolden == NULL) {
        printf("FAIL: cannot open %s\n", aPath.c_str());
        return 1;
    }
    std::vector<string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aGolden)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aGolden);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_hostsock_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
