// C:\EEX\Empire Earth\Graphics Thread\EEE2DPlasmaPulse.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DPlasmaPulse-Data"). 0xe56d0..0xe5c63: the key chain, both ctors .. SetSound; EEE2DPulseWave.cpp follows.
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
#include "ee/EEE2DPlasmaPulse.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e56d0
// SYNTHETIC: EXE 0x4e56da
// SYNTHETIC: EXE 0x4e56eb
// SYNTHETIC: EXE 0x4e56f7
const UString EEE2DPlasmaPulse::sData("EEE2DPlasmaPulse-Data");

// FUNCTION: EXE 0x4e5713
// Creates a plasma pulse effect on a unit, with no graphic or sound yet
EEE2DPlasmaPulse::EEE2DPlasmaPulse(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DPlasmaPulse, false), mGraphicObject(NULL), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4e5750
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e576c
// Creates an empty plasma pulse for loading from a saved game
EEE2DPlasmaPulse::EEE2DPlasmaPulse()
    : EEEffect2D(gEffect2DModel, kEffect2DPlasmaPulse, false), mSound(NULL)
{
}

// FUNCTION: EXE 0x4e579b
// Destroys the pulse, stopping its sound and unloading its graphic's media
EEE2DPlasmaPulse::~EEE2DPlasmaPulse()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

// FUNCTION: EXE 0x4e57be
// Stops the pulse's sound, if any, and releases it
void EEE2DPlasmaPulse::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4e57e0
// Drops the pulse's reference to its sound
void EEE2DPlasmaPulse::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE2DPlasmaPulse::DrawPass2()
{
    return 0;
}

long EEE2DPlasmaPulse::DrawPass4()
{
    return 0;
}

long EEE2DPlasmaPulse::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e5804
// Queues the pulse in render queue 4 under its texture, snapshotting its frame and position (also the fire trail's)
long EEE2DPlasmaPulse::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawPosition = this->mPosition;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DPlasmaPulse::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e5877
// Loads the pulse's graphic at its first frame and places it at the unit; returns the load result
long EEE2DPlasmaPulse::SetUp()
{
    GE3DPoint thePosition;
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    unsigned long theFrame = this->mGraphicObject->mStartFrames[0];
    this->mTime = 0.0f;
    this->mDrawFrame = 0;
    this->mFrame = theFrame;
    this->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(thePosition);
    this->SetPosition(thePosition);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e58f0
// Draws the pulse as an additive fog-lit sprite at its snapshotted position; returns the draw result
long EEE2DPlasmaPulse::DrawPass6()
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

// FUNCTION: EXE 0x4e5a78
// Reloads the pulse's graphic and reads its frame time and frame; returns the first read error (base result ignored)
long EEE2DPlasmaPulse::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mFrame);
    return theResult;
}

// FUNCTION: EXE 0x4e5abe
// Writes the pulse's frame time and frame; returns the first write error (the base class result is ignored)
long EEE2DPlasmaPulse::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mFrame);
    return theResult;
}

// FUNCTION: EXE 0x4e5af6
// Follows the unit, loops the animation (frame modulo the frame count) and plays its sound; returns 7 (remove)
// once the unit has no hit points
long EEE2DPlasmaPulse::Step()
{
    GE3DPoint theLocation;
    long theResult = 0;
    unsigned long theOldFrame = this->mFrame;
    unsigned long theFrame = theOldFrame;
    float theTime = this->mTime;
    unsigned long theFrameCount = this->mGraphicObject->mFrameCounts[0];
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[0];
    bool theStart;
    if (theFrame == this->mGraphicObject->mStartFrames[0] && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    if (this->mUnit->mHitPoints == 0) {
        theResult = 7;
    } else {
        const GETransformation& theUnitLocation = this->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        theLocation.SetX(theUnitLocation.GetX());
        theLocation.SetY(theUnitLocation.GetY());
        theLocation.SetZ(theUnitLocation.GetZ());
        this->SetPosition(theLocation);
        theTime = gPhysics->GetTickTime() + theTime;
        if (theTime >= (float)theFrameTime) {
            theFrame = UMFloatToLong(theTime / (float)theFrameTime);
            theTime -= (float)(theFrame * theFrameTime);
            theFrame = (theFrame + theOldFrame) % theFrameCount;
        }
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

// FUNCTION: EXE 0x4e5c42
// Replaces the pulse's sound, releasing the old one and referencing the new one
void EEE2DPlasmaPulse::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
