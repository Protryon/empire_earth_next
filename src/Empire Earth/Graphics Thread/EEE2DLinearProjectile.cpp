// C:\EEX\Empire Earth\Graphics Thread\EEE2DLinearProjectile.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DLinearProjectile-Data"). 0xe2edd..0xe3954: the key chain, three ctors .. Virtual4C; EEE2DMarchingIcons.cpp
// follows.
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
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "lle/GE3DLine.h"
#include "ee/EEE2DLinearProjectile.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e2edd
// SYNTHETIC: EXE 0x4e2ee7
// SYNTHETIC: EXE 0x4e2ef8
// SYNTHETIC: EXE 0x4e2f04
const UString EEE2DLinearProjectile::sData("EEE2DLinearProjectile-Data");

// NEAR: the tag list's begin() and _Ptrit::operator+ are out of line in the original
// Scales the rotation part of a transformation by a factor (as EETagPointList::Scale), for shooters without tag points
static void ScaleTargetTransformation(GETransformation& ioTransformation, float inScale)
{
    for (unsigned long theRow = 0; theRow < 3; theRow++)
        for (unsigned long theColumn = 0; theColumn < 3; theColumn++)
            ioTransformation.SetElement(theRow, theColumn, ioTransformation.GetElement(theRow, theColumn) * inScale);
}

// FUNCTION: EXE 0x4e2f20
// Creates a straight projectile from the shooter's weapon tag point (turret-aware) to the target's model point
// (on the target's model scaled by its size)
EEE2DLinearProjectile::EEE2DLinearProjectile(EEUnit* inUnit, EEUnit* inTarget, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DLinearProjectile, false)
{
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GETransformation theTargetTransformation(inTarget->mComplexUnit->mPhysicalModel.GetLocation());
    GEVector theDirection;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    unsigned long thePart = theUnit->mAnimation;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[thePart];
    float theScale = inUnit->mComplexUnit->mPhysicalModel.GetScale();
    float theTargetScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    this->mGraphicObject = NULL;
    this->mT = 0.0f;
    this->mDrawT = 0.0f;
    if (theList && theList->mPoints.size()) {
        EETagPointList::Scale(theTransformation, theScale);
        if (theModel->GetNumParts() > 0) {
            GETransformation thePartTransformation(false);
            unsigned long theModelIndex;
            thePartTransformation.MakeIdentity();
            if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, theModelIndex) == 0)
                thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, thePartTransformation,
                                                                theStart);
        } else {
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, theStart);
        }
        EETagPointList::Scale(theTargetTransformation, theTargetScale);
    } else {
        theTransformation.Apply(inUnit->GetModel()->mDefaultPoint, theStart);
        // the target's point is on its scaled model whatever the shooter has
        ScaleTargetTransformation(theTargetTransformation, theTargetScale);
    }
    theOffset = inTarget->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    theDirection.SetX(theEnd.GetX() - theStart.GetX());
    theDirection.SetY(theEnd.GetY() - theStart.GetY());
    theDirection.SetZ(theEnd.GetZ() - theStart.GetZ());
    this->mLine.SetStart(theStart);
    this->mLine.SetDirection(theDirection);
    theDirection.Normalize();
    this->SetPosition(theStart);
}

// SYNTHETIC: EXE 0x4e3213
// (the scalar deleting destructor, emitted with the vtable)

// NEAR: as the first ctor
// FUNCTION: EXE 0x4e322f
// Creates a straight projectile from the shooter's weapon tag point (turret-aware) to a target point
EEE2DLinearProjectile::EEE2DLinearProjectile(EEUnit* inUnit, const GE3DPoint& inTarget,
                                             EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DLinearProjectile, false)
{
    GE3DPoint theStart;
    GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GEVector theDirection;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    unsigned long thePart = theUnit->mAnimation;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[thePart];
    float theScale = inUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    this->mT = 0.0f;
    this->mGraphicObject = NULL;
    this->mDrawT = 0.0f;
    if (theList && theList->mPoints.size()) {
        EETagPointList::Scale(theTransformation, theScale);
        if (theModel->GetNumParts() > 0) {
            GETransformation thePartTransformation(false);
            unsigned long theModelIndex;
            thePartTransformation.MakeIdentity();
            if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, theModelIndex) == 0)
                thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, thePartTransformation,
                                                                theStart);
        } else {
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theTransformation, theStart);
        }
    } else {
        theTransformation.Apply(inUnit->GetModel()->mDefaultPoint, theStart);
    }
    theDirection.SetX(inTarget.GetX() - theStart.GetX());
    theDirection.SetY(inTarget.GetY() - theStart.GetY());
    theDirection.SetZ(inTarget.GetZ() - theStart.GetZ());
    this->mLine.SetStart(theStart);
    this->mLine.SetDirection(theDirection);
    theDirection.Normalize();
    this->SetPosition(theStart);
}

// FUNCTION: EXE 0x4e3486
// Creates an empty projectile for loading from a saved game
EEE2DLinearProjectile::EEE2DLinearProjectile()
    : EEEffect2D(gEffect2DModel, kEffect2DLinearProjectile, false)
{
}

// FUNCTION: EXE 0x4e34ba
// Destroys the projectile, unloading its graphic's media
EEE2DLinearProjectile::~EEE2DLinearProjectile()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DLinearProjectile::DrawPass2()
{
    return 0;
}

long EEE2DLinearProjectile::DrawPass4()
{
    return 0;
}

long EEE2DLinearProjectile::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e34d8
// Queues the projectile in render queue 4 under its texture, snapshotting its progress, frame and position
long EEE2DLinearProjectile::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawT = this->mT;
        this->mDrawFrame = this->mFrame;
        this->mDrawPosition = this->mPosition;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DLinearProjectile::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e355a
// Loads the projectile's graphic at its first frame and sets its speed from the effect duration over the path length;
// returns the load result
long EEE2DLinearProjectile::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[0];
    this->mTime = 0.0f;
    this->mDrawFrame = 0;
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    float theDuration = this->mGraphicEffect->mDuration * 0.00001f;
    this->mSpeed = theDuration / this->mLine.GetDirection().GetMagnitude();
    return theResult;
}

// FUNCTION: EXE 0x4e35c9
// Reloads the graphic and reads progress, speed, path, frame time, frame and the path again (as Save writes it)
// Returns the first read error; the base class result is ignored
long EEE2DLinearProjectile::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mT);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mSpeed);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mLine.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mFrame);
    if (theResult == kErrorOK)
        theResult = this->mLine.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x4e3656
// Draws the projectile as an additive fog-lit sprite at its snapshotted position; returns the draw result
long EEE2DLinearProjectile::DrawPass6()
{
    GETransformation theTransformation(gWorld.mTransformation);
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    GETexture* theTexture = this->mGraphicObject->mTextures[0];
    theTexture->SetFrame(this->mDrawFrame);
    theMaterial->SetTexture(theTexture, theScreen);
    float theValue;
    const GE3DPoint& thePosition = this->mDrawPosition;
    GetFogFactor(thePosition, theFog, theValue);
    gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e37e1
// Writes progress, speed, path, frame time, frame and then the path a second time; returns the first write error
long EEE2DLinearProjectile::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mT);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mSpeed);
    if (theResult != 0)
        return theResult;
    theResult = this->mLine.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFrame);
    if (theResult == 0)
        theResult = this->mLine.Save(outFile);
    return theResult;
}

// NEAR: register choice (mFrame/frame time/frame count), mTime is loaded before the mT update in the original
// FUNCTION: EXE 0x4e3860
// Moves the projectile along its path and loops its animation; returns 7 (remove) once it reaches the end
long EEE2DLinearProjectile::Step()
{
    GE3DPoint thePoint;
    long theResult = 0;
    unsigned long theFrame = this->mFrame;
    float theTime = this->mTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[0];
    unsigned long theFrameCount = this->mGraphicObject->mFrameCounts[0];
    this->mT += gPhysics->GetTickTime() * this->mSpeed;
    if (this->mT <= 1.0f) {
        theTime += gPhysics->GetTickTime();
        if (theTime >= (float)theFrameTime) {
            long theFrames = UMFloatToLong(theTime / (float)theFrameTime);
            theTime -= (float)(theFrames * theFrameTime);
            theFrame = (theFrames + theFrame) % theFrameCount;
        }
        this->mTime = theTime;
        this->mFrame = theFrame;
        this->mLine.GetPointAt(this->mT, thePoint);
        this->SetPosition(thePoint);
    } else {
        theResult = 7;
    }
    return theResult;
}
