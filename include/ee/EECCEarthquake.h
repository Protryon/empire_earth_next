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

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCEarthquake : public EELocationCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    // Provisional names (by behaviour)
    bool CreateCracks(EECalamity* inCalamity);          // 0xb0502: mCrackCount cracks around the target
    bool SetAffectedArea(EECalamity* inCalamity);       // 0xb06b5: the calamity's rectangle (mRadius around the target)
    long CreateCrack(GE3DPoint& inFrom, float inX, float inY);   // 0xb071c
    // UpdateEpicenterFactor 0xb085c, UpdateQuakeSound 0xb090f and ShakeCamera 0xb0956: Rust (calamity_update.rs)
};

#endif
