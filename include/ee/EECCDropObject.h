// C:\EEX\Empire Earth\Calamities\EECCDropObject.h (RECONSTRUCTION; file name inferred). The drop-object calamity
// Layout proven: EECCDropObject sizeof 0x3c = its new-site size, ctor 0xafef9 exact in EECCDropObject.cpp. Member names
// are provisional.
// class: a unit of a database class dropped next to the target unit, attacking it (vtable 0x43c90c).
// EECCDropObject.cpp is 0xafef9..0xb037f. sizeof 0x3c. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCDROPOBJECT_H
#define EE_EECCDROPOBJECT_H

class EEGraphicEffect;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCDropObject : public EEUnitCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
};

#endif
