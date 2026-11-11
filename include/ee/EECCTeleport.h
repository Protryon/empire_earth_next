// C:\EEX\Empire Earth\Calamities\EECCTeleport.h (RECONSTRUCTION; file name inferred). The teleport calamity
// Layout proven: EECCTeleport sizeof 0x44 = its new-site size, ctor 0xb3494 exact in EECCTeleport.cpp. Member names are
// provisional.
// class: the caster is held, then moved to the target location (vtable 0x43ce48; RTTI EECCTeleport >
// EELocationCalamityClass > EECalamityClass). EECCTeleport.cpp is 0xb3494..0xb3ade. sizeof 0x44. Field names
// are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCTELEPORT_H
#define EE_EECCTELEPORT_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCTeleport : public EELocationCalamityClass {
public:
    EECCTeleport();                         // 0xb3494
    virtual ~EECCTeleport();                // 0xb34d6
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb34fa
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb3592
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb35b8
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (ICF-folded 0xaee30: clears both bools)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb3636
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c (empty: ICF-folded 0x157269)
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb39a9
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb39d0
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb3aac

protected:
    void Teleport(EECalamity* inCalamity);  // 0xb3665 (provisional name)
    void HoldCaster(EECalamity* inCalamity);   // 0xb390b (provisional name)

public:   // the data: the prophet's teleport analyzer reads mL3C (Virtual28 0x406b24)
    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; on the caster at Start)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; on the caster at arrival)
    EESoundObject* mSound;                  // +0x30 (record +0xd4)
    EESoundObject* mSecondarySound;         // +0x34 (record +0xd8; at the teleport)
    float mDamageFactor;                    // +0x38 (record +0x64: the caster loses this share of its hit points)
    long mPlacementRange;                   // +0x3c (record +0xb4: EEFindPlacement's 7th argument)
    long mHoldTime;                         // +0x40 (mGraphicEffect1's duration, else 2000)
};

#endif
