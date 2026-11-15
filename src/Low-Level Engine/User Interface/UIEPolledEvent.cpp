// C:\EEX\Low-Level Engine\User Interface\UIEPolledEvent.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1006e5a8..0x1006e6de: the implicit copy ctor, operator= and ??_E first, then the ctor, dtor and ProcessEvent. No
// U2D key chains (the unit does not include the geometry headers). UIESliderChange.cpp starts at 0x1006e6de.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"
#include "lle/UIEvent.h"
#include "lle/UIEPolledEvent.h"

// FUNCTION: LLE 0x1006e64b
// Creates an event that runs at most once per interval, starting its timer
UIEPolledEvent::UIEPolledEvent(unsigned long inInterval)
{
    this->mPoll = new TMPoll(inInterval, 0, false);
    this->mPoll->Start();
}

// FUNCTION: LLE 0x1006e68e
// Stops and deletes the event's timer
UIEPolledEvent::~UIEPolledEvent()
{
    this->mPoll->Stop();
    if (this->mPoll)
        delete this->mPoll;
}

// FUNCTION: LLE 0x1006e6b4
// Runs the polled event when its interval has elapsed; OK otherwise
long UIEPolledEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = kErrorOK;
    if (this->mPoll->Poll() > 0)
        theResult = this->ProcessPolledEvent(inForm, inEntity);
    return theResult;
}

// Implicit copy members and ??_E (emitted first)
// FUNCTION: LLE 0x1006e5a8
// FUNCTION: LLE 0x1006e5d5
// SYNTHETIC: LLE 0x1006e5f0
