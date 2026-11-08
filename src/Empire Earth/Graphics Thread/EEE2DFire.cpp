// C:\EEX\Empire Earth\Graphics Thread\EEE2DFire.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DFire-Data"). 0xe0389..0xe0cf5: the key chain, both ctors .. UpdatePart; EEE2DFireTrail.cpp follows. Its
// sound helpers are ICF-folded into EEE2DExplosion.cpp's.
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
#include "ee/EEE2DFire.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4e0389
// SYNTHETIC: EXE 0x4e0393
// SYNTHETIC: EXE 0x4e03a4
// SYNTHETIC: EXE 0x4e03b0
const UString EEE2DFire::sData("EEE2DFire-Data");

// FUNCTION: EXE 0x4e03cc
// Creates a fire effect on a unit, with no sound playing
EEE2DFire::EEE2DFire(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, kEffect2DFire, false), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4e03f6
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e0412
// Creates an empty fire effect for loading from a saved game
EEE2DFire::EEE2DFire()
    : EEEffect2D(gEffect2DModel, kEffect2DFire, false), mSound(NULL)
{
}

// FUNCTION: EXE 0x4e0432
// Destroys the fire, stopping and releasing its sound
EEE2DFire::~EEE2DFire()
{
    this->StopSound();
}

void EEE2DFire::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

void EEE2DFire::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE2DFire::DrawPass2()
{
    return 0;
}

long EEE2DFire::DrawPass4()
{
    return 0;
}

long EEE2DFire::DrawPass5()
{
    return 0;
}

// NEAR: the original calls the iterator operator+ (0xdfcee) out of line in the part loop; ours expands it (inline budget),
// so registers and frame differ
// FUNCTION: EXE 0x4e0449
// Queues the fire in render queue 4 while the unit is alive and at or below the effect's health threshold,
// showing more parts the more damaged it is; otherwise returns 9 (skip)
long EEE2DFire::Queue(EEEffectQueue* inQueues)
{
    float theThreshold = this->mGraphicEffect->mParam2;
    EEUnit* theUnit = this->mUnit;
    unsigned long theCount = this->mParts.size();
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        for (unsigned long i = 0; i < theCount; i++)
            (this->mParts.begin() + i)->mDrawFrame = this->mParts[i].mFrame;
        float theHealth = (float)theUnit->mHitPoints / theUnit->mStats->mMaxHitPoints;
        if (theHealth <= theThreshold && theUnit->mHitPoints > 0) {
            float theDamage = 1.0f - theHealth / theThreshold;
            this->mLastPart = min(UMFloatToLong(this->mParts.size() * theDamage), (long)this->mParts.size() - 1);
            GETexture* theTexture;
            theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
            inQueues[4][theTexture].push_back(this);
        } else {
            theResult = 9;
        }
    }
    return theResult;
}

void EEE2DFire::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e05e6
// Creates the fire's sprite parts raised by half the effect size, each starting on a random frame; returns 0
long EEE2DFire::SetUp()
{
    GE3DPoint thePosition;
    float theHeight;
    float theOffset = this->mGraphicEffect->GetSize() * 0.5f;
    this->CreateParts(thePosition, theHeight, 0.0f);
    unsigned long theCount = this->mParts.size();
    for (unsigned long i = 0; i < theCount; i++) {
        float theZ = this->mParts[i].mPosition.GetZ() + theOffset;
        this->mParts[i].mPosition.SetZ(theZ);
        unsigned long theStart = this->mParts[i].mGraphicObject->mStartFrames[0];
        this->mParts[i].mFrame = theStart + GetEffectRandomLong(0, this->mParts[i].mGraphicObject->mFrameCounts[0] - 1);
    }
    this->SetPosition(thePosition);
    this->SetBoundingRadius(theHeight);
    return 0;
}

// FUNCTION: EXE 0x4e06d1
// Draws the shown parts as camera-facing additive sprites lit by the fog at each part; returns the first draw error
long EEE2DFire::DrawPass6()
{
    GETransformation theTransformation;
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    long theResult = 0;
    unsigned long theScreen = gScreenIndex;
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theCount = this->mLastPart + 1;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, 0.0f);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetWhiteLighting();
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        GETexture* theTexture = this->mParts[i].mGraphicObject->mTextures[0];
        theTexture->SetFrame((this->mParts.begin() + i)->mDrawFrame);
        theMaterial->SetTexture(theTexture, theScreen);
        float theValue;
        GetFogFactor(this->mParts[i].mPosition, theFog, theValue);
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
        const GE3DPoint& thePosition = this->mParts[i].mPosition;
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

// FUNCTION: EXE 0x4e08d1
// Reads the number of shown parts; returns the read result (the base class result is ignored)
long EEE2DFire::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    return inFile.Read(this->mLastPart);
}

// FUNCTION: EXE 0x4e08f7
// Writes the number of shown parts; returns the write result (the base class result is ignored)
long EEE2DFire::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    return outFile.Write(this->mLastPart);
}

// FUNCTION: EXE 0x4e0920
// Follows the unit and animates every part while it is alive and damaged enough; else returns 7 (remove)
long EEE2DFire::Step()
{
    GE3DPoint theLocation = this->GetUnitLocation();
    float theThreshold = this->mGraphicEffect->mParam2;
    EEUnit* theUnit = this->mUnit;
    long theResult = 0;
    unsigned long theCount = this->mParts.size();
    if (this->mPosition != theLocation)
        this->SetPosition(theLocation);
    if ((float)theUnit->mHitPoints / theUnit->mStats->mMaxHitPoints <= theThreshold && theUnit->mHitPoints > 0) {
        for (unsigned long i = 0; i < theCount; i++)
            this->UpdatePart(i);
    } else {
        theResult = 7;
    }
    return theResult;
}

// FUNCTION: EXE 0x4e09cb
// Moves a part to its model tag on the unit and loops its animation (frame modulo the frame count, ignoring the start
// frame); the first part also drives the fire sound
void EEE2DFire::UpdatePart(unsigned long inIndex)
{
    GE3DPoint theOffset;
    GE3DPoint theLocation = this->mPosition;
    EEComplexUnit* theUnit = this->mUnit->GetComplexUnit();
    float theHalf = this->mGraphicEffect->GetSize() * 0.5f;
    float theTime = this->mParts[inIndex].mTime;
    unsigned long theOldFrame = this->mParts[inIndex].mFrame;
    unsigned long theFrameTime = this->mParts[inIndex].mGraphicObject->mFrameTimes[0];
    unsigned long theFrameCount = this->mParts[inIndex].mGraphicObject->mFrameCounts[0];
    unsigned long theStartFrame = this->mParts[inIndex].mGraphicObject->mStartFrames[0];
    unsigned long theFrame = theOldFrame;
    bool theFirstPart = inIndex == 0;
    bool theStart;
    if (this->mParts[inIndex].mFrame == theStartFrame && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    if (theUnit != NULL) {
        if (this->mParts[inIndex].mTag != 10000) {
            GEModel* theModel = theUnit->GetModel()->mModel;
            unsigned long theModelFrame = theUnit->mPhysicalModel.GetFrame();
            theOffset = theModel->GetTagPoint(this->mParts[inIndex].mTag, theModelFrame);
        }
        theUnit->GetPhysicalModel().GetLocation().Apply(theOffset, this->mParts[inIndex].mPosition);
        theHalf += this->mParts[inIndex].mPosition.GetZ();
        this->mParts[inIndex].mPosition.SetZ(theHalf);
    }
    theTime = gPhysics->GetTickTime() + theTime;
    if (theTime >= (float)theFrameTime) {
        theFrame = UMFloatToLong(theTime / (float)theFrameTime);
        theTime -= (float)(theFrame * theFrameTime);
        // loop within the graphic's frames [start, start + count), where SetUp starts the parts
        theFrame = theStartFrame + (theFrame + theOldFrame - theStartFrame) % theFrameCount;
    }
    this->mParts[inIndex].mFrame = theFrame;
    this->mParts[inIndex].mTime = theTime;
    if (!theFirstPart)
        return;
    bool theChanged;
    if (theStart || theFrame != theOldFrame)
        theChanged = true;
    else
        theChanged = false;
    this->SetSound(PlayAnimationSound(theLocation, this->mParts[0].mGraphicObject, 0, this->mSound, theFrame, theChanged));
    if (this->mSound)
        this->mSound->Dereference();
}

void EEE2DFire::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
