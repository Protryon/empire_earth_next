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
    EECCCloaking();                         // 0xaf8c0
    virtual ~EECCCloaking();                // 0xaf8f6
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xaf915
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xafa26
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xafa68
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xafabe
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 (ICF-folded 0xb1c2f)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xafae1
    virtual bool Virtual4C(unsigned long, unsigned long);    // +0x4c, new here (ICF-folded 0x2135bc: true)

protected:
    // 0xafb38 (provisional name): the caster's group of type-0x29 units within reach, each one cloaked
    void CloakGroup(EECalamity* inCalamity, EEComplexUnit* inUnit, EEAreaEffect* inEffect);
    void CloakUnit(EECalamity* inCalamity, EEComplexUnit* inUnit);   // 0xafd37 (provisional name)

    EEGraphicEffect* mGraphicEffect;        // +0x28 (record +0x94)
    EESoundObject* mSound;                  // +0x2c (record +0xd4)
};

#endif
