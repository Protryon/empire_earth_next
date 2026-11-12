// C:\EEX\Empire Earth\Calamities\EECCPlague.h (RECONSTRUCTION; file name inferred). The plague calamity class:
// Layout proven: EECCPlague sizeof 0x3c = its new-site size, ctor 0xb2f31 exact in EECCPlague.cpp. Member names are
// provisional.
// spreads from the target unit through a contagion helper (vtable 0x43cd3c). EECCPlague.cpp is 0xb2f31..0xb3217.
// sizeof 0x3c. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCPLAGUE_H
#define EE_EECCPLAGUE_H
#include "ee/EEEffectKindIDs.h"

class EEGraphicEffect;
class EEPlagueFunctor;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCPlague : public EEUnitCalamityClass {
public:
    EECCPlague();                           // 0xb2f31
    virtual ~EECCPlague();                  // 0xb2f7c
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb2fac
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb2fba
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual EEContagionFunctor* GetContagionFunctor();           // +0x0c 0xb3001
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb3005
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb30c7 (its contagion helper)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb3102
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb31c5
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb31f0
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb3208

protected:
    void ShowEffect(EECalamity* inCalamity);    // 0xb3131 (Start's effect from caster to target) (provisional name)

    EEPlagueFunctor* mFunctor;              // +0x28 (owned)
    EEGraphicEffect* mGraphicEffect;        // +0x2c (record +0x94; referenced)
    EESoundObject* mSound;                  // +0x30 (record +0xd4)
    EESoundObject* mSecondarySound;         // +0x34 (record +0xd8)
    long mDamage;                           // +0x38 (record +0xb4; functor argument)
};

#endif
