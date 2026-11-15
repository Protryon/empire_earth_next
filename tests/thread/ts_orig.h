// The original Thread System (include/lle/TSCriticalSection.h, TSEvent.h, TSReadWriteLock.h, TSThread.h,
// TSScheduler.h and src/Low-Level Engine/Thread System/TSCriticalSection.cpp, TSEvent.cpp, TSReadWriteLock.cpp,
// TSScheduler.cpp, TSThread.cpp, Thread System.cpp at a1775ddf, before they moved to rust/crates/ee_ts), unchanged
// but for the includes, for the golden driver ts_golden.cpp (which supplies the platform layer, UString, the Error
// System's calls and URegisterShutdown).
// C:\EEX\Low-Level Engine\Thread System\TSCriticalSection.h (RECONSTRUCTION; layout proven by the engine's own
// TSCriticalSection.cpp 0x53ca1..0x53cee and TSScheduler::CreateCriticalSection operator new(0x20)).
#ifndef LLE_TSCRITICALSECTION_H
#define LLE_TSCRITICALSECTION_H


class LLE_API TSCriticalSection {
    friend class TSScheduler;               // only the scheduler creates/destroys (private ctor/dtor exported)
public:
    void Enter();
    void Leave();
    bool TryEnter();   // port: Enter without waiting; false when another thread holds it (the test harness)
private:
    TSCriticalSection();
    ~TSCriticalSection();

    PlatMutex* mSection;                    // +0x00 (the original held a CRITICAL_SECTION here; platform/PlatThread.h)
    unsigned long mOwnerThread;             // +0x18 thread id while held (set on first Enter)
    unsigned long mLockCount;               // +0x1c recursion count
};

#endif
// C:\EEX\Low-Level Engine\Thread System\TSEvent.h (RECONSTRUCTION; layout proven by TSEvent.cpp 0x53cee..0x53d8b,
// TSScheduler::CreateEvent operator new(8)).
#ifndef LLE_TSEVENT_H
#define LLE_TSEVENT_H


// TSEvent::Wait / WaitAny timeouts are milliseconds; this one waits forever (Win32 INFINITE)
const unsigned long kTSWaitInfinite = PLAT_WAIT_INFINITE;
// TSEvent::WaitAny results besides the index of the event that was set
enum TSWaitResult {
    kTSWaitTimeout = PLAT_WAIT_TIMEOUT,
    kTSWaitFailed = PLAT_WAIT_FAILED,
};

class LLE_API TSEvent {
    friend class TSScheduler;
public:
    long Set();
    long Clear();
    long Wait(unsigned long inTimeout);
    // Waits until one of the events is set (at most PLAT_WAIT_MAX) -> its index (the lowest when several are),
    // kTSWaitTimeout or kTSWaitFailed (PlatGetLastError); the threads' message loops wait on several at once.
    static long WaitAny(TSEvent* const* inEvents, unsigned long inCount, unsigned long inTimeout);
    // exported inline members, ICF-folded: GetIsManuallyReset 0x3d783 (in NELagLossSimulator.cpp's inline block,
    // just before TSThread's), operator void* 0x10b8d
    bool GetIsManuallyReset() { return this->mManualReset; }
    operator void*() const { return this->mHandle; }

    PlatEvent* mHandle;                     // +0x00 (a Win32 event HANDLE in the original; TMTimer's event timers use it)
private:
    TSEvent(PlatEvent* inHandle, bool inManualReset);
    ~TSEvent();

    bool mManualReset;                      // +0x04
};

#endif
// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.h (RECONSTRUCTION; TSReadWriteLock.cpp 0x53d8b..0x53e55,
// operator new(0x10) in TSScheduler::CreateReadWriteLock). Member names are provisional.
#ifndef LLE_TSREADWRITELOCK_H
#define LLE_TSREADWRITELOCK_H


class TSCriticalSection;
class TSEvent;

class LLE_API TSReadWriteLock {
    friend class TSScheduler;
public:
    long GetReadLockCount() const { return this->mReaderCount; }  // exported, ICF-folded (0x4495)
    void ReadLock();
    void ReadUnlock();
    void WriteLock();
    void WriteUnlock();
private:
    TSReadWriteLock(TSCriticalSection* inWriteLock, TSEvent* inNoReaders, TSEvent* inReading);
    ~TSReadWriteLock();

    TSCriticalSection* mWriteLock;          // +0x00
    TSEvent* mNoReaders;                    // +0x04 auto-reset; set when the last reader leaves
    TSEvent* mReading;                      // +0x08 manual-reset; set while readers hold the lock
    volatile int32_t mReaderCount;          // +0x0c (PlatAtomicIncrement/Decrement)
};

#endif
// C:\EEX\Low-Level Engine\Thread System\TSThread.h (RECONSTRUCTION; TSThread.cpp 0x54203..0x545ff, operator
// new(0x3c) in TSScheduler::CreateThread). Member names are provisional.
#ifndef LLE_TSTHREAD_H
#define LLE_TSTHREAD_H


class TSCriticalSection;

// a message posted to a thread (TSThread::Post: the original's Win32 thread message)
struct TSPostedMessage {
    unsigned long mMessage;
    void* mParameter1;
    void* mParameter2;
};

// winbase.h's `#define Yield()` would swallow TSThread::Yield (exported ?Yield@TSThread@@SAXK@Z)
#ifdef Yield
#undef Yield
#endif

class LLE_API TSThread {
    friend class TSScheduler;
public:
    unsigned long GetExitCode();
    long GetPriority();
    long SetPriority(long inPriority);
    long Post(unsigned long inMessage, void* inParameter1, void* inParameter2, bool inQueue);
    long Receive(unsigned long& outMessage, void*& outParameter1, void*& outParameter2);
    long Resume();
    long Suspend();
    long Terminate();
    void SetName(UString& inName);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void SetName(const UString& inName) { SetName(const_cast<UString&>(inName)); }
#endif
    static void SetThreadName(unsigned long inThreadID, const char* inName);
    // proven inline: exported COMDATs kept in NELagLossSimulator.cpp's range (0x3d78b..0x3d7ba, declaration
    // order), i.e. the first unit in link order that includes this header after the Network Engine headers.
    // Requires TSCriticalSection (TSScheduler.h includes it first).
    static void ForceYield() {}             // exported, empty: ICF-folded into the bare ret at 0x1000c16a
    UString& GetName() { return this->mName; }
    bool IsSuspended() const { return this->mSuspended; }
    bool IsTerminated() { return this->GetExitCode() != PLAT_THREAD_STILL_ACTIVE; }
    void LockMessageQueue() { this->mMessageLock->Enter(); }
    void UnlockMessageQueue() { this->mMessageLock->Leave(); }
    static void Yield(unsigned long inMilliseconds) { PlatSleep(inMilliseconds); }
    // The window events' pump (platform/PlatWindow.h): Receive on inThreadID (the thread that runs the window system)
    // dispatches one window event per call once its posted messages are drained, as the original's PeekMessage
    // dispatched window messages. inPump returns true when it dispatched one.
    static void SetEventPump(bool (*inPump)(), unsigned long inThreadID);
    // Dispatches the window events waiting now (on the event thread; elsewhere nothing): what Win32 sent a new window
    // during its creation (activation, focus) is handled before the game knows the window, as it was
    static void DispatchWindowEvents();
private:
    TSThread(unsigned long (*inFunction)(void*), void* inParameter, unsigned long inStackSize, bool inSuspended);
    TSThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended);
    ~TSThread();
    long StartThread(unsigned long inStackSize);
    static uint32_t ThreadMain(void* inThread);

    TSCriticalSection* mMessageLock;        // +0x00 (ESVERIFY text: this->mMessageLock)
    std::deque<unsigned long> mMessages;    // +0x04 queued message ids (Post inQueue / Receive)
    UString mName;                          // +0x18
    unsigned long (*mFunction)(void*);      // +0x28
    void* mParameter;                       // +0x2c
    PlatThread* mHandle;                    // +0x30 (a Win32 thread HANDLE in the original)
    unsigned long mThreadID;                // +0x34
    bool mSuspended;                        // +0x38
    // the thread's message queue (Post/Receive; the original used the Win32 thread message queue)
    PlatMutex* mPostLock;
    std::deque<TSPostedMessage> mPosted;
    static bool (*sEventPump)();
    static unsigned long sEventThread;
};

// TSThread::Post message ids. Only the id every thread shares is named: the other ids mean different things per receiving thread.
enum TSThreadMessage {
    kThreadMessageQuit = 0x401,  // every thread's HandleMessage stops on 0x401 (RTSGraphics mIsQuitting, RTSPhysics mStopThread, RTSMousePointer/EEDataPreLoading mQuit, EESELoadThread mIsDone, EEUserInterface mB05F); every Shutdown() / PostQuit posts it (provisional name)
};

#endif
// C:\EEX\Low-Level Engine\Thread System\TSScheduler.h (RECONSTRUCTION; TSScheduler.cpp 0x53e55..0x54203).
// Include <windows.h> first: CreateEvent is exported as ?CreateEventA@TSScheduler@@...
#ifndef LLE_TSSCHEDULER_H
#define LLE_TSSCHEDULER_H


// Only declared here: TSEvent, TSReadWriteLock and TSThread have exported inline members, which the original's first
// includer (NELagLossSimulator.cpp) emits; Error System.cpp uses the scheduler without seeing them.
// lle/LowLevelEngine.h includes their headers before this one.
class TSEvent;
class TSReadWriteLock;
class TSThread;

LLE_API long TSInitialize();   // Thread System.cpp

class LLE_API TSScheduler {
    friend LLE_API long TSInitialize();           // registers the main thread through the private CreateThread
public:
    static long CreateCriticalSection(TSCriticalSection*& outSection);
    static void DestroyCriticalSection(TSCriticalSection* inSection);
    // defaults proven by TCP 0x287c: the assert text passes one argument, the call pushes 1
    static long CreateEvent(TSEvent*& outEvent, bool inManualReset = true);
    static void DestroyEvent(TSEvent* inEvent);
    static long CreateReadWriteLock(TSReadWriteLock*& outLock);
    static void DestroyReadWriteLock(TSReadWriteLock* inLock);
    // defaults proven by TCP 0x287c: the assert text passes three arguments, the call pushes 0, 0
    static long CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                             unsigned long inStackSize = 0, bool inSuspended = false);
    static long DestroyThread(TSThread* inThread);
    static TSThread* GetCurrentThread();
    static unsigned long GetCurrentThreadIndex();
private:
    static long CreateThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended);

    // Inferred: visible to the engine only. A static data member consumes a VC6 `$E` number in every unit that
    // sees the class, and the plug-ins that use TSScheduler (tcp, mixer) number their initializers as if they saw
    // none (see LowLevelEngine.h, Timing).
#ifdef LOWLEVELENGINE_EXPORTS
    static std::vector<TSThread*> sThreads;
#endif
};

#endif
// C:\EEX\Low-Level Engine\Thread System\TSCriticalSection.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


// Creates an unowned critical section
TSCriticalSection::TSCriticalSection()
{
    this->mLockCount = 0;
    this->mOwnerThread = 0;
    this->mSection = PlatMutexCreate();
    ESVERIFY(this->mSection != NULL);
}

// Destroys the critical section
TSCriticalSection::~TSCriticalSection()
{
    PlatMutexDestroy(this->mSection);
}

// Enters the critical section, recording the owning thread on the first entry
void TSCriticalSection::Enter()
{
    PlatMutexLock(this->mSection);
    this->mLockCount++;
    if (this->mLockCount == 1)
        this->mOwnerThread = PlatCurrentThreadId();
}

// Leaves the critical section, clearing the owning thread on the last exit
void TSCriticalSection::Leave()
{
    this->mLockCount--;
    if (this->mLockCount == 0)
        this->mOwnerThread = 0;
    PlatMutexUnlock(this->mSection);
}

// Port addition: enters the critical section only if no other thread holds it (tools/harness's UI fixture must never
// block the render thread); records the owner as Enter does
bool TSCriticalSection::TryEnter()
{
    if (!PlatMutexTryLock(this->mSection))
        return false;
    this->mLockCount++;
    if (this->mLockCount == 1)
        this->mOwnerThread = PlatCurrentThreadId();
    return true;
}
// C:\EEX\Low-Level Engine\Thread System\TSEvent.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


// Wraps an event handle (manual or automatic reset)
TSEvent::TSEvent(PlatEvent* inHandle, bool inManualReset)
    : mHandle(inHandle), mManualReset(inManualReset)
{
}

// Destroys the event, closing its handle
TSEvent::~TSEvent()
{
    PlatEventDestroy(this->mHandle);
}

// Signals the event; records the native error on failure
long TSEvent::Set()
{
    long theResult;
    if (PlatEventSet(this->mHandle))
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// Resets the event to unsignalled; records the native error on failure
long TSEvent::Clear()
{
    long theResult;
    if (PlatEventReset(this->mHandle))
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// Waits for the event up to the timeout; kErrorTimeout on time-out, records the native error on failure
long TSEvent::Wait(unsigned long inTimeout)
{
    long theResult;
    long theWait = PlatEventWait(this->mHandle, inTimeout);
    if (theWait == PLAT_WAIT_TIMEOUT)
        theResult = kErrorTimeout;
    else if (theWait != PLAT_WAIT_FAILED)
        theResult = kErrorOK;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// Waits until one of the events is set -> its index, kTSWaitTimeout or kTSWaitFailed (the callers report failures)
long TSEvent::WaitAny(TSEvent* const* inEvents, unsigned long inCount, unsigned long inTimeout)
{
    PlatEvent* theHandles[PLAT_WAIT_MAX];
    if (inCount > PLAT_WAIT_MAX)
        return kTSWaitFailed;
    for (unsigned long i = 0; i < inCount; i++)
        theHandles[i] = inEvents[i]->mHandle;
    return PlatEventWaitAny(theHandles, inCount, inTimeout);
}
// C:\EEX\Low-Level Engine\Thread System\TSReadWriteLock.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


// Creates a read/write lock from a writer lock and two events, with no readers
TSReadWriteLock::TSReadWriteLock(TSCriticalSection* inWriteLock, TSEvent* inNoReaders, TSEvent* inReading)
    : mWriteLock(inWriteLock), mNoReaders(inNoReaders), mReading(inReading)
{
    this->mNoReaders->Set();
    this->mReading->Clear();
    this->mReaderCount = 0;
}

// Destroys the read/write lock and its writer lock and events
TSReadWriteLock::~TSReadWriteLock()
{
    TSScheduler::DestroyCriticalSection(this->mWriteLock);
    TSScheduler::DestroyEvent(this->mNoReaders);
    TSScheduler::DestroyEvent(this->mReading);
}

// Takes a read lock: the first reader waits until no writer holds the lock, then readers wait for reading to open
void TSReadWriteLock::ReadLock()
{
    if (PlatAtomicIncrement(&this->mReaderCount) == 1) {
        this->mNoReaders->Wait(kTSWaitInfinite);
        this->mReading->Set();
    }
    this->mReading->Wait(kTSWaitInfinite);
}

// Releases a read lock: the last reader closes reading and lets writers in
void TSReadWriteLock::ReadUnlock()
{
    if (PlatAtomicDecrement(&this->mReaderCount) == 0) {
        this->mReading->Clear();
        this->mNoReaders->Set();
    }
}

// Takes the write lock: one writer at a time, waiting until no readers are left
void TSReadWriteLock::WriteLock()
{
    this->mWriteLock->Enter();
    this->mNoReaders->Wait(kTSWaitInfinite);
}

// Releases the write lock and lets readers in
void TSReadWriteLock::WriteUnlock()
{
    this->mNoReaders->Set();
    this->mWriteLock->Leave();
}
// C:\EEX\Low-Level Engine\Thread System\TSScheduler.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


std::vector<TSThread*> TSScheduler::sThreads;

// Creates a critical section; kErrorOutOfMemory if the allocation fails
long TSScheduler::CreateCriticalSection(TSCriticalSection*& outSection)
{
    outSection = new TSCriticalSection;
    return outSection ? kErrorOK : kErrorOutOfMemory;
}

// Creates an event (manual or auto reset) wrapped in a TSEvent; records the native error if creation fails
// NEAR: register choice only (the original keeps theResult/NULL in esi and the handle in edi)
long TSScheduler::CreateEvent(TSEvent*& outEvent, bool inManualReset)
{
    long theResult = kErrorOK;
    PlatEvent* theHandle = PlatEventCreate(inManualReset, 0);
    if (theHandle) {
        outEvent = new TSEvent(theHandle, inManualReset);
        if (outEvent == NULL) {
            PlatEventDestroy(theHandle);
            theResult = kErrorOutOfMemory;
        }
    } else {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// Creates a read/write lock from a write critical section and two events, destroying the parts made so far on failure
long TSScheduler::CreateReadWriteLock(TSReadWriteLock*& outLock)
{
    TSCriticalSection* theWriteLock = NULL;
    TSEvent* theNoReaders = NULL;
    TSEvent* theReading = NULL;
    long theResult = TSScheduler::CreateCriticalSection(theWriteLock);
    if (theResult == kErrorOK) {
        theResult = TSScheduler::CreateEvent(theNoReaders, false);
        if (theResult == kErrorOK) {
            theResult = TSScheduler::CreateEvent(theReading, true);
            if (theResult == kErrorOK) {
                outLock = new TSReadWriteLock(theWriteLock, theNoReaders, theReading);
                if (outLock == NULL) {
                    TSScheduler::DestroyCriticalSection(theWriteLock);
                    TSScheduler::DestroyEvent(theNoReaders);
                    TSScheduler::DestroyEvent(theReading);
                    theResult = kErrorOutOfMemory;
                }
            } else {
                TSScheduler::DestroyCriticalSection(theWriteLock);
                TSScheduler::DestroyEvent(theNoReaders);
            }
        } else
            TSScheduler::DestroyCriticalSection(theWriteLock);
    }
    return theResult;
}

// Creates, registers and starts a thread object for a function; kErrorOutOfMemory if allocation fails
// NEAR: block layout only (the original places the kErrorOutOfMemory return after the success path)
long TSScheduler::CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                               unsigned long inStackSize, bool inSuspended)
{
    outThread = new TSThread(inFunction, inParameter, inStackSize, inSuspended);
    if (outThread) {
        TSScheduler::sThreads.push_back(outThread);
        return outThread->StartThread(inStackSize);
    }
    return kErrorOutOfMemory;
}

// Registers an existing thread (handle and ID) as a thread object; kErrorOutOfMemory if allocation fails
long TSScheduler::CreateThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended)
{
    TSThread* theThread = new TSThread(inHandle, inThreadID, inSuspended);
    if (theThread) {
        TSScheduler::sThreads.push_back(theThread);
        return kErrorOK;
    }
    return kErrorOutOfMemory;
}

// Destroys a critical section (NULL is ignored)
void TSScheduler::DestroyCriticalSection(TSCriticalSection* inSection)
{
    if (inSection)
        delete inSection;
}

// Destroys an event (NULL is ignored)
void TSScheduler::DestroyEvent(TSEvent* inEvent)
{
    if (inEvent)
        delete inEvent;
}

// Destroys a read/write lock (NULL is ignored)
void TSScheduler::DestroyReadWriteLock(TSReadWriteLock* inLock)
{
    if (inLock)
        delete inLock;
}

// Unregisters and deletes a thread object; kErrorNotFound if it is not registered (NULL is ignored)
long TSScheduler::DestroyThread(TSThread* inThread)
{
    long theResult = kErrorOK;
    if (inThread) {
        std::vector<TSThread*>::iterator theThread = std::find(TSScheduler::sThreads.begin(), TSScheduler::sThreads.end(), inThread);
        if (theThread != TSScheduler::sThreads.end()) {
            delete inThread;
            TSScheduler::sThreads.erase(theThread);
            theResult = kErrorOK;
        } else
            theResult = kErrorNotFound;
    }
    return theResult;
}

// Returns the registered thread object of the calling thread, or NULL
TSThread* TSScheduler::GetCurrentThread()
{
    TSThread* theThread = NULL;
    unsigned long theThreadID = PlatCurrentThreadId();
    for (unsigned long i = 0; i < TSScheduler::sThreads.size(); i++)
        if (TSScheduler::sThreads[i]->mThreadID == theThreadID) {
            theThread = TSScheduler::sThreads[i];
            break;
        }
    return theThread;
}

// Returns the registry index of the calling thread, or -1 if it is not registered
unsigned long TSScheduler::GetCurrentThreadIndex()
{
    unsigned long theIndex = (unsigned long)-1;
    unsigned long theThreadID = PlatCurrentThreadId();
    for (unsigned long i = 0; i < TSScheduler::sThreads.size(); i++)
        if (TSScheduler::sThreads[i]->mThreadID == theThreadID) {
            theIndex = i;
            break;
        }
    return theIndex;
}
// C:\EEX\Low-Level Engine\Thread System\TSThread.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


// Creates a thread object for a function and parameter (not started yet), with its message lock
TSThread::TSThread(unsigned long (*inFunction)(void*), void* inParameter, unsigned long inStackSize,
                   bool inSuspended)
    : mFunction(inFunction), mParameter(inParameter), mSuspended(inSuspended), mPostLock(PlatMutexCreate())
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 51);
}

// Creates a thread object wrapping an existing thread handle and ID, with its message lock
TSThread::TSThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended)
    : mFunction(NULL), mParameter(NULL), mHandle(inHandle), mThreadID(inThreadID), mSuspended(inSuspended),
      mPostLock(PlatMutexCreate())
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 71);
}

// Destroys the thread object, closing its handle and message lock (the thread itself keeps running)
TSThread::~TSThread()
{
    PlatThreadClose(this->mHandle);
    TSScheduler::DestroyCriticalSection(this->mMessageLock);
    PlatMutexDestroy(this->mPostLock);
}

bool (*TSThread::sEventPump)();
unsigned long TSThread::sEventThread;

void TSThread::SetEventPump(bool (*inPump)(), unsigned long inThreadID)
{
    TSThread::sEventPump = inPump;
    TSThread::sEventThread = inThreadID;
}

void TSThread::DispatchWindowEvents()
{
    if (TSThread::sEventPump != NULL && PlatCurrentThreadId() == TSThread::sEventThread)
        while (TSThread::sEventPump())
            ;
}

// Returns the thread's exit code (STILL_ACTIVE while running); 0 and a recorded native error on failure
unsigned long TSThread::GetExitCode()
{
    uint32_t theExitCode;
    if (!PlatThreadGetExitCode(this->mHandle, &theExitCode)) {
        ESRecordNativeError(PlatGetLastError());
        ESGetNativeError();
        theExitCode = 0;
    }
    return theExitCode;
}

// Returns the thread's priority (PLAT_PRIORITY_ERROR, with the error recorded, on failure)
long TSThread::GetPriority()
{
    long thePriority = PlatThreadGetPriority(this->mHandle);
    if (thePriority == PLAT_PRIORITY_ERROR) {
        ESRecordNativeError(PlatGetLastError());
        ESGetNativeError();
    }
    return thePriority;
}

// (0x1005433f, POSIXThreadMain: the unused _beginthreadex twin of the thread entry, gone with it.)

// Posts a message to the thread's queue (the original's Win32 thread message queue, retried until it accepted);
// optionally queues its ID too
long TSThread::Post(unsigned long inMessage, void* inParameter1, void* inParameter2, bool inQueue)
{
    TSPostedMessage theMessage = {inMessage, inParameter1, inParameter2};
    PlatMutexLock(this->mPostLock);
    this->mPosted.push_back(theMessage);
    PlatMutexUnlock(this->mPostLock);
    if (inQueue != true)
        return kErrorOK;
    this->mMessageLock->Enter();
    this->mMessages.push_back(inMessage);
    this->mMessageLock->Leave();
    return kErrorOK;
}

// Takes one message off the calling thread's queue, else (on the window thread) dispatches one window event;
// kErrorNotFound if none is left. A posted message counts when it is WM_QUIT (0x12) or a WM_USER (0x400) one, as a
// window-less Win32 thread message did; a dispatched event counts only while queued IDs are left (Post inQueue).
long TSThread::Receive(unsigned long& outMessage, void*& outParameter1, void*& outParameter2)
{
    long theResult = kErrorNotFound;
    TSPostedMessage theMessage = {0, NULL, NULL};
    bool thePosted = false;
    PlatMutexLock(this->mPostLock);
    if (!this->mPosted.empty()) {
        theMessage = this->mPosted.front();
        this->mPosted.pop_front();
        thePosted = true;
    }
    PlatMutexUnlock(this->mPostLock);
    if (!thePosted && !(TSThread::sEventPump != NULL && PlatCurrentThreadId() == TSThread::sEventThread
                        && TSThread::sEventPump()))
        return theResult;
    this->mMessageLock->Enter();
    if (thePosted && (theMessage.mMessage == 0x12 || theMessage.mMessage >= 0x400))
        theResult = kErrorOK;
    else
        theResult = this->mMessages.size() > 0 ? kErrorOK : kErrorNotFound;
    outMessage = theMessage.mMessage;
    outParameter1 = theMessage.mParameter1;
    outParameter2 = theMessage.mParameter2;
    if (this->mMessages.size() > 0) {
        this->mMessages.front();
        this->mMessages.pop_front();
    }
    this->mMessageLock->Leave();
    return theResult;
}

// Resumes the suspended thread; kErrorNotSuspended if it was not suspended, kErrorNativeError on failure
// NEAR: the original stores mSuspended = false from al after zeroing eax for the return
long TSThread::Resume()
{
    unsigned long theCount = PlatThreadResume(this->mHandle);
    if (theCount == 0)
        return kErrorNotSuspended;
    if (theCount == 1) {
        this->mSuspended = false;
        return kErrorOK;
    }
    if (theCount != 0xFFFFFFFFu)
        return kErrorOK;
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// Names the thread (debugger name is a no-op in this build) and stores the name
void TSThread::SetName(UString& inName)
{
    TSThread::SetThreadName(this->mThreadID, inName);
    this->mName = inName;
}

// Starts the thread (suspended if requested), retrying up to 100 times; kErrorNativeError if it never starts
long TSThread::StartThread(unsigned long inStackSize)
{
    PlatThread* theHandle = NULL;
    uint32_t theThreadID;
    unsigned long i;
    for (i = theThreadID = 0; i < 100; i++) {
        if (theHandle != NULL)
            break;
        theHandle = PlatThreadCreate(TSThread::ThreadMain, this, inStackSize, this->mSuspended, &theThreadID);
        if (theHandle == NULL) {
            PlatGetLastError();
            PlatSleep(1);
        }
    }
    if (theHandle != NULL) {
        this->mHandle = theHandle;
        this->mThreadID = theThreadID;
    } else {
        ESVERIFY_AT("Thread could not be created" == NULL, 397);
        return kErrorNativeError;   // Fixed: used to report success for a thread that never started
    }
    return kErrorOK;
}

// Sets the thread's priority (PLAT_PRIORITY_*); kErrorNativeError on failure
long TSThread::SetPriority(long inPriority)
{
    if (PlatThreadSetPriority(this->mHandle, inPriority))
        return kErrorOK;
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// Suspends the thread; kErrorNativeError on failure
long TSThread::Suspend()
{
    if (PlatThreadSuspend(this->mHandle) != 0xFFFFFFFFu) {
        this->mSuspended = true;
        return kErrorOK;
    }
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// Kills the thread; kErrorNativeError on failure
long TSThread::Terminate()
{
    if (PlatThreadTerminate(this->mHandle))
        return kErrorOK;
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// Thread entry: runs the thread function; its result is the thread's exit code (the platform thread procedure
// initialises COM around it on Win32, as WIN32ThreadMain did here)
uint32_t TSThread::ThreadMain(void* inThread)
{
    TSThread* theThread = static_cast<TSThread*>(inThread);
    return theThread->mFunction(theThread->mParameter);
}

// Sets the debugger name of a thread: does nothing in this build (the debugger thread-name exception is debug-only).
// Exported, called out of line by SetName (0x100544b8). No marker: ICF folds it into the bare ret at 0x10080c07, so
// it has no address of its own.
void TSThread::SetThreadName(unsigned long inThreadID, const char* inName)
{
}
// C:\EEX\Low-Level Engine\Thread System\Thread System.cpp (file name inferred: no __FILE__; by analogy with
// Error System\Error System.cpp: the subsystem's initialize/shutdown entry points)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS


static void TSShutdown(bool inForced);

bool gThreadSystemInitialized;

// Starts the thread system: registers its shutdown and registers the calling (main) thread as a thread object
long TSInitialize()
{
    long theResult;
    PlatThread* theThread;
    gThreadSystemInitialized = true;
    if (URegisterShutdown(TSShutdown) == kErrorOK) {
        theThread = PlatThreadOpenCurrent();
        if (theThread != NULL)
            theResult = TSScheduler::CreateThread(theThread, PlatCurrentThreadId(), false);
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(PlatGetLastError());
            ESGetNativeError();
        }
    } else
        theResult = kErrorFailed;
    return theResult;
}

// Shuts the thread system down by destroying the calling thread's object
static void TSShutdown(bool inForced)
{
    TSThread* theThread = TSScheduler::GetCurrentThread();
    if (theThread)
        TSScheduler::DestroyThread(theThread);
}
