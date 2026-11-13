// The 3D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs, effect_follow.rs, effect_shot.rs):
// EEE3DRust's virtuals call Rust by the record's kind and answer its questions about the world here. Replaces
// EEE3DTeleport.cpp, EEE3DTimeWarp.cpp, EEE3DBlastWave.cpp, EEE3DShield.cpp, EEE3DPulseCannonBall.cpp, EEE3DTracers.cpp
// and EEE3DShells.cpp.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEEffectManager.h"
#include "ee/EEE3DRust.h"
#include "ee/EECombatFormulas.h"

namespace {
// the world's answers for one call
struct Ask {
    EEE3DRust* mEffect;
    EEEffectQueue* mQueues;
    static EEE3DRust* E(void* inAsk) { return static_cast<Ask*>(inAsk)->mEffect; }
    static EeGe3DPoint UnitPosition(void* inAsk)
    {
        GE3DPoint thePoint;
        E(inAsk)->mUnit->GetPosition(thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static float ModelHeight(void* inAsk)
    {
        return E(inAsk)->mGraphicEffect->mGraphicObject->mModels[0]->GetBoundingBox(0).GetHeight();
    }
    static void MoveTo(void* inAsk)
    {
        GETransformation theTransformation(E(inAsk)->mTransformation);
        E(inAsk)->SetTransformation(theTransformation);
    }
    static void Line(void* inAsk, const EeGe3DPoint* inStart, const EeGeVector* inDirection, uint32_t inLinked)
    {
        GE3DLine theLine;
        theLine.SetStart(GE3DPoint(inStart->x, inStart->y, inStart->z));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        EEGraphicEffect* theEffect = E(inAsk)->mGraphicEffect;
        gEffectManager->CreateLineEffect(theLine, inLinked == 1 ? theEffect->mLinkedEffect1 : theEffect->mLinkedEffect2, NULL);
    }
    static int32_t BaseQueue(void* inAsk) { return E(inAsk)->EEEffect3D::Queue(static_cast<Ask*>(inAsk)->mQueues); }
    static void Push(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        GETexture* theTexture = theObject ? theObject->mTextures[gScreenIndex] : NULL;
        static_cast<Ask*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    static int32_t Media(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->GetGraphicObject();
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void* Model(void* inAsk) { return E(inAsk)->mGraphicObject->mModel; }
    static void* Texture(void* inAsk) { return E(inAsk)->mGraphicObject->mTextures[0]; }
    // to the unit's first tag point of type 0x12 (keeping the unit's orientation), else to the unit (EEE3DBlastWave's
    // and EEE3DShield's FollowUnit, 0xe904a)
    static void Follow(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        GE3DPoint thePoint;
        GETransformation theLocation(theUnit->mPhysicalModel.GetLocation());
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0x12);
        if (theList != NULL && !theList->mPoints.empty()) {
            EETagPointList::Scale(theLocation, theScale);
            theList->mPoints.front()->GetPoint(theFrame, theLocation, thePoint);
            GETransformation theTransformation(false);
            theTransformation = theLocation;
            theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
            theEffect->SetTransformation(theTransformation);
        } else {
            theEffect->SetTransformation(theLocation);
        }
    }
    static void* ModelAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mModels[inIndex]; }
    // a tracer's end: its impact effect at its position, then the target's hit (EEE3DTracers::Step, 0xfdad4)
    static void ShotHit(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicEffect* theImpact = theEffect->mGraphicEffect->mLinkedEffect1;
        if (theImpact) {
            GE3DPoint theLocation(theEffect->mTransformation);
            gEffectManager->CreatePlacedEffect(theLocation, theImpact, NULL);
        }
        if (theEffect->mUnit)
            theEffect->mUnit->mUnitClass->UnitDamaged(theEffect->mUnit->mComplexUnit, NULL, 0);
    }
};

// a shot's start: the unit's weapon point (ee_combat's weapon_point; inMode 6 the pulse cannon's, 7 the tracers');
// whether the unit's model has tag points at its animation
bool ShotStart(long inMode, EEUnit* inUnit, EeGe3DPoint& outStart)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->GetAnimation()];
    bool theTagged = theList != NULL && theList->GetNumTagPoints() != 0;
    EECombatEffectWeaponPoint(inMode, theUnit, theModel, theList, theTagged, theStart);
    outStart = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    return theTagged;
}
}

EEE3DRust::EEE3DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    this->Call(EeGfxEffect3DOp::Create);
}

EEE3DRust::EEE3DRust(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->mRecord->aged.location = EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
    this->Call(EeGfxEffect3DOp::CreateAt);
}

// the pulse cannon ball (0x43): from the unit's weapon point to the target (EEE3DPulseCannonBall, 0xf6a26)
EEE3DRust::EEE3DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    ShotStart(6, inUnit, this->mRecord->shot.path.start);
    this->mRecord->shot.location = EeGe3DPoint{inTarget.mX, inTarget.mY, inTarget.mZ};
    this->Call(EeGfxEffect3DOp::Create);
}

// the tracers (0x21) and shells (0x26) on inTo: from inFrom's weapon point to a point of inTo's model, in inFrom's
// player colour (EEE3DTracers, 0xfd373)
EEE3DRust::EEE3DRust(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inTo, inGraphicEffect, inKind)
{
    GE3DPoint theEnd;
    GETransformation theToTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    float theToScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    if (ShotStart(7, inFrom, this->mRecord->shot.path.start))
        EETagPointList::Scale(theToTransformation, theToScale);
    GE3DPoint theOffset = inTo->GetModel()->PickPoint();
    theToTransformation.Apply(theOffset, theEnd);
    const GEColor& theColor = inFrom->mPlayer->mColor;
    this->mRecord->shot.location = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->mRecord->shot.color = EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
    this->Call(EeGfxEffect3DOp::Create);
}

EEE3DRust::EEE3DRust(long inKind)
    : EEEffect3D(inKind)
{
}

// unloads the graphic object's media
EEE3DRust::~EEE3DRust()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

long EEE3DRust::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile, long inBase,
                     unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    EEGraphicObject* theObject = theEffect ? theEffect->mGraphicObject : NULL;
    EeGfxGraphicFacts theFacts = {};
    if (theEffect != NULL) {
        theFacts.size = theEffect->mSize;
        theFacts.param1 = theEffect->mParam1;
        theFacts.param2 = theEffect->mParam2;
        theFacts.param3 = theEffect->mParam3;
        theFacts.param4 = theEffect->mParam4;
        theFacts.param5 = theEffect->mParam5;
        theFacts.param6 = theEffect->mParam6;
        theFacts.flag1 = theEffect->mFlag1;
        theFacts.duration = (uint32_t)theEffect->mDuration;
        theFacts.has_object = theObject != NULL;
        theFacts.alpha_ref = theObject != NULL ? theObject->mAlphaReference : 0.0f;
    }
    theFacts.tick_seconds = gPhysics->GetTickSeconds();
    theFacts.tick_time = gPhysics->GetTickTime();
    theFacts.has_unit = this->mUnit != NULL;
    theFacts.unit_hit_points = this->mUnit != NULL ? (int32_t)this->mUnit->mHitPoints : 0;
    Ask theAsk = {this, inQueues};
    EeGfxEffect3DHost theHost = {&theAsk, Ask::UnitPosition, Ask::ModelHeight, Ask::MoveTo, Ask::Line, Ask::BaseQueue,
                                 Ask::Push, Ask::Media, Ask::Model, Ask::Texture, Ask::Follow, Ask::ModelAt,
                                 Ask::ShotHit};
    if (inFile != NULL)
        return ee_gfx_effect3d_file(inOp, inBase, this->mRecord, inFile, inMajorVersion, inMinorVersion, &theHost);
    return ee_gfx_effect3d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// the base 3D effect, then the kind's own (ee_gfx); the first error, as each original
long EEE3DRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theBase = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}

// after loading: the base effect's pointers, then the kind's own (the media of the teleport and time warp)
long EEE3DRust::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    return theResult != 0 ? theResult : this->Call(EeGfxEffect3DOp::Repair);
}

long EEE3DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theBase = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}
