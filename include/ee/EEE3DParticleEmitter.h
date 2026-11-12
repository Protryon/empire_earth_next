// C:\EEX\Empire Earth\Graphics Thread\EEE3DParticleEmitter.h (RECONSTRUCTION, from EEE3DParticleEmitter.cpp
// 0xf5d94..0xf69e3; file name inferred). An emitter of 3D particles (kind 0x3a) at a
// point, on a line (with its direction as the velocity) or on a unit (vtable 0x440b24; RTTI EEE3DParticleEmitter >
// EEEffect3D). sizeof 0x94. Names are provisional. Requires ee/EEEffect3D.h.
#ifndef EE_EEE3DPARTICLEEMITTER_H
#define EE_EEE3DPARTICLEEMITTER_H

#include "ee/EEEffect3D.h"

class GE3DLine;
class SESMInstanceNode;

class EEE3DParticleEmitter : public EEEffect3D {
public:
    EEE3DParticleEmitter(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xf5dd7
    EEE3DParticleEmitter(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind);        // 0xf5e82
    // 0xf5f23: on inUnit (its complex unit's transformation, else its tile's centre on the terrain)
    EEE3DParticleEmitter(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, unsigned long inTagPoint);
    EEE3DParticleEmitter(long inKind);      // 0xf604d (the load-time ctor)
    virtual ~EEE3DParticleEmitter();        // +0x00 0xf6089 (??_G 0xf5e66)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 (likewise)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 (10: ICF-folded into EEEDamagedEffect's 0x1020dc)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xf634b (the timers)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf635e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf63b9
    virtual long Step();                    // +0x4c 0xf6417 (emits the particles)
    // +0x58 0xf60f5: one step of inSeconds: velocity under the graphic's gravity/drag, the new position in
    // outPosition (7 off the map; bounces off the terrain when the graphic's mB89 is set) (provisional name)
    virtual long Move(float inSeconds, GE3DPoint& outPosition);

    void StopSound();                       // 0xf60a6
    void ReleaseSound();                    // 0xf60cb
    void SetSound(SESMInstanceNode* inSound); // 0xf69bc (set-if-different, referenced; provisional name)

    static const UString sData;             // "EEE3DParticleEmitter" (no -Data suffix; provisional name)

    // the record's (ee_gfx effect_state.rs `EeGfxEffectEmitter`); the move, burst, load and save are Rust's
    GEVector& mVelocity = *reinterpret_cast<GEVector*>(&this->mRecord->emitter.velocity);   // +0x74 (saved)
    uint32_t& mTagPoint = this->mRecord->emitter.tag_point;   // +0x80 (the unit ctor's inArg4; saved)
    SESMInstanceNode* mSound;               // +0x84
    float& mTime = this->mRecord->emitter.time;   // +0x88 (saved)
    float& mEmitTime = this->mRecord->emitter.emit_time;   // +0x8c (saved)
    bool mIsFirstStep;                      // +0x90 (the ctors: true)
};

#endif
