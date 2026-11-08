// C:\EEX\Empire Earth\Graphics Thread\EEEffect2D.h (RECONSTRUCTION; file name inferred).
// RTTI EEEffect2D > EEEffect (vtable 0x43f9a4; ctor 0xdf44e, dtor 0x1042cf, ??_G 0xdf526). Layout gap-free from the
// ctor and dtor; sizeof 0x54 (EEE2DExplosion's own member is +0x54 of 0x58). Names are provisional. Requires
// ee/EEEffect.h and lle/GE3DPoint.h.
#ifndef EE_EEEFFECT2D_H
#define EE_EEEFFECT2D_H

#include <vector>
#include "lle/GEModel.h"

class GEModel;
class EEGraphicObject;

// the 2D effects' shared model (0x4e8df8: their ctors pass it to EEEffect2D's) (provisional name)
extern GEModel* gEffect2DModel;
// a second shared model (0x4e8df4: EEE2DPulseWave's and EEE2DVortex's ctors pass it) (provisional name)
extern GEModel* gU4E8DF4;

// The 0x24-byte element of EEEffect2D::mParts: one sprite of the effect (at a tag point of the unit, or the effect's
// position). Layout from its two inline ctors (COMDATs 0x105089/0x1050b0 at the end of EEEffect2D.cpp), Load/Save
// and the dtor's UnloadMedia loop; copied with `rep movsd` (implicit copy ctor), assigned field by field (implicit
// operator=). Names are provisional.
struct EEEffect2DPart {
    // inline, defined in EEEffect2D.cpp (its only user; COMDATs 0x105089 / 0x1050b0)
    void SetDrawFrame(unsigned long inFrame) { this->mDrawFrame = inFrame; }  // inline: the 2D effects' Queue loop (0x4df75d)
    EEEffect2DPart();
    EEEffect2DPart(const GE3DPoint& inPosition, EEGraphicObject* inGraphicObject, unsigned long inTag);

    GE3DPoint mPosition;                    // +0x00 (saved)
    EEGraphicObject* mGraphicObject;        // +0x0c (LoadMedia when made, UnloadMedia by the effect)
    unsigned long mTag;                     // +0x10 (the tag point's index, 10000 for the effect's own position; saved)
    unsigned long mFrame;                   // +0x14 the animation frame (= the object's mL360; saved)
    unsigned long mDrawFrame;               // +0x18 (Virtual24's copy of mFrame)
    float mTime;                            // +0x1c the time into the frame (saved)
    bool mIsDone;                           // +0x20 (saved)
};

class EEEffect2D : public EEEffect {
public:
    static const UString sData;             // "EEEffect2D-Data" (0x4e8e08, guard 0x4e8e00; chain 0x10428c)
    // 0xdf44e: EEEffect(inUnit, inGraphicEffect, inKind, inB2C), mParts, mPosition = 0, mModel (referenced), mF50 = 0
    EEEffect2D(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, GEModel* inModel, long inKind, bool inKeepsUnit)
        : EEEffect(inUnit, inGraphicEffect, inKind, inKeepsUnit)
    {
        this->mModel = inModel;
        if (this->mModel)
            this->mModel->Reference();
        this->mBoundingRadius = 0.0f;
    }
    // 0xdf4a2: the load-time ctor (EEEffect(inKind, inB2C) expanded, then as above; callers EEE2DParticle 0xe47b1,
    // EEE2DMarchingIcons 0xe3aa3). Both are inline COMDATs.
    EEEffect2D(GEModel* inModel, long inKind, bool inKeepsUnit)
        : EEEffect(inKind, inKeepsUnit)
    {
        this->mModel = inModel;
        if (this->mModel)
            this->mModel->Reference();
        this->mBoundingRadius = 0.0f;
    }
    virtual ~EEEffect2D();                  // +0x00 0x1042cf
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0x104332 (projects mPosition through the viewport)
    virtual void ReleaseResources();        // +0x28 0x1048b1
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0x104985
    virtual long RepairPointers();          // +0x3c 0xe8eb1 (ICF-shared)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0x104a8a

    // 0x104623: one part per tag point of kind mGraphicEffect->mKind on the unit's model (or one at the unit's
    // location), raised above the terrain by inOffset; the location and the parts' radius + height (provisional name)
    void CreateParts(GE3DPoint& outPosition, float& outHeight, float inOffset);
    // 0x10490e / 0x10493b: EEEffect::Reinit, Clear(), then the model (referenced) (provisional names)
    void Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, GEModel* inModel, bool inKeepsUnit);
    void Reinit(GEModel* inModel, bool inKeepsUnit);
    void Clear();                           // 0x104960 (parts, position, model, mF50) (provisional name)

    // inline in the real header (COMDAT 0xdf430 after its first user in EEE2DBattlefieldAmbient's range): mPosition's
    // three fields, then Virtual48(inPosition); declared only here (provisional name)
    void SetPosition(const GE3DPoint& inPosition)
    {
        this->mPosition = inPosition;
        this->MoveToPoint(inPosition);
    }
    // inline (the marching icons' ctors store the radius through it: fld/fstp, after sqrt's stack cleanup) (provisional
    // name)
    void SetBoundingRadius(float inValue) { this->mBoundingRadius = inValue; }

    std::vector<EEEffect2DPart> mParts;     // +0x30
    GE3DPoint mPosition;                    // +0x40 (Load: GE3DPoint::Load)
    GEModel* mModel;                        // +0x4c (referenced; GEModel::Dereference in the dtor)
    float mBoundingRadius;                  // +0x50
};

#endif
