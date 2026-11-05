// C:\EEX\Low-Level Engine\Thread System\TSScheduler.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x100541b2
// TEMPLATE: LLE 0x100541de
// TEMPLATE: LLE 0x100541eb
// SYNTHETIC: LLE 0x10053e55
// SYNTHETIC: LLE 0x10053e5f
// SYNTHETIC: LLE 0x10053e77
// SYNTHETIC: LLE 0x10053e83

std::vector<TSThread*> TSScheduler::sThreads;

// FUNCTION: LLE 0x10053e9e
// Creates a critical section; kErrorOutOfMemory if the allocation fails
long TSScheduler::CreateCriticalSection(TSCriticalSection*& outSection)
{
    outSection = new TSCriticalSection;
    return outSection ? kErrorOK : kErrorOutOfMemory;
}

// FUNCTION: LLE 0x10053ec6
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

// FUNCTION: LLE 0x10053f26
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

// FUNCTION: LLE 0x10053fdc
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

// FUNCTION: LLE 0x10054029
// Registers an existing Win32 thread (handle and ID) as a thread object; kErrorOutOfMemory if allocation fails
long TSScheduler::CreateThread(HANDLE inHandle, unsigned long inThreadID, bool inSuspended)
{
    TSThread* theThread = new TSThread(inHandle, inThreadID, inSuspended);
    if (theThread) {
        TSScheduler::sThreads.push_back(theThread);
        return kErrorOK;
    }
    return kErrorOutOfMemory;
}

// FUNCTION: LLE 0x1005406f
// Destroys a critical section (NULL is ignored)
void TSScheduler::DestroyCriticalSection(TSCriticalSection* inSection)
{
    if (inSection)
        delete inSection;
}

// FUNCTION: LLE 0x10054088
// Destroys an event (NULL is ignored)
void TSScheduler::DestroyEvent(TSEvent* inEvent)
{
    if (inEvent)
        delete inEvent;
}

// FUNCTION: LLE 0x100540a1
// Destroys a read/write lock (NULL is ignored)
void TSScheduler::DestroyReadWriteLock(TSReadWriteLock* inLock)
{
    if (inLock)
        delete inLock;
}

// FUNCTION: LLE 0x100540ba
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

// FUNCTION: LLE 0x10054142
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

// FUNCTION: LLE 0x1005417a
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
