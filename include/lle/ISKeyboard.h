// C:\EEX\Low-Level Engine\Input System\ISKeyboard.h (RECONSTRUCTION; ISKeyboard.cpp 0x39ca1..0x3b2ce before). The
// system keyboard (DirectInput's in the original; the window layer's events now: lle/ISDeviceInput.h). Whole in
// Rust (rust/crates/ee_base/src/input devices.rs, LLE lane round 2): gISKeyboard points at Rust's keyboard and these
// forwarders call it; Refresh and Resume pass TMGetTime (Input System.cpp). The class has no data.
#ifndef LLE_ISKEYBOARD_H
#define LLE_ISKEYBOARD_H

#include "platform/PlatKeys.h"              // DIK_* scan codes
#include "lle/ISDeviceInput.h"
#include "lle/ISEntity.h"
#include "lle/ISKeyCode.h"
#include "lle/ISModifierState.h"

class LLE_API ISKeyboard {
public:
    // modifier bit (1 Shift, 2 Control, 4 Alt) of a DIK_* code; 0xe0..0xe2 are the extended (either-side) keys
    static unsigned long ComputeModifierState(unsigned long inKey) { return ee_input_modifier_state(inKey); }
    // the key at a scan code (NULL for a code without one); kErrorBeyondEnd past 255
    long GetEntity(unsigned long inIndex, ISEntity*& outEntity) const
    {
        const EeInputEntity* theEntity;
        long theResult = ee_input_keyboard_entity(Rust(), inIndex, &theEntity);
        outEntity = reinterpret_cast<ISEntity*>(const_cast<EeInputEntity*>(theEntity));
        return theResult;
    }
    unsigned long GetModifierState() const { return ee_input_keyboard_modifiers(Rust()); }
    bool GetShouldUpdate() const { return ee_input_keyboard_should_update(Rust()); }
    // reads the buffered key events (presses, releases, modifiers, double presses) at TMGetTime
    void Refresh() { ee_input_keyboard_refresh_now(Rust()); }
    // reacquires the keyboard and rereads its state
    void Resume() { ee_input_keyboard_resume_now(Rust()); }
    void Suspend() { ee_input_keyboard_suspend(Rust()); }
private:
    ISKeyboard() = delete;
    ~ISKeyboard() = delete;
    const EeInputKeyboard* Rust() const { return reinterpret_cast<const EeInputKeyboard*>(this); }
};

#endif
