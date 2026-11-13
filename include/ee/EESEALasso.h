// C:\EEX\Empire Earth\Scenario Editor Actions\EESEALasso.h (RECONSTRUCTION; file name inferred). The
// editor's lasso action (RTTI EESEALasso > EEALasso; vtable 0x4446a0, sizeof 0x10; ctor 0x19177d and ??_G 0x191795
// emitted by their first user). Its unit EESEALasso.cpp 0x1a7358..0x1a747d is gone: the bodies call the Rust
// selection (ee_screens editor_actions.rs; the host in Scenario Editor Actions/EESEARust.cpp). Requires ee/EEALasso.h.
#ifndef EE_EESEALASSO_H
#define EE_EESEALASSO_H

#include <stdint.h>

// (rust/ee_screens.h declares these too; here so the action's users need no Rust header)
extern "C" void ee_screens_seu_lasso_message(void* message);
extern "C" void ee_screens_seu_lasso_in_rectangle(void* action, const void* rectangle);
extern "C" void ee_screens_seu_lasso_select(const void* rectangle, int32_t unit_type, uint8_t deselect, uint8_t deselect_only);

class EESEALasso : public EEALasso {
public:
    EESEALasso() {}
    virtual ~EESEALasso() {}   // user-declared empty (its ??_G stores the vptr first; EESETSObject.cpp)
    // +0x04 (0x1a73ef): applies the queued lasso message (replaces the selection with, adds, or toggles the units in
    // its rectangle), refreshes the unit tab and drops the message
    virtual void Virtual4()
    {
        if (this->mMessage == NULL)
            return;
        ee_screens_seu_lasso_message(this->mMessage);
        this->mMessage = NULL;
    }
    // +0x0c (0x1a745b): deselects the selected units inside the rectangle and posts the action (inArg2 unused)
    virtual void SelectInRectangle(const GERectangle& inRectangle, long inArg2)
    {
        ee_screens_seu_lasso_in_rectangle(static_cast<EEAction*>(this), &inRectangle);
    }
    // 0x1a7358: the units of a type in inRectangle selected when not yet (unless inDeselectOnly), else deselected
    // with inDeselect (provisional names)
    void SelectUnits(const GERectangle& inRectangle, long inUnitType, bool inDeselect, bool inDeselectOnly)
    {
        ee_screens_seu_lasso_select(&inRectangle, inUnitType, inDeselect ? 1 : 0, inDeselectOnly ? 1 : 0);
    }
};

#endif
