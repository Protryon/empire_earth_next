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
#include "ee/EESMIDMap.h"

class EESMEffect {
public:
    EESMEffect();                           // 0x1bb5a5
    ~EESMEffect();                          // 0x1bb609
    void Copy(const EESMEffect& inEffect);  // 0x1bc60f (field-wise; EEScenarioManager 0x1b000d; provisional name)
    void FreezeUnits(bool inFlag);          // 0x1bf7f9: toggles scenario object unit actions (provisional name)
    void SetUnitsOwner();                  // 0x1bcf61 (transfer selected units; void)
    void SetPlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount);  // 0x1bcb1f
    void FaceObject(); // original rotation effect, void ret0
    void SetClassName();                   // 0x1bc942: renames selected unit/class upgrades (provisional name)
    void KillUnits(bool inFlag);            // 0x1bf9b7: applies Virtual60 to the scenario object units (provisional name)
    void Execute(bool inIsCinematic);       // 0x1bd5eb (6.4 KB: the effect's execution; provisional name)
    void ExecuteUncaptured(bool inIsCinematic);  // Execute's body (Execute adds the capture's SMEX record)
    void BuildObject();                    // 0x1bb82f: effect execution helper, original this/no args
    void CreateAIMilitaryGroup();          // 0x1bbba1: effect execution helper, original this/no args
    void ModifyAIMilitaryTarget();         // 0x1bc333: effect execution helper, original this/no args
    void CreateUnits();                    // 0x1bd099: effect execution helper, original this/no args
    void ScriptCamera();                   // 0x1c07c4: effect execution helper, original this/no args
    void SetAIVariable();                  // 0x1c0cc9: effect execution helper, original this/no args
    void SetFavoriteUnits();               // 0x1c2507, actual same-this/no-arg effect helper
    void SetAIToggle();                    // 0x1c26da
    void SetUnitsStance();                 // 0x1c2811
    void ChangeAttribute();                // 0x1c2b3e
    void TaskUnitsToObject();              // 0x1c2f55
    void TaskUnitsToArea();                // 0x1c42d2
    void MaintainObject();                  // ICF-folded empty member 0xe123d (Execute's mAIAction 3)
    void Reset();                           // 0x1bb65a (the ctor's tail; assigns "" to the strings, the numbers below)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1bfa67
    virtual void ExpandTokens(const UWideString& inText, UWideString& outText);            // +0x04 0x1bef17
    virtual void RemapAreas(EESMIDMap& inMap);  // +0x08 0x1bf508 (remaps +0x164/+0x120/+0x12c/+0x178)
    virtual void RenumberAreas(unsigned long inOldIndex, unsigned long inNewIndex);  // +0x0c 0x1bf2b3 (renumbers Virtual08's)
    virtual void RemapObjects(EESMIDMap& inMap);  // +0x10 0x1bf54c (remaps +0x158/+0x17c/+0x168/+0x130/+0x124, mV04)
    virtual void RenumberObjects(unsigned long inOldIndex, unsigned long inNewIndex);  // +0x14 0x1bf384 (renumbers Virtual10's)
    virtual void RemapTriggers(EESMIDMap& inMap);  // +0x18 0x1bf5c3 (remaps +0x180)
    virtual void RenumberTriggers(unsigned long inOldIndex, unsigned long inNewIndex);  // +0x1c 0x1bf4c9 (renumbers +0x180)
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
    unsigned long mPlayerAttribute;                   // +0xc8
    unsigned long mDiplomacy;                         // +0xcc
    unsigned long mULD0;                              // +0xd0  // reset: = 8
    unsigned long mMessagePlayer;                     // +0xd4  // reset: = 7
    unsigned long mNewOwner;                          // +0xd8  // reset: = 7
    unsigned long mPlayer;                            // +0xdc  // reset: = 7
    unsigned long mDiplomacyPlayer;                   // +0xe0  // reset: = 8
    unsigned long mAIAction;                          // +0xe4
    unsigned long mAIAttribute;                       // +0xe8
    unsigned long mAIToggle;                          // +0xec
    unsigned long mMediaAction;                       // +0xf0
    unsigned long mULF4;                              // +0xf4
    unsigned long mPlayerAction;                      // +0xf8
    unsigned long mTriggerState;                      // +0xfc
    unsigned long mType;                               // +0x100  // reset: = 1
    unsigned long mObjectAction;                       // +0x104
    unsigned long mAttributeMode;                      // +0x108
    long mTechnology;                        // +0x10c
    float mFloatValue;                        // +0x110
    long mIntValue1;                         // +0x114
    long mIntValue2;                         // +0x118
    long mAttributePlayer;                   // +0x11c  // reset: = -1
    unsigned long mTargetArea;                         // +0x120
    unsigned long mTargetObject;                       // +0x124
    unsigned long mPlacedEffect;                       // +0x128
    unsigned long mPlaceArea;                          // +0x12c
    unsigned long mPlaceObject;                        // +0x130
    unsigned long mDaylightHour;            // +0x134 // reset: = 0xf (unsigned: StartCinematic's fild qword)
    unsigned long mCycleTime;                          // +0x138
    unsigned long mGameAction;                         // +0x13c
    unsigned long mDuration;                           // +0x140  // reset: = 0x14
    unsigned long mTextObject;              // +0x144 (unsigned: ChangeText's GetCurrentSelection(&) target)
    unsigned long mAttribute;                          // +0x148
    unsigned long mAttributeAmount;                    // +0x14c
    unsigned long mAttributeUnitType;                  // +0x150
    unsigned long mAttributeUnit;                      // +0x154  // reset: = -1
    unsigned long mObject;                             // +0x158
    unsigned long mStance;                             // +0x15c
    unsigned long mTaskAction;                         // +0x160
    unsigned long mTaskArea;                           // +0x164
    unsigned long mTaskObject;                         // +0x168
    unsigned long mCivPoints;                          // +0x16c  // reset: = 1
    unsigned long mPlayerAttributeMode;                // +0x170
    unsigned long mPlayerAttributeAmount;              // +0x174
    unsigned long mLocationArea;                       // +0x178
    unsigned long mLocationObject;                     // +0x17c
    unsigned long mTrigger;                            // +0x180
    unsigned long mID;                                 // +0x184
    bool mIsOn;                              // +0x188
    bool mUsesTargetArea;                    // +0x189
    bool mIsAttack;                          // +0x18a
    bool mUsesFavorites;                     // +0x18b
    bool mIsAddTarget;                       // +0x18c  // reset: = 1
    bool mB18D;                              // +0x18d
    bool mIsClearText;                       // +0x18e  // reset: = 1
    bool mUsesLocationArea;                  // +0x18f  // reset: = 1
    bool mUsesPercent;                       // +0x190
    bool mIsAddTechnology;                   // +0x191  // reset: = 1
    bool mUsesPlaceArea;                     // +0x192  // reset: = 1
    bool mB193;                              // +0x193
};

#endif
