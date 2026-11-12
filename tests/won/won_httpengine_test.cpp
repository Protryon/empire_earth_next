// HTTPEngine (HTTPSession's per-host connections; HTTPConnection's two queues, pipelining, retry mode, kill and op
// count): the original's statements (copied from HTTPEngine.cpp before the port, over stand-ins) against the Rust
// objects (ee_won httpengine.rs, `EeWonHttpSession` and `EeWonHttpConn`) over the same stand-ins, on generated
// scripts.
//
// The stand-ins are the HTTPGetOps, their completions, the connections' completions (the ParamCompletion holding the
// connection) and the sockets. Every call they get is logged into a trace, with their creations and frees. A running
// op is held by a "thread" list until the script ends it with a status (success, send success, an error) and a
// socket (none, the one it was given, a new one); a killed running op ends inside the kill. A user completion may
// add another op from inside (a queue changing during a kill or a callback). Objects are allocated in creation order
// from an arena. Runs of consecutive frees are compared sorted.
//
// `--print N` prints N scripts in the golden's format (rust/crates/ee_won/src/golden_httpengine.txt; the Rust replay
// is `httpengine::tests::golden`). The exit code is the number of failed checks.
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
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONStringCompare.h"
#include "rust/ee_rust.h"

using namespace WONAPI;
using std::string;

static int sFailures;
static long long sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 8181;
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

// ---- one side's world

struct FGet;
struct World {
    string mTrace;
    int mNextId;
    char* mArena;
    size_t mUsed;
    std::vector<FGet*> mThread;   // running ops, a reference each
    time_t mNow;
    void* mSession;               // the side's session (a user completion adds ops to it)
    void (*mAddOp)(void* inSession, FGet* inOp);
};
static World* sWorld;
static void Log(const string& s)
{
    if (!sWorld->mTrace.empty()) sWorld->mTrace += ' ';
    sWorld->mTrace += s;
}

struct FObj : public RefCount {
    char mKind;
    int mId;
    FObj(char k) : mKind(k), mId(sWorld->mNextId++) { Log("new:" + Name()); }
    virtual ~FObj() { Log("free:" + Name()); }
    string Name() const { return string(1, mKind) + Num(mId); }
    static void* operator new(size_t n)
    {
        void* p = sWorld->mArena + sWorld->mUsed;
        sWorld->mUsed += (n + 15) & ~(size_t)15;
        return p;
    }
    static void operator delete(void*) {}
};
static string NameOf(const FObj* o) { return o ? o->Name() : string("-"); }

struct FSock : public FObj {
    FSock() : FObj('s') {}
    void Kill() { Log("kill:" + Name()); }
};

// a completion: a user's ('u', it may add an op to the session from inside) or a connection's ('q', holding the
// connection as ParamCompletion's SmartPtr<RefCount> does)
struct FComp : public FObj {
    int mAdd;                    // a user's: 0 nothing, else add an op for host mAdd - 1
    SmartPtr<RefCount> mConn;    // a connection's
    void (*mCallback)(RefCount* inConn, FGet* inOp);
    FComp(char k) : FObj(k), mAdd(0), mCallback(NULL) {}
    void Complete(FGet* inOp);
};

static const char* kHosts[] = {"a.com", "A.COM", "b.org", "c.net", "B.org"};

struct FGet : public FObj {
    string mHost;
    bool mOnlySend, mOnlyRecv, mRunning, mFinished;
    long mStatus;
    SmartPtr<FComp> mCompletion;
    SmartPtr<FSock> mSocket;
    FGet(const string& h, long st) : FObj('g'), mHost(h), mOnlySend(false), mOnlyRecv(false), mRunning(false),
                                     mFinished(false), mStatus(st) {}
    long GetStatus() const { return mStatus; }
    SmartPtr<FComp> GetCompletion() const { return mCompletion; }
    void SetCompletion(FComp* c) { Log("setcomp:" + Name() + ":" + NameOf(c)); mCompletion = c; }
    void SetCloseConnection(bool b) { Log("close:" + Name() + ":" + Num(b)); }
    void SetDoRangeResume(bool b) { Log("range:" + Name() + ":" + Num(b)); }
    void SetSocket(FSock* s) { Log("setsock:" + Name() + ":" + NameOf(s)); mSocket = s; }
    FSock* GetSocket() { return mSocket.get(); }
    void SetOnlyRecv() { mOnlyRecv = true; mOnlySend = false; }
    unsigned long GetTimeout() const { return 1000 + mId; }
    void Hold()
    {
        this->AddRef();
        sWorld->mThread.push_back(this);
    }
    void Unhold()
    {
        std::vector<FGet*>::iterator i = std::find(sWorld->mThread.begin(), sWorld->mThread.end(), this);
        if (i != sWorld->mThread.end()) {
            sWorld->mThread.erase(i);
            this->Release();
        }
    }
    void Complete(long inStatus)
    {
        if (mFinished) return;
        mFinished = true;
        mRunning = false;
        mStatus = inStatus;
        Log("done:" + Name() + ":" + Num(inStatus));
        this->AddRef();
        SmartPtr<FComp> c = mCompletion;
        if (c.get()) c->Complete(this);
        Unhold();
        this->Release();
    }
    void RunAsync(unsigned long t)
    {
        Log("runasync:" + Name() + ":" + Num((long long)t) + ":" + Num(mOnlySend) + Num(mOnlyRecv));
        mRunning = true;
        mFinished = false;
        mStatus = WS_Pending;
        Hold();
    }
    void Kill(bool)
    {
        Log("kill:" + Name());
        if (mRunning) Complete(WS_Killed);
    }
    void ForceFinish(long s)
    {
        Log("force:" + Name() + ":" + Num(s));
        Complete(s);
    }
};
void FComp::Complete(FGet* inOp)
{
    Log("complete:" + Name() + ":" + inOp->Name());
    if (mCallback) mCallback(mConn.get(), inOp);
    if (mAdd) {
        int h = mAdd - 1;
        mAdd = 0;
        SmartPtr<FGet> op(new FGet(kHosts[h], 0));
        op->mCompletion = new FComp('u');
        if (sWorld->mSession) sWorld->mAddOp(sWorld->mSession, op);
    }
}

// ---- the original's statements (HTTPEngine.cpp before the port), over the stand-ins

typedef FGet HTTPGetOpX;
typedef SmartPtr<FGet> AsyncOpPtrX;
struct OrigConn : public FObj {
    CriticalSection mDataCrit;
    void* mSession;
    SmartPtr<FSock> mSocket;
    int mNumPipelineFailures;
    int mNumPipelineSuccesses;
    bool mDoPipeline;
    time_t mLastUseTime;
    typedef std::list<SmartPtr<FGet> > OpQueue;
    OpQueue mOpQueue[2];
    bool mRetryMode;
    FGet* mCurOp[2];
    SmartPtr<FComp> mCurOpCompletion[2];

    OrigConn(void* inSession) : FObj('n')
    {
        this->mCurOp[1] = NULL;
        this->mCurOp[0] = NULL;
        this->mDoPipeline = true;
        this->mSession = inSession;
        this->mRetryMode = false;
        this->mNumPipelineFailures = 0;
        this->mNumPipelineSuccesses = 0;
    }
    void Kill()
    {
        AutoCrit guard(this->mDataCrit);
        this->mRetryMode = false;
        this->mNumPipelineFailures = 0;
        this->mNumPipelineSuccesses = 0;
        this->mDoPipeline = true;
        for (int id=0; id<2; ++id) {
            if (this->mCurOp[id] != NULL) {
                this->mCurOp[id]->Kill(true);
                this->mCurOp[id] = NULL;
                this->mCurOpCompletion[id] = NULL;
            }
            for (OpQueue::iterator it=this->mOpQueue[id].begin(); it!=this->mOpQueue[id].end(); ++it) {
                FGet* op = it->get();
                bool wasNotPending = op->GetStatus() != WS_Pending;
                op->Kill(true);
                if (wasNotPending) op->ForceFinish(WS_Killed);
            }
            this->mOpQueue[id].clear();
        }
        if (this->mSocket.get() != NULL) {
            this->mSocket->Kill();
            this->mSocket = NULL;
        }
    }
    void QueueOp(FGet* inOp)
    {
        AutoCrit guard(this->mDataCrit);
        if (this->mDoPipeline) {
            inOp->mOnlySend = true;
            inOp->mOnlyRecv = false;
            this->QueueOp(inOp,0);
        } else {
            inOp->mOnlySend = false;
            inOp->mOnlyRecv = false;
            this->QueueOp(inOp,1);
        }
    }
    void QueueOp(FGet* inOp,int inId)
    {
        AutoCrit guard(this->mDataCrit);
        this->mOpQueue[inId].push_back(inOp);
        if (this->mOpQueue[inId].size() == 1) this->RunOp(inId);
    }
    static void StaticOpCallbackX(RefCount* inConn, FGet* inOp) { static_cast<OrigConn*>(inConn)->OpCallback(inOp); }
    void RunOp(int inId)
    {
        AutoCrit guard(this->mDataCrit);
        if (this->mRetryMode)
            return;
        if (this->mOpQueue[inId].empty())
            return;
        if (this->mCurOp[inId]!=NULL)
            return;
        SmartPtr<FGet> op=this->mOpQueue[inId].front();
        this->mOpQueue[inId].pop_front();
        this->mCurOp[inId]=op.get();
        this->mCurOpCompletion[inId]=this->mCurOp[inId]->GetCompletion();
        FComp* aComp = new FComp('q');
        aComp->mConn = this;
        aComp->mCallback = &OrigConn::StaticOpCallbackX;
        this->mCurOp[inId]->SetCompletion(aComp);
        this->mCurOp[inId]->SetCloseConnection(false);
        this->mCurOp[inId]->SetDoRangeResume(false);
        this->mCurOp[inId]->SetSocket(this->mSocket.get());
        this->mCurOp[inId]->RunAsync(this->mCurOp[inId]->GetTimeout());
    }
    void OpCallback(FGet* inOp)
    {
        int anId;
        for (anId=0;anId<2;++anId)
            if (inOp==this->mCurOp[anId])
                break;
        if (anId>=2)
            return;
        this->mCurOp[anId]=NULL;
        SmartPtr<FComp> aCompletion=this->mCurOpCompletion[anId];
        this->mCurOpCompletion[anId]=NULL;
        inOp->SetCompletion(aCompletion);

        if (!this->mRetryMode) {
            this->mSocket=inOp->GetSocket();
            if (anId==1 && this->mSocket.get()!=NULL) {
                if (++this->mNumPipelineSuccesses>2)
                    this->mNumPipelineFailures=0;
            }
            if (this->mSocket.get()==NULL && this->mDoPipeline &&
                (!this->mOpQueue[1].empty() || this->mCurOp[1]!=NULL || this->mCurOp[0]!=NULL)) {
                AutoCrit aGuard(this->mDataCrit);
                this->mRetryMode=true;
                if (this->mCurOp[0]!=NULL)
                    this->mCurOp[0]->Kill(true);
                if (this->mCurOp[1]!=NULL)
                    this->mCurOp[1]->Kill(true);
            }
        }

        if (this->mRetryMode) {
            if (anId!=0 && inOp->GetStatus()==WS_Success) {
                if (aCompletion.get()!=NULL)
                    aCompletion->Complete(AsyncOpPtrX(inOp));
            }
            else
                this->QueueOp(inOp);

            if (this->mCurOp[0]==NULL && this->mCurOp[1]==NULL) {
                AutoCrit aGuard(this->mDataCrit);
                int aLastQueue=1;
                if (this->mDoPipeline && ++this->mNumPipelineFailures>2) {
                    this->mDoPipeline=false;
                    aLastQueue=0;
                }
                for (anId=1;anId>=aLastQueue;--anId) {
                    OpQueue aQueue=this->mOpQueue[anId];
                    this->mOpQueue[anId].clear();
                    for (OpQueue::iterator anItr=aQueue.begin();anItr!=aQueue.end();++anItr)
                        this->QueueOp(anItr->get());
                }
                this->mRetryMode=false;
                this->RunOp(this->mDoPipeline?0:1);
            }
            return;
        }
        else {
            if (anId==0 && this->mSocket.get()!=NULL && inOp->GetStatus()==WS_HTTP_SendSuccess) {
                inOp->SetOnlyRecv();
                this->QueueOp(inOp,1);
            }
            else {
                if (aCompletion.get()!=NULL)
                    aCompletion->Complete(AsyncOpPtrX(inOp));
                this->mLastUseTime=sWorld->mNow;
            }
            this->RunOp(anId);
        }
    }
    int GetNumOps()
    {
        int aNumOps = 0;
        for(int i=0; i<2; i++)
        {
            aNumOps += this->mOpQueue[i].size();
            if(this->mCurOp[i]!=NULL)
                aNumOps++;
        }
        return aNumOps;
    }
};

struct OrigSession : public FObj {
    CriticalSection mDataCrit;
    typedef std::map<std::string,SmartPtr<OrigConn>,StringLessNoCase> ConnectionMap;
    ConnectionMap mConnectionMap;
    OrigSession() : FObj('e') {}
    static void Add(void* s, FGet* op) { static_cast<OrigSession*>(s)->AddOp(op); }
    void AddOp(FGet* inOp)
    {
        AutoCrit guard(this->mDataCrit);
        ConnectionMap::iterator anItr = this->mConnectionMap.insert(ConnectionMap::value_type(inOp->mHost, SmartPtr<OrigConn>())).first;
        SmartPtr<OrigConn>& stored = anItr->second;
        if (stored.get() == NULL) stored = new OrigConn(NULL);
        SmartPtr<OrigConn> connection = stored.get();
        connection->QueueOp(inOp);
    }
    void KillAll()
    {
        AutoCrit guard(this->mDataCrit);
        ConnectionMap::iterator it=this->mConnectionMap.begin();
        while(it!=this->mConnectionMap.end()) { it->second->Kill();
            ++it;
        }
    }
    int GetNumOps()
    {
        AutoCrit aCrit(this->mDataCrit);
        int aNumOps = 0;
        ConnectionMap::iterator anItr = this->mConnectionMap.begin();
        while(anItr!=this->mConnectionMap.end())
        {
            OrigConn *aConnection = anItr->second;
            aNumOps += aConnection->GetNumOps();
            ++anItr;
        }
        return aNumOps;
    }
};

// ---- the Rust objects over the same stand-ins
#ifndef EE_HTTPENGINE_ORIG_ONLY

static FObj* Obj(void* p) { return static_cast<FObj*>(static_cast<RefCount*>(p)); }
static FGet* Get(void* p) { return static_cast<FGet*>(Obj(p)); }
static void* Ref(RefCount* p) { return p; }

struct RustConn : public FObj {
    CriticalSection mDataCrit;
    EeWonHttpConn* mImpl;
    static void HLock(void* c) { static_cast<RustConn*>(c)->mDataCrit.Enter(); }
    static void HUnlock(void* c) { static_cast<RustConn*>(c)->mDataCrit.Leave(); }
    static void HAddRef(void* p) { static_cast<RefCount*>(p)->AddRef(); }
    static void HRelease(void* p) { static_cast<RefCount*>(p)->Release(); }
    static long HStatus(void* p) { return Get(p)->GetStatus(); }
    static void HKill(void* p) { Get(p)->Kill(true); }
    static void HForceFinish(void* p, long s) { Get(p)->ForceFinish(s); }
    static void* HCompletion(void* p)
    {
        FComp* c = Get(p)->GetCompletion().get();
        if (c == NULL) return NULL;
        c->AddRef();
        return Ref(c);
    }
    static void HSetCompletion(void* p, void* c) { Get(p)->SetCompletion(c ? static_cast<FComp*>(Obj(c)) : NULL); }
    static void HConnCallback(RefCount* inConn, FGet* inOp)
    {
        ee_won_http_conn_op_callback(static_cast<RustConn*>(inConn)->mImpl, Ref(inOp));
    }
    static void HSetConnCompletion(void* p, void* c)
    {
        FComp* aComp = new FComp('q');
        aComp->mConn = static_cast<RustConn*>(c);
        aComp->mCallback = &RustConn::HConnCallback;
        Get(p)->SetCompletion(aComp);
    }
    static void HPrepare(void* p, void* s)
    {
        FGet* op = Get(p);
        op->SetCloseConnection(false);
        op->SetDoRangeResume(false);
        op->SetSocket(s ? static_cast<FSock*>(Obj(s)) : NULL);
    }
    static void HRunAsync(void* p) { Get(p)->RunAsync(Get(p)->GetTimeout()); }
    static void* HSocket(void* p)
    {
        FSock* s = Get(p)->GetSocket();
        if (s == NULL) return NULL;
        s->AddRef();
        return Ref(s);
    }
    static void HSetMode(void* p, int m)
    {
        FGet* op = Get(p);
        if (m == 2) op->SetOnlyRecv();
        else { op->mOnlySend = m == 0; op->mOnlyRecv = false; }
    }
    static void HComplete(void* c, void* p) { static_cast<FComp*>(Obj(c))->Complete(Get(p)); }
    static void HSocketKill(void* s) { static_cast<FSock*>(Obj(s))->Kill(); }
    static int64_t HNow(void*) { return (int64_t)sWorld->mNow; }

    RustConn() : FObj('n')
    {
        EeWonHttpConnHooks h;
        h.context = this;
        h.lock = HLock;
        h.unlock = HUnlock;
        h.add_ref = HAddRef;
        h.release = HRelease;
        h.op_status = HStatus;
        h.op_kill = HKill;
        h.op_force_finish = HForceFinish;
        h.op_completion = HCompletion;
        h.op_set_completion = HSetCompletion;
        h.op_set_conn_completion = HSetConnCompletion;
        h.op_prepare = HPrepare;
        h.op_run_async = HRunAsync;
        h.op_socket = HSocket;
        h.op_set_mode = HSetMode;
        h.complete = HComplete;
        h.socket_kill = HSocketKill;
        h.now = HNow;
        mImpl = ee_won_http_conn_new(&h);
    }
    ~RustConn() { ee_won_http_conn_free(mImpl); }
};

struct RustSession : public FObj {
    CriticalSection mDataCrit;
    EeWonHttpSession* mImpl;
    static void HLock(void* c) { static_cast<RustSession*>(c)->mDataCrit.Enter(); }
    static void HUnlock(void* c) { static_cast<RustSession*>(c)->mDataCrit.Leave(); }
    static void HAddRef(void* p) { static_cast<RefCount*>(p)->AddRef(); }
    static void HRelease(void* p) { static_cast<RefCount*>(p)->Release(); }
    static void HHost(void* p, EeWonByteSink sink, void* sc)
    {
        const string& h = Get(p)->mHost;
        sink(sc, reinterpret_cast<const uint8_t*>(h.data()), h.size());
    }
    static void* HNewConn(void*)
    {
        RustConn* c = new RustConn;
        c->AddRef();
        return Ref(c);
    }
    static void HQueue(void* c, void* p) { ee_won_http_conn_queue_op(static_cast<RustConn*>(Obj(c))->mImpl, p); }
    static void HKill(void* c) { ee_won_http_conn_kill(static_cast<RustConn*>(Obj(c))->mImpl); }
    static int HNumOps(void* c) { return ee_won_http_conn_num_ops(static_cast<RustConn*>(Obj(c))->mImpl); }
    RustSession() : FObj('e')
    {
        EeWonHttpSessionHooks h;
        h.context = this;
        h.lock = HLock;
        h.unlock = HUnlock;
        h.add_ref = HAddRef;
        h.release = HRelease;
        h.op_host = HHost;
        h.new_conn = HNewConn;
        h.conn_queue_op = HQueue;
        h.conn_kill = HKill;
        h.conn_num_ops = HNumOps;
        mImpl = ee_won_http_session_new(&h);
    }
    ~RustSession() { ee_won_http_session_free(mImpl); }
    static void Add(void* s, FGet* op) { static_cast<RustSession*>(s)->AddOp(op); }
    void AddOp(FGet* op) { ee_won_http_session_add_op(mImpl, Ref(op)); }
    void KillAll() { ee_won_http_session_kill_all(mImpl); }
    int GetNumOps() { return ee_won_http_session_num_ops(mImpl); }
};
#endif

// ---- the script

struct Step {
    char mCmd;
    long long mA, mB, mC;
};

static int sNoneOdds;
static bool sBusy;     // a script without kills
   // a script's odds against an op ending without a socket
static Step MakeStep()
{
    Step s = {0, 0, 0, 0};
    static const char kCmds[] = "AAAAAACCCCCCCCKNT";
    static const char kBusy[] = "AAAACCCCCSSSZZNT";     // fewer kills: long pipelines, drops
    s.mCmd = sBusy ? kBusy[Pick((int)sizeof(kBusy) - 1)] : kCmds[Pick((int)sizeof(kCmds) - 1)];
    switch (s.mCmd) {
    case 'A':                                                        // AddOp: host, the op's status before it runs,
        s.mA = Pick(5);                                              //  its completion adds an op (host + 1)?
        s.mB = Pick(3) ? 0 : WS_Pending;
        s.mC = Pick(5) ? 0 : 1 + Pick(5);
        break;
    case 'C': {                                                      // a running op ends: which, status, socket
        static const long kStatus[] = {WS_Success, WS_HTTP_SendSuccess, WS_HTTP_SendSuccess, 10057};
        s.mA = Pick(8);
        s.mB = kStatus[Pick(4)];
        s.mC = Pick(sNoneOdds) == 0 ? 0 : (Pick(2) ? 1 : 2);         //  0 none, 1 its own, 2 a new one
        break;
    }
    case 'T': s.mA = 1000 + Pick(5000); break;                       // the clock
    case 'S': case 'Z': s.mA = Pick(8); break;                       // a running send ends sent, with a new
    }                                                                //  socket (S) or none (Z)
    return s;
}

static string StepText(const Step& s)
{
    return string(1, s.mCmd) + "," + Num(s.mA) + "," + Num(s.mB) + "," + Num(s.mC);
}

template <class Session> struct Runner {
    static void Do(Session& ses, const Step& s)
    {
        switch (s.mCmd) {
        case 'A': {
            SmartPtr<FGet> op(new FGet(kHosts[s.mA], (long)s.mB));
            FComp* c = new FComp('u');
            c->mAdd = (int)s.mC;
            op->mCompletion = c;
            ses.AddOp(op);
            break;
        }
        case 'C': {
            if (sWorld->mThread.empty()) break;
            FGet* op = sWorld->mThread[(size_t)(s.mA % (long long)sWorld->mThread.size())];
            if (s.mC == 0) op->mSocket = NULL;
            else if (s.mC == 2) op->mSocket = new FSock;
            op->Complete((long)s.mB);
            break;
        }
        case 'S': case 'Z': {
            std::vector<FGet*> v;
            for (size_t i = 0; i < sWorld->mThread.size(); i++)
                if (sWorld->mThread[i]->mOnlySend) v.push_back(sWorld->mThread[i]);
            if (v.empty()) break;
            FGet* op = v[(size_t)(s.mA % (long long)v.size())];
            op->mSocket = s.mCmd == 'S' ? new FSock : NULL;
            op->Complete(WS_HTTP_SendSuccess);
            break;
        }
        case 'K': ses.KillAll(); break;
        case 'N': Log("numops:" + Num(ses.GetNumOps())); break;
        case 'T': sWorld->mNow = (time_t)s.mA; break;
        }
    }
    static std::vector<string> Run(World& w, const std::vector<Step>& steps)
    {
        sWorld = &w;
        w.mTrace.clear();
        w.mNextId = 1;
        w.mUsed = 0;
        w.mNow = 1000;
        std::vector<string> out;
        {
            SmartPtr<Session> ses(new Session);
            w.mSession = ses.get();
            w.mAddOp = &Session::Add;
            w.mTrace.clear();
            for (size_t i = 0; i < steps.size(); i++) {
                Do(*ses.get(), steps[i]);
                out.push_back(w.mTrace);
                w.mTrace.clear();
            }
            // the end: everything killed, then the session let go
            ses->KillAll();
            w.mSession = NULL;
        }
        while (!w.mThread.empty()) {
            FGet* op = w.mThread.back();
            w.mThread.pop_back();
            op->Release();
        }
        out.push_back(w.mTrace);
        return out;
    }
};

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

static World sOrig, sRust;

static void Script(bool inPrint)
{
    std::vector<Step> steps;
    int n = 10 + Pick(50);
    sNoneOdds = Pick(2) ? 5 : 2;
    sBusy = Pick(2) != 0;
    if (sBusy) n += 40;
    for (int i = 0; i < n; i++) steps.push_back(MakeStep());
    std::vector<string> a = Runner<OrigSession>::Run(sOrig, steps);
    for (size_t i = 0; i < a.size(); i++) a[i] = Normal(a[i]);
#ifndef EE_HTTPENGINE_ORIG_ONLY
    std::vector<string> b = Runner<RustSession>::Run(sRust, steps);
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
    sOrig.mArena = (char*)malloc(1 << 22);
    sRust.mArena = (char*)malloc(1 << 22);
    if (aPrint) {
        for (int i = 0; i < aPrint; i++) Script(true);
        return 0;
    }
    for (int i = 0; i < 100000; i++) Script(false);
    printf("won_httpengine_test: %lld checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
