// Golden driver for the Thread System (TSCriticalSection, TSEvent, TSReadWriteLock, TSThread, TSScheduler,
// TSInitialize): random single-threaded call sequences over the original units (ts_orig.h) on a scripted platform
// layer. Every platform call is printed with its arguments and the result the driver chose; the Rust replay
// (rust/crates/ee_base/src/ts/golden.rs) makes the same calls on the Rust objects, checks each platform call it makes
// against the next recorded one and answers with the recorded result, and regenerates every other line.
//
//   python -I build/lle3/cc.py build/lle3/tsg.exe tests/thread/ts_golden.cpp -Itests/thread
//   build/lle3/tsg.exe <sequences> [first seed] > golden.txt
//
// Input lines (the replay acts on these; i is an object slot, h a platform handle):
//   seq <n> | end                      a sequence (end: every object left is destroyed, threads through DestroyThread)
//   cur <thread id> | err <code>       the calling thread's id, the platform's last error
//   cs.new|cs.del|cs.enter|cs.leave|cs.try <i>
//   ev.new <i> <manual> | ev.del|ev.set|ev.clear|ev.manual|ev.handle <i> | ev.wait <i> <timeout>
//   ev.any <i> <timeout> <n> <slots...>
//   rw.new|rw.del|rw.rl|rw.ru|rw.wl|rw.wu|rw.count <i>
//   th.new <i> <stack> <suspended> | th.del <i> | th.del0 <i> | th.post <i> <msg> <p1> <p2> <queue> | th.recv <i>
//   th.resume|th.suspend|th.term|th.prio|th.exit|th.lock|th.unlock|th.run|th.issusp|th.isterm <i>
//   th.setprio <i> <p> | th.name <i> <text>
//   sch.cur | sch.idx | init | shutdown | setpump <0/1> <thread id> | dispatch
// Output lines: "plat <call> <args> [-> <result>]", "native <code>", "verify <line> <expression>", "pump -> <0/1>",
// "fn <param>" (a thread function), "= ..." results, "CS|EV|RW|TH <i> ..." an object's state, "S <handles>" the
// scheduler's threads.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <deque>
#include <vector>
#include <algorithm>
#include <string>

#define LLE_API
enum {
    kErrorOK = 0, keseOK = 0, kErrorNativeError = 1, kErrorNotSuspended = 2, kErrorNotFound = 3,
    kErrorOutOfMemory = 8, kErrorFailed = 9, kErrorTimeout = 0x11
};
#define LOWLEVELENGINE_EXPORTS
struct UString {
    std::string s;
    UString() {}
    UString(const char* inText) : s(inText) {}
    operator const char*() const { return s.c_str(); }
};

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }
static unsigned long long H(const void* p) { return (unsigned long long)(uintptr_t)p; }

// ---------------------------------------------------------------------------------------------- the platform
struct PlatMutex;
struct PlatEvent;
struct PlatThread;
#define PLAT_WAIT_INFINITE 0xFFFFFFFFu
#define PLAT_WAIT_TIMEOUT (-1)
#define PLAT_WAIT_FAILED (-2)
#define PLAT_WAIT_MAX 64
#define PLAT_THREAD_STILL_ACTIVE 259u
#define PLAT_PRIORITY_ERROR 0x7FFFFFFF
static uint32_t sCurrent = 1, sLastError, sNextHandle = 0x100, sNextId = 500;
static int sCreateFailures;  // thread creations left to fail
static uint32_t (*sThreadMain)(void*);
static void* sThreadParam;
static void* NewHandle() { return (void*)(uintptr_t)(sNextHandle += 0x10); }
static uint32_t PlatGetLastError() { return sLastError; }
static uint32_t PlatCurrentThreadId() { return sCurrent; }
static PlatMutex* PlatMutexCreate()
{
    PlatMutex* h = Below(40) == 0 ? NULL : (PlatMutex*)NewHandle();
    printf("plat mcreate -> %llu\n", H(h));
    return h;
}
static void PlatMutexDestroy(PlatMutex* h) { printf("plat mdestroy %llu\n", H(h)); }
static void PlatMutexLock(PlatMutex* h) { printf("plat mlock %llu\n", H(h)); }
static void PlatMutexUnlock(PlatMutex* h) { printf("plat munlock %llu\n", H(h)); }
static int PlatMutexTryLock(PlatMutex* h)
{
    int r = Below(3) != 0;
    printf("plat mtry %llu -> %d\n", H(h), r);
    return r;
}
static PlatEvent* PlatEventCreate(int inManual, int inSet)
{
    PlatEvent* h = Below(25) == 0 ? NULL : (PlatEvent*)NewHandle();
    printf("plat ecreate %d %d -> %llu\n", inManual, inSet, H(h));
    return h;
}
static void PlatEventDestroy(PlatEvent* h) { printf("plat edestroy %llu\n", H(h)); }
static int PlatEventSet(PlatEvent* h)
{
    int r = Below(10) != 0;
    printf("plat eset %llu -> %d\n", H(h), r);
    return r;
}
static int PlatEventReset(PlatEvent* h)
{
    int r = Below(10) != 0;
    printf("plat ereset %llu -> %d\n", H(h), r);
    return r;
}
static int32_t PlatEventWait(PlatEvent* h, uint32_t t)
{
    int32_t r = Below(6) == 0 ? (Below(2) ? PLAT_WAIT_TIMEOUT : PLAT_WAIT_FAILED) : 0;
    printf("plat ewait %llu %u -> %d\n", H(h), t, r);
    return r;
}
static int32_t PlatEventWaitAny(PlatEvent* const* h, uint32_t n, uint32_t t)
{
    printf("plat eany");
    for (uint32_t i = 0; i < n; i++)
        printf(" %llu", H(h[i]));
    int32_t r = Below(5) == 0 ? (Below(2) ? PLAT_WAIT_TIMEOUT : PLAT_WAIT_FAILED) : n ? (int32_t)Below(n) : PLAT_WAIT_FAILED;
    printf(" %u -> %d\n", t, r);
    return r;
}
static PlatThread* PlatThreadCreate(uint32_t (*inMain)(void*), void* inParam, uint32_t inStack, int inSuspended,
                                    uint32_t* outId)
{
    PlatThread* h = NULL;
    if (sCreateFailures > 0)
        sCreateFailures--;
    else {
        h = (PlatThread*)NewHandle();
        *outId = sNextId++;
        sThreadMain = inMain;
        sThreadParam = inParam;
    }
    printf("plat tcreate %u %d -> %llu %u\n", inStack, inSuspended, H(h), h ? *outId : 0);
    return h;
}
static PlatThread* PlatThreadOpenCurrent()
{
    PlatThread* h = Below(8) == 0 ? NULL : (PlatThread*)NewHandle();
    printf("plat topen -> %llu\n", H(h));
    return h;
}
static void PlatThreadClose(PlatThread* h) { printf("plat tclose %llu\n", H(h)); }
static int PlatThreadGetExitCode(PlatThread* h, uint32_t* outCode)
{
    int r = Below(6) != 0;
    if (r)
        *outCode = Below(2) ? PLAT_THREAD_STILL_ACTIVE : Below(5);
    printf("plat texit %llu -> %d %u\n", H(h), r, r ? *outCode : 0);
    return r;
}
static int32_t PlatThreadGetPriority(PlatThread* h)
{
    int32_t r = Below(5) == 0 ? PLAT_PRIORITY_ERROR : (int32_t)Below(5) - 2;
    printf("plat tprio %llu -> %d\n", H(h), r);
    return r;
}
static int PlatThreadSetPriority(PlatThread* h, int32_t p)
{
    int r = Below(5) != 0;
    printf("plat tsetprio %llu %d -> %d\n", H(h), p, r);
    return r;
}
static uint32_t PlatThreadSuspend(PlatThread* h)
{
    uint32_t r = Below(5) == 0 ? 0xFFFFFFFFu : Below(3);
    printf("plat tsuspend %llu -> %u\n", H(h), r);
    return r;
}
static uint32_t PlatThreadResume(PlatThread* h)
{
    uint32_t r = Below(5) == 0 ? 0xFFFFFFFFu : Below(4);
    printf("plat tresume %llu -> %u\n", H(h), r);
    return r;
}
static int PlatThreadTerminate(PlatThread* h)
{
    int r = Below(4) != 0;
    printf("plat tterm %llu -> %d\n", H(h), r);
    return r;
}
static void PlatSleep(uint32_t ms) { printf("plat sleep %u\n", ms); }
static int32_t PlatAtomicIncrement(volatile int32_t* v) { return ++*v; }
static int32_t PlatAtomicDecrement(volatile int32_t* v) { return --*v; }

// ---------------------------------------------------------------------------------------------- the engine
static unsigned long ESRecordNativeError(unsigned long e)
{
    printf("native %lu\n", e);
    return e;
}
static unsigned long ESGetNativeError() { return 0; }
static void Verify(unsigned long inLine, const char* inExpression) { printf("verify %lu %s\n", inLine, inExpression); }
#define ESVERIFY(expr) if (!(expr)) Verify(0, #expr)
#define ESVERIFY_AT(expr, line) if (!(expr)) Verify(line, #expr)
static long sShutdownResult;
static long URegisterShutdown(void (*)(bool))
{
    printf("plat shutdownreg -> %ld\n", sShutdownResult);
    return sShutdownResult;
}

#define private public
#define protected public
#include "ts_orig.h"
#undef private
#undef protected

static unsigned long ThreadFunction(void* inParam)
{
    printf("fn %llu\n", H(inParam));
    return (unsigned long)(uintptr_t)inParam * 3 + 1;
}
static bool Pump()
{
    bool r = Below(3) == 0;
    printf("pump -> %d\n", r);
    return r;
}

static const int kSlots = 4;
static TSCriticalSection* sSections[kSlots];
static TSEvent* sEvents[kSlots];
static TSReadWriteLock* sLocks[kSlots];
static TSThread* sThreads[kSlots];

static void DumpSection(int i)
{
    TSCriticalSection* s = sSections[i];
    if (s)
        printf("CS %d %llu %lu %lu\n", i, H(s->mSection), s->mOwnerThread, s->mLockCount);
    else
        printf("CS %d -\n", i);
}
static void DumpEvent(int i)
{
    TSEvent* e = sEvents[i];
    if (e)
        printf("EV %d %llu %d\n", i, H(e->mHandle), e->mManualReset);
    else
        printf("EV %d -\n", i);
}
static void DumpLock(int i)
{
    TSReadWriteLock* l = sLocks[i];
    if (l)
        printf("RW %d %llu %lu %lu %llu %llu %d\n", i, H(l->mWriteLock->mSection), l->mWriteLock->mOwnerThread,
               l->mWriteLock->mLockCount, H(l->mNoReaders->mHandle), H(l->mReading->mHandle), (int)l->mReaderCount);
    else
        printf("RW %d -\n", i);
}
static void DumpThread(int i)
{
    TSThread* t = sThreads[i];
    if (!t) {
        printf("TH %d -\n", i);
        return;
    }
    printf("TH %d %llu %lu %d %llu %llu %lu %lu [", i, H(t->mHandle), t->mThreadID, t->mSuspended, H(t->mPostLock),
           H(t->mMessageLock->mSection), t->mMessageLock->mOwnerThread, t->mMessageLock->mLockCount);
    for (size_t k = 0; k < t->mMessages.size(); k++)
        printf("%s%lu", k ? " " : "", t->mMessages[k]);
    printf("] [");
    for (size_t k = 0; k < t->mPosted.size(); k++)
        printf("%s%lu/%llu/%llu", k ? " " : "", t->mPosted[k].mMessage, H(t->mPosted[k].mParameter1),
               H(t->mPosted[k].mParameter2));
    printf("] %s\n", t->mName.s.empty() ? "-" : t->mName.s.c_str());
}
static void DumpScheduler()
{
    printf("S");
    for (size_t k = 0; k < TSScheduler::sThreads.size(); k++)
        printf(" %llu/%lu", H(TSScheduler::sThreads[k]->mHandle), TSScheduler::sThreads[k]->mThreadID);
    printf("\n");
}
static uint32_t PickThreadId()
{
    if (Below(4) == 0 || TSScheduler::sThreads.empty())
        return 1 + Below(3);
    return TSScheduler::sThreads[Below((uint32_t)TSScheduler::sThreads.size())]->mThreadID;
}

static void SectionOp(int i)
{
    static const char* kNames[] = {"new", "del", "enter", "leave", "try"};
    int theOp = sSections[i] ? 1 + Below(5) : 0;
    if (theOp == 5)
        theOp = 2;
    if (theOp == 3 && sSections[i]->mLockCount == 0 && Below(4))
        theOp = 2;   // a leave without an enter now and then only
    printf("cs.%s %d\n", kNames[theOp], i);
    switch (theOp) {
    case 0: printf("= %ld\n", TSScheduler::CreateCriticalSection(sSections[i])); break;
    case 1: TSScheduler::DestroyCriticalSection(sSections[i]); sSections[i] = NULL; break;
    case 2: sSections[i]->Enter(); break;
    case 3: sSections[i]->Leave(); break;
    case 4: printf("= %d\n", sSections[i]->TryEnter()); break;
    }
    DumpSection(i);
}

static void EventOp(int i)
{
    if (!sEvents[i] || Below(12) == 0) {
        bool theManual = Below(2);
        printf("ev.new %d %d\n", i, theManual);
        TSScheduler::DestroyEvent(sEvents[i]);
        sEvents[i] = NULL;
        printf("= %ld\n", TSScheduler::CreateEvent(sEvents[i], theManual));
        DumpEvent(i);
        return;
    }
    switch (Below(8)) {
    case 0: printf("ev.del %d\n", i); TSScheduler::DestroyEvent(sEvents[i]); sEvents[i] = NULL; break;
    case 1: printf("ev.set %d\n", i); printf("= %ld\n", sEvents[i]->Set()); break;
    case 2: printf("ev.clear %d\n", i); printf("= %ld\n", sEvents[i]->Clear()); break;
    case 3: {
        unsigned long t = Below(2) ? kTSWaitInfinite : Below(1000);
        printf("ev.wait %d %lu\n", i, t); printf("= %ld\n", sEvents[i]->Wait(t));
        break;
    }
    case 4: printf("ev.manual %d\n", i); printf("= %d\n", sEvents[i]->GetIsManuallyReset()); break;
    case 5: printf("ev.handle %d\n", i); printf("= %llu\n", H((void*)*sEvents[i])); break;
    default: {
        TSEvent* theEvents[kSlots];
        int theSlots[kSlots];
        unsigned long n = 0;
        for (int k = 0; k < kSlots; k++)
            if (sEvents[k] && Below(2)) {
                theSlots[n] = k;
                theEvents[n++] = sEvents[k];
            }
        unsigned long t = Below(2) ? kTSWaitInfinite : Below(1000);
        printf("ev.any %d %lu %lu", i, t, n);
        for (unsigned long k = 0; k < n; k++)
            printf(" %d", theSlots[k]);
        printf("\n"); printf("= %ld\n", TSEvent::WaitAny(theEvents, n, t));
        break;
    }
    }
    DumpEvent(i);
}

static void LockOp(int i)
{
    static const char* kNames[] = {"new", "del", "rl", "ru", "wl", "wu", "count"};
    int theOp = sLocks[i] ? 1 + Below(6) : 0;
    if (theOp == 1 && Below(2))
        theOp = 2;
    if (theOp == 3 && sLocks[i]->mReaderCount == 0 && Below(4))
        theOp = 2;
    if (theOp == 5 && sLocks[i]->mWriteLock->mLockCount == 0)
        theOp = 4;
    if (Below(8) == 0)
        theOp = sLocks[i] ? 6 : 0;
    printf("rw.%s %d\n", kNames[theOp], i);
    switch (theOp) {
    case 0: {
        TSReadWriteLock* theLock = NULL;
        long r = TSScheduler::CreateReadWriteLock(theLock);
        printf("= %ld\n", r);
        if (r == kErrorOK)
            sLocks[i] = theLock;
        break;
    }
    case 1: TSScheduler::DestroyReadWriteLock(sLocks[i]); sLocks[i] = NULL; break;
    case 2: sLocks[i]->ReadLock(); break;
    case 3: sLocks[i]->ReadUnlock(); break;
    case 4: sLocks[i]->WriteLock(); break;
    case 5: sLocks[i]->WriteUnlock(); break;
    case 6: printf("= %ld\n", sLocks[i]->GetReadLockCount()); break;
    }
    DumpLock(i);
}

static void ThreadOp(int i)
{
    if (!sThreads[i]) {
        unsigned long theStack = Below(2) ? 0 : 4096 * Below(64);
        bool theSuspended = Below(2);
        sCreateFailures = Below(3) == 0 ? 1 + Below(5) : 0;
        printf("th.new %d %lu %d\n", i, theStack, theSuspended);
        long r = TSScheduler::CreateThread(sThreads[i], ThreadFunction, (void*)(uintptr_t)(16 * (i + 1)), theStack,
                                           theSuspended);
        printf("= %ld\n", r);
        DumpThread(i);
        DumpScheduler();
        return;
    }
    TSThread* t = sThreads[i];
    switch (Below(20)) {
    case 0:
        printf("th.del %d\n", i); printf("= %ld\n", TSScheduler::DestroyThread(t));
        sThreads[i] = NULL;
        DumpScheduler();
        break;
    case 1: printf("th.del0 %d\n", i); printf("= %ld\n", TSScheduler::DestroyThread(NULL)); break;
    case 2: case 3: case 4: case 5: {
        static const unsigned long kMessages[] = {0x12, 0x11, 0x3ff, 0x400, 0x401, 0x4aa, 0x7fff, 0};
        unsigned long m = Below(4) == 0 ? Next() : kMessages[Below(8)];
        unsigned long long p1 = Below(3) ? Below(1000) : 0, p2 = Below(3) ? Next() : 0;
        bool q = Below(2);
        printf("th.post %d %lu %llu %llu %d\n", i, m, p1, p2, q);
        printf("= %ld\n", t->Post(m, (void*)(uintptr_t)p1, (void*)(uintptr_t)p2, q));
        break;
    }
    case 6: case 7: case 8: {
        unsigned long m = 7;
        void* p1 = (void*)8;
        void* p2 = (void*)9;
        printf("th.recv %d\n", i);
        long r = t->Receive(m, p1, p2);
        printf("= %ld %lu %llu %llu\n", r, m, H(p1), H(p2));
        break;
    }
    case 9: printf("th.resume %d\n", i); printf("= %ld\n", t->Resume()); break;
    case 10: printf("th.suspend %d\n", i); printf("= %ld\n", t->Suspend()); break;
    case 11: printf("th.term %d\n", i); printf("= %ld\n", t->Terminate()); break;
    case 12: printf("th.prio %d\n", i); printf("= %ld\n", t->GetPriority()); break;
    case 13: {
        long p = (long)Below(7) - 3;
        printf("th.setprio %d %ld\n", i, p); printf("= %ld\n", t->SetPriority(p));
        break;
    }
    case 14: printf("th.exit %d\n", i); printf("= %lu\n", t->GetExitCode()); break;
    case 15:
        if (t->mMessageLock->mLockCount && Below(2))
            printf("th.unlock %d\n", i), t->UnlockMessageQueue();
        else
            printf("th.lock %d\n", i), t->LockMessageQueue();
        break;
    case 16:
        if (!sThreadMain)
            return;
        printf("th.run %d\n", i);
        printf("= %u\n", TSThread::ThreadMain(t));
        break;
    case 17: printf("th.issusp %d\n", i); printf("= %d\n", t->IsSuspended()); break;
    case 18: printf("th.isterm %d\n", i); printf("= %d\n", t->IsTerminated()); break;
    default: {
        static const char* kNames[] = {"Graphics", "Physics", "AI", "Music"};
        UString theName(kNames[Below(4)]);
        printf("th.name %d %s\n", i, theName.s.c_str());
        t->SetName(theName);
        break;
    }
    }
    if (sThreads[i])
        DumpThread(i);
}

static void Sequence(int n)
{
    printf("seq %d\n", n);
    sCurrent = 1 + Below(3);
    sLastError = Below(1000);
    printf("cur %u\nerr %u\n", sCurrent, sLastError);
    int theCount = 30 + Below(90);
    for (int k = 0; k < theCount; k++) {
        int theOp = Below(50);
        int i = Below(kSlots);
        if (theOp < 3) {
            sCurrent = PickThreadId();
            printf("cur %u\n", sCurrent);
        } else if (theOp < 4) {
            sLastError = Below(3) ? Below(1000) : Next();
            printf("err %u\n", sLastError);
        } else if (theOp < 12)
            SectionOp(i);
        else if (theOp < 20)
            EventOp(i);
        else if (theOp < 27)
            LockOp(i);
        else if (theOp < 44)
            ThreadOp(i);
        else {
            switch (Below(7)) {
            case 0: {
                TSThread* t = TSScheduler::GetCurrentThread();
                printf("sch.cur\n"); printf("= %llu\n", H(t ? t->mHandle : NULL));
                break;
            }
            case 1: printf("sch.idx\n"); printf("= %lu\n", TSScheduler::GetCurrentThreadIndex()); break;
            case 2:
                sShutdownResult = Below(6) == 0 ? 9 : 0;
                printf("init\n"); printf("= %ld\n", TSInitialize());
                DumpScheduler();
                break;
            case 3:
                printf("shutdown\n");
                TSShutdown(false);
                DumpScheduler();
                for (int k = 0; k < kSlots; k++) {
                    // a slot's thread the shutdown destroyed (the calling thread's) is forgotten
                    if (sThreads[k] && std::find(TSScheduler::sThreads.begin(), TSScheduler::sThreads.end(),
                                                 sThreads[k]) == TSScheduler::sThreads.end())
                        sThreads[k] = NULL;
                }
                break;
            case 4: {
                bool thePump = Below(4) != 0;
                uint32_t theThread = PickThreadId();
                printf("setpump %d %u\n", thePump, theThread);
                TSThread::SetEventPump(thePump ? Pump : NULL, theThread);
                break;
            }
            default:
                printf("dispatch\n");
                TSThread::DispatchWindowEvents();
                break;
            }
        }
    }
    printf("end\n");
    for (int k = 0; k < kSlots; k++) {
        TSScheduler::DestroyCriticalSection(sSections[k]);
        TSScheduler::DestroyEvent(sEvents[k]);
        TSScheduler::DestroyReadWriteLock(sLocks[k]);
        sSections[k] = NULL;
        sEvents[k] = NULL;
        sLocks[k] = NULL;
        sThreads[k] = NULL;
    }
    while (!TSScheduler::sThreads.empty())
        TSScheduler::DestroyThread(TSScheduler::sThreads.front());
    TSThread::SetEventPump(NULL, 0);
    sThreadMain = NULL;
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    for (int n = theFirst; n < theFirst + theCount; n++) {
        sRandom = 0x2545f491u ^ (uint32_t)n * 2654435761u;
        if (sRandom == 0)
            sRandom = 1;
        Sequence(n);
    }
    return 0;
}
