// C:\EEX\Empire Earth\Calamities\EECCFire.h (RECONSTRUCTION; file name inferred). The fire calamity class:
// Layout proven: EECCFire sizeof 0x48 = its new-site size, ctor 0xb0e2e exact in EECCFire.cpp. Member names are
// provisional.
// spreads from the target unit through a contagion helper (vtable 0x43c9c4). EECCFire.cpp is 0xb0e2e..0xb114a.
// sizeof 0x48. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCFIRE_H
#define EE_EECCFIRE_H

class EEFireFunctor;
class EEGraphicEffect;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCFire : public EEUnitCalamityClass {
public:
    EECCFire();                             // 0xb0e2e
    virtual ~EECCFire();                    // 0xb0e80
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb0ebd
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb0f60
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual EEContagionFunctor* GetContagionFunctor();           // +0x0c (ICF-folded 0x12c730: mFunctor)
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb0fa7
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb105f (its contagion helper)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb109a
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb10c9
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb10f4
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb113b

protected:
    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; on the target at Start)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; the functor's)
    EESoundObject* mSound;                  // +0x30 (record +0xd4)
    EEFireFunctor* mFunctor;                // +0x34 (owned)
    long mBurn;                             // +0x38 (record +0xb8)
    long mDamage;                           // +0x3c (record +0xbc)
    float mRadius;                          // +0x40 (record +0x64)
    float mChance;                          // +0x44 (record +0x68)
};

#endif
