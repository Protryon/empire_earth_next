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
    EECCLogicBomb();                        // 0xb2034
    virtual ~EECCLogicBomb();               // 0xb2076
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb20af
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb221e
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb22ad
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 (ICF-folded 0xaf5f2)
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c (empty: ICF-folded 0x157269)
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 (ICF-folded 0xaf632)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb2337
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb25b4

protected:
    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; at the target on Start)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; placed, at mEffectTime2)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (record +0x9c; on the unit, at mEffectTime3)
    EESoundObject* mSound;                  // +0x34 (record +0xd4)
    unsigned long mEffectTime2;             // +0x38 (mGraphicEffect1's duration, else 600)
    unsigned long mEffectTime3;             // +0x3c (mEffectTime2 + mGraphicEffect2's duration)
};

#endif
