// C:\EEX\Empire Earth\Graphics Thread\EEE3DPulseCannonBall.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DPulseCannonBall-Data"). 0xf69e3..0xf7394: the key chain, both ctors .. Virtual4C.
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
#include "ee/EEE3DPulseCannonBall.h"

// SYNTHETIC: EXE 0x4f69e3
// SYNTHETIC: EXE 0x4f69ed
// SYNTHETIC: EXE 0x4f69fe
// SYNTHETIC: EXE 0x4f6a0a
const UString EEE3DPulseCannonBall::sData("EEE3DPulseCannonBall-Data");

// FUNCTION: EXE 0x4f6a26
// Creates a pulse cannon shot from the unit's weapon tag point (turret-aware) to the target, facing along its path,
// its travel rate the effect's duration value over the path length
EEE3DPulseCannonBall::EEE3DPulseCannonBall(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect,
                                           long inKind, long inArg5)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    GE3DPoint theStart;
    GETransformation theTransformation;
    GETransformation theUnitTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
    GEVector theDirection;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->GetAnimation()];
    float theScale = inUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    this->mPosition = 0.0f;
    this->mGraphicObject = NULL;
    this->mDrawPosition = 0.0f;
    if (theList != NULL && theList->GetNumTagPoints() != 0) {
        EETagPointList::Scale(theUnitTransformation, theScale);
        if (theModel->GetNumParts() > 0) {
            GETransformation thePartTransformation(false);
            unsigned long theModelIndex;
            thePartTransformation.MakeIdentity();
            if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, theModelIndex) == 0)
                thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theUnitTransformation, thePartTransformation, theStart);
        } else {
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theUnitTransformation, theStart);
        }
    } else {
        theUnitTransformation.Apply(inUnit->GetModel()->mDefaultPoint, theStart);
    }
    theDirection.SetX(inTarget.GetX() - theStart.GetX());
    theDirection.SetY(inTarget.GetY() - theStart.GetY());
    theDirection.SetZ(inTarget.GetZ() - theStart.GetZ());
    this->mPath.SetStart(theStart);
    this->mPath.SetDirection(theDirection);
    theDirection.Normalize();
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    theTransformation.SetPosition(theStart.GetX(), theStart.GetY(), theStart.GetZ());
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    this->SetTransformation(theTransformation);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    float theDuration = this->mGraphicEffect->mDuration * 0.00001f;
    this->mSpeed = theDuration / this->mPath.GetDirection().GetMagnitude();
    this->mGlowScale = 0.1f;
    this->mScale = 1.0f;
}

// SYNTHETIC: EXE 0x4f6d38
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f6d54
// Creates an empty pulse cannon shot of the given kind (for loading)
EEE3DPulseCannonBall::EEE3DPulseCannonBall(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4f6da1
// Destroys the shot, unloading its graphic object's media
EEE3DPulseCannonBall::~EEE3DPulseCannonBall()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DPulseCannonBall::DrawPass2()
{
    return 0;
}

long EEE3DPulseCannonBall::DrawPass4()
{
    return 0;
}

long EEE3DPulseCannonBall::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4f6dc8
// Snapshots the shot's path position, scales and location for drawing and queues it in render queue 4 under its texture
long EEE3DPulseCannonBall::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawPosition = this->mPosition;
        this->mDrawScale = this->mScale;
        this->mDrawGlowScale = this->mGlowScale;
        this->mTransformation.GetPosition(this->mLocation);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DPulseCannonBall::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f6e5e
// Loads the shot's graphic object media
long EEE3DPulseCannonBall::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4f6e75
// Reads the shot's path position, rate, path and both scales from a saved game; returns the first read error
long EEE3DPulseCannonBall::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mPosition);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSpeed);
    if (theResult == kErrorOK)
        theResult = this->mPath.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mScale);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mGlowScale);
    return theResult;
}

// FUNCTION: EXE 0x4f6ef6
// Draws the shot's ball and glow models additively at its snapshot location, lit by the fog/light at that point
long EEE3DPulseCannonBall::DrawPass6()
{
    GETransformation theTransformation(this->mTransformation);
    GETransformation theGlowTransformation(theTransformation);
    float theValue;
    GEModel* theModel = this->mGraphicObject->mModel;
    GEModel* theGlow = this->mGraphicObject->mModels[1];
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    GEMaterial* theGlowMaterial = theGlow->GetMaterial(0);
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
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    const GE3DPoint& theLocation = this->mLocation;
    GetFogFactor(theLocation, theFog, theValue);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale * this->mDrawScale);
    gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    theModel->PrepareTransforms();
    gRasterizer->Draw(theModel, theScreen, 1.0f, gOrthographicView);
    theGlowTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theGlow->SetGraphicsPostModelTransform(theGlowTransformation);
    theGlow->SetGraphicsScale(theScale * this->mDrawGlowScale);
    theGlowMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theGlow->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theGlow, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// (ICF-folded into EEE3DBlastWave::RepairPointers 0xe93e0)
long EEE3DPulseCannonBall::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4f710e
// Writes the shot's path position, rate, path and both scales to the saved game; returns the first write error
long EEE3DPulseCannonBall::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mPosition);
    if (theResult == 0)
        theResult = outFile.Write(this->mSpeed);
    if (theResult == 0)
        theResult = this->mPath.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mScale);
    if (theResult == 0)
        theResult = outFile.Write(this->mGlowScale);
    return theResult;
}

// FUNCTION: EXE 0x4f717b
// Replaces the shot's unit as the base effect does
void EEE3DPulseCannonBall::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    EEEffect::ReplaceUnit(inOldUnit, inNewUnit);
}

// FUNCTION: EXE 0x4f718b
// Moves the shot along its path, pulsing its scale every tenth of the way, then grows it as an explosion past the end;
// returns 7 when the explosion ends (path position 1.5)
long EEE3DPulseCannonBall::Step()
{
    GETransformation theTransformation(this->mTransformation);
    GE3DPoint thePoint;
    long theResult = 0;
    this->mPosition += gPhysics->GetTickTime() * this->mSpeed;
    if (this->mPosition <= 1.0f) {
        this->mPath.GetPointAt(this->mPosition, thePoint);
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
        if (this->mPosition > 0.9f)
            this->mScale = 1.25f;
        else if (this->mPosition > 0.8f)
            this->mScale = 1.0f;
        else if (this->mPosition > 0.7f)
            this->mScale = 1.25f;
        else if (this->mPosition > 0.6f)
            this->mScale = 1.0f;
        else if (this->mPosition > 0.5f)
            this->mScale = 1.25f;
        else if (this->mPosition > 0.4f)
            this->mScale = 1.0f;
        else if (this->mPosition > 0.3f)
            this->mScale = 1.25f;
        else if (this->mPosition > 0.2f)
            this->mScale = 1.0f;
        else if (this->mPosition > 0.1f) {
            this->mScale = 1.25f;
            this->mGlowScale = 0.1f;
        }
    } else if (this->mPosition < 1.5f) {
        if (this->mPosition > 1.4f) {
            this->mScale = 32.0f;
            this->mGlowScale = 0.1f;
        } else if (this->mPosition > 1.3f) {
            this->mScale = 16.0f;
            this->mGlowScale = 0.5f;
        } else if (this->mPosition > 1.2f) {
            this->mScale = 8.0f;
            this->mGlowScale = 1.0f;
        } else if (this->mPosition > 1.1f) {
            this->mScale = 4.0f;
            this->mGlowScale = 1.5f;
        } else {
            this->mGlowScale = this->mScale = 2.0f;
        }
    } else {
        theResult = 7;
    }
    return theResult;
}
