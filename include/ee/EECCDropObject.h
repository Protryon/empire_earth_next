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
    EECCDropObject();                       // 0xafef9
    virtual ~EECCDropObject();              // 0xaff35
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xaff6e
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb004f (the drop)
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb0252
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    // +0x28/+0x34: the same bodies as the area effects' (same field layout): ICF-folded into 0xaf5f2/0xaf632
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);
    virtual void UnloadMedia(EEPlayer* inPlayer);
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb02c7
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb0310

protected:
    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; played at the target on Start)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (record +0x9c)
    EESoundObject* mSound;                  // +0x34 (record +0xd4)
    long mUnitClassIndex;                   // +0x38 (record +0xb0: gUnitClassTable index) (provisional name)
};

#endif
