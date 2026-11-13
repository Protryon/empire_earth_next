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
#include "won_ftp_msg.h"
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
    int mLine; // the next line of the session LineBytes gives
    bool mHttp; // an HTTP get's sequence (its reply lines: HttpLine)
    bool mDoc; // an HTTP document's sequence (its downloads' statuses: invalid redirects at times)
    bool mReq; // a request exchange's sequence (its own answers: ReqBytes, the peer auth's statuses)
    int mAuth; // the request's mAuthType
    std::vector<int> mRands;
    std::vector<string> mAddrs;
    std::map<int, int> mOwn;  // the op's own references per object (NEW_*, ADD_REF, RELEASE)
    std::map<int, int> mKinds; // each socket op's kind (its bytes: a line for a RecvCRMsgOp)
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
    char aBuffer[4096];
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
// a RecvCRMsgOp's line (NUL ended): FTP replies of every kind
static string LineBytes(bool* outNull)
{
    static const char* const k[] = { "220 ready", "331 need password", "230 logged in", "250 ok", "200 type set",
                                     "227 Entering Passive Mode (10,0,0,1,4,1)", "350 restarting", "504 not implemented",
                                     "150 opening", "226 done", "530 not logged in", "220-hello", "227 bad", "" };
    // mostly the next line of a successful session (so the steps reach the data connection), else any
    static const int kSession[] = { 0, 1, 2, 3, 4, 5, 6, 8, 9 };
    int i = APick(5) != 0 ? kSession[S.mLine++ % 9] : APick(15);
    *outNull = i == 14;
    if (*outNull) return string();
    return string(k[i]) + '\0';
}
// an HTTP reply's lines (NUL ended), biased to a session: the status first, then headers, blank lines, chunk
// lengths
static string HttpLine(bool* outNull)
{
    static const char* const kStatus[] = { "HTTP/1.1 200 OK", "HTTP/1.1 206 Partial", "HTTP/1.1 301 Moved",
                                           "HTTP/1.0 302 Found", "HTTP/1.1 304 Not Modified", "HTTP/1.1 404 Not Found",
                                           "HTTP/1.1 500 Error", "garbage" };
    static const char* const kHeaders[] = {
        "Content-Length: 12", "Content-Type: text/plain", "Last-Modified: Sun, 06 Nov 1994 08:49:37 GMT",
        "Location: http://www.sierra.com/motd/x.txt", "Location: http://other", "Location: nowhere",
        "Transfer-Encoding: chunked", "Connection: close", "Date: Sun, 06 Nov 1994 08:49:37 GMT",
        "Expires: Sun, 06 Nov 1994 09:49:37 GMT", "ETag: \"abc\"x", "ETag: none", "X-Other: y", "no colon" };
    static const char* const kChunks[] = { "c", "0", "1a;ext", "zz", "" };
    int k = APick(10);
    *outNull = k == 0 && APick(3) == 0;
    if (*outNull) return string();
    string l;
    if (S.mLine == 0 || k == 1) l = kStatus[APick(8)];
    else if (k < 6) l = kHeaders[APick(14)];
    else if (k < 8) l = "";
    else l = kChunks[APick(5)];
    S.mLine++;
    return l + '\0';
}
// a request exchange's reply: a reused session's "expired", a ping reply, anything
static string ReqBytes(bool* outNull)
{
    int k = APick(6);
    if (k == 0) {
        *outNull = false;
        const char r[10] = { 1, 0, 0, 0, 15, 0, 0, 0, (char)APick(3), 0 };
        return string(r, 10);
    }
    if (k == 1 || k == 2) {
        *outNull = false;
        unsigned t = S.mTick - APick(50);
        const char r[] = { 3, 1, 6, (char)(t & 0xff), (char)((t >> 8) & 0xff), (char)((t >> 16) & 0xff),
                           (char)(t >> 24), (char)APick(2) };
        return string(r, k == 1 ? 7 : 8);
    }
    if (k == 3) { // a DB proxy reply (the user id lookup's), the last one or not
        *outNull = false;
        const char r[] = { 5, 14, 0, (char)0xea, 3, 17, 0, 0, 0, 0, 0, (char)(APick(3) ? 0x80 : 0), 7, 0, 0, 0 };
        string b(r, sizeof(r));
        if (APick(4) == 0) b.resize(APick((int)sizeof(r)));
        return b;
    }
    return Bytes(outNull);
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
        S.mKinds[id] = (int)n0;
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
        std::map<int, int>::const_iterator k = S.mKinds.find(o0);
        int v = (k != S.mKinds.end() && k->second == EE_WON_HOST_OP_RECV_CR_MSG) ? SendStatus() : RecvStatus();
        if (S.mReq && APick(6) == 0) v = 100053; // WS_PeerAuth_GetCertFailure
        if (S.mDoc && APick(4) == 0) v = 100094; // WS_HTTP_InvalidRedirect
        c->out_num = v;
        Log("st#%d=%d", o0, v);
        break;
    }
    case EE_WON_HOST_OP_BYTES: {
        bool isNull;
        std::map<int, int>::const_iterator k = S.mKinds.find(o0);
        string b = S.mHttp ? HttpLine(&isNull)
                   : (k != S.mKinds.end() && k->second == EE_WON_HOST_OP_RECV_CR_MSG) ? LineBytes(&isNull)
                   : S.mReq ? ReqBytes(&isNull) : Bytes(&isNull);
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
        string b = c->code == EE_WON_HOST_RECV_BYTES ? AckBytes(&isNull) : S.mReq ? ReqBytes(&isNull) : StatusBytes(&isNull);
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
    case EE_WON_HOST_OP_KILLED: {
        int v = APick(6) == 0 ? 1 : 0;
        c->out_num = v;
        Log("killed#%d=%d", o0, v);
        break;
    }
    case EE_WON_HOST_RESOLVE: {
        string a = Addr();
        memcpy(c->out_addr, a.data(), 6);
        c->out_num = 1;
        Log("resolve %s %d=%s", Hex(data).c_str(), (int)n0, Hex(a).c_str());
        break;
    }
    case EE_WON_HOST_CONNECT: {
        int v = SendStatus();
        c->out_num = v;
        Log("connect#%d %s t%u=%d", o0, Hex(addr).c_str(), (unsigned)n0, v);
        break;
    }
    case EE_WON_HOST_RECV_PROGRESS: Log("progress#%d c%d %d", o0, o1, (int)n0); break;
    case EE_WON_HOST_RECV_FILE: Log("rfile#%d %s %d", o0, Hex(data).c_str(), (int)n0); break;
    case EE_WON_HOST_FILE_SIZE: {
        int v = APick(3) == 0 ? -1 : APick(5000);
        c->out_num = v;
        Log("fsize %s=%d", Hex(data).c_str(), v);
        break;
    }
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
    case EE_WON_HOST_AUTH_TYPE: c->out_num = S.mAuth; break;
    case EE_WON_HOST_HAS_AUTH: c->out_num = APick(5) != 0; Log("hasauth=%d", (int)c->out_num); break;
    case EE_WON_HOST_CONNECT_TIME:
    case EE_WON_HOST_TIMEOUT: {
        unsigned v = APick(2) ? 20000 : 0;
        c->out_num = v;
        Log("%s=%u", c->code == EE_WON_HOST_TIMEOUT ? "timeout" : "ctime", v);
        break;
    }
    case EE_WON_HOST_CERT_USER_ID: c->out_num = ANext(); Log("uid=%u", (unsigned)c->out_num); break;
    case EE_WON_HOST_CTX_ADDRS: {
        int n = APick(4);
        string b;
        for (int i = 0; i < n; i++) b += Addr();
        Give(c, b, n == 0 && APick(2) == 0);
        Log("addrs#%d=%s", o0, Hex(b).c_str());
        break;
    }
    case EE_WON_HOST_CTX_FAILED: Log("failed#%d %s", o0, Hex(addr).c_str()); break;
    case EE_WON_HOST_GET_SESSION:
    case EE_WON_HOST_PEER_AUTH_SESSION: {
        int id = APick(3) == 0 ? 0 : NewObj();
        c->out_obj = P(id);
        if (c->code == EE_WON_HOST_GET_SESSION) Log("session %s=%d", Hex(addr).c_str(), id);
        else Log("psession#%d=%d", o0, id);
        break;
    }
    case EE_WON_HOST_PUT_SESSION: Log("put %s %d", Hex(addr).c_str(), o0); break;
    case EE_WON_HOST_ADD_CRYPT: Log("crypt#%d %d", o0, o1); break;
    case EE_WON_HOST_KILL_ALL: Log("killall"); break;
    case EE_WON_HOST_PEER_AUTH_CERT: {
        static const int k[] = { 0, 100001, 100062, 100072 };
        c->out_num = k[APick(4)];
        Log("cert#%d=%d", o0, (int)c->out_num);
        break;
    }
    case EE_WON_HOST_OP_IS_RECV_MSG: c->out_num = APick(4) != 0; Log("isrecv#%d=%d", o0, (int)c->out_num); break;
    case EE_WON_HOST_SOCKET_CONNECTED: c->out_num = APick(3) == 0; Log("conn?#%d=%d", o0, (int)c->out_num); break;
    case EE_WON_HOST_SOCKET_KILL: Log("skill#%d", o0); break;
    case EE_WON_HOST_SOCKET_STATUS: {
        c->out_num = APick(2) ? 100004 : 100001;
        Log("sstat#%d=%d", o0, (int)c->out_num);
        break;
    }
    case EE_WON_HOST_RECV_CR: {
        int v = SendStatus();
        bool isNull;
        string b = HttpLine(&isNull);
        c->out_num = v;
        Give(c, b, isNull);
        Log("recvcr#%d t%u=%d %s", o0, (unsigned)n0, v, BytesLog(b, isNull).c_str());
        break;
    }
    case EE_WON_HOST_FILE_STAT: {
        static const long long k[] = { 0, 12, 4096, 784111777 };
        long long v = n0 == 0 ? APick(3) != 0 : k[APick(4)];
        c->out_num = v;
        Log("stat %s %d=%lld", Hex(data).c_str(), (int)n0, v);
        break;
    }
    case EE_WON_HOST_TIME: {
        long long v = 784111000 + APick(9000);
        c->out_num = v;
        Log("time=%lld", v);
        break;
    }
    case EE_WON_HOST_CACHE_ENTRY: {
        int id = APick(3) == 0 ? 0 : NewObj();
        c->out_obj = P(id);
        Log("entry#%d %s=%d", o0, Hex(data).c_str(), id);
        break;
    }
    case EE_WON_HOST_ENTRY_GET:
        if (n0 == 0 || n0 == 1) {
            long long v = n0 == 0 ? APick(2) : 784111000 + APick(9000);
            c->out_num = v;
            Log("eget#%d %d=%lld", o0, (int)n0, v);
        } else {
            static const char* const k[] = { "", "_cache1.txt", "\"tag\"" };
            string b = k[APick(3)];
            Give(c, b, false);
            Log("eget#%d %d=%s", o0, (int)n0, Hex(b).c_str());
        }
        break;
    case EE_WON_HOST_ENTRY_SET: Log("eset#%d %d %lld %s", o0, (int)n0, n1, Hex(data).c_str()); break;
    case EE_WON_HOST_CACHE_UPDATE: Log("update#%d %d", o0, o1); break;
    case EE_WON_HOST_RECV_SKIP: Log("skip#%d", o0); break;
    case EE_WON_HOST_RECV_BINARY: Log("binary#%d %d", o0, (int)n0); break;
    case EE_WON_HOST_RECV_MODIFY_TIME: Log("mtime#%d %lld", o0, n0); break;
    case EE_WON_HOST_HTTP_PROXY: {
        static const char* const k[] = { "", "", "proxy:8080" };
        string b = k[APick(3)];
        Give(c, b, false);
        Log("proxy=%s", Hex(b).c_str());
        break;
    }
    case EE_WON_HOST_NEW_HTTP_GET: {
        int id = NewObj();
        c->out_obj = P(id);
        Log("http#%d %d %s", id, (int)n0, Hex(data).c_str());
        break;
    }
    case EE_WON_HOST_HOSTOP_NUM: {
        long long v = APick(3) == 0 ? (long long)ANext() : APick(4) == 0 ? 404 : APick(3);
        c->out_num = v;
        Log("cnum#%d %d=%lld", o0, (int)n0, v);
        break;
    }
    case EE_WON_HOST_HOSTOP_BYTES: {
        bool isNull = APick(5) == 0;
        string b = isNull ? string() : APick(2) ? string() : string("x.txt");
        Give(c, b, isNull);
        Log("cbytes#%d %d=%s", o0, (int)n0, BytesLog(b, isNull).c_str());
        break;
    }
    case EE_WON_HOST_HOSTOP_SET: Log("cset#%d %d %lld %s o%d", o0, (int)n0, n1, Hex(data).c_str(), o1); break;
    case EE_WON_HOST_HTTP_MODIFIED: {
        long long v = APick(3) == 0 ? 0 : 784111000 + APick(9000);
        c->out_num = v;
        Log("hmod#%d=%lld", o0, v);
        break;
    }
    case EE_WON_HOST_FILE_FIRST_CHAR: {
        static const int k[] = { -2, -1, '1', '0', '<' };
        int v = k[APick(5)];
        c->out_num = v;
        Log("fgetc %s=%d", Hex(data).c_str(), v);
        break;
    }
    case EE_WON_HOST_SESSION_ADD: Log("sadd#%d %d", o0, o1); break;
    case EE_WON_HOST_TRACKER_EMPTY: c->out_num = APick(2); Log("tempty=%d", (int)c->out_num); break;
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

// ---- the original's statements: FTPGetOp.cpp before the port (FtpMsg, won_ftp_msg.h, over the op stand-in; the
// members the constructors left unset 0)

namespace OrigFtp {
enum { FTP_Track_RequestConnect = 1, FTP_Track_Request = 2, FTP_Track_DataConnect = 3, FTP_Track_RecvContent = 4 };
struct Op {
    enum RequestState { RequestState_Start, RequestState_User, RequestState_Password, RequestState_ChangeDir,
                        RequestState_ChangeType, RequestState_Passive, RequestState_Restart, RequestState_Retrieve };
    int mRequestSocket, mDataSocket;
    string mHost, mRemotePath, mLocalPath, mUserName, mPassword, mActualPath, mActualFile;
    bool mHasContent;
    string mContent;
    int mFTPStatus;
    int mRecvChunkCompletion;
    unsigned mRecvChunkSize;
    IPAddr mDataAddr;
    bool mIsBinary, mDoResume;
    unsigned mLocalFileSize;
    RequestState mRequestState;
    Op() : mRequestSocket(0), mDataSocket(0), mHasContent(false), mFTPStatus(0), mRecvChunkCompletion(0),
           mRecvChunkSize(0), mIsBinary(true), mDoResume(false), mLocalFileSize(0), mRequestState(RequestState_Start)
    {
        mUserName = "anonymous";
        mPassword = "anonymous";
        mDataAddr.SetSixByte("\0\0\0\0\0\0");
    }
    void SetUserName(const string& s) { mUserName = s; }
    void SetPassword(const string& s) { mPassword = s; }
    int GetMode() { return (int)Do(EE_WON_HOST_MODE).mNum; }
    bool IsAsync() { return GetMode() == 1; }
    unsigned TimeLeft() { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
    void Finish(int s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
    int Track(int inOp, int inId) { Do(EE_WON_HOST_TRACK, inOp, 0, inId); return inOp; }
    void RecvLineAsync();
    bool SendRequest();
    bool DoDataConnection();
    void RecvContent();
};
static string Six(const IPAddr& a) { return string(a.GetSixByte(), 6); }
static bool CheckStatus(Op& o, int theStatus)
{
    if (theStatus == 0)
        return true;
    o.Finish(theStatus);
    return false;
}
static bool DoConnect(Op& o)
{
    string anAddr = Do(EE_WON_HOST_RESOLVE, 0, 0, 21, 0, o.mHost).mAddr; // IPAddr::SetWithDefaultPort(mHost, 21)
    if (o.GetMode() == 1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_CONNECT, 0, string(), anAddr).mObj;
        Do(EE_WON_HOST_QUEUE, o.mRequestSocket, o.Track(anOp, FTP_Track_RequestConnect), 0xffffffffu);
        return false;
    }
    unsigned aTime = o.TimeLeft();
    int aStatus = (int)Do(EE_WON_HOST_CONNECT, o.mRequestSocket, 0, aTime, 0, string(), anAddr).mNum;
    return CheckStatus(o, aStatus);
}
void Op::RecvLineAsync()
{
    int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_RECV_CR_MSG).mObj;
    Do(EE_WON_HOST_QUEUE, this->mRequestSocket, this->Track(anOp, FTP_Track_Request), 0xffffffffu);
}
bool Op::SendRequest()
{
    std::string aReq = FtpMsg<Op>::Command(*this);
    if (this->GetMode() == 1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_SEND_BYTES, 0, aReq).mObj;
        Do(EE_WON_HOST_QUEUE, this->mRequestSocket, this->Track(anOp, 0), 0xffffffffu);
        this->RecvLineAsync();
        return false;
    }
    unsigned aTime = this->TimeLeft();
    Do(EE_WON_HOST_SEND_BYTES, this->mRequestSocket, 0, aTime, 0, aReq);
    return true;
}
static bool HandleReply(Op& o, int theReply)
{
    if (!CheckStatus(o, (int)Do(EE_WON_HOST_OP_STATUS, theReply).mNum))
        return false;
    string aLine = Do(EE_WON_HOST_OP_BYTES, theReply).mBytes;
    return FtpMsg<Op>::HandleReply(o, aLine.data(), (int)aLine.length());
}
bool Op::DoDataConnection()
{
    if (this->GetMode() == 1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_CONNECT, 0, string(), Six(this->mDataAddr)).mObj;
        Do(EE_WON_HOST_QUEUE, this->mDataSocket, this->Track(anOp, FTP_Track_DataConnect), 0xffffffffu);
        return false;
    }
    unsigned aTime = this->TimeLeft();
    int aStatus = (int)Do(EE_WON_HOST_CONNECT, this->mDataSocket, 0, aTime, 0, string(), Six(this->mDataAddr)).mNum;
    if (!CheckStatus(*this, aStatus))
        return false;
    return true;
}
void Op::RecvContent()
{
    int aRecvBytes = Do(EE_WON_HOST_NEW_OP, this->mDataSocket, 0, EE_WON_HOST_OP_RECV_BYTES, 0).mObj;
    if (this->mRecvChunkCompletion != 0)
        Do(EE_WON_HOST_RECV_PROGRESS, aRecvBytes, this->mRecvChunkCompletion, this->mRecvChunkSize);
    if (!this->mLocalPath.empty())
        Do(EE_WON_HOST_RECV_FILE, aRecvBytes, 0, this->mDoResume ? 1 : 0, 0, this->mLocalPath);
    if (this->GetMode() == 1) {
        this->Track(aRecvBytes, FTP_Track_RecvContent);
        Do(EE_WON_HOST_RUN_ASYNC, aRecvBytes, 0, 0xffffffffu);
    } else {
        unsigned aTime = this->TimeLeft();
        if (Do(EE_WON_HOST_RUN_BLOCK, aRecvBytes, 0, aTime).mNum != 0) {
            if (this->mLocalPath.empty()) {
                Res r = Do(EE_WON_HOST_OP_BYTES, aRecvBytes);
                this->mHasContent = !r.mNull;
                this->mContent = r.mBytes;
            }
        }
        this->Finish((int)Do(EE_WON_HOST_OP_STATUS, aRecvBytes).mNum);
    }
}
static bool CallbackHook(Op& o, int theOp, int theParam)
{
    if (Do(EE_WON_HOST_OP_KILLED, theOp).mNum != 0) {
        Do(EE_WON_HOST_KILL, 0, 0, 1);
        return true;
    }
    if (!((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum == 0)) {
        o.Finish((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum);
        return true;
    }
    switch (theParam) {
    case FTP_Track_RequestConnect: o.RecvLineAsync(); break;
    case FTP_Track_DataConnect: o.SendRequest(); break;
    case FTP_Track_Request: HandleReply(o, theOp); break;
    case FTP_Track_RecvContent:
        if (o.mLocalPath.empty() && ((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum == 0)) {
            Res r = Do(EE_WON_HOST_OP_BYTES, theOp);
            o.mHasContent = !r.mNull;
            o.mContent = r.mBytes;
        }
        o.Finish((int)Do(EE_WON_HOST_OP_STATUS, theOp).mNum);
        break;
    default: return false;
    }
    return true;
}
static void RunHook(Op& o)
{
    o.mHasContent = false;
    o.mContent.clear();
    o.mFTPStatus = 0;
    if (o.mDoResume) {
        long long aSize = Do(EE_WON_HOST_FILE_SIZE, 0, 0, 0, 0, o.mLocalPath).mNum; // stat
        if (aSize >= 0)
            o.mLocalFileSize = (unsigned)aSize;
        else
            o.mDoResume = false;
    }
    o.mRequestSocket = Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 0).mObj;
    o.mDataSocket = Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 0).mObj;
    o.mRequestState = Op::RequestState_Start;
    if (!DoConnect(o))
        return;
    while (true) {
        int aRecvOp = Do(EE_WON_HOST_NEW_OP, o.mRequestSocket, 0, EE_WON_HOST_OP_RECV_CR_MSG).mObj;
        Do(EE_WON_HOST_RUN_BLOCK, aRecvOp, 0, o.TimeLeft());
        if (!HandleReply(o, aRecvOp))
            return;
    }
}
static void CleanupHook(Op& o)
{
    Do(EE_WON_HOST_BASE_CLEANUP);
    if (o.mRequestSocket != 0) {
        Do(EE_WON_HOST_CLOSE, o.mRequestSocket);
        o.mRequestSocket = 0;
    }
    if (o.mDataSocket != 0) {
        Do(EE_WON_HOST_CLOSE, o.mDataSocket);
        o.mDataSocket = 0;
    }
}
static string Describe(const Op& o)
{
    char b[400];
    snprintf(b, sizeof(b), " st %d chunk %d:%u data %s bin %d resume %d size %u state %d rsock %d dsock %d", o.mFTPStatus,
             o.mRecvChunkCompletion, o.mRecvChunkSize, Hex(Six(o.mDataAddr)).c_str(), o.mIsBinary ? 1 : 0,
             o.mDoResume ? 1 : 0, o.mLocalFileSize, (int)o.mRequestState, o.mRequestSocket, o.mDataSocket);
    return "host " + Hex(o.mHost) + " remote " + Hex(o.mRemotePath) + " local " + Hex(o.mLocalPath) + " user " +
           Hex(o.mUserName) + " pass " + Hex(o.mPassword) + " path " + Hex(o.mActualPath) + " file " +
           Hex(o.mActualFile) + " content " + (o.mHasContent ? Hex(o.mContent) : string("null")) + b;
}
static string Results(const Op& o)
{
    char b[40];
    snprintf(b, sizeof(b), "st %d ", o.mFTPStatus);
    return b + (o.mHasContent ? Hex(o.mContent) : string("null")) + " " + Hex(o.mLocalPath) + " " + Hex(o.mRemotePath);
}
}

// ---- the original's statements: HTTPGetOp.cpp before the port (WONHttpMsg.h's helpers over the op stand-in; the
// redirect's child op, its setters and its results through the host calls the Rust op makes; the members the
// constructors left unset 0)

namespace OrigHttp {
enum { HTTP_Track_Connect = 1, HTTP_Track_RecvStatus = 2, HTTP_Track_RecvHeader = 3, HTTP_Track_RecvContent = 4,
       HTTP_Track_Redirect = 5, HTTP_Track_RecvChunkLength = 6, HTTP_Track_RecvChunkCRLF = 7,
       HTTP_Track_RecvFooter = 8, HTTP_Track_SendRequest = 9 };
struct Op {
    int mSocket, mHTTPCache, mHTTPCacheEntry;
    string mHost, mRemotePath, mLocalPath;
    bool mContentIsChunked, mIsFirstChunk, mBinaryFile, mCloseConnection, mDoClose, mDoRangeResume;
    string mProxyHost, mRedirectHost, mRedirectPath;
    long long mServerTime, mExpireTime, mLastModified, mLocalFileModifyTime;
    unsigned long long mLocalFileSize;
    string mEntityTag, mContentType;
    unsigned mContentLength, mChunkLength;
    bool mHasContent;
    string mContent;
    string mContentAccumulator;
    int mHTTPStatus;
    bool mContentIsNew;
    int mRecvChunkCompletion;
    unsigned mRecvChunkSize;
    bool mOnlySend, mOnlyRecv, mAutoRedirect;
    int mNumRedirects;
    Op() : mSocket(0), mHTTPCache(0), mHTTPCacheEntry(0), mContentIsChunked(false), mIsFirstChunk(false),
           mBinaryFile(true), mCloseConnection(true), mDoClose(false), mDoRangeResume(true), mServerTime(0),
           mExpireTime(0), mLastModified(0), mLocalFileModifyTime(0), mLocalFileSize(0), mContentLength(0),
           mChunkLength(0), mHasContent(false), mHTTPStatus(0), mContentIsNew(false), mRecvChunkCompletion(0),
           mRecvChunkSize(0), mOnlySend(false), mOnlyRecv(false), mAutoRedirect(true), mNumRedirects(0) {}
    int GetMode() { return (int)Do(EE_WON_HOST_MODE).mNum; }
    bool IsAsync() { return GetMode() == 1; }
    unsigned TimeLeft() { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
    void Finish(int s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
    int Track(int inOp, int inId) { Do(EE_WON_HOST_TRACK, inOp, 0, inId); return inOp; }
    bool GetOnlySend() { return mOnlySend; }
    void SetProxy(const string& s) { mProxyHost = s; }
    // the helpers' calls (WONHttpMsg.h)
    void DoFinish();
    bool CheckStatus(int inStatus);
    bool ExtractStatus(const char* inLine) { return HttpExtractStatus(*this, inLine); }
    bool ExtractRedirect(const char* inLocation) { return HttpExtractRedirect(*this, inLocation); }
    bool ExtractHeaderLine(const char* inLine) { return HttpExtractHeaderLine(*this, inLine); }
    bool DoConnect();
    bool SendRequest();
    void AsyncRecvCRMsg(int inType);
    bool ExtractChunkLength(const char* inLine);
    bool RecvChunkLength();
    bool DoneRecvContent(int inOp);
    void RecvContent();
    bool CallbackHook(int inOp, int inParam);
    void FinishRedirect(int inOp);
    void FollowRedirect();
    void DoneRecvHeader();
    void RunHook();
    void CleanupHook();
};
// a line of a socket's or an op's (NULL, which the original dereferenced: empty), NUL ended for the helpers
static string LineOf(const Res& r) { return r.mNull ? string(1, '\0') : r.mBytes + '\0'; }
static size_t LengthOf(const Res& r) { return r.mNull ? 0 : r.mBytes.size(); }
static int EntryNum(int inEntry, int inWhat) { return (int)Do(EE_WON_HOST_ENTRY_GET, inEntry, 0, inWhat).mNum; }
static long long EntryTime(int inEntry) { return Do(EE_WON_HOST_ENTRY_GET, inEntry, 0, 1).mNum; }
static string EntryBytes(int inEntry, int inWhat) { return Do(EE_WON_HOST_ENTRY_GET, inEntry, 0, inWhat).mBytes; }
static long long Now() { return Do(EE_WON_HOST_TIME).mNum; }
void Op::DoFinish()
{
    if (this->mHTTPStatus>304) this->Finish(100093);
    else this->Finish(0);
}
bool Op::CheckStatus(int inStatus)
{
    if (inStatus==0) return true;
    this->Finish(inStatus);
    return false;
}
bool Op::DoConnect()
{
    if (Do(EE_WON_HOST_SOCKET_CONNECTED, this->mSocket).mNum != 0)
        return true;
    string anAddress;
    if (!this->mProxyHost.empty())
        anAddress = Do(EE_WON_HOST_RESOLVE, 0, 0, 80, 0, this->mProxyHost).mAddr;
    else
        anAddress = Do(EE_WON_HOST_RESOLVE, 0, 0, 80, 0, this->mHost).mAddr;
    if (this->GetMode()==1) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_CONNECT, 0, string(), anAddress).mObj;
        Do(EE_WON_HOST_QUEUE, this->mSocket, this->Track(anOp, HTTP_Track_Connect), 0xffffffffu);
        return false;
    }
    else {
        unsigned aTime = this->TimeLeft();
        return this->CheckStatus((int)Do(EE_WON_HOST_CONNECT, this->mSocket, 0, aTime, 0, string(), anAddress).mNum);
    }
}
bool Op::SendRequest()
{
    bool aFile=false;
    if (!this->mLocalPath.empty()) {
        if (Do(EE_WON_HOST_FILE_STAT, 0, 0, 0, 0, this->mLocalPath).mNum != 0) { // WONFile::Exists
            this->mLocalFileSize=(unsigned long long)Do(EE_WON_HOST_FILE_STAT, 0, 0, 1, 0, this->mLocalPath).mNum;
            this->mLocalFileModifyTime=Do(EE_WON_HOST_FILE_STAT, 0, 0, 2, 0, this->mLocalPath).mNum;
            aFile=true;
        }
    }
    bool aCacheOk = this->mHTTPCacheEntry==0 || EntryNum(this->mHTTPCacheEntry, 0) != 0;
    ByteBufferPtr aRequest=HttpRequestBytes(this->mProxyHost.length()!=0,this->mHost,this->mRemotePath,
                                            this->mCloseConnection,aFile,(size_t)this->mLocalFileSize,
                                            (time_t)this->mLocalFileModifyTime,this->mDoRangeResume,this->mEntityTag,
                                            aCacheOk);
    string aReq(aRequest->data(), aRequest->length());
    if (this->IsAsync()) {
        int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_SEND_BYTES, 0, aReq).mObj;
        Do(EE_WON_HOST_QUEUE, this->mSocket, this->Track(anOp, HTTP_Track_SendRequest), 0xffffffffu);
        if (!this->GetOnlySend())
            this->AsyncRecvCRMsg(HTTP_Track_RecvStatus);
        return false;
    }
    else {
        unsigned aTime = this->TimeLeft();
        Do(EE_WON_HOST_SEND_BYTES, this->mSocket, 0, aTime, 0, aReq);
    }
    return true;
}
void Op::AsyncRecvCRMsg(int inType)
{
    int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_RECV_CR_MSG).mObj;
    Do(EE_WON_HOST_QUEUE, this->mSocket, this->Track(anOp, inType), 0xffffffffu);
}
bool Op::ExtractChunkLength(const char* inLine)
{
    uint32_t aChunkLength=0;
    if (ee_won_http_chunk_length(reinterpret_cast<const uint8_t*>(inLine),&aChunkLength)) {
        this->mChunkLength=aChunkLength;
        this->mContentLength+=this->mChunkLength;
        if (this->mChunkLength==0) {
            if (this->mHTTPCacheEntry!=0)
                Do(EE_WON_HOST_ENTRY_SET, this->mHTTPCacheEntry, 0, 0, 1); // SetValid(true)
            if (this->mLocalPath.empty()) {
                this->mHasContent=true;
                this->mContent=this->mContentAccumulator;
                this->mContentAccumulator.clear();
            }
            if (this->GetMode()==1) {
                this->AsyncRecvCRMsg(HTTP_Track_RecvFooter);
                return false;
            }
            while (true) {
                unsigned aTime = this->TimeLeft();
                Res aFooter = Do(EE_WON_HOST_RECV_CR, this->mSocket, 0, aTime);
                if (!this->CheckStatus((int)aFooter.mNum))
                    return false;
                if (!aFooter.mNull && aFooter.mBytes.size()==1)
                    break;
            }
            this->DoFinish();
            return false;
        }
        return true;
    }
    this->Finish(100092);
    return false;
}
bool Op::RecvChunkLength()
{
    if (this->mIsFirstChunk)
        this->mContentLength=0;
    if (this->GetMode()==1) {
        if (!this->mIsFirstChunk)
            this->AsyncRecvCRMsg(HTTP_Track_RecvChunkCRLF);
        this->AsyncRecvCRMsg(HTTP_Track_RecvChunkLength);
        return false;
    }
    if (!this->mIsFirstChunk) {
        unsigned aTime = this->TimeLeft();
        if (!this->CheckStatus((int)Do(EE_WON_HOST_RECV_CR, this->mSocket, 0, aTime).mNum))
            return false;
    }
    unsigned aTime = this->TimeLeft();
    Res aLine = Do(EE_WON_HOST_RECV_CR, this->mSocket, 0, aTime);
    if (!this->CheckStatus((int)aLine.mNum))
        return false;
    return this->ExtractChunkLength(LineOf(aLine).c_str());
}
static int OpStatus(int inOp) { return (int)Do(EE_WON_HOST_OP_STATUS, inOp).mNum; }
bool Op::DoneRecvContent(int inOp)
{
    this->mIsFirstChunk=false;
    if (this->mLocalPath.empty() && OpStatus(inOp)==0) {
        Res b = Do(EE_WON_HOST_OP_BYTES, inOp);
        if (this->mContentIsChunked)
            this->mContentAccumulator+=b.mBytes; // AppendBuffer: NULL appends nothing
        else {
            this->mHasContent=!b.mNull;
            this->mContent=b.mBytes;
        }
    }
    if (OpStatus(inOp)!=0) {
        this->Finish(OpStatus(inOp));
        return false;
    }
    if (this->mContentIsChunked)
        return this->RecvChunkLength();
    if (this->mHTTPCacheEntry!=0)
        Do(EE_WON_HOST_ENTRY_SET, this->mHTTPCacheEntry, 0, 0, 1);
    this->DoFinish();
    return false;
}
void Op::RecvContent()
{
    if (this->mHTTPStatus==304) {
        this->mContentIsNew=false;
        this->DoFinish();
        return;
    }
    bool skip;
    if (this->mHTTPStatus>=200 && this->mHTTPStatus<300) skip=false;
    else {
        skip=true;
        if (this->mDoClose) {
            this->Finish(100093);
            return;
        }
    }
    for (;;) {
        unsigned long length=this->mContentLength;
        if (this->mContentIsChunked) length=this->mChunkLength;
        int recv = Do(EE_WON_HOST_NEW_OP, this->mSocket, 0, EE_WON_HOST_OP_RECV_BYTES, (long long)length).mObj;
        if (skip) Do(EE_WON_HOST_RECV_SKIP, recv, 0, 1);
        else {
            if (this->mRecvChunkCompletion!=0)
                Do(EE_WON_HOST_RECV_PROGRESS, recv, this->mRecvChunkCompletion, this->mRecvChunkSize);
            if (this->mHTTPCacheEntry!=0) Do(EE_WON_HOST_ENTRY_SET, this->mHTTPCacheEntry, 0, 0, 0);
            if (!this->mLocalPath.empty()) {
                bool append=this->mHTTPStatus==206 || (this->mContentIsChunked && !this->mIsFirstChunk);
                if (ee_won_http_up_to_date(append,this->mLocalFileSize,this->mContentLength,
                                           this->mLocalFileModifyTime,this->mLastModified,this->mDoClose)) {
                    this->DoFinish();
                    this->mContentIsNew=false;
                    return;
                }
                Do(EE_WON_HOST_RECV_BINARY, recv, 0, this->mBinaryFile ? 1 : 0);
                Do(EE_WON_HOST_RECV_FILE, recv, 0, append ? 1 : 0, 0, this->mLocalPath);
                Do(EE_WON_HOST_RECV_MODIFY_TIME, recv, 0, this->mLastModified);
            }
        }
        if (this->GetMode()==1) {
            this->Track(recv,HTTP_Track_RecvContent);
            Do(EE_WON_HOST_RUN_ASYNC, recv, 0, 0xffffffffu);
            return;
        }
        unsigned aTime = this->TimeLeft();
        if (Do(EE_WON_HOST_RUN_BLOCK, recv, 0, aTime).mNum != 0) {
            if (!this->DoneRecvContent(recv)) return;
        } else {
            this->Finish(OpStatus(recv));
            return;
        }
    }
}
bool Op::CallbackHook(int inOp,int inParam)
{
    if (Do(EE_WON_HOST_OP_KILLED, inOp).mNum != 0) {
        Do(EE_WON_HOST_KILL, 0, 0, 1);
        return true;
    }
    if (OpStatus(inOp)!=0) {
        this->Finish(OpStatus(inOp));
        return true;
    }
    switch (inParam) {
    case HTTP_Track_Connect:
        this->SendRequest();
        break;
    case HTTP_Track_SendRequest:
        if (this->mOnlySend) this->Finish(100097);
        break;
    case HTTP_Track_RecvStatus: {
        Res line=Do(EE_WON_HOST_OP_BYTES, inOp);
        if (!this->ExtractStatus(LineOf(line).c_str())) break;
        this->AsyncRecvCRMsg(HTTP_Track_RecvHeader);
        break;
    }
    case HTTP_Track_RecvHeader: {
        Res line=Do(EE_WON_HOST_OP_BYTES, inOp);
        if (LengthOf(line)==1) {
            this->DoneRecvHeader();
            break;
        }
        if (!this->ExtractHeaderLine(LineOf(line).c_str())) break;
        this->AsyncRecvCRMsg(HTTP_Track_RecvHeader);
        break;
    }
    case HTTP_Track_RecvFooter: {
        Res line=Do(EE_WON_HOST_OP_BYTES, inOp);
        if (LengthOf(line)==1) this->DoFinish();
        else this->AsyncRecvCRMsg(HTTP_Track_RecvFooter);
        break;
    }
    case HTTP_Track_RecvContent:
        this->DoneRecvContent(inOp);
        break;
    case HTTP_Track_Redirect:
        this->FinishRedirect(inOp);
        break;
    case HTTP_Track_RecvChunkLength: {
        Res line=Do(EE_WON_HOST_OP_BYTES, inOp);
        if (this->ExtractChunkLength(LineOf(line).c_str())) this->RecvContent();
        break;
    }
    default:
        return false;
    }
    return true;
}
static long long ChildNum(int inOp, int inWhich) { return Do(EE_WON_HOST_HOSTOP_NUM, inOp, 0, inWhich).mNum; }
static Res ChildBytes(int inOp, int inWhich) { return Do(EE_WON_HOST_HOSTOP_BYTES, inOp, 0, inWhich); }
static void ChildSet(int inOp, int inWhich, long long inNum, const string& inData = string(), int inObj = 0)
{
    Do(EE_WON_HOST_HOSTOP_SET, inOp, inObj, inWhich, inNum, inData);
}
void Op::FinishRedirect(int inOp)
{
    this->mLastModified=ChildNum(inOp, EE_WON_HTTPGET_LAST_MODIFIED);
    this->mContentType=ChildBytes(inOp, EE_WON_HTTPGET_CONTENT_TYPE).mBytes;
    this->mContentLength=(unsigned)ChildNum(inOp, EE_WON_HTTPGET_CONTENT_LENGTH);
    Res aContent=ChildBytes(inOp, EE_WON_HTTPGET_CONTENT);
    this->mHasContent=!aContent.mNull;
    this->mContent=aContent.mBytes;
    this->mLocalPath=ChildBytes(inOp, EE_WON_HTTPGET_LOCAL_PATH).mBytes;
    string aRedirectHost=ChildBytes(inOp, EE_WON_HTTPGET_REDIRECT_HOST).mBytes;
    if (!aRedirectHost.empty()) {
        this->mRedirectHost=aRedirectHost;
        this->mRedirectPath=ChildBytes(inOp, EE_WON_HTTPGET_REDIRECT_PATH).mBytes;
    }
    this->mHTTPStatus=(int)ChildNum(inOp, EE_WON_HTTPGET_STATUS);
    this->mContentIsNew=ChildNum(inOp, EE_WON_HTTPGET_IS_NEW)!=0;
    this->mNumRedirects=(int)ChildNum(inOp, EE_WON_HTTPGET_REDIRECTS);
    this->Finish(OpStatus(inOp));
}
void Op::FollowRedirect()
{
    Do(EE_WON_HOST_KILL_ALL);
    if (this->mSocket!=0) {
        Do(EE_WON_HOST_SOCKET_KILL, this->mSocket);
        this->mSocket=0;
    }
    if (this->mNumRedirects>5) {
        this->Finish(100095);
        return;
    }
    if (!this->mAutoRedirect) {
        this->Finish(100096);
        return;
    }
    int anOp=Do(EE_WON_HOST_NEW_HTTP_GET, 0, 0, (long long)this->mRedirectHost.size(), 0,
                this->mRedirectHost+this->mRedirectPath).mObj;
    ChildSet(anOp, EE_WON_HTTPGET_BINARY, this->mBinaryFile ? 1 : 0);
    ChildSet(anOp, EE_WON_HTTPGET_LOCAL_PATH, 0, this->mLocalPath);
    ChildSet(anOp, EE_WON_HTTPGET_CACHE, 0, string(), this->mHTTPCache);
    ChildSet(anOp, EE_WON_HTTPGET_REDIRECTS, this->mNumRedirects+1);
    if (this->mRecvChunkCompletion!=0)
        ChildSet(anOp, EE_WON_HTTPGET_CHUNK, this->mRecvChunkSize, string(), this->mRecvChunkCompletion);
    if (this->GetMode()==1) {
        this->Track(anOp,HTTP_Track_Redirect);
        Do(EE_WON_HOST_RUN_ASYNC, anOp, 0, 0xffffffffu);
        return;
    }
    unsigned aTime = this->TimeLeft();
    Do(EE_WON_HOST_RUN_BLOCK, anOp, 0, aTime);
    this->FinishRedirect(anOp);
}
void Op::DoneRecvHeader()
{
    if (this->mHTTPStatus==301 || this->mHTTPStatus==302) {
        this->FollowRedirect();
        return;
    }
    int anEntry=this->mHTTPCacheEntry;
    if (anEntry!=0) {
        if (this->mServerTime>0 && this->mExpireTime>0 && this->mExpireTime>this->mServerTime)
            Do(EE_WON_HOST_ENTRY_SET, anEntry, 0, 1, Now()+this->mExpireTime-this->mServerTime);
        if (!this->mEntityTag.empty())
            Do(EE_WON_HOST_ENTRY_SET, this->mHTTPCacheEntry, 0, 3, 0, this->mEntityTag);
    }
    if (this->mContentIsChunked && !this->RecvChunkLength())
        return;
    this->RecvContent();
}
void Op::RunHook()
{
    if (!this->mOnlyRecv) {
        string aProxy = Do(EE_WON_HOST_HTTP_PROXY).mBytes; // HTTPGetOp::mStaticProxyHost
        if (!aProxy.empty())
            this->SetProxy(aProxy);
        this->mLastModified=0;
        this->mServerTime=0;
        this->mExpireTime=0;
        this->mContentType.erase();
        this->mContentLength=0;
        this->mHasContent=false;
        this->mContent.clear();
        this->mDoClose=this->mCloseConnection;
        this->mHTTPStatus=0;
        this->mLocalFileModifyTime=0;
        this->mLocalFileSize=0;
        this->mNumRedirects=0;
        this->mContentIsNew=true;
        this->mChunkLength=0;
        this->mContentIsChunked=false;
        this->mIsFirstChunk=false;
        this->mHTTPCacheEntry=0;
        this->mContentAccumulator.clear();
        if (this->mSocket==0)
            this->mSocket=Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 0).mObj;
        this->mRedirectHost="";
        this->mRedirectPath="";
        if (this->mHTTPCache!=0) {
            this->mHTTPCacheEntry=Do(EE_WON_HOST_CACHE_ENTRY, this->mHTTPCache, 0, 0, 0, this->mHost+this->mRemotePath).mObj;
            if (this->mHTTPCacheEntry!=0) {
                this->mLocalPath=EntryBytes(this->mHTTPCacheEntry, 2);
                this->mEntityTag=EntryBytes(this->mHTTPCacheEntry, 3);
                if (EntryNum(this->mHTTPCacheEntry, 0) != 0 &&
                    EntryTime(this->mHTTPCacheEntry)>Now()) {
                    this->mContentIsNew=false;
                    this->Finish(0);
                    return;
                }
            }
        }
        if (!this->DoConnect())
            return;
        if (!this->SendRequest())
            return;
    }
    if (this->mOnlySend) {
        this->Finish(100097);
        return;
    }
    if (this->mOnlyRecv && this->GetMode()==1) {
        this->AsyncRecvCRMsg(HTTP_Track_RecvStatus);
        return;
    }
    unsigned aTime = this->TimeLeft();
    Res aLine = Do(EE_WON_HOST_RECV_CR, this->mSocket, 0, aTime);
    if (!this->CheckStatus((int)aLine.mNum))
        return;
    if (!this->ExtractStatus(LineOf(aLine).c_str()))
        return;
    while (true) {
        unsigned aTime2 = this->TimeLeft();
        aLine = Do(EE_WON_HOST_RECV_CR, this->mSocket, 0, aTime2);
        if (!this->CheckStatus((int)aLine.mNum))
            return;
        if (LengthOf(aLine)==1)
            break;
        if (!this->ExtractHeaderLine(LineOf(aLine).c_str()))
            return;
    }
    this->DoneRecvHeader();
}
void Op::CleanupHook()
{
    Do(EE_WON_HOST_BASE_CLEANUP);
    if (this->mSocket!=0) {
        if ((this->mDoClose && !this->mOnlySend) || Do(EE_WON_HOST_SOCKET_STATUS, this->mSocket).mNum!=100004) {
            Do(EE_WON_HOST_CLOSE, this->mSocket);
            this->mSocket=0;
        }
    }
    if (!this->mOnlySend && this->mHTTPCache!=0 && this->mHTTPCacheEntry!=0)
        Do(EE_WON_HOST_CACHE_UPDATE, this->mHTTPCache, this->mHTTPCacheEntry);
}
// the constructor of a URL (HTTPGetOp.cpp's split over ee_won_http_split_url)
static void FromURL(Op& o, const string& inURL)
{
    size_t aHost=0,anEnd=0;
    intptr_t aPath=-1;
    ee_won_http_split_url(reinterpret_cast<const uint8_t*>(inURL.data()),inURL.size(),&aHost,&anEnd,&aPath);
    o.mHost=inURL.substr(aHost,anEnd-aHost);
    if (aPath>=0)
        o.mRemotePath=inURL.substr(aPath);
    else
        o.mRemotePath="/";
}
static string Describe(const Op& o)
{
    char b[600];
    snprintf(b, sizeof(b),
             " sock %d cache %d entry %d chunk %d:%u f %d%d%d%d%d%d%d%d%d%d0 times %lld %lld %lld %lld size %llu len %u "
             "clen %u st %d redirects %d",
             o.mSocket, o.mHTTPCache, o.mHTTPCacheEntry, o.mRecvChunkCompletion, o.mRecvChunkSize,
             o.mContentIsChunked ? 1 : 0, o.mIsFirstChunk ? 1 : 0, o.mBinaryFile ? 1 : 0, o.mCloseConnection ? 1 : 0,
             o.mDoClose ? 1 : 0, o.mDoRangeResume ? 1 : 0, o.mContentIsNew ? 1 : 0, o.mOnlySend ? 1 : 0,
             o.mOnlyRecv ? 1 : 0, o.mAutoRedirect ? 1 : 0, o.mServerTime, o.mExpireTime, o.mLastModified,
             o.mLocalFileModifyTime, o.mLocalFileSize, o.mContentLength, o.mChunkLength, o.mHTTPStatus,
             o.mNumRedirects);
    return "host " + Hex(o.mHost) + " path " + Hex(o.mRemotePath) + " local " + Hex(o.mLocalPath) + " proxy " +
           Hex(o.mProxyHost) + " redir " + Hex(o.mRedirectHost) + Hex(o.mRedirectPath) + " tag " + Hex(o.mEntityTag) +
           " type " + Hex(o.mContentType) + " content " + (o.mHasContent ? Hex(o.mContent) : string("null")) +
           " acc " + Hex(o.mContentAccumulator) + b;
}
}

// ---- the original's statements: GetHTTPDocumentOp.cpp and HTTPDocumentOps.cpp before the port (the downloads are
// child ops made, set, run and read through the host calls the Rust op makes; the members as the C++ side's
// constructor sets them: the product's and the language's lower case, the cache file names)

namespace OrigDoc {
enum { HTTPDocType_MOTD = 0, HTTPDocType_TOU = 1, HTTPDocType_ADV = 2 };
struct Op {
    int mDocumentType;
    string mProductLower, mConfigLower;
    int mSession;
    bool mIsNew[2], mRetried[2];
    int mDocumentStatus[2], mHTTPStatus[2];
    long long mLastModified[2];
    string mDocuments[2];
    Op() : mDocumentType(0), mSession(0)
    {
        for (int i = 0; i < 2; i++) {
            mIsNew[i] = mRetried[i] = false;
            mDocumentStatus[i] = mHTTPStatus[i] = 0;
            mLastModified[i] = 0;
        }
    }
    int GetMode() { return (int)Do(EE_WON_HOST_MODE).mNum; }
    unsigned TimeLeft() { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
    void Finish(int s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
    void Track(int inOp, int inId) { Do(EE_WON_HOST_TRACK, inOp, 0, inId); }
    void DoFinish();
    void OpFinished(int inOp, int inOwner);
    void TryHTTPOp(int inOwner, bool inForceDefault);
    void RunHook();
    bool CallbackHook(int inOp, int inParam);
};
static int Status(int inOp) { return (int)Do(EE_WON_HOST_OP_STATUS, inOp).mNum; }
void Op::DoFinish()
{
    if (this->mDocumentStatus[0] != 0 && this->mDocumentType != HTTPDocType_ADV)
        this->Finish(100098);
    else if (this->mDocumentStatus[1] != 0)
        this->Finish(100099);
    else
        this->Finish(0);
}
void Op::OpFinished(int inOp, int inOwner)
{
    if (!this->mRetried[inOwner] && (Status(inOp) == 100094 ||
                                     Do(EE_WON_HOST_HOSTOP_NUM, inOp, 0, EE_WON_HTTPGET_STATUS).mNum == 404)) {
        this->TryHTTPOp(inOwner, true);
        return;
    }
    this->mDocumentStatus[inOwner] = Status(inOp);
    this->mHTTPStatus[inOwner] = (int)Do(EE_WON_HOST_HOSTOP_NUM, inOp, 0, EE_WON_HTTPGET_STATUS).mNum;
    this->mLastModified[inOwner] = Do(EE_WON_HOST_HTTP_MODIFIED, inOp).mNum;
    if (Status(inOp) != 0) return;
    this->mIsNew[inOwner] = Do(EE_WON_HOST_HOSTOP_NUM, inOp, 0, EE_WON_HTTPGET_IS_NEW).mNum != 0;
    if (this->mDocumentType != HTTPDocType_MOTD) return;
    int first = (int)Do(EE_WON_HOST_FILE_FIRST_CHAR, 0, 0, 0, 0, this->mDocuments[inOwner]).mNum; // fopen, fgetc
    if (first != -2) {
        if (first == -1) {
            this->mIsNew[inOwner] = false;
            return;
        }
        if (toupper(first & 0xff) != '1') this->mIsNew[inOwner] = true;
    }
}
void Op::TryHTTPOp(int inOwner, bool inForceDefault)
{
    std::string document;
    std::string config = this->mConfigLower;
    if (config == "en") config.erase();
    if (config.empty() || inForceDefault) {
        this->mRetried[inOwner] = true;
        if (this->mDocumentType == HTTPDocType_MOTD) document = "motd.txt";
        else if (this->mDocumentType == HTTPDocType_TOU) document = "tou.txt";
        else if (this->mDocumentType == HTTPDocType_ADV) document = "adv.txt";
    } else {
        if (this->mDocumentType == HTTPDocType_MOTD) document = "motd_" + config + ".txt";
        else if (this->mDocumentType == HTTPDocType_TOU) document = "tou_" + config + ".txt";
        else if (this->mDocumentType == HTTPDocType_ADV) document = "adv_" + config + ".txt";
    }
    std::string path;
    if (inOwner == 0) path = std::string("/motd/") + "sys/";
    else if (inOwner == 1) path = std::string("/motd/") + this->mProductLower + "/";
    std::string host = "www.sierra.com";
    int op = Do(EE_WON_HOST_NEW_HTTP_GET, 0, 0, (long long)host.size(), 0, host + path + document).mObj;
    Do(EE_WON_HOST_HOSTOP_SET, op, 0, EE_WON_HTTPGET_RANGE_RESUME, 0);
    Do(EE_WON_HOST_HOSTOP_SET, op, 0, EE_WON_HTTPGET_LOCAL_PATH, 0, this->mDocuments[inOwner]);
    if (this->GetMode() == 1) {
        this->Track(op, inOwner);
        if (this->mSession != 0) Do(EE_WON_HOST_SESSION_ADD, this->mSession, op);
        else Do(EE_WON_HOST_RUN_ASYNC, op, 0, 0xffffffffu);
        return;
    }
    unsigned aTime = this->TimeLeft();
    Do(EE_WON_HOST_RUN, op, 0, 2, aTime);
    this->OpFinished(op, inOwner);
}
void Op::RunHook()
{
    for (int i = 0; i < 2; ++i) {
        this->mIsNew[i] = false;
        this->mDocumentStatus[i] = 100004;
        this->mHTTPStatus[i] = 0;
        this->mLastModified[i] = 0;
        this->mRetried[i] = false;
    }
    if (this->mDocumentType == HTTPDocType_ADV) this->TryHTTPOp(1, false);
    else for (int owner = 0; owner < 2; ++owner)
        this->TryHTTPOp(owner, false);
    if (this->GetMode() != 1) this->DoFinish();
}
bool Op::CallbackHook(int inOp, int inParam)
{
    this->OpFinished(inOp, inParam & 1); // the owner (any other id indexed past the arrays)
    if (Do(EE_WON_HOST_TRACKER_EMPTY).mNum != 0) this->DoFinish();
    return true;
}
static string Describe(const Op& o)
{
    char b[300];
    snprintf(b, sizeof(b), " new %d%d retried %d%d st %d %d http %d %d mod %lld %lld docs ", o.mIsNew[0] ? 1 : 0,
             o.mIsNew[1] ? 1 : 0, o.mRetried[0] ? 1 : 0, o.mRetried[1] ? 1 : 0, o.mDocumentStatus[0],
             o.mDocumentStatus[1], o.mHTTPStatus[0], o.mHTTPStatus[1], o.mLastModified[0], o.mLastModified[1]);
    char t[80];
    snprintf(t, sizeof(t), "type %d product ", o.mDocumentType);
    return t + Hex(o.mProductLower) + " config " + Hex(o.mConfigLower) + " session " + std::to_string(o.mSession) + b +
           Hex(o.mDocuments[0]) + " " + Hex(o.mDocuments[1]);
}
}


// ---- the original's statements: ServerRequestOp.cpp before round 10 (its exchange is server.rs's, as the C++
// called it; its primitives over the op stand-in, a Rust op derived from it the request's own: ee_won_reqop_*).
// The iterator before any RunHook (default-constructed there) and after the context's copy (left dangling there)
// is the list's begin; the reuse and expiry flags, unset before a RunHook, false.

namespace OrigReq {
typedef std::list<string> AddrList;
struct Op {
    AddrList mAddrList;
    AddrList::iterator mAddrItr;
    int mCurSession, mUsedSession, mSocket, mServerContext, mUDPSocket;
    bool mReusingSession, mSessionExpired, mUseAuth2, mCloseConnection, mUseLast;
    int32_t mGetCertStatus, mLastServerError;
    string mCurAddr;
    bool mHasRequest, mHasResponse;
    string mRequest, mResponse;
    EeWonReqOp* mRustOp;
    explicit Op(EeWonReqOp* inOp)
        : mCurSession(0), mUsedSession(0), mSocket(0), mServerContext(0), mUDPSocket(0), mReusingSession(false),
          mSessionExpired(false), mUseAuth2(true), mCloseConnection(true), mUseLast(false), mGetCertStatus(100004),
          mLastServerError(100004), mCurAddr(6, '\0'), mHasRequest(false), mHasResponse(false), mRustOp(inOp)
    {
        this->mUseAuth2 = ee_won_reqop_use_auth2(inOp);
        this->mAddrItr = this->mAddrList.begin();
    }
    ~Op() { ee_won_reqop_free(this->mRustOp); }
};
// a SmartPtr's assignment: a new object (its reference the slot's), or none
static void Adopt(int& ioSlot, int inObj)
{
    int anOld = ioSlot;
    ioSlot = inObj;
    if (anOld) Do(EE_WON_HOST_RELEASE, anOld);
}
// a SmartPtr's assignment from another's object
static void Assign(int& ioSlot, int inObj)
{
    if (inObj) Do(EE_WON_HOST_ADD_REF, inObj);
    Adopt(ioSlot, inObj);
}
static void SetLastServerError(Op& o, int inError)
{
    int32_t aLast = o.mLastServerError;
    ee_won_server_req_last_error(&aLast, &o.mUseLast, inError);
    o.mLastServerError = aLast;
}
static void ReqSink(void* inContext, const uint8_t* inData, size_t inLength)
{
    string* s = static_cast<string*>(inContext);
    s->assign(reinterpret_cast<const char*>(inData), inLength);
    s->push_back('\x01');
}
static int GetNextRequest(Op& o, bool inDatagram)
{
    unsigned aUserId = 0;
    if (ee_won_reqop_needs_user_id(o.mRustOp)) aUserId = (unsigned)Do(EE_WON_HOST_CERT_USER_ID).mNum;
    ee_won_reqop_set_env(o.mRustOp, aUserId, (unsigned)Do(EE_WON_HOST_TICK).mNum);
    unsigned char aLfs = inDatagram ? 0 : (unsigned char)Do(EE_WON_HOST_LFS).mNum;
    string aRequest;
    int aStatus = ee_won_reqop_next_request(o.mRustOp, aLfs, ReqSink, &aRequest);
    if (!aRequest.empty()) {
        aRequest.resize(aRequest.size() - 1);
        o.mHasRequest = true;
        o.mRequest = aRequest;
    }
    return aStatus;
}
static int CheckResponse(Op& o, bool* outThrew)
{
    *outThrew = false;
    ee_won_reqop_set_env(o.mRustOp, 0, (unsigned)Do(EE_WON_HOST_TICK).mNum);
    int32_t aVerdict = 0;
    long aStatus = ee_won_reqop_check_response(
        o.mRustOp, o.mHasResponse ? reinterpret_cast<const uint8_t*>(o.mResponse.data()) : NULL,
        o.mHasResponse ? o.mResponse.size() : 0, &aVerdict);
    if (aVerdict == EE_WON_REQOP_CUT) {
        *outThrew = true;
        return 0;
    }
    if (aVerdict == EE_WON_REQOP_INVALID_HEADER) {
        SetLastServerError(o, 100063);
        return 100057;
    }
    return (int)aStatus;
}
static void QueueSocketOp(Op& o, int inOp, unsigned inTimeout)
{
    Do(EE_WON_HOST_TRACK, inOp, 0, 1);
    Do(EE_WON_HOST_QUEUE, o.mSocket, inOp, inTimeout);
    Do(EE_WON_HOST_RELEASE, inOp);
}
static int NewPeerAuth(Op& o)
{
    return Do(EE_WON_HOST_NEW_PEER_AUTH, o.mSocket, 0, o.mUseAuth2 ? 1 : 0, 0, string(), o.mCurAddr).mObj;
}

struct Glue {
    Op* mOp;
    int mDone;
};
static Op& O(void* g) { return *static_cast<Glue*>(g)->mOp; }
static int32_t RStatus(void*) { return (int)Do(EE_WON_HOST_STATUS).mNum; }
static uint32_t RTimeLeft(void*) { return (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum; }
static bool RIsAsync(void*) { return Do(EE_WON_HOST_MODE).mNum == 1; }
static int32_t RAuthType(void*) { return (int)Do(EE_WON_HOST_AUTH_TYPE).mNum; }
static void RFinish(void*, int32_t s) { Do(EE_WON_HOST_FINISH, 0, 0, s); }
static void RFinishSave(void* g, int32_t s)
{
    Op& o = O(g);
    Do(EE_WON_HOST_FINISH, 0, 0, s);
    if (Do(EE_WON_HOST_AUTH_TYPE).mNum == 1)
        Do(EE_WON_HOST_PUT_SESSION, o.mCurSession, 0, 0, 0, string(), o.mCurAddr);
    Assign(o.mUsedSession, o.mCurSession);
    Adopt(o.mCurSession, 0);
}
static void RKill(void*) { Do(EE_WON_HOST_KILL, 0, 0, 1); }
static bool RPrepare(void* g)
{
    Op& o = O(g);
    Adopt(o.mUsedSession, 0);
    if (o.mServerContext) {
        o.mAddrList.clear();
        Res r = Do(EE_WON_HOST_CTX_ADDRS, o.mServerContext);
        for (size_t i = 0; i + 6 <= r.mBytes.size(); i += 6) o.mAddrList.push_back(r.mBytes.substr(i, 6));
        o.mAddrItr = o.mAddrList.begin(); // the old iterator dangled (an empty copy finishes before the rewind)
    }
    return !o.mAddrList.empty();
}
static void RRewind(void* g) { O(g).mAddrItr = O(g).mAddrList.begin(); }
static bool RAtBegin(void* g) { return O(g).mAddrItr == O(g).mAddrList.begin(); }
static bool RAtEnd(void* g) { return O(g).mAddrItr == O(g).mAddrList.end(); }
static bool RHasContext(void* g) { return O(g).mServerContext != 0; }
static bool RHasAuthContext(void*) { return Do(EE_WON_HOST_HAS_AUTH).mNum != 0; }
static void RNotify(void* g) { Do(EE_WON_HOST_CTX_FAILED, O(g).mServerContext, 0, 0, 0, string(), O(g).mCurAddr); }
static void RNextAddr(void* g)
{
    Op& o = O(g);
    Adopt(o.mSocket, Do(EE_WON_HOST_NEW_SOCKET, 0, 0, 0).mObj);
    long long aLfs = Do(EE_WON_HOST_LFS).mNum;
    Do(EE_WON_HOST_SOCKET_SET_LFS, o.mSocket, 0, aLfs);
    o.mCurAddr = *o.mAddrItr;
    ++o.mAddrItr;
}
static void RRetreat(void* g) { --O(g).mAddrItr; }
static void RReset(void* g)
{
    Op& o = O(g);
    Do(EE_WON_HOST_KILL_ALL);
    ee_won_reqop_reset(o.mRustOp);
    if (o.mSocket) {
        Do(EE_WON_HOST_CLOSE, o.mSocket);
        Adopt(o.mSocket, 0);
    }
}
static void RLoadSession(void* g)
{
    Op& o = O(g);
    Adopt(o.mCurSession, Do(EE_WON_HOST_GET_SESSION, 0, 0, 0, 0, string(), o.mCurAddr).mObj);
}
static void RClearSession(void* g) { Adopt(O(g).mCurSession, 0); }
static bool RHasSession(void* g) { return O(g).mCurSession != 0; }
static void RAddCrypt(void* g) { Do(EE_WON_HOST_ADD_CRYPT, O(g).mSocket, O(g).mCurSession); }
static void RQueueConnect(void* g)
{
    Op& o = O(g);
    int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_CONNECT, 0, string(), o.mCurAddr).mObj;
    QueueSocketOp(o, anOp, (unsigned)Do(EE_WON_HOST_CONNECT_TIME).mNum);
}
static int32_t RConnectBlock(void* g)
{
    Op& o = O(g);
    unsigned t = (unsigned)Do(EE_WON_HOST_CONNECT_TIME).mNum;
    return (int)Do(EE_WON_HOST_CONNECT, o.mSocket, 0, t, 0, string(), o.mCurAddr).mNum;
}
static void RPeerAuthAsync(void* g)
{
    Op& o = O(g);
    int p = NewPeerAuth(o);
    Do(EE_WON_HOST_TRACK, p, 0, 2);
    Do(EE_WON_HOST_RUN_ASYNC, p, 0, 0xffffffffu);
    Do(EE_WON_HOST_RELEASE, p);
}
static bool RPeerAuthBlock(void* g, int32_t* outStatus, int32_t* outCert)
{
    Op& o = O(g);
    int p = NewPeerAuth(o);
    unsigned t = (unsigned)Do(EE_WON_HOST_TIME_LEFT).mNum;
    if (Do(EE_WON_HOST_RUN_BLOCK, p, 0, t).mNum != 0) {
        Adopt(o.mCurSession, Do(EE_WON_HOST_PEER_AUTH_SESSION, p).mObj);
        Do(EE_WON_HOST_RELEASE, p);
        return true;
    }
    *outStatus = (int)Do(EE_WON_HOST_OP_STATUS, p).mNum;
    *outCert = (int)Do(EE_WON_HOST_PEER_AUTH_CERT, p).mNum;
    Do(EE_WON_HOST_RELEASE, p);
    return false;
}
static int32_t RGetNextRequest(void* g) { return GetNextRequest(O(g), false); }
static void RQueueSend(void* g)
{
    Op& o = O(g);
    int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_SEND_MSG, 0, o.mHasRequest ? o.mRequest : string()).mObj;
    QueueSocketOp(o, anOp, (unsigned)Do(EE_WON_HOST_SEND_TIME).mNum);
}
static int32_t RSendBlock(void* g)
{
    Op& o = O(g);
    unsigned t = (unsigned)Do(EE_WON_HOST_SEND_TIME).mNum;
    return (int)Do(EE_WON_HOST_SEND_MSG, o.mSocket, 0, t, 0, o.mHasRequest ? o.mRequest : string()).mNum;
}
static void RQueueRecv(void* g)
{
    Op& o = O(g);
    int anOp = Do(EE_WON_HOST_NEW_OP, 0, 0, EE_WON_HOST_OP_RECV_MSG).mObj;
    QueueSocketOp(o, anOp, (unsigned)Do(EE_WON_HOST_RECV_TIME).mNum);
}
static int32_t RRecvBlock(void* g)
{
    Op& o = O(g);
    unsigned t = (unsigned)Do(EE_WON_HOST_RECV_TIME).mNum;
    Res r = Do(EE_WON_HOST_RECV_MSG, o.mSocket, 0, t);
    o.mHasResponse = !r.mNull;
    o.mResponse = r.mBytes;
    return (int)r.mNum;
}
static void RResponse(void* g, const uint8_t** outData, size_t* outLength)
{
    Op& o = O(g);
    *outData = o.mHasResponse ? reinterpret_cast<const uint8_t*>(o.mResponse.data()) : NULL;
    *outLength = o.mHasResponse ? o.mResponse.size() : 0;
}
static int32_t RCheckResponse(void* g, bool* outThrew) { return CheckResponse(O(g), outThrew); }
static void RTakeOpSession(void* g)
{
    Adopt(O(g).mCurSession, Do(EE_WON_HOST_PEER_AUTH_SESSION, static_cast<Glue*>(g)->mDone).mObj);
}
static void RTakeOpResponse(void* g)
{
    Res r = Do(EE_WON_HOST_OP_BYTES, static_cast<Glue*>(g)->mDone);
    O(g).mHasResponse = !r.mNull;
    O(g).mResponse = r.mBytes;
}
static EeWonServerReqHooks Hooks(Glue* g)
{
    Op& o = *g->mOp;
    EeWonServerReqHooks h = {g, &o.mLastServerError, &o.mGetCertStatus, &o.mUseLast, &o.mReusingSession,
                             &o.mSessionExpired, RStatus, RTimeLeft, RIsAsync, RAuthType, RFinish, RFinishSave, RKill,
                             RPrepare, RRewind, RAtBegin, RAtEnd, RHasContext, RHasAuthContext, RNotify, RNextAddr,
                             RRetreat, RReset, RLoadSession, RClearSession, RHasSession, RAddCrypt, RQueueConnect,
                             RConnectBlock, RPeerAuthAsync, RPeerAuthBlock, RGetNextRequest, RQueueSend, RSendBlock,
                             RQueueRecv, RRecvBlock, RResponse, RCheckResponse, RTakeOpSession, RTakeOpResponse};
    return h;
}

static void RunDatagram(Op& o)
{
    if (o.mAddrList.empty()) {
        Do(EE_WON_HOST_FINISH, 0, 0, 100055);
        return;
    }
    string anAddr = o.mAddrList.front();
    ee_won_reqop_reset(o.mRustOp);
    GetNextRequest(o, true);
    int anOp = Do(EE_WON_HOST_NEW_OP, o.mUDPSocket, 0, EE_WON_HOST_OP_SEND_BYTES_TO, 0,
                  o.mHasRequest ? o.mRequest : string(), anAddr).mObj;
    int aMode = (int)Do(EE_WON_HOST_MODE).mNum;
    unsigned aTimeout = (unsigned)Do(EE_WON_HOST_TIMEOUT).mNum;
    if (aMode == 1) Do(EE_WON_HOST_RUN_ASYNC, anOp, 0, aTimeout);
    else Do(EE_WON_HOST_RUN_BLOCK, anOp, 0, aTimeout);
    Do(EE_WON_HOST_FINISH, 0, 0, 0);
    Do(EE_WON_HOST_RELEASE, anOp);
}
static void RunHook(Op& o)
{
    int32_t aFinish = 0;
    if (!ee_won_reqop_run_hook(o.mRustOp, (int)Do(EE_WON_HOST_AUTH_TYPE).mNum, &aFinish)) {
        Do(EE_WON_HOST_FINISH, 0, 0, aFinish);
        return;
    }
    if (o.mUDPSocket) {
        RunDatagram(o);
        return;
    }
    Glue g = {&o, 0};
    EeWonServerReqHooks h = Hooks(&g);
    ee_won_server_req_run(&h);
}
static void CleanupHook(Op& o)
{
    Do(EE_WON_HOST_BASE_CLEANUP);
    int aStatus = (int)Do(EE_WON_HOST_STATUS).mNum;
    bool aKilled = Do(EE_WON_HOST_OP_KILLED).mNum != 0;
    unsigned aPlan = ee_won_server_req_cleanup_plan(aStatus, aKilled, o.mServerContext != 0, o.mSocket != 0,
                                                    o.mCloseConnection);
    if (aPlan & 1) Do(EE_WON_HOST_CTX_FAILED, o.mServerContext, 0, 0, 0, string(), o.mCurAddr);
    if (aPlan & 2) {
        Do(EE_WON_HOST_CLOSE, o.mSocket);
        Adopt(o.mSocket, 0);
    }
    Adopt(o.mCurSession, 0);
}
static bool CallbackHook(Op& o, int inOp, int inParam)
{
    EeWonServerReqDone aDone;
    aDone.killed = Do(EE_WON_HOST_OP_KILLED, inOp).mNum != 0;
    aDone.status = (int)Do(EE_WON_HOST_OP_STATUS, inOp).mNum;
    aDone.param = inParam;
    aDone.cert_status = 0;
    if (inParam == 2 && !aDone.killed && aDone.status == 100053)
        aDone.cert_status = (int)Do(EE_WON_HOST_PEER_AUTH_CERT, inOp).mNum;
    aDone.is_recv = inParam == 1 && Do(EE_WON_HOST_OP_IS_RECV_MSG, inOp).mNum != 0;
    Glue g = {&o, inOp};
    EeWonServerReqHooks h = Hooks(&g);
    return ee_won_server_req_callback(&h, &aDone);
}
static void DescribeTo(void* c, const uint8_t* b, size_t n)
{
    static_cast<string*>(c)->assign(reinterpret_cast<const char*>(b), n);
}
static string Describe(const Op& o)
{
    char b[200];
    string a;
    for (AddrList::const_iterator i = o.mAddrList.begin(); i != o.mAddrList.end(); ++i) a += Hex(*i) + ",";
    int at = 0;
    for (AddrList::const_iterator i = o.mAddrList.begin(); i != o.mAddrList.end() && i != o.mAddrItr; ++i) at++;
    snprintf(b, sizeof(b), " i%d cur %s s%d/%d sock %d udp %d f%d%d%d%d l%d c%d ", at, Hex(o.mCurAddr).c_str(),
             o.mCurSession, o.mUsedSession, o.mSocket, o.mUDPSocket, (int)o.mReusingSession, (int)o.mSessionExpired,
             (int)o.mUseLast, (int)o.mUseAuth2, (int)o.mLastServerError, (int)o.mGetCertStatus);
    string r;
    ee_won_reqop_describe(o.mRustOp, DescribeTo, &r);
    return a + b + (o.mHasRequest ? Hex(o.mRequest) : string("null")) + " " +
           (o.mHasResponse ? Hex(o.mResponse) : string("null")) + " | " + r;
}
} // namespace OrigReq

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

static const char* const kFtpUrls[] = { "ftp://host.example/dir/file.bin", "ftp://bob:pw@ftp.won.net/patch.exe",
                                        "ftp://anon@h/a/b/c.txt", "ftp://h" };
static const char* const kFtpPaths[] = { "x.bin", "/top/y.txt", "a/b/", "" };

static Step MakeFtpStep()
{
    static const int kKinds[] = { 0, 0, 1, 2, 3, 4, 5, 5, 6, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 11 };
    Step s;
    s.mKind = kKinds[Pick(21)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: s.mData = kNames[Pick(4)]; break;
    case 1: s.mData = kFtpPaths[Pick(4)]; break;
    case 2: case 3: s.mData = kNames[Pick(4)]; break;
    case 4: case 5: s.mA = Pick(2); break;
    case 6: s.mA = Pick(2); s.mB = Pick(2) ? 10000 : 7; break;          // chunk (completion, size)
    case 7: s.mA = Pick(2) + 1; break;                                   // run (mode)
    case 8: case 9: { static const int k[] = { 0, 1, 2, 2, 2, 2, 3, 4, 4, 7 }; s.mA = k[Pick(10)]; break; }
    default: break;
    }
    return s;
}
static string FtpStep(OrigFtp::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0: if (o) o->mLocalPath = s.mData; else RSet(r, EE_WON_FTPGET_LOCAL_PATH, 0, s.mData); break;
    case 1: if (o) FtpMsg<OrigFtp::Op>::SetRemotePath(*o, s.mData); else RSet(r, EE_WON_FTPGET_REMOTE_PATH, 0, s.mData); break;
    case 2: if (o) o->SetUserName(s.mData); else RSet(r, EE_WON_FTPGET_USER, 0, s.mData); break;
    case 3: if (o) o->SetPassword(s.mData); else RSet(r, EE_WON_FTPGET_PASSWORD, 0, s.mData); break;
    case 4: if (o) o->mIsBinary = s.mA != 0; else RSet(r, EE_WON_FTPGET_BINARY, s.mA); break;
    case 5: if (o) o->mDoResume = s.mA != 0; else RSet(r, EE_WON_FTPGET_RESUME, s.mA); break;
    case 6: {
        int c = s.mA ? NewObj() : 0;
        if (o) { o->mRecvChunkCompletion = c; o->mRecvChunkSize = (unsigned)s.mB; }
        else RSet(r, EE_WON_FTPGET_CHUNK, s.mB, string(), NULL, c);
        if (c) S.mOwn[c]--;
        break;
    }
    case 7:
        S.mMode = (int)s.mA;
        S.mStatus = 100000;
        if (o) OrigFtp::RunHook(*o); else ee_won_hostop_run(r);
        break;
    case 8: case 9: {
        int done = ++S.mSerial;
        if (s.mA == 2) S.mKinds[done] = EE_WON_HOST_OP_RECV_CR_MSG; // the reply ops are line receives
        bool b;
        if (o) b = OrigFtp::CallbackHook(*o, done, (int)s.mA); else b = ee_won_hostop_callback(r, (int)s.mA, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 10: if (o) OrigFtp::CleanupHook(*o); else ee_won_hostop_cleanup(r); break;
    case 11: {
        if (o) ret = OrigFtp::Results(*o);
        else {
            char b[40];
            snprintf(b, sizeof(b), "st %d ", (int)ee_won_hostop_num(r, EE_WON_FTPGET_STATUS, 0));
            string c, l, m;
            bool has = ee_won_hostop_bytes(r, EE_WON_FTPGET_CONTENT, 0, DescribeSink, &c);
            ee_won_hostop_bytes(r, EE_WON_FTPGET_LOCAL, 0, DescribeSink, &l);
            ee_won_hostop_bytes(r, EE_WON_FTPGET_REMOTE, 0, DescribeSink, &m);
            ret = b + (has ? Hex(c) : string("null")) + " " + Hex(l) + " " + Hex(m);
        }
        break;
    }
    }
    string d = o ? OrigFtp::Describe(*o) : RDescribe(r);
    return ret + "|" + S.mTrace + "|" + d;
}

static const char* const kHttpUrls[] = { "http://www.sierra.com/motd/sys/motd.txt", "www.sierra.com",
                                         "http://h:8080/a/b?c=1", "http://onlyhost" };
static const char* const kHttpHosts[] = { "www.sierra.com", "h:8080", "", "x" };
static const char* const kHttpPaths[] = { "/motd/sys/motd.txt", "/", "", "/a b" };

static Step MakeHttpStep()
{
    // weighted: runs, many callbacks, the setters, cleanups, the results
    static const int kKinds[] = { 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 12 };
    Step s;
    s.mKind = kKinds[Pick(23)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: s.mA = Pick(2) + 1; break; // run (mode)
    case 1: { static const int k[] = { 1, 2, 2, 3, 3, 3, 3, 4, 5, 6, 6, 7, 8, 8, 9, 10 }; s.mA = k[Pick(16)]; break; }
    case 3: s.mData = kNames[Pick(4)]; break;
    case 4: case 9: case 11: s.mA = Pick(2); s.mB = Pick(2) ? 10000 : 7; break;
    case 5: case 7: case 8: s.mA = Pick(2); break;
    case 6: s.mA = Pick(3); break;
    case 12: s.mA = Pick(8); break;
    default: break;
    }
    return s;
}
static string HttpResults(OrigHttp::Op* o, EeWonHostOp* r)
{
    static const int kNums[] = { EE_WON_HTTPGET_REDIRECTS, EE_WON_HTTPGET_LAST_MODIFIED, EE_WON_HTTPGET_STATUS,
                                 EE_WON_HTTPGET_IS_NEW, EE_WON_HTTPGET_CONTENT_LENGTH, EE_WON_HTTPGET_ONLY_SEND,
                                 EE_WON_HTTPGET_ONLY_RECV };
    static const int kBytes[] = { EE_WON_HTTPGET_LOCAL_PATH, EE_WON_HTTPGET_HOST, EE_WON_HTTPGET_REMOTE_PATH,
                                  EE_WON_HTTPGET_CONTENT_TYPE, EE_WON_HTTPGET_CONTENT, EE_WON_HTTPGET_REDIRECT_HOST,
                                  EE_WON_HTTPGET_REDIRECT_PATH };
    string out;
    char b[40];
    for (int i = 0; i < 7; i++) {
        long long n = 0;
        if (o) {
            switch (i) {
            case 0: n = o->mNumRedirects; break;
            case 1: n = o->mLastModified; break;
            case 2: n = o->mHTTPStatus; break;
            case 3: n = o->mContentIsNew; break;
            case 4: n = o->mContentLength; break;
            case 5: n = o->mOnlySend; break;
            case 6: n = o->mOnlyRecv; break;
            }
        } else
            n = ee_won_hostop_num(r, kNums[i], 0);
        snprintf(b, sizeof(b), "%lld ", n);
        out += b;
    }
    for (int i = 0; i < 7; i++) {
        string d;
        bool has = true;
        if (o) {
            switch (i) {
            case 0: d = o->mLocalPath; break;
            case 1: d = o->mHost; break;
            case 2: d = o->mRemotePath; break;
            case 3: d = o->mContentType; break;
            case 4: has = o->mHasContent; d = o->mContent; break;
            case 5: d = o->mRedirectHost; break;
            case 6: d = o->mRedirectPath; break;
            }
        } else
            has = ee_won_hostop_bytes(r, kBytes[i], 0, DescribeSink, &d);
        out += (has ? Hex(d) : string("null")) + " ";
    }
    return out;
}
static string HttpStep(OrigHttp::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0:
        S.mMode = (int)s.mA;
        S.mStatus = 100000;
        S.mLine = 0;
        if (o) o->RunHook(); else ee_won_hostop_run(r);
        break;
    case 1: {
        int done = ++S.mSerial;
        bool b = o ? o->CallbackHook(done, (int)s.mA) : ee_won_hostop_callback(r, (int)s.mA, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 2: if (o) o->CleanupHook(); else ee_won_hostop_cleanup(r); break;
    case 3: if (o) o->mLocalPath = s.mData; else RSet(r, EE_WON_HTTPGET_LOCAL_PATH, 0, s.mData); break;
    case 4: case 9: case 11: {
        int c = s.mA ? NewObj() : 0;
        int which = s.mKind == 4 ? EE_WON_HTTPGET_CACHE : s.mKind == 9 ? EE_WON_HTTPGET_SOCKET : EE_WON_HTTPGET_CHUNK;
        if (o) {
            if (s.mKind == 4) o->mHTTPCache = c;
            else if (s.mKind == 9) o->mSocket = c; // SetSocket
            else { o->mRecvChunkCompletion = c; o->mRecvChunkSize = (unsigned)s.mB; } // SetRecvChunkCompletion
        } else
            RSet(r, which, s.mKind == 11 ? s.mB : 0, string(), NULL, c);
        if (c) S.mOwn[c]--;
        break;
    }
    case 5: if (o) o->mBinaryFile = s.mA != 0; else RSet(r, EE_WON_HTTPGET_BINARY, s.mA); break;
    case 6:
        if (o) {
            if (s.mA == 0) { o->mOnlySend = true; o->mOnlyRecv = false; }      // SetOnlySend
            else if (s.mA == 2) { o->mOnlyRecv = true; o->mOnlySend = false; } // SetOnlyRecv
            else { o->mOnlySend = false; o->mOnlyRecv = false; }               // SetSendAndRecv
        } else
            RSet(r, EE_WON_HTTPGET_MODE, s.mA);
        break;
    case 7: if (o) o->mDoRangeResume = s.mA != 0; else RSet(r, EE_WON_HTTPGET_RANGE_RESUME, s.mA); break;
    case 8: if (o) o->mCloseConnection = s.mA != 0; else RSet(r, EE_WON_HTTPGET_CLOSE, s.mA); break;
    case 10: ret = HttpResults(o, r); break;
    case 12: if (o) o->mNumRedirects = (int)s.mA; else RSet(r, EE_WON_HTTPGET_REDIRECTS, s.mA); break;
    }
    string d = o ? OrigHttp::Describe(*o) : RDescribe(r);
    return ret + "|" + S.mTrace + "|" + d;
}

static const char* const kDocProducts[] = { "eeaoc", "", "x y" };
static const char* const kDocConfigs[] = { "", "en", "fr", "de" };
static const char* const kDocNames[] = { "motd", "tou", "adv", "zzz" };
static string DocPath(int inType, int inProduct, int inOwner)
{
    return string("_won") + (inOwner == 0 ? string("sys") : string(kDocProducts[inProduct])) + kDocNames[inType] + ".txt";
}
static Step MakeDocStep()
{
    static const int kKinds[] = { 0, 0, 0, 1, 1, 1, 1, 1, 2, 3, 4, 5 };
    Step s;
    s.mKind = kKinds[Pick(12)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: s.mA = Pick(2) + 1; break;               // run (mode)
    case 1: s.mA = Pick(8) ? Pick(2) : Pick(4); break; // callback (the owner, else another id)
    case 3: s.mData = kDocConfigs[Pick(4)]; break;  // the language, lower case
    case 4: s.mA = Pick(2); break;                   // the session, or none
    default: break;
    }
    return s;
}
static string DocStep(OrigDoc::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0:
        S.mMode = (int)s.mA;
        S.mStatus = 100000;
        if (o) o->RunHook(); else ee_won_hostop_run(r);
        break;
    case 1: {
        int done = ++S.mSerial;
        bool b = o ? o->CallbackHook(done, (int)s.mA) : ee_won_hostop_callback(r, (int)s.mA, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 2: if (o) Do(EE_WON_HOST_BASE_CLEANUP); else ee_won_hostop_cleanup(r); break;
    case 3: if (o) o->mConfigLower = s.mData; else RSet(r, EE_WON_HTTPDOC_CONFIG, 0, s.mData); break;
    case 4: {
        int c = s.mA ? NewObj() : 0;
        if (o) o->mSession = c; else RSet(r, EE_WON_HTTPDOC_SESSION, 0, string(), NULL, c);
        if (c) S.mOwn[c]--;
        break;
    }
    case 5: {
        char b[200];
        string out;
        for (int w = 0; w < 2; w++) {
            if (o)
                snprintf(b, sizeof(b), "%d %d %d %d %lld %s ", o->mDocumentType, o->mIsNew[w] ? 1 : 0,
                         o->mDocumentStatus[w], o->mHTTPStatus[w], o->mLastModified[w], Hex(o->mDocuments[w]).c_str());
            else {
                string d;
                ee_won_hostop_bytes(r, EE_WON_HTTPDOC_PATH, w, DescribeSink, &d);
                snprintf(b, sizeof(b), "%d %d %d %d %lld %s ", (int)ee_won_hostop_num(r, EE_WON_HTTPDOC_TYPE, w),
                         (int)ee_won_hostop_num(r, EE_WON_HTTPDOC_IS_NEW, w),
                         (int)ee_won_hostop_num(r, EE_WON_HTTPDOC_STATUS, w),
                         (int)ee_won_hostop_num(r, EE_WON_HTTPDOC_HTTP_STATUS, w),
                         ee_won_hostop_num(r, EE_WON_HTTPDOC_MODIFIED, w), Hex(d).c_str());
            }
            out += b;
        }
        ret = out;
        break;
    }
    }
    string d = o ? OrigDoc::Describe(*o) : RDescribe(r);
    return ret + "|" + S.mTrace + "|" + d;
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

// the request exchange's ops: a contest call, a ping, a user id lookup (Rust ops derived from ServerRequestOp)
static EeWonReqOp* NewReqOp(int inKind, int inArg)
{
    if (inKind == 0) {
        static const char kData[] = "\x01\x02\x03";
        EeWonAdminValue v[3] = {{inArg, NULL, 0}, {inArg & 1, NULL, 0}, {0, reinterpret_cast<const uint8_t*>(kData), 3}};
        return ee_won_reqops_new(EE_WON_REQOPS_CONTEST, v, 3);
    }
    if (inKind == 1) {
        EeWonAdminValue v[2] = {{(inArg & 1) ? 2 : 4, NULL, 0}, {(inArg >> 1) & 1, NULL, 0}};
        return ee_won_reqops_new(EE_WON_REQOPS_PING, v, 2);
    }
    static const char kName[] = "b\0o\0b\0";
    return ee_won_reqop_user_id(reinterpret_cast<const uint8_t*>(kName), 6);
}
static Step MakeReqStep()
{
    static const int kKinds[] = { 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 4, 4, 5, 6 };
    Step s;
    s.mKind = kKinds[Pick(14)];
    s.mA = s.mB = 0;
    switch (s.mKind) {
    case 0: s.mA = Pick(2) + 1; s.mB = Pick(4) == 0 ? 0 : Pick(3); break; // run (mode, auth type)
    case 5: s.mA = Pick(3) == 0; break;                                   // the datagram socket, or none
    default: break;
    }
    return s;
}
static string ReqResults(OrigReq::Op* o, EeWonHostOp* r)
{
    static const int kNums[] = { EE_WON_REQOP_PING_LAG, EE_WON_REQOP_PING_VERSION, EE_WON_REQOP_USER_ID };
    static const int kBytes[] = { EE_WON_REQOP_CONTEST_REPLY, EE_WON_REQOP_PING_TEXT };
    string out;
    if (o) out = Hex(o->mCurAddr);
    else {
        string a;
        ee_won_hostop_bytes(r, EE_WON_REQHOST_CUR_ADDR, 0, DescribeSink, &a);
        out = Hex(a);
    }
    char b[40];
    for (int i = 0; i < 3; i++) {
        long long n = o ? ee_won_reqop_num(o->mRustOp, kNums[i]) : ee_won_hostop_num(r, kNums[i], 0);
        snprintf(b, sizeof(b), " %lld", n);
        out += b;
    }
    for (int i = 0; i < 2; i++) {
        string d;
        bool has = o ? ee_won_reqop_bytes(o->mRustOp, kBytes[i], DescribeSink, &d)
                     : ee_won_hostop_bytes(r, kBytes[i], 0, DescribeSink, &d);
        out += " " + (has ? Hex(d) : string("null"));
    }
    return out;
}
static string ReqStep(OrigReq::Op* o, EeWonHostOp* r, const Step& s)
{
    S.mTrace.clear();
    S.mCalls = 0;
    string ret = "-";
    switch (s.mKind) {
    case 0:
        S.mMode = (int)s.mA;
        S.mAuth = (int)s.mB;
        S.mStatus = 100000;
        if (o) OrigReq::RunHook(*o); else ee_won_hostop_run(r);
        break;
    case 1: case 2: case 3: {
        int done = ++S.mSerial;
        int param = s.mKind == 3 ? 7 : s.mKind;
        bool b = o ? OrigReq::CallbackHook(*o, done, param) : ee_won_hostop_callback(r, param, P(done));
        ret = b ? "1" : "0";
        break;
    }
    case 4: if (o) OrigReq::CleanupHook(*o); else ee_won_hostop_cleanup(r); break;
    case 5: {
        int u = s.mA ? NewObj() : 0;
        if (o) OrigReq::Assign(o->mUDPSocket, u); else RSet(r, EE_WON_REQHOST_UDP_SOCKET, 0, string(), NULL, u);
        if (u) S.mOwn[u]--;
        break;
    }
    case 6: ret = ReqResults(o, r); break;
    }
    string d;
    if (o) d = OrigReq::Describe(*o);
    else ee_won_hostop_describe(r, DescribeSink, &d);
    return ret + "|" + S.mTrace + "|" + d;
}
// the sequence's op: kind, its argument, a server context (else one address) and the address
static void ReqSetup(const string& inHead, OrigReq::Op** outOrig, EeWonHostOp** outRust)
{
    int aKind = inHead[0], anArg = inHead[1];
    bool aContext = inHead[2] != 0;
    string anAddr(kAddrs[(int)inHead[3]], 6);
    S.mReq = true;
    if (outOrig) {
        OrigReq::Op* o = new OrigReq::Op(NewReqOp(aKind, anArg));
        S.mLfs = ee_won_reqop_length_field_size(o->mRustOp);
        if (aContext) {
            int c = NewObj();
            OrigReq::Assign(o->mServerContext, c);
            S.mOwn[c]--;
        } else {
            o->mAddrList.push_back(anAddr);
            o->mAddrItr = o->mAddrList.begin();
        }
        *outOrig = o;
        return;
    }
    EeWonReqOp* aRustOp = NewReqOp(aKind, anArg);
    S.mLfs = ee_won_reqop_length_field_size(aRustOp);
    EeWonHostOp* r = ee_won_hostop_request(aRustOp);
    ee_won_hostop_bind(r, HostFn, NULL);
    if (aContext) {
        int c = NewObj();
        RSet(r, EE_WON_REQHOST_CONTEXT, 0, string(), NULL, c);
        S.mOwn[c]--;
    } else {
        RSet(r, EE_WON_REQHOST_ADDR, 0, string(), &anAddr);
    }
    *outRust = r;
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
    S.mLine = 0;
    S.mReq = false;
    S.mHttp = false;
    S.mDoc = false;
    S.mAuth = 0;
    S.mRands.clear();
    S.mAddrs.clear();
    S.mOwn.clear();
    S.mKinds.clear();
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
    for (int i = 0; i < inSteps; i++)
        steps.push_back(inOp == 'M' ? MakePingStep() : inOp == 'F' ? MakePushStep() : inOp == 'R' ? MakeReqStep()
                        : inOp == 'H' ? MakeHttpStep() : inOp == 'D' ? MakeDocStep() : MakeFtpStep());
    string addr(kAddrs[Pick(6)], 6);
    if (inOp == 'P') { // FTPGetOp(url, extract): the URL's index and the flag
        addr.assign(2, '\0');
        addr[0] = (char)Pick(4);
        addr[1] = (char)Pick(2);
    }
    if (inOp == 'R') { // the request's op, its argument, a server context or one address, the address
        addr.assign(4, ' ');
        addr[0] = (char)Pick(3);
        addr[1] = (char)Pick(4);
        addr[2] = (char)Pick(2);
        addr[3] = (char)Pick(6);
    }
    if (inOp == 'H') { // HTTPGetOp(url) or (host, path): the URL's or host's index, the constructor, the path's
        addr.assign(3, '\0');
        addr[0] = (char)Pick(4);
        addr[1] = (char)Pick(2);
        addr[2] = (char)Pick(4);
    }
    if (inOp == 'D') { // GetHTTPDocumentOp(product, type): the type (3: none), the product's index
        addr.assign(2, '\0');
        addr[0] = (char)(Pick(8) ? Pick(3) : 3);
        addr[1] = (char)Pick(3);
    }
    if (inPrint) printf("S %c %u %d %s\n", inOp, aSeed, inSteps, Hex(addr).c_str());
    // the original
    std::vector<string> orig;
    Reset(aSeed);
    if (inOp == 'M') {
        OrigPing::Op p;
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(PingStep(&p, NULL, steps[i]));
    } else if (inOp == 'F') {
        OrigPush::Op f(addr);
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(PushStep(&f, NULL, steps[i]));
    } else if (inOp == 'R') {
        OrigReq::Op* q = NULL;
        ReqSetup(addr, &q, NULL);
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(ReqStep(q, NULL, steps[i]));
        delete q;
    } else if (inOp == 'D') {
        OrigDoc::Op d;
        d.mDocumentType = addr[0];
        d.mProductLower = kDocProducts[(int)addr[1]];
        if (addr[0] < 3)
            for (int w = 0; w < 2; w++) d.mDocuments[w] = DocPath(addr[0], addr[1], w);
        S.mDoc = true;
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(DocStep(&d, NULL, steps[i]));
    } else if (inOp == 'H') {
        OrigHttp::Op h;
        if (addr[1]) { h.mHost = kHttpHosts[(int)addr[0]]; h.mRemotePath = kHttpPaths[(int)addr[2]]; }
        else OrigHttp::FromURL(h, kHttpUrls[(int)addr[0]]);
        S.mHttp = true;
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(HttpStep(&h, NULL, steps[i]));
    } else {
        OrigFtp::Op f;
        FtpMsg<OrigFtp::Op>::FromURL(f, kFtpUrls[(int)addr[0]], addr[1] != 0);
        for (size_t i = 0; i < steps.size(); i++) orig.push_back(FtpStep(&f, NULL, steps[i]));
    }
    if (inPrint) {
        for (size_t i = 0; i < steps.size(); i++) printf("%s => %s\n", StepText(inOp, steps[i]).c_str(), orig[i].c_str());
        return;
    }
    // the Rust op
    Reset(aSeed);
    EeWonHostOp* r;
    if (inOp == 'M') r = ee_won_multiping_new();
    else if (inOp == 'F') r = ee_won_filepush_new(reinterpret_cast<const uint8_t*>(addr.data()));
    else if (inOp == 'R') ReqSetup(addr, NULL, &r);
    else if (inOp == 'D') {
        S.mDoc = true;
        string p = kDocProducts[(int)addr[1]];
        // the WON directory: none (the cache file names are the original half's DocPath)
        r = ee_won_httpdoc_new(addr[0], reinterpret_cast<const uint8_t*>(p.data()), p.size(), NULL, 0);
    } else if (inOp == 'H') {
        S.mHttp = true;
        if (addr[1]) {
            const char* h = kHttpHosts[(int)addr[0]];
            const char* p = kHttpPaths[(int)addr[2]];
            r = ee_won_httpget_new(reinterpret_cast<const uint8_t*>(h), strlen(h), reinterpret_cast<const uint8_t*>(p),
                                   strlen(p));
        } else {
            const char* u = kHttpUrls[(int)addr[0]];
            r = ee_won_httpget_new_url(reinterpret_cast<const uint8_t*>(u), strlen(u));
        }
    } else {
        const char* u = kFtpUrls[(int)addr[0]];
        r = ee_won_ftpget_new_url(reinterpret_cast<const uint8_t*>(u), strlen(u), addr[1] != 0);
    }
    ee_won_hostop_bind(r, HostFn, NULL);
    for (size_t i = 0; i < steps.size(); i++) {
        string got = inOp == 'M'   ? PingStep(NULL, r, steps[i])
                     : inOp == 'F' ? PushStep(NULL, r, steps[i])
                     : inOp == 'R' ? ReqStep(NULL, r, steps[i])
                     : inOp == 'H' ? HttpStep(NULL, r, steps[i])
                     : inOp == 'D' ? DocStep(NULL, r, steps[i])
                                   : FtpStep(NULL, r, steps[i]);
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
        for (int i = 0; i < aPrint; i++) Sequence('P', 8 + Pick(40), true);
        for (int i = 0; i < aPrint; i++) Sequence('R', 8 + Pick(40), true);
        for (int i = 0; i < 5 * aPrint; i++) Sequence('H', 8 + Pick(40), true);
        for (int i = 0; i < 3 * aPrint; i++) Sequence('D', 8 + Pick(30), true);
        return 0;
    }
    for (int i = 0; i < aCount; i++) Sequence('M', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('F', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('P', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('R', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('H', 8 + Pick(40), false);
    for (int i = 0; i < aCount; i++) Sequence('D', 8 + Pick(30), false);
    printf("won_hostop_test: %lld checks, %d failed\n", sChecks, sFailures);
    return sFailures;
}
