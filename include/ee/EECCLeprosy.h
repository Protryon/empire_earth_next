// C:\EEX\Empire Earth\Calamities\EECCLeprosy.h (RECONSTRUCTION; file name inferred). The leprosy calamity
// Layout proven: EECCLeprosy sizeof 0x40 = its new-site size, ctor 0xb18e2 exact in EECCLeprosy.cpp. Member names are
// provisional.
// class: a contagion over the units around the target location (vtable 0x43ca74; RTTI EECCLeprosy >
// EELocationCalamityClass > EECalamityClass). EECCLeprosy.cpp is 0xb18e2..0xb1f4f (__FILE__ 0x4b9c14). sizeof 0x40.
// Its GetSourceLocation, Reset and Virtual34 are the ICF representatives of EECCAreaEffect's, EECCMorale's and
// EECCCloaking's identical members. Field names are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCLEPROSY_H
#define EE_EECCLEPROSY_H

class EEGraphicEffect;
class EELeprosyFunctor;
class EESoundObject;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCLeprosy : public EELocationCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    // the same members as EECCAreaEffect's (provisional names)
    void GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation);   // 0xb1a52
    void GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles);   // 0xb1a97
    void ShowEffects(EECalamity* inCalamity);   // 0xb1cca: an effect from the source to every affected tile
    void ApplyEffect(EECalamity* inCalamity);   // 0xb1dd3: the units around the target join the contagion
};

#endif
