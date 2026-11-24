// C:\EEX\Low-Level Engine\Input System\ISMouse.h (RECONSTRUCTION; ISMouse.cpp 0x3b2ce..0x3bab2 before). The system
// mouse (DirectInput's exclusive mouse in the original; the window layer's events in relative mode now:
// lle/ISDeviceInput.h). Whole in Rust (rust/crates/ee_base/src/input devices.rs, LLE lane round 2): gISMouse points at Rust's
// mouse and these forwarders call it; Resume and Suspend also put the game window's pointer in and out of relative
// mode, and Refresh passes TMGetTime (Input System.cpp). The class has no data.
#ifndef LLE_ISMOUSE_H
#define LLE_ISMOUSE_H
#include "lle/LLEForward.h"
#include "rust/base/lle_enums.h"

#include "lle/ISEntity.h"

class LLE_API ISMouse : URustView {
public:
    // the axis or button at an index (ISMouseEntity); kErrorBeyondEnd past 6
    long GetEntity(unsigned long inIndex, ISEntity*& outEntity) const
        { return ee_input_mouse_entity(Rust(), inIndex, (const EeInputEntity**)&outEntity); }
    bool GetShouldUpdate() const { return ee_input_mouse_should_update(Rust()); }
    // reads the buffered mouse events: motion, wheel, button presses and releases, double presses
    void Refresh() { ee_input_mouse_refresh_now(Rust()); }
    void RefreshXY() { ee_input_mouse_refresh_xy(Rust()); }
    // takes the mouse for the game (the window's pointer in relative mode) and rereads its state
    void Resume() { ee_input_mouse_resume_window(Rust()); }
    void SetUseBufferedMovement(bool inUse) { ee_input_mouse_set_use_buffered_movement(Rust(), inUse); }
    // releases the mouse (the pointer leaves relative mode)
    void Suspend() { ee_input_mouse_suspend_window(Rust()); }
    const EeInputMouse* Rust() const { return reinterpret_cast<const EeInputMouse*>(this); }
};

#endif
