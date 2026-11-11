// C:\EEX\Empire Earth\Calamities\EECCMorale.h (RECONSTRUCTION; file name inferred). The morale calamity class:
// Layout proven: EECCMorale sizeof 0x3c = its new-site size, ctor 0xb2ba3 exact in EECCMorale.cpp. Member names are
// provisional.
// a morale bonus or penalty on the target unit while it lasts (vtable 0x43cce8). EECCMorale.cpp is 0xb2ba3..0xb2f31.
// sizeof 0x3c. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCMORALE_H
#define EE_EECCMORALE_H

class EEGraphicEffect;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCMorale : public EEUnitCalamityClass {
public:
    EECCMorale();                           // 0xb2ba3
    virtual ~EECCMorale();                  // 0xb2bdf
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb2bfe
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb2cad
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb2d13
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (ICF-folded 0xb1c06: mBoolPlaceHolder1 off)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb2d6d
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb2ed2
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb2d9c
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb2efd

protected:
    EEGraphicEffect* mGraphicEffect;        // +0x28 (record +0x94; from caster to target at Start)
    EESoundObject* mSound;                  // +0x2c (record +0xd4)
    EESoundObject* mSecondarySound;         // +0x30 (record +0xd8)
    long mEffectDuration;                   // +0x34 (mGraphicEffect's duration: the morale starts after it)
    bool mPositive;                         // +0x38 (record +0xcd: the +0x2c8 counter, else +0x2cc) (provisional name)
};

#endif
