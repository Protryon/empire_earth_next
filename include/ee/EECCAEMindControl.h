// C:\EEX\Empire Earth\Calamities\EECCAEMindControl.h (RECONSTRUCTION; file name inferred). The mind-control
// Layout proven: EECCAEMindControl sizeof 0x4c = its new-site size, ctor 0xaef4c exact in EECCAEMindControl.cpp. Member
// names are provisional.
// area-effect calamity class (vtable 0x43c798). EECCAEMindControl.cpp is 0xaef4c..0xaf1ce. sizeof 0x4c.
// Requires ee/EECCAreaEffect.h.
#ifndef EE_EECCAEMINDCONTROL_H
#define EE_EECCAEMINDCONTROL_H

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCAEMindControl : public EECCAreaEffect {
public:
    EECCAEMindControl();
    virtual ~EECCAEMindControl();
    virtual bool Start(EECalamity* inCalamity);
    virtual void Stop(EECalamity* inCalamity);
    virtual void Initialize(EECalamityData* inData);
    // identical to EECCAEExtraDamage's: ICF-folded into them (0xaee30, 0xaee45)
    virtual void Reset(EECalamity* inCalamity);
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // +0x50 (ICF-folded 0x18aa9: true, not EECCAreaEffect's false)
    virtual bool AffectsOwnUnits();

private:
    long mEffect2Time;                      // +0x48 (provisional name)
};

#endif
