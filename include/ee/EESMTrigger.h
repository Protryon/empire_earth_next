// C:\EEX\Empire Earth\Scenario Manager\EESMTrigger.h (RECONSTRUCTION; file name inferred).
// One scenario trigger (RTTI EESMTrigger, a root class; vtable 0x4460e4, six slots). Layout from the ctor 0x1c97b7
// (two UWideStrings, two vectors of wrapper pointers, then the scalars) and the dtor 0x1c98dd (not virtual; stores
// the vptr: user-declared; deletes every wrapper, non-virtually). EEScenarioManager keeps them in mTriggers.
// Slots: +0x00 Load 0x1c9d56, +0x04/+0x0c remap (EESMIDMap&) through the condition/effect wrappers' +0x00,
// +0x08/+0x10 renumber them (wrappers' +0x04), +0x14 Save 0x1ca0b3. Member and slot names by offset (provisional name).
// Requires lle/UWideString.h, lle/FSFile.h, <vector>.
#ifndef EE_EESMTRIGGER_H
#define EE_EESMTRIGGER_H

#include <vector>
#include "ee/EESMIDMap.h"
#include "ee/EESMTConditionWrapper.h"
#include "ee/EESMTEffectWrapper.h"

class EESMTrigger {
public:
    EESMTrigger();                          // 0x1c97b7
    ~EESMTrigger();                         // 0x1c98dd
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1c9d56
    virtual void RemapConditions(EESMIDMap& inMap);  // +0x04 0x1ca045 (condition wrappers' +0x00)
    virtual void RenumberConditions(unsigned long inOld, unsigned long inNew);  // +0x08 0x1c9fcd (condition wrappers' +0x04)
    virtual void RemapEffects(EESMIDMap& inMap);  // +0x0c 0x1ca07c (effect wrappers' +0x00)
    virtual void RenumberEffects(unsigned long inOld, unsigned long inNew);  // +0x10 0x1ca009 (effect wrappers' +0x04)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x14 0x1ca0b3
    void Copy(const EESMTrigger& inTrigger);    // 0x1c9966 (provisional name)
    void DeleteCondition(unsigned long inIndex);    // 0x1c99c7 (provisional name)
    void DeleteEffect(unsigned long inIndex);   // 0x1c9a3e (provisional name)
    EESMTConditionWrapper* GetCondition(unsigned long inIndex);  // 0x1c9d0e (0 past the end)
    EESMTEffectWrapper* GetEffect(unsigned long inIndex);        // 0x1c9d32 (0 past the end)
    void Evaluate(float inTime);            // 0x1c9ab5 (runs the effects when the conditions hold; provisional name)
    void SetCondition(unsigned long inIndex, bool inNot, unsigned long inCondition);   // 0x1ca29d
    void SetConditionOperator(unsigned long inIndex, long inOperator);  // 0x1ca2c3 (appends a wrapper after the last)
    void CopyConditions(const std::vector<EESMTConditionWrapper*>& inConditions);  // 0x1ca331
    void SetEffect(unsigned long inIndex, long inDelay, unsigned long inEffect);  // 0x1ca39b
    void SetEffectOperator(unsigned long inIndex, long inType);  // 0x1ca3c1 (appends a wrapper after the last)
    void CopyEffects(const std::vector<EESMTEffectWrapper*>& inEffects);    // 0x1ca42f
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
    unsigned long mConditionDuration;                   // +0x4c
    unsigned long mConditionStartTime;                  // +0x50
    unsigned long mID;                                  // +0x54
    bool mIsOn;                             // +0x58 (ctor: true)
    bool mHasFired;                         // +0x59
    bool mIsLooping;                        // +0x5a
    bool mIsInCinematic;                    // +0x5b
};

#endif
