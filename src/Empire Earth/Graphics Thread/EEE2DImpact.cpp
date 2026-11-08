// C:\EEX\Empire Earth\Graphics Thread\EEE2DImpact.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DImpact-Data"). 0xe0ffa..0xe1867: the key chain, three ctors .. SetSound, then EETagPointList::Scale (its
// first user is UpdatePosition); EEE2DLightGlow.cpp follows.
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
#include "ee/EEE2DImpact.h"

// SYNTHETIC: EXE 0x4e0ffa
// SYNTHETIC: EXE 0x4e1004
// SYNTHETIC: EXE 0x4e1015
// SYNTHETIC: EXE 0x4e1021
const UString EEE2DImpact::sData("EEE2DImpact-Data");

// FUNCTION: EXE 0x4e103d
// Creates an impact effect of the given kind on a unit, with no graphic or sound yet
EEE2DImpact::EEE2DImpact(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gEffect2DModel, inKind, false), mLifeFraction(0.0f), mGraphicObject(NULL), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4e1093
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e10af
// Creates an impact effect of the given kind at a point, with no unit (its life fraction starts at zero)
EEE2DImpact::EEE2DImpact(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, inKind, false), mLifeFraction(0.0f)
{
    this->mStart = inPosition;
    this->mGraphicObject = NULL;
    this->mSound = NULL;
}

// FUNCTION: EXE 0x4e1113
// Creates an empty impact effect of the given kind for loading from a saved game
EEE2DImpact::EEE2DImpact(long inKind)
    : EEEffect2D(gEffect2DModel, inKind, false), mSound(NULL)
{
}

// FUNCTION: EXE 0x4e1158
// Destroys the impact, stopping its sound and unloading its graphic's media
EEE2DImpact::~EEE2DImpact()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

// FUNCTION: EXE 0x4e117b
// Stops the impact's sound, if any, and releases it
void EEE2DImpact::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4e11a0
// Drops the impact's reference to its sound
void EEE2DImpact::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE2DImpact::DrawPass2()
{
    return 0;
}

long EEE2DImpact::DrawPass4()
{
    return 0;
}

long EEE2DImpact::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e11ca
// Queues the impact in render queue 4 under its texture for this screen, snapshotting its frame and position
long EEE2DImpact::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawPosition = this->mStart;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DImpact::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e123e
// Loads the impact's graphic at its first frame and places it on a tag point or at its start; returns the load result
long EEE2DImpact::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    unsigned long theFrame = this->mGraphicObject->mStartFrames[0];
    this->mTime = 0.0f;
    this->mDrawFrame = 0;
    this->mFrame = theFrame;
    if (this->mUnit)
        this->PickTagPoint();
    else
        this->SetPosition(this->mStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e1290
// Draws the impact as an additive fog-lit sprite (randomly spun when the effect asks); returns the draw result
long EEE2DImpact::DrawPass6()
{
    GETransformation theTransformation(gWorld.mTransformation);
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
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
    GetFogFactor(this->mDrawPosition, theFog, theValue);
    gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
    theTransformation.SetPosition(this->mDrawPosition);
    if (this->mGraphicEffect->mIntParam1) {
        GETransformation theRotation(false);
        theRotation.SetOrientationYPR(0.0f, 0.0f, GetEffectRandomFloat(0.0f, 3.1415927f));
        GE3DPoint thePosition(theTransformation);
        theRotation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
        theRotation.SetElement(3, 0, 0.0f);
        theRotation.SetElement(3, 1, 0.0f);
        theRotation.SetElement(3, 2, 0.0f);
        theRotation.SetElement(3, 3, 1.0f);
        theModel->SetGraphicsPostModelTransform(theRotation);
    } else {
        theModel->SetGraphicsPostModelTransform(theTransformation);
    }
    theModel->SetGraphicsScale(theScale);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4e1497
// Reloads the impact's graphic and reads its point, timing, frame, life and tag index (saves before 0.245 have none)
// Returns the first read error; the base class result is ignored
long EEE2DImpact::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = this->mStart.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mFrame);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mLifeFraction);
    if (theResult == 0 && (inMajorVersion > 0 || inMinorVersion >= 0xf5))
        theResult = inFile.Read(this->mTagIndex);
    else
        this->mTagIndex = 0;
    return theResult;
}

// FUNCTION: EXE 0x4e1524
// Writes the impact's point, timing, frame, life fraction and tag index; returns the first write error
long EEE2DImpact::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mStart.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFrame);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mLifeFraction);
    if (theResult == 0)
        theResult = outFile.Write(this->mTagIndex);
    return theResult;
}

// FUNCTION: EXE 0x4e158e
// Picks one of the unit's impact tag points (type 6) by game time and moves the impact there
void EEE2DImpact::PickTagPoint()
{
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    if (!theUnit)
        return;
    unsigned long thePart = theUnit->GetAnimation();
    EETagPointList* theList = theUnit->GetModel()->GetTagPoints(thePart, 6);
    if (theList && theList->mPoints.size()) {
        this->mTagIndex = gPhysics->mGameTime % theList->mPoints.size();
        this->UpdatePosition();
    }
}

// NEAR: the frame stays in ebx and the tag-point element address is computed first in the original
// FUNCTION: EXE 0x4e15e3
// Moves the impact to its chosen tag point on the unit's current animation frame (the tag list is not null-checked)
void EEE2DImpact::UpdatePosition()
{
    // An impact placed at a point has no unit to follow.
    if (this->mUnit == NULL)
        return;
    EEComplexUnit* theUnit = this->mUnit->mComplexUnit;
    if (!theUnit)
        return;
    unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
    float theScale = theUnit->mPhysicalModel.GetScale();
    GETransformation theTransformation(theUnit->mPhysicalModel.GetLocation());
    EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 6);
    EETagPointList::Scale(theTransformation, theScale);
    const std::vector<EETagPoint*>& thePoints = theList->mPoints;
    thePoints[this->mTagIndex]->GetPoint(theFrame, theTransformation, this->mStart);
    this->SetPosition(this->mStart);
}

// FUNCTION: EXE 0x4e166b
// Animates the impact and plays its sound; returns 7 (remove) after its last frame unless looping, or when a timed
// impact's life ends or its unit dies (a loop restarts at a fresh tag point)
long EEE2DImpact::Step()
{
    GE3DPoint theLocation = this->mPosition;
    long theResult = 0;
    unsigned long theOldFrame = this->mFrame;
    float theTime = this->mTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[0];
    unsigned long theLastFrame = this->mGraphicObject->mLastFrames[0];
    unsigned long theStartFrame = this->mGraphicObject->mStartFrames[0];
    unsigned long theFrame = theOldFrame;
    bool theStart;
    if (theFrame == theStartFrame && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    theTime = gPhysics->GetTickTime() + theTime;
    if (this->mGraphicEffect->mFlag2) {
        // An impact placed at a point has no unit: only its timed life ends it.
        if (this->mUnit != NULL && this->mUnit->mHitPoints <= 0) {
            theResult = 7;
        } else {
            this->mLifeFraction += gPhysics->GetTickSeconds() * 1000.0f / this->mGraphicEffect->mDuration;
            if (this->mLifeFraction > 1.0f)
                theResult = 7;
        }
    }
    if (theResult == 0 && theTime >= (float)theFrameTime) {
        theFrame = UMFloatToLong(theTime / (float)theFrameTime);
        theTime -= (float)(theFrame * theFrameTime);
        theFrame += theOldFrame;
        if (theFrame > theLastFrame) {
            if (!this->mGraphicEffect->mFlag1) {
                theResult = 7;
            } else {
                theFrame = theStartFrame;
                if (this->mGraphicEffect->mFlag2)
                    this->UpdatePosition();
            }
        }
    }
    if (theResult == 0) {
        this->mFrame = theFrame;
        this->mTime = theTime;
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

// FUNCTION: EXE 0x4e17ed
// Replaces the impact's sound, releasing the old one and referencing the new one
void EEE2DImpact::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// FUNCTION: EXE 0x4e1814
// Scales the rotation part of a transformation by a factor (the translation is kept)
void EETagPointList::Scale(GETransformation& ioTransformation, float inScale)
{
    ioTransformation.SetElement(0, 0, ioTransformation.GetElement(0, 0) * inScale);
    ioTransformation.SetElement(0, 1, ioTransformation.GetElement(0, 1) * inScale);
    ioTransformation.SetElement(0, 2, ioTransformation.GetElement(0, 2) * inScale);
    ioTransformation.SetElement(1, 0, ioTransformation.GetElement(1, 0) * inScale);
    ioTransformation.SetElement(1, 1, ioTransformation.GetElement(1, 1) * inScale);
    ioTransformation.SetElement(1, 2, ioTransformation.GetElement(1, 2) * inScale);
    ioTransformation.SetElement(2, 0, ioTransformation.GetElement(2, 0) * inScale);
    ioTransformation.SetElement(2, 1, ioTransformation.GetElement(2, 1) * inScale);
    ioTransformation.SetElement(2, 2, ioTransformation.GetElement(2, 2) * inScale);
}

// The ICF-shared copy LINK kept in this unit's range (the empty PrepareQueue)
// SYNTHETIC: EXE 0x4e123d
