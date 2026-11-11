// WON's async machinery: AsyncOp's runs, finishes, kills and completions through an AsyncOpContainer (queue, pump,
// purge, the op being pumped queued again, the pump's op limit), the TimerThread (timeouts due and replaced, removed,
// purged; its own thread), and Thread (start, InThisThread, stop, auto-delete), one line per event with every op's
// reference count, compared with rust/crates/ee_won/src/golden_asyncop.txt. The golden was recorded over the C++
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
            + std::to_string(anOp->mKilled ? 1 : 0));
    }
};

static std::string State(TestOp* inOp)
{
    return inOp->mName + " status " + std::to_string(inOp->GetStatus()) + " refs " +
           std::to_string(inOp->GetRefCount() - 1) + " pending " + std::to_string(inOp->mCompletionPending ? 1 : 0) +
           " killed " + std::to_string(inOp->mKilled ? 1 : 0);
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
    anOp->mContainer = inContainer;
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

// the stop flag and the signal wait, as a subclass's ThreadFunc reads them (the original's protected members)
bool TestThread::IsStoppedForTest() { return this->mStopped; }
bool TestThread::WaitForSignalForTest(unsigned long inMs) { return this->mSignalEvent.WaitFor(inMs); }
