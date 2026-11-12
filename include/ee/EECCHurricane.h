// C:\EEX\Empire Earth\Calamities\EECCHurricane.h (RECONSTRUCTION; file name inferred). The hurricane calamity
// Layout proven: EECCHurricane sizeof 0x50 = its new-site size, ctor 0xb114a exact in EECCHurricane.cpp. Member names
// are provisional.
// class: a storm unit at the target location that damages the enemy units around it (vtable 0x43ca1c; RTTI
// EECCHurricane > EELocationCalamityClass > EECalamityClass). EECCHurricane.cpp is 0xb114a..0xb18e2. sizeof 0x50.
// Field names are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCHURRICANE_H
#define EE_EECCHURRICANE_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCHurricane : public EELocationCalamityClass {
public:
    EECCHurricane();                        // 0xb114a
    virtual ~EECCHurricane();               // 0xb1186
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb11bf
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb12d4
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb1324
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb13be
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb13c9
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c 0xb141f: starts the sound
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb1480
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb14c3
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb17cb

protected:
    bool CreateStorm(EECalamity* inCalamity);   // 0xb1226 (provisional name)

    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; at the location on Start; its duration delays the storm)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; on the storm unit; its height is the calamity's)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (record +0x9c; on each damaged unit)
    EESoundObject* mSound;                  // +0x34 (record +0xd4)
    long mDamage;                           // +0x38 (record +0xb4) per tick
    unsigned long mTickTime;                // +0x3c (record +0xb8)
    long mArea;                             // +0x40 (record +0xc8: EEMap::IsAreaOnMap's radius)
    long mUnitClassIndex;                   // +0x44 (record +0xb0: gUnitClassTable index, the storm)
    float mRadius;                          // +0x48 (record +0x64)
    bool mOverWater;                        // +0x4c (record +0xcd: the location must be EEWorld::IsTileBlocked)
};

#endif
