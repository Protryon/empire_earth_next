// C:\EEX\Low-Level Engine\Time Manager\TMTimer.h (folder name inferred; RECONSTRUCTION from TMTimer.cpp
// 0x53ad3..0x53ca1; sizeof 0x24 from the implicit operator= `rep movsd` x9). A system-timer wrapper (platform/PlatTime.h:
// the multimedia timers on Win32).
#ifndef LLE_TMTIMER_H
#define LLE_TMTIMER_H

class TSEvent;

class LLE_API TMTimer {
public:
    TMTimer(unsigned long inInterval, unsigned long inTimeUnit, bool inRepeating);
    ~TMTimer();
    long SetCallback(void (*inCallback)(void*), void* inParameter);
    long SetEvent(TSEvent* inEvent);
    long SetInterval(unsigned long inInterval, unsigned long inTimeUnit);
    long SetRepeating(bool inRepeating);
    long SetTimerType(unsigned long inType);
    long Start();
    long Stop();
    long Stop(unsigned long& outElapsed, unsigned long& outRemaining);
private:
    static void TimerCallback(void* inUser);

    void (*mCallback)(void*);               // +0x00
    void* mParameter;                       // +0x04
    TSEvent* mEvent;                        // +0x08 signalled instead of the callback when mTimerType != 0
    unsigned long mTimerType;               // +0x0c 0: callback, else event
    unsigned long mUnknown10;               // +0x10 zeroed by the ctor and SetInterval only; (provisional name)
    unsigned long mInterval;                // +0x14 milliseconds
    unsigned long mStartTime;               // +0x18 the millisecond clock at Start / each period
    unsigned long mTimerID;                 // +0x1c the platform timer's id
    bool mRepeating;                        // +0x20
    bool mRunning;                          // +0x21
};

#endif
