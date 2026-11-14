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
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    void ShowEffect(EECalamity* inCalamity);    // 0xb3131 (Start's effect from caster to target) (provisional name)
};

#endif
