// Golden driver for the Time Manager (TMPoll, TMTimer, TMGetTime, TMGetMicroTime, TMGetTimeOfDay, TMInitialize):
// random call sequences over the original units (tm_orig.h) on a scripted platform clock and timer table. The Rust
// replay (rust/crates/ee_time/src/golden.rs) reads the input lines, makes the same calls on the Rust objects and
// regenerates every line; the whole text must be equal.
//
//   python -I build/lle3/cc.py build/lle3/tmg.exe tests/time/tm_golden.cpp -Itests/time
//   build/lle3/tmg.exe <sequences> [first seed] > golden.txt
//
// Input lines (the replay acts on these):
//   seq <n>                                   fresh polls, timer, time base and pin
//   clock <frequency> <ticks> <ms>            the platform's counter frequency, counter and millisecond clock
//   local <year> <month> <dow> <day> <hour> <minute> <second> <ms>   the platform's local time
//   tid <id> <stop ok>                        the next timer id the platform hands out, whether a stop succeeds
//   init | time | micro | tod | pin <v>       TMInitialize, TMGetTime, TMGetMicroTime, TMGetTimeOfDay, the pin
//   new <i> <delay> <unit> <start>            poll i = TMPoll(delay, unit, start)
//   start|stop|pause|resume|poll|left|paused|running <i>, setd <i> <delay>
//   tnew <interval> <unit> <repeating>        the timer = TMTimer(...)
//   tcb <callback 0-2> <param> | tev <handle> | tint <interval> <unit> | trep <0/1> | ttype <type>
//   tstart | tstop | tstop2 | tfire           tfire: the platform calls the running callback timer's procedure
//   end                                       the sequence's objects destroyed
// Output lines: "= ..." results, "P <i> ..." a poll's state and the shared frequency, "T ..." the timer's state,
// "plat ..." the platform calls made, "cb <callback> <param>" a user callback.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LLE_API
enum { kErrorOK = 0, kErrorNotFound = 3, kErrorBusy = 5 };
struct PlatEvent;
struct PlatDateTime {
    uint16_t mYear, mMonth, mDayOfWeek, mDay, mHour, mMinute, mSecond, mMilliseconds;
};
struct TMTime {
    unsigned long mMonth, mDay, mYear, mHour, mMinute, mSecond;
};
class TSEvent {
public:
    PlatEvent* mHandle;
};

static int64_t sFrequency, sTicks;
static uint32_t sMs, sNextId;
static int sStopOk;
static PlatDateTime sLocal;
static int64_t PlatCounterFrequency() { return sFrequency; }
static int64_t PlatCounterTicks() { return sTicks; }
static uint32_t PlatTimeMilliseconds() { return sMs; }
static void PlatTimeBeginPeriod(uint32_t inMs) { printf("plat begin %u\n", inMs); }
static void PlatTimeEndPeriod(uint32_t inMs) { printf("plat end %u\n", inMs); }
static void PlatGetLocalTime(PlatDateTime* outTime) { *outTime = sLocal; }
static void (*sTimerProc)(void*);
static void* sTimerUser;
static uint32_t PlatTimerStartCallback(uint32_t inInterval, int inPeriodic, void (*inProc)(void*), void* inUser)
{
    printf("plat startc %u %d\n", inInterval, inPeriodic);
    sTimerProc = inProc;
    sTimerUser = inUser;
    return sNextId;
}
static uint32_t PlatTimerStartEvent(uint32_t inInterval, int inPeriodic, PlatEvent* inEvent)
{
    printf("plat starte %u %d %llu\n", inInterval, inPeriodic, (unsigned long long)(uintptr_t)inEvent);
    return sNextId;
}
static int PlatTimerStop(uint32_t inId)
{
    printf("plat stop %u\n", inId);
    return sStopOk;
}
static long URegisterShutdown(void (*)(bool)) { return kErrorOK; }

#define private public
#include "tm_orig.h"
#undef private

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }
static uint64_t Next64() { return ((uint64_t)Next() << 32) | Next(); }

static void CallbackA(void* inParam) { printf("cb 1 %llu\n", (unsigned long long)(uintptr_t)inParam); }
static void CallbackB(void* inParam) { printf("cb 2 %llu\n", (unsigned long long)(uintptr_t)inParam); }

static const int kPolls = 3;
static TMPoll* sPolls[kPolls];
static TMTimer* sTimer;
static TSEvent sEvents[4];

static void DumpPoll(int i)
{
    TMPoll* p = sPolls[i];
    printf("P %d %lld %lld %lu %lu %d %d %lld\n", i, (long long)p->mStartTime, (long long)p->mPauseRemaining,
           p->mTimeUnit, p->mTimeDelay, p->mUseCounter, p->mRunning, (long long)TMPoll::sCounterFrequency);
}
static void DumpTimer()
{
    TMTimer* t = sTimer;
    int theCallback = t->mCallback == CallbackA ? 1 : t->mCallback == CallbackB ? 2 : 0;
    printf("T %d %llu %llu %lu %lu %lu %lu %lu %d %d\n", theCallback, (unsigned long long)(uintptr_t)t->mParameter,
           (unsigned long long)(uintptr_t)(t->mEvent ? t->mEvent->mHandle : NULL), t->mTimerType, t->mUnknown10,
           t->mInterval, t->mStartTime, t->mTimerID, t->mRepeating, t->mRunning);
}
static void Clock()
{
    printf("clock %lld %lld %u\n", (long long)sFrequency, (long long)sTicks, sMs);
}
static int64_t PickFrequency()
{
    switch (Below(6)) {
    case 0: return 0;
    case 1: return 1000000 + Below(9000000);
    case 2: return 3579545;
    case 3: return 10000000;
    case 4: return 1000000000;
    default: return 2400000000LL + Below(1000000000);
    }
}
static unsigned long PickDelay()
{
    switch (Below(8)) {
    case 0: return 1 + Below(3);
    case 1: return 0xffffffffu - Below(3);
    case 2: return 1 + Next();
    case 3: return 1000 + Below(100000);
    default: return 1 + Below(3000);
    }
}
static void Advance()
{
    uint64_t theStep;
    switch (Below(6)) {
    case 0: theStep = 0; break;
    case 1: theStep = Below(5); break;
    case 2: theStep = Below(5000); break;
    case 3: theStep = Below(100000); break;
    case 4: theStep = Next(); break;
    default: theStep = Below(3000); break;
    }
    // the counter advances by the same milliseconds (scaled), the millisecond clock by its own amount
    int64_t thePerMs = sFrequency ? sFrequency / 1000 : 1000;
    sTicks += (int64_t)(theStep % 100000000ull) * thePerMs + Below(1000);
    sMs += (uint32_t)theStep + Below(2);
}

static void Sequence(int n)
{
    printf("seq %d\n", n);
    TMPoll::sCounterFrequency = 0;
    gCounterFrequency = 0;
    gMicrosecondFrequency = 0;
    gHasPerformanceCounter = false;
    gTMPinnedTime = -1;
    sFrequency = PickFrequency();
    sTicks = (int64_t)(Next64() >> Below(64)) & 0x3fffffffffffffffLL;
    sMs = Below(4) == 0 ? 0xffffffffu - Below(100000) : Next();
    Clock();
    sNextId = 1 + Below(100);
    sStopOk = 1;
    printf("tid %u %d\n", sNextId, sStopOk);
    int theCount = 30 + Below(90);
    for (int k = 0; k < theCount; k++) {
        int theOp = Below(40);
        int i = Below(kPolls);
        if (theOp < 6) {
            Advance();
            if (Below(12) == 0) {
                // a new frequency (a poll made later sets the shared one); never one a microsecond poll divides to 0
                sFrequency = PickFrequency();
            }
            Clock();
        } else if (theOp < 9) {
            unsigned long theDelay = PickDelay();
            unsigned long theUnit = Below(4) == 0;
            bool theStart = Below(2);
            if (theUnit && sFrequency == 0 && Below(2))
                theDelay = Below(3000);   // a microsecond delay without a counter may round to 0 (made 1)
            printf("new %d %lu %lu %d\n", i, theDelay, theUnit, theStart);
            delete sPolls[i];
            sPolls[i] = new TMPoll(theDelay, theUnit, theStart);
            DumpPoll(i);
        } else if (theOp < 23) {
            if (!sPolls[i])
                continue;
            static const char* kNames[] = {"start", "stop", "pause", "resume", "poll", "left", "paused", "running", "setd"};
            int theCall = Below(13);
            if (theCall >= 9)
                theCall = 4 + (theCall & 1);   // more polls and time-left reads
            if (theCall == 8) {
                unsigned long theDelay = Below(5) == 0 ? 0 : PickDelay();
                printf("setd %d %lu\n", i, theDelay);
                sPolls[i]->SetTimeDelay(theDelay);
            } else {
                printf("%s %d\n", kNames[theCall], i);
                switch (theCall) {
                case 0: sPolls[i]->Start(); break;
                case 1: sPolls[i]->Stop(); break;
                case 2: sPolls[i]->Pause(); break;
                case 3: sPolls[i]->Resume(); break;
                case 4: printf("= %lu\n", sPolls[i]->Poll()); break;
                case 5: printf("= %lu\n", sPolls[i]->GetTimeLeft()); break;
                case 6: printf("= %d\n", sPolls[i]->GetIsPaused()); break;
                case 7: printf("= %d\n", sPolls[i]->GetIsRunning()); break;
                }
            }
            DumpPoll(i);
        } else if (theOp < 27) {
            switch (Below(6)) {
            case 0:
                printf("init\n");
                TMInitialize();
                printf("= %lld %08x %d\n", (long long)gCounterFrequency, *(uint32_t*)&gMicrosecondFrequency,
                       gHasPerformanceCounter);
                break;
            case 1:
                printf("time\n= %lu\n", TMGetTime());
                break;
            case 2:
                printf("micro\n= %lld\n", (long long)TMGetMicroTime());
                break;
            case 3: {
                long long thePin = Below(3) == 0 ? -1 : (long long)Below(4) == 0 ? (long long)Next() : Below(100000);
                printf("pin %lld\n", thePin);
                gTMPinnedTime = thePin;
                break;
            }
            case 4: {
                uint16_t* theFields = &sLocal.mYear;
                for (int f = 0; f < 8; f++)
                    theFields[f] = (uint16_t)(Below(3) == 0 ? Next() : Below(60) + (f == 1 && Below(6)));
                printf("local %u %u %u %u %u %u %u %u\n", sLocal.mYear, sLocal.mMonth, sLocal.mDayOfWeek, sLocal.mDay,
                       sLocal.mHour, sLocal.mMinute, sLocal.mSecond, sLocal.mMilliseconds);
                TMTime theTime;
                memset(&theTime, 0xcc, sizeof(theTime));
                printf("tod\n");
                TMGetTimeOfDay(theTime);
                printf("= %lu %lu %lu %lu %lu %lu\n", theTime.mMonth, theTime.mDay, theTime.mYear, theTime.mHour,
                       theTime.mMinute, theTime.mSecond);
                break;
            }
            default:
                sNextId = Below(4) == 0 ? 0 : 1 + Below(1000);
                sStopOk = Below(4) != 0;
                printf("tid %u %d\n", sNextId, sStopOk);
                break;
            }
        } else {
            if (!sTimer || Below(30) == 0) {
                unsigned long theInterval = Below(3) == 0 ? Next() : Below(5000);
                unsigned long theUnit = Below(3) == 0;
                bool theRepeating = Below(2);
                printf("tnew %lu %lu %d\n", theInterval, theUnit, theRepeating);
                delete sTimer;
                sTimer = new TMTimer(theInterval, theUnit, theRepeating);
                DumpTimer();
                continue;
            }
            switch (Below(11)) {
            case 0: {
                int theCallback = Below(3);
                unsigned long long theParam = Below(4) == 0 ? 0 : Below(100000);
                printf("tcb %d %llu\n", theCallback, theParam);
                printf("= %ld\n", sTimer->SetCallback(theCallback == 1 ? CallbackA : theCallback == 2 ? CallbackB : NULL,
                                                      (void*)(uintptr_t)theParam));
                break;
            }
            case 1: {
                int theEvent = Below(4);
                printf("tev %d\n", theEvent ? theEvent * 16 : 0);
                printf("= %ld\n", sTimer->SetEvent(theEvent ? &sEvents[theEvent] : NULL));
                break;
            }
            case 2: {
                unsigned long theInterval = Below(3) == 0 ? Next() : Below(5000);
                unsigned long theUnit = Below(3) == 0;
                printf("tint %lu %lu\n", theInterval, theUnit);
                printf("= %ld\n", sTimer->SetInterval(theInterval, theUnit));
                break;
            }
            case 3: {
                bool theRepeating = Below(2);
                printf("trep %d\n", theRepeating);
                printf("= %ld\n", sTimer->SetRepeating(theRepeating));
                break;
            }
            case 4: {
                unsigned long theType = Below(3) == 0 ? Below(5) : 0;
                printf("ttype %lu\n", theType);
                printf("= %ld\n", sTimer->SetTimerType(theType));
                break;
            }
            case 5:
            case 6:
                // the original reads the event's handle at Start: never with no event (it crashed)
                if (sTimer->mTimerType != 0 && sTimer->mEvent == NULL)
                    continue;
                printf("tstart\n");
                printf("= %ld\n", sTimer->Start());
                break;
            case 7:
                printf("tstop\n");
                printf("= %ld\n", sTimer->Stop());
                break;
            case 8: {
                printf("tstop2\n");
                unsigned long theElapsed = 7, theRemaining = 9;
                long theResult = sTimer->Stop(theElapsed, theRemaining);
                printf("= %ld %lu %lu\n", theResult, theElapsed, theRemaining);
                break;
            }
            default:
                if (!sTimer->mRunning || sTimer->mTimerType != 0)
                    continue;
                printf("tfire\n");
                sTimerProc(sTimerUser);
                break;
            }
            DumpTimer();
        }
    }
    // the destructors (a running timer is stopped)
    printf("end\n");
    for (int i = 0; i < kPolls; i++) {
        delete sPolls[i];
        sPolls[i] = NULL;
    }
    delete sTimer;
    sTimer = NULL;
}

int main(int argc, char** argv)
{
    for (int i = 1; i < 4; i++)
        sEvents[i].mHandle = (PlatEvent*)(uintptr_t)(i * 16);
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    for (int n = theFirst; n < theFirst + theCount; n++) {
        sRandom = 0x9e3779b9u ^ (uint32_t)n * 2654435761u;
        if (sRandom == 0)
            sRandom = 1;
        Sequence(n);
    }
    return 0;
}
