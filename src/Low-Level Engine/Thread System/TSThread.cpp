// C:\EEX\Low-Level Engine\Thread System\TSThread.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10054203
// Creates a thread object for a function and parameter (not started yet), with its message lock
TSThread::TSThread(unsigned long (*inFunction)(void*), void* inParameter, unsigned long inStackSize,
                   bool inSuspended)
    : mFunction(inFunction), mParameter(inParameter), mSuspended(inSuspended), mPostLock(PlatMutexCreate())
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 51);
}

// FUNCTION: LLE 0x1005425d
// Creates a thread object wrapping an existing thread handle and ID, with its message lock
TSThread::TSThread(PlatThread* inHandle, unsigned long inThreadID, bool inSuspended)
    : mFunction(NULL), mParameter(NULL), mHandle(inHandle), mThreadID(inThreadID), mSuspended(inSuspended),
      mPostLock(PlatMutexCreate())
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 71);
}

// FUNCTION: LLE 0x100542bf
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

// FUNCTION: LLE 0x100542e5
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

// FUNCTION: LLE 0x10054315
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

// FUNCTION: LLE 0x10054377
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

// FUNCTION: LLE 0x100543c4
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

// FUNCTION: LLE 0x1005446f
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

// FUNCTION: LLE 0x100544aa
// Names the thread (debugger name is a no-op in this build) and stores the name
void TSThread::SetName(UString& inName)
{
    TSThread::SetThreadName(this->mThreadID, inName);
    this->mName = inName;
}

// FUNCTION: LLE 0x100544cd
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

// FUNCTION: LLE 0x10054552
// Sets the thread's priority (PLAT_PRIORITY_*); kErrorNativeError on failure
long TSThread::SetPriority(long inPriority)
{
    if (PlatThreadSetPriority(this->mHandle, inPriority))
        return kErrorOK;
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// FUNCTION: LLE 0x1005457a
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

// FUNCTION: LLE 0x100545a5
// Kills the thread; kErrorNativeError on failure
long TSThread::Terminate()
{
    if (PlatThreadTerminate(this->mHandle))
        return kErrorOK;
    ESRecordNativeError(PlatGetLastError());
    return kErrorNativeError;
}

// FUNCTION: LLE 0x100545c8
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
