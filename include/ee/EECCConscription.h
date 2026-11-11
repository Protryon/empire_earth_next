// C:\EEX\Empire Earth\Calamities\EECCConscription.h (RECONSTRUCTION; file name inferred). The conscription
// Layout proven: EECCConscription sizeof 0x28 = its new-site size, ctor 0xafdd3 exact in EECCConscription.cpp. Member
// names are provisional.
// calamity class (vtable 0x43c8ac). EECCConscription.cpp is 0xafdd3..0xafef9. sizeof 0x28 (no fields of its own).
// Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCCONSCRIPTION_H
#define EE_EECCCONSCRIPTION_H

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCConscription : public EEUnitCalamityClass {
public:
    EECCConscription();                     // 0xafdd3
    virtual ~EECCConscription();            // 0xafe01
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xafe0c
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 (empty: ICF-folded 0x157269)
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xafea3
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xafeaf: 7 (over at once)
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xafeb5
};

#endif
