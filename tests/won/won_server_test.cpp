// WONServer (ServerRequestOp, ServerOp, PingOp, MultiPingOp, ServerContext, ServerConnection): the original's
// statements (copied from the C++ before the port, the objects replaced by a scripted environment: an address
// list, a session, scripted connects, peer auths, requests, sends, receives, replies, response checks and time
// left, every primitive logged) against ee_won's server module through its C ABI. ServerRequestOp's RunHook,
// blocking and asynchronous, and its CallbackHook over generated scripts: both traces and the final members must
// agree. The ping requests against the original's WriteBuffer statements, the ping replies (generated, cut at every
// length, mutated) against the original's ReadBuffer statements; MultiPingOp's rules, order and the context's
// shuffle. `--print N` prints N scenarios in the golden's format (rust/crates/ee_won/src/golden_server.txt);
// `--seed S` changes the scenarios. The exit code is the number of failed checks.
#include <windows.h>
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
#include "rust/ee_rust.h"

using namespace WONAPI;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 12345;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }

enum {
    S_SUCCESS = 0, S_PENDING = 100000, S_TIMEDOUT = 100001, S_KILLED = 100002, S_NONE = 100004,
    S_PEERAUTH_GETCERT = 100053, S_FAILEDALL = 100054, S_NOSERVERS = 100055, S_NEEDAUTH = 100056,
    S_TRYNEXT = 100057, S_SEND = 100058, S_RECV = 100059, S_EXIT = 100060, S_EXPIRED = 100061,
    S_GETCERT = 100062, S_UNPACK = 100064
};

static const char* const kResponses[] = {
    "", "\x01\0\0\0\x0f\0\0\0\0\0", "\x01\0\0\0\x0e\0\0\0\0\0", "\x01\0\0\0\x0f\0\0\0\0", "\x02\0\0\0\x0f\0\0\0\0\0\0\0",
    "\x01\0\0\0\x0f\0\0\0\xff\xff\xff\xff"
};
static const size_t kResponseLengths[] = { 0, 10, 10, 9, 12, 12 };

// ---- the scripted environment

struct Scenario {
    int mAddrs, mAsync, mAuthType, mHasContext, mHasAuthContext;
    int mLast, mUseLast, mReusing, mExpired;
    std::vector<unsigned> mT;            // time left
    std::vector<int> mS;                 // load_session
    std::vector<int> mC;                 // connect_block
    std::vector<int> mPOk, mPSt, mPCert; // peer_auth_block
    std::vector<int> mG;                 // get_next_request
    std::vector<int> mD;                 // send_block
    std::vector<int> mR;                 // recv_block
    std::vector<int> mB;                 // responses (recv and take_op_response)
    std::vector<int> mK;                 // check_response (S_THROW: throws)
    struct Done { int killed, status, param, cert, recv; };
    std::vector<Done> mDone;
};
static const int S_THROW = 0x7fffffff;

struct Env {
    const Scenario* mS;
    size_t mT, mSi, mC, mP, mG, mD, mR, mB, mK;
    int mStatus, mIdx, mSession, mResponse, mCalls;
    int mLast, mCert;
    bool mUseLast, mReusing, mExpired;
    std::string mLog;
    void Init(const Scenario& s)
    {
        mS = &s;
        mT = mSi = mC = mP = mG = mD = mR = mB = mK = 0;
        mStatus = S_PENDING;
        mIdx = 0;
        mSession = 0;
        mResponse = 0;
        mCalls = 0;
        mLast = s.mLast;
        mCert = 0;
        mUseLast = s.mUseLast != 0;
        mReusing = s.mReusing != 0;
        mExpired = s.mExpired != 0;
        mLog.clear();
    }
    void Log(const char* inFormat, long long v = 0)
    {
        char aBuffer[64];
        snprintf(aBuffer, sizeof(aBuffer), inFormat, v);
        if (!mLog.empty()) mLog += ' ';
        mLog += aBuffer;
        mCalls++;
    }
    bool Over() const { return mCalls > 300; }
    template <class T> static T Pop(const std::vector<T>& v, size_t& i, T inDefault)
    {
        return i < v.size() ? v[i++] : inDefault;
    }
    // the primitives
    int Status() { return mStatus; }
    unsigned TimeLeft()
    {
        unsigned v = Over() ? 0 : Pop(mS->mT, mT, 0u);
        Log("t%lld", v);
        return v;
    }
    bool IsAsync() { return mS->mAsync != 0; }
    int AuthType() { return mS->mAuthType; }
    void Finish(int s) { Log("F%lld", s); mStatus = s; }
    void FinishSaveSession(int s) { Log("V%lld", s); mStatus = s; }
    void Kill() { Log("K"); mStatus = S_KILLED; }
    bool PrepareAddresses() { Log("p"); return mS->mAddrs > 0; }
    void Rewind() { Log("w"); mIdx = 0; }
    bool AtBegin() { return mIdx == 0; }
    bool AtEnd() { return mIdx >= mS->mAddrs || Over(); }
    bool HasContext() { return mS->mHasContext != 0; }
    bool HasAuthContext() { return mS->mHasAuthContext != 0; }
    void NotifyFailed() { Log("n%lld", mIdx); }
    void NextAddr() { Log("a%lld", mIdx); mIdx++; }
    void Retreat() { Log("r"); if (mIdx > 0) mIdx--; }
    void ResetServer() { Log("z"); }
    void LoadSession() { mSession = Pop(mS->mS, mSi, 0); Log("s%lld", mSession); }
    void ClearSession() { Log("c"); mSession = 0; }
    bool HasSession() { return mSession != 0; }
    void AddCrypt() { Log("e"); }
    void QueueConnect() { Log("q"); }
    int ConnectBlock() { int v = Pop(mS->mC, mC, 0); Log("C%lld", v); return v; }
    void PeerAuthAsync() { Log("pa"); }
    bool PeerAuthBlock(int* outStatus, int* outCert)
    {
        size_t i = mP;
        int ok = Pop(mS->mPOk, mP, 0);
        *outStatus = i < mS->mPSt.size() ? mS->mPSt[i] : 5;
        *outCert = i < mS->mPCert.size() ? mS->mPCert[i] : 0;
        Log("P%lld", ok);
        if (ok) mSession = 1;
        return ok != 0;
    }
    int GetNextRequest() { int v = Over() ? 0 : Pop(mS->mG, mG, 0); Log("G%lld", v); return v; }
    void QueueSend() { Log("qs"); }
    int SendBlock() { int v = Pop(mS->mD, mD, 0); Log("D%lld", v); return v; }
    void QueueRecv() { Log("qr"); }
    int RecvBlock()
    {
        int v = Over() ? S_TIMEDOUT : Pop(mS->mR, mR, (int)S_TIMEDOUT);
        Log("R%lld", v);
        if (v == 0) mResponse = Pop(mS->mB, mB, 0);
        return v;
    }
    const char* Response(size_t* outLength) { *outLength = kResponseLengths[mResponse]; return kResponses[mResponse]; }
    int CheckResponse(bool* outThrew)
    {
        int v = Pop(mS->mK, mK, 0);
        *outThrew = v == S_THROW;
        if (*outThrew) Log("Kx");
        else Log("K%lld", v);
        return *outThrew ? 0 : v;
    }
    void TakeOpSession() { Log("o"); mSession = 1; }
    void TakeOpResponse() { mResponse = Pop(mS->mB, mB, 0); Log("b%lld", mResponse); }
};

// ---- the original's statements (ServerRequestOp.cpp before the port), over the environment

struct Orig {
    Env& e;
    explicit Orig(Env& inEnv) : e(inEnv) {}
    void SetLastServerError(int inError)
    {
        if (e.mLast == S_NONE) e.mUseLast = true;
        else if (inError != e.mLast) e.mUseLast = false;
        e.mLast = inError;
    }
    bool TryConnect()
    {
        if (e.HasSession()) {
            e.AddCrypt();
            e.mReusing = true;
        }
        if (e.IsAsync()) {
            e.QueueConnect();
            this->Send();
            return true;
        }
        int aStatus = e.ConnectBlock();
        if (aStatus == 0) return true;
        this->SetLastServerError(aStatus);
        return false;
    }
    bool TryPeerToPeerAuth()
    {
        if (e.IsAsync()) {
            e.PeerAuthAsync();
            return true;
        }
        int aStatus, aCert;
        if (e.PeerAuthBlock(&aStatus, &aCert)) return true;
        if (aStatus == S_PEERAUTH_GETCERT) {
            e.mCert = aCert;
            e.Finish(S_GETCERT);
        } else this->SetLastServerError(aStatus);
        return false;
    }
    bool TryNextServer()
    {
        if (e.Status() != S_PENDING) return false;
        for (;;) {
            e.ResetServer();
            if (!e.AtBegin() && !e.mExpired && e.HasContext()) e.NotifyFailed();
            if (e.AtEnd()) {
                if (e.mUseLast) e.Finish(e.mLast);
                else e.Finish(S_FAILEDALL);
                return false;
            }
            e.NextAddr();
            bool wasReusingSession = e.mReusing;
            e.mReusing = false;
            e.mExpired = false;
            if (!wasReusingSession && e.AuthType() == 1) e.LoadSession();
            else e.ClearSession();
            if (e.HasSession() || e.AuthType() == 0) {
                if (this->TryConnect()) return true;
            } else {
                if (this->TryPeerToPeerAuth()) return true;
                if (e.Status() != S_PENDING) return false;
            }
            if (e.TimeLeft() <= 0) {
                e.Finish(S_TIMEDOUT);
                return false;
            }
        }
    }
    int Send()
    {
        for (;;) {
            int aReqStatus = e.GetNextRequest();
            if (aReqStatus != S_SEND && aReqStatus != S_RECV) return aReqStatus;
            if (e.IsAsync()) {
                e.QueueSend();
            } else {
                int aStatus = e.SendBlock();
                if (aStatus != 0) {
                    this->SetLastServerError(aStatus);
                    return S_TRYNEXT;
                }
            }
            if (aReqStatus != S_RECV) continue;
            if (e.IsAsync()) e.QueueRecv();
            return aReqStatus;
        }
    }
    bool CheckExpiredSession()
    {
        size_t aLength;
        const char* aData;
        if (!e.mReusing) return false;
        aData = e.Response(&aLength);
        if (aLength < 10) return false;
        try {
            ReadBuffer aBuf(aData, aLength);
            unsigned long aServiceType = aBuf.ReadLong();
            unsigned long aMessageType = aBuf.ReadLong();
            if (aServiceType == 1 && aMessageType == 15) {
                e.mExpired = true;
                e.Retreat();
                return true;
            }
        } catch (ReadBufferException&) {}
        return false;
    }
    int Recv()
    {
        for (;;) {
            int aStatus = e.RecvBlock();
            if (aStatus != 0) {
                this->SetLastServerError(aStatus);
                return S_TRYNEXT;
            }
            if (this->CheckExpiredSession()) return S_EXPIRED;
            bool aThrew;
            aStatus = e.CheckResponse(&aThrew);
            if (aThrew) {
                this->SetLastServerError(S_UNPACK);
                return S_TRYNEXT;
            }
            if (aStatus != 100059) return aStatus;
        }
    }
    void RunHook()
    {
        e.mLast = e.mCert = S_NONE;
        e.mUseLast = false;
        if (!e.PrepareAddresses()) {
            e.Finish(S_NOSERVERS);
            return;
        }
        e.Rewind();
        e.mReusing = false;
        e.mExpired = false;
        if (e.AuthType() != 0 && !e.HasAuthContext()) {
            e.Finish(S_NEEDAUTH);
            return;
        }
        if (e.IsAsync()) {
            this->TryNextServer();
            return;
        }
        while (this->TryNextServer()) {
            for (;;) {
                int aStatus;
                do { aStatus = this->Send(); } while (aStatus == 100058);
                if (aStatus == 100057) break;
                do { aStatus = this->Recv(); } while (aStatus == 100059);
                if (aStatus == 100057 || aStatus == 100061) break;
                if (aStatus == 100058) continue;
                e.FinishSaveSession(aStatus);
                return;
            }
            if (e.TimeLeft() <= 0) {
                e.Finish(S_TIMEDOUT);
                return;
            }
        }
    }
    bool CallbackHook(const Scenario::Done& d)
    {
        if (d.killed) {
            e.Kill();
            return true;
        }
        if (d.status != S_SUCCESS) {
            if (d.param == 2 && d.status == S_PEERAUTH_GETCERT) {
                e.mCert = d.cert;
                e.Finish(S_GETCERT);
                return true;
            }
            this->SetLastServerError(d.status);
            this->TryNextServer();
            return true;
        }
        switch (d.param) {
        case 2:
            e.TakeOpSession();
            this->Send();
            break;
        case 1: {
            if (d.recv) {
                e.TakeOpResponse();
                if (this->CheckExpiredSession()) {
                    this->TryNextServer();
                    return true;
                }
                bool aThrew;
                int status = e.CheckResponse(&aThrew);
                if (aThrew) {
                    this->SetLastServerError(S_UNPACK);
                    this->TryNextServer();
                } else {
                    if (status == S_TRYNEXT) this->TryNextServer();
                    else if (status == S_SEND) this->Send();
                    else if (status == S_RECV) e.QueueRecv();
                    else if (status != S_EXIT) e.FinishSaveSession(status);
                }
            }
            break;
        }
        default: return false;
        }
        return true;
    }
};

// ---- the hooks over the environment

static Env& E(void* c) { return *static_cast<Env*>(c); }
static int32_t HStatus(void* c) { return E(c).Status(); }
static uint32_t HTimeLeft(void* c) { return E(c).TimeLeft(); }
static bool HIsAsync(void* c) { return E(c).IsAsync(); }
static int32_t HAuthType(void* c) { return E(c).AuthType(); }
static void HFinish(void* c, int32_t s) { E(c).Finish(s); }
static void HFinishSave(void* c, int32_t s) { E(c).FinishSaveSession(s); }
static void HKill(void* c) { E(c).Kill(); }
static bool HPrepare(void* c) { return E(c).PrepareAddresses(); }
static void HRewind(void* c) { E(c).Rewind(); }
static bool HAtBegin(void* c) { return E(c).AtBegin(); }
static bool HAtEnd(void* c) { return E(c).AtEnd(); }
static bool HHasContext(void* c) { return E(c).HasContext(); }
static bool HHasAuthContext(void* c) { return E(c).HasAuthContext(); }
static void HNotify(void* c) { E(c).NotifyFailed(); }
static void HNextAddr(void* c) { E(c).NextAddr(); }
static void HRetreat(void* c) { E(c).Retreat(); }
static void HReset(void* c) { E(c).ResetServer(); }
static void HLoadSession(void* c) { E(c).LoadSession(); }
static void HClearSession(void* c) { E(c).ClearSession(); }
static bool HHasSession(void* c) { return E(c).HasSession(); }
static void HAddCrypt(void* c) { E(c).AddCrypt(); }
static void HQueueConnect(void* c) { E(c).QueueConnect(); }
static int32_t HConnectBlock(void* c) { return E(c).ConnectBlock(); }
static void HPeerAuthAsync(void* c) { E(c).PeerAuthAsync(); }
static bool HPeerAuthBlock(void* c, int32_t* s, int32_t* k) { return E(c).PeerAuthBlock(s, k); }
static int32_t HGetNextRequest(void* c) { return E(c).GetNextRequest(); }
static void HQueueSend(void* c) { E(c).QueueSend(); }
static int32_t HSendBlock(void* c) { return E(c).SendBlock(); }
static void HQueueRecv(void* c) { E(c).QueueRecv(); }
static int32_t HRecvBlock(void* c) { return E(c).RecvBlock(); }
static void HResponse(void* c, const uint8_t** d, size_t* n)
{
    *d = reinterpret_cast<const uint8_t*>(E(c).Response(n));
}
static int32_t HCheckResponse(void* c, bool* t) { return E(c).CheckResponse(t); }
static void HTakeOpSession(void* c) { E(c).TakeOpSession(); }
static void HTakeOpResponse(void* c) { E(c).TakeOpResponse(); }

static EeWonServerReqHooks Hooks(Env& e)
{
    EeWonServerReqHooks h;
    h.ctx = &e;
    h.last_error = &e.mLast;
    h.get_cert_status = &e.mCert;
    h.use_last = &e.mUseLast;
    h.reusing = &e.mReusing;
    h.expired = &e.mExpired;
    h.status = HStatus;
    h.time_left = HTimeLeft;
    h.is_async = HIsAsync;
    h.auth_type = HAuthType;
    h.finish = HFinish;
    h.finish_save_session = HFinishSave;
    h.kill = HKill;
    h.prepare_addresses = HPrepare;
    h.rewind_addresses = HRewind;
    h.at_begin = HAtBegin;
    h.at_end = HAtEnd;
    h.has_context = HHasContext;
    h.has_auth_context = HHasAuthContext;
    h.notify_failed = HNotify;
    h.next_addr = HNextAddr;
    h.retreat_addr = HRetreat;
    h.reset_server = HReset;
    h.load_session = HLoadSession;
    h.clear_session = HClearSession;
    h.has_session = HHasSession;
    h.add_crypt = HAddCrypt;
    h.queue_connect = HQueueConnect;
    h.connect_block = HConnectBlock;
    h.peer_auth_async = HPeerAuthAsync;
    h.peer_auth_block = HPeerAuthBlock;
    h.get_next_request = HGetNextRequest;
    h.queue_send = HQueueSend;
    h.send_block = HSendBlock;
    h.queue_recv = HQueueRecv;
    h.recv_block = HRecvBlock;
    h.response = HResponse;
    h.check_response = HCheckResponse;
    h.take_op_session = HTakeOpSession;
    h.take_op_response = HTakeOpResponse;
    return h;
}

// ---- scenarios

static int AStatus()
{
    static const int kStatuses[] = { 0, 0, 0, S_SEND, S_RECV, S_TRYNEXT, S_EXIT, S_EXPIRED, 5, -1201, S_TIMEDOUT,
                                     S_PEERAUTH_GETCERT, 10054, -5 };
    return kStatuses[Pick(sizeof(kStatuses) / sizeof(kStatuses[0]))];
}

static Scenario Generate()
{
    Scenario s;
    s.mAddrs = Pick(5);
    s.mAsync = Pick(2);
    s.mAuthType = Pick(4) == 0 ? 0 : Pick(4);
    s.mHasContext = Pick(3) != 0;
    s.mHasAuthContext = Pick(6) != 0;
    static const int kLast[] = { S_NONE, 0, 5, -1201, S_TIMEDOUT };
    s.mLast = kLast[Pick(5)];
    s.mUseLast = Pick(2);
    s.mReusing = Pick(2);
    s.mExpired = Pick(2);
    int n = Pick(8);
    for (int i = 0; i < n; i++) {
        static const unsigned kT[] = { 0, 1, 500, 20000, 0xffffffffu };
        s.mT.push_back(kT[Pick(5)]);
    }
    n = Pick(6);
    for (int i = 0; i < n; i++) s.mS.push_back(Pick(2));
    n = Pick(6);
    for (int i = 0; i < n; i++) s.mC.push_back(Pick(3) ? 0 : AStatus());
    n = Pick(5);
    for (int i = 0; i < n; i++) {
        s.mPOk.push_back(Pick(2));
        s.mPSt.push_back(Pick(2) ? S_PEERAUTH_GETCERT : AStatus());
        s.mPCert.push_back(Pick(2) ? -4 : AStatus());
    }
    n = Pick(10);
    for (int i = 0; i < n; i++) {
        static const int kG[] = { S_SEND, S_RECV, S_RECV, S_RECV, 0, S_TRYNEXT, S_EXIT, 7 };
        s.mG.push_back(kG[Pick(8)]);
    }
    n = Pick(8);
    for (int i = 0; i < n; i++) s.mD.push_back(Pick(4) ? 0 : AStatus());
    n = Pick(10);
    for (int i = 0; i < n; i++) s.mR.push_back(Pick(3) ? 0 : AStatus());
    n = Pick(10);
    for (int i = 0; i < n; i++) s.mB.push_back(Pick(6));
    n = Pick(10);
    for (int i = 0; i < n; i++) s.mK.push_back(Pick(7) == 0 ? S_THROW : AStatus());
    n = s.mAsync ? Pick(8) : Pick(3);
    for (int i = 0; i < n; i++) {
        Scenario::Done d;
        d.killed = Pick(8) == 0;
        d.status = Pick(3) ? 0 : AStatus();
        d.param = Pick(6) == 0 ? Pick(5) : 1 + Pick(2);
        d.cert = Pick(2) ? -4 : AStatus();
        d.recv = Pick(5) != 0;
        s.mDone.push_back(d);
    }
    return s;
}

static std::string Join(const char* inKey, const std::vector<int>& v)
{
    std::string anOut = inKey;
    char aBuffer[16];
    for (size_t i = 0; i < v.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%d", i ? "," : "", v[i]);
        anOut += aBuffer;
    }
    return anOut;
}

static std::string Describe(const Scenario& s)
{
    char aBuffer[128];
    snprintf(aBuffer, sizeof(aBuffer), "n%d a%d t%d c%d x%d l%d u%d r%d e%d", s.mAddrs, s.mAsync, s.mAuthType,
             s.mHasContext, s.mHasAuthContext, s.mLast, s.mUseLast, s.mReusing, s.mExpired);
    std::string anOut = aBuffer;
    std::vector<int> aT(s.mT.begin(), s.mT.end());
    anOut += " " + Join("T", aT) + " " + Join("S", s.mS) + " " + Join("C", s.mC) + " " + Join("P", s.mPOk) + " " +
             Join("Q", s.mPSt) + " " + Join("Z", s.mPCert) + " " + Join("G", s.mG) + " " + Join("D", s.mD) + " " +
             Join("R", s.mR) + " " + Join("B", s.mB) + " " + Join("K", s.mK);
    for (size_t i = 0; i < s.mDone.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), " d%d:%d:%d:%d:%d", s.mDone[i].killed, s.mDone[i].status, s.mDone[i].param,
                 s.mDone[i].cert, s.mDone[i].recv);
        anOut += aBuffer;
    }
    return anOut;
}

static std::string State(const Env& e)
{
    char aBuffer[96];
    snprintf(aBuffer, sizeof(aBuffer), " = st%d l%d u%d r%d e%d k%d", e.mStatus, e.mLast, e.mUseLast ? 1 : 0,
             e.mReusing ? 1 : 0, e.mExpired ? 1 : 0, e.mCert);
    return aBuffer;
}

// the original's run of a scenario, as a trace
static std::string RunOrig(const Scenario& s)
{
    Env e;
    e.Init(s);
    Orig o(e);
    o.RunHook();
    std::string anOut = e.mLog + State(e);
    for (size_t i = 0; i < s.mDone.size(); i++) {
        e.mLog.clear();
        e.mCalls = 0;
        bool r = o.CallbackHook(s.mDone[i]);
        anOut += " | " + std::string(r ? "1 " : "0 ") + e.mLog + State(e);
    }
    return anOut;
}

static std::string RunRust(const Scenario& s)
{
    Env e;
    e.Init(s);
    EeWonServerReqHooks h = Hooks(e);
    ee_won_server_req_run(&h);
    std::string anOut = e.mLog + State(e);
    for (size_t i = 0; i < s.mDone.size(); i++) {
        e.mLog.clear();
        e.mCalls = 0;
        EeWonServerReqDone d;
        d.killed = s.mDone[i].killed != 0;
        d.status = s.mDone[i].status;
        d.param = s.mDone[i].param;
        d.cert_status = s.mDone[i].cert;
        d.is_recv = s.mDone[i].recv != 0;
        bool r = ee_won_server_req_callback(&h, &d);
        anOut += " | " + std::string(r ? "1 " : "0 ") + e.mLog + State(e);
    }
    return anOut;
}

// ---- the wire: requests and replies

static void Append(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}

static std::string Bytes(WriteBuffer& inBuffer)
{
    ConstSmartPtr<ByteBuffer> aBuffer = inBuffer.ToByteBuffer(true);
    return std::string(aBuffer->data(), aBuffer->length());
}

static void TestRequests()
{
    unsigned char aSizes[] = { 0, 1, 2, 4 };
    unsigned long aTicks[] = { 0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, 0x12345678 };
    for (size_t f = 0; f < 4; f++) {
        for (size_t t = 0; t < 6; t++)
            for (int x = 0; x < 2; x++) {
                WriteBuffer aBuf(aSizes[f], true); // PingOp::GetNextRequest
                aBuf.AppendByte(3);
                aBuf.AppendByte(1);
                aBuf.AppendByte(5);
                aBuf.AppendLong(aTicks[t]);
                aBuf.AppendBool(x != 0);
                std::string aWant = Bytes(aBuf), aGot;
                ee_won_server_ping_request(aSizes[f], (uint32_t)aTicks[t], x != 0, Append, &aGot);
                CHECK(aGot == aWant);
            }
        WriteBuffer aBuf(aSizes[f], true); // ServerConnection::SendNoOp
        aBuf.AppendByte(3);
        aBuf.AppendByte(1);
        aBuf.AppendByte(7);
        std::string aWant = Bytes(aBuf), aGot;
        ee_won_server_no_op(aSizes[f], Append, &aGot);
        CHECK(aGot == aWant);
    }
    for (int i = 0; i < 2000; i++) {
        int anId = i < 4 ? (i == 0 ? 0 : i == 1 ? -1 : i == 2 ? 0x7fff : (int)0x80000000) : (int)(Next() * 397u);
        WriteBuffer aBuf(0, true); // MultiPingOp::GetRequest
        aBuf.AppendByte(3);
        aBuf.AppendByte(1);
        aBuf.AppendByte(5);
        aBuf.AppendLong(anId);
        aBuf.AppendBool(false);
        std::string aWant = Bytes(aBuf), aGot;
        ee_won_server_multi_ping_request(anId, Append, &aGot);
        CHECK(aGot == aWant);
    }
}

struct PingFields {
    int mEnd, mFields;
    unsigned long mLag;
    std::string mApp, mLogical, mImage, mPorts, mReg, mServ;
    long mVersion, mPid;
    bool mAuth;
};

// PingOp::CheckResponse's statements, each field counted as it lands
static PingFields OrigPing(const std::string& inReply, unsigned long inNow)
{
    PingFields p;
    p.mEnd = 0;
    p.mFields = 0;
    p.mLag = 0;
    p.mVersion = p.mPid = 0;
    p.mAuth = false;
    try {
        ReadBuffer aMsg(inReply.data(), inReply.size());
        unsigned char aHeaderType = aMsg.ReadByte();
        unsigned char aServiceType = aMsg.ReadByte();
        unsigned char aMessageType = aMsg.ReadByte();
        if (aHeaderType != 3 || aServiceType != 1 || aMessageType != 6) {
            p.mEnd = 1;
            return p;
        }
        p.mLag = inNow - aMsg.ReadLong();
        p.mFields = 1;
        if (aMsg.Available() > 0) {
            aMsg.ReadString(p.mApp); p.mFields = 2;
            aMsg.ReadString(p.mLogical); p.mFields = 3;
            p.mVersion = aMsg.ReadLong(); p.mFields = 4;
            aMsg.ReadString(p.mImage); p.mFields = 5;
            p.mPid = aMsg.ReadLong(); p.mFields = 6;
            aMsg.ReadString(p.mPorts); p.mFields = 7;
            p.mAuth = aMsg.ReadBool(); p.mFields = 8;
            if (aMsg.Available() > 0) {
                aMsg.ReadString(p.mReg); p.mFields = 9;
                aMsg.ReadString(p.mServ); p.mFields = 10;
            }
        }
    } catch (ReadBufferException&) {
        p.mEnd = 2;
    }
    return p;
}

static std::string Text(const std::string& inReply, const EeWonPingReply& r, int k)
{
    return inReply.substr(r.text_at[k], r.text_len[k]);
}

static void CheckPing(const std::string& inReply, unsigned long inNow)
{
    PingFields p = OrigPing(inReply, inNow);
    EeWonPingReply r;
    ee_won_server_ping_reply(reinterpret_cast<const uint8_t*>(inReply.data()), inReply.size(), (uint32_t)inNow, &r);
    CHECK(r.end == p.mEnd);
    CHECK((int)r.fields == p.mFields);
    if (p.mFields >= 1) CHECK(r.lag == (uint32_t)p.mLag);
    // the fields read (the C++ assigns these; PingOp's members keep what was read before a throw)
    if (p.mFields >= 2) CHECK(Text(inReply, r, 0) == p.mApp);
    if (p.mFields >= 3) CHECK(Text(inReply, r, 1) == p.mLogical);
    if (p.mFields >= 4) CHECK(r.version == (int32_t)p.mVersion);
    if (p.mFields >= 5) CHECK(Text(inReply, r, 2) == p.mImage);
    if (p.mFields >= 6) CHECK(r.pid == (int32_t)p.mPid);
    if (p.mFields >= 7) CHECK(Text(inReply, r, 3) == p.mPorts);
    if (p.mFields >= 8) CHECK(r.is_auth == p.mAuth);
    if (p.mFields >= 9) CHECK(Text(inReply, r, 4) == p.mReg);
    if (p.mFields >= 10) CHECK(Text(inReply, r, 5) == p.mServ);
}

static bool OrigMultiPing(const std::string& inReply, int inId)
{
    ReadBuffer aBuf(inReply.data(), inReply.size());
    try {
        unsigned char aHeader = aBuf.ReadByte();
        unsigned char aService = aBuf.ReadByte();
        unsigned char aMsg = aBuf.ReadByte();
        if (aHeader != 3 || aService != 1 || aMsg != 6) return false;
        int anId = aBuf.ReadLong();
        if (anId != inId) return false;
        return true;
    } catch (ReadBufferException&) {
        return false;
    }
}

static void TestReplies()
{
    for (int i = 0; i < 3000; i++) {
        WriteBuffer aBuf(0, true);
        int aKind = Pick(4);
        aBuf.AppendByte(Pick(8) ? 3 : (unsigned char)Pick(256));
        aBuf.AppendByte(Pick(8) ? 1 : (unsigned char)Pick(256));
        aBuf.AppendByte(Pick(8) ? 6 : (unsigned char)Pick(256));
        unsigned long aTick = Next() * 251u;
        aBuf.AppendLong(aTick);
        if (aKind >= 1) {
            aBuf.AppendString(std::string("EE-app").substr(0, Pick(7)));
            aBuf.AppendString("logical\xe9");
            aBuf.AppendLong((long)(Next() * 7u));
            aBuf.AppendString(std::string(Pick(40), 'i'));
            aBuf.AppendLong(-(long)Pick(100000));
            aBuf.AppendString("15101,15200");
            aBuf.AppendBool(Pick(2) != 0);
            if (aKind >= 2) {
                aBuf.AppendString(std::string(Pick(3), '\0') + "reg");
                aBuf.AppendString(aKind == 3 ? std::string() : std::string("serv\x01"));
            }
        }
        std::string aReply = Bytes(aBuf);
        if (Pick(5) == 0 && aReply.size() > 8) aReply[3 + Pick(int(aReply.size()) - 3)] ^= (char)(1 + Pick(255));
        if (Pick(5) == 0) aReply += std::string(Pick(4), 'x');
        unsigned long aNow = aTick + Pick(5000) - 100;
        for (size_t aCut = 0; aCut <= aReply.size(); aCut++) {
            std::string aPart = aReply.substr(0, aCut);
            CheckPing(aPart, aNow);
            int anId = Pick(3) ? (int)aTick : (int)aTick + 1;
            CHECK(ee_won_server_multi_ping_reply(reinterpret_cast<const uint8_t*>(aPart.data()), aPart.size(), anId) ==
                  OrigMultiPing(aPart, anId));
        }
    }
}

// ---- the rules

static int sDraws[64];
static int sDrawAt;
static int32_t Draw() { return sDraws[sDrawAt++]; }
static int sOrigDrawAt;
static int OrigRand() { return sDraws[sOrigDrawAt++]; }

static void TestRules()
{
    int aStatuses[] = { 0, 5, -1, 10040, 100001, 100000, 100002, 100057, 100058, 100059, 100060, 100061, 10054, -1201 };
    const int aN = sizeof(aStatuses) / sizeof(aStatuses[0]);
    for (int i = 0; i < aN; i++) {
        int theStatus = aStatuses[i];
        int aWant; // MultiPingOp::HandleResponsePrv
        if (theStatus != 0 && theStatus != 10040) aWant = 0;
        else if (theStatus == 0) aWant = 2;
        else aWant = 1;
        CHECK(ee_won_server_multi_ping_recv_status(theStatus) == aWant);
        CHECK(ee_won_server_multi_ping_continues(theStatus) == !(theStatus != 0 && theStatus != 100001));
        for (int k = 0; k < 2; k++)
            for (int x = 0; x < 2; x++)
                for (int c = 0; c < 2; c++)
                    for (int s = 0; s < 2; s++)
                        for (int cl = 0; cl < 2; cl++) {
                            unsigned aPlan = 0; // ServerRequestOp::CleanupHook
                            if (theStatus != 0 && (theStatus == 100001 || k) && c) aPlan |= 1;
                            if (s && (cl || theStatus != 0 || k)) aPlan |= 2;
                            CHECK(ee_won_server_req_cleanup_plan(theStatus, k != 0, c != 0, s != 0, cl != 0) == aPlan);
                        }
        int aLasts[] = { 100004, 0, 5, -1201, 100001 };
        for (int l = 0; l < 5; l++)
            for (int u = 0; u < 2; u++) {
                int aLast = aLasts[l];
                bool aUse = u != 0;
                if (aLast == 100004) aUse = true; // SetLastServerError
                else if (theStatus != aLast) aUse = false;
                aLast = theStatus;
                int32_t aGotLast = aLasts[l];
                bool aGotUse = u != 0;
                ee_won_server_req_last_error(&aGotLast, &aGotUse, theStatus);
                CHECK(aGotLast == aLast && aGotUse == aUse);
            }
    }
    for (size_t inFlight = 0; inFlight < 3; inFlight++)
        for (size_t aWaiting = 0; aWaiting < 3; aWaiting++) {
            int aWant; // MultiPingOp::RecvCallback
            if (inFlight != 0) aWant = 0;
            else if (aWaiting == 0) aWant = 1;
            else aWant = 2;
            CHECK(ee_won_server_multi_ping_next(inFlight, aWaiting) == aWant);
        }
    for (int f = -2; f < 300; f++)
        for (int m = 0; m < 256; m += 1 + (m > 8) * 30) {
            unsigned char aMax = (unsigned char)m;
            CHECK(ee_won_server_multi_ping_gives_up(f, aMax) == (f >= aMax));
        }
    unsigned long aTimes[] = { 0, 1, 2999, 3000, 3001, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };
    for (int a = 0; a < 9; a++)
        for (int b = 0; b < 9; b++)
            for (int m = 0; m < 9; m++)
                for (int l = 0; l < 9; l++) {
                    DWORD aNow = aTimes[a], theStartTick = aTimes[b], aMaxPing = aTimes[m], aLeft = aTimes[l];
                    DWORD aWant; // MultiPingOp::ResponseTimeLeft
                    DWORD anElapsed = aNow - theStartTick;
                    if (anElapsed > aMaxPing) aWant = 0;
                    else {
                        DWORD aTimeLeft = aMaxPing - anElapsed;
                        aWant = aTimeLeft < aLeft ? aTimeLeft : aLeft;
                    }
                    CHECK(ee_won_server_multi_ping_time_left(aNow, theStartTick, aMaxPing, aLeft) == aWant);
                    for (int x = 0; x < 2; x++) {
                        DWORD aTime = aMaxPing; // ServerOp::ConnectTime / SendTime / RecvTime
                        if (!x && aLeft < aTime) aTime = aLeft;
                        CHECK(ee_won_server_step_time(aMaxPing, x != 0, aLeft) == aTime);
                    }
                    uint32_t aMs = 0; // ServerConnection::StartKeepAlive
                    bool aTimer = ee_won_server_keep_alive_ms(aTimes[a], &aMs);
                    CHECK(aTimer == (aTimes[a] != 0));
                    if (aTimer) CHECK(aMs == (DWORD)(aTimes[a] * 1000));
                }
    for (int t = 0; t < 4000; t++) {
        int n = Pick(12);
        std::vector<int> aTimes2;
        for (int i = 0; i < n; i++) {
            static const int kPing[] = { -1, 0, 1, 50, 50, 300, -2, 0x7fffffff, (int)0x80000000 };
            aTimes2.push_back(kPing[Pick(9)]);
        }
        std::multimap<DWORD, int> aMap; // MultiPingOp::CleanupHook
        for (int i = 0; i < n; i++) aMap.insert(std::multimap<DWORD, int>::value_type((DWORD)aTimes2[i], i));
        std::vector<size_t> aGot(n ? n : 1);
        ee_won_server_multi_ping_order(n ? &aTimes2[0] : NULL, n, &aGot[0]);
        int k = 0;
        for (std::multimap<DWORD, int>::iterator i = aMap.begin(); i != aMap.end(); ++i, ++k)
            CHECK(aGot[k] == (size_t)i->second);
        for (int i = 0; i < n; i++)
            for (int d = 0; d < 2; d++)
                CHECK(ee_won_server_multi_ping_keeps(aTimes2[i], d != 0) == (aTimes2[i] != -1 || !d));
        // ServerContext::CopyAddresses' shuffle over scripted rand() values
        for (int i = 0; i < 64; i++) sDraws[i] = Pick(4) ? Pick(0x8000) : (int)(Next() & 0x7fff);
        std::vector<int> anAddresses;
        for (int i = 0; i < n; i++) anAddresses.push_back(i);
        sOrigDrawAt = 0;
        for (std::vector<int>::size_type i = 1; i < anAddresses.size(); ++i)
            std::swap(anAddresses[i], anAddresses[OrigRand() % (i + 1)]);
        sDrawAt = 0;
        ee_won_server_shuffle(n, Draw, &aGot[0]);
        for (int i = 0; i < n; i++) CHECK(aGot[i] == (size_t)anAddresses[i]);
        CHECK(sDrawAt == sOrigDrawAt);
    }
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    if (aPrint) {
        // every scenario whose session expired (a reused session rejected) and one in four of the others
        for (int i = 0; i < aPrint;) {
            Scenario s = Generate();
            std::string aTrace = RunOrig(s);
            if (aTrace.find(" r ") == std::string::npos && Pick(4) != 0) continue;
            printf("%s\n> %s\n", Describe(s).c_str(), aTrace.c_str());
            i++;
        }
        return 0;
    }
    TestRequests();
    TestReplies();
    TestRules();
    int aScenarios = 200000;
    for (int i = 0; i < aScenarios; i++) {
        Scenario s = Generate();
        std::string anOrig = RunOrig(s), aRust = RunRust(s);
        CHECK(anOrig == aRust);
        if (anOrig != aRust && sFailures < 5)
            printf("scenario %s\n  orig %s\n  rust %s\n", Describe(s).c_str(), anOrig.c_str(), aRust.c_str());
    }
    printf("won_server_test: %lld checks, %d failures (%d scenarios)\n", sChecks, sFailures, aScenarios);
    return sFailures;
}
