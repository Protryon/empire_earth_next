// C:\EEX\Low-Level Engine\User Interface\UIEPolledEvent.h (RECONSTRUCTION). An event that fires its handler at most
// once per poll interval. UIEPolledEvent.cpp (0x1006e5a8..0x1006e6de) emitted the implicit copy members and ??_E,
// then the ctor, dtor and ProcessEvent; they are inline here (the unit is gone; TMPoll is Rust's, ee_base time).
// Vtable 0x10099988 (slot 0x10 is pure). sizeof 0x10. Requires lle/UIEvent.h.
#ifndef LLE_UIEPOLLEDEVENT_H
#define LLE_UIEPOLLEDEVENT_H

#include "lle/UIEvent.h"
#include "lle/TMPoll.h"

class LLE_API UIEPolledEvent : public UIEvent {
public:
    // Creates an event that runs at most once per interval, starting its timer
    UIEPolledEvent(unsigned long inInterval)
    {
        this->mPoll = new TMPoll(inInterval, 0, false);
        this->mPoll->Start();
    }
    // Stops and deletes the event's timer
    virtual ~UIEPolledEvent()
    {
        this->mPoll->Stop();
        if (this->mPoll)
            delete this->mPoll;
    }
    // Runs the polled event when its interval has elapsed; OK otherwise
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        long theResult = kErrorOK;
        if (this->mPoll->Poll() > 0)
            theResult = this->ProcessPolledEvent(inForm, inEntity);
        return theResult;
    }
protected:
    virtual long ProcessPolledEvent(UIForm* inForm, UIEntity* inEntity) = 0;   // slot 0x10 (provisional name)

    TMPoll* mPoll;                          // +0x0c
};

#endif
