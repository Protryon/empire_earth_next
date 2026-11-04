// C:\EEX\Empire Earth\Graphics Thread\EEE3DLinearProjectile.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DLinearProjectile-Data"). 0xf2d93..0xf3580: the key chain, both ctors .. Virtual4C; EEE3DMarchingLine.cpp follows
// (its ??_G 0xf3580). Unit flags: the exe's module set.
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
#include "ee/EEE3DLinearProjectile.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4f2d93
// SYNTHETIC: EXE 0x4f2d9d
// SYNTHETIC: EXE 0x4f2dae
// SYNTHETIC: EXE 0x4f2dba
const UString EEE3DLinearProjectile::sData("EEE3DLinearProjectile-Data");

// FUNCTION: EXE 0x4f2dd6
// Creates a projectile flying straight from one point to another in about the effect's duration (+-12.5% at random)
// (the 0.001 factor 0x43c2a4 is `/ 1000.0f`: VC6 multiplies by the unrounded reciprocal)
EEE3DLinearProjectile::EEE3DLinearProjectile(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DLinearProjectile)
{
    GETransformation theTransformation;
    float theDuration = this->mGraphicEffect->mDuration;
    this->mGraphicObject = NULL;
    this->mPosition = 0.0f;
    this->mDrawPosition = 0.0f;
    GEVector theDirection(inTo.GetX() - inFrom.GetX(), inTo.GetY() - inFrom.GetY(), inTo.GetZ() - inFrom.GetZ());
    this->mPath.SetStart(inFrom);
    this->mPath.SetDirection(theDirection);
    theTransformation.SetPosition(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
    this->SetTransformation(theTransformation);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mSpeed = 1000.0f / (((EEEffect::GetRandom() - 0.5f) * theDuration) * 0.25f + theDuration) / 1000.0f;
}

// SYNTHETIC: EXE 0x4f2f29
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f2f45
// Creates an empty projectile for loading from a saved game
EEE3DLinearProjectile::EEE3DLinearProjectile()
    : EEEffect3D(kEffect3DLinearProjectile)
{
}

// FUNCTION: EXE 0x4f2f9f
// Destroys the projectile, unloading its graphic and deleting its orbitals
EEE3DLinearProjectile::~EEE3DLinearProjectile()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    for (long i = 0; i < this->mOrbitalCount; i++)
        delete this->mOrbitals[i];
}

long EEE3DLinearProjectile::DrawPass2()
{
    return 0;
}

long EEE3DLinearProjectile::DrawPass4()
{
    return 0;
}

long EEE3DLinearProjectile::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4f2ffe
// Queues the projectile in render queue 3, snapshotting its progress, position and orbital angles
// NEAR: as EEE3DDisease::Queue (the result in [ebp-8], inQueues in ebx, mOrbitals[i] through operator+)
long EEE3DLinearProjectile::Queue(EEEffectQueue* inQueues)
{
    long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawPosition = this->mPosition;
        this->mTransformation.GetPosition(this->mLocation);
        for (i = 0; i < this->mOrbitalCount; i++)
            this->mAngles[i] = this->mOrbitals[i]->mAngle;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f30ba
// Loads the graphic and copies its orbitals at random starting angles; returns the load result
long EEE3DLinearProjectile::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
        this->mOrbitals[i]->mAngle = EEEffect::GetRandom() * 6.2831855f;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f3172
// Draws the projectile's model with its orbitals turned, fading out over the last fifth of the path; returns the
// draw result
long EEE3DLinearProjectile::DrawPass5()
{
    GETransformation theOrientation;
    GETransformation theTransformation;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    Fn10F71C();
    const GE3DPoint& theLocation = this->mLocation;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    gRasterizer->SetShadingColorSource(0, theScreen);
    theModel->SetGraphicsScale(theScale);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        theOrientation.SetOrientationYPR(this->mAngles[i], 0.0f, 0.0f);
        this->mOrbitals[i]->mModel->SetGraphicsPreModelTransform(theOrientation);
    }
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theModel->PrepareTransforms();
    long theResult;
    if (this->mDrawPosition >= 0.8f) {
        theModel->SetOpacity((1.0f - this->mDrawPosition) * 5.0f);
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
        theModel->SetOpacity(1.0f);
    } else {
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DLinearProjectile::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f3373
// Reloads the graphic and orbitals (random angles) and reads progress, speed and path; returns the first error
long EEE3DLinearProjectile::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
        this->mOrbitals[i]->mAngle = EEEffect::GetRandom() * 6.2831855f;
    }
    long theResult = inFile.Read(this->mPosition);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSpeed);
    if (theResult == kErrorOK)
        theResult = this->mPath.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x4f346d
// Writes the base 3D effect (result ignored), progress, speed and path; returns the first write error
long EEE3DLinearProjectile::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mPosition);
    if (theResult == 0)
        theResult = outFile.Write(this->mSpeed);
    if (theResult == 0)
        theResult = this->mPath.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x4f34b9
// Updates the orbitals and moves the projectile along its path; returns 7 (remove) past the end
long EEE3DLinearProjectile::Step()
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    long theResult = 0;
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals[i]->Update();
    this->mPosition += gPhysics->GetTickTime() * this->mSpeed;
    if (this->mPosition <= 1.0f) {
        this->mPath.GetPointAt(this->mPosition, thePoint);
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        theResult = 7;
        this->mPosition = 1.0f;
    }
    return theResult;
}
