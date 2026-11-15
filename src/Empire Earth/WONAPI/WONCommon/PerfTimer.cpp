// PerfTimer: global performance timing on the high-resolution counter (the original: rdtsc, calibrated against it)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include "ee/wonapi/WONPerfTimer.h"
#include "platform/PlatTime.h"
using namespace WONAPI;
// The original's state: RVA 565ec0/8, 565ed0/8, 565ed8/8, 565ee0/4, 565ee4/1

static __int64 gPerfTotalTime = 0;
static __int64 gPerfResetTick = 0;
static double gPerfElapsedTime = 0;
static int gPerfTimerStartCount = 0;
static bool gPerfTimerStarted = false;
// 0x6a37f0 (emitted from the list below): reads the CPU time-stamp counter (rdtsc in inline asm); now the
// platform's high-resolution counter
static __int64 MyQueryPerformanceCounter()
{
    return PlatCounterTicks();
}
// Returns the counter's rate per second (the original measured rdtsc over a 50 ms sleep), cached
static __int64 MyQueryPerformanceFrequency()
{
    static __int64 aFreq=0; //cached frequency, RVA 565ee8/8
    if(aFreq!=0) return aFreq;
    aFreq=PlatCounterFrequency();
    return aFreq;
}
// Returns the time counted by the timers in milliseconds (raw ticks when the rate is unknown)
double PerfTimer::GetCountedTime(const char*)
{
    __int64 frequency=MyQueryPerformanceFrequency();
    if(frequency==0) return gPerfTotalTime;
    return (double)gPerfTotalTime/frequency*1000.0;
}
// Returns the milliseconds between the last global start and stop
double PerfTimer::GetElapsedTime(const char*) {return gPerfElapsedTime;}
// Returns the counted time as a fraction of the elapsed time, 0 when nothing elapsed
double PerfTimer::GetPercentTime(const char*)
{
    if(gPerfElapsedTime==0) return 0;
    return PerfTimer::GetCountedTime()/gPerfElapsedTime;
}
// Starts a global timing run, clearing the counted time and noting the start tick
void PerfTimer::GlobalStart(const char*)
{
    gPerfTimerStarted=true;
    gPerfTotalTime=0;
    gPerfTimerStartCount=0;
    gPerfElapsedTime=0;
    gPerfResetTick=PlatCounterTicks();
}
// Ends the global timing run and records its elapsed milliseconds
void PerfTimer::GlobalStop(const char*)
{
    int64_t tick=PlatCounterTicks();
    int64_t frequency=PlatCounterFrequency();
    gPerfElapsedTime=(double)(tick-gPerfResetTick)/frequency*1000.0;
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
