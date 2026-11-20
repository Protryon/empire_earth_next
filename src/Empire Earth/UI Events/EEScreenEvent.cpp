// New (port): the game screens' one event class (ee/EEScreenEvent.h), its factory and the forms' grab scroll events,
// in a unit of their own (they were UI Events/EEScreens.cpp's). The game's messages, the command line, the
// player, the server and the civilization builder make screen events; in EEScreens.cpp, the screens' host, they
// sat in the C++ dependency cycle with every screen (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts, round 3"). It
// names only the toolkit's event and the Rust dispatcher.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UIEvent.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"

long EEScreenEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return ee_screens_event(this->mEvent, inForm, inEntity);
}

UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    if (inEvent == 0)
        return NULL;
    return new EEScreenEvent(inEvent);
}

// The screens' events for ee_screens toolkit.rs (its event group): 0 a new event of the code, 1 the event's
// Reference, 2 its Dereference
extern "C" void* ee_screenscpp_event(uint32_t inOp, void* inEvent, uint32_t inCode)
{
    if (inOp == 0)
        return EECreateScreenEvent(inCode);
    if (inOp == 1)
        static_cast<UIEvent*>(inEvent)->Reference();
    else
        static_cast<UIEvent*>(inEvent)->Dereference();
    return NULL;
}

// A form's grab scroll event (0 the main game form's, 1 the scenario editor's), its record fresh
UIEvent* EECreateGrabScroll(unsigned long inForm)
{
    EEReleaseGrabScroll(inForm);
    return EECreateScreenEvent(inForm ? EE_SCREENS_CAM_GRAB_EDITOR : EE_SCREENS_CAM_GRAB_MAIN);
}

// (the mouse move event the grab saved is released there: ee_screens toolkit.rs's event group holds it)
void EEReleaseGrabScroll(unsigned long inForm) { ee_screens_camera_grab_reset(inForm); }
