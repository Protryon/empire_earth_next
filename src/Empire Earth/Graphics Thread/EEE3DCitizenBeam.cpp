// C:\EEX\Empire Earth\Graphics Thread\EEE3DCitizenBeam.cpp (no __FILE__; name from the RTTI class and the key).
// 0xe9474..0xe9bee: the key chain, ctors .. Virtual4C; EEE3DCommandConfirmation.cpp follows.
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
#include "ee/EEE3DCitizenBeam.h"

// SYNTHETIC: EXE 0x4e9474
// SYNTHETIC: EXE 0x4e947e
// SYNTHETIC: EXE 0x4e948f
// SYNTHETIC: EXE 0x4e949b
const UString EEE3DCitizenBeam::sData("EEE3DCitizenBeam-Data");

// FUNCTION: EXE 0x4e94b7
// Creates an inactive beam from a unit to a target point raised by the effect's first parameter
EEE3DCitizenBeam::EEE3DCitizenBeam(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect,
                                   long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    this->mGraphicObject = NULL;
    this->mEnd = inTarget;
    this->mEnd.SetZ(this->mGraphicEffect->GetParam1() + this->mEnd.GetZ());
    this->mActive = false;
}

// SYNTHETIC: EXE 0x4e9568
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e9584
// Creates an empty beam of a kind for loading from a saved game
EEE3DCitizenBeam::EEE3DCitizenBeam(long inKind)
    : EEEffect3D(inKind)
{
}

// FUNCTION: EXE 0x4e95e2
// Destroys the beam, unloading its graphic's media
EEE3DCitizenBeam::~EEE3DCitizenBeam()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE3DCitizenBeam::DrawPass2()
{
    return 0;
}

long EEE3DCitizenBeam::DrawPass4()
{
    return 0;
}

long EEE3DCitizenBeam::DrawPass5()
{
    return 0;
}

// NEAR: one pop edi scheduled later in ours (the mMiddle tail)
// FUNCTION: EXE 0x4e9606
// Aims the beam from the unit's first tag point at its end point: sets its line, its transform (yaw and pitch,
// a quarter unit out) and its middle point
void EEE3DCitizenBeam::Aim()
{
    GE3DPoint theStart;
    GEVector theDirection;
    EEModel* theModel = this->mUnit->GetModel();
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    GETransformation theUnitTransformation(theUnit->mPhysicalModel.GetLocation());
    float theScale = theUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    EETagPointList* theList = theModel->mTagPoints[theUnit->mAnimation];
    EETagPointList::Scale(theUnitTransformation, theScale);
    theList->GetTagPoint(0)->GetPoint(theFrame, theUnitTransformation, theStart);
    theDirection.SetX(this->mEnd.GetX() - theStart.GetX());
    theDirection.SetY(this->mEnd.GetY() - theStart.GetY());
    theDirection.SetZ(this->mEnd.GetZ() - theStart.GetZ());
    this->mLine.SetStart(theStart);
    this->mLine.SetDirection(theDirection);
    theDirection.Normalize();
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    GE3DPoint thePosition(theDirection.GetX() * 0.25f + theStart.GetX(), theDirection.GetY() * 0.25f + theStart.GetY(),
                          theDirection.GetZ() * 0.25f + theStart.GetZ());
    GETransformation theTransformation(false);
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    this->SetTransformation(theTransformation);
    this->mMiddle.SetX(theDirection.GetX() * 0.5f + theStart.GetX());
    this->mMiddle.SetY(theDirection.GetY() * 0.5f + theStart.GetY());
    this->mMiddle.SetZ(theDirection.GetZ() * 0.5f + theStart.GetZ());
}

// FUNCTION: EXE 0x4e97d7
// Queues the beam in render queue 4 under its texture, snapshotting whether it is active and its position
long EEE3DCitizenBeam::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawActive = this->mActive;
        this->mTransformation.GetPosition(this->mDrawLocation);
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DCitizenBeam::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e9855
// Loads the beam's graphic and sets its bounding radius to the effect's size; returns the load result
long EEE3DCitizenBeam::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4e9872
// Draws an active beam's model additively at its position and scale, lit by the terrain light; returns the draw result
long EEE3DCitizenBeam::DrawPass6()
{
    GEModel* theModel = this->mGraphicObject->mModel;
    unsigned long theScreen = gScreenIndex;
    float theValue;
    if (this->mDrawActive == false)
        return 0;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    SetWhiteLighting();
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    const GE3DPoint& theLocation = this->mDrawLocation;
    GetFogFactor(theLocation, theFog, theValue);
    GETransformation theTransformation(this->mTransformation);
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(this->mScale);
    gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e99fd
// Reloads the beam's graphic and reads its end point, line, middle point and active flag; returns the first error
long EEE3DCitizenBeam::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = inFile.Read(this->mF9C);
    if (theResult == 0)
        theResult = this->mEnd.Load(inFile);
    if (theResult == 0)
        theResult = this->mLine.Load(inFile);
    if (theResult == 0)
        theResult = this->mMiddle.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mActive);
    return theResult;
}

// RepairPointers (relinks the effect's pointers): ICF-folded into EEE3DBlastWave's 0xe93e0
long EEE3DCitizenBeam::RepairPointers()
{
    return EEEffect::RepairPointers();
}

// FUNCTION: EXE 0x4e9a7d
// Writes the base 3D effect and the beam's end point, line, middle point and active flag; returns the first error
long EEE3DCitizenBeam::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mF9C);
    if (theResult == 0)
        theResult = this->mEnd.Save(outFile);
    if (theResult == 0)
        theResult = this->mLine.Save(outFile);
    if (theResult == 0)
        theResult = this->mMiddle.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mActive);
    return theResult;
}

// FUNCTION: EXE 0x4e9aec
// Turns the beam on during the effect's animation frame window (hidden for cloaked enemies), aiming it and placing
// the linked effect when it switches on; flickers its scale with game time; always returns 0
long EEE3DCitizenBeam::Step()
{
    bool theActive = false;
    EEUnit* theOwner = this->mUnit;
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(theOwner);
    if (theUnit) {
        long theFrame = theUnit->mPhysicalModel.GetFrame();
        if (theUnit->mAnimation == this->mGraphicEffect->mIntParam1 && theFrame >= this->mGraphicEffect->mIntParam2
            && theFrame < this->mGraphicEffect->mIntParam3) {
            theActive = true;
            bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
            EEPlayer* thePlayer;
            EEPlayer* theCurrent;
            if (theCloaked && (thePlayer = theUnit->mPlayer) != (theCurrent = gWorld.GetCurrentPlayer())
                && theCurrent->mEnemies[thePlayer->mPlayerID] != 0) {
                theActive = false;
            } else if (this->mActive == false) {
                this->Aim();
                if (this->mGraphicEffect->mLinkedEffect1)
                    gEffectManager->CreatePlacedEffect(this->mMiddle, this->mGraphicEffect->mLinkedEffect1, NULL);
            }
        }
    }
    this->mActive = theActive;
    if (theActive)
        this->mScale = (1.0f - ((unsigned long)gPhysics->mGameTime & 7) * 0.03f) * this->mGraphicEffect->GetSize();
    return 0;
}
