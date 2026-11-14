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
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
};

#endif
