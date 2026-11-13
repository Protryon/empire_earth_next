// AuthContext's peer sessions, certificate flow and peer data (PutSession, GetSession, SetPeerData, the
// auto-refresh timer, RefreshAsync, RefreshBlock, GetCertCallback, Kill) and LightGetCertOp's member: the
// original's statements (copied from AuthContext.cpp before the port, over stand-ins) against the Rust object (ee_won
// authflow.rs, `EeWonAuthFlow`, `EeWonLightCert`) over the same stand-ins, on generated scripts.
//
// Round 15 moved the context's peer data and the waiting ops' peer data into the Rust object, so the vocabulary
// changed: the original's reads of its peer data (IsValid, GetExpireDelta) and the waiting ops' SetPeerData are no
// calls out any more and are not logged; a waiting op's peer data is logged when it completes
// (`done:l3:0:p5`); peer data is never freed in the log (the Rust side's is an Arc). The Rust side's peer data is
// real (peerobj.rs): a valid one is a certificate and key block of the test key with the private key, its clock
// delta chosen so that its expire delta at the certificates' clock (kCertNow, passed for `now`) is the stand-in's.
//
// The stand-ins are the ops (the auto-refresh timer, the certificate request, the waiting LightGetCertOps), the
// sessions and the peer data. Every call they get is logged into a trace, with their creations and frees; ops that
// run asynchronously are held by a "thread" list until the script completes them (a timer fires or is killed, a
// certificate request ends with a status and new peer data, a waiting op is finished from outside), as the timer
// and socket threads hold them. A certificate request runs pending or completes inside its Run; a waiting op may
// start another refresh from its completion (an insert into the refresh set during GetCertCallback's walk). The
// ops are allocated in creation order from an arena, so the refresh set's address order is the creation order on
// both sides. Runs of consecutive frees are compared sorted (a map's or set's clear frees in its tree's order).
//
// `--print N` prints N scripts in the golden's format (rust/crates/ee_won/src/golden_authflow.txt; the Rust replay
// is `authflow::tests::golden`). The exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "rust/ee_won.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 7171;
static unsigned int Next()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Next() % (unsigned)n); }
static string Num(long long n)
{
    char b[32];
    sprintf(b, "%lld", n);
    return b;
}

// ---- the Rust side's real peer data
static const char* const kPriv =
    "3065021900eb12ee7e116db19e460e1ce1a29dec70ee79c611454658dd020900f70c3664732d264b021855dfacffe4be2080b34dd52aab818a"
    "01ad613e700f0fe73a021900e90a2cffe3fabbe6ece0e935a39d1d5d424c542c077c261802080139dba7acd3581d";
static const long long kCertNow = 1000000000; // the certificates' clock
static const long long kExpire = 0x7f000000;  // 2037
static string Unhex(const char* h)
{
    string r;
    for (; h[0] && h[1]; h += 2) {
        int v = 0;
        for (int i = 0; i < 2; i++) v = v * 16 + (h[i] <= '9' ? h[i] - '0' : h[i] - 'a' + 10);
        r += (char)v;
    }
    return r;
}
struct Wire {
    string d;
    void B(int v) { d += (char)(v & 0xff); }
    void S(int v) { B(v); B(v >> 8); }
    void L(long long v) { S((int)(v & 0xffff)); S((int)((v >> 16) & 0xffff)); }
    void Str(const string& s) { S((int)s.size()); d += s; }
};
static EeWonAuthObj* sRustCert;
static EeWonAuthObj* sRustBlock;
static EeWonElGamal* sRustKey;
static void MakeRustKeys()
{
    string priv = Unhex(kPriv);
    string pub = priv.substr(0, priv.size() - 10); // the sequence without x (its ten bytes)
    pub[1] = (char)(pub.size() - 2);
    sRustKey = ee_won_elgamal_new();
    ee_won_elgamal_set_key(sRustKey, reinterpret_cast<const uint8_t*>(priv.data()), priv.size(), true);
    Wire c;
    c.S(1); c.L(1000); c.L(kExpire); c.L(5); c.L(42); c.S(1); c.Str(pub); c.B(1); // + a signature byte
    sRustCert = ee_won_cert_new(EE_WON_AUTH_CERTIFICATE, reinterpret_cast<const uint8_t*>(c.d.data()), c.d.size());
    Wire b;
    b.S(1); b.L(1000); b.L(kExpire); b.S(7); b.S(1); b.Str(pub); b.B(1);
    sRustBlock = ee_won_cert_new(EE_WON_AUTH_PUB_KEY_BLOCK, reinterpret_cast<const uint8_t*>(b.d.data()), b.d.size());
}
// peer data that is valid or not and expires `delta` seconds after kCertNow (0 when expired)
static EeWonAuthObj* RustPeer(bool inValid, long long inDelta)
{
    if (!inValid) return ee_won_peer_new_empty(NULL);
    return ee_won_peer_new(sRustCert, sRustBlock, sRustKey, kExpire - kCertNow - inDelta, NULL);
}

static const long kPending = 100001;   // a running op's status
static const long kKilled = 100002;    // a killed op's status

// ---- one side's world: its trace, its arena, its "threads"

struct FOp;
struct World {
    string mTrace;
    int mNextId;
    char* mArena;
    size_t mUsed;
    std::vector<FOp*> mThread;   // running async ops, a reference each
    // the next certificate request's behaviour: Run(1) completes inside with this status (else pending), Run(2)
    // returns it
    bool mCertSync;
    long mCertStatus;
    std::vector<std::pair<EeWonAuthObj*, string> > mPeers; // every peer data's Rust object, by name
    bool mIsRust;
};
static World* sWorld;
static World sOrig, sRust;
static void Log(const string& s)
{
    if (!sWorld->mTrace.empty()) sWorld->mTrace += ' ';
    sWorld->mTrace += s;
}

struct FObj : public RefCount {
    char mKind;
    int mId;
    FObj(char k) : mKind(k), mId(sWorld->mNextId++) { Log("new:" + Name()); }
    virtual ~FObj() { if (mKind != 'p') Log("free:" + Name()); }
    string Name() const { return string(1, mKind) + Num(mId); }
    static void* operator new(size_t n)
    {
        void* p = sWorld->mArena + sWorld->mUsed;
        sWorld->mUsed += (n + 15) & ~(size_t)15;
        return p;
    }
    static void operator delete(void*) {}
};

struct FSession : public FObj {
    time_t mLastUse;
    FSession(time_t t) : FObj('s'), mLastUse(t) {}
    time_t GetLastUseTime() const { Log("lastuse:" + Name()); return mLastUse; }
};

struct FPeer : public FObj {
    bool mValid;
    long long mDelta;
    EeWonAuthObj* mRust; // the Rust side's (the world's list owns it)
    FPeer(bool v, long long d) : FObj('p'), mValid(v), mDelta(d), mRust(RustPeer(v, d))
    {
        sWorld->mPeers.push_back(std::make_pair(mRust, Name()));
    }
    bool IsValid() const { return mValid; }
    long long GetExpireDelta() const { return mDelta; }
};
// the name of a Rust peer data object ("-" for none)
static string RustPeerName(EeWonAuthObj* p)
{
    if (p == NULL) return "-";
    for (size_t i = 0; i < sWorld->mPeers.size(); i++)
        if (ee_won_authobj_same(sWorld->mPeers[i].first, p)) return sWorld->mPeers[i].second;
    return "?";
}

// an op: the timer ('t'), a certificate request ('c') or a waiting LightGetCertOp ('l')
typedef void (*FDone)(void* inCtx, FOp* inOp);
struct FOp : public FObj {
    bool mRunning, mFinished, mKilled, mCertSync, mReenter;
    long mStatus, mCertStatus;
    FDone mDone;
    void* mCtx;
    ConstSmartPtr<FPeer> mPeer;   // a certificate request's result; a waiting op's peer data (the original's)
    EeWonLightCert* mLight;       // the Rust side's waiting op's member
    FOp(char k) : FObj(k), mRunning(false), mFinished(false), mKilled(false), mCertSync(sWorld->mCertSync),
                  mReenter(false), mStatus(kPending), mCertStatus(sWorld->mCertStatus), mDone(NULL), mCtx(NULL),
                  mLight(k == 'l' ? ee_won_light_cert_new() : NULL) {}
    ~FOp() { ee_won_light_cert_free(mLight); }
    // a waiting op's peer data: the Rust member's in the Rust world, else the original's member
    string PeerName() const
    {
        if (sWorld->mIsRust) {
            EeWonAuthObj* p = ee_won_light_cert_peer(mLight);
            string n = RustPeerName(p);
            ee_won_authobj_free(p);
            return n;
        }
        return mPeer.get() ? mPeer->Name() : string("-");
    }
    void SetDone(FDone d, void* c) { mDone = d; mCtx = c; }
    bool IsKilled() const { return mKilled; }
    long GetStatus() const { return mStatus; }
    ConstSmartPtr<FPeer> GetPeerData() const { return mPeer; }
    void SetPeerData(const FPeer* p) { mPeer = p; }
    void Hold()
    {
        this->AddRef();
        sWorld->mThread.push_back(this);
    }
    void Unhold()
    {
        std::vector<FOp*>::iterator i = std::find(sWorld->mThread.begin(), sWorld->mThread.end(), this);
        if (i != sWorld->mThread.end()) {
            sWorld->mThread.erase(i);
            this->Release();
        }
    }
    // the end of a run: the status, the completion, then the thread lets go
    void Complete(bool inKilled, long inStatus)
    {
        if (mFinished) return;
        mFinished = true;
        mKilled = inKilled;
        mStatus = inStatus;
        Log("done:" + Name() + ":" + Num(inStatus) + (mKind == 'l' ? ":" + PeerName() : string()));
        this->AddRef();
        if (mDone) mDone(mCtx, this);
        Unhold();
        this->Release();
    }
    void Kill(bool)
    {
        Log("kill:" + Name());
        if (mRunning) Complete(true, kKilled);
    }
    void RunAsync(long inTimeout)
    {
        Log("runasync:" + Name() + ":" + Num(inTimeout));
        mRunning = true;
        Hold();
    }
    long Run(long inMode, unsigned long inTimeout)
    {
        Log("run:" + Name() + ":" + Num(inMode) + ":" + Num((long long)inTimeout));
        if (inMode == 2) {
            mFinished = true;
            mStatus = mCertStatus;
            return mStatus;
        }
        mRunning = true;
        if (mCertSync) {
            Hold();
            Complete(false, mCertStatus);
            return mStatus;
        }
        Hold();
        return kPending;
    }
    void Finish(long inStatus)
    {
        Log("finish:" + Name() + ":" + Num(inStatus));
        if (mRunning) Complete(false, inStatus);
    }
};

// ---- the original's statements (AuthContext.cpp before the port), over the stand-ins

struct OrigCtx {
    typedef std::list<SmartPtr<FSession> > SessionList;
    struct SessionStruct { time_t mLastUseTime; SessionList mSessionList; };
    typedef std::map<IPAddr,SessionStruct> SessionMap;
    CriticalSection mDataCrit;
    std::set<SmartPtr<FOp>, SmartPtr<FOp>::Comp> mRefreshSet;
    SmartPtr<FOp> mGetCertOp;
    SmartPtr<FOp> mAutoRefreshOp;
    long mRefreshStatus;
    bool mDoAutoRefresh;
    SessionMap mSessionMap;
    SessionMap::iterator mSessionCleanItr;
    ConstSmartPtr<FPeer> mPeerData;

    OrigCtx() : mRefreshStatus(0x186a4), mDoAutoRefresh(false) { mSessionCleanItr = mSessionMap.begin(); }

    static void StaticAutoRefreshCallback(void* c, FOp* op) { static_cast<OrigCtx*>(c)->AutoRefreshCallback(op); }
    static void StaticGetCertCallback(void* c, FOp* op) { static_cast<OrigCtx*>(c)->GetCertCallback(op); }

    void PutSession(const IPAddr& address,FSession* session,time_t now)
    {
        AutoCrit lock(this->mDataCrit);
        if(session==NULL) return;
        SessionStruct& record=this->mSessionMap[address];
        record.mLastUseTime=now;
        record.mSessionList.push_back(session);
        for(int i=0;i<5;++i) {
            if(this->mSessionCleanItr==this->mSessionMap.end()) {
                this->mSessionCleanItr=this->mSessionMap.begin();
                break;
            }
            if(now-this->mSessionCleanItr->second.mLastUseTime>300)
                this->mSessionMap.erase(this->mSessionCleanItr++);
            else ++this->mSessionCleanItr;
        }
    }
    SmartPtr<FSession> GetSession(const IPAddr& address,time_t now)
    {
        AutoCrit lock(this->mDataCrit);
        SessionMap::iterator found=this->mSessionMap.find(address);
        if(found==this->mSessionMap.end()) return NULL;
        SessionStruct& record=found->second;
        record.mLastUseTime=now;
        SessionList& sessions=record.mSessionList;
        SessionList::iterator next=sessions.begin();
        while(next!=sessions.end()) {
            SmartPtr<FSession> session=*next;
            sessions.erase(next++);
            if(now-session->GetLastUseTime()<300) return session;
        }
        return NULL;
    }
    void SetPeerData(const FPeer* data)
    {
        AutoCrit lock(this->mDataCrit);
        this->mPeerData=data;
        this->mSessionMap.clear();
        this->mSessionCleanItr=this->mSessionMap.begin();
    }
    void AutoRefreshCallback(FOp* op)
    {
        AutoCrit lock(this->mDataCrit);
        if(op!=this->mAutoRefreshOp.get()) return;
        this->mAutoRefreshOp=NULL;
        if(op->IsKilled()) return;
        this->RefreshAsync(NULL,INFINITE);
    }
    void StopAutoRefresh()
    {
        AutoCrit lock(this->mDataCrit);
        if(this->mAutoRefreshOp.get()!=NULL) {
            this->mAutoRefreshOp->Kill(true);
            this->mAutoRefreshOp=NULL;
        }
    }
    void SetDoAutoRefresh(bool enabled)
    {
        AutoCrit lock(this->mDataCrit);
        this->mDoAutoRefresh=enabled;
        this->StartAutoRefresh(false);
    }
    void Kill()
    {
        AutoCrit lock(this->mDataCrit);
        if(this->mGetCertOp.get()!=NULL) this->mGetCertOp->Kill(true);
        this->StopAutoRefresh();
    }
    void StartAutoRefresh(bool force)
    {
        AutoCrit lock(this->mDataCrit);
        this->StopAutoRefresh();
        bool valid=this->mDoAutoRefresh && this->mPeerData->IsValid();
        long long aWait=ee_won_auth_auto_refresh_wait(this->mDoAutoRefresh,force,valid,
                                                      valid?(long long)this->mPeerData->GetExpireDelta():0);
        if(aWait>=0) {
            time_t wait=(time_t)aWait;
            {
                this->mAutoRefreshOp=new FOp('t');
                this->mAutoRefreshOp->SetDone(&OrigCtx::StaticAutoRefreshCallback,this);
                this->mAutoRefreshOp->RunAsync(static_cast<long>(static_cast<unsigned long>(wait)*1000UL));
            }
        }
    }
    void GetCertCallback(FOp* op)
    {
        AutoCrit lock(this->mDataCrit);
        if(op!=this->mGetCertOp.get()) return;
        ConstSmartPtr<FPeer> peerData=op->GetPeerData();
        std::set<SmartPtr<FOp>,SmartPtr<FOp>::Comp>::iterator next=this->mRefreshSet.begin();
        while(next!=this->mRefreshSet.end()) {
            FOp* waiting=next->get();
            waiting->SetPeerData(peerData);
            waiting->Finish(op->GetStatus());
            ++next;
        }
        this->mRefreshSet.clear();
        this->mGetCertOp=NULL;
        this->mRefreshStatus=op->GetStatus();
        this->StartAutoRefresh(true);
    }
    void RefreshAsync(FOp* op,DWORD timeout)
    {
        AutoCrit lock(this->mDataCrit);
        this->StopAutoRefresh();
        if(op!=NULL) {
            this->mRefreshSet.insert(op);
            op->RunAsync((long)timeout);
        }
        if(this->mGetCertOp.get()==NULL) {
            this->mGetCertOp=new FOp('c');
            this->mGetCertOp->SetDone(&OrigCtx::StaticGetCertCallback,this);
            this->mRefreshStatus=this->mGetCertOp->Run(1,60000);
        }
    }
    long RefreshBlock(DWORD timeout)
    {
        SmartPtr<FOp> op;
        op=new FOp('c');
        this->mRefreshStatus=op->Run(2,timeout);
        if(this->mRefreshStatus==0) this->StartAutoRefresh(false);
        long status=this->mRefreshStatus;
        return status;
    }
};

// ---- the Rust object over the same stand-ins
#ifndef EE_AUTHFLOW_ORIG_ONLY

static FObj* Obj(void* p) { return static_cast<FObj*>(static_cast<RefCount*>(p)); }
static FOp* Op(void* p) { return static_cast<FOp*>(Obj(p)); }
struct RustCtx;
static void RustAutoDone(void* c, FOp* op);
static void RustCertDone(void* c, FOp* op);

struct RustCtx {
    EeWonAuthFlow* mFlow;
    static void HAddRef(void* p) { Obj(p)->AddRef(); }
    static void HRelease(void* p) { Obj(p)->Release(); }
    static int64_t HLastUse(void* p) { return (int64_t) static_cast<FSession*>(Obj(p))->GetLastUseTime(); }
    static void HKill(void* p) { Op(p)->Kill(true); }
    static bool HKilled(void* p) { return Op(p)->IsKilled(); }
    static long HStatus(void* p) { return Op(p)->GetStatus(); }
    static void HRunAsync(void* p, long t) { Op(p)->RunAsync(t); }
    static long HRun(void* p, long m, unsigned long t) { return Op(p)->Run(m, t); }
    static void HFinish(void* p, long s) { Op(p)->Finish(s); }
    static void* HNewTimer(void* c)
    {
        FOp* op = new FOp('t');
        op->AddRef();
        op->SetDone(RustAutoDone, c);
        return static_cast<RefCount*>(op);
    }
    static void* HNewCert(void* c, bool completion)
    {
        FOp* op = new FOp('c');
        op->AddRef();
        if (completion) op->SetDone(RustCertDone, c);
        return static_cast<RefCount*>(op);
    }
    static EeWonAuthObj* HCertPeer(void* p)
    {
        const FPeer* peer = Op(p)->GetPeerData().get();
        return peer != NULL ? ee_won_authobj_clone(peer->mRust) : NULL;
    }

    RustCtx()
    {
        EeWonAuthFlowHooks h;
        h.context = this;
        h.add_ref = HAddRef;
        h.release = HRelease;
        h.session_last_use = HLastUse;
        h.op_kill = HKill;
        h.op_killed = HKilled;
        h.op_status = HStatus;
        h.op_run_async = HRunAsync;
        h.op_run = HRun;
        h.op_finish = HFinish;
        h.new_timer = HNewTimer;
        h.new_cert_op = HNewCert;
        h.cert_peer_data = HCertPeer;
        mFlow = ee_won_auth_flow_new(&h);
    }
    ~RustCtx() { ee_won_auth_flow_free(mFlow); }
    void PutSession(const IPAddr& a, FSession* s, time_t now)
    {
        ee_won_auth_flow_put_session(mFlow, a.GetValue(), s ? static_cast<RefCount*>(s) : NULL, (int64_t)now);
    }
    SmartPtr<FSession> GetSession(const IPAddr& a, time_t now)
    {
        void* p = ee_won_auth_flow_get_session(mFlow, a.GetValue(), (int64_t)now);
        SmartPtr<FSession> s(p ? static_cast<FSession*>(Obj(p)) : NULL);
        if (p) Obj(p)->Release();
        return s;
    }
    void SetPeerData(const FPeer* d)
    {
        SmartPtr<FPeer> keep(const_cast<FPeer*>(d)); // the stand-in is freed (unlogged); its Rust object lives on
        ee_won_auth_flow_set_peer_data(mFlow, d ? d->mRust : NULL);
    }
    void StopAutoRefresh() { ee_won_auth_flow_stop_auto_refresh(mFlow); }
    void SetDoAutoRefresh(bool b) { ee_won_auth_flow_set_do_auto_refresh(mFlow, b, kCertNow); }
    void Kill() { ee_won_auth_flow_kill(mFlow); }
    void StartAutoRefresh(bool f) { ee_won_auth_flow_start_auto_refresh(mFlow, f, kCertNow); }
    void RefreshAsync(FOp* op, DWORD t)
    {
        ee_won_auth_flow_refresh_async(mFlow, op ? static_cast<RefCount*>(op) : NULL, op ? op->mLight : NULL, t);
    }
    long RefreshBlock(DWORD t) { return ee_won_auth_flow_refresh_block(mFlow, t, kCertNow); }
};
static void RustAutoDone(void* c, FOp* op)
{
    ee_won_auth_flow_auto_refresh_callback(static_cast<RustCtx*>(c)->mFlow, static_cast<RefCount*>(op));
}
static void RustCertDone(void* c, FOp* op)
{
    ee_won_auth_flow_get_cert_callback(static_cast<RustCtx*>(c)->mFlow, static_cast<RefCount*>(op), kCertNow);
}
#endif

// ---- the script

struct Step {
    char mCmd;
    long long mA, mB, mC, mD;
};

static Step MakeStep()
{
    Step s = {0, 0, 0, 0, 0};
    static const char kCmds[] = "TTUUPPPPOONGGGGDAASSXKRRRRQBFFCCLL";
    s.mCmd = kCmds[Pick((int)sizeof(kCmds) - 1)];
    switch (s.mCmd) {
    case 'T': s.mA = 1000 + Pick(2000); break;                       // the clock
    case 'P': s.mA = Pick(6); s.mB = Pick(2000) + 500; break;        // put: address, the session's last use
    case 'U': s.mA = 298 + Pick(5); break;                           // the clock moved on by about 300 s
    case 'O': s.mA = Pick(6); s.mB = 298 + Pick(5); break;           // put a session last used about 300 s ago
    case 'N': s.mA = Pick(6); break;                                 // put no session
    case 'G': s.mA = Pick(7); break;                                 // get
    case 'D': s.mA = Pick(3) != 0; s.mB = Pick(600) - 100; break;    // peer data: valid, expire delta
    case 'A': s.mA = Pick(3) != 0; break;                            // SetDoAutoRefresh
    case 'S': s.mA = Pick(2); break;                                 // StartAutoRefresh(force)
    case 'R':                                                        // RefreshAsync: a new waiting op?, timeout,
        s.mA = Pick(4);                                              //  0 none, 1 plain, 2 re-entering
        s.mB = Pick(2) ? (long long)INFINITE : Pick(90000);          //  the timeout
        s.mC = Pick(2);                                              //  a request's Run completes inside?
        s.mD = Pick(3) ? 0 : 10000 + Pick(5);                        //  its status
        if (s.mA == 3) s.mA = 0;
        break;
    case 'Q': s.mA = Pick(4); s.mB = Pick(50000); break;             // RefreshAsync with a held waiting op
    case 'B': s.mA = Pick(60000); s.mB = Pick(2) ? 0 : 20000 + Pick(5); break;   // RefreshBlock: timeout, status
    case 'F': s.mA = Pick(4); s.mB = Pick(3) == 0; break;            // fire a held timer (killed?)
    case 'C':                                                        // a held request ends: status, peer
        s.mA = Pick(4); s.mB = Pick(3) ? 0 : 30000 + Pick(5); s.mC = Pick(3); s.mD = Pick(600) - 100;
        break;
    case 'L': s.mA = Pick(4); s.mB = 40000 + Pick(5); break;         // a held waiting op is finished
    }
    return s;
}

static string StepText(const Step& s)
{
    string r(1, s.mCmd);
    r += "," + Num(s.mA) + "," + Num(s.mB) + "," + Num(s.mC) + "," + Num(s.mD);
    return r;
}

static IPAddr Addr(long long i) { return IPAddr((long)(0x0a000001 + (i % 3)), (unsigned short)(1000 + i / 3)); }

// the n-th held op of a kind, or NULL
static FOp* Held(char k, long long n)
{
    std::vector<FOp*> v;
    for (size_t i = 0; i < sWorld->mThread.size(); i++)
        if (sWorld->mThread[i]->mKind == k) v.push_back(sWorld->mThread[i]);
    return v.empty() ? NULL : v[(size_t)(n % (long long)v.size())];
}

template <class Ctx> struct Runner {
    static void LightReenter(void* c, FOp*)
    {
        FOp* l = new FOp('l');
        SmartPtr<FOp> keep(l);
        static_cast<Ctx*>(c)->RefreshAsync(l, 7);
    }
    static void Do(Ctx& ctx, time_t& now, const Step& s)
    {
        switch (s.mCmd) {
        case 'T': now = (time_t)s.mA; break;
        case 'U': now += (time_t)s.mA; break;
        case 'O': {
            SmartPtr<FSession> ses(new FSession(now - (time_t)s.mB));
            ctx.PutSession(Addr(s.mA), ses, now);
            break;
        }
        case 'P': {
            SmartPtr<FSession> ses(new FSession((time_t)s.mB));
            ctx.PutSession(Addr(s.mA), ses, now);
            break;
        }
        case 'N': ctx.PutSession(Addr(s.mA), NULL, now); break;
        case 'G': {
            SmartPtr<FSession> got = ctx.GetSession(Addr(s.mA), now);
            Log("got:" + (got.get() ? got->Name() : string("-")));
            break;
        }
        case 'D': ctx.SetPeerData(new FPeer(s.mA != 0, s.mB)); break;
        case 'A': ctx.SetDoAutoRefresh(s.mA != 0); break;
        case 'S': ctx.StartAutoRefresh(s.mA != 0); break;
        case 'X': ctx.StopAutoRefresh(); break;
        case 'K': ctx.Kill(); break;
        case 'R': {
            sWorld->mCertSync = s.mC != 0;
            sWorld->mCertStatus = s.mD;
            SmartPtr<FOp> l;
            if (s.mA) {
                l = new FOp('l');
                if (s.mA == 2) l->SetDone(&LightReenter, &ctx);
            }
            ctx.RefreshAsync(l, (DWORD)s.mB);
            break;
        }
        case 'Q': {
            FOp* l = Held('l', s.mA);
            if (l) ctx.RefreshAsync(l, (DWORD)s.mB);
            break;
        }
        case 'B': {
            sWorld->mCertStatus = s.mB;
            long st = ctx.RefreshBlock((DWORD)s.mA);
            Log("block:" + Num(st));
            break;
        }
        case 'F': {
            FOp* t = Held('t', s.mA);
            if (t) t->Complete(s.mB != 0, s.mB ? kKilled : 0);
            break;
        }
        case 'C': {
            FOp* c = Held('c', s.mA);
            if (c) {
                if (s.mC) c->mPeer = new FPeer(s.mC == 2, s.mD);
                c->Complete(false, s.mB);
            }
            break;
        }
        case 'L': {
            FOp* l = Held('l', s.mA);
            if (l) l->Finish(s.mB);
            break;
        }
        }
    }
    // one script on a fresh world; the trace of each step
    static std::vector<string> Run(World& w, const std::vector<Step>& steps)
    {
        sWorld = &w;
        w.mIsRust = &w == &sRust;
        w.mTrace.clear();
        w.mNextId = 1;
        w.mUsed = 0;
        w.mCertSync = false;
        w.mCertStatus = 0;
        std::vector<string> out;
        time_t now = 1000;
        {
            Ctx* ctx = new Ctx;
            ctx->SetPeerData(new FPeer(false, 0));
            w.mTrace.clear();
            for (size_t i = 0; i < steps.size(); i++) {
                Do(*ctx, now, steps[i]);
                out.push_back(w.mTrace);
                w.mTrace.clear();
            }
            delete ctx;
        }
        // the threads let go of what they hold
        while (!w.mThread.empty()) {
            FOp* op = w.mThread.back();
            w.mThread.pop_back();
            op->Release();
        }
        out.push_back(w.mTrace);
        for (size_t i = 0; i < w.mPeers.size(); i++) ee_won_authobj_free(w.mPeers[i].first);
        w.mPeers.clear();
        return out;
    }
};

// runs of consecutive frees sorted
static string Normal(const string& t)
{
    std::vector<string> tok, out, run;
    size_t p = 0;
    while (p < t.size()) {
        size_t q = t.find(' ', p);
        if (q == string::npos) q = t.size();
        tok.push_back(t.substr(p, q - p));
        p = q + 1;
    }
    for (size_t i = 0; i <= tok.size(); i++) {
        if (i < tok.size() && tok[i].compare(0, 5, "free:") == 0) { run.push_back(tok[i]); continue; }
        std::sort(run.begin(), run.end());
        out.insert(out.end(), run.begin(), run.end());
        run.clear();
        if (i < tok.size()) out.push_back(tok[i]);
    }
    string r;
    for (size_t i = 0; i < out.size(); i++) { if (i) r += ' '; r += out[i]; }
    return r;
}


static void Script(bool inPrint)
{
    std::vector<Step> steps;
    int n = 5 + Pick(40);
    for (int i = 0; i < n; i++) steps.push_back(MakeStep());
    std::vector<string> a = Runner<OrigCtx>::Run(sOrig, steps);
    for (size_t i = 0; i < a.size(); i++) a[i] = Normal(a[i]);
#ifndef EE_AUTHFLOW_ORIG_ONLY
    std::vector<string> b = Runner<RustCtx>::Run(sRust, steps);
    for (size_t i = 0; i < b.size(); i++) b[i] = Normal(b[i]);
    CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        sChecks++;
        if (a[i] != b[i]) {
            sFailures++;
            if (sFailures < 10)
                printf("FAIL step %d %s\n  orig %s\n  rust %s\n", (int)i,
                       i < steps.size() ? StepText(steps[i]).c_str() : "end", a[i].c_str(), b[i].c_str());
        }
    }
#endif
    if (inPrint) {
        string line;
        for (size_t i = 0; i < a.size(); i++) {
            if (i) line += '|';
            line += (i < steps.size() ? StepText(steps[i]) : string("E")) + "=" + a[i];
        }
        printf("%s\n", line.c_str());
    }
}

int main(int argc, char** argv)
{
    int aPrint = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--print") && i + 1 < argc) aPrint = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) sSeed = (unsigned)atoi(argv[++i]);
    }
    MakeRustKeys();
    {
        EeWonAuthObj* p = RustPeer(true, 50);
        CHECK(ee_won_authobj_num(p, EE_WON_AUTHOBJ_VALID, 0, 0, 0) == 1);
        ee_won_authobj_free(p);
    }
    sOrig.mArena = (char*)malloc(1 << 22);
    sRust.mArena = (char*)malloc(1 << 22);
    if (aPrint) {
        for (int i = 0; i < aPrint; i++) Script(true);
        return 0;
    }
    for (int i = 0; i < 100000; i++) Script(false);
    printf("won_authctx_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
