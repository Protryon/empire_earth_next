// C:\EEX\Empire Earth\Calamities\EECCMeteorStorm.h (RECONSTRUCTION; file name inferred). The meteor storm
// Layout proven: EECCMeteorStorm sizeof 0x3c = its new-site size, ctor 0xb2634 exact in EECCMeteorStorm.cpp. Member
// names are provisional.
// calamity class: meteors of one unit class fall around the target location at random intervals (vtable 0x43cb90;
// RTTI EECCMeteorStorm > EELocationCalamityClass > EECalamityClass). EECCMeteorStorm.cpp is 0xb2634..0xb2ba3. sizeof
// 0x3c (EECalamityManager's new). Field names are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCMETEORSTORM_H
#define EE_EECCMETEORSTORM_H

class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCMeteorStorm : public EELocationCalamityClass {
public:
    EECCMeteorStorm();                      // 0xb2634
    virtual ~EECCMeteorStorm();             // 0xb2662
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb266d
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 (empty: ICF-folded 0x157269)
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb2684
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb26d0
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c 0xb2700: plays the sound at the target
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY);       // (the volcano reads inUnit->mPlayer) +0x30 (empty: ICF-folded 0xac98e)
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb277a
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb2abe
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb2b62

protected:
    bool LaunchMeteor(EECalamity* inCalamity);   // 0xb2795 (provisional name)

    EESoundObject* mSound;                  // +0x28 (record +0xd4)
    float mMinimumInterval;                 // +0x2c (record +0xb8) seconds between meteors
    float mMaximumInterval;                 // +0x30 (record +0xbc)
    float mRadius;                          // +0x34 (record +0x64) the spread around the target
    long mUnitClassIndex;                   // +0x38 (record +0xb0: gUnitClassTable index, the meteor)
};

#endif
