// C:\EEX\Low-Level Engine\Input System\Input System.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The devices read the window layer's events (lle/ISDeviceInput.h; DirectInput 7 before, PORT.md step 4.4). The
// devices (LLE lane round 1), the keyboard, the mouse and their entities (round 2) and the entry points (round 7) are
// Rust's: this unit keeps the key names' resource strings and the exported globals.
// 0x10038abf..0x10039715: first Input System unit, so it emits every IS class's header-inline members, implicit copy
// members and vector deleting dtors (ISEntity, ISDevice, ISKeyboard, ISMouse, ISKey, ISExtendedKey, ISMouseAxis,
// ISMouseButton, in include order), then ISInitialize, ISSetDoublePressTimeDelta and the shutdown hook.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/UResourceFile.h"
#include "lle/GE2DPoint.h"
#include "lle/TSScheduler.h"
#include "lle/FSMemoryFile.h"
#include "GEWindow.h"
#include "lle/Input System.h"
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"

ISKeyboard* gISKeyboard;
ISMouse* gISMouse;
// ---- the devices, the keyboard, the mouse and their entities are Rust's (rust/crates/ee_base/src/input); so are the members
// that read the clock or the window's relative mode, and the shutdown (ee_input system.rs)

// Feeds a window event to the system keyboard and mouse: its type and the fields of the key and mouse views (only
// those of the event's type are read: keys by their DIK_* scan code, the mouse's motion, button and wheel)
void ISHandleEvent(const PlatInputEvent& inEvent)
{
    ee_input_handle_event(inEvent.mType, inEvent.mKey.mScanCode, inEvent.mMouse.mDeltaX, inEvent.mMouse.mDeltaY,
                          inEvent.mMouse.mButton, inEvent.mMouse.mWheel);
}

// Creates the system keyboard and mouse on the window's events (DirectInput's devices in the original: a
// non-exclusive keyboard and an exclusive mouse, buffers of 256). The keys are named by the resource strings from
// inFirstStringID on (a string that is not found repeats the previous one), else by the English table.
long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources, unsigned long inFirstStringID,
                  unsigned long inDoublePressTimeDelta)
{
    unsigned long theCount = ee_input_keyboard_key_count();
    std::vector<UWideString> theNames(inResources ? theCount : 0);
    std::vector<const char*> theNamePointers(theNames.size());
    UWideString theName;
    for (unsigned long i = 0; i < theNames.size(); i++) {
        inResources->GetString(i + inFirstStringID, theName);
        theNames[i] = theName;
        theNamePointers[i] = theNames[i];
    }
    return ee_input_initialize(inWindow->GetOSRepresentation(), inResources ? theNamePointers.data() : NULL,
                               inDoublePressTimeDelta, (const EeInputKeyboard**)&gISKeyboard,
                               (const EeInputMouse**)&gISMouse);
}
