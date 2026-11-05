// C:\EEX\Low-Level Engine\Time Manager\TMPoll.h (folder name inferred; RECONSTRUCTION from TMPoll.cpp
// 0x53819..0x53ad3). Inline members (the UI expands them) are emitted first in TMPoll.cpp. A polled countdown on
// the platform's high-resolution counter (or its millisecond clock when there is none; platform/PlatTime.h).
#ifndef LLE_TMPOLL_H
#define LLE_TMPOLL_H

class LLE_API TMPoll {
public:
    TMPoll(unsigned long inTimeDelay, unsigned long inTimeUnit, bool inStart);
    ~TMPoll() {}                                                                    // exported, ICF-folded at 0xc16a; UIHotKey's sPressedPoll registers a guard-only atexit dtor
    bool GetIsPaused() const { return this->mRunning && this->mPauseRemaining > 0; }  // 0x53819
    bool GetIsRunning() const { return this->mRunning; }                            // 0x53835
    unsigned long GetTimeLeft() const;
    void SetTimeDelay(unsigned long inTimeDelay) { this->mTimeDelay = inTimeDelay > 0 ? inTimeDelay : 1; }  // 0x53839
    void Start();
    void Stop() { this->mRunning = false; }                                         // 0x5384a
    void Pause();
    void Resume();
    unsigned long Poll();
private:
    int64_t mStartTime;                     // +0x00 counter ticks (or millisecond-clock ms; the original stored the halves separately)
    int64_t mPauseRemaining;                // +0x08 time left when paused (0: not paused)
    unsigned long mTimeUnit;                // +0x10 0: milliseconds, 1: microseconds (ctor: divisor 1000 / 1000000)
    unsigned long mTimeDelay;               // +0x14
    bool mUseCounter;                       // +0x18 the system has a high-resolution counter
    bool mRunning;                          // +0x19

    static int64_t sCounterFrequency;       // exported ?sCounterFrequency@TMPoll@@0_JA: ticks per time unit
};

#endif
