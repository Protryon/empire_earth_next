// C:\EEX\Empire Earth\Calamities\EECCTeleport.h (RECONSTRUCTION; file name inferred). The teleport calamity
// Layout proven: EECCTeleport sizeof 0x44 = its new-site size, ctor 0xb3494 exact in EECCTeleport.cpp. Member names are
// provisional.
// class: the caster is held, then moved to the target location (vtable 0x43ce48; RTTI EECCTeleport >
// EELocationCalamityClass > EECalamityClass). EECCTeleport.cpp is 0xb3494..0xb3ade. sizeof 0x44. Field names
// are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCTELEPORT_H
#define EE_EECCTELEPORT_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCTeleport : public EELocationCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    void Teleport(EECalamity* inCalamity);  // 0xb3665 (provisional name)
    void HoldCaster(EECalamity* inCalamity);   // 0xb390b (provisional name)
};

#endif
