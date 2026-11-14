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
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
};

#endif
