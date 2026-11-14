// C:\EEX\Low-Level Engine\Input System\Input System.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The devices read the window layer's events (lle/ISDeviceInput.h; DirectInput 7 before, PORT.md step 4.4). The
// devices (LLE lane round 1), the keyboard, the mouse and their entities (round 2) are Rust's: this unit keeps the
// entry points, the forwarders that pass TMGetTime and the window's relative mode.
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

void ISShutdown(bool inForced);

ISKeyboard* gISKeyboard;
ISMouse* gISMouse;
// the game window, whose pointer is in relative mode while the mouse is resumed (DirectInput's exclusive mouse)
static PlatWindow* sMouseWindow;
// ---- the devices, the keyboard, the mouse and their entities are Rust's (rust/crates/ee_input)

// Feeds a window event to the system keyboard and mouse: its type and the fields of the key and mouse views (only
// those of the event's type are read: keys by their DIK_* scan code, the mouse's motion, button and wheel)
void ISHandleEvent(const PlatInputEvent& inEvent)
{
    ee_input_handle_event(inEvent.mType, inEvent.mKey.mScanCode, inEvent.mMouse.mDeltaX, inEvent.mMouse.mDeltaY,
                          inEvent.mMouse.mButton, inEvent.mMouse.mWheel);
}

// Reads the buffered key events (presses, releases, modifiers, double presses)
void ISKeyboard::Refresh()
{
    ee_input_keyboard_refresh(this->Rust(), TMGetTime());
}

// Reacquires the keyboard and rereads its state
void ISKeyboard::Resume()
{
    ee_input_keyboard_resume(this->Rust(), TMGetTime());
}

// Reads the buffered mouse events: motion, wheel, button presses and releases, double presses
void ISMouse::Refresh()
{
    ee_input_mouse_refresh(this->Rust(), TMGetTime());
}

// Takes the mouse for the game (the pointer in relative mode: DirectInput's exclusive foreground mouse) and rereads
// its state
void ISMouse::Resume()
{
    if (sMouseWindow != NULL)
        PlatMouseSetRelative(sMouseWindow, 1);
    ee_input_mouse_resume(this->Rust());
}

// Releases the mouse (its events are no longer buffered; the pointer leaves relative mode)
void ISMouse::Suspend()
{
    ee_input_mouse_suspend(this->Rust());
    if (sMouseWindow != NULL)
        PlatMouseSetRelative(sMouseWindow, 0);
}

// FUNCTION: LLE 0x100394a7
// Creates the system keyboard and mouse on the window's events (DirectInput's devices in the original: a
// non-exclusive keyboard and an exclusive mouse, buffers of 256). The keys are named by the resource strings from
// inFirstStringID on (a string that is not found repeats the previous one), else by the English table.
long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources, unsigned long inFirstStringID,
                  unsigned long inDoublePressTimeDelta)
{
    ee_input_set_double_press_delay(inDoublePressTimeDelta);
    URegisterShutdown(ISShutdown);
    ee_input_system_init();
    unsigned long theCount = ee_input_keyboard_key_count();
    std::vector<UWideString> theNames(inResources ? theCount : 0);
    std::vector<const char*> theNamePointers(theNames.size());
    UWideString theName;
    for (unsigned long i = 0; i < theNames.size(); i++) {
        inResources->GetString(i + inFirstStringID, theName);
        theNames[i] = theName;
        theNamePointers[i] = theNames[i];
    }
    gISKeyboard = reinterpret_cast<ISKeyboard*>(const_cast<EeInputKeyboard*>(
        ee_input_keyboard_create(inResources ? theNamePointers.data() : NULL, TMGetTime())));
    // the original constructor resumed and suspended the mouse: the pointer went into relative mode and out again
    sMouseWindow = inWindow->GetOSRepresentation();
    if (sMouseWindow != NULL)
        PlatMouseSetRelative(sMouseWindow, 1);
    gISMouse = reinterpret_cast<ISMouse*>(const_cast<EeInputMouse*>(ee_input_mouse_create()));
    if (sMouseWindow != NULL)
        PlatMouseSetRelative(sMouseWindow, 0);
    return kErrorOK;
}

// FUNCTION: LLE 0x1003969b
// Suspends and destroys the system mouse and keyboard and their event buffers
void ISShutdown(bool inForced)
{
    if (gISMouse) {
        gISMouse->Suspend();
        ee_input_mouse_destroy(reinterpret_cast<const EeInputMouse*>(gISMouse));
        gISMouse = NULL;
    }
    if (gISKeyboard) {
        gISKeyboard->Suspend();
        ee_input_keyboard_destroy(reinterpret_cast<const EeInputKeyboard*>(gISKeyboard));
        gISKeyboard = NULL;
    }
    sMouseWindow = NULL;
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
