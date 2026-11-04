// C:\EEX\Empire Earth\Graphics Thread\EEE2DPulseWave.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DPulseWave-Data"). 0xe5c63..0xe614f: the key chain, both ctors .. Step; EEE2DShockwave.cpp follows.
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
#include "ee/EEE2DPulseWave.h"
#include "LostDebug.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e5c63
// SYNTHETIC: EXE 0x4e5c6d
// SYNTHETIC: EXE 0x4e5c7e
// SYNTHETIC: EXE 0x4e5c8a
const UString EEE2DPulseWave::sData("EEE2DPulseWave-Data");

// FUNCTION: EXE 0x4e5ca6
// Creates a pulse wave around a unit at age 0 and radius 0, growing
EEE2DPulseWave::EEE2DPulseWave(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, kEffect2DPulseWave, false), mGraphicObject(NULL), mAge(0.0f), mDrawAge(0.0f),
      mShrinking(false)
{
    this->mRadius = 0.0f;
}

// SYNTHETIC: EXE 0x4e5ce3
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e5cff
// Creates an empty pulse wave for loading from a saved game
EEE2DPulseWave::EEE2DPulseWave()
    : EEEffect2D(gU4E8DF4, kEffect2DPulseWave, false)
{
}

// FUNCTION: EXE 0x4e5d1b
// Destroys the pulse wave, unloading its graphic's media
EEE2DPulseWave::~EEE2DPulseWave()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DPulseWave::DrawPass2()
{
    return 0;
}

long EEE2DPulseWave::DrawPass4()
{
    return 0;
}

long EEE2DPulseWave::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e5d39
// Queues the wave in render queue 4 under its texture for this screen, snapshotting its age and radius
long EEE2DPulseWave::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawRadius = this->mRadius;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DPulseWave::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e5da0
// Loads the wave's graphic and starts it at the full radius (parameter 3); returns the load result
long EEE2DPulseWave::SetUp()
{
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mRadius = this->mGraphicEffect->GetParam3();
    return theResult;
}

// NEAR: argument and stack-cleanup scheduling.
// FUNCTION: EXE 0x4e5dc6
// Draws the wave as an additive camera-facing sprite sized by its radius, lit by fog; past 90% of life the light
// fades linearly to zero; returns the draw result
long EEE2DPulseWave::DrawPass6()
{
    float theValue;
    GETransformation theTransformation(false);
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    Fn10F7C8();
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    float theScale = (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mDrawRadius
                     + this->mGraphicEffect->GetStartSize();
    theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, 0.0f);
    const GE3DPoint& thePosition = this->mPosition;
    theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ() + 0.2f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    Fn11A130(thePosition, theFog, theValue);
    if (this->mDrawAge < 0.9f) {
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    } else {
        // Fade from full light at 90% of life to none at the end (the factor 10 keeps the fade continuous).
        float theLight = (1.0f - this->mDrawAge) * theValue * 10.0f;
        gRasterizer->SetAmbientColor(GEColor(theLight, theLight, theLight), theScreen);
    }
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e5fa0
// Reloads the wave's graphic and reads its age, radius, last seen hit points and shrinking flag; returns the first
// read error (base result ignored)
long EEE2DPulseWave::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mAge);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mRadius);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mHitPoints);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mShrinking);
    return theResult;
}

// FUNCTION: EXE 0x4e6004
// Writes the wave's age, radius, last seen hit points and shrinking flag; returns the first write error
long EEE2DPulseWave::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mRadius);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mHitPoints);
    if (theResult == 0)
        theResult = outFile.Write(this->mShrinking);
    return theResult;
}

// FUNCTION: EXE 0x4e605a
// Follows the unit; when it loses hit points the wave jumps to full radius and shrinks to parameter 2, else grows
// back by parameter 1; returns 7 (remove) when the unit dies or the wave's life ends
long EEE2DPulseWave::Step()
{
    long theResult = 0;
    if (this->mUnit->mHitPoints > 0) {
        this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
        if (this->mAge <= 1.0f) {
            GE3DPoint thePosition(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
            this->SetPosition(thePosition);
            if (this->mShrinking) {
                this->mRadius = max(this->mGraphicEffect->GetParam2(), this->mRadius - this->mGraphicEffect->GetParam1());
                this->mShrinking = this->mRadius != this->mGraphicEffect->GetParam2() ? 1 : 0;
            } else if (this->mHitPoints > this->mUnit->mHitPoints) {
                this->mRadius = this->mGraphicEffect->GetParam3();
                this->mShrinking = true;
            } else {
                this->mRadius = min(this->mGraphicEffect->GetParam3(), this->mGraphicEffect->GetParam1() + this->mRadius);
            }
            this->mHitPoints = this->mUnit->mHitPoints;
        } else {
            theResult = 7;
        }
    } else {
        theResult = 7;
    }
    return theResult;
    // LOST-DEBUG: none: exact only (tools lostdebug_search)
    EE_LOST_DEBUG(5);
}
