// New (port): the game screens' one event class (ee/EEScreenEvent.h), its factory and the forms' grab scroll events,
// in a unit of their own (they were UI Events/EEScreens.cpp's). The game's messages, the command line, the
// player, the server and the civilization builder make screen events; in EEScreens.cpp, the screens' host, they
// sat in the C++ dependency cycle with every screen (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3"). It
// names only the toolkit's event and the Rust dispatcher.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UIEvent.h"
#include "ee/EEScreenEvent.h"

// a new screen event of the code, NULL for code 0
UIEvent* EECreateScreenEvent(unsigned long inEvent) { return inEvent == 0 ? NULL : new EEScreenEvent(inEvent); }

// The screens' events for ee_screens toolkit.rs (its event group): a new event of the code, an event's Reference,
// its Dereference
extern "C" void* ee_screenscpp_event_new(uint32_t inCode) { return EECreateScreenEvent(inCode); }
extern "C" void ee_screenscpp_event_ref(void* inEvent) { static_cast<UIEvent*>(inEvent)->Reference(); }
extern "C" void ee_screenscpp_event_unref(void* inEvent) { static_cast<UIEvent*>(inEvent)->Dereference(); }

// A form's grab scroll event (0 the main game form's, 1 the scenario editor's), its record fresh
UIEvent* EECreateGrabScroll(unsigned long inForm)
{
    EEReleaseGrabScroll(inForm);
    return EECreateScreenEvent(inForm ? EE_SCREENS_CAM_GRAB_EDITOR : EE_SCREENS_CAM_GRAB_MAIN);
}

// (the mouse move event the grab saved is released there: ee_screens toolkit.rs's event group holds it)
void EEReleaseGrabScroll(unsigned long inForm) { ee_screens_camera_grab_reset(inForm); }
