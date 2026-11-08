// PerfTimer: global performance timing with rdtsc calibration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include "ee/wonapi/WONPerfTimer.h"
using namespace WONAPI;
// The original's state: RVA 565ec0/8, 565ed0/8, 565ed8/8, 565ee0/4, 565ee4/1

static __int64 gPerfTotalTime = 0;
static __int64 gPerfResetTick = 0;
static double gPerfElapsedTime = 0;
static int gPerfTimerStartCount = 0;
static bool gPerfTimerStarted = false;
// 0x6a37f0 (emitted from the list below): reads the CPU time-stamp counter
// The original SDK wrote this rdtsc read in inline asm
static __int64 MyQueryPerformanceCounter()
{
    unsigned long x,y;
    __asm {
        rdtsc
        mov x,eax
        mov y,edx
    }
    __int64 result=y;
    result<<=32;
    result|=x;
    return result;
}
// FUNCTION: WON 0x006a3850
// Returns the time-stamp counter rate per second, measured once over a 50 ms sleep and cached
static __int64 MyQueryPerformanceFrequency()
{
    static __int64 aFreq=0; //cached frequency, RVA 565ee8/8
    if(aFreq!=0) return aFreq;
    LARGE_INTEGER s1,e1,f1;
    __int64 s2,e2,f2;
    QueryPerformanceCounter(&s1);
    s2=MyQueryPerformanceCounter();
    Sleep(50);
    e2=MyQueryPerformanceCounter();
    QueryPerformanceCounter(&e1);
    QueryPerformanceFrequency(&f1);
    double time=(double)(e1.QuadPart-s1.QuadPart)/f1.QuadPart;
    f2=(e2-s2)/time;
    aFreq=f2;
    return aFreq;
}
// FUNCTION: WON 0x006a3820
// Returns the time counted by the timers in milliseconds (raw ticks when the rate is unknown)
double PerfTimer::GetCountedTime(const char*)
{
    __int64 frequency=MyQueryPerformanceFrequency();
    if(frequency==0) return gPerfTotalTime;
    return (double)gPerfTotalTime/frequency*1000.0;
}
// FUNCTION: WON 0x006a3900
// Returns the milliseconds between the last global start and stop
double PerfTimer::GetElapsedTime(const char*) {return gPerfElapsedTime;}
// FUNCTION: WON 0x006a3910
// Returns the counted time as a fraction of the elapsed time, 0 when nothing elapsed
double PerfTimer::GetPercentTime(const char*)
{
    if(gPerfElapsedTime==0) return 0;
    return PerfTimer::GetCountedTime()/gPerfElapsedTime;
}
// FUNCTION: WON 0x006a3940
// Starts a global timing run, clearing the counted time and noting the start tick
void PerfTimer::GlobalStart(const char*)
{
    gPerfTimerStarted=true;
    gPerfTotalTime=0;
    gPerfTimerStartCount=0;
    gPerfElapsedTime=0;
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);
    gPerfResetTick=tick.QuadPart;
}
// FUNCTION: WON 0x006a3990
// Ends the global timing run and records its elapsed milliseconds
void PerfTimer::GlobalStop(const char*)
{
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    gPerfElapsedTime=(double)(tick.QuadPart-gPerfResetTick)/frequency.QuadPart*1000.0;
    gPerfTimerStarted=false;
}

// No original address identified: WON SDK PerfTimer instance methods
static __int64 gPerfStartTick = 0; // start tick of the outermost running timer

PerfTimer::PerfTimer(const char *theName)
{
	this->mName = theName;
	this->mStarted = false;
	this->Start();
}

PerfTimer::~PerfTimer()
{
	this->Stop();
}

void PerfTimer::Start()
{
	if(this->mName!=NULL)
		return;

	if(this->mStarted)
		return;
	this->mStarted = true;

	gPerfTimerStartCount++;
	if(gPerfTimerStartCount==1)
		gPerfStartTick = MyQueryPerformanceCounter();
}

void PerfTimer::Stop()
{
	if(this->mName!=NULL)
		return;

	if(!this->mStarted)
		return;
	gPerfTimerStartCount--;
	if(gPerfTimerStartCount<=0 && gPerfTimerStarted)
	{
		gPerfTotalTime += MyQueryPerformanceCounter() - gPerfStartTick;
		gPerfTimerStartCount = 0;
	}
	else
	{
		int x = 0;
		x++;
	}
	this->mStarted = false;
}

void PerfTimer::DumpTimesToStr(std::string &theStr)
{
}

void PerfTimer::DumpTimesToFile(const char *theFileName)
{
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a37f0
// (__int64 __cdecl MyQueryPerformanceCounter(void)    [?MyQueryPerformanceCounter@@YA_JXZ])
