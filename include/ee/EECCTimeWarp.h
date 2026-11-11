// C:\EEX\Empire Earth\Calamities\EECCTimeWarp.h (RECONSTRUCTION; file name inferred). The time warp calamity
// Layout proven: EECCTimeWarp sizeof 0x4c = its new-site size, ctor 0xb3b42 exact in EECCTimeWarp.cpp. Member names are
// provisional.
// class: the target unit leaves the map for a while and returns where it was (vtable 0x43cea4; RTTI EECCTimeWarp >
// EEUnitCalamityClass > EECalamityClass). EECCTimeWarp.cpp is 0xb3ade..0xb4634 (its two colour globals' init
// chains first). sizeof 0x4c. Field names are provisional. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCTIMEWARP_H
#define EE_EECCTIMEWARP_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCTimeWarp : public EEUnitCalamityClass {
public:
    EECCTimeWarp();                         // 0xb3b42
    virtual ~EECCTimeWarp();                // 0xb3b8a
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb3bd0
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb3c89
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb3d7c
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb3e36
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb3e9d
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c (empty: ICF-folded 0x157269)
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb432a
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb4361
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);  // +0x4c 0xb45fa

protected:
    // 0xb3ede: the departure (inDeparting) or arrival effect at inLocation and the secondary sound
    void PlayEffect(GE3DPoint& inLocation, EECalamity* inCalamity, bool inDeparting);
    void Return(EECalamity* inCalamity);    // 0xb3fc9: the unit comes back at its tile (provisional name)
    void Depart(EECalamity* inCalamity);    // 0xb41dd: the unit leaves (provisional name)

    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; on the target at Start)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; at the departure)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (record +0x9c; at the arrival)
    EEGraphicEffect* mGraphicEffect4;       // +0x34 (record +0xa0; placed at the departure and the return)
    EESoundObject* mSound;                  // +0x38 (record +0xd4)
    EESoundObject* mSecondarySound;         // +0x3c (record +0xd8)
    long mPlacementRange;                   // +0x40 (record +0xb4: EEFindPlacement's 7th argument)
    long mDepartTime;                       // +0x44 (mGraphicEffect1's duration)
    long mAwayTime;                         // +0x48 (record +0xb8, then mGraphicEffect2's duration, else 3000)
};

#endif
