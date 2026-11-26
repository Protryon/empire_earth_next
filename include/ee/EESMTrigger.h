// C:\EEX\Empire Earth\Scenario Manager\EESMTrigger.h (RECONSTRUCTION; file name inferred).
// One scenario trigger (RTTI EESMTrigger, a root class; vtable 0x4460e4, six slots). Layout from the ctor 0x1c97b7
// (two UWideStrings, two vectors of wrapper pointers, then the scalars) and the dtor 0x1c98dd (not virtual; stores
// the vptr: user-declared; deletes every wrapper, non-virtually). EEScenarioManager keeps them in mTriggers.
// Slots: +0x00 Load 0x1c9d56, +0x04..+0x10 the entries' remap and renumber (ee_scenario lists.rs), +0x14 Save 0x1ca0b3. Member and slot names by offset (provisional name).
// Requires lle/UWideString.h, lle/FSFile.h, <vector>.
#ifndef EE_EESMTRIGGER_H
#define EE_EESMTRIGGER_H

#include <vector>
#include "rust/sim/scenario.h"

// a condition entry and an effect entry (new'd value-initialized: zeros, the End operator) are Rust records
// (ee_scenario fields.rs)
typedef EeScenarioTConditionWrapper EESMTConditionWrapper;
typedef EeScenarioTEffectWrapper EESMTEffectWrapper;

// the numbers (mConditionDuration .. mIsInCinematic) are EeScenarioTriggerFields (ee_scenario fields.rs)
class EESMTrigger : public EeScenarioTriggerFields {
public:
    // Creates an enabled, non-looping trigger with one empty condition entry and one empty effect entry
    EESMTrigger() { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_CONSTRUCT, 0, 0, 0, 0); }
    // Destroys the trigger, deleting all its condition and effect entries
    ~EESMTrigger() { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_DESTROY, 0, 0, 0, 0); }
    // Reads the trigger's settings, condition entries and effect entries (id -1 becomes 0); returns the first error
    // (ee_scenario save.rs trigger_load)
    // +0x00 0x1c9d56
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_trigger_io(this, &inFile, false); }
    // Writes the trigger's settings, condition entries and effect entries; returns the first error
    // (ee_scenario save.rs trigger_save)
    // +0x14 0x1ca0b3
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_trigger_io(this, &outFile, true); }
    // Copies a trigger: texts, entries, the flags, the condition duration and the id
    void Copy(const EESMTrigger& inTrigger) { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_COPY, 0, (int64_t)(intptr_t)&inTrigger, 0, 0); }
    // Returns condition entry inIndex, or NULL past the end
    // 0x1c9d0e (0 past the end)
    EESMTConditionWrapper* GetCondition(unsigned long inIndex) { return (EESMTConditionWrapper*)(intptr_t)ee_scenario_trigger_member(this, EE_SCENARIO_TRM_GET, 0, (int64_t)inIndex, 0, 0); }
    // Returns effect entry inIndex, or NULL past the end
    // 0x1c9d32 (0 past the end)
    EESMTEffectWrapper* GetEffect(unsigned long inIndex) { return (EESMTEffectWrapper*)(intptr_t)ee_scenario_trigger_member(this, EE_SCENARIO_TRM_GET, 1, (int64_t)inIndex, 0, 0); }
    // Fires the trigger when its AND/OR condition chain has held for the set duration: queues all effects, or one random
    // OR-separated group, at their delays, and turns a non-looping trigger off
    // (EEScenarioManager::Update 0x1b20e6)
    // (trigger.rs evaluate over the entries, trigger_members.rs; entry.rs adds the simulation capture's SMTE record)
    // 0x1c9ab5 (runs the effects when the conditions hold; provisional name)
    void Evaluate(float inTime) { ee_scenario_en_trigger_evaluate(this, inTime); }
    // Sets effect entry inIndex's operator (appending an entry after the last when it gets one)
    // 0x1ca3c1 (appends a wrapper after the last)
    void SetEffectOperator(unsigned long inIndex, long inType) { ee_scenario_trigger_member(this, EE_SCENARIO_TRM_SET_OPERATOR, 1, (int64_t)inIndex, inType, 0); }
    // inline (COMDAT 0x196dff after its first user, EESETSTrigger.cpp; the effect count is ICF-folded into 0x88ce9)
    // (provisional names)
    unsigned long GetConditionCount() { return this->mConditions.size(); }
    unsigned long GetEffectCount() { return this->mEffects.size(); }
    // inline const accessors (Save 0x1ca0b3 reads the wrappers through the const operator[]: lea; mov [eax])
    // (provisional names)
    EESMTConditionWrapper* GetConditionAt(unsigned long inIndex) const { return this->mConditions[inIndex]; }
    EESMTEffectWrapper* GetEffectAt(unsigned long inIndex) const { return this->mEffects[inIndex]; }
    void SetName(const UWideString& inName) { this->mName = inName; }
    void SetText(const UWideString& inText) { this->mText = inText; }
    void SetConditionDuration(unsigned long inValue) { this->mConditionDuration = inValue; }
    void SetIsOn(bool inValue) { this->mIsOn = inValue; }
    void SetIsLooping(bool inValue) { this->mIsLooping = inValue; }
    void SetIsInCinematic(bool inValue) { this->mIsInCinematic = inValue; }

    UWideString mName;                      // +0x04
    UWideString mText;                      // +0x18
    std::vector<EESMTConditionWrapper*> mConditions;    // +0x2c
    std::vector<EESMTEffectWrapper*> mEffects;  // +0x3c
};

#endif
