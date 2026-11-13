// C:\EEX\Empire Earth\Graphics Thread\EEE2DSmokePlume.h (RECONSTRUCTION; file name inferred). A
// smoke puff that rises and drifts with the wind (RTTI EEE2DSmokePlume > EEEffect2D; vtable 0x440084;
// EEE2DSmokePlume.cpp 0xe6ef2..0xe72fa; sizeof 0x80; EEE2DVelocityParticle derives from it). Names are provisional.
// Requires ee/EEEffect2D.h.
//
// New (port, RUST_PLAN R4.4): the class is also the original's EEE2DVelocityParticle (kind 0xc, vtable 0x440158: a
// puff thrown along a line's direction, slowing down over the effect's int param 1). The saved state is the effect's
// record and the constructor's draws, the tick, load and save are ee_gfx effect_state.rs's, by kind; the C++ keeps
// the drawn copies, the graphic object and the draw.
#ifndef EE_EEE2DSMOKEPLUME_H
#define EE_EEE2DSMOKEPLUME_H

#include "ee/EEEffect2D.h"

class EEGraphicObject;
class GE3DLine;

class EEE2DSmokePlume : public EEEffect2D {
public:
    static const UString sData;             // "EEE2DSmokePlume-Data" (chain 0xe6ef2)

    EEE2DSmokePlume(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xe6f35
    // the velocity particle at the line's start with its direction (kind 0xc; EEE2DVelocityParticle 0xe7580)
    EEE2DSmokePlume(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect);
    EEE2DSmokePlume(long inKind);           // 0xe7020 (the load-time ctor: 0xb, 0xc)
    virtual ~EEE2DSmokePlume();             // +0x00 0xe705e (??_G 0xe7004)
    virtual long DrawPass2();               // +0x08 (0xfca98)
    virtual long DrawPass4();               // +0x10 (0xfca98)
    virtual long DrawPass5();               // +0x14 (0xfca98)
    virtual long DrawPass6();               // +0x18 0xe711d (the billboard 0x10387b)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xe707c
    virtual void PrepareQueue();            // +0x2c (empty: 0xe123d)
    virtual long SetUp();                   // +0x30 0xe70ef
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x38 0xe718e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x40 0xe71f1
    virtual long Step();                    // +0x4c 0xe7243 (ages; drifts with the wind, rises; 0xe7646)

    // the record's (ee_gfx effect_state.rs)
    float& mAge = this->mRecord->plume.age;   // +0x54 0..1 (saved)
    GE3DPoint& mStart = *reinterpret_cast<GE3DPoint*>(&this->mRecord->plume.start);   // +0x58 the puff's position
    GE3DPoint mDrawPosition;                // +0x64 (Virtual24's copy of mStart)
    EEGraphicObject* mGraphicObject;        // +0x70 (UnloadMedia in the dtor)
    float mDrawAge;                         // +0x74 (Virtual24's copy of mAge)
    float& mRotation = this->mRecord->plume.rotation;   // +0x78 (random 0..2pi; saved)
    float& mBrightness = this->mRecord->plume.brightness;   // +0x7c (random round param 1 by param 2, 0..1; saved)
};

#endif
