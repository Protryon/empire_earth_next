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

#include "rust/sim/scenario.h"

// the numbers (mType .. mUsesPlayerAttribute) are EeScenarioConditionFields (ee_scenario fields.rs)
class EESMCondition : public EeScenarioConditionFields {
public:
    EESMCondition();
    ~EESMCondition();
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1bb154
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x14 0x1bb3b6
    // 0x1ba998: inValue against inTarget by the operator index (mOperator: <, <=, ==, >=, >) (provisional name)
    bool Compare(unsigned long inValue, unsigned long inOperator, unsigned long inTarget);
    bool Evaluate();
    bool EvaluateUncaptured();              // Evaluate's body (Evaluate adds the capture's SMCE record)
    void Copy(const EESMCondition& inCondition);    // 0x1ba9e7 (EEScenarioManager 0x1b14c1; provisional name)
    // inline accessors (the condition screen's Virtual24/Virtual2C: their inline cost decides which
    // GetTriggerScreen sites VC6 refuses)
    const UWideString& GetName() const { return this->mName; }
    const UWideString& GetText() const { return this->mText; }
    unsigned long GetType() const { return this->mType; }
    unsigned long GetID() const { return this->mID; }
    unsigned long GetObjectIndex() const { return this->mObjectIndex; }  // inline (EEScenarioManager 0x5b0d37 / 0x5b3f11)
    void SetName(const UWideString& inName) { this->mName = inName; }
    void SetText(const UWideString& inText) { this->mText = inText; }
    void SetType(unsigned long inType) { this->mType = inType; }

    UWideString mName;                      // +0x04 (the condition screen's name edit box)
    UWideString mText;                      // +0x18 (the condition screen's text edit box)
    UWideString mChatText;                  // +0x2c (EESETSCTChatMessage: the chat text)
};

#endif
