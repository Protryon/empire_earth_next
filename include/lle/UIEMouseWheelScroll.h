// C:\EEX\Low-Level Engine\User Interface\UIEMouseWheelScroll.h (RECONSTRUCTION). The form's default mouse wheel event
// (UIForm's FormWheelEventNew news one): forwards the wheel to the control under the mouse. UIEMouseWheelScroll.cpp
// (0x1006de9d..0x1006e304) held the ctor, dtor and ProcessEvent; they are inline here (the unit is gone). Vtable
// 0x10099968. sizeof 0xc. Requires lle/UIEvent.h, lle/UIForm.h, lle/UIManager.h.
#ifndef LLE_UIEMOUSEWHEELSCROLL_H
#define LLE_UIEMOUSEWHEELSCROLL_H

#include "lle/UIEvent.h"

class LLE_API UIEMouseWheelScroll : public UIEvent {
public:
    // Scrolls the control under the mouse by the manager's wheel scroll amount
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        if (UIControl* theControl = inForm->GetMouseOverControl())
            theControl->MouseWheelTurned(UIManager::GetMouseWheelScrollAmount());
        return kErrorOK;
    }
};

#endif
