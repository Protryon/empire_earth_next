// C:\EEX\Empire Earth\Calamities\EECCHurricane.h (RECONSTRUCTION; file name inferred). The hurricane calamity
// Layout proven: EECCHurricane sizeof 0x50 = its new-site size, ctor 0xb114a exact in EECCHurricane.cpp. Member names
// are provisional.
// class: a storm unit at the target location that damages the enemy units around it (vtable 0x43ca1c; RTTI
// EECCHurricane > EELocationCalamityClass > EECalamityClass). EECCHurricane.cpp is 0xb114a..0xb18e2. sizeof 0x50.
// Field names are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCHURRICANE_H
#define EE_EECCHURRICANE_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCHurricane : public EELocationCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    bool CreateStorm(EECalamity* inCalamity);   // 0xb1226 (provisional name)
};

#endif
