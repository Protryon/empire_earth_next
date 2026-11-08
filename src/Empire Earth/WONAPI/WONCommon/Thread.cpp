// ThreadBase (0x14 bytes) and the Win32 Thread (0x1c bytes) with its execution hooks
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <process.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
namespace WONAPI {
// FUNCTION: WON 0x006a6c10
// Creates a stopped thread object with normal priority
ThreadBase::ThreadBase()
{
    this->mStopped=true;
    this->mRunning=false;
    this->mAutoDelete=false;
    this->mPriority=ThreadPriority_Normal;
}
// FUNCTION: WON 0x006a6c50
// Default start hook: starts nothing and marks the thread not running
void ThreadBase::StartHook() {this->mRunning=false;}
void ThreadBase::WaitForStopHook() {}
bool ThreadBase::InThisThreadHook() {return false;}
void ThreadBase::SetPriorityHook() {}
// 0x6a6c80 (emitted from the list below): destroys the thread object and its signal event
ThreadBase::~ThreadBase() {}
// FUNCTION: WON 0x006a6ca0
// Starts the thread through its start hook unless it is already running
void ThreadBase::Start()
{
    if(!this->mRunning) {this->mStopped=false;this->mRunning=true;this->StartHook();}
}
// FUNCTION: WON 0x006a6cc0
// Asks a running thread to stop, signals it and optionally waits for it to end
void ThreadBase::Stop(bool waitForStop)
{
    if(this->mStopped)
        return;
    this->mStopped=true;
    this->Signal();
    if(waitForStop) this->WaitForStop();
}
// FUNCTION: WON 0x006a6cf0
// Wakes the thread by setting its signal event
void ThreadBase::Signal() {this->mSignalEvent.Set();}
// FUNCTION: WON 0x006a6d00
// Returns true when called on this running thread
bool ThreadBase::InThisThread()
{
    if(!this->mRunning) return false;
    return this->InThisThreadHook();
}
// FUNCTION: WON 0x006a6d10
// Waits for a running thread to end, unless called from that thread
void ThreadBase::WaitForStop()
{
    if(this->mRunning && !this->InThisThread()) this->WaitForStopHook();
}
// FUNCTION: WON 0x006a85e0
// Thread entry: runs ThreadFunc, marks the thread stopped and auto-deletes it; dumps the stack on a crash
unsigned __stdcall Thread::StaticThreadFunc(void* data)
{
    __try {
        Thread* thread=static_cast<Thread*>(data);
        thread->mThreadId=GetCurrentThreadId();
        thread->ThreadFunc();
        thread->mRunning=false;
        if(thread->mAutoDelete) delete thread;
    } __except(WONDebug::DumpStackToFile(GetExceptionInformation())) {
    }
    return 0;
}
// FUNCTION: WON 0x006a8670
// Creates the Win32 thread, suspended first to apply a non-normal priority
void Thread::StartHook()
{
    unsigned threadAddress;
    int flags=this->mPriority==ThreadPriority_Normal ? 0 : CREATE_SUSPENDED;
    this->mThreadHandle=(HANDLE)_beginthreadex(NULL,0,Thread::StaticThreadFunc,this,flags,&threadAddress);
    if(flags) {
        this->SetPriorityHook();
        ResumeThread(this->mThreadHandle);
    }
}
// The original does not close the thread handle here
// FUNCTION: WON 0x006a86c0
// Waits for the Win32 thread to end
void Thread::WaitForStopHook() {WaitForSingleObject(this->mThreadHandle,INFINITE);}
// FUNCTION: WON 0x006a86d0
// Returns true when called on this thread
bool Thread::InThisThreadHook() {return GetCurrentThreadId()==this->mThreadId;}
// FUNCTION: WON 0x006a86f0
// Applies the thread's priority level to the Win32 thread
void Thread::SetPriorityHook()
{
    int priority=THREAD_PRIORITY_NORMAL;
    switch(this->mPriority) {
    case ThreadPriority_Highest:priority=THREAD_PRIORITY_HIGHEST;break;
    case ThreadPriority_High:priority=THREAD_PRIORITY_ABOVE_NORMAL;break;
    case ThreadPriority_Normal:priority=THREAD_PRIORITY_NORMAL;break;
    case ThreadPriority_Low:priority=THREAD_PRIORITY_BELOW_NORMAL;break;
    case ThreadPriority_Lowest:priority=THREAD_PRIORITY_LOWEST;break;
    }
    SetThreadPriority(this->mThreadHandle,priority);
}
}

// No original address identified: WON SDK SetPriority
namespace WONAPI {
void ThreadBase::SetPriority(ThreadPriority thePriority)
{
    if(thePriority == this->mPriority)
        return;

    this->mPriority = thePriority;
    if(this->mRunning)
        this->SetPriorityHook();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a6c60
// (public: virtual void * __thiscall WONAPI::ThreadBase::`scalar deleting destructor'(unsigned int)    [??_GThreadBase@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a6c80
// (public: virtual __thiscall WONAPI::ThreadBase::~ThreadBase(void)    [??1ThreadBase@WONAPI@@UAE@XZ])
