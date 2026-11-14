// C:\EEX\Low-Level Engine\User Interface\UIEPolledEvent.h (RECONSTRUCTION). An event that fires its handler at most
// once per poll interval. UIEPolledEvent.cpp (0x1006e5a8..0x1006e6de) emits the implicit copy members and ??_E, then
// the ctor, dtor and ProcessEvent. Vtable 0x10099988 (slot 0x10 is pure). sizeof 0x10. Requires lle/UIEvent.h,
// lle/TMPoll.h.
#ifndef LLE_UIEPOLLEDEVENT_H
#define LLE_UIEPOLLEDEVENT_H

#include "lle/UIEvent.h"

class TMPoll;

class LLE_API UIEPolledEvent : public UIEvent {
public:
    UIEPolledEvent(unsigned long inInterval);
    virtual ~UIEPolledEvent();
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);
protected:
    virtual long ProcessPolledEvent(UIForm* inForm, UIEntity* inEntity) = 0;   // slot 0x10 (provisional name)

    TMPoll* mPoll;                          // +0x0c
};

#endif
