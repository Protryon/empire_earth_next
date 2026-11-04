// C:\EEX\Low-Level Engine\Thread System\TSThread.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <process.h>

#include "lle/LowLevelEngine.h"

// FUNCTION: LLE 0x10054203
// Creates a thread object for a function and parameter (not started yet), with its message lock
TSThread::TSThread(unsigned long (*inFunction)(void*), void* inParameter, unsigned long inStackSize,
                   bool inSuspended)
    : mFunction(inFunction), mParameter(inParameter), mSuspended(inSuspended)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 51);
}

// FUNCTION: LLE 0x1005425d
// Creates a thread object wrapping an existing Win32 thread handle and ID, with its message lock
TSThread::TSThread(HANDLE inHandle, unsigned long inThreadID, bool inSuspended)
    : mFunction(NULL), mParameter(NULL), mHandle(inHandle), mThreadID(inThreadID), mSuspended(inSuspended)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mMessageLock) == keseOK, 71);
}

// FUNCTION: LLE 0x100542bf
// Destroys the thread object, closing its handle and message lock (the thread itself keeps running)
TSThread::~TSThread()
{
    CloseHandle(this->mHandle);
    TSScheduler::DestroyCriticalSection(this->mMessageLock);
}

// FUNCTION: LLE 0x100542e5
// Returns the thread's exit code (STILL_ACTIVE while running); 0 and a recorded native error on failure
unsigned long TSThread::GetExitCode()
{
    unsigned long theExitCode;
    if (!GetExitCodeThread(this->mHandle, &theExitCode)) {
        ESRecordNativeError(GetLastError());
        ESGetNativeError();
        theExitCode = 0;
    }
    return theExitCode;
}

// FUNCTION: LLE 0x10054315
// Returns the thread's Win32 priority (THREAD_PRIORITY_ERROR_RETURN, with the error recorded, on failure)
long TSThread::GetPriority()
{
    long thePriority = GetThreadPriority(this->mHandle);
    if (thePriority == THREAD_PRIORITY_ERROR_RETURN) {
        ESRecordNativeError(GetLastError());
        ESGetNativeError();
    }
    return thePriority;
}

// FUNCTION: LLE 0x1005433f
// Thread entry for _beginthreadex: initialises COM, runs the thread function, then ends the thread with its result
unsigned long __stdcall TSThread::POSIXThreadMain(void* inThread)
{
    unsigned long theResult;
    HRESULT theInitialize = CoInitialize(NULL);
    if (theInitialize == S_OK || theInitialize == S_FALSE) {
        TSThread* theThread = static_cast<TSThread*>(inThread);
        theResult = theThread->mFunction(theThread->mParameter);
        CoUninitialize();
    } else
        theResult = (unsigned long)-1;
    _endthreadex(theResult);
    return theResult;
}

// FUNCTION: LLE 0x10054377
// Posts a message to the thread's Win32 queue (retrying every 1 ms until it is accepted); optionally queues its ID too
long TSThread::Post(unsigned long inMessage, void* inParameter1, void* inParameter2, bool inQueue)
{
    BOOL thePosted;
    do {
        thePosted = PostThreadMessage(this->mThreadID, inMessage, reinterpret_cast<WPARAM>(inParameter1), (LPARAM)inParameter2);
        if (!thePosted)
            Sleep(1);
    } while (!thePosted);
    if (inQueue != true)
        return kErrorOK;
    this->mMessageLock->Enter();
    this->mMessages.push_back(inMessage);
    this->mMessageLock->Leave();
    return kErrorOK;
}

// FUNCTION: LLE 0x100543c4
// Takes one Win32 message off the calling thread's queue, dispatching window messages; kErrorNotFound if none is left
long TSThread::Receive(unsigned long& outMessage, void*& outParameter1, void*& outParameter2)
{
    long theResult = kErrorNotFound;
    MSG theMessage;
    if (PeekMessage(&theMessage, NULL, 0, 0, PM_REMOVE)) {
        this->mMessageLock->Enter();
        if (theMessage.message == WM_QUIT || (theMessage.hwnd == NULL && theMessage.message >= WM_USER))
            theResult = kErrorOK;
        else {
            TranslateMessage(&theMessage);
            DispatchMessage(&theMessage);
            theResult = this->mMessages.size() > 0 ? kErrorOK : kErrorNotFound;
        }
        outMessage = theMessage.message;
        outParameter1 = (void*)theMessage.wParam;
        outParameter2 = (void*)theMessage.lParam;
        if (this->mMessages.size() > 0) {
            this->mMessages.front();
            this->mMessages.pop_front();
        }
        this->mMessageLock->Leave();
    }
    return theResult;
}

// FUNCTION: LLE 0x1005446f
// Resumes the suspended thread; kErrorNotSuspended if it was not suspended, kErrorNativeError on failure
// NEAR: the original stores mSuspended = false from al after zeroing eax for the return
long TSThread::Resume()
{
    unsigned long theCount = ResumeThread(this->mHandle);
    if (theCount == 0)
        return kErrorNotSuspended;
    if (theCount == 1) {
        this->mSuspended = false;
        return kErrorOK;
    }
    if (theCount != (unsigned long)-1)
        return kErrorOK;
    ESRecordNativeError(GetLastError());
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
// Starts the Win32 thread (suspended if requested), retrying up to 100 times; kErrorNativeError if it never starts
long TSThread::StartThread(unsigned long inStackSize)
{
    HANDLE theHandle = NULL;
    unsigned long theThreadID;
    unsigned long i;
    for (i = theThreadID = 0; i < 100; i++) {
        if (theHandle != NULL)
            break;
        theHandle = CreateThread(NULL, inStackSize, TSThread::WIN32ThreadMain, this, this->mSuspended ? CREATE_SUSPENDED : 0,
                                 &theThreadID);
        if (theHandle == NULL) {
            GetLastError();
            Sleep(1);
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
// Sets the thread's Win32 priority; kErrorNativeError on failure
long TSThread::SetPriority(long inPriority)
{
    if (SetThreadPriority(this->mHandle, inPriority))
        return kErrorOK;
    ESRecordNativeError(GetLastError());
    return kErrorNativeError;
}

// FUNCTION: LLE 0x1005457a
// Suspends the thread; kErrorNativeError on failure
long TSThread::Suspend()
{
    if (SuspendThread(this->mHandle) != (unsigned long)-1) {
        this->mSuspended = true;
        return kErrorOK;
    }
    ESRecordNativeError(GetLastError());
    return kErrorNativeError;
}

// FUNCTION: LLE 0x100545a5
// Kills the thread with TerminateThread; kErrorNativeError on failure
long TSThread::Terminate()
{
    if (TerminateThread(this->mHandle, 0))
        return kErrorOK;
    ESRecordNativeError(GetLastError());
    return kErrorNativeError;
}

// FUNCTION: LLE 0x100545c8
// Thread entry for CreateThread: initialises COM, runs the thread function, then exits the thread with its result
unsigned long __stdcall TSThread::WIN32ThreadMain(void* inThread)
{
    unsigned long theResult;
    HRESULT theInitialize = CoInitialize(NULL);
    if (theInitialize == S_OK || theInitialize == S_FALSE) {
        TSThread* theThread = static_cast<TSThread*>(inThread);
        theResult = theThread->mFunction(theThread->mParameter);
        CoUninitialize();
    } else
        theResult = (unsigned long)-1;
    ExitThread(theResult);
    return theResult;
}

// Sets the debugger name of a thread: does nothing in this build (the debugger thread-name exception is debug-only).
// Exported, called out of line by SetName (0x100544b8). No marker: ICF folds it into the bare ret at 0x10080c07, so
// it has no address of its own.
void TSThread::SetThreadName(unsigned long inThreadID, const char* inName)
{
}
