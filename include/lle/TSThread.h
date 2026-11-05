// C:\EEX\Low-Level Engine\Thread System\TSThread.h (RECONSTRUCTION; TSThread.cpp 0x54203..0x545ff, operator
// new(0x3c) in TSScheduler::CreateThread). Member names are provisional.
#ifndef LLE_TSTHREAD_H
#define LLE_TSTHREAD_H

#include <deque>

class TSCriticalSection;

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
};

// TSThread::Post message ids. Only the id every thread shares is named: the other ids mean different things per receiving thread.
enum TSThreadMessage {
    kThreadMessageQuit = 0x401,  // every thread's HandleMessage stops on 0x401 (RTSGraphics mIsQuitting, RTSPhysics mStopThread, RTSMousePointer/EEDataPreLoading mQuit, EESELoadThread mIsDone, EEUserInterface mB05F); every Shutdown() / PostQuit posts it (provisional name)
};

#endif
