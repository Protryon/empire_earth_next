// C:\EEX\Empire Earth\Calamities\EECCSpecialAttack.h (RECONSTRUCTION; file name inferred). The special-attack
// Layout proven: EECCSpecialAttack sizeof 0x28 = its new-site size, ctor 0xb3217 exact in EECCSpecialAttack.cpp. Member
// names are provisional.
// calamity class: the caster attacks the target unit (vtable 0x43cd94). EECCSpecialAttack.cpp is 0xb3217..0xb338d.
// sizeof 0x28. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCSPECIALATTACK_H
#define EE_EECCSPECIALATTACK_H

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCSpecialAttack : public EEUnitCalamityClass {
public:
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // +0x3c 0xb32da: as EEUnitCalamityClass's, an area-effect refusal (2) counting as fine
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                           bool inCheck, EEUnit* inTargetUnit);
};

#endif
