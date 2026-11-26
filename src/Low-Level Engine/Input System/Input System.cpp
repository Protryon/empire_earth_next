// C:\EEX\Low-Level Engine\Input System\Input System.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The devices read the window layer's events (platform/PlatWindow.h; DirectInput 7 before, PORT.md step 4.4): the
// window thread hands every key and mouse event to ee_input_handle_event (ee_game window_pump.rs), which buffers up to
// 256 records per device while it is acquired and keeps the key bytes, the mouse's motion and its buttons; ISKeyboard
// and ISMouse read them on Refresh. The devices (LLE lane round 1), the keyboard, the mouse and their entities (round
// 2) and the entry points (round 7) are Rust's: this unit keeps the key names' resource strings and the exported
// globals.
// 0x10038abf..0x10039715: first Input System unit, so it emits every IS class's header-inline members, implicit copy
// members and vector deleting dtors (ISEntity, ISDevice, ISKeyboard, ISMouse, ISKey, ISExtendedKey, ISMouseAxis,
// ISMouseButton, in include order), then ISInitialize, ISSetDoublePressTimeDelta and the shutdown hook.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "GEWindow.h"
#include "lle/Input System.h"
#include "platform/PlatWindow.h"

ISKeyboard* gISKeyboard;
ISMouse* gISMouse;
// ---- the devices, the keyboard, the mouse and their entities are Rust's (rust/crates/ee_base/src/input); so are the members
// that read the clock or the window's relative mode, and the shutdown (ee_input system.rs)

// Creates the system keyboard and mouse on the window's events (DirectInput's devices in the original: a
// non-exclusive keyboard and an exclusive mouse, buffers of 256). The keys are named by the resource strings from
// inFirstStringID on (a string that is not found repeats the previous one), else by the English table.
long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources, unsigned long inFirstStringID,
                  unsigned long inDoublePressTimeDelta)
{
    return ee_input_initialize_strings(inWindow->GetOSRepresentation(), inResources != NULL, inFirstStringID,
                                       inDoublePressTimeDelta, (const EeInputKeyboard**)&gISKeyboard,
                                       (const EeInputMouse**)&gISMouse);
}
