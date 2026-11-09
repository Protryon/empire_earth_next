// C:\EEX\Empire Earth\Graphics Thread\EEE3DTracers.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DTracers-Data"). 0xfd330..0xfdbab: the key chain, both ctors .. Virtual4C; EEE3DVirus.cpp follows.
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
#include "ee/EEE3DTracers.h"

// SYNTHETIC: EXE 0x4fd330
// SYNTHETIC: EXE 0x4fd33a
// SYNTHETIC: EXE 0x4fd34b
// SYNTHETIC: EXE 0x4fd357
const UString EEE3DTracers::sData("EEE3DTracers-Data");

// FUNCTION: EXE 0x4fd373
// Creates a tracer shot from the shooter's weapon tag point (turret-aware) to a random point on the target, in the
// shooter's player colour, with its travel rate the effect's duration value over the path length
EEE3DTracers::EEE3DTracers(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inTo, inGraphicEffect, inKind)
{
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theFromTransformation(inFrom->mComplexUnit->mPhysicalModel.GetLocation());
    GETransformation theToTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    GEVector theDirection;
    EEModel* theModel = inFrom->GetModel();
    EEComplexUnit* theUnit = inFrom->mComplexUnit;
    EETagPointList* theList = inFrom->GetModel()->mTagPoints[theUnit->GetAnimation()];
    float theScale = inFrom->mComplexUnit->mPhysicalModel.GetScale();
    float theToScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    unsigned long theIndex = theUnit->mWeaponTagPoint;
    this->mPosition = 0.0f;
    this->mGraphicObject = NULL;
    this->mDrawPosition = 0.0f;
    if (theList != NULL && theList->GetNumTagPoints() != 0) {
        EETagPointList::Scale(theFromTransformation, theScale);
        if (theModel->GetNumParts() > 0) {
            GETransformation thePartTransformation(false);
            unsigned long theModelIndex;
            thePartTransformation.MakeIdentity();
            if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, theModelIndex) == 0)
                thePartTransformation.SetOrientationYPR(theUnit->mTurretAngles[theModelIndex], 0.0f, 0.0f);
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theFromTransformation, thePartTransformation, theStart);
        } else {
            theList->GetTagPoint(theIndex)->GetPoint(theFrame, theFromTransformation, theStart);
        }
        EETagPointList::Scale(theToTransformation, theToScale);
    } else {
        theFromTransformation.Apply(inFrom->GetModel()->mDefaultPoint, theStart);
    }
    theOffset = inTo->GetModel()->PickPoint();
    theToTransformation.Apply(theOffset, theEnd);
    theDirection.SetX(theEnd.GetX() - theStart.GetX());
    theDirection.SetY(theEnd.GetY() - theStart.GetY());
    theDirection.SetZ(theEnd.GetZ() - theStart.GetZ());
    this->mPath.SetStart(theStart);
    this->mPath.SetDirection(theDirection);
    theDirection.Normalize();
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float thePitch = UMArcCos(-theDirection.GetZ());
    thePitch -= 1.5707964f;
    GETransformation theTransformation(false);
    theTransformation.SetPosition(theStart.GetX(), theStart.GetY(), theStart.GetZ());
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    this->SetTransformation(theTransformation);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    float theDuration = this->mGraphicEffect->mDuration * 0.00001f;
    this->mSpeed = theDuration / this->mPath.GetDirection().GetMagnitude();
    this->mColor = inFrom->mPlayer->mColor;
}

// SYNTHETIC: EXE 0x4fd742
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fd75e
// Creates an empty tracer shot of the given kind (for loading)
EEE3DTracers::EEE3DTracers(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4fd7ab
// Destroys the shot, unloading its graphic object's media
EEE3DTracers::~EEE3DTracers()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DTracers::DrawPass2()
{
    return 0;
}

long EEE3DTracers::DrawPass4()
{
    return 0;
}

long EEE3DTracers::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4fd7d2
// Snapshots the shot's path position and location for drawing and queues it in render queue 4 under its texture
long EEE3DTracers::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawPosition = this->mPosition;
        this->mTransformation.GetPosition(this->mLocation);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DTracers::PrepareQueue()
{
}

// ICF-folded into EEE3DPulseCannonBall's 0xf6e5e
long EEE3DTracers::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4fd850
// Draws the tracer model additively, in player colour (flag 1) or textured, dimming over the second half of its path
long EEE3DTracers::DrawPass6()
{
    GETransformation theTransformation(this->mTransformation);
    float theValue;
    GEModel* theModel = this->mGraphicObject->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    float theScale = this->mGraphicEffect->mSize;
    bool theColoured = this->mGraphicEffect->mFlag1;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    if (theColoured)
        theMaterial->SetColor(this->mColor);
    else
        theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    const GE3DPoint& theLocation = this->mLocation;
    GetFogFactor(theLocation, theFog, theValue);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    if (this->mDrawPosition >= 0.5f) {
        float theLight = (1.0f - this->mDrawPosition) * theValue * 2.0f;
        gRasterizer->SetAmbientColor(GEColor(theLight, theLight, theLight), theScreen);
    } else {
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    }
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4fda2e
// Reads the shot's path position, rate and path from a saved game; returns the first read error
long EEE3DTracers::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mPosition);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSpeed);
    if (theResult == kErrorOK)
        theResult = this->mPath.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x4fda8b
// Writes the shot's path position, rate and path to the saved game; returns the first write error
long EEE3DTracers::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mPosition);
    if (theResult == 0)
        theResult = outFile.Write(this->mSpeed);
    if (theResult == 0)
        theResult = this->mPath.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x4fdad4
// Moves the shot along its path; at the end places its impact effect, reports a hit to the target and returns 7
long EEE3DTracers::Step()
{
    GETransformation theTransformation(this->mTransformation);
    GE3DPoint thePoint;
    long theResult = 0;
    this->mPosition += gPhysics->GetTickTime() * this->mSpeed;
    if (this->mPosition <= 1.0f) {
        this->mPath.GetPointAt(this->mPosition, thePoint);
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        EEGraphicEffect* theImpact = this->mGraphicEffect->mLinkedEffect1;
        if (theImpact) {
            GE3DPoint theLocation(this->mTransformation);
            gEffectManager->CreatePlacedEffect(theLocation, theImpact, NULL);
        }
        if (this->mUnit)
            this->mUnit->mUnitClass->UnitDamaged(this->mUnit->mComplexUnit, NULL, 0);
        theResult = 7;
    }
    return theResult;
}
