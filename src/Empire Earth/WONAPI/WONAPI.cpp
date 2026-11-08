// WONAPICore: start-up, shutdown and pumping of the WON API's op queue, timer and socket threads
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/wonapi/WONStatus.h"
namespace WONAPI {
WONAPICore* WONAPICore::mInstance;
bool WONAPICore::mHaveWrittenVersion;
// FUNCTION: WON 0x006f2750
// Sets the core's defaults (version "wonapi121401", socket and timer threads on, no pump thread) and starts Winsock
void WONAPICore::Init()
{
    this->mVersionString="wonapi121401";
    WONAPICore::mInstance=this;
    this->mIsStarted=false;
    this->mDoPumpThread=false;
    this->mDoSocketAndTimerThreads=true;
    this->mSocketThread=NULL;
    AsyncSocket::StartWinsock();
}
// Creates the core and initialises its defaults (0x6f28d0 in the original, 173 bytes; listed below)
WONAPICore::WONAPICore(const char*) { this->Init(); }
// FUNCTION: WON 0x006f2980
// Destroys the core: shuts it down, deletes the socket thread and releases Winsock
WONAPICore::~WONAPICore()
{
    this->Shutdown();
    delete this->mSocketThread;
    AsyncSocket::StopWinsock();
    WONAPICore::mInstance=NULL;
}
// FUNCTION: WON 0x006f2a80
// Once per run, rewrites _wonapiversion.txt in the file directory with the API version, only if the file exists
void WONAPICore::WriteVersion()
{
    if(WONAPICore::mHaveWrittenVersion) return;
    WONAPICore::mHaveWrittenVersion=true;
    std::string path=WONAPICore::GetDefaultFileDirectory()+"_wonapiversion.txt";
    FILE* file=fopen(path.c_str(),"r");
    if(file!=NULL) {
        fclose(file);
        file=fopen(path.c_str(),"w");
        if(file!=NULL) {
            fprintf(file,"%s\n",this->mVersionString.c_str());
            fclose(file);
        }
    }
}
// FUNCTION: WON 0x006f2b50
// Starts the API once: writes the version file and starts the socket, timer and (if enabled) pump threads
void WONAPICore::Startup()
{
    if(this->mIsStarted) return;
    this->mIsStarted=true;
    this->WriteVersion();
    if(this->mSocketThread==NULL) this->mSocketThread=new SocketThreadSimple;
    this->mSocketThread->SetSingleThreaded(!this->mDoSocketAndTimerThreads);
    if(this->mDoSocketAndTimerThreads) {
        if(this->mSocketThread->NeedThread()) this->mSocketThread->Start();
        this->mTimerThread.Start();
    }
    if(this->mDoPumpThread) this->mPumpThread.Start();
}
// FUNCTION: WON 0x006f2c80
// Stops the pump, timer and socket threads, waits for them and kills every op they or the queue still hold
void WONAPICore::Shutdown()
{
    if(!this->mIsStarted) return;
    this->mIsStarted=false;
    this->mPumpThread.Stop(false);
    this->mTimerThread.Stop(false);
    this->mSocketThread->Stop(false);
    this->mPumpThread.WaitForStop();
    this->mTimerThread.WaitForStop();
    this->mSocketThread->WaitForStop();
    this->mTimerThread.PurgeOps();
    this->mSocketThread->PurgeOps();
    this->mOpContainer.PurgeOps();
}
// FUNCTION: WON 0x006f2d00
// Returns whether the API core exists and has been started
bool WONAPICore::IsStarted() { return WONAPICore::mInstance?WONAPICore::mInstance->mIsStarted:false; }
// FUNCTION: WON 0x006f2d20
// Starts and completes queued ops for the given milliseconds; without its own threads also pumps socket and timer ops
void WONAPICore::Pump(unsigned long milliseconds,int maxOps)
{
    DWORD start=GetTickCount();
    DWORD elapsed=0;
    while(true) {
        DWORD waitTime=milliseconds-elapsed;
        if(this->mDoSocketAndTimerThreads) this->mOpContainer.Pump(waitTime,maxOps);
        else {
            DWORD timerWait=this->mTimerThread.GetWaitTime();
            if(timerWait<waitTime) waitTime=timerWait;
            this->mSocketThread->Pump(waitTime);
            this->mTimerThread.Pump();
            this->mOpContainer.Pump(0,maxOps);
        }
        elapsed=GetTickCount()-start;
        if(elapsed>=milliseconds) break;
    }
}
// FUNCTION: WON 0x006f2da0
// Returns the directory the API writes its files to (empty without a core)
const std::string& WONAPICore::GetDefaultFileDirectory()
{
    static std::string anEmptyStr;
    return WONAPICore::mInstance==NULL?anEmptyStr:WONAPICore::mInstance->mFileDirectory;
}
// FUNCTION: WON 0x006f2e60
// Queues an op for the pump to start, or to complete if finished; before start-up only finished ops are queued
void WONAPICore::QueueOp(AsyncOp* op)
{
    if(WONAPICore::IsStarted()) WONAPICore::mInstance->mOpContainer.QueueOp(op);
    else if(WONAPICore::mInstance!=NULL) {
        if(op->mStatus!=WS_Pending) WONAPICore::mInstance->mOpContainer.QueueOp(op);
    }
}
// FUNCTION: WON 0x006f2ea0
// Hands an op to the timer thread with a timeout; kills it when the API is not started
void WONAPICore::AddTimerOp(AsyncOp* op,unsigned long timeout)
{
    if(WONAPICore::IsStarted()) WONAPICore::mInstance->mTimerThread.AddTimerOp(op,timeout);
    else op->Kill(true);
}
// FUNCTION: WON 0x006f2ed0
// Takes an op back from the timer thread
void WONAPICore::RemoveTimerOp(AsyncOp* op)
{
    if(WONAPICore::IsStarted()) WONAPICore::mInstance->mTimerThread.RemoveTimerOp(op);
}
// FUNCTION: WON 0x006f2ef0
// Hands a pending socket op to the socket thread; kills it when the API is not started
void WONAPICore::AddToSocketThread(SocketOp* op)
{
    if(WONAPICore::IsStarted()) WONAPICore::mInstance->mSocketThread->AddSocketOp(op);
    else op->Kill(true);
}
// FUNCTION: WON 0x006f2f20
// Takes a socket op back from the socket thread
void WONAPICore::RemoveFromSocketThread(SocketOp* op)
{
    if(WONAPICore::IsStarted()) WONAPICore::mInstance->mSocketThread->RemoveSocketOp(op);
}
// Creates the core with an empty name (0x6f2f40 in the original, 23 bytes; listed below)
WONAPICoreEx::WONAPICoreEx() : WONAPICore("") {}
// Turns the pump thread on or off before start-up (0x6f2f90 in the original, 23 bytes)
void WONAPICoreEx::SetDoPumpThread(bool value)
{
    if(!this->mIsStarted) this->mDoPumpThread=value;
}

void WONAPICore::PumpThread::ThreadFunc()
{
    WONAPICore* api=WONAPICore::GetInstance();
    while(true) {
        api->Pump(20);
        if(this->mStopped) return;
    }
}
}

namespace WONAPI {
// No original address identified: WON SDK WONAPICoreEx settings
void WONAPICoreEx::SetDefaultFileDirectory(const char* theDirectory)
{
	if(this->mIsStarted)
		return;
	this->mFileDirectory = theDirectory;
	if(this->mFileDirectory.empty())
		return;
	char anEndChar = this->mFileDirectory[this->mFileDirectory.length()-1];
	if(anEndChar!='\\' && anEndChar!='/')
		this->mFileDirectory += '/';
}

void WONAPICoreEx::SetSocketThread(SocketThread *theThread)
{
	if(!this->mIsStarted)
	{
		if(this->mSocketThread!=NULL)
			delete this->mSocketThread;

		this->mSocketThread = theThread;
	}
	else
		delete theThread;
}

void WONAPICoreEx::SetDoSocketAndTimerThreads(bool doSocketAndTimerThreads)
{
	if(!this->mIsStarted)
		this->mDoSocketAndTimerThreads = doSocketAndTimerThreads;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f2f40
// (public: __thiscall WONAPI::WONAPICoreEx::WONAPICoreEx(void))
// TEMPLATE: WON 0x006f2f80
// (public: virtual __thiscall WONAPI::WONAPICoreEx::~WONAPICoreEx(void))
// TEMPLATE: WON 0x006f2f60
// (public: virtual void * __thiscall WONAPI::WONAPICoreEx::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006f28a0
// (public: virtual void * __thiscall WONAPI::WONAPICore::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x006f28d0
// (protected: __thiscall WONAPI::WONAPICore::WONAPICore(char const *))
