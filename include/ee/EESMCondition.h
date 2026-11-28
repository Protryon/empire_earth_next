// C:\EEX\Empire Earth\Scenario Manager\EESMCondition.h (RECONSTRUCTION; file name inferred). One trigger condition of
// the scenario (RTTI EESMCondition, a root class; vtable 0x446044, sizeof 0x88: `new` at 0x1afc3e/0x1b0861/0x1b0aa7;
// ctor 0x1ba89f; the dtor 0x1ba976 is not virtual and stores the vptr: user-declared). EEScenarioManager keeps them
// in mV0F4 (GetCondition). The editor's condition screen
// (EESETSCondition) edits mName/mText/mType. Slots: +0x00 Load(FSFile&) 0x1bb154, +0x14 Save 0x1bb3b6 (UWideString
// Load/Save of +0x04 first), +0x04/+0x0c remap the ids at +0x60/+0x78 through a map (0x1b9fcf), +0x08/+0x10 renumber
// them when an entry moves. Member names by offset unless stated; slot names by offset (provisional name).
// Requires lle/UWideString.h, lle/FSFile.h.
#ifndef EE_EESMCONDITION_H
#define EE_EESMCONDITION_H

#include "rust/sim/records.h"
#include "rust/sim/scenario.h"

// the numbers (mType .. mUsesPlayerAttribute) are EeScenarioConditionFields (ee_scenario fields.rs)
class EESMCondition : public EeScenarioConditionFields {
public:
    // Creates a player-attribute condition with default players, attribute and operator, and no trigger or time tests
    // (the numbers: ee_scenario fields.rs condition_defaults; the strings empty)
    EESMCondition() : EeScenarioConditionFields(ee_scenario_condition_defaults()) {  }
    // Destroys the condition (members only)
    ~EESMCondition() {  }
    // Reads the condition (diplomacy above 1 becomes 1); returns the first error, ignoring the chat text's
    // (ee_scenario save.rs condition_load)
    // +0x00 0x1bb154
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_condition_io(this, &inFile, false); }
    // Writes the condition; returns the first error (ee_scenario save.rs condition_save)
    // +0x14 0x1bb3b6
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_condition_io(this, &outFile, true); }
    // Returns whether the condition holds: object exists, player attribute/technology/diplomacy for any or all chosen
    // players, trigger on/fired, elapsed time/hour/difficulty, or the last chat text containing a phrase (ee_scenario
    // condition.rs over the host's reads; entry.rs adds the simulation capture's SMCE record)
    bool Evaluate() { return ee_scenario_en_condition_evaluate(this); }
    // Copies every setting of another condition, including its name, texts and ID (ee_scenario entry.rs)
    // 0x1ba9e7 (EEScenarioManager 0x1b14c1; provisional name)
    void Copy(const EESMCondition& inCondition) { ee_scenario_en_condition_copy(this, (void*)&inCondition); }
    // inline accessors (the condition screen's Virtual24/Virtual2C: their inline cost decides which
    // GetTriggerScreen sites VC6 refuses)
    unsigned long GetType() const { return this->mType; }

    UWideString mName;                      // +0x04 (the condition screen's name edit box)
    UWideString mText;                      // +0x18 (the condition screen's text edit box)
    UWideString mChatText;                  // +0x2c (EESETSCTChatMessage: the chat text)
};

#endif
