// WONAPICore as a Rust object (R6 WONLobby round 12: ee_won apicore.rs; WONAPI.cpp deleted) against the original:
// WONAPI.cpp's and WONAPICore.h's statements copied below as OrigCore / OrigCoreEx (over the same C++ op container,
// timer thread and pump thread shims the unit used), against the shim WONAPICoreEx over the Rust core.
//
// Each of 3,000 sequences makes a core and applies 1 to 16 random calls: the settings (SetDoPumpThread,
// SetDoSocketAndTimerThreads, SetSocketThread with a tracing stand-in or NULL, SetDefaultFileDirectory), Startup,
// Shutdown, Pump, the static calls (QueueOp, AddTimerOp, RemoveTimerOp, AddToSocketThread, RemoveFromSocketThread on
// never-run ops), IsStarted, GetInstance, GetDefaultFileDirectory; then deletes it. The stand-in socket thread logs
// every virtual call it gets, whether its thread runs and its single-threaded flag at each pump; each op's status is
// logged after the call. The first sequence sets the directory to a scratch one holding an old _wonapiversion.txt
// and starts: the file's content after that start-up is logged (WriteVersion, once per run). The two halves' lines
// must be the same. Most sequences run without the socket and timer threads (their timing is not traced); with
// them, a stand-in's thread only waits for its stop. `--mutate` prints the count of differing sequences.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
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
#include "platform/PlatFile.h"
#include "platform/PlatPath.h"

using namespace WONAPI;

static std::vector<std::string> sLines;
static std::string N(long long v) { return std::to_string(v); }
static bool sEcho;
static DWORD sMainThread;
// (a call on another thread, the pump thread's, is not logged: its count depends on timing)
static void Log(const std::string& inLine) { if (GetCurrentThreadId() != sMainThread) return; sLines.push_back(inLine); if (sEcho) { fprintf(stderr, "%s\n", inLine.c_str()); fflush(stderr); } }
static unsigned sSeed;
static unsigned R(unsigned n) { sSeed = sSeed * 1103515245u + 12345u; return ((sSeed >> 16) & 0x7fff) % n; }

// ---- the tracing stand-in socket thread

static int sNextThread;
class TraceThread : public SocketThread {
public:
    TraceThread(bool inNeed) : mId(++sNextThread), mNeed(inNeed) { Log("thread t" + N(mId) + " made"); }
    virtual ~TraceThread() { Log("t" + N(mId) + " deleted running=" + N(this->Running())); }
    virtual void ThreadFunc() { while (!this->IsStopped()) this->WaitForSignal(1000); }
    virtual void AddSocketOp(SocketOp*) { Log("t" + N(mId) + " add"); }
    virtual void RemoveSocketOp(SocketOp*) { Log("t" + N(mId) + " remove"); }
    virtual void Pump(unsigned long inWait) { Log("t" + N(mId) + " pump " + N(inWait > 0) + " single=" + N(this->mSingleThreaded)); }
    virtual void PurgeOps() { Log("t" + N(mId) + " purge running=" + N(this->Running())); }
    virtual bool NeedThread() { Log("t" + N(mId) + " need " + N(mNeed)); return mNeed; }
    bool Running() { return ee_won_thread_is_running(this->mThread); }
    int mId;
    bool mNeed;
};

// ---- the original: WONAPI.cpp and WONAPICore.h as they were (the class names OrigCore, OrigCoreEx)

class OrigCore {
public:
    static void QueueOp(AsyncOp*);
    static void AddTimerOp(AsyncOp*,unsigned long);
    static void RemoveTimerOp(AsyncOp*);
    static const std::string& GetDefaultFileDirectory();
    virtual ~OrigCore();
    virtual void Startup();
    virtual void Shutdown();
    void Pump(unsigned long,int=0);
    static OrigCore* GetInstance() { return OrigCore::mInstance; }
    static bool IsStarted();
    static void AddToSocketThread(SocketOp*);
    static void RemoveFromSocketThread(SocketOp*);
protected:
    OrigCore(const char*);
    void Init();
    void WriteVersion();
    static OrigCore* mInstance;
    AsyncOpContainer mOpContainer;
    TimerThread mTimerThread;
    SocketThread* mSocketThread;
    bool mIsStarted;
    bool mDoSocketAndTimerThreads;
    bool mDoPumpThread;
    static bool mHaveWrittenVersion;
    std::string mVersionString;
    std::string mFileDirectory;
    class PumpThread : public Thread {
        virtual void ThreadFunc();
    };
    PumpThread mPumpThread;
};
class OrigCoreEx : public OrigCore {
public:
    OrigCoreEx();
    void SetDefaultFileDirectory(const char*);
    void SetSocketThread(SocketThread*);
    void SetDoSocketAndTimerThreads(bool);
    void SetDoPumpThread(bool);
};
OrigCore* OrigCore::mInstance;
bool OrigCore::mHaveWrittenVersion;
void OrigCore::Init()
{
    this->mVersionString="wonapi121401";
    OrigCore::mInstance=this;
    this->mIsStarted=false;
    this->mDoPumpThread=false;
    this->mDoSocketAndTimerThreads=true;
    this->mSocketThread=NULL;
    ee_won_api_set_file_directory(NULL,0);
    AsyncSocket::StartWinsock();
}
OrigCore::OrigCore(const char*) { this->Init(); }
OrigCore::~OrigCore()
{
    this->Shutdown();
    delete this->mSocketThread;
    AsyncSocket::StopWinsock();
    OrigCore::mInstance=NULL;
    ee_won_api_set_file_directory(NULL,0);
}
void OrigCore::WriteVersion()
{
    if(OrigCore::mHaveWrittenVersion) return;
    OrigCore::mHaveWrittenVersion=true;
    std::string path=OrigCore::GetDefaultFileDirectory()+"_wonapiversion.txt";
    FILE* file=PlatFOpen(path.c_str(),"r");
    if(file!=NULL) {
        fclose(file);
        file=PlatFOpen(path.c_str(),"w");
        if(file!=NULL) {
            fprintf(file,"%s\n",this->mVersionString.c_str());
            fclose(file);
        }
    }
}
void OrigCore::Startup()
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
void OrigCore::Shutdown()
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
bool OrigCore::IsStarted() { return OrigCore::mInstance?OrigCore::mInstance->mIsStarted:false; }
void OrigCore::Pump(unsigned long milliseconds,int maxOps)
{
    DWORD start=WONTickCount();
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
        elapsed=WONTickCount()-start;
        if(elapsed>=milliseconds) break;
    }
}
const std::string& OrigCore::GetDefaultFileDirectory()
{
    static std::string aDataRoot;
    if (OrigCore::mInstance!=NULL && !OrigCore::mInstance->mFileDirectory.empty())
        return OrigCore::mInstance->mFileDirectory;
    if (aDataRoot.empty()) {
        char aPath[1024];
        if (PlatPathGetDataRoot(aPath,sizeof(aPath))!=0)
            aDataRoot=aPath;
    }
    return aDataRoot;
}
void OrigCore::QueueOp(AsyncOp* op)
{
    if(OrigCore::IsStarted()) OrigCore::mInstance->mOpContainer.QueueOp(op);
    else if(OrigCore::mInstance!=NULL) {
        if(op->GetStatus()!=WS_Pending) OrigCore::mInstance->mOpContainer.QueueOp(op);
    }
}
void OrigCore::AddTimerOp(AsyncOp* op,unsigned long timeout)
{
    if(OrigCore::IsStarted()) OrigCore::mInstance->mTimerThread.AddTimerOp(op,timeout);
    else op->Kill(true);
}
void OrigCore::RemoveTimerOp(AsyncOp* op)
{
    if(OrigCore::IsStarted()) OrigCore::mInstance->mTimerThread.RemoveTimerOp(op);
}
void OrigCore::AddToSocketThread(SocketOp* op)
{
    if(OrigCore::IsStarted()) OrigCore::mInstance->mSocketThread->AddSocketOp(op);
    else op->Kill(true);
}
void OrigCore::RemoveFromSocketThread(SocketOp* op)
{
    if(OrigCore::IsStarted()) OrigCore::mInstance->mSocketThread->RemoveSocketOp(op);
}
OrigCoreEx::OrigCoreEx() : OrigCore("") {}
void OrigCoreEx::SetDoPumpThread(bool value)
{
    if(!this->mIsStarted) this->mDoPumpThread=value;
}
void OrigCore::PumpThread::ThreadFunc()
{
    OrigCore* api=OrigCore::GetInstance();
    while(true) {
        api->Pump(20);
        if(this->IsStopped()) return;
    }
}
void OrigCoreEx::SetDefaultFileDirectory(const char* theDirectory)
{
	if(this->mIsStarted)
		return;
	this->mFileDirectory = theDirectory;
	if(this->mFileDirectory.empty()) {
		ee_won_api_set_file_directory(NULL,0);
		return;
	}
	char anEndChar = this->mFileDirectory[this->mFileDirectory.length()-1];
	if(anEndChar!='\\' && anEndChar!='/')
		this->mFileDirectory += '/';
	ee_won_api_set_file_directory(reinterpret_cast<const uint8_t*>(this->mFileDirectory.data()),this->mFileDirectory.size());
}
void OrigCoreEx::SetSocketThread(SocketThread *theThread)
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
void OrigCoreEx::SetDoSocketAndTimerThreads(bool doSocketAndTimerThreads)
{
	if(!this->mIsStarted)
		this->mDoSocketAndTimerThreads = doSocketAndTimerThreads;
}

// ---- the port: the shim (its protected statics reached through a subclass)

class PortCore : public WONAPICoreEx {
public:
    static void AddSock(SocketOp* inOp) { WONAPICore::AddToSocketThread(inOp); }
    static void RemoveSock(SocketOp* inOp) { WONAPICore::RemoveFromSocketThread(inOp); }
};

// ---- one sequence over either

static std::string sScratch;
static std::string ReadFile(const std::string& inPath)
{
    FILE* f = PlatFOpen(inPath.c_str(), "rb");
    if (f == NULL) return "<none>";
    char aBuffer[256];
    size_t n = fread(aBuffer, 1, sizeof(aBuffer), f);
    fclose(f);
    std::string s;
    for (size_t i = 0; i < n; i++) s += aBuffer[i] == '\r' ? std::string("\\r") : aBuffer[i] == '\n' ? std::string("\\n") : std::string(1, aBuffer[i]);
    return s;
}
static std::string DirName(unsigned k) { return k == 0 ? std::string() : k == 1 ? sScratch : sScratch + "sub/"; }
static std::string Rel(const std::string& inDir)
{
    return inDir.compare(0, sScratch.size(), sScratch) == 0 && !sScratch.empty() ? "S:" + inDir.substr(sScratch.size()) : "root";
}

template <class Core, class Ex> struct Ops;
template <> struct Ops<OrigCore, OrigCoreEx> {
    static void AddSock(SocketOp* o) { OrigCore::AddToSocketThread(o); }
    static void RemoveSock(SocketOp* o) { OrigCore::RemoveFromSocketThread(o); }
};
template <> struct Ops<WONAPICore, WONAPICoreEx> {
    static void AddSock(SocketOp* o) { PortCore::AddSock(o); }
    static void RemoveSock(SocketOp* o) { PortCore::RemoveSock(o); }
};

template <class Core, class Ex> static void Sequence(int inSeq)
{
    sSeed = 0xa91c0000u + (unsigned)inSeq * 2654435761u;
    sNextThread = 0;
    Log("seq " + N(inSeq));
    Ex* c = new Ex;
    Log("instance " + N(Core::GetInstance() == static_cast<Core*>(c)) + " started " + N(Core::IsStarted()));
    std::vector<SmartPtr<AsyncOp> > anOps;
    // threads on in a quarter of the sequences; the pump thread in a sixth, with no ops (its pumps race the log)
    bool aThreads = !(inSeq > 0 && R(4) != 0), aStarted = false, aHasThread = false;
    if (!aThreads) c->SetDoSocketAndTimerThreads(false);
    bool aPumpSeq = inSeq > 0 && R(6) == 0;
    unsigned aCalls = inSeq == 0 ? 0 : 1 + R(16);
    if (inSeq == 0) {
        std::string aPath = sScratch + "_wonapiversion.txt";
        FILE* f = PlatFOpen(aPath.c_str(), "w");
        if (f != NULL) { fputs("old\n", f); fclose(f); }
        c->SetDefaultFileDirectory(sScratch.c_str());
        c->SetDoSocketAndTimerThreads(false);
        c->SetSocketThread(new TraceThread(false));
        c->Startup();
        Log("version file " + ReadFile(aPath));
    }
    for (unsigned k = 0; k < aCalls; k++) {
        unsigned w = R(15);
        if (sEcho) { fprintf(stderr, "  call %u\n", w); fflush(stderr); }
        std::string aLine;
        switch (w) {
        case 0: { bool b = aPumpSeq && R(2) != 0; c->SetDoPumpThread(b); aLine = "pump thread " + N(b); break; }
        case 1: {
            bool b = R(2) != 0;
            c->SetDoSocketAndTimerThreads(b);
            if (!aStarted) aThreads = b;
            aLine = "threads " + N(b);
            break;
        }
        case 2: {
            SocketThread* t = R(4) == 0 ? NULL : new TraceThread(R(2) != 0);
            c->SetSocketThread(t);
            if (!aStarted) aHasThread = t != NULL;
            aLine = "set thread";
            break;
        }
        case 3: { std::string d = DirName(R(3)); c->SetDefaultFileDirectory(d.c_str()); aLine = "dir " + Rel(d); break; }
        case 4: case 5: c->Startup(); aStarted = aHasThread = true; aLine = "startup"; break;
        case 6: c->Shutdown(); aStarted = false; aLine = "shutdown"; break;
        case 7: {
            // (single-threaded without a socket thread, the original's Pump faulted: no call makes it so)
            // (0 ms: one pass; a longer pump passes as often as the clock allows)
            unsigned ms = R(3) * 0;
            if (aThreads || aHasThread) c->Pump(ms, 0);
            aLine = "pump";
            break;
        }
        case 8: if (aPumpSeq) { aLine = "started " + N(Core::IsStarted()); break; } {
            SmartPtr<AsyncOp> o = new AsyncOp;
            anOps.push_back(o);
            Core::QueueOp(o.get());
            aLine = "queue " + N(o->GetStatus());
            break;
        }
        case 9: if (aPumpSeq) { aLine = "started " + N(Core::IsStarted()); break; } {
            SmartPtr<AsyncOp> o = new AsyncOp;
            anOps.push_back(o);
            // (with the timer thread a due op would time out while this logs: none is due then)
            Core::AddTimerOp(o.get(), !aThreads && R(2) ? 0 : 100000);
            aLine = "timer " + N(o->GetStatus());
            if (R(2)) { Core::RemoveTimerOp(o.get()); aLine += " removed " + N(o->GetStatus()); }
            break;
        }
        case 10: if (aPumpSeq || aThreads) { aLine = "started " + N(Core::IsStarted()); break; } {
            // (a socket thread on its own thread would continue the op while this logs)
            SmartPtr<SocketOp> o = new SocketOp(NULL);
            anOps.push_back(o.get());
            Ops<Core, Ex>::AddSock(o.get());
            aLine = "socket op " + N(o->GetStatus());
            if (R(2)) { Ops<Core, Ex>::RemoveSock(o.get()); aLine += " removed"; }
            break;
        }
        case 11: aLine = "dir is " + Rel(Core::GetDefaultFileDirectory()); break;
        default: aLine = "started " + N(Core::IsStarted()) + " instance " + N(Core::GetInstance() == static_cast<Core*>(c)); break;
        }
        Log(aLine);
    }
    delete c;
    std::string aStatuses = "ops";
    for (size_t i = 0; i < anOps.size(); i++) aStatuses += " " + N(anOps[i]->GetStatus());
    Log(aStatuses);
    Log("after instance " + N(Core::GetInstance() != NULL) + " started " + N(Core::IsStarted()) + " dir " +
        Rel(Core::GetDefaultFileDirectory()));
}

int main(int argc, char** argv)
{
    bool aMutate = argc > 1 && strcmp(argv[1], "--mutate") == 0;
    sEcho = argc > 1 && strcmp(argv[1], "--echo") == 0;
    sMainThread = GetCurrentThreadId();
    // the scratch directory, under the working directory
    sScratch = "won_apicore_scratch/";
    PlatCreateDirectory("won_apicore_scratch");
    PlatCreateDirectory("won_apicore_scratch/sub");
    const int kSequences = 3000;
    int aFailures = 0, aDiffering = 0;
    size_t aLines = 0;
    for (int s = 0; s < kSequences; s++) {
        sLines.clear();
        Sequence<OrigCore, OrigCoreEx>(s);
        std::vector<std::string> aWant = sLines;
        sLines.clear();
        Sequence<WONAPICore, WONAPICoreEx>(s);
        aLines += aWant.size();
        if (aWant != sLines) {
            aDiffering++;
            if (!aMutate && ++aFailures < 6) {
                printf("FAIL sequence %d\n", s);
                for (size_t i = 0; i < aWant.size() || i < sLines.size(); i++) {
                    std::string a = i < aWant.size() ? aWant[i] : "<none>", b = i < sLines.size() ? sLines[i] : "<none>";
                    if (a != b) { printf(" line %u\n  want %s\n   got %s\n", (unsigned)i, a.c_str(), b.c_str()); break; }
                }
            }
        }
        if (s == 0) for (size_t i = 0; i < aWant.size(); i++) if (aWant[i].compare(0, 12, "version file") == 0) printf("%s\n", aWant[i].c_str());
    }
    if (aMutate) { printf("won_apicore_test --mutate: %d of %d sequences differ\n", aDiffering, kSequences); return 0; }
    printf("won_apicore_test: %d sequences, %u lines, %d failures\n", kSequences, (unsigned)aLines, aFailures);
    return aFailures;
}
