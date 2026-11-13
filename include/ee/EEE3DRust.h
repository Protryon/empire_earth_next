// The 3D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs): one class for every kind that moved,
// each virtual a call into Rust by the record's kind, answered through EeGfxEffect3DHost and drawn through the
// rasterizer table gEERustRaster. Kinds: 0x3f the teleport and 0x44 the time warp (EEE3DTeleport, EEE3DTimeWarp,
// 0xfb9f6..0xfc382), 0x18 the blast wave (EEE3DBlastWave, 0xe8ed8..0xe9474), 0x45 the shield (EEE3DShield,
// 0xf923f..), all deleted. Requires ee/EEEffect3D.h and rust/ee_gfx.h.
#ifndef EE_EEE3DRUST_H
#define EE_EEE3DRUST_H
#include "rust/ee_gfx.h"

class EEGraphicObject;

class EEE3DRust : public EEEffect3D {
public:
    EEE3DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);
    EEE3DRust(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);   // at a point
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
};

// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

#endif
