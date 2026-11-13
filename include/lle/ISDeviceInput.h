// The Input System's devices on the window layer's events (platform/PlatWindow.h; PORT.md step 4.4): what the
// keyboard and the mouse read from DirectInput 7 before, with its semantics. The devices are Rust's
// (rust/crates/ee_input, LLE lane round 1): the window thread hands every key and mouse event to ISHandleEvent
// (Input System.cpp, one line into ee_input_handle_event); the game threads read them through ee_input_*:
// - a buffer of up to 256 records (DirectInput's DIPROP_BUFFERSIZE 256) taken while the device is acquired, read
//   oldest first; records that did not fit are reported once as DI_BUFFEROVERFLOW was (ee_input_get_data);
// - the device state: the keyboard's 256 key bytes (0x80 held, by DIK_* scan code; ee_input_get_key_state), the
//   mouse's motion since the last state read (relative axes: X, Y and the wheel Z in WHEEL_DELTA units) and its four
//   buttons (ee_input_get_mouse_state).
// Records: dwOfs is the DIK_* code (keyboard) or the DIMOFS_* offset (mouse), dwData 0x80 / 0 for a press / release,
// the motion for an axis.
#ifndef LLE_ISDEVICEINPUT_H
#define LLE_ISDEVICEINPUT_H

#include "rust/ee_input.h"

typedef EeInputDevice ISDeviceInput;        // opaque: ee_input_system_device(0) the keyboard's, (1) the mouse's
typedef EeInputMouseState ISMouseState;     // DIMOUSESTATE: long lX, lY, lZ; unsigned char rgbButtons[4]
typedef EeInputRecord ISInputRecord;        // DIDEVICEOBJECTDATA's dwOfs and dwData (unsigned long)

// DirectInput's mouse data offsets (the DIMOUSESTATE field offsets)
enum {
    kISMouseOffsetX = 0,
    kISMouseOffsetY = 4,
    kISMouseOffsetZ = 8,
    kISMouseOffsetButton0 = 12,             // left
    kISMouseOffsetButton1 = 13,             // right
    kISMouseOffsetButton2 = 14,             // middle
    kISMouseOffsetButton3 = 15              // the first side button
};

// ee_input_get_data's results (DI_OK, DI_BUFFEROVERFLOW, DIERR_NOTACQUIRED)
enum { kISInputOK = 0, kISInputOverflow = 1, kISInputNotAcquired = -1 };

struct PlatInputEvent;
// Feeds a window event to the system keyboard and mouse (the window thread calls it for every event)
LLE_API void ISHandleEvent(const PlatInputEvent& inEvent);

#endif
