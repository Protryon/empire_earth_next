// C:\EEX\Empire Earth\Calamities\EECCLogicBomb.h (RECONSTRUCTION; file name inferred). The logic bomb calamity
// Layout proven: EECCLogicBomb sizeof 0x40 = its new-site size, ctor 0xb2034 exact in EECCLogicBomb.cpp. Member names
// are provisional.
// class: effects on the target unit at two times, then the target is shut down (vtable 0x43cb28; RTTI EECCLogicBomb
// > EEUnitCalamityClass > EECalamityClass). EECCLogicBomb.cpp is 0xb2034..0xb2634. sizeof 0x40. Field names
// are provisional; +0x28..+0x34 as the area effects' (Virtual28/34 are ICF-folded into theirs). Requires
// ee/EEUnitCalamityClass.h.
#ifndef EE_EECCLOGICBOMB_H
#define EE_EECCLOGICBOMB_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCLogicBomb : public EEUnitCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
};

#endif
