// EEUnitAction.h (RECONSTRUCTION; file name inferred). An action (state) of a unit AI script (the unit's +0x1e8,
// mCommand, is one) and the action and trigger ids. New (port, R5b.4 round 20): the scripts are Rust's (ee_units
// script.rs: the parser of Unit AI Scripts\*.tai, the actions and their triggers, the name tables, the cache); an
// action is a Rust object whose first field is its id, all the C++ reads of it. EEUnitTrigger is gone.
#ifndef EE_EEUNITACTION_H
#define EE_EEUNITACTION_H

#include <stdint.h>

#include "rust/sim/units.h"   // the scripts' names (ee_units script.rs)

class UString;

class EEUnitAction {
public:
    unsigned long GetType() const { return this->mType; }
    uint32_t mType;                         // the action id (5000 is the idle action)

    // 0x22c75a ("" for an unknown id)
    static const char* GetActionName(unsigned long inAction) { return ee_units_script_action_name((uint32_t)inAction); }
    static void* GetScript(const UString& inName) { return reinterpret_cast<void*>(static_cast<uintptr_t>(ee_units_script_get((const uint8_t*)(const char*)inName, inName.GetLength()))); }  // 0x22c7e0 (the script's handle; ee_units_script_find)

private:
    EEUnitAction();                         // made only by the Rust scripts
};

// The action and trigger ids: Rust's enums (ee_sim unit_ids.rs), emitted back as plain C++ enums with their evidence comments.
#include "rust/sim/unit_ids.h"

#endif
