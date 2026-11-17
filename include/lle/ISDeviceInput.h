// The Input System's devices on the window layer's events (platform/PlatWindow.h; PORT.md step 4.4): what the
// keyboard and the mouse read from DirectInput 7 before, with its semantics. The devices, the keyboard and the mouse
// are Rust's (rust/crates/ee_base/src/input; the devices LLE lane round 1, the keyboard and mouse round 2): the window thread
// hands every key and mouse event to ISHandleEvent (Input System.cpp, one line into ee_input_handle_event), which
// buffers up to 256 records per device while it is acquired (DirectInput's DIPROP_BUFFERSIZE) and keeps the key
// bytes, the mouse's motion and its buttons; ISKeyboard and ISMouse read them on Refresh.
#ifndef LLE_ISDEVICEINPUT_H
#define LLE_ISDEVICEINPUT_H

struct PlatInputEvent;
// Feeds a window event to the system keyboard and mouse (the window thread calls it for every event)
LLE_API void ISHandleEvent(const PlatInputEvent& inEvent);

#endif
