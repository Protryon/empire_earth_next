// C:\EEX\Empire Earth\Calamities\EECCCloaking.h (RECONSTRUCTION; file name inferred). The cloaking calamity class
// Layout proven: EECCCloaking sizeof 0x30 = its new-site size, ctor 0xaf8c0 exact in EECCCloaking.cpp. Member names are
// provisional.
// (vtable 0x43c854). EECCCloaking.cpp is 0xaf8c0..0xafdd3. sizeof 0x30. Requires ee/EEGlobalCalamityClass.h.
#ifndef EE_EECCCLOAKING_H
#define EE_EECCCLOAKING_H

class EEAreaEffect;
class EEGraphicEffect;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCCloaking : public EEGlobalCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool Virtual4C(unsigned long, unsigned long);    // +0x4c, new here (ICF-folded 0x2135bc: true)

protected:
    // 0xafb38 (provisional name): the caster's group of type-0x29 units within reach, each one cloaked
    void CloakGroup(EECalamity* inCalamity, EEComplexUnit* inUnit, EEAreaEffect* inEffect);
    void CloakUnit(EECalamity* inCalamity, EEComplexUnit* inUnit);   // 0xafd37 (provisional name)
};

#endif
