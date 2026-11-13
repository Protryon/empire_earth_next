// C:\EEX\Low-Level Engine\Input System\Input System.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The devices read the window layer's events (lle/ISDeviceInput.h; DirectInput 7 before, PORT.md step 4.4).
// 0x10038abf..0x10039715: first Input System unit, so it emits every IS class's header-inline members, implicit copy
// members and vector deleting dtors (ISEntity, ISDevice, ISKeyboard, ISMouse, ISKey, ISExtendedKey, ISMouseAxis,
// ISMouseButton, in include order), then ISInitialize, ISSetDoublePressTimeDelta and the shutdown hook.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/TSScheduler.h"
#include "lle/FSMemoryFile.h"
#include "GEWindow.h"
#include "lle/Input System.h"
#include "lle/ISExtendedKey.h"
#include "lle/ISMouseAxis.h"
#include "lle/ISMouseButton.h"
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"

void ISShutdown(bool inForced);

ISKeyboard* gISKeyboard;
ISMouse* gISMouse;
unsigned long gDoublePressTimeDelta = 250;
// ---- the devices are Rust's (rust/crates/ee_input; ISDeviceInput and the event switch before)

// Feeds a window event to the system keyboard and mouse: its type and the fields of the key and mouse views (only
// those of the event's type are read: keys by their DIK_* scan code, the mouse's motion, button and wheel)
void ISHandleEvent(const PlatInputEvent& inEvent)
{
    ee_input_handle_event(inEvent.mType, inEvent.mKey.mScanCode, inEvent.mMouse.mDeltaX, inEvent.mMouse.mDeltaY,
                          inEvent.mMouse.mButton, inEvent.mMouse.mWheel);
}

// FUNCTION: LLE 0x100394a7
// Creates the system keyboard and mouse on the window's events (DirectInput's devices in the original: a
// non-exclusive keyboard and an exclusive mouse, buffers of 256)
long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources, unsigned long inFirstStringID,
                  unsigned long inDoublePressTimeDelta)
{
    gDoublePressTimeDelta = inDoublePressTimeDelta;
    URegisterShutdown(ISShutdown);
    ee_input_system_init();
    gISKeyboard = new ISKeyboard(ee_input_system_device(0), "System Keyboard", inResources, inFirstStringID);
    gISMouse = new ISMouse(ee_input_system_device(1), inWindow->GetOSRepresentation(), "System Mouse");
    return kErrorOK;
}

// FUNCTION: LLE 0x10039691
// Sets the maximum time between two presses that counts as a double press
void ISSetDoublePressTimeDelta(unsigned long inDelta)
{
    gDoublePressTimeDelta = inDelta;
}

// FUNCTION: LLE 0x1003969b
// Suspends and destroys the system mouse and keyboard and their event buffers
void ISShutdown(bool inForced)
{
    if (gISMouse) {
        gISMouse->Suspend();
        delete gISMouse;
        gISMouse = NULL;
    }
    if (gISKeyboard) {
        gISKeyboard->Suspend();
        delete gISKeyboard;
        gISKeyboard = NULL;
    }
    ee_input_system_shutdown();
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10038ada
// SYNTHETIC: LLE 0x10038b89
// SYNTHETIC: LLE 0x10038d82
// SYNTHETIC: LLE 0x10038f53
// SYNTHETIC: LLE 0x100390b6
// SYNTHETIC: LLE 0x1003925a
// SYNTHETIC: LLE 0x1003931f
// SYNTHETIC: LLE 0x1003944d

// Header-inline members emitted here (markers at the end):
// SYNTHETIC: LLE 0x10038abf
// SYNTHETIC: LLE 0x10038ac8
// SYNTHETIC: LLE 0x10038acf
// SYNTHETIC: LLE 0x10038b29
// SYNTHETIC: LLE 0x10038b41
// SYNTHETIC: LLE 0x10038b45
// SYNTHETIC: LLE 0x10038b6a
// SYNTHETIC: LLE 0x10038be3
// SYNTHETIC: LLE 0x10038c2f
// SYNTHETIC: LLE 0x10038c47
// SYNTHETIC: LLE 0x10038cd6
// SYNTHETIC: LLE 0x10038de1
// SYNTHETIC: LLE 0x10038e1d
// SYNTHETIC: LLE 0x10038e27
// SYNTHETIC: LLE 0x10038e2b
// SYNTHETIC: LLE 0x10038eb2
// SYNTHETIC: LLE 0x10038faf
// SYNTHETIC: LLE 0x10038fbe
// SYNTHETIC: LLE 0x10038fd0
// SYNTHETIC: LLE 0x10039007
// SYNTHETIC: LLE 0x1003903e
// SYNTHETIC: LLE 0x10039044
// SYNTHETIC: LLE 0x10039083
// SYNTHETIC: LLE 0x10039110
// SYNTHETIC: LLE 0x10039116
// SYNTHETIC: LLE 0x1003911a
// SYNTHETIC: LLE 0x10039136
// SYNTHETIC: LLE 0x10039152
// SYNTHETIC: LLE 0x10039178
// SYNTHETIC: LLE 0x1003919e
// SYNTHETIC: LLE 0x100391c4
// SYNTHETIC: LLE 0x100391d0
// SYNTHETIC: LLE 0x100391dc
// SYNTHETIC: LLE 0x10039221
// SYNTHETIC: LLE 0x100392b5
// SYNTHETIC: LLE 0x100392bf
// SYNTHETIC: LLE 0x100392c5
// SYNTHETIC: LLE 0x100392f8
// SYNTHETIC: LLE 0x1003937a
// SYNTHETIC: LLE 0x1003938a
// SYNTHETIC: LLE 0x1003938e
// SYNTHETIC: LLE 0x1003939c
// SYNTHETIC: LLE 0x100393aa
// SYNTHETIC: LLE 0x100393dc
// SYNTHETIC: LLE 0x1003940e
