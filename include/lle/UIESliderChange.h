// C:\EEX\Low-Level Engine\User Interface\UIESliderChange.h (RECONSTRUCTION). An event that writes a slider's value
// (scaled by mModifier, plus mOffset) into a text label, or the string registered for that value. UIESliderChange.cpp
// (0x1006e6de..0x1006f278) emits the inline members first (declaration order), then the implicit copy members and
// ??_E, then ProcessEvent (the only out-of-line member). Vtable 0x100999a0. sizeof 0x24.
// Whole in Rust (UI round 15: rust/crates/ee_ui/src/slider_change.rs, the map, the label, the offset and the
// modifier); every member here is one call. Requires lle/UIEvent.h.
#ifndef LLE_UIESLIDERCHANGE_H
#define LLE_UIESLIDERCHANGE_H

#include "lle/UIEvent.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "rust/ui/slider_change.h"

class LLE_API UIESliderChange : public UIEvent {
public:
    UIESliderChange(UITextLabel* inLabel, float inModifier, long inOffset)
        : mRust(ee_ui_slider_change_new(static_cast<UIControl*>(inLabel), inModifier, inOffset)) {}
    virtual ~UIESliderChange() { ee_ui_slider_change_free(this->mRust); }
    void AddString(float inValue, const UWideString& inString) { ee_ui_slider_change_add(this->mRust, inValue, inString); }
    // Shows the slider's value (scaled by the modifier, plus the offset) in the label: its string when one was
    // added for that value, else the number ("%g")
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity)
    {
        return ee_ui_slider_change_process(this->mRust, static_cast<UIControl*>(static_cast<UISlider*>(inEntity)));
    }
    void SetModifier(float inModifier) { ee_ui_slider_change_set(this->mRust, 0, inModifier, 0); }
    EeUiSliderChange* mRust;                // the map, the label, the offset, the modifier (slider_change.rs)
};

#endif
