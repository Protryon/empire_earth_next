// Platform layer, POSIX: threads and synchronisation (include/platform/PlatThread.h), and the per-thread last
// error the whole POSIX implementation sets (PlatPosix.h).
//
// Events: every event shares one process-wide mutex and condition variable. A wait checks its events under the
// mutex and sleeps on the condition; PlatEventSet broadcasts it. That makes PlatEventWaitAny exact (the lowest set
// event wins, an automatic-reset event is cleared by the one wait it releases) at the cost of waking every waiter
// on every set, which is cheap for the game's dozen threads. Timed waits run on the monotonic clock.
//
// Threads: a PlatThread is a reference-counted record (the thread holds one reference, each handle one), so a handle
// stays valid after the thread ends and PlatThreadClose never touches a running thread. A thread created suspended
// waits at a start gate until PlatThreadResume; suspending a running thread is not possible with pthreads, so
// PlatThreadSuspend fails there (ERROR_NOT_SUPPORTED), and so does PlatThreadTerminate (the engine never calls
// either on a running thread). PlatThreadWait waits on a done event the thread sets as it ends. Priorities are kept
// and applied as a nice value where the system allows it.
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif

#include <new>

#include "platform/PlatThread.h"
#include "PlatPosix.h"

// ---- the last error

namespace {
thread_local uint32_t tLastError;
}

namespace PlatPosix {

void SetLastError(uint32_t inError)
{
    tLastError = inError;
}

uint32_t ErrorFromErrno(int inErrno)
{
    switch (inErrno) {
    case 0: return 0;
    case ENOENT: return kErrorFileNotFound;
    case ENOTDIR: return kErrorPathNotFound;
    case EMFILE:
    case ENFILE: return kErrorTooManyOpenFiles;
    case EACCES:
    case EPERM:
    case EISDIR: return kErrorAccessDenied;
    case EBADF: return kErrorInvalidHandle;
    case ENOMEM: return kErrorNotEnoughMemory;
    case EROFS: return kErrorWriteProtect;
    case EXDEV: return kErrorNotSameDevice;
    case EEXIST: return kErrorAlreadyExists;
    case EINVAL: return kErrorInvalidParameter;
    case ENOSPC: return kErrorDiskFull;
    case ENOTEMPTY: return kErrorDirNotEmpty;
    case EBUSY: return kErrorBusy;
    case ENAMETOOLONG: return kErrorFilenameTooLong;
    case ETIMEDOUT: return kErrorTimeout;
    case ENOSYS:
    case ENOTSUP: return kErrorNotSupported;
    default: return 0x20000000u | static_cast<uint32_t>(inErrno);
    }
}

int FailErrno()
{
    tLastError = ErrorFromErrno(errno);
    return 0;
}

int Fail(uint32_t inError)
{
    tLastError = inError;
    return 0;
}

}   // namespace PlatPosix

extern "C" uint32_t PlatGetLastError(void)
{
    return tLastError;
}

extern "C" uint32_t PlatCurrentThreadId(void)
{
#if defined(__linux__)
    return static_cast<uint32_t>(syscall(SYS_gettid));
#elif defined(__APPLE__)
    uint64_t theID = 0;
    pthread_threadid_np(NULL, &theID);
    return static_cast<uint32_t>(theID);
#else
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pthread_self()));
#endif
}

// ---- mutexes

struct PlatMutex {
    pthread_mutex_t mMutex;
};

extern "C" PlatMutex* PlatMutexCreate(void)
{
    PlatMutex* theMutex = new (std::nothrow) PlatMutex;
    if (!theMutex) {
        PlatPosix::Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    pthread_mutexattr_t theAttributes;
    pthread_mutexattr_init(&theAttributes);
    pthread_mutexattr_settype(&theAttributes, PTHREAD_MUTEX_RECURSIVE);
    int theError = pthread_mutex_init(&theMutex->mMutex, &theAttributes);
    pthread_mutexattr_destroy(&theAttributes);
    if (theError) {
        delete theMutex;
        PlatPosix::SetLastError(PlatPosix::ErrorFromErrno(theError));
        return NULL;
    }
    return theMutex;
}

extern "C" void PlatMutexDestroy(PlatMutex* inMutex)
{
    if (!inMutex)
        return;
    pthread_mutex_destroy(&inMutex->mMutex);
    delete inMutex;
}

extern "C" void PlatMutexLock(PlatMutex* inMutex)
{
    pthread_mutex_lock(&inMutex->mMutex);
}

extern "C" void PlatMutexUnlock(PlatMutex* inMutex)
{
    pthread_mutex_unlock(&inMutex->mMutex);
}

extern "C" int PlatMutexTryLock(PlatMutex* inMutex)
{
    return pthread_mutex_trylock(&inMutex->mMutex) == 0 ? 1 : 0;
}

// ---- events

struct PlatEvent {
    bool mManualReset;
    bool mSet;
};

namespace {

pthread_mutex_t sEventLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sEventCondition;
pthread_once_t sEventOnce = PTHREAD_ONCE_INIT;

void InitEventCondition()
{
    pthread_condattr_t theAttributes;
    pthread_condattr_init(&theAttributes);
#if !defined(__APPLE__)
    pthread_condattr_setclock(&theAttributes, CLOCK_MONOTONIC);
#endif
    pthread_cond_init(&sEventCondition, &theAttributes);
    pthread_condattr_destroy(&theAttributes);
}

struct timespec MonotonicNow()
{
    struct timespec theNow;
    clock_gettime(CLOCK_MONOTONIC, &theNow);
    return theNow;
}

struct timespec AddMilliseconds(struct timespec inTime, uint32_t inMs)
{
    inTime.tv_sec += inMs / 1000;
    inTime.tv_nsec += static_cast<long>(inMs % 1000) * 1000000L;
    if (inTime.tv_nsec >= 1000000000L) {
        inTime.tv_sec++;
        inTime.tv_nsec -= 1000000000L;
    }
    return inTime;
}

// Sleeps on the event condition (sEventLock held) until inDeadline -> false once the deadline has passed
bool WaitUntil(const struct timespec& inDeadline)
{
#if defined(__APPLE__)
    // macOS has no monotonic condition clock: wait relative to what is left
    struct timespec theNow = MonotonicNow();
    if (theNow.tv_sec > inDeadline.tv_sec || (theNow.tv_sec == inDeadline.tv_sec && theNow.tv_nsec >= inDeadline.tv_nsec))
        return false;
    struct timespec theLeft;
    theLeft.tv_sec = inDeadline.tv_sec - theNow.tv_sec;
    theLeft.tv_nsec = inDeadline.tv_nsec - theNow.tv_nsec;
    if (theLeft.tv_nsec < 0) {
        theLeft.tv_sec--;
        theLeft.tv_nsec += 1000000000L;
    }
    pthread_cond_timedwait_relative_np(&sEventCondition, &sEventLock, &theLeft);
    return true;
#else
    return pthread_cond_timedwait(&sEventCondition, &sEventLock, &inDeadline) != ETIMEDOUT;
#endif
}

}   // namespace

extern "C" PlatEvent* PlatEventCreate(int inManualReset, int inInitiallySet)
{
    pthread_once(&sEventOnce, InitEventCondition);
    PlatEvent* theEvent = new (std::nothrow) PlatEvent;
    if (!theEvent) {
        PlatPosix::Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    theEvent->mManualReset = inManualReset != 0;
    theEvent->mSet = inInitiallySet != 0;
    return theEvent;
}

extern "C" void PlatEventDestroy(PlatEvent* inEvent)
{
    delete inEvent;
}

extern "C" int PlatEventSet(PlatEvent* inEvent)
{
    if (!inEvent)
        return PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
    pthread_mutex_lock(&sEventLock);
    inEvent->mSet = true;
    pthread_cond_broadcast(&sEventCondition);
    pthread_mutex_unlock(&sEventLock);
    return 1;
}

extern "C" int PlatEventReset(PlatEvent* inEvent)
{
    if (!inEvent)
        return PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
    pthread_mutex_lock(&sEventLock);
    inEvent->mSet = false;
    pthread_mutex_unlock(&sEventLock);
    return 1;
}

extern "C" int32_t PlatEventWaitAny(PlatEvent* const* inEvents, uint32_t inCount, uint32_t inTimeoutMs)
{
    if (inCount == 0 || inCount > PLAT_WAIT_MAX || !inEvents) {
        PlatPosix::Fail(PlatPosix::kErrorInvalidParameter);
        return PLAT_WAIT_FAILED;
    }
    for (uint32_t i = 0; i < inCount; i++)
        if (!inEvents[i]) {
            PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
            return PLAT_WAIT_FAILED;
        }
    pthread_once(&sEventOnce, InitEventCondition);
    struct timespec theDeadline = AddMilliseconds(MonotonicNow(), inTimeoutMs == PLAT_WAIT_INFINITE ? 0 : inTimeoutMs);
    pthread_mutex_lock(&sEventLock);
    int32_t theResult = PLAT_WAIT_TIMEOUT;
    for (;;) {
        for (uint32_t i = 0; i < inCount; i++) {
            if (inEvents[i]->mSet) {
                if (!inEvents[i]->mManualReset)
                    inEvents[i]->mSet = false;
                theResult = static_cast<int32_t>(i);
                break;
            }
        }
        if (theResult != PLAT_WAIT_TIMEOUT || inTimeoutMs == 0)
            break;
        if (inTimeoutMs == PLAT_WAIT_INFINITE)
            pthread_cond_wait(&sEventCondition, &sEventLock);
        else if (!WaitUntil(theDeadline)) {
            // one last look: a set that raced with the deadline still counts
            for (uint32_t i = 0; i < inCount; i++) {
                if (inEvents[i]->mSet) {
                    if (!inEvents[i]->mManualReset)
                        inEvents[i]->mSet = false;
                    theResult = static_cast<int32_t>(i);
                    break;
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&sEventLock);
    return theResult;
}

extern "C" int32_t PlatEventWait(PlatEvent* inEvent, uint32_t inTimeoutMs)
{
    return PlatEventWaitAny(&inEvent, 1, inTimeoutMs);
}

// ---- threads

struct PlatThread {
    pthread_mutex_t mLock;
    pthread_cond_t mStartCondition;
    int mReferences;
    uint32_t mStartGate;            // suspend count before the thread's first instruction
    bool mRunning;
    uint32_t mExitCode;
    int32_t mPriority;
    uint32_t mThreadID;
    pthread_t mThread;
    bool mOwnsThread;               // created here (joinable until detached)
    PlatThreadMain mMain;
    void* mParameter;
    PlatEvent* mDone;               // manual reset, set when inMain has returned (PlatThreadWait)
};

namespace {

thread_local PlatThread* tCurrentThread;

PlatThread* NewThreadRecord()
{
    PlatThread* theThread = new (std::nothrow) PlatThread;
    if (!theThread)
        return NULL;
    pthread_mutex_init(&theThread->mLock, NULL);
    pthread_cond_init(&theThread->mStartCondition, NULL);
    theThread->mReferences = 1;
    theThread->mStartGate = 0;
    theThread->mRunning = true;
    theThread->mExitCode = PLAT_THREAD_STILL_ACTIVE;
    theThread->mPriority = PLAT_PRIORITY_NORMAL;
    theThread->mThreadID = 0;
    theThread->mOwnsThread = false;
    theThread->mMain = NULL;
    theThread->mParameter = NULL;
    theThread->mDone = PlatEventCreate(1, 0);
    if (!theThread->mDone) {
        pthread_cond_destroy(&theThread->mStartCondition);
        pthread_mutex_destroy(&theThread->mLock);
        delete theThread;
        return NULL;
    }
    return theThread;
}

void Release(PlatThread* inThread)
{
    pthread_mutex_lock(&inThread->mLock);
    int theLeft = --inThread->mReferences;
    pthread_mutex_unlock(&inThread->mLock);
    if (theLeft == 0) {
        PlatEventDestroy(inThread->mDone);
        pthread_cond_destroy(&inThread->mStartCondition);
        pthread_mutex_destroy(&inThread->mLock);
        delete inThread;
    }
}

void Retain(PlatThread* inThread)
{
    pthread_mutex_lock(&inThread->mLock);
    inThread->mReferences++;
    pthread_mutex_unlock(&inThread->mLock);
}

// Applies a PLAT_PRIORITY_* value to the calling thread, as far as the system lets an unprivileged process
void ApplyPriority(int32_t inPriority)
{
#if defined(__APPLE__)
    qos_class_t theClass = inPriority >= PLAT_PRIORITY_HIGHEST ? QOS_CLASS_USER_INTERACTIVE
        : inPriority >= PLAT_PRIORITY_NORMAL ? QOS_CLASS_USER_INITIATED
        : inPriority >= PLAT_PRIORITY_LOWEST ? QOS_CLASS_UTILITY : QOS_CLASS_BACKGROUND;
    pthread_set_qos_class_self_np(theClass, 0);
#elif defined(__linux__)
    // a per-thread nice value: lowering always works, raising above the start value needs privileges (ignored)
    int theNice = inPriority <= PLAT_PRIORITY_IDLE ? 19 : inPriority >= PLAT_PRIORITY_TIME_CRITICAL ? -10
        : -inPriority * 2;
    setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), theNice);
#else
    (void)inPriority;
#endif
}

void* ThreadEntry(void* inThread)
{
    PlatThread* theThread = static_cast<PlatThread*>(inThread);
    tCurrentThread = theThread;
    pthread_mutex_lock(&theThread->mLock);
    theThread->mThreadID = PlatCurrentThreadId();
    pthread_cond_broadcast(&theThread->mStartCondition);
    while (theThread->mStartGate > 0)
        pthread_cond_wait(&theThread->mStartCondition, &theThread->mLock);
    int32_t thePriority = theThread->mPriority;
    pthread_mutex_unlock(&theThread->mLock);
    if (thePriority != PLAT_PRIORITY_NORMAL)
        ApplyPriority(thePriority);
    uint32_t theExitCode = theThread->mMain(theThread->mParameter);
    pthread_mutex_lock(&theThread->mLock);
    theThread->mExitCode = theExitCode;
    theThread->mRunning = false;
    pthread_mutex_unlock(&theThread->mLock);
    PlatEventSet(theThread->mDone);
    tCurrentThread = NULL;
    Release(theThread);
    return NULL;
}

}   // namespace

extern "C" PlatThread* PlatThreadCreate(PlatThreadMain inMain, void* inParameter, uint32_t inStackSize,
                                        int inSuspended, uint32_t* outThreadID)
{
    PlatThread* theThread = NewThreadRecord();
    if (!theThread) {
        PlatPosix::Fail(PlatPosix::kErrorNotEnoughMemory);
        return NULL;
    }
    theThread->mReferences = 2;   // the thread's and the caller's handle
    theThread->mStartGate = inSuspended ? 1 : 0;
    theThread->mMain = inMain;
    theThread->mParameter = inParameter;
    theThread->mOwnsThread = true;
    pthread_attr_t theAttributes;
    pthread_attr_init(&theAttributes);
    pthread_attr_setdetachstate(&theAttributes, PTHREAD_CREATE_DETACHED);
    if (inStackSize) {
        // Win32 rounds a stack size up to its granularity; pthreads refuses one below PTHREAD_STACK_MIN
        size_t theSize = inStackSize < 65536 ? 65536 : inStackSize;
        theSize = (theSize + 65535) & ~static_cast<size_t>(65535);
        pthread_attr_setstacksize(&theAttributes, theSize);
    }
    pthread_mutex_lock(&theThread->mLock);
    int theError = pthread_create(&theThread->mThread, &theAttributes, ThreadEntry, theThread);
    pthread_attr_destroy(&theAttributes);
    if (theError) {
        pthread_mutex_unlock(&theThread->mLock);
        theThread->mReferences = 1;
        Release(theThread);
        PlatPosix::SetLastError(PlatPosix::ErrorFromErrno(theError));
        return NULL;
    }
    // the id is the system's thread id, known once the thread runs: wait for it (the start gate holds it after)
    while (theThread->mThreadID == 0)
        pthread_cond_wait(&theThread->mStartCondition, &theThread->mLock);
    *outThreadID = theThread->mThreadID;
    pthread_mutex_unlock(&theThread->mLock);
    return theThread;
}

extern "C" PlatThread* PlatThreadOpenCurrent(void)
{
    if (!tCurrentThread) {
        // a thread not started here (the main thread): a record that lives as long as the thread
        PlatThread* theThread = NewThreadRecord();
        if (!theThread) {
            PlatPosix::Fail(PlatPosix::kErrorNotEnoughMemory);
            return NULL;
        }
        theThread->mThread = pthread_self();
        theThread->mThreadID = PlatCurrentThreadId();
        tCurrentThread = theThread;   // the thread's own reference (never released: such threads do not end here)
    }
    Retain(tCurrentThread);
    return tCurrentThread;
}

extern "C" void PlatThreadClose(PlatThread* inThread)
{
    if (inThread)
        Release(inThread);
}

extern "C" int PlatThreadGetExitCode(PlatThread* inThread, uint32_t* outExitCode)
{
    if (!inThread)
        return PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
    pthread_mutex_lock(&inThread->mLock);
    *outExitCode = inThread->mRunning ? PLAT_THREAD_STILL_ACTIVE : inThread->mExitCode;
    pthread_mutex_unlock(&inThread->mLock);
    return 1;
}

extern "C" int32_t PlatThreadWait(PlatThread* inThread, uint32_t inTimeoutMs)
{
    if (!inThread) {
        PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
        return PLAT_WAIT_FAILED;
    }
    // a thread not started here (PlatThreadOpenCurrent) never sets it: as Win32, a thread waiting on itself waits
    return PlatEventWait(inThread->mDone, inTimeoutMs);
}

extern "C" int32_t PlatThreadGetPriority(PlatThread* inThread)
{
    if (!inThread) {
        PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
        return PLAT_PRIORITY_ERROR;
    }
    pthread_mutex_lock(&inThread->mLock);
    int32_t thePriority = inThread->mPriority;
    pthread_mutex_unlock(&inThread->mLock);
    return thePriority;
}

extern "C" int PlatThreadSetPriority(PlatThread* inThread, int32_t inPriority)
{
    if (!inThread)
        return PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
    if (inPriority < PLAT_PRIORITY_IDLE || inPriority > PLAT_PRIORITY_TIME_CRITICAL)
        return PlatPosix::Fail(PlatPosix::kErrorInvalidParameter);
    pthread_mutex_lock(&inThread->mLock);
    inThread->mPriority = inPriority;
    bool theStarted = inThread->mStartGate == 0;
    pthread_mutex_unlock(&inThread->mLock);
    // the calling thread applies its own priority now; another thread's goes through the start gate (not yet
    // running) or, on Linux, through its thread id
    if (inThread == tCurrentThread)
        ApplyPriority(inPriority);
#if defined(__linux__)
    else if (theStarted && inThread->mRunning) {
        int theNice = inPriority <= PLAT_PRIORITY_IDLE ? 19 : inPriority >= PLAT_PRIORITY_TIME_CRITICAL ? -10
            : -inPriority * 2;
        setpriority(PRIO_PROCESS, static_cast<id_t>(inThread->mThreadID), theNice);
    }
#else
    (void)theStarted;
#endif
    return 1;
}

extern "C" uint32_t PlatThreadSuspend(PlatThread* inThread)
{
    if (!inThread) {
        PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
        return 0xFFFFFFFFu;
    }
    pthread_mutex_lock(&inThread->mLock);
    uint32_t theResult = 0xFFFFFFFFu;
    if (inThread->mStartGate > 0)
        theResult = inThread->mStartGate++;   // not started yet: one more resume needed
    pthread_mutex_unlock(&inThread->mLock);
    if (theResult == 0xFFFFFFFFu)
        PlatPosix::Fail(PlatPosix::kErrorNotSupported);
    return theResult;
}

extern "C" uint32_t PlatThreadResume(PlatThread* inThread)
{
    if (!inThread) {
        PlatPosix::Fail(PlatPosix::kErrorInvalidHandle);
        return 0xFFFFFFFFu;
    }
    pthread_mutex_lock(&inThread->mLock);
    uint32_t theCount = inThread->mStartGate;
    if (theCount > 0 && --inThread->mStartGate == 0)
        pthread_cond_broadcast(&inThread->mStartCondition);
    pthread_mutex_unlock(&inThread->mLock);
    return theCount;
}

extern "C" int PlatThreadTerminate(PlatThread* inThread)
{
    (void)inThread;
    return PlatPosix::Fail(PlatPosix::kErrorNotSupported);
}

extern "C" int32_t PlatAtomicIncrement(volatile int32_t* ioValue)
{
    return __atomic_add_fetch(ioValue, 1, __ATOMIC_SEQ_CST);
}

extern "C" int32_t PlatAtomicDecrement(volatile int32_t* ioValue)
{
    return __atomic_sub_fetch(ioValue, 1, __ATOMIC_SEQ_CST);
}
