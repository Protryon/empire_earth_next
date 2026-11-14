// The 2D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_2d.rs): EEE3DRust's twin over EEEffect2D, one
// class for every 2D-based kind that moved, each virtual a call into Rust by the record's kind, answered through
// EeGfxEffect2DHost and drawn through the rasterizer table gEERustRaster. Kinds: 0x41 the command confirmation
// (EEE3DCommandConfirmation, 0xe9bee..0xea703, deleted; an EEEffect2D despite its name).
// Requires ee/EEEffect2D.h and rust/ee_gfx.h.
#ifndef EE_EEE2DRUST_H
#define EE_EEE2DRUST_H
#include "rust/ee_gfx.h"

class EEGraphicObject;
class GETexture;

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
    virtual ~EEE2DRust();
    virtual long DrawPass2() { return this->Call(EeGfxEffect3DOp::DrawPass2); }
    virtual long DrawPass3() { return this->Call(EeGfxEffect3DOp::DrawPass3); }
    virtual long DrawPass4() { return this->Call(EeGfxEffect3DOp::DrawPass4); }
    virtual long DrawPass5() { return this->Call(EeGfxEffect3DOp::DrawPass5); }
    virtual long DrawPass6() { return this->Call(EeGfxEffect3DOp::DrawPass6); }
    virtual long Queue(EEEffectQueue* inQueues) { return this->Call(EeGfxEffect3DOp::Queue, inQueues); }
    virtual void PrepareQueue() {}
    virtual long SetUp() { return this->Call(EeGfxEffect3DOp::SetUp); }
    // the confirmation is never saved: Load, Save and RepairPointers return 0 without the base's
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
        { return this->Call(EeGfxEffect3DOp::Load); }
    virtual long RepairPointers() { return this->Call(EeGfxEffect3DOp::Repair); }
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
        { return this->Call(EeGfxEffect3DOp::Save); }
    virtual long Step() { return this->Call(EeGfxEffect3DOp::Step); }

    long Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues = NULL);

    EEGraphicObject* mGraphicObject = NULL;
};

// the facts of an effect's graphic effect, unit and tick the Rust effects read (EEE3DRust.cpp)
EeGfxGraphicFacts EEEffectFacts(EEGraphicEffect* inEffect, EEUnit* inUnit);

#endif
