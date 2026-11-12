// WON's async machinery: AsyncOp's runs, finishes, kills and completions through an AsyncOpContainer (queue, pump,
// purge, the op being pumped queued again, the pump's op limit), the TimerThread (timeouts due and replaced, removed,
// purged; its own thread), and Thread (start, InThisThread, stop, auto-delete), one line per event with every op's
// reference count, then (round 13) the op's remaining transitions, its time left and AsyncOpTracker /
// AsyncOpWithTracker, then SocketThreadSimple's adds, pumps, removes and purges, compared with rust/crates/ee_won/src/golden_asyncop.txt. The golden was recorded over the C++
// classes before their move to Rust (rust/crates/ee_won/src/thread.rs). Deadlines are 0 ms or an hour, so the
// lines do not depend on the clock. `--print` prints the lines instead. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <string.h>
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
#include "ee/wonapi/WONAPICore.h"
#include "platform/PlatThread.h"
#include "platform/PlatTime.h"

using namespace WONAPI;

static std::vector<std::string> sLines;
static PlatMutex* sLinesLock = PlatMutexCreate();   // the auto-deleting thread logs its destruction from itself
static void Log(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    sLines.push_back(inLine);
    PlatMutexUnlock(sLinesLock);
}
static bool Logged(const std::string& inLine)
{
    PlatMutexLock(sLinesLock);
    bool aFound = false;
    for (size_t i = 0; i < sLines.size(); i++)
        if (sLines[i] == inLine) aFound = true;
    PlatMutexUnlock(sLinesLock);
    return aFound;
}

// an op that logs its hooks and does what its script says in RunHook: 0 stay pending, 1 finish with success,
// 2 finish with WS_TimedOut... (the status is mFinishWith when mFinishInRun)
class TestOp : public AsyncOp {
public:
    TestOp(const char* inName) : mName(inName), mFinishInRun(false), mFinishWith(WS_Success), mRequeue(false) {}
    virtual ~TestOp() { Log(this->mName + " destroyed"); }
    virtual void RunHook()
    {
        Log(this->mName + " RunHook status " + std::to_string(this->GetStatus()));
        if (this->mRequeue) {
            this->mRequeue = false;
            this->QueueInOpContainer();
            Log(this->mName + " requeued itself");
        }
        if (this->mFinishInRun)
            this->Finish(this->mFinishWith);
    }
    virtual void CleanupHook() { Log(this->mName + " CleanupHook"); }
    std::string mName;
    bool mFinishInRun;
    WONStatus mFinishWith;
    bool mRequeue;
};

class Done : public CompletionBase<AsyncOpPtr> {
public:
    virtual void Complete(AsyncOpPtr inOp)
    {
        TestOp* anOp = static_cast<TestOp*>(inOp.get());
        Log(anOp->mName + " completed status " + std::to_string(anOp->GetStatus()) + " killed "
            + std::to_string(anOp->IsKilled() ? 1 : 0));
    }
};

static std::string State(TestOp* inOp)
{
    return inOp->mName + " status " + std::to_string(inOp->GetStatus()) + " refs " +
           std::to_string(inOp->GetRefCount() - 1) + " pending " + std::to_string(inOp->IsCompletionPending() ? 1 : 0) +
           " killed " + std::to_string(inOp->IsKilled() ? 1 : 0);
}

static void Show(const char* inStep, TestOp* a, TestOp* b = NULL, TestOp* c = NULL)
{
    std::string s = std::string(inStep) + ": " + State(a);
    if (b) s += " | " + State(b);
    if (c) s += " | " + State(c);
    Log(s);
}

static SmartPtr<TestOp> NewOp(const char* inName, AsyncOpContainer* inContainer, CompletionBase<AsyncOpPtr>* inDone)
{
    SmartPtr<TestOp> anOp = new TestOp(inName);
    anOp->SetContainer(inContainer);
    anOp->SetCompletion(inDone);
    return anOp;
}

static void Containers()
{
    SmartPtr<Done> aDone = new Done;
    AsyncOpContainer* c = new AsyncOpContainer;
    {
        SmartPtr<TestOp> a = NewOp("a", c, aDone), b = NewOp("b", c, aDone), d = NewOp("d", c, aDone);
        a->mFinishInRun = true;
        // async runs: queued until pumped
        a->RunAsync(-1);
        b->RunAsync(-1);
        Show("queued", a, b);
        c->Pump(0);
        Show("pumped", a, b);
        c->Pump(0);
        Show("pumped again", a, b);
        // b finishes from outside: its completion is queued and delivered by the next pump
        b->Finish(WS_Success);
        Show("b finished", b);
        c->Pump(0);
        Show("b delivered", b);
        // a run while pending, a run after completing
        Log("rerun a -> " + std::to_string(a->Run(OP_MODE_ASYNC, -1)));
        c->Pump(0);
        Show("a rerun pumped", a);
        // kill a queued op without finishing, then with
        d->RunAsync(-1);
        d->Kill(false);
        Show("d killed", d);
        c->Pump(0);
        Show("d pumped", d);
        d->Kill(true);
        Show("d killed finishing", d);
        c->Pump(0);
        Show("d delivered", d);
        // the op being pumped queues itself while it runs
        SmartPtr<TestOp> e = NewOp("e", c, aDone);
        e->mRequeue = true;
        e->RunAsync(-1);
        c->Pump(0);
        Show("e pumped", e);
        c->Pump(0);
        Show("e pumped again", e);
        e->Finish(WS_Killed);
        c->Pump(0);
        Show("e delivered", e);
        // the pump's op limit
        SmartPtr<TestOp> f = NewOp("f", c, aDone), g = NewOp("g", c, aDone), h = NewOp("h", c, aDone);
        f->mFinishInRun = g->mFinishInRun = h->mFinishInRun = true;
        f->RunAsync(-1);
        g->RunAsync(-1);
        h->RunAsync(-1);
        c->Pump(0, 2);
        Show("limit 2", f, g, h);
        c->Pump(0, 2);
        Show("limit 2 again", f, g, h);
        c->Pump(0);
        Show("rest", f, g, h);
        // run immediately: the run hook at once, the completion queued
        SmartPtr<TestOp> i = NewOp("i", c, aDone);
        i->SetRunAsyncImmediately(true);
        i->mFinishInRun = true;
        i->RunAsync(-1);
        Show("i immediate", i);
        c->Pump(0);
        Show("i delivered", i);
        // a blocking run: hooks and completion at once
        SmartPtr<TestOp> j = NewOp("j", c, aDone);
        j->mFinishInRun = true;
        j->mFinishWith = WS_Success;
        Log("j block -> " + std::to_string(j->RunBlock(-1) ? 1 : 0));
        Show("j", j);
        // a zero timeout times out at the start of the async run
        SmartPtr<TestOp> k = NewOp("k", c, aDone);
        k->RunAsync(0);
        c->Pump(0);
        Show("k zero timeout", k);
        c->Pump(0);
        Show("k delivered", k);
        // purge with ops queued: pending ones killed, every one completed
        SmartPtr<TestOp> l = NewOp("l", c, aDone), m = NewOp("m", c, aDone), n = NewOp("n", c, aDone);
        l->RunAsync(-1);
        m->RunAsync(-1);
        n->RunAsync(-1);
        n->Finish(WS_Success);
        Show("before purge", l, m, n);
        c->PurgeOps();
        Show("purged", l, m, n);
        // ops queued at the container's destruction
        SmartPtr<TestOp> o = NewOp("o", c, aDone);
        o->RunAsync(-1);
        Show("before delete", o);
        delete c;
        c = NULL;
        Show("container deleted", o);
        Log("ops released");
    }
    Log("containers done");
}

static void Timers()
{
    SmartPtr<Done> aDone = new Done;
    AsyncOpContainer* c = new AsyncOpContainer;
    {
        TimerThread* t = new TimerThread;
        SmartPtr<TestOp> a = NewOp("ta", c, aDone), b = NewOp("tb", c, aDone), d = NewOp("td", c, aDone);
        a->RunAsync(-1);
        b->RunAsync(-1);
        d->RunAsync(-1);
        c->Pump(0);
        t->AddTimerOp(a, 0);
        t->AddTimerOp(b, 3600000);
        Show("added", a, b);
        t->AddTimerOp(b, 3600000);
        Show("b added again", b);
        PlatSleep(5);
        t->Pump();
        Show("pumped", a, b);
        c->Pump(0);
        Show("delivered", a, b);
        t->RemoveTimerOp(b);
        Show("b removed", b);
        t->RemoveTimerOp(b);
        Show("b removed again", b);
        t->AddTimerOp(b, 3600000);
        t->AddTimerOp(d, 3600000);
        Log("wait time below an hour+1 -> " + std::to_string(t->GetWaitTime() <= 3600000 ? 1 : 0));
        t->PurgeOps();
        Show("purged", b, d);
        c->Pump(0);
        Show("purged delivered", b, d);
        // a timer on its own thread
        SmartPtr<TestOp> e = NewOp("te", c, aDone);
        e->RunAsTimer(0);
        Show("te as timer (no core)", e);
        SmartPtr<TestOp> f = NewOp("tf", c, aDone);
        f->RunAsync(-1);
        c->Pump(0);
        t->Start();
        t->AddTimerOp(f, 1);
        uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
        while (f->GetStatus() == WS_Pending && PlatTimeMilliseconds64() < anEnd)
            PlatSleep(1);
        Show("tf timed out on the thread", f);
        t->Stop(true);
        t->AddTimerOp(f, 3600000);
        Show("tf added after stop", f);
        delete t;
        Show("timer thread deleted", f);
        c->Pump(0);
        Show("tf delivered", f);
        Show("te delivered", e);
    }
    delete c;
    Log("timers done");
}

class TestThread : public Thread {
public:
    TestThread(const char* inName) : mName(inName), mInThis(false) {}
    virtual ~TestThread() { Log(this->mName + " thread destroyed"); }
    virtual void ThreadFunc()
    {
        this->mInThis = this->InThisThread();
        while (!this->IsStoppedForTest())
            this->WaitForSignalForTest(1000);
    }
    bool IsStoppedForTest();
    bool WaitForSignalForTest(unsigned long inMs);
    std::string mName;
    volatile bool mInThis;
};

static void Threads()
{
    TestThread* t = new TestThread("t1");
    Log("before start in this thread -> " + std::to_string(t->InThisThread() ? 1 : 0));
    t->Start();
    t->Start();
    uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
    while (!t->mInThis && PlatTimeMilliseconds64() < anEnd)
        PlatSleep(1);
    Log("ThreadFunc saw InThisThread -> " + std::to_string(t->mInThis ? 1 : 0));
    Log("main in this thread -> " + std::to_string(t->InThisThread() ? 1 : 0));
    t->Stop(true);
    Log("stopped");
    t->Stop(true);
    t->WaitForStop();
    delete t;
    TestThread* u = new TestThread("t2");
    u->SetPriority(ThreadPriority_Low);
    u->SetAutoDelete(true);
    u->Start();
    PlatSleep(20);
    u->Stop(false);
    anEnd = PlatTimeMilliseconds64() + 5000;
    while (!Logged("t2 thread destroyed") && PlatTimeMilliseconds64() < anEnd)
        PlatSleep(1);
    Log("threads done");
}

// ---- round 13: the op's remaining transitions and timings, and AsyncOpTracker / AsyncOpWithTracker (recorded over
// the C++ AsyncOp and AsyncOpTracker before their move to Rust, ee_won opobj.rs)

class CountedDone : public CompletionBase<AsyncOpPtr> {
public:
    CountedDone(const char* inName) : mName(inName) {}
    virtual ~CountedDone() { Log(this->mName + " completion destroyed"); }
    virtual void Complete(AsyncOpPtr inOp)
    {
        Log(this->mName + " got " + static_cast<TestOp*>(inOp.get())->mName + " refs " +
            std::to_string(this->GetRefCount()));
    }
    std::string mName;
};

// a blocking op whose run hook samples its time left (an hour's timeout: within a second of it; a 1 ms timeout
// after 40 ms: none; more than two of the system clock's 15.6 ms ticks)
class TimedOp : public TestOp {
public:
    TimedOp(const char* inName, unsigned long inSleep) : TestOp(inName), mSleep(inSleep) {}
    virtual void RunHook()
    {
        if (this->mSleep) PlatSleep(this->mSleep);
        unsigned long aLeft = this->TimeLeft();
        DWORD aReal = this->RealTimeLeft();
        Log(this->mName + " left " + (aLeft == 0 ? "0" : aLeft + 1000 > this->GetTimeout() ? "near" : "far") +
            " real " + (aReal == 0 ? "0" : aReal + 1000 > this->GetTimeout() ? "near" : "far") + " mode " +
            std::to_string(this->GetMode()));
        TestOp::RunHook();
    }
    unsigned long mSleep;
};

static void OpCases()
{
    AsyncOpContainer* c = new AsyncOpContainer;
    {
        SmartPtr<CountedDone> x = new CountedDone("x"), y = new CountedDone("y");
        SmartPtr<TestOp> a = NewOp("oa", c, x);
        Log("x refs " + std::to_string(x->GetRefCount()) + " container set " +
            std::to_string(a->GetContainer() == c ? 1 : 0));
        a->SetCompletion(y);
        Log("replaced: x refs " + std::to_string(x->GetRefCount()) + " y refs " + std::to_string(y->GetRefCount()));
        a->SetCompletion(NULL);
        Log("cleared: y refs " + std::to_string(y->GetRefCount()) + " completion " +
            std::to_string(a->GetCompletion() == NULL ? 0 : 1));
        a->SetCompletion(x);
        // timeout and mode: set while idle, kept while pending
        a->SetTimeout(1234);
        a->SetMode(OP_MODE_BLOCK);
        Log("idle set: timeout " + std::to_string(a->GetTimeout()) + " mode " + std::to_string(a->GetMode()) +
            " async " + std::to_string(a->IsAsync() ? 1 : 0) + " immediate " +
            std::to_string(a->IsRunAsyncImmediately() ? 1 : 0));
        a->RunAsync(-1);
        a->SetTimeout(99);
        a->SetMode(OP_MODE_BLOCK);
        Log("pending set: timeout " + std::to_string(a->GetTimeout()) + " mode " + std::to_string(a->GetMode()) +
            " async " + std::to_string(a->IsAsync() ? 1 : 0));
        // finish twice: the second is ignored; force-finish while the completion is pending: ignored
        a->Finish(WS_Success);
        a->Finish(WS_TimedOut);
        a->ForceFinish(WS_Killed);
        Show("finished twice, forced", a);
        Log("succeeded " + std::to_string(a->Succeeded() ? 1 : 0));
        c->Pump(0);
        Show("delivered", a);
        // force-finish a finished op: finished again, with the new status
        a->ForceFinish(WS_TimedOut);
        Show("forced after delivery", a);
        c->Pump(0);
        Show("forced delivered", a);
        Log("succeeded " + std::to_string(a->Succeeded() ? 1 : 0));
        // a direct Complete with nothing pending does nothing
        a->Complete();
        Show("complete with nothing pending", a);
        // kill an idle op, then run it: the run clears the flag
        a->Kill(true);
        Show("idle killed", a);
        a->RunAsync(-1);
        Show("run after kill", a);
        a->Kill(true);
        c->Pump(0);
        Show("killed delivered", a);
        Log("succeeded " + std::to_string(a->Succeeded() ? 1 : 0));
        // a blocking run left pending: the finish queues nothing, a Complete delivers
        SmartPtr<TestOp> b = NewOp("ob", c, x);
        Log("ob block -> " + std::to_string(b->Run(OP_MODE_BLOCK, -1)));
        Show("ob pending", b);
        b->Finish(WS_Success);
        Show("ob finished", b);
        c->Pump(0);
        Show("ob after pump", b);
        b->Complete();
        Show("ob completed", b);
        // blocking runs' time left
        SmartPtr<TimedOp> t = new TimedOp("ot", 0);
        t->SetContainer(c);
        t->SetCompletion(x);
        t->mFinishInRun = true;
        Log("ot block hour -> " + std::to_string(t->Run(OP_MODE_BLOCK, 3600000)));
        t->mSleep = 40;
        Log("ot block 1ms -> " + std::to_string(t->Run(OP_MODE_BLOCK, 1)));
        Log("ot after: left " + std::to_string(t->TimeLeft()) + " start set " +
            std::to_string(t->GetStartTime() != 0 ? 1 : 0));
        t->mSleep = 0;
        t->SetMode(OP_MODE_ASYNC);
        t->SetRunAsyncImmediately(true);
        Log("ot async hour -> " + std::to_string(t->Run(OP_MODE_ASYNC, 3600000)));
        Show("ot async", t);
        c->Pump(0);
        // an op queued in its container by hand
        SmartPtr<TestOp> d = NewOp("od", c, x);
        d->QueueInOpContainer();
        c->Pump(0);
        Show("od queued idle and pumped", d);
        Log("x refs at end " + std::to_string(x->GetRefCount()));
    }
    delete c;
    Log("op cases done");
}

class TrackingOp : public AsyncOpWithTracker {
public:
    TrackingOp(const char* inName, bool inFinishOnLast) : mName(inName), mFinishOnLast(inFinishOnLast) {}
    virtual ~TrackingOp() { Log(this->mName + " destroyed"); }
    virtual void RunHook() { Log(this->mName + " RunHook tracked " + std::to_string(this->mTracker.GetNumTrack())); }
    virtual void CleanupHook()
    {
        Log(this->mName + " CleanupHook tracked " + std::to_string(this->mTracker.GetNumTrack()));
        AsyncOpWithTracker::CleanupHook();
        Log(this->mName + " after cleanup tracked " + std::to_string(this->mTracker.GetNumTrack()));
    }
    virtual bool CallbackHook(AsyncOp* inOp, int inParam)
    {
        Log(this->mName + " CallbackHook " + static_cast<TestOp*>(inOp)->mName + " param " +
            std::to_string(inParam) + " status " + std::to_string(inOp->GetStatus()) + " tracked " +
            std::to_string(this->mTracker.GetNumTrack()));
        if (this->mFinishOnLast && this->mTracker.IsEmpty())
            this->Finish(WS_Success);
        return true;
    }
    std::string mName;
    bool mFinishOnLast;
};

class ParentDone : public CompletionBase<AsyncOpPtr> {
public:
    virtual void Complete(AsyncOpPtr inOp)
    {
        TrackingOp* anOp = static_cast<TrackingOp*>(inOp.get());
        Log(anOp->mName + " completed status " + std::to_string(anOp->GetStatus()) + " tracked " +
            std::to_string(anOp->mTracker.GetNumTrack()));
    }
};

static std::string Tracked(AsyncOpTracker& inTracker)
{
    std::string s = "tracked " + std::to_string(inTracker.GetNumTrack()) + " empty " +
                    std::to_string(inTracker.IsEmpty() ? 1 : 0) + ":";
    for (int i = 0; i < inTracker.GetNumTrack(); i++)
        s += " " + static_cast<TestOp*>(inTracker.GetTrackedOp(i))->mName;
    return s;
}

static void Trackers()
{
    SmartPtr<Done> aDone = new Done;
    SmartPtr<ParentDone> aParentDone = new ParentDone;
    AsyncOpContainer* c = new AsyncOpContainer;
    {
        AsyncOpTracker* t = new AsyncOpTracker;
        SmartPtr<TestOp> a = NewOp("ka", c, aDone), b = NewOp("kb", c, aDone), d = NewOp("kd", c, aDone),
                         e = NewOp("ke", c, aDone), f = NewOp("kf", c, aDone);
        Log(Tracked(*t));
        Log("add ka 1 -> " + std::to_string(t->AddOp(a, 1) == a.get() ? 1 : 0));
        t->AddOp(b, 1);
        t->AddOp(d, 0);
        t->AddOp(e, 7);
        t->AddOp(f, 1);
        Log("add ka 2 again -> " + std::to_string(t->AddOp(a, 2) == a.get() ? 1 : 0));
        Log(Tracked(*t));
        bool aFirst = t->RemoveOp(b);
        bool aSecond = t->RemoveOp(b);
        Log("remove kb -> " + std::to_string(aFirst ? 1 : 0) + " again -> " + std::to_string(aSecond ? 1 : 0));
        Show("kb untracked", b);
        Log(Tracked(*t));
        a->RunAsync(-1);
        d->RunAsync(-1);
        e->Run(OP_MODE_BLOCK, -1);
        c->Pump(0);
        t->KillAll();
        Log(Tracked(*t));
        Show("killed all", a, d, e);
        c->Pump(0);
        Show("kills delivered", a, d, f);
        t->AddOp(a, 3);
        t->AddOp(d, 3);
        Log(Tracked(*t));
        delete t;
        Show("tracker deleted", a, d);
    }
    {
        // a parent tracking three sub-ops: each sub-op's completion goes to CallbackHook while the parent runs
        SmartPtr<TrackingOp> p = new TrackingOp("p", true);
        p->SetContainer(c);
        p->SetCompletion(aParentDone);
        SmartPtr<TestOp> s1 = NewOp("s1", c, aDone), s2 = NewOp("s2", c, aDone), s3 = NewOp("s3", c, aDone);
        p->RunAsync(-1);
        c->Pump(0);
        Log("track s1 -> " + std::to_string(p->Track(s1, 11) == s1.get() ? 1 : 0));
        p->Track(s2, 12);
        p->Track(s3, 13);
        Log("p " + Tracked(p->mTracker));
        s1->mFinishInRun = s2->mFinishInRun = true;
        s1->RunAsync(-1);
        s2->RunAsync(-1);
        s3->RunAsync(-1);
        c->Pump(0);
        Log("p after s1 s2 " + Tracked(p->mTracker));
        s3->Finish(WS_TimedOut);
        c->Pump(0);
        Show("p after s3", s1, s2, s3);
        c->Pump(0);
        Log("p status " + std::to_string(p->GetStatus()));
        // a parent that finishes with sub-ops left: its cleanup kills them, their completions come after it
        SmartPtr<TrackingOp> q = new TrackingOp("q", false);
        q->SetContainer(c);
        q->SetCompletion(aParentDone);
        SmartPtr<TestOp> s4 = NewOp("s4", c, aDone), s5 = NewOp("s5", c, aDone);
        q->RunAsync(-1);
        c->Pump(0);
        q->Track(s4, 1);
        q->Track(s5, 2);
        s4->RunAsync(-1);
        s5->RunAsync(-1);
        c->Pump(0);
        q->Finish(WS_Success);
        c->Pump(0);
        Show("q finished", s4, s5);
        c->Pump(0);
        Show("q sub-ops delivered", s4, s5);
        // a sub-op that completes after its parent finished: CallbackHook is not called
        SmartPtr<TrackingOp> r = new TrackingOp("r", false);
        r->SetContainer(c);
        SmartPtr<TestOp> s6 = NewOp("s6", c, aDone);
        r->Track(s6, 4);
        s6->mFinishInRun = true;
        s6->RunAsync(-1);
        c->Pump(0);
        Log("r " + Tracked(r->mTracker));
        Log("sub-ops released");
    }
    delete c;
    Log("trackers done");
}

// ---- round 13: SocketThreadSimple (recorded over the C++ class before its move to Rust, ee_won sockthread.rs). Its
// op set is in address order, so the ops continued by one pump are logged sorted by name.

static std::vector<std::string> sPumped;

// a socket op whose Continue returns the next scripted status (WS_TimedOut: still waiting) and, when asked, finishes
// itself inside Continue
class ScriptOp : public SocketOp {
public:
    ScriptOp(const char* inName) : SocketOp(NULL), mName(inName), mNext(0), mFinishInContinue(false) {}
    virtual ~ScriptOp() { Log(this->mName + " destroyed"); }
    virtual WONStatus Continue()
    {
        WONStatus aStatus = this->mNext < this->mScript.size() ? this->mScript[this->mNext++] : WS_Success;
        sPumped.push_back(this->mName + " Continue -> " + std::to_string(aStatus) + " in thread " +
                          std::to_string(this->mInSocketThread ? 1 : 0));
        if (this->mFinishInContinue)
            this->Finish(WS_Killed);
        return aStatus;
    }
    virtual void CleanupHook() { Log(this->mName + " CleanupHook in thread " + std::to_string(this->mInSocketThread ? 1 : 0)); }
    std::string mName;
    std::vector<WONStatus> mScript;
    size_t mNext;
    bool mFinishInContinue;
};

static void FlushPumped(const char* inStep)
{
    std::sort(sPumped.begin(), sPumped.end());
    for (size_t i = 0; i < sPumped.size(); i++)
        Log(std::string(inStep) + ": " + sPumped[i]);
    sPumped.clear();
}

static std::string SockState(ScriptOp* inOp)
{
    return inOp->mName + " status " + std::to_string(inOp->GetStatus()) + " refs " +
           std::to_string(inOp->GetRefCount() - 1) + " in thread " + std::to_string(inOp->mInSocketThread ? 1 : 0) +
           " pending " + std::to_string(inOp->IsCompletionPending() ? 1 : 0) + " killed " +
           std::to_string(inOp->IsKilled() ? 1 : 0);
}

static void SockShow(const char* inStep, ScriptOp* a, ScriptOp* b = NULL, ScriptOp* c = NULL)
{
    std::string s = std::string(inStep) + ": " + SockState(a);
    if (b) s += " | " + SockState(b);
    if (c) s += " | " + SockState(c);
    Log(s);
}

static SmartPtr<ScriptOp> NewScriptOp(const char* inName, AsyncOpContainer* inContainer, bool inRun)
{
    SmartPtr<ScriptOp> anOp = new ScriptOp(inName);
    anOp->SetContainer(inContainer);
    if (inRun)
        anOp->RunAsync(-1);   // pending, queued in the container (never pumped before the thread's steps)
    return anOp;
}

static void SocketThreads()
{
    AsyncOpContainer* c = new AsyncOpContainer;
    {
        SocketThreadSimple* t = new SocketThreadSimple;
        Log("socket thread need thread " + std::to_string(t->NeedThread() ? 1 : 0));
        SmartPtr<ScriptOp> a = NewScriptOp("sa", c, true), b = NewScriptOp("sb", c, true),
                           d = NewScriptOp("sd", c, false);
        a->mScript.push_back(WS_TimedOut);
        a->mScript.push_back(WS_Success);
        b->mScript.push_back(WS_TimedOut);
        b->mScript.push_back(WS_TimedOut);
        b->mScript.push_back(WS_TimedOut);
        t->AddSocketOp(a);
        t->AddSocketOp(a);
        t->AddSocketOp(b);
        t->AddSocketOp(d);
        SockShow("added", a, b, d);
        t->Pump(0);
        FlushPumped("pump 1");
        SockShow("pumped once", a, b, d);
        t->Pump(0);
        FlushPumped("pump 2");
        SockShow("pumped twice", a, b, d);
        t->RemoveSocketOp(b);
        SockShow("sb removed", b);
        t->RemoveSocketOp(b);
        SockShow("sb removed again", b);
        t->Pump(0);
        FlushPumped("pump 3");
        c->Pump(0);
        SockShow("container pumped", a, b, d);
        // an op that finishes itself inside Continue, with a status that would keep it waiting
        SmartPtr<ScriptOp> e = NewScriptOp("se", c, true);
        e->mScript.push_back(WS_TimedOut);
        e->mFinishInContinue = true;
        t->AddSocketOp(e);
        t->Pump(0);
        FlushPumped("pump 4");
        SockShow("se finished in Continue", e);
        // purge: every op held killed and dropped
        SmartPtr<ScriptOp> f = NewScriptOp("sf", c, true), g = NewScriptOp("sg", c, true);
        f->mScript.push_back(WS_TimedOut);
        t->AddSocketOp(f);
        t->AddSocketOp(g);
        t->Pump(0);
        FlushPumped("pump 5");
        t->PurgeOps();
        SockShow("purged", f, g);
        t->Pump(0);
        FlushPumped("pump after purge");
        c->Pump(0);
        SockShow("purged delivered", f, g);
        // on its own thread: an op finishing there
        SmartPtr<ScriptOp> h = NewScriptOp("sh", c, true);
        h->mScript.push_back(WS_TimedOut);
        h->mScript.push_back(WS_Success);
        t->Start();
        t->AddSocketOp(h);
        uint64_t anEnd = PlatTimeMilliseconds64() + 5000;
        while (h->GetStatus() == WS_Pending && PlatTimeMilliseconds64() < anEnd)
            PlatSleep(1);
        t->Stop(true);
        sPumped.clear();
        SockShow("sh finished on the thread", h);
        // ops held at the thread's deletion
        SmartPtr<ScriptOp> k = NewScriptOp("sk", c, true);
        k->mScript.push_back(WS_TimedOut);
        t->AddSocketOp(k);
        delete t;
        SockShow("socket thread deleted", k);
        c->Pump(0);
        SockShow("sk delivered", k);
    }
    delete c;
    Log("socket threads done");
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_asyncop.txt";
}

int main(int argc, char** argv)
{
    Containers();
    Timers();
    Threads();
    OpCases();
    Trackers();
    SocketThreads();
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aLine[4096];
    while (fgets(aLine, sizeof(aLine), aFile)) {
        size_t n = strlen(aLine);
        while (n && (aLine[n - 1] == '\n' || aLine[n - 1] == '\r'))
            aLine[--n] = 0;
        aWant.push_back(aLine);
    }
    fclose(aFile);
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
    printf("won_asyncop_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}

// the stop flag and the signal wait, as a subclass's ThreadFunc reads them (the original's protected mStopped and
// mSignalEvent, read through the shim since the move to Rust)
bool TestThread::IsStoppedForTest() { return this->IsStopped(); }
bool TestThread::WaitForSignalForTest(unsigned long inMs) { return this->WaitForSignal(inMs); }
