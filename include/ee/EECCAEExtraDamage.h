// C:\EEX\Empire Earth\Calamities\EECCAEExtraDamage.h (RECONSTRUCTION; file name inferred). The extra-damage
// Layout proven: EECCAEExtraDamage sizeof 0x4c = its new-site size, ctor 0xaebb7 exact in EECCAEExtraDamage.cpp. Member
// names are provisional.
// area-effect calamity class (vtable 0x43c734). EECCAEExtraDamage.cpp is 0xaebb7..0xaef4c. sizeof 0x4c.
// Requires ee/EECCAreaEffect.h.
#ifndef EE_EECCAEEXTRADAMAGE_H
#define EE_EECCAEEXTRADAMAGE_H

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCAEExtraDamage : public EECCAreaEffect {
public:
    EECCAEExtraDamage();
    virtual ~EECCAEExtraDamage();
    virtual bool Start(EECalamity* inCalamity);
    virtual void Stop(EECalamity* inCalamity);
    virtual void Initialize(EECalamityData* inData);
    virtual void Reset(EECalamity* inCalamity);
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

private:
    long mEffect2Time;                      // +0x48 (mGraphicEffect2's duration after mGraphicEffect1's) (provisional name)
};

#endif
