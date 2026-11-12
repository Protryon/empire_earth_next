// The Rust op base (ee_won hostop.rs) and the ops on it: MultiPingOp (multiping.rs) and FilePushOp (filepush.rs).
// The original's statements (copied from MultiPingOp.cpp and FilePushOp.cpp before the port, `this->` an op
// stand-in, every object a numbered stand-in) against the Rust ops through the base's one C call, both over the
// same stand-in: one function answering every EeWonHostCall from a generated stream and logging it. Sequences of
// each op's steps (the setters, RunHook asynchronous and blocking, the callbacks, CleanupHook, the results) on one
// op: the trace, the return and the members after each step; after each sequence the Rust op is freed and every
// reference it took must be gone. The exit code is the number of failed checks.
//
// `--print N` prints N sequences of each op with what the original did (rust/crates/ee_won/src/golden_hostop.txt,
// recorded before the port; `hostop::tests::golden` replays it); `--count N` runs N sequences of each.
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONBlowfish.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- generators: the steps' (sSeed) and the stand-in's answers (sAnswer, reseeded per sequence)

static unsigned int sSeed = 4242;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

static string Hex(const void* inData, size_t inLength)
{
    static const char kHex[] = "0123456789abcdef";
    string s;
    const unsigned char* p = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inLength; i++) { s += kHex[p[i] >> 4]; s += kHex[p[i] & 15]; }
    return s.empty() ? "-" : s;
}
static string Hex(const string& s) { return Hex(s.data(), s.size()); }

// ---- the stand-in: every call of the op answered from the stream and logged

struct Standin {
    unsigned int mSeed;
    int mSerial, mStatus, mMode, mLfs, mLastSocket, mCalls;
    unsigned mTick;
    std::vector<int> mRands;
    std::vector<string> mAddrs;
    std::map<int, int> mOwn;  // the op's own references per object (NEW_*, ADD_REF, RELEASE)
    string mTrace;
};
static Standin S;

static unsigned int ANext()
{
    S.mSeed = S.mSeed * 1103515245u + 12345u;
    return (S.mSeed >> 8) & 0xffffff;
}
static int APick(int n) { return (int)(ANext() % (unsigned)n); }
static void Log(const char* inFormat, ...)
{
    char aBuffer[512];
    va_list ap;
    va_start(ap, inFormat);
    vsnprintf(aBuffer, sizeof(aBuffer), inFormat, ap);
    va_end(ap);
    S.mTrace += aBuffer;
    S.mTrace += ';';
}
static bool OverBudget() { return S.mCalls > 300; }
static int SendStatus() { return (APick(6) == 0 || OverBudget()) ? 100001 : 0; }
static int RecvStatus()
{
    static const int k[] = { 0, 0, 0, 0, 100001, 100002, 10040, 100012 };
    int s = k[APick(8)];
    return OverBudget() ? 100001 : s;
}
static string Bytes(bool* outNull)
{
    *outNull = false;
    string b;
    int k = APick(8);
    if (k == 0) { *outNull = true; return b; }
    if (k == 1) return b;
    if (k == 2) { b += (char)APick(3); return b; }
    if (k == 3 || k == 4) {
        int s = APick(3) == 0 ? 0xffff : 0;
        const char r[7] = { 5, 5, 0, 22, 0, (char)(s & 0xff), (char)(s >> 8) };
        b.assign(r, 7);
        if (k == 4) b.resize(APick(7));
        return b;
    }
    if (k == 5 || k == 6) {
        int id = (!S.mRands.empty() && APick(3) != 0) ? S.mRands[APick((int)S.mRands.size())] : (int)(ANext() & 0x7fff);
        const char r[7] = { 3, 1, 6, (char)(id & 0xff), (char)((id >> 8) & 0xff), (char)((id >> 16) & 0xff),
                            (char)((id >> 24) & 0xff) };
        b.assign(r, 7);
        return b;
    }
    int n = APick(9);
    for (int i = 0; i < n; i++) b += (char)(ANext() & 0xff);
    return b;
}
// the blocking receives' bytes, biased to what each expects: a status reply, an acknowledgement, a ping reply to a
// recent ping
static string StatusBytes(bool* outNull)
{
    int k = APick(4);
    if (k == 3) return Bytes(outNull);
    *outNull = false;
    int s = APick(3) == 0 ? 0xffff : 0;
    const char r[7] = { 5, 5, 0, 22, 0, (char)(s & 0xff), (char)(s >> 8) };
    string b(r, 7);
    if (k == 2) b.resize(APick(7));
    return b;
}
static string AckBytes(bool* outNull)
{
    if (APick(4) == 3) return Bytes(outNull);
    *outNull = false;
    return string(1, '');
}
static string PingBytes(bool* outNull)
{
    if (APick(4) == 3 || S.mRands.empty()) return Bytes(outNull);
    *outNull = false;
    size_t n = S.mRands.size();
    int id = S.mRands[n - 1 - APick(n < 3 ? (int)n : 3)];
    const char r[7] = { 3, 1, 6, (char)(id & 0xff), (char)((id >> 8) & 0xff), (char)((id >> 16) & 0xff),
                        (char)((id >> 24) & 0xff) };
    return string(r, 7);
}
static string Addr()
{
    if (!S.mAddrs.empty() && APick(4) != 0) return S.mAddrs[APick((int)S.mAddrs.size())];
    string a;
    for (int i = 0; i < 6; i++) a += (char)(ANext() & 0xff);
    return a;
}
static void Give(EeWonHostCall* c, const string& b, bool isNull)
{
    if (!isNull) c->out_sink(c->out_context, reinterpret_cast<const uint8_t*>(b.data()), b.size());
}
static string BytesLog(const string& b, bool isNull) { return isNull ? "null" : Hex(b); }
static int NewObj() { int id = ++S.mSerial; S.mOwn[id] = 1; return id; }
static void* P(int id) { return reinterpret_cast<void*>((intptr_t)id); }
static int Id(void* p) { return (int)(intptr_t)p; }

static void Call(EeWonHostCall* c)
{
    S.mCalls++;
    int o0 = Id(c->obj[0]), o1 = Id(c->obj[1]);
    long long n0 = c->num[0], n1 = c->num[1];
    string data = c->data ? string(reinterpret_cast<const char*>(c->data), c->len) : string();
    string addr(reinterpret_cast<const char*>(c->addr), 6);
    switch (c->code) {
    case EE_WON_HOST_STATUS: c->out_num = S.mStatus; break;
    case EE_WON_HOST_MODE: c->out_num = S.mMode; break;
    case EE_WON_HOST_LFS: c->out_num = S.mLfs; break;
    case EE_WON_HOST_TIME_LEFT: {
        static const unsigned k[] = { 0, 7, 300, 5000 };
        unsigned v = k[APick(4)];
        if (OverBudget()) v = 0;
        c->out_num = v;
        Log("tl=%u", v);
        break;
    }
    case EE_WON_HOST_TICK: {
        S.mTick += APick(1500);
        c->out_num = S.mTick;
        Log("tick=%u", S.mTick);
        break;
    }
    case EE_WON_HOST_RAND: {
        int v = (int)(ANext() & 0x7fff);
        S.mRands.push_back(v);
        c->out_num = v;
        Log("rand=%d", v);
        break;
    }
    case EE_WON_HOST_SRAND: Log("srand"); break;
    case EE_WON_HOST_SEND_TIME:
    case EE_WON_HOST_RECV_TIME: {
        unsigned v = APick(2) ? 20000 : 0;
        c->out_num = v;
        Log("%s=%u", c->code == EE_WON_HOST_SEND_TIME ? "stime" : "rtime", v);
        break;
    }
    case EE_WON_HOST_NEW_SOCKET: {
        int id = NewObj();
        S.mLastSocket = id;
        c->out_obj = P(id);
        Log("sock#%d u%d", id, (int)n0);
        break;
    }
    case EE_WON_HOST_NEW_OP: {
        int id = NewObj();
        c->out_obj = P(id);
        if (n0 == EE_WON_HOST_OP_SEND_BYTES_TO) S.mAddrs.push_back(addr);
        Log("op#%d k%d s%d n%d a%s d%s", id, (int)n0, o0, (int)n1, Hex(addr).c_str(), Hex(data).c_str());
        break;
    }
    case EE_WON_HOST_NEW_TIMER: {
        int id = NewObj();
        c->out_obj = P(id);
        Log("timer#%d id%d", id, (int)n0);
        break;
    }
    case EE_WON_HOST_NEW_PEER_AUTH: {
        int id = NewObj();
        c->out_obj = P(id);
        Log("pauth#%d s%d a%s %d", id, o0, Hex(addr).c_str(), (int)n0);
        break;
    }
    case EE_WON_HOST_OP_STATUS: {
        int v = RecvStatus();
        c->out_num = v;
        Log("st#%d=%d", o0, v);
        break;
    }
    case EE_WON_HOST_OP_BYTES: {
        bool isNull;
        string b = Bytes(&isNull);
        Give(c, b, isNull);
        Log("bytes#%d=%s", o0, BytesLog(b, isNull).c_str());
        break;
    }
    case EE_WON_HOST_OP_ADDR: {
        string a = Addr();
        memcpy(c->out_addr, a.data(), 6);
        Log("addr#%d=%s", o0, Hex(a).c_str());
        break;
    }
    case EE_WON_HOST_OP_SOCKET: {
        int v = APick(5) == 0 ? 999 : S.mLastSocket;
        c->out_obj = P(v);
        Log("osock#%d=%d", o0, v);
        break;
    }
    case EE_WON_HOST_PEER_AUTH_KEY: {
        bool isNull = APick(4) == 0;
        string k;
        if (!isNull)
            for (int i = 0; i < 8; i++) k += (char)(ANext() & 0xff);
        Give(c, k, isNull);
        Log("key#%d=%s", o0, BytesLog(k, isNull).c_str());
        break;
    }
    case EE_WON_HOST_SOCKET_BIND: {
        int v = APick(4) == 0 ? 10048 : 0;
        c->out_num = v;
        Log("bind#%d %d %d=%d", o0, (int)n0, (int)n1, v);
        break;
    }
    case EE_WON_HOST_SOCKET_SET_LFS: Log("lfs#%d %d", o0, (int)n0); break;
    case EE_WON_HOST_SEND_BYTES_TO: {
        int v = SendStatus();
        c->out_num = v;
        Log("sendto#%d %s %s t%u=%d", o0, Hex(data).c_str(), Hex(addr).c_str(), (unsigned)n0, v);
        break;
    }
    case EE_WON_HOST_RECV_BYTES_FROM: {
        int v = RecvStatus();
        bool isNull;
        string b = PingBytes(&isNull);
        string a = Addr();
        c->out_num = v;
        Give(c, b, isNull);
        memcpy(c->out_addr, a.data(), 6);
        Log("recvfrom#%d n%d t%u=%d %s %s", o0, (int)n0, (unsigned)n1, v, BytesLog(b, isNull).c_str(), Hex(a).c_str());
        break;
    }
    case EE_WON_HOST_SEND_BYTES:
    case EE_WON_HOST_SEND_MSG: {
        int v = SendStatus();
        c->out_num = v;
        Log("%s#%d %s t%u=%d", c->code == EE_WON_HOST_SEND_BYTES ? "send" : "sendmsg", o0, Hex(data).c_str(),
            (unsigned)n0, v);
        break;
    }
    case EE_WON_HOST_RECV_BYTES:
    case EE_WON_HOST_RECV_MSG: {
        int v = SendStatus();
        bool isNull;
        string b = c->code == EE_WON_HOST_RECV_BYTES ? AckBytes(&isNull) : StatusBytes(&isNull);
        c->out_num = v;
        Give(c, b, isNull);
        if (c->code == EE_WON_HOST_RECV_BYTES)
            Log("recv#%d n%d t%u=%d %s", o0, (int)n0, (unsigned)n1, v, BytesLog(b, isNull).c_str());
        else
            Log("recvmsg#%d t%u=%d %s", o0, (unsigned)n0, v, BytesLog(b, isNull).c_str());
        break;
    }
    case EE_WON_HOST_RUN_BLOCK: {
        int v = (APick(3) != 0 && !OverBudget()) ? 1 : 0;
        c->out_num = v;
        Log("block#%d t%u=%d", o0, (unsigned)n0, v);
        break;
    }
    case EE_WON_HOST_FILE_OPEN: {
        int v = APick(5) != 0 ? 1 : 0;
        c->out_num = v;
        Log("fopen %s=%d", Hex(data).c_str(), v);
        break;
    }
    case EE_WON_HOST_FILE_READ: {
        static const unsigned k[] = { 0, 3, 17, 64 };
        unsigned n = k[APick(4)];
        if ((unsigned long long)n0 < n) n = (unsigned)n0;
        string b;
        for (unsigned i = 0; i < n; i++) b += (char)(ANext() & 0xff);
        Give(c, b, false);
        Log("fread %u=%s", (unsigned)n0, Hex(b).c_str());
        break;
    }
    case EE_WON_HOST_FILE_CLOSE: Log("fclose"); break;
    case EE_WON_HOST_FINISH:
        Log("finish %d", (int)n0);
        if (S.mStatus == 100000) S.mStatus = (int)n0;
        break;
    case EE_WON_HOST_KILL: Log("kill#%d %d", o0, (int)n0); break;
    case EE_WON_HOST_TRACK: Log("track#%d %d", o0, (int)n0); break;
    case EE_WON_HOST_QUEUE: Log("queue#%d s%d t%u", o1, o0, (unsigned)n0); break;
    case EE_WON_HOST_RUN_ASYNC: Log("async#%d t%u", o0, (unsigned)n0); break;
    case EE_WON_HOST_CLOSE: Log("close#%d", o0); break;
    case EE_WON_HOST_SET_REPEAT: Log("repeat#%d s%d id%d", o1, o0, (int)n0); break;
    case EE_WON_HOST_COMPLETE: Log("complete#%d", o0); break;
    case EE_WON_HOST_BASE_CLEANUP: Log("basecleanup"); break;
    case EE_WON_HOST_RELEASE: S.mOwn[o0]--; break;
    case EE_WON_HOST_ADD_REF: S.mOwn[o0]++; break;
    default: Log("unknown %d", c->code); break;
    }
}
static void HostFn(void*, EeWonHostCall* c) { Call(c); }

// the original half's calls, through the same stand-in
static void Sink(void* inContext, const uint8_t* inData, size_t inLength)
{
    string* s = static_cast<string*>(inContext);
    s->assign(reinterpret_cast<const char*>(inData), inLength);
    s->push_back('\x01'); // present
}
struct Res {
    long long mNum;
    int mObj;
    string mAddr;
    bool mNull;
    string mBytes;
};
static Res Do(int inCode, int o0 = 0, int o1 = 0, long long n0 = 0, long long n1 = 0, const string& inData = string(),
              const string& inAddr = string(6, '\0'))
{
    EeWonHostCall c;
    memset(&c, 0, sizeof(c));
    c.code = inCode;
    c.obj[0] = P(o0);
    c.obj[1] = P(o1);
    c.num[0] = n0;
    c.num[1] = n1;
    c.data = inData.empty() ? NULL : reinterpret_cast<const uint8_t*>(inData.data());
    c.len = inData.size();
    memcpy(c.addr, inAddr.data(), 6);
    string out;
    c.out_sink = Sink;
    c.out_context = &out;
    Call(&c);
    Res r;
    r.mNum = c.out_num;
    r.mObj = Id(c.out_obj);
    r.mAddr.assign(reinterpret_cast<const char*>(c.out_addr), 6);
    r.mNull = out.empty();
    if (!r.mNull) out.resize(out.size() - 1);
    r.mBytes = out;
    return r;
}

// ---- the original's statements: MultiPingOp.cpp before the port (ee_won's server.rs rules as the C++ called
// them; the never-run structs' id and start, indeterminate there, 0)

namespace OrigPing {
struct Ping {
    string mAddr;
    int mPingTime, mNumFailures;
    unsigned mStartPingTick;
    int mPingId, mEntity;
};
struct AddrLess {
    bool operator()(const string& a, const string& b) const { return memcmp(a.data(), b.data(), 6) < 0; }
};
typedef std::list<Ping*> List;
typedef std::map<string, Ping*, AddrLess> Map;
struct Op {
    List mPingList;
    Map mPingMap;
    List mFinishedList;
    unsigned short mBatchSize;
    unsigned char mMaxFailures;
    unsigned mMaxPingTime;
    unsigned short mMaxPingReplySize;
    bool mOrderListByPingTime;
    int mSocket, mRecvTimeoutOp;
    Op() : mBatchSize(20), mMaxFailures(3), mMaxPingTime(3000), mMaxPingReplySize(2048), mOrderListByPingTime(true),
           mSocket(0), mRecvTimeoutOp(0) {}
    int GetStatus() { return (int)Do(EE_WON_HOST_STATUS).mNum; }
    int GetMode() { return (int)Do(EE_WON_HOST_MODE).mNum; }
    unsigned TimeLeft() { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
    void Finish(int s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
};
static unsigned WONTickCount() { return (unsigned)Do(EE_WON_HOST_TICK).mNum; }

static void AddServer(Op& o, const string& inAddr, int inEntity)
{
    Ping* p = new Ping;
    p->mAddr = inAddr;
    p->mPingTime = -1;
    p->mNumFailures = 0;
    p->mStartPingTick = 0;
    p->mPingId = 0;
    p->mEntity = inEntity;
    o.mPingList.push_back(p);
}
static string GetRequest(Op& o, Ping* theStruct)
{
    theStruct->mPingId = (int)Do(EE_WON_HOST_RAND).mNum;
    theStruct->mStartPingTick = WONTickCount();
    string aBytes;
    ee_won_server_multi_ping_request(theStruct->mPingId, [](void* c, const uint8_t* b, size_t n) {
        static_cast<string*>(c)->append(reinterpret_cast<const char*>(b), n); }, &aBytes);
    return aBytes;
}
enum PingAction { PingAction_InvalidReply, PingAction_Done, PingAction_More };
static PingAction HandleResponse(Op& o, Ping* theStruct, const string& theResponse)
{
    if (!ee_won_server_multi_ping_reply(reinterpret_cast<const uint8_t*>(theResponse.data()), theResponse.length(),
                                        theStruct->mPingId))
        return PingAction_InvalidReply;
    theStruct->mPingTime = WONTickCount() - theStruct->mStartPingTick;
    return PingAction_Done;
}
static void PrepareBatch(Op& o)
{
    o.mPingMap.clear();
    List::iterator anItr = o.mPingList.begin();
    int aCount = 0;
    while (anItr != o.mPingList.end() && aCount < o.mBatchSize) {
        o.mPingMap[(*anItr)->mAddr] = *anItr;
        o.mPingList.erase(anItr++);
        aCount++;
    }
}
static void FinishBatch(Op& o)
{
    Map::iterator anItr = o.mPingMap.begin();
    while (anItr != o.mPingMap.end()) {
        Ping* aStruct = anItr->second;
        aStruct->mNumFailures++;
        if (ee_won_server_multi_ping_gives_up(aStruct->mNumFailures, o.mMaxFailures))
            o.mFinishedList.push_back(aStruct);
        else
            o.mPingList.push_back(aStruct);
        ++anItr;
    }
    o.mPingMap.clear();
}
static void PingBatchAsync(Op& o)
{
    if (o.mRecvTimeoutOp != 0) {
        Do(EE_WON_HOST_KILL, o.mRecvTimeoutOp, 0, 1);
        o.mRecvTimeoutOp = 0;
    }
    PrepareBatch(o);
    Map::iterator anItr = o.mPingMap.begin();
    while (anItr != o.mPingMap.end()) {
        string aPing = GetRequest(o, anItr->second);
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_SEND_BYTES_TO, 0, aPing, anItr->first).mObj;
        Do(EE_WON_HOST_QUEUE, o.mSocket, anOp, 0xffffffffu);
        ++anItr;
    }
    o.mRecvTimeoutOp = Do(EE_WON_HOST_NEW_TIMER, 0, 0, 2).mObj;
    Do(EE_WON_HOST_RUN_ASYNC, o.mRecvTimeoutOp, 0, o.mMaxPingTime);
}
static unsigned ResponseTimeLeft(Op& o, unsigned theStartTick)
{
    unsigned aNow = WONTickCount();
    if (ee_won_server_multi_ping_time_left(aNow, theStartTick, o.mMaxPingTime, 0xffffffff) == 0)
        return 0;
    return ee_won_server_multi_ping_time_left(aNow, theStartTick, o.mMaxPingTime, o.TimeLeft());
}
static bool HandleResponsePrv(Op& o, int theStatus, const string& theResponse, const string& theAddr)
{
    int aRule = ee_won_server_multi_ping_recv_status(theStatus);
    if (aRule == 0)
        return false;
    if (aRule == 2) {
        Map::iterator anItr = o.mPingMap.find(theAddr);
        if (anItr != o.mPingMap.end()) {
            PingAction anAction = HandleResponse(o, anItr->second, theResponse);
            if (anAction == PingAction_Done) {
                o.mFinishedList.push_back(anItr->second);
                o.mPingMap.erase(anItr);
            } else if (anAction == PingAction_More) {
                o.mPingList.push_back(anItr->second);
                o.mPingMap.erase(anItr);
            }
        }
    }
    return true;
}
static void RecvCallback(Op& o, int theOp)
{
    if (o.GetStatus() != 100000 || Do(EE_WON_HOST_OP_SOCKET, theOp).mObj != o.mSocket)
        return;
    int aStatus = (int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum;
    Res aBytes = Do(EE_WON_HOST_OP_BYTES, theOp);
    string anAddr = Do(EE_WON_HOST_OP_ADDR, theOp).mAddr;
    if (!HandleResponsePrv(o, aStatus, aBytes.mBytes, anAddr))
        o.Finish((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum);
    int aNext = ee_won_server_multi_ping_next(o.mPingMap.size(), o.mPingList.size());
    if (aNext == 1)
        o.Finish(0);
    else if (aNext == 2)
        PingBatchAsync(o);
}
static void TimeoutCallback(Op& o, int theOp)
{
    if (o.GetStatus() != 100000 || theOp != o.mRecvTimeoutOp)
        return;
    o.mRecvTimeoutOp = 0;
    FinishBatch(o);
    PingBatchAsync(o);
}
static int PingBatchBlock(Op& o)
{
    PrepareBatch(o);
    Map::iterator anItr = o.mPingMap.begin();
    int aStatus = 0;
    while (o.TimeLeft() > 0 && anItr != o.mPingMap.end()) {
        string aPing = GetRequest(o, anItr->second);
        aStatus = (int)Do(EE_WON_HOST_SEND_BYTES_TO, o.mSocket, 0, o.TimeLeft(), 0, aPing, anItr->first).mNum;
        if (aStatus != 0)
            return aStatus;
        ++anItr;
    }
    unsigned aStartTick = WONTickCount();
    while (ResponseTimeLeft(o, aStartTick) > 0 && !o.mPingMap.empty()) {
        Res r = Do(EE_WON_HOST_RECV_BYTES_FROM, o.mSocket, 0, o.mMaxPingReplySize, ResponseTimeLeft(o, aStartTick));
        aStatus = (int)r.mNum;
        if (!HandleResponsePrv(o, aStatus, r.mBytes, r.mAddr))
            return aStatus;
    }
    return aStatus;
}
static void RunHook(Op& o)
{
    Do(EE_WON_HOST_SRAND);
    o.mSocket = Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 1).mObj;
    Do(EE_WON_HOST_SOCKET_BIND, o.mSocket, 0, 0, 0);
    o.mFinishedList.clear();
    if (o.GetMode() == 1) {
        int aRecv = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_RECV_BYTES_FROM, o.mMaxPingReplySize).mObj;
        Do(EE_WON_HOST_SET_REPEAT, o.mSocket, aRecv, 1);
        PingBatchAsync(o);
        return;
    }
    int aStatus;
    while (o.TimeLeft() > 0 && !o.mPingList.empty()) {
        aStatus = PingBatchBlock(o);
        FinishBatch(o);
        if (!ee_won_server_multi_ping_continues(aStatus)) {
            o.Finish(aStatus);
            return;
        }
    }
    o.Finish(0);
}
static void CleanupHook(Op& o)
{
    if (o.mSocket != 0) {
        Do(EE_WON_HOST_CLOSE, o.mSocket);
        o.mSocket = 0;
    }
    if (o.mRecvTimeoutOp != 0) {
        Do(EE_WON_HOST_KILL, o.mRecvTimeoutOp, 0, 1);
        o.mRecvTimeoutOp = 0;
    }
    FinishBatch(o);
    List::iterator aListItr = o.mPingList.begin();
    while (aListItr != o.mPingList.end()) {
        o.mFinishedList.push_back(*aListItr);
        ++aListItr;
    }
    o.mPingList.clear();
    if (!o.mOrderListByPingTime)
        return;
    std::vector<Ping*> aStructs(o.mFinishedList.begin(), o.mFinishedList.end());
    std::vector<int> aTimes;
    for (size_t i = 0; i < aStructs.size(); i++)
        aTimes.push_back(aStructs[i]->mPingTime);
    std::vector<size_t> anOrder(aStructs.size());
    if (!aStructs.empty())
        ee_won_server_multi_ping_order(&aTimes[0], aTimes.size(), &anOrder[0]);
    o.mFinishedList.clear();
    for (size_t i = 0; i < anOrder.size(); i++)
        o.mFinishedList.push_back(aStructs[anOrder[i]]);
}
static string One(const Ping* p)
{
    char b[100];
    snprintf(b, sizeof(b), ":%d:%d:%d:%u", p->mPingTime, p->mNumFailures, p->mPingId, p->mStartPingTick);
    return Hex(p->mAddr) + b;
}
static string Describe(const Op& o)
{
    string l, m, f;
    for (List::const_iterator i = o.mPingList.begin(); i != o.mPingList.end(); ++i) l += (l.empty() ? "" : ",") + One(*i);
    for (Map::const_iterator i = o.mPingMap.begin(); i != o.mPingMap.end(); ++i) m += (m.empty() ? "" : ",") + One(i->second);
    for (List::const_iterator i = o.mFinishedList.begin(); i != o.mFinishedList.end(); ++i)
        f += (f.empty() ? "" : ",") + One(*i);
    char b[200];
    snprintf(b, sizeof(b), " bs %u mf %u mp %u sock %d timer %d", (unsigned)o.mBatchSize, (unsigned)o.mMaxFailures,
             o.mMaxPingTime, o.mSocket, o.mRecvTimeoutOp);
    return "list " + l + " map " + m + " fin " + f + b;
}
// GetPingList and GetServerContext(discard): each finished server's address, time, failures and both keeps
static string Results(const Op& o)
{
    string r;
    char b[100];
    snprintf(b, sizeof(b), "n%d", (int)o.mFinishedList.size());
    r += b;
    for (List::const_iterator i = o.mFinishedList.begin(); i != o.mFinishedList.end(); ++i) {
        snprintf(b, sizeof(b), " %d:%d:%d:%d:%d", (*i)->mPingTime, (*i)->mNumFailures, (*i)->mEntity,
                 ee_won_server_multi_ping_keeps((*i)->mPingTime, true), ee_won_server_multi_ping_keeps((*i)->mPingTime, false));
        r += Hex((*i)->mAddr) + b;
    }
    return r;
}
}

// ---- the original's statements: FilePushOp.cpp before the port (the file a stand-in; the session's key copied
// as its bytes)

namespace OrigPush {
enum { TRACK_AUTH = 1, TRACK_STREAM_REQUEST = 2, TRACK_STATUS_REPLY = 3, TRACK_ACK = 4 };
struct Op {
    string mConnectAddr, mFileName, mRemoteFileName, mComment;
    bool mIsCompress;
    unsigned mModifyHigh, mModifyLow;
    int mProgressCompletion;
    unsigned mTotalBytesSent, mLastBytesSent;
    Blowfish mEncryptKey;
    unsigned mBlockTransferSize;
    int mSocket;
    explicit Op(const string& inAddr)
        : mConnectAddr(inAddr), mIsCompress(false), mModifyHigh(0), mModifyLow(0), mProgressCompletion(0),
          mTotalBytesSent(0), mLastBytesSent(0), mBlockTransferSize(65535), mSocket(0) {}
    int GetMode() { return (int)Do(EE_WON_HOST_MODE).mNum; }
    unsigned TimeLeft() { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
    unsigned SendTime() { return (unsigned)Do(EE_WON_HOST_SEND_TIME).mNum; }
    unsigned RecvTime() { return (unsigned)Do(EE_WON_HOST_RECV_TIME).mNum; }
    unsigned char LengthFieldSize() { return (unsigned char)Do(EE_WON_HOST_LFS).mNum; }
    void Finish(int s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
    int Track(int inOp, int inId) { Do(EE_WON_HOST_TRACK, inOp, 0, inId); return inOp; }
};
static bool SendFileChunk(Op& o);
static bool CheckError(Op& o, int theStatus)
{
    if (theStatus != 0) {
        o.Finish(theStatus);
        return true;
    }
    return false;
}
static bool GetEncryptKey(Op& o, int theOp)
{
    Res aSession = Do(EE_WON_HOST_PEER_AUTH_KEY, theOp);
    if (aSession.mNull) {
        o.Finish(100013);
        return false;
    }
    Blowfish aKey;
    aKey.SetKey(aSession.mBytes.data(), (int)aSession.mBytes.size());
    o.mEncryptKey = aKey;
    return true;
}
static void DoProgressCompletion(Op& o)
{
    if (o.mProgressCompletion != 0)
        Do(EE_WON_HOST_COMPLETE, o.mProgressCompletion);
}
static bool HandleFactStatusReply(Op& o, const string& theReply)
{
    ReadBuffer aReply(theReply.data(), (unsigned long)theReply.length());
    int aStatus = 100004;
    try {
        unsigned char aHeaderType = aReply.ReadByte();
        unsigned short aServiceType = aReply.ReadShort();
        unsigned short aMessageType = aReply.ReadShort();
        aStatus = aReply.ReadShort();
        if (CheckError(o, aStatus))
            return false;
    } catch (ReadBufferException&) {
        o.Finish(100012);
        return false;
    }
    if (o.mTotalBytesSent > 0) {
        DoProgressCompletion(o);
        o.Finish(0);
        return false;
    }
    return true;
}
static bool RecvFactStatusReply(Op& o)
{
    if (o.GetMode() == 1) {
        // RecvMsgAsync(TRACK_STATUS_REPLY)
        int anOp = Do(EE_WON_HOST_NEW_OP, o.mSocket, 0, EE_WON_HOST_OP_RECV_MSG).mObj;
        o.Track(anOp, TRACK_STATUS_REPLY);
        Do(EE_WON_HOST_QUEUE, o.mSocket, anOp, 0xffffffffu);
        return false;
    }
    unsigned aTime = o.RecvTime();
    Res aReply = Do(EE_WON_HOST_RECV_MSG, o.mSocket, 0, aTime);
    if (CheckError(o, (int)aReply.mNum))
        return false;
    return HandleFactStatusReply(o, aReply.mBytes);
}
static bool HandleAck(Op& o, char theAck)
{
    if (theAck == 1) {
        DoProgressCompletion(o);
        if (o.GetMode() == 1)
            SendFileChunk(o);
        return true;
    }
    return RecvFactStatusReply(o);
}
static bool RecvAck(Op& o)
{
    if (o.GetMode() == 1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, o.mSocket, 0, EE_WON_HOST_OP_RECV_BYTES, 1).mObj;
        o.Track(anOp, TRACK_ACK);
        Do(EE_WON_HOST_QUEUE, o.mSocket, anOp, 0xffffffffu);
        return false;
    }
    Res aByte = Do(EE_WON_HOST_RECV_BYTES, o.mSocket, 0, 1, 0xffffffffu);
    if (CheckError(o, (int)aByte.mNum))
        return false;
    return HandleAck(o, aByte.mBytes.empty() ? 0 : aByte.mBytes[0]);
}
static bool SendFileChunk(Op& o)
{
    WriteBuffer aBuf(4, false);
    aBuf.Reserve(o.mBlockTransferSize + 20);
    string aRead = Do(EE_WON_HOST_FILE_READ, 0, 0, o.mBlockTransferSize).mBytes;
    int aLen = (int)aRead.size();
    if (aLen > 0) {
        ByteBufferPtr anEncrypt = o.mEncryptKey.Encrypt(aRead.data(), aLen);
        if (anEncrypt.get() != NULL)
            aBuf.AppendBytes(anEncrypt->data(), anEncrypt->length());
    }
    o.mLastBytesSent = aLen;
    o.mTotalBytesSent += aLen;
    ByteBufferPtr aMsg = aBuf.ToByteBuffer(true);
    string aBytes(aMsg->data(), aMsg->length());
    if (o.GetMode() == 1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_SEND_BYTES, 0, aBytes).mObj;
        Do(EE_WON_HOST_QUEUE, o.mSocket, o.Track(anOp, 0), 0xffffffffu);
    } else {
        unsigned aTime = o.SendTime();
        if (CheckError(o, (int)Do(EE_WON_HOST_SEND_BYTES, o.mSocket, 0, aTime, 0, aBytes).mNum))
            return false;
    }
    if (aLen > 0)
        return RecvAck(o);
    else
        return RecvFactStatusReply(o);
}
static bool SendStreamFileRequest(Op& o)
{
    WriteBuffer aMsg(o.LengthFieldSize(), true);
    aMsg.AppendByte(5);
    aMsg.AppendShort(5);
    aMsg.AppendShort(21);
    aMsg.AppendString(o.mRemoteFileName);
    aMsg.AppendBool(o.mIsCompress);
    aMsg.AppendLong(o.mModifyHigh);
    aMsg.AppendLong(o.mModifyLow);
    aMsg.AppendString(o.mComment);
    ByteBufferPtr aBuf = aMsg.ToByteBuffer(true);
    string aBytes(aBuf->data(), aBuf->length());
    if (o.GetMode() == 1) {
        // SendMsgAsync(msg, TRACK_STREAM_REQUEST)
        int anOp = Do(EE_WON_HOST_NEW_OP, o.mSocket, 0, EE_WON_HOST_OP_SEND_MSG, 0, aBytes).mObj;
        o.Track(anOp, TRACK_STREAM_REQUEST);
        Do(EE_WON_HOST_QUEUE, o.mSocket, anOp, 0xffffffffu);
        RecvFactStatusReply(o);
        RecvAck(o);
        return false;
    }
    unsigned aTime = o.SendTime();
    if (CheckError(o, (int)Do(EE_WON_HOST_SEND_MSG, o.mSocket, 0, aTime, 0, aBytes).mNum))
        return false;
    if (!RecvFactStatusReply(o))
        return false;
    return RecvAck(o);
}
static bool CallbackHook(Op& o, int theOp, int theId)
{
    if ((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum != 0) {
        o.Finish((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum);
        return true;
    }
    switch (theId) {
    case TRACK_AUTH:
        if (GetEncryptKey(o, theOp))
            SendStreamFileRequest(o);
        return true;
    case TRACK_STATUS_REPLY: {
        Res r = Do(EE_WON_HOST_OP_BYTES, theOp);
        HandleFactStatusReply(o, r.mBytes);
        return true;
    }
    case TRACK_ACK: {
        Res r = Do(EE_WON_HOST_OP_BYTES, theOp);
        HandleAck(o, r.mBytes.empty() ? 0 : r.mBytes[0]);
        return true;
    }
    }
    return false;
}
static void RunHook(Op& o)
{
    o.mTotalBytesSent = 0;
    o.mLastBytesSent = 0;
    if (Do(EE_WON_HOST_FILE_OPEN, 0, 0, 0, 0, o.mFileName).mNum == 0) {
        o.Finish(100007);
        return;
    }
    o.mSocket = Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 0).mObj;
    Do(EE_WON_HOST_SOCKET_SET_LFS, o.mSocket, 0, o.LengthFieldSize());
    int aPeerAuthOp = Do(EE_WON_HOST_NEW_PEER_AUTH, o.mSocket, 0, 1, 0, string(), o.mConnectAddr).mObj;
    if (o.GetMode() == 1) {
        o.Track(aPeerAuthOp, TRACK_AUTH);
        Do(EE_WON_HOST_RUN_ASYNC, aPeerAuthOp, 0, 0xffffffffu);
        return;
    }
    if (Do(EE_WON_HOST_RUN_BLOCK, aPeerAuthOp, 0, o.TimeLeft()).mNum == 0) {
        o.Finish((int)Do(EE_WON_HOST_OP_STATUS, aPeerAuthOp).mNum);
        return;
    }
    if (!GetEncryptKey(o, aPeerAuthOp))
        return;
    if (!SendStreamFileRequest(o))
        return;
    while (true) {
        if (!SendFileChunk(o))
            break;
    }
}
static void CleanupHook(Op& o)
{
    Do(EE_WON_HOST_BASE_CLEANUP);
    Do(EE_WON_HOST_FILE_CLOSE);
}
static string Describe(const Op& o)
{
    char b[300];
    snprintf(b, sizeof(b), " comp %d mod %x:%x prog %d total %u last %u key %s bs %u comment %s sock %d",
             o.mIsCompress ? 1 : 0, o.mModifyHigh, o.mModifyLow, o.mProgressCompletion, o.mTotalBytesSent,
             o.mLastBytesSent, Hex(o.mEncryptKey.GetKey(), o.mEncryptKey.GetKeyLen()).c_str(), o.mBlockTransferSize,
             Hex(o.mComment).c_str(), o.mSocket);
    return "addr " + Hex(o.mConnectAddr) + " file " + Hex(o.mFileName) + " remote " + Hex(o.mRemoteFileName) + b;
}
static string Results(const Op& o)
{
    char b[60];
    snprintf(b, sizeof(b), "total %u last %u", o.mTotalBytesSent, o.mLastBytesSent);
    return b;
}
}

// ---- the Rust half: the ops through the base's C interface, bound to the same stand-in

static void DescribeSink(void* inContext, const uint8_t* inData, size_t inLength)
{
    static_cast<string*>(inContext)->assign(reinterpret_cast<const char*>(inData), inLength);
}
static string RDescribe(EeWonHostOp* op)
{
    string s;
    ee_won_hostop_describe(op, DescribeSink, &s);
    return s;
}
static void RSet(EeWonHostOp* op, int which, long long num, const string& data = string(), const string* addr = NULL,
                 int obj = 0)
{
    ee_won_hostop_set(op, which, num, data.empty() ? NULL : reinterpret_cast<const uint8_t*>(data.data()), data.size(),
                      addr ? reinterpret_cast<const uint8_t*>(addr->data()) : NULL, P(obj));
}
static string RPingResults(EeWonHostOp* op)
{
    char b[100];
    int n = (int)ee_won_hostop_num(op, EE_WON_MULTIPING_COUNT, 0);
    snprintf(b, sizeof(b), "n%d", n);
    string r = b;
    for (int i = 0; i < n; i++) {
        string a;
        ee_won_hostop_bytes(op, EE_WON_MULTIPING_ADDR, i, DescribeSink, &a);
        snprintf(b, sizeof(b), " %d:%d:%d:%d:%d", (int)ee_won_hostop_num(op, EE_WON_MULTIPING_TIME, i),
                 (int)ee_won_hostop_num(op, EE_WON_MULTIPING_FAILURES, i), (int)ee_won_hostop_num(op, EE_WON_MULTIPING_ENTITY, i),
                 (int)ee_won_hostop_num(op, EE_WON_MULTIPING_KEEPS, i), (int)ee_won_hostop_num(op, EE_WON_MULTIPING_KEEPS_ALL, i));
        r += Hex(a) + b;
    }
    return r;
}

// ---- the sequences

static const char* const kAddrs[] = { "\x00\x50\x0a\x00\x00\x01", "\x00\x50\x0a\x00\x00\x02", "\x1f\x90\x0a\x00\x00\x01",
                                      "\x00\x51\xc0\xa8\x01\x07", "\x00\x50\x0a\x00\x00\x09", "\x02\x00\x7f\x00\x00\x01" };
static const char* const kNames[] = { "a.txt", "", "dir/b.bin", "x" };

struct Step {
    int mKind;
    long long mA, mB;
    string mData;
};

// one sequence's step (the args drawn from the step generator)
static Step MakePingStep()
{
    // weighted: adds, the setters, runs, many callbacks (kinds as PingStep's)
    static const int kKinds[] = { 0, 0, 1, 1, 1, 2, 3, 4, 5, 6, 6, 7, 7, 8, 8, 8, 9, 9, 10, 11 };
    Step s;
    s.mKind = kKinds[Pick(20)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: case 1: s.mA = Pick(6); s.mB = Pick(2) ? 0 : 1; break;            // add (addr, entity)
    case 3: { static const int k[] = { 1, 2, 3, 20 }; s.mA = k[Pick(4)]; break; } // batch size
    case 4: s.mA = Pick(4); break;                                           // max failures
    case 5: { static const int k[] = { 0, 100, 3000 }; s.mA = k[Pick(3)]; break; } // max ping time
    case 6: s.mA = Pick(2) + 1; break;                                       // run (mode)
    case 7: case 8: break;                                                   // recv callback
    case 9: s.mA = Pick(4) == 0 ? 1 : 0; break;                              // timeout callback (0 the timer)
    default: break;
    }
    return s;
}
static Step MakePushStep()
{
    static const int kKinds[] = { 0, 0, 1, 2, 3, 4, 4, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 11, 12 };
    Step s;
    s.mKind = kKinds[Pick(22)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: case 1: case 2: s.mData = kNames[Pick(4)]; break;
    case 3: s.mA = Pick(2); break;
    case 4: { static const int k[] = { 1, 5, 16, 65535 }; s.mA = k[Pick(4)]; break; }
    case 5: s.mA = ((long long)Next() << 32) | Next(); break;
    case 6: s.mA = Pick(2); break;
    case 7: s.mA = Pick(2) + 1; s.mB = Pick(3) ? 4 : 2; break;  // run (mode, lfs)
    case 8: case 9: { static const int k[] = { 0, 1, 1, 2, 3, 3, 4, 4, 4, 7 }; s.mA = k[Pick(10)]; break; } // callback id
    default: break;
    }
    return s;
}

static string StepText(char inOp, const Step& s)
{
    char b[200];
    snprintf(b, sizeof(b), "T %c %d %lld %lld %s", inOp, s.mKind, s.mA, s.mB, Hex(s.mData).c_str());
    return b;
}

static int sTimer; // the op's current timer, per half (from its describe)
static int TimerOf(const string& inDescribe)
{
    size_t at = inDescribe.rfind("timer ");
    return at == string::npos ? 0 : atoi(inDescribe.c_str() + at + 6);
}

// runs one ping step on either half; returns the line after "=>"
static string PingStep(OrigPing::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0: case 1: {
        string a(kAddrs[s.mA], 6);
        int entity = 0;
        if (s.mB) entity = NewObj();
        if (o) OrigPing::AddServer(*o, a, entity);
        else RSet(r, EE_WON_MULTIPING_ADD, 0, string(), &a, entity);
        if (entity) S.mOwn[entity]--; // the driver's reference goes
        break;
    }
    case 2: if (o) o->mPingList.clear(); else RSet(r, EE_WON_MULTIPING_CLEAR, 0); break;
    case 3: if (o) o->mBatchSize = (unsigned short)s.mA; else RSet(r, EE_WON_MULTIPING_BATCH_SIZE, s.mA); break;
    case 4: if (o) o->mMaxFailures = (unsigned char)s.mA; else RSet(r, EE_WON_MULTIPING_MAX_FAILURES, s.mA); break;
    case 5: if (o) o->mMaxPingTime = (unsigned)s.mA; else RSet(r, EE_WON_MULTIPING_MAX_PING_TIME, s.mA); break;
    case 6:
        S.mMode = (int)s.mA;
        S.mStatus = 100000;
        if (o) OrigPing::RunHook(*o); else ee_won_hostop_run(r);
        break;
    case 7: case 8: {
        int done = ++S.mSerial;
        bool b = true;
        if (o) OrigPing::RecvCallback(*o, done); else b = ee_won_hostop_callback(r, 1, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 9: {
        int done = s.mA == 0 ? sTimer : ++S.mSerial;
        bool b = true;
        if (o) OrigPing::TimeoutCallback(*o, done); else b = ee_won_hostop_callback(r, 2, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 10: if (o) OrigPing::CleanupHook(*o); else ee_won_hostop_cleanup(r); break;
    case 11: ret = o ? OrigPing::Results(*o) : RPingResults(r); break;
    }
    string d = o ? OrigPing::Describe(*o) : RDescribe(r);
    sTimer = TimerOf(d);
    return ret + "|" + S.mTrace + "|" + d;
}
static string PushStep(OrigPush::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0: if (o) o->mFileName = s.mData; else RSet(r, EE_WON_FILEPUSH_FILE, 0, s.mData); break;
    case 1: if (o) o->mRemoteFileName = s.mData; else RSet(r, EE_WON_FILEPUSH_REMOTE_FILE, 0, s.mData); break;
    case 2: if (o) o->mComment = s.mData; else RSet(r, EE_WON_FILEPUSH_COMMENT, 0, s.mData); break;
    case 3: if (o) o->mIsCompress = s.mA != 0; else RSet(r, EE_WON_FILEPUSH_COMPRESS, s.mA); break;
    case 4: if (o) o->mBlockTransferSize = (unsigned)s.mA; else RSet(r, EE_WON_FILEPUSH_BLOCK_SIZE, s.mA); break;
    case 5:
        if (o) { o->mModifyHigh = (unsigned)((unsigned long long)s.mA >> 32); o->mModifyLow = (unsigned)s.mA; }
        else RSet(r, EE_WON_FILEPUSH_MODIFY_TIME, s.mA);
        break;
    case 6: {
        int c = s.mA ? NewObj() : 0;
        if (o) o->mProgressCompletion = c; else RSet(r, EE_WON_FILEPUSH_PROGRESS, 0, string(), NULL, c);
        if (c) S.mOwn[c]--;
        break;
    }
    case 7:
        S.mMode = (int)s.mA;
        S.mLfs = (int)s.mB;
        S.mStatus = 100000;
        if (o) OrigPush::RunHook(*o); else ee_won_hostop_run(r);
        break;
    case 8: case 9: {
        int done = ++S.mSerial;
        bool b;
        if (o) b = OrigPush::CallbackHook(*o, done, (int)s.mA); else b = ee_won_hostop_callback(r, (int)s.mA, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 10: case 11: if (o) OrigPush::CleanupHook(*o); else ee_won_hostop_cleanup(r); break;
    case 12: {
        if (o) ret = OrigPush::Results(*o);
        else {
            char b[60];
            snprintf(b, sizeof(b), "total %u last %u", (unsigned)ee_won_hostop_num(r, EE_WON_FILEPUSH_TOTAL_SENT, 0),
                     (unsigned)ee_won_hostop_num(r, EE_WON_FILEPUSH_LAST_SENT, 0));
            ret = b;
        }
        break;
    }
    }
    string d = o ? OrigPush::Describe(*o) : RDescribe(r);
    return ret + "|" + S.mTrace + "|" + d;
}

static void Reset(unsigned inSeed)
{
    S.mSeed = inSeed;
    S.mSerial = 0;
    S.mStatus = 0;
    S.mMode = 2;
    S.mLfs = 4;
    S.mLastSocket = 0;
    S.mCalls = 0;
    S.mTick = 0;
    S.mRands.clear();
    S.mAddrs.clear();
    S.mOwn.clear();
    S.mTrace.clear();
    sTimer = 0;
}
static bool Balanced()
{
    for (std::map<int, int>::const_iterator i = S.mOwn.begin(); i != S.mOwn.end(); ++i)
        if (i->second != 0) {
            if (sFailures < 40) printf("object %d: %d references left\n", i->first, i->second);
            return false;
        }
    return true;
}

// one sequence of `inSteps` steps of op 'M' or 'F'; printed (the original's) or compared
static void Sequence(char inOp, int inSteps, bool inPrint)
{
    unsigned aSeed = Next();
    std::vector<Step> steps;
    for (int i = 0; i < inSteps; i++) steps.push_back(inOp == 'M' ? MakePingStep() : MakePushStep());
    string addr(kAddrs[Pick(6)], 6);
    if (inPrint) printf("S %c %u %d %s\n", inOp, aSeed, inSteps, Hex(addr).c_str());
    // the original
    std::vector<string> orig;
    Reset(aSeed);
    if (inOp == 'M') {
        OrigPing::Op p;
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(PingStep(&p, NULL, steps[i]));
    } else {
        OrigPush::Op f(addr);
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(PushStep(&f, NULL, steps[i]));
    }
    if (inPrint) {
        for (size_t i = 0; i < steps.size(); i++) printf("%s => %s\n", StepText(inOp, steps[i]).c_str(), orig[i].c_str());
        return;
    }
    // the Rust op
    Reset(aSeed);
    EeWonHostOp* r = inOp == 'M' ? ee_won_multiping_new() : ee_won_filepush_new(reinterpret_cast<const uint8_t*>(addr.data()));
    ee_won_hostop_bind(r, HostFn, NULL);
    for (size_t i = 0; i < steps.size(); i++) {
        string got = inOp == 'M' ? PingStep(NULL, r, steps[i]) : PushStep(NULL, r, steps[i]);
        CHECK(got == orig[i]);
        if (got != orig[i] && sFailures < 6)
            printf("%s\n  orig %s\n  rust %s\n", StepText(inOp, steps[i]).c_str(), orig[i].c_str(), got.c_str());
    }
    ee_won_hostop_free(r);
    CHECK(Balanced());
}

int main(int argc, char** argv)
{
    int aPrint = 0, aCount = 20000;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--print") == 0) aPrint = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--count") == 0) aCount = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--seed") == 0) sSeed = (unsigned)atoi(argv[i + 1]);
    }
    if (aPrint > 0) {
        for (int i = 0; i < aPrint; i++) Sequence('M', 8 + Pick(40), true);
        for (int i = 0; i < aPrint; i++) Sequence('F', 8 + Pick(40), true);
        return 0;
    }
    for (int i = 0; i < aCount; i++) Sequence('M', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('F', 8 + Pick(40), false);
    printf("won_hostop_test: %lld checks, %d failed\n", sChecks, sFailures);
    return sFailures;
}
