// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAMouseClick.h (RECONSTRUCTION; file name inferred). The
// editor's mouse click action (RTTI EESEAMouseClick > EEAMouseClick; vtable 0x44468c, sizeof 0x10; ctor 0x19176b and
// ??_G 0x191951 emitted by their first user). Its unit EESEAMouseClick.cpp 0x1a747d..0x1a75e0 is gone: Virtual4
// applies the message in Rust, Execute builds it there (ee_screens editor_actions.rs; the host in
// Scenario Editor Actions/EESEARust.cpp). Requires ee/EEAMouseClick.h.
#ifndef EE_EESEAMOUSECLICK_H
#define EE_EESEAMOUSECLICK_H

#include <stdint.h>

// (rust/ee_screens.h declares these too; here so the action's users need no Rust header)
extern "C" void ee_screens_seu_click(void* message);
extern "C" void ee_screens_seu_click_execute(void* action, uint8_t rotating);

class EESEAMouseClick : public EEAMouseClick {
public:
    EESEAMouseClick() {}
    virtual ~EESEAMouseClick() {}   // user-declared empty (its ??_G stores the vptr first; EESETSObject.cpp)
    // +0x04 (0x1a747d): applies the queued click message (select only the unit, toggle it, clear the selection or
    // add the unit), refreshes the unit tab and drops the message
    virtual void Virtual4()
    {
        if (this->mMessage == NULL)
            return;
        ee_screens_seu_click(this->mMessage);
        this->mMessage = NULL;
    }
    // +0x0c (0x1a7556): a click (not while rotating): a selected unit under the mouse is toggled off, empty ground
    // clears the selection, an unselected unit does nothing; posts itself with the message
    virtual void Execute(float inX, float inY, bool inMove, unsigned long inModifiers, bool inIsRotating, float inFacing, bool inForceMove)
    {
        ee_screens_seu_click_execute(static_cast<EEAction*>(this), inIsRotating ? 1 : 0);
    }
};

#endif
