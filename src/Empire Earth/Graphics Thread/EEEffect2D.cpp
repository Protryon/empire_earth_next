// C:\EEX\Empire Earth\Graphics Thread\EEEffect2D.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffect2D-Data"). The base of the 2D effects (vtable 0x43f9a4): 0x10428c..0x1050fb (the key chain, the dtor ..
// Save, then the vector<EEEffect2DPart> template COMDATs and EEEffect2DPart's two inline ctors); EEEffect3D.cpp
// follows. The two EEEffect2D ctors and SetPosition are inline COMDATs emitted by EEE2DBattlefieldAmbient.cpp
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
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "rust/ee_ge.h"   // ee_gfx view.rs: the frustum test (RUST_PLAN R4.4)
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"

#include <math.h>
#include "rust/ee_gfx.h"

const UString EEEffect2D::sData("EEEffect2D-Data");

// Destroys the 2D effect, unloading every part's media and releasing its model
EEEffect2D::~EEEffect2D()
{
    unsigned long theCount = this->mParts.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mParts[i].mGraphicObject->UnloadMedia();
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
}

// Returns 10 (skip) when the effect's tile is missing or unseen by the current player (unless the camera is free) or
// its bounding sphere lies outside the view on any side; else 0
long EEEffect2D::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect::Queue(inQueues);
    if (theResult != 0)
        return theResult;
    GE3DPoint theLocation(this->mPosition.GetX(), this->mPosition.GetY(), this->mPosition.GetZ());
    float theRadius = this->mBoundingRadius;
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    bool theOrthographic = gOrthographicView;
    EEMapTile* theTile = gWorld.mMap->GetTile(theX, theY);
    if (theTile == NULL)
        return 10;
    if (!theTile->IsSeenBy(gWorld.GetCurrentPlayer()) && !gFreeCamera)
        return 10;
    if (!ee_gfx_effect_in_view(reinterpret_cast<const EeGeViewport*>(&gViewport), theOrthographic,
                               reinterpret_cast<const EeGe3DPoint*>(&theLocation), theRadius))
        return 10;
    return 0;
}

// NEAR: the floor conversions (y first, through a copy) and the register choice of the tag list
// Creates one sprite part per tag point of the effect's kind on the unit (else one part at the unit, tag 10000),
// lifting a buried position by inOffset; returns the position and the height (farthest part + effect size)
void EEEffect2D::CreateParts(GE3DPoint& outPosition, float& outHeight, float inOffset)
{
    GE3DPoint theTagPoint;
    GETerrainMesh* theMesh = &gWorld.mMap->mTerrainMesh;
    GE3DPoint thePosition = this->GetUnitLocation();
    float theMaximum = 0.0f;
    float theHeight = 1.0f;
    EEUnit* theUnit = this->mUnit;
    // The original reads the stats above the null test ([unit+0x34] before the je); a unitless effect (an
    // explosion at a point) has no unit, so ours reads them inside it.
    if (theUnit) {
        EEUnitClassUpgrade* theStats = theUnit->GetStats();
        EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theComplexUnit ? theComplexUnit->mAnimation : 0,
                                                                this->GetGraphicEffect()->GetKind());
        unsigned long theY = UMFloorUnsigned(thePosition.GetY());
        unsigned long theX = UMFloorUnsigned(thePosition.GetX());
        if (theMesh->GetIsValidPoint(theX, theY)) {
            float theZ = theMesh->GetMeshZ(thePosition.GetX(), thePosition.GetY());
            if (thePosition.GetZ() < theZ)
                thePosition.SetZ(theZ + inOffset);
        }
        if (theList && theList->GetNumTagPoints() != 0) {
            GETransformation theTransformation(false);
            float theScale;
            unsigned long theFrame;
            if (theComplexUnit) {
                theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
                theScale = theComplexUnit->mPhysicalModel.GetScale();
                theFrame = theComplexUnit->mPhysicalModel.GetFrame();
            } else {
                theTransformation.MakeIdentity();
                theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
                theFrame = 0;
                theScale = theStats->GetScale();
            }
            EETagPointList::Scale(theTransformation, theScale);
            for (unsigned long i = 0; i < theList->GetNumTagPoints(); i++) {
                EEGraphicObject* theObject = this->GetGraphicObject();
                theObject->LoadMedia();
                theList->GetTagPoint(i)->GetPoint(theFrame, theTransformation, theTagPoint);
                this->mParts.push_back(EEEffect2DPart(theTagPoint, theObject, i));
                float theDistance = thePosition.DistanceToSquared(theTagPoint);
                if (theDistance > theMaximum)
                    theMaximum = theDistance;
            }
            theHeight = (float)sqrt(theMaximum) + this->GetGraphicEffect()->GetSize();
        } else {
            EEGraphicObject* theObject = this->GetGraphicObject();
            theObject->LoadMedia();
            this->mParts.push_back(EEEffect2DPart(thePosition, theObject, 10000));
            theHeight = this->GetGraphicEffect()->GetSize();
        }
    }
    outPosition = thePosition;
    outHeight = theHeight;
}

// Unloads every part's media, drops the parts and the model, then releases the base effect's resources
void EEEffect2D::ReleaseResources()
{
    unsigned long theCount = this->mParts.size();
    for (unsigned long i = 0; i < theCount; i++)
        this->mParts[i].mGraphicObject->UnloadMedia();
    this->mParts.clear();
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
    EEEffect::ReleaseResources();
}

// Resets a pooled 2D effect onto a new unit and graphic effect with the given model (referenced), with no parts
void EEEffect2D::Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, GEModel* inModel, bool inKeepsUnit)
{
    EEEffect::Reinit(inUnit, inGraphicEffect, inKeepsUnit);
    this->Clear();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

// Resets a pooled 2D effect to the empty state with the given model (referenced), with no parts
void EEEffect2D::Reinit(GEModel* inModel, bool inKeepsUnit)
{
    EEEffect::Reinit(inKeepsUnit);
    this->Clear();
    this->mModel = inModel;
    if (inModel)
        inModel->Reference();
}

// Drops the parts and the model pointer (without releasing it) and zeroes the position and bounding radius (the
// record's)
void EEEffect2D::Clear()
{
    this->mParts.clear();
    ee_gfx_effect_clear_2d(this->mRecord);
    this->mModel = NULL;
}

// The parts as ee_gfx effect_state.rs's Load and Save use them (the context: the effect)
struct EEEffect2DPartOps {
    static EEEffect2D& Effect(void* inContext) { return *static_cast<EEEffect2D*>(inContext); }
    // a loaded part takes the effect's graphic object and is kept when its media loads
    static int32_t Push(void* inContext, const EeGfxEffectPart* inPart)
    {
        EEEffect2DPart thePart;
        thePart.mPosition = *reinterpret_cast<const GE3DPoint*>(&inPart->position);
        thePart.mTag = inPart->tag;
        thePart.mFrame = inPart->frame;
        thePart.mTime = inPart->time;
        thePart.mIsDone = inPart->done;
        thePart.mGraphicObject = Effect(inContext).GetGraphicObject();
        long theResult = thePart.mGraphicObject->LoadMedia();
        if (theResult == 0)
            Effect(inContext).mParts.push_back(thePart);
        return (int32_t)theResult;
    }
    static uint32_t Count(void* inContext) { return (uint32_t)Effect(inContext).mParts.size(); }
    static void Get(void* inContext, uint32_t inIndex, EeGfxEffectPart* outPart)
    {
        const EEEffect2DPart& thePart = Effect(inContext).mParts[inIndex];
        outPart->position = *reinterpret_cast<const EeGe3DPoint*>(&thePart.mPosition);
        outPart->tag = (uint32_t)thePart.mTag;
        outPart->frame = (uint32_t)thePart.mFrame;
        outPart->time = thePart.mTime;
        outPart->done = thePart.mIsDone;
    }
};

// Reads the position, bounding radius and parts (position, tag, frame, time, done), reloading each part's media;
// returns the first error, stopping at a part whose media fails (the base class result is ignored)
// (ee_gfx effect_state.rs)
long EEEffect2D::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    EeGfxEffectParts theParts = {this, EEEffect2DPartOps::Push, EEEffect2DPartOps::Count, EEEffect2DPartOps::Get};
    return ee_gfx_effect_load_2d(this->mRecord, &theFile, &theParts);
}

// Writes the position, bounding radius and parts (position, tag, frame, time, done); returns the first write error
// (the base class result is ignored)
// (ee_gfx effect_state.rs)
long EEEffect2D::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    EeGfxEffectParts theParts = {this, EEEffect2DPartOps::Push, EEEffect2DPartOps::Count, EEEffect2DPartOps::Get};
    return ee_gfx_effect_save_2d(this->mRecord, &theFile, &theParts);
}

// Creates an empty part (no graphic object, frame 0, not done)
inline EEEffect2DPart::EEEffect2DPart()
    : mGraphicObject(NULL), mTag(0), mFrame(0), mDrawFrame(0), mTime(0.0f), mIsDone(false)
{
}

// Creates a part at a position with a graphic object and tag, starting on the object's first start frame
inline EEEffect2DPart::EEEffect2DPart(const GE3DPoint& inPosition, EEGraphicObject* inGraphicObject,
                                      unsigned long inTag)
{
    this->mPosition = inPosition;
    this->mGraphicObject = inGraphicObject;
    this->mTag = inTag;
    this->mFrame = inGraphicObject->mStartFrames[0];
    this->mDrawFrame = 0;
    this->mTime = 0.0f;
    this->mIsDone = false;
}

// Repairs the effect's unit pointer (a `jmp` to EEEffect::RepairPointers)
long EEEffect2D::RepairPointers()
{
    return EEEffect::RepairPointers();
}
