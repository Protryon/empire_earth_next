// C:\EEX\Empire Earth\Calamities\EECalamity.h (RECONSTRUCTION; file name inferred). One active calamity (a cast or
// triggered event: plague, fire, mind control, ...): where it is, who cast it, its clock, and the effect and sound
// handles of its class's presentation. EECalamity.cpp is 0xacb75..0xadcd9 (its 21 save keys' init chains first;
// vtable 0x43c698, bases FSRestorable).
// Field names follow EECalamity's out-of-synch dump (0xad377, "Calamity Target Location X: %6.6f", ...) and its save
// keys ("TargetUnitStringID", "TimeAliveStringID", "EffectHandleStringID", ...); offsets proven by the ctor, Reset,
// Load and Save. Requires lle/LowLevelEngine.h, lle/FSRestorable.h, lle/GE3DPoint.h, <stdio.h> (FILE),
// ee/EEEffectManager.h (EEEffectHandle).
#ifndef EE_EECALAMITY_H
#define EE_EECALAMITY_H

#include "lle/U2DPoint.h"
#include "rust/sim/tech.h"   // EeCalamityFields (ee_tech calamity_record.rs)
#include "rust/game/gamecpp.h"
class EECalamityClass;
class EEUnit;
class SESMInstanceNode;

// New (port): the plain members are the Rust record EeCalamityFields (ee_tech calamity_record.rs: Reset's numbers,
// the checksum, Load and Save); the members below stay C++
class EECalamity : public FSRestorable, public EeCalamityFields {
public:
    EECalamity() : mEffectGeneration(6), mTilePlaceHolder1(0, 0) { ee_gamecpp_calamity_construct(this, NULL, NULL, false); }
    EECalamity(EECalamityClass* inClass, EEUnit* inCasterUnit) : mEffectGeneration(6), mTilePlaceHolder1(0, 0) { ee_gamecpp_calamity_construct(this, inClass, inCasterUnit, true); }
    // +0x04 0xad618
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_calamity_io(this, &inFile, true); }
    // +0x0c 0xad85e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_calamity_io(this, &outFile, false); }
    // +0x10 0xad218
    virtual ~EECalamity() { ee_gamecpp_calamity_destruct(this); }
    // +0x14 0xad377 (provisional name)
    virtual void DumpOutOfSynch(FILE* inFile) { ee_gamecpp_calamity_dump(this, inFile); }
    // +0x18 (empty: ICF-folded 0x157269)
    virtual void Virtual18(unsigned long) {  }
    // +0x1c 0xad50f: the sum of the dumped fields (provisional name)
    virtual long GetOutOfSynchChecksum() { return ee_gamecpp_calamity_checksum(this); }
    // +0x20 0xadaa3: mClass->Virtual2C(this)
    virtual void PlaySound() { ee_gamecpp_calamity_play_sound(this); }

    // 0xad267: stops mSound (provisional name)
    void StopSound() { ee_gamecpp_calamity_stop_sound(this, false); }
    void ReleaseSound() { ee_gamecpp_calamity_release_sound(this, false); }
    void ReleaseSecondarySound() { ee_gamecpp_calamity_release_sound(this, true); }
    void Reset() { ee_gamecpp_calamity_reset(this); }
    // 0xad5bb: the whole discrete updates in mTimeElapsedSinceLastUpdate (keeps the remainder) (provisional name)
    unsigned long GetDiscreteUpdates() { return ee_gamecpp_calamity_discrete_updates(this); }
    // 0xad5fc: mTargetUnit (0 and false if none)
    bool GetTargetUnit(EEUnit*& outUnit) { return (outUnit = this->mTargetUnit) != NULL; }
    // 0xadab3 (keeps a reference) (provisional name)
    void SetCasterUnit(EEUnit* inUnit) { ee_gamecpp_calamity_set_unit(this, inUnit, false); }
    // 0xadae7: stops the old one, keeps a reference (provisional name)
    void SetSound(SESMInstanceNode* inSound) { ee_gamecpp_calamity_set_sound(this, inSound, false); }
    void SetSecondarySound(SESMInstanceNode* inSound) { ee_gamecpp_calamity_set_sound(this, inSound, true); }
    // 0xadb1b (keeps a reference) (provisional name)
    void SetTargetUnit(EEUnit* inUnit) { ee_gamecpp_calamity_set_unit(this, inUnit, true); }
    // out of line in another unit (0xdfd80; EECalamity.cpp calls it): stops mSecondarySound (provisional name)
    void StopSecondarySound() { ee_gamecpp_calamity_stop_sound(this, true); }
    void SetEffectHandle(const EEEffectHandle& inHandle) { this->mEffectHandle = inHandle; }
    float GetTimeAlive() const { return this->mTimeAlive; }  // inline (compared as an FP value, not a memory operand)
    unsigned long GetPlayerNumber() const { return this->mPlayerNumber; }  // inline (provisional name; Update 0xb0a60: with IsCarried, theUnits[i] op+ refused as in the original, 57 -> 68)
    long GetIntPlaceHolder1() const { return this->mIntPlaceHolder1; }  // inline (provisional name; Update 0xb0a60 68 -> 69)
    float GetHeight() const { return this->mHeight; }  // inline (the hurricane's Start loads it as an FP value)
    // inline: a nested GE3DPoint::operator= the budget refuses first (the logic bomb's Update and the meteor storm call
    // __imp_??4GE3DPoint here, their smaller Starts expand it)
    void GetTargetLocation(GE3DPoint& outLocation) const { outLocation = this->mTargetLocation; }
    // inline (inferred); its nested GERectangle::operator= is refused in CreateSourceEffect (0xb0409 79.8 -> 93.5)
    void SetRectangle(const GERectangle& inRectangle) { this->mRectangle = inRectangle; }
    // inline: the meteor storm's Update loads them as FP values (`fld [5c]; fld st1; fcompp`)
    float GetFloatPlaceHolder1() const { return this->mFloatPlaceHolder1; }
    void SetFloatPlaceHolder2(float inValue) { this->mFloatPlaceHolder2 = inValue; }
    // inline: the cloaking calamity loads the caster into a register before its dynamic_cast (0xaf920)
    EEUnit* GetCasterUnit() const { return this->mCasterUnit; }

    GERectangle mRectangle;                 // +0x04 (saved first, no key; provisional name)
    GE3DPoint mTargetLocation;              // +0x14
    GE3DPoint mShakeLocation;               // +0x20 (saved after the target location; provisional name)
    std::vector<long> mEffectGeneration;    // +0x2c six entries (key "EffectGenerationStringID")
    U2DPoint<unsigned long> mTilePlaceHolder1;  // +0x3c (Load reads a long into a temporary, Save writes a (long) copy)
    EEUnit* mTargetUnit;                    // +0x44 (GetTargetUnit 0xad5fc)
    EEUnit* mCasterUnit;                    // +0x48 (EEEffectManager::CreateEffect's unit; the unit-calamity
                                            // classes read its +0x18 and +0x2c; provisional name)
    EECalamityClass* mClass;                // +0x4c (dump: "Class is %d" = mClass->mClassEnum)
    SESMInstanceNode* mSound;               // +0x50 (SetSound/StopSound)
    SESMInstanceNode* mSecondarySound;      // +0x54 (EECalamityClass +0x48 picks one of the two by its bool)
    EEEffectHandle mEffectHandle;           // +0x74 (EEEffectManager handle; reset to kNoEffectHandle)

    // the effect generations' save key (FSLoadVector's; the other twenty keys of the original's init chains
    // 0xacb75..0xad0f4 were never read)
    static const UString mEffectGenerationStringID;
};

#endif
