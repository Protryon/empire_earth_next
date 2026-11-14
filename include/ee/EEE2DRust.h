// The 2D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_2d.rs): EEE3DRust's twin over EEEffect2D, one
// class for every 2D-based kind that moved, each virtual a call into Rust by the record's kind, answered through
// EeGfxEffect2DHost and drawn through the rasterizer table gEERustRaster. Kinds: 0x41 the command confirmation
// (EEE3DCommandConfirmation, 0xe9bee..0xea703, deleted; an EEEffect2D despite its name), 0x12 the shockwave, and
// since the twentieth round the sprites of ee_gfx effect_sprite.rs: 6 the impact, 7 the blood impact, 8 the lightning,
// 9 the muzzle flash, 0xa the plasma pulse, 0xe the fire trail, 0x22 the water ripple, 0x2f the pulse wave (their
// classes deleted), and those of ee_gfx effect_parts.rs: 3 the exhaust trail, 4 the explosion, 5 the fire, 0xb and 0xc
// the smoke puffs, 0x14 the smoke muzzle blast; of effect_trail.rs: 0x16 the winter breath, 0x30 the 2D projectile; 0x31
// the vortex (effect_vortex.rs).
// Requires ee/EEEffect2D.h and rust/ee_gfx.h.
#ifndef EE_EEE2DRUST_H
#define EE_EEE2DRUST_H
#include "rust/ee_gfx.h"

class EEGraphicObject;
class GETexture;
class SESMInstanceNode;

// EEROUnits.cpp's three command textures (0x5294e0/0x5294e4/0x5294e8, loaded there; the confirmation draws with them)
// (provisional names)
extern GETexture* sTexture4E0;
extern GETexture* sTexture4E4;
extern GETexture* sTexture4E8;

class EEE2DRust : public EEEffect2D {
public:
    // the command confirmation (0x41): on a unit for its goal queue inGoalIndex (0: its current one), or at a point
    EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, unsigned long inGoalIndex, long inKind);
    EEE2DRust(EEUnit* inUnit, const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, unsigned long inGoalIndex,
              long inKind);
    // the other kinds on a unit, at a point (kept in the record's sprite.start for Rust), at a transformation (the
    // water ripple's: the record's aged.transformation) and for loading; each with its kind's shared model
    EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE2DRust(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE2DRust(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect, long inKind);
    // along a line (the velocity particle: the start and direction in the record's sprite.start and sprite.velocity);
    // from a shooter's weapon point to its target's model point or to a point (the muzzle blast and the projectile: the
    // two ends in the record's sprite.start and sprite.draw_position); the winter breath at its unit's mouth (the
    // target unused)
    EEE2DRust(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE2DRust(EEUnit* inUnit, EEUnit* inTarget, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE2DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE2DRust(long inKind);
    virtual ~EEE2DRust();
    virtual long DrawPass2() { return this->Call(EeGfxEffect3DOp::DrawPass2); }
    virtual long DrawPass3() { return this->Call(EeGfxEffect3DOp::DrawPass3); }
    virtual long DrawPass4() { return this->Call(EeGfxEffect3DOp::DrawPass4); }
    virtual long DrawPass5() { return this->Call(EeGfxEffect3DOp::DrawPass5); }
    virtual long DrawPass6() { return this->Call(EeGfxEffect3DOp::DrawPass6); }
    virtual long Queue(EEEffectQueue* inQueues) { return this->Call(EeGfxEffect3DOp::Queue, inQueues); }
    virtual void PrepareQueue() {}
    virtual long SetUp() { return this->Call(EeGfxEffect3DOp::SetUp); }
    // the confirmation is never saved: Load, Save and RepairPointers return 0 without the base's; the others after
    // EEEffect2D's (its results ignored)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long RepairPointers();
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    virtual long Step() { return this->Call(EeGfxEffect3DOp::Step); }

    long Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues = NULL, const EeGfxEffectFile* inFile = NULL,
              unsigned long inMajorVersion = 0, unsigned long inMinorVersion = 0);

    EEGraphicObject* mGraphicObject = NULL;
    SESMInstanceNode* mSound = NULL;        // the animation sound (impacts, lightning, plasma pulse)
};

// the facts of an effect's graphic effect, unit and tick the Rust effects read (EEE3DRust.cpp)
EeGfxGraphicFacts EEEffectFacts(EEGraphicEffect* inEffect, EEUnit* inUnit);

#endif
