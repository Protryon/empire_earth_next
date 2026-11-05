// C:\EEX\Empire Earth\Calamities\EECCEarthquake.h (RECONSTRUCTION; file name inferred). The earthquake calamity
// Layout proven: EECCEarthquake sizeof 0x6c = its new-site size, ctor 0xb037f exact in EECCEarthquake.cpp. Member names
// are provisional.
// class: cracks spread from the target location, the ground shakes (camera) and the units of the caster's continent
// inside the growing radius take damage per tick (vtable 0x43c964; RTTI EECCEarthquake > EELocationCalamityClass >
// EECalamityClass). EECCEarthquake.cpp is 0xb037f..0xb0e2e. sizeof 0x6c (EECalamityManager's new). Field names
// are provisional (offsets proven by Initialize 0xb0765). Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCEARTHQUAKE_H
#define EE_EECCEARTHQUAKE_H

class EEGraphicEffect;
class EESoundObject;
class GETransformation;

// 0x5309e0: a global transformation the earthquake maps its target location through before it measures the distance
// to the origin (the view? provisional name; owner header inferred; also read at 0x10440b/0x10520c/0x10f5af)

class EECCEarthquake : public EELocationCalamityClass {
public:
    EECCEarthquake();                       // 0xb037f
    virtual ~EECCEarthquake();              // 0xb03b5 (??_G 0xb0399)
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb03e1
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb0750
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb0765
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 0xb0850
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb0621
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c 0xb0654: starts the sound
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 (ICF-folded into EECCFire's 0xb10c9)
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb0a60
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb0da5

protected:
    // Provisional names (by behaviour)
    bool CreateSourceEffect(EECalamity* inCalamity);    // 0xb0409: the effect from the caster to the target
    bool CreateCracks(EECalamity* inCalamity);          // 0xb0502: mCrackCount cracks around the target
    bool SetAffectedArea(EECalamity* inCalamity);       // 0xb06b5: the calamity's rectangle (mRadius around the target)
    long CreateCrack(GE3DPoint& inFrom, float inX, float inY);   // 0xb071c
    void UpdateEpicenterFactor(EECalamity* inCalamity); // 0xb085c
    void UpdateQuakeSound(EECalamity* inCalamity);      // 0xb090f
    void ShakeCamera(EECalamity* inCalamity, float inElapsedTime);   // 0xb0956

    EEGraphicEffect* mGraphicEffect2;       // +0x28 (record +0x98; from the caster to the target at Start)
    EEGraphicEffect* mGraphicEffect1;       // +0x2c (record +0x94; the cracks; its height is the radius, its duration
                                            // the quake's)
    EESoundObject* mSound;                  // +0x30 (record +0xd4)
    long mDamage;                           // +0x34 (record +0xb8) per tick (times the game variant's +0x44)
    long mTickTime;                         // +0x38 (record +0xc4)
    long mShake;                            // +0x3c (record +0xbc) the camera shake
    long mCrackCount;                       // +0x40 (record +0xc8)
    float mDuration;                        // +0x44 mGraphicEffect1's duration in seconds
    float mRadius;                          // +0x48 mGraphicEffect1's height
    float mShakeInterval;                   // +0x4c (1 / record +0xc0)
    float mVolume;                          // +0x50 (record +0x68) before the cracks
    float mQuakeVolume;                     // +0x54 (record +0x70) once they are out
    float mMinimumFactor;                   // +0x58 (record +0x64) before the cracks
    float mQuakeMinimumFactor;              // +0x5c (record +0x6c) once they are out
    float mSpeed;                           // +0x60 mRadius / mDuration
    float mSourceSpeed;                     // +0x64 mGraphicEffect2's duration / 100
    float mFalloff;                         // +0x68 (1 / record +0xb4)
};

#endif
