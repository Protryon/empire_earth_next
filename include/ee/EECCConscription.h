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
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
};

#endif
