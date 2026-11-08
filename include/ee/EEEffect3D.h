// C:\EEX\Empire Earth\Graphics Thread\EEEffect3D.h (RECONSTRUCTION; file name inferred). RTTI EEEffect3D > EEEffect
// (vtable 0x440334; ??_G 0xe8eb6, dtor 0x102600 = EEEffect's).
// EEEffect3D.cpp is 0x1050fb..0x1054ec (key chain "EEEffect3D-Data", Virtual24 .. Save). The load-time ctor
// EEEffect3D(long) 0xe8e76, the Virtual3C forwarder 0xe8eb1 and ??_G 0xe8eb6 are inline COMDATs emitted at the end of
// the first user's range (EEE3DAdditivePass.cpp); the other ctor is inline too (EEE3DVolcano 0xfe596 expands it:
// EEEffect's ctor, the transformation, mF70 = 0 before this class's vptr). sizeof 0x74 (EEE3DVolcano's own members
// start at +0x74). Names are provisional. Requires ee/EEEffect.h, ee/EEMap.h and lle/GETransformation.h.
#ifndef EE_EEEFFECT3D_H
#define EE_EEEFFECT3D_H

#include "ee/EEEffect.h"

class SESMInstanceNode;

// 0x1487a3 (cdecl, in the unnamed unit 0x1482e4..0x1497f4): the graphic's sound for the animation frame inFrame at
// inLocation (inStart: the frame changed); takes a reference to inSound, returns a referenced node (the 3D effects
// Disease/Hurricane/Virus/ParticleEmitter pass it to their SetSound and dereference once) (provisional name)
SESMInstanceNode* PlayAnimationSound(const GE3DPoint& inLocation, EEGraphicObject* inGraphicObject, long inAnimation,
                           SESMInstanceNode* inSound, unsigned long inFrame, bool inStart);

class EEEffect3D : public EEEffect {
public:
    EEEffect3D(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
        : EEEffect(inUnit, inGraphicEffect, inKind, false), mBoundingRadius(0.0f) {}
    // 0xe8e76: `: EEEffect(inKind)` (EEEffect's inline ctor expanded), the transformation. Every derived default ctor
    // calls it out of line, so it is declared out of line here; its body sits at the end of EEE3DAdditivePass.cpp's
    // range with this class's Virtual3C and ??_G (probably an inline COMDAT the first user emitted: OPEN)
    EEEffect3D(long inKind);
    virtual ~EEEffect3D() {}                // ??_G 0xe8eb6 stores this class's vptr first, then EEEffect's dtor
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x10513e (10: off screen; ICF: EEE3DStorm)
    virtual void ReleaseResources();        // +0x28 0x105424 (EEEffect::ReleaseResources)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x105484
    virtual long RepairPointers() { return EEEffect::RepairPointers(); }  // +0x3c 0xe8eb1 (a `jmp`; ICF-shared)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x1054b8
    // +0x54 0x105429: inTile->IsSeenBy(the current player) (Virtual24 passes the tile under the effect)
    virtual bool IsTileVisible(EEMapTile* inTile);

    void Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0x105441 (EEEffect::Reinit(.., false)) (provisional name)
    void Reinit();                          // 0x10545e (EEEffect::Reinit(false)) (provisional name)
    void InitTransformation();              // 0x105471 (identity, mF70 = 0) (provisional name)
    // inline: the ctors that take a location call Virtual44 through the vtable after the copy (EEE3DBlastWave 0xe8ffa,
    // EEE3DParticleEmitter 0xf5eef: a ctor's own direct call would be static) (provisional name)
    void SetTransformation(const GETransformation& inTransformation)
        { this->mTransformation = inTransformation; this->MoveToTransformation(inTransformation); }

    // inline accessors of mF70: Load/Save copy it through the FPU (EEE3DRain 0x4f7c83/0x4f7d7f) (provisional names)
    float GetBoundingRadius() const { return this->mBoundingRadius; }
    void SetBoundingRadius(float inValue) { this->mBoundingRadius = inValue; }

    static const UString sData;             // "EEEffect3D-Data" (0x4e8e20; provisional name)

    GETransformation mTransformation;       // +0x30
    float mBoundingRadius;                  // +0x70 (the bounding radius Virtual24 tests on screen; EEE3DBlastWave: its
                                            // graphic effect's +0x1c)
};

#endif
