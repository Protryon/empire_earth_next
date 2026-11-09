// C:\EEX\Empire Earth\Graphics Thread\EEE2DLightning.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DLightning-Data"). 0xe28be..0xe2edd: the key chain, three ctors .. SetSound; EEE2DLinearProjectile.cpp follows.
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
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEE2DLightning.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e28be
// SYNTHETIC: EXE 0x4e28c8
// SYNTHETIC: EXE 0x4e28d9
// SYNTHETIC: EXE 0x4e28e5
const UString EEE2DLightning::sData("EEE2DLightning-Data");

// FUNCTION: EXE 0x4e2901
// Creates a lightning effect on a unit, with no graphic or sound yet
EEE2DLightning::EEE2DLightning(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DLightning, false), mGraphicObject(NULL), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4e292f
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e294b
// Creates a lightning effect at a point, with no unit, graphic or sound yet
EEE2DLightning::EEE2DLightning(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, kEffect2DLightning, false), mGraphicObject(NULL), mSound(NULL)
{
    this->SetPosition(inPosition);
}

// FUNCTION: EXE 0x4e2982
// Creates an empty lightning effect for loading from a saved game
EEE2DLightning::EEE2DLightning()
    : EEEffect2D(gEffect2DModel, kEffect2DLightning, false), mSound(NULL)
{
}

// FUNCTION: EXE 0x4e29a2
// Destroys the lightning, stopping its sound and unloading its graphic's media
EEE2DLightning::~EEE2DLightning()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

// FUNCTION: EXE 0x4e29c5
// Stops the lightning's sound, if any, and releases it
void EEE2DLightning::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4e29e7
// Drops the lightning's reference to its sound
void EEE2DLightning::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE2DLightning::DrawPass2()
{
    return 0;
}

long EEE2DLightning::DrawPass4()
{
    return 0;
}

long EEE2DLightning::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e2a0b
// Queues the lightning in render queue 4 under its texture for this screen, snapshotting its frame
long EEE2DLightning::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DLightning::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e2a6c
// Loads the lightning's graphic at its first frame and places it half its size above the unit's model point (or
// its own point); returns the load result
long EEE2DLightning::SetUp()
{
    GE3DPoint theOffset;
    GE3DPoint thePosition;
    GETransformation theTransformation(false);
    EEUnit* theUnit = this->mUnit;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[0];
    this->mTime = 0.0f;
    this->mDrawFrame = 0;
    if (theUnit != NULL && theUnit->mComplexUnit != NULL) {
        theTransformation = theUnit->mComplexUnit->mPhysicalModel.GetLocation();
        theOffset = theUnit->GetModel()->mDefaultPoint;
        float theScale = theUnit->mStats->mScale;
        theTransformation.SetElement(0, 0, theTransformation.GetElement(0, 0) * theScale);
        theTransformation.SetElement(0, 1, theTransformation.GetElement(0, 1) * theScale);
        theTransformation.SetElement(0, 2, theTransformation.GetElement(0, 2) * theScale);
        theTransformation.SetElement(1, 0, theTransformation.GetElement(1, 0) * theScale);
        theTransformation.SetElement(1, 1, theTransformation.GetElement(1, 1) * theScale);
        theTransformation.SetElement(1, 2, theTransformation.GetElement(1, 2) * theScale);
        theTransformation.SetElement(2, 0, theTransformation.GetElement(2, 0) * theScale);
        theTransformation.SetElement(2, 1, theTransformation.GetElement(2, 1) * theScale);
        theTransformation.SetElement(2, 2, theTransformation.GetElement(2, 2) * theScale);
        theTransformation.Apply(theOffset, thePosition);
    } else {
        thePosition = this->mPosition;
    }
    thePosition.SetZ(thePosition.GetZ() + this->mGraphicEffect->GetSize() * 0.5f);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e2ba8
// Draws the lightning as an additive fog-lit sprite only on frames 0 and 2 (a flicker); returns the draw result
long EEE2DLightning::DrawPass6()
{
    GETransformation theTransformation(gWorld.mTransformation);
    float theValue;
    float theScale = this->mGraphicEffect->mSize;
    long theResult = 0;
    GEModel* theModel = this->mModel;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    if (this->mDrawFrame == 0 || this->mDrawFrame == 2) {
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
        gRasterizer->SetShadingColorSource(0, theScreen);
        SetWhiteLighting();
        theModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
        const GE3DPoint& thePosition = this->mPosition;
        GetFogFactor(thePosition, theFog, theValue);
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
        theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->SetGraphicsScale(theScale);
        theModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    }
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e2d31
// Reloads the lightning's graphic and reads its frame time and frame; returns the first error (base result ignored)
long EEE2DLightning::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mFrame);
    return theResult;
}

// FUNCTION: EXE 0x4e2d7b
// Writes the lightning's frame time and frame; returns the first write error (the base class result is ignored)
long EEE2DLightning::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mFrame);
    return theResult;
}

// FUNCTION: EXE 0x4e2db3
// Animates the lightning and plays its sound; returns 7 (remove) after its last frame
long EEE2DLightning::Step()
{
    GE3DPoint theLocation = this->mPosition;
    long theResult = 0;
    unsigned long theOldFrame = this->mFrame;
    unsigned long theFrame = theOldFrame;
    float theTime = this->mTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[0];
    unsigned long theLastFrame = this->mGraphicObject->mLastFrames[0];
    bool theStart;
    if (theFrame == this->mGraphicObject->mStartFrames[0] && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    theTime = gPhysics->GetTickTime() + theTime;
    if (theTime >= (float)theFrameTime) {
        theFrame = UMFloatToLong(theTime / (float)theFrameTime);
        theTime -= (float)(theFrame * theFrameTime);
        theFrame += theOldFrame;
        if (theFrame > theLastFrame)
            theResult = 7;
    }
    if (theResult == 0) {
        this->mTime = theTime;
        this->mFrame = theFrame;
        bool theChanged;
        if (theStart || theFrame != theOldFrame)
            theChanged = true;
        else
            theChanged = false;
        this->SetSound(PlayAnimationSound(theLocation, this->mGraphicObject, 0, this->mSound, theFrame, theChanged));
        if (this->mSound)
            this->mSound->Dereference();
    }
    return theResult;
}

// FUNCTION: EXE 0x4e2ebc
// Replaces the lightning's sound, releasing the old one and referencing the new one
void EEE2DLightning::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
