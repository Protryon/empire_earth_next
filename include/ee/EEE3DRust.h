// The 3D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs): one class for every kind that moved,
// each virtual a call into Rust by the record's kind, answered through EeGfxEffect3DHost and drawn through the
// rasterizer table gEERustRaster. Kinds: 0x3f the teleport and 0x44 the time warp (EEE3DTeleport, EEE3DTimeWarp,
// 0xfb9f6..0xfc382), 0x18 the blast wave (EEE3DBlastWave, 0xe8ed8..0xe9474), 0x45 the shield (EEE3DShield,
// 0xf923f..), 0x43 the pulse cannon ball (EEE3DPulseCannonBall, 0xf69e3..0xf7394), 0x21 the tracers and 0x26 the
// shells (EEE3DTracers 0xfd330..0xfdbab, EEE3DShells 0xf90e9..0xf923f), 0x33 the citizen beam (EEE3DCitizenBeam,
// 0xe9474..0xe9bee), 0x1b the water splash (EEE3DWaterSplash, 0x101c34..0x101fce), 0x46 the screen flash
// (EEE3DScreenFlash, 0xf8ceb..0xf90e9), 0x27 the additive pass (EEE3DAdditivePass, 0xe89c6..0xe8ed8) and 0x25 the
// linear projectile (EEE3DLinearProjectile, 0xf2d93..0xf3580), 0x23 the disease (EEE3DDisease, 0xebde9..0xec6e4) and
// 0x42 the virus (EEE3DVirus, 0xfdbab..0xfe553), 0x24 the hurricane (EEE3DHurricane, 0xf1600..0xf2046), 0x28 the
// nuclear explosion (EEE3DNuclearExplosion, 0xf43c7..0xf4bf3) and 0x47 the energy wave (EEE3DEnergyWave,
// 0xee1b4..0xee5c0), all deleted.
// Requires ee/EEEffect3D.h and rust/ee_gfx.h.
#ifndef EE_EEE3DRUST_H
#define EE_EEE3DRUST_H
#include "rust/ee_gfx.h"

class EEGraphicObject;
class SESMInstanceNode;

class EEE3DRust : public EEEffect3D {
public:
    EEE3DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE3DRust(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);   // at a point
    // a shot from the unit's weapon point to a point (the pulse cannon ball), the citizen beam towards it
    EEE3DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind);
    // a shot on inTo from inFrom's weapon point (the tracers, the shells)
    EEE3DRust(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind);
    // from one point to another (the linear projectile)
    EEE3DRust(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE3DRust(long inKind);                 // for loading
    virtual ~EEE3DRust();
    virtual long DrawPass2() { return this->Call(EeGfxEffect3DOp::DrawPass2); }
    virtual long DrawPass3() { return this->Call(EeGfxEffect3DOp::DrawPass3); }
    virtual long DrawPass4() { return this->Call(EeGfxEffect3DOp::DrawPass4); }
    virtual long DrawPass5() { return this->Call(EeGfxEffect3DOp::DrawPass5); }
    virtual long DrawPass6() { return this->Call(EeGfxEffect3DOp::DrawPass6); }
    virtual long Queue(EEEffectQueue* inQueues) { return this->Call(EeGfxEffect3DOp::Queue, inQueues); }
    virtual void PrepareQueue() {}
    virtual long SetUp() { return this->Call(EeGfxEffect3DOp::SetUp); }
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long RepairPointers();
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual long Step() { return this->Call(EeGfxEffect3DOp::Step); }

    long Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues = NULL, const EeGfxEffectFile* inFile = NULL, long inBase = 0,
              unsigned long inMajorVersion = 0, unsigned long inMinorVersion = 0);

    EEGraphicObject* mGraphicObject = NULL;
    EEGraphicObject* mUnitModel = NULL;     // the additive pass's: its unit's model (EEModel), loaded while it lives
    SESMInstanceNode* mSound = NULL;        // the disease's, virus's and hurricane's animation sound
};

// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

#endif
