// C:\EEX\Low-Level Engine\Time Manager\TMPoll.h (folder name inferred; RECONSTRUCTION from TMPoll.cpp
// 0x53819..0x53ad3). A polled countdown on the platform's high-resolution counter (or its millisecond clock when there
// is none; platform/PlatTime.h). Whole in Rust (rust/crates/ee_base/src/time, LLE lane round 3): the class holds the Rust
// record by value (the original's members, sizeof 0x20, 8-aligned: the classes that embed a poll keep their layout)
// and every member forwards. The original's shared TMPoll::sCounterFrequency is ee_time's TIME state.
#ifndef LLE_TMPOLL_H
#define LLE_TMPOLL_H

#include "rust/ee_time.h"

class LLE_API TMPoll {
public:
    TMPoll(unsigned long inTimeDelay, unsigned long inTimeUnit, bool inStart)
    {
        ee_time_poll_init(&this->mState, (uint32_t)inTimeDelay, (uint32_t)inTimeUnit, inStart);
    }
    ~TMPoll() {}
    bool GetIsPaused() const { return ee_time_poll_is_paused(&this->mState); }
    bool GetIsRunning() const { return ee_time_poll_is_running(&this->mState); }
    unsigned long GetTimeLeft() const { return ee_time_poll_time_left(&this->mState); }
    void SetTimeDelay(unsigned long inTimeDelay) { ee_time_poll_set_time_delay(&this->mState, (uint32_t)inTimeDelay); }
    void Start() { ee_time_poll_start(&this->mState); }
    void Stop() { ee_time_poll_stop(&this->mState); }
    void Pause() { ee_time_poll_pause(&this->mState); }
    void Resume() { ee_time_poll_resume(&this->mState); }
    unsigned long Poll() { return ee_time_poll_poll(&this->mState); }
private:
    EeTimePoll mState;                      // +0x00 start, pause remainder, unit, delay, counter, running
};

#endif
