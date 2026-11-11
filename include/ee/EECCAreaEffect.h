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
    EECCAreaEffect();                       // 0xaf1ce
    virtual ~EECCAreaEffect();              // 0xaf216
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xaf260
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xaf3a8
    virtual EEContagionFunctor* GetContagionFunctor();           // +0x0c (ICF-folded 0x12c734: mFunctor)
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xaf521
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xaf5f2 (loads the effects and the sound)
    virtual void UnloadMedia(EEPlayer* inPlayer);  // +0x34 0xaf632 (unloads them)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xaf669
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // +0x4c 0xaf8a3: whether the area around (inX, inY) is on the map
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);
    // +0x50, new here (ICF-folded 0xac9f3: false; ApplyEffect asks it for the caster's own units)
    virtual bool AffectsOwnUnits();

protected:
    // 0xaf411 (provisional name): the map tiles in mL44 of the calamity's target (minus the immune ones when mB25)
    void GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles);
    void ApplyEffect(EECalamity* inCalamity);   // 0xaf726 (provisional name: the effect once its delay is over)
    // Provisional name: the caster's position, else the target location (ICF-folded into 0xb1a52)
    void GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation);

    EEGraphicEffect* mGraphicEffect1;       // +0x28 (referenced; record +0x94)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (referenced; record +0x98)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (referenced; record +0x9c)
    EESoundObject* mSound;                  // +0x34 (record +0xd4)
    EEAreaEffectFunctor* mFunctor;          // +0x38 (owned: deleted by the dtor)
    long mDamageFactor;                     // +0x3c (record +0xb8)
    unsigned long mEffect1Duration;         // +0x40 (mGraphicEffect1's +0x78; 600 without one)
    unsigned long mRadius;                  // +0x44 (record +0xb4; the radius in tiles: unsigned, fild qword and
                                            // (r + 1) >> 1)
};

#endif
