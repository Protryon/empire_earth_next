// C:\EEX\Empire Earth\Calamities\EECCAreaEffect.h (RECONSTRUCTION; file name inferred). Base of the calamity
// Layout proven: EECCAreaEffect sizeof 0x48 = its new-site size, ctor 0xaf1ce exact in EECCAreaEffect.cpp. Member names
// are provisional.
// classes that apply an area effect through a contagion functor (extra damage, mind control). EECCAreaEffect.cpp is
// 0xaf1ce..0xaf8c0 (ctor 0xaf1ce, dtor 0xaf216, vtable 0x43c7f8). sizeof 0x48 (the ctor zeroes +0x28..+0x44).
// Field names are provisional. Requires ee/EELocationCalamityClass.h, <vector>.
#ifndef EE_EECCAREAEFFECT_H
#define EE_EECCAREAEFFECT_H

class EEAreaEffectFunctor;
class EEGraphicEffect;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCAreaEffect : public EELocationCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // +0x50, new here (ICF-folded 0xac9f3: false; ApplyEffect asks it for the caster's own units)
    virtual bool AffectsOwnUnits();

protected:
    // 0xaf411 (provisional name): the map tiles in mL44 of the calamity's target (minus the immune ones when mB25)
    void GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles);
    void ApplyEffect(EECalamity* inCalamity);   // 0xaf726 (provisional name: the effect once its delay is over)
    // Provisional name: the caster's position, else the target location (ICF-folded into 0xb1a52)
    void GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation);
};

#endif
