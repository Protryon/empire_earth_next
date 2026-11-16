// C:\EEX\Empire Earth\Scenario Manager\EESMEffect.h (RECONSTRUCTION; file name inferred). One scenario trigger effect
// (RTTI EESMEffect, a root class; vtable 0x446068, nine slots +0x00..+0x20 at
// 0x1bfa67/0x1bef17/0x1bf508/0x1bf2b3/0x1bf54c/0x1bf384/0x1bf5c3/ 0x1bf4c9/0x1c0134; sizeof 0x194 from the `new`
// sites). Layout from the ctor 0x1bb5a5 (vector<long> at +0x04, nine UWideStrings +0x14..+0xc8, then Reset 0x1bb65a)
// and the dtor 0x1bb609 (not virtual; stores the vptr). The slots'
// signatures are unproven (declared without arguments) except Load/Save/remaps. Member types from Load 0x1bfa67 / Save
// 0x1c0134 (FSFile Read/Write slots). Member names by offset (provisional name). Requires lle/UWideString.h.
#ifndef EE_EESMEFFECT_H
#define EE_EESMEFFECT_H

#include <vector>
#include "lle/UWideString.h"
#include "rust/ee_scenario.h"

// the numbers (mPlayerAttribute .. mB193, their order and types) are EeScenarioEffectFields (ee_scenario fields.rs):
// their defaults, saves and index upkeep are Rust's
class EESMEffect : public EeScenarioEffectFields {
public:
    EESMEffect();
    ~EESMEffect();
    void Copy(const EESMEffect& inEffect);  // 0x1bc60f (field-wise; EEScenarioManager 0x1b000d; provisional name)
    void SetPlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount);
    void SetPlayerAttributeUncaptured(long inPlayer, unsigned long inAttribute, long inMode, long inAmount);  // (+ SMPA)
    // 0x1bd5eb (6.4 KB: the effect's execution, its 17 helpers' bodies with it: ee_scenario effect.rs, effect_ai.rs,
    // task.rs over EESMEffectWorld; provisional name)
    void Execute(bool inIsCinematic);
    void ExecuteUncaptured(bool inIsCinematic);  // Execute's body (Execute adds the capture's SMEX record)
    void CreateUnits();                    // 0x1bd099: effect execution helper, original this/no args
    void Reset();                           // 0x1bb65a (the ctor's tail; assigns "" to the strings, the numbers below)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1bfa67
    virtual void ExpandTokens(const UWideString& inText, UWideString& outText);            // +0x04 0x1bef17
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x20 0x1c0134
    // inline accessor (the effect editors' std::find takes its value as a temporary: 0x184bd6 exact; provisional name)
    long GetPlacedEffect() const { return this->mPlacedEffect; }
    // inline accessors (EESETSEffect's Virtual24/Virtual2C: EESMCondition's shape; provisional names)
    const UWideString& GetName() const { return this->mName; }
    const UWideString& GetText() const { return this->mText; }
    // inline (out-of-line COMDAT 0x1c577c `lea eax, [ecx + 0x8c]`: SetClassName's two SetName copies; provisional name)
    const UWideString& GetNewName() const { return this->mNewName; }
    unsigned long GetType() const { return this->mType; }
    unsigned long GetID() const { return this->mID; }
    float GetFloatValue() const { return this->mFloatValue; }  // inline (Copy 0x5bc60f copies it with fld/fstp; provisional name)
    long GetIntValue1() const { return this->mIntValue1; }  // inline (0x58d4d5 passes it to std::find as an rvalue temporary; provisional name)
    long GetIntValue2() const { return this->mIntValue2; }  // inline (0x58d4d5, as GetIntValue1; provisional name)
    unsigned long GetPlayerAction() const { return this->mPlayerAction; }
    bool GetIsAttack() const { return this->mIsAttack; }
    void SetName(const UWideString& inName) { this->mName = inName; }
    void SetText(const UWideString& inText) { this->mText = inText; }
    void SetType(unsigned long inType) { this->mType = inType; }

    std::vector<unsigned long> mObjects;    // +0x04 (`_Buy` 0xe7d2e, `_Tidy` 0x2002c7; Load/Save: unsigned long elements)
    UWideString mName;                      // +0x14
    UWideString mText;                      // +0x28
    UWideString mS3C;                       // +0x3c (not assigned by Reset)
    UWideString mMessage;                   // +0x50
    UWideString mS64;                       // +0x64
    UWideString mS78;                       // +0x78
    UWideString mNewName;                   // +0x8c
    UWideString mFileName;                  // +0xa0
    UWideString mDescription;               // +0xb4
};

#endif
