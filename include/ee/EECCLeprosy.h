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
    EECCLeprosy();                          // 0xb18e2
    virtual ~EECCLeprosy();                 // 0xb1928
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb1958
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb1a07
    virtual EEContagionFunctor* GetContagionFunctor();           // +0x0c 0xb1a4e: mFunctor
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb1b7c
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb1c06
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb1c14
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c (empty: ICF-folded 0x157269)
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb1c2f
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb1c4e
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb1f32

protected:
    // the same members as EECCAreaEffect's (provisional names)
    void GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation);   // 0xb1a52
    void GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles);   // 0xb1a97
    void ShowEffects(EECalamity* inCalamity);   // 0xb1cca: an effect from the source to every affected tile
    void ApplyEffect(EECalamity* inCalamity);   // 0xb1dd3: the units around the target join the contagion

    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94)
    EESoundObject* mSound;                  // +0x2c (record +0xd4)
    EELeprosyFunctor* mFunctor;             // +0x30 (owned)
    long mDamage;                           // +0x34 (record +0xb8: the functor's first argument)
    unsigned long mEffect1Duration;         // +0x38 (mGraphicEffect1's duration: the infection's delay)
    unsigned long mRadius;                  // +0x3c (record +0xb4) in tiles
};

#endif
