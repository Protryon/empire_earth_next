// C:\EEX\Empire Earth\Calamities\EECCSpecialLocation.h (RECONSTRUCTION; file name inferred). The
// Layout proven: EECCSpecialLocation sizeof 0x28 = its new-site size, ctor 0xb338d exact in EECCSpecialLocation.cpp.
// Member names are provisional.
// special-location calamity class: the caster goes to the target location (vtable 0x43cdec). EECCSpecialLocation.cpp
// is 0xb338d..0xb3494. sizeof 0x28. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCSPECIALLOCATION_H
#define EE_EECCSPECIALLOCATION_H

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCSpecialLocation : public EELocationCalamityClass {
public:
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // +0x50, new here, 0xb3443: EECalamityClass::CanCastAt with an area-effect refusal (2) counting as fine
    virtual long CanCastIgnoringAreaEffects(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                           bool inCheck, EEUnit* inTargetUnit);
};

#endif
