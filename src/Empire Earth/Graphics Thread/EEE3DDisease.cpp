// C:\EEX\Empire Earth\Graphics Thread\EEE3DDisease.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DDisease-Data"). 0xebde9..0xec6e4: the key chain, both ctors .. SetSound. Unit flags: the exe's module set.
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
#include "ee/EEE3DDisease.h"
#include "LostDebug.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4ebde9
// SYNTHETIC: EXE 0x4ebdf3
// SYNTHETIC: EXE 0x4ebe04
// SYNTHETIC: EXE 0x4ebe10
const UString EEE3DDisease::sData("EEE3DDisease-Data");

// FUNCTION: EXE 0x4ebe2c
// Creates a disease cloud on a unit, with no graphic or sound yet
EEE3DDisease::EEE3DDisease(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DDisease), mGraphicObject(NULL), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4ebea8
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ebec4
// Creates an empty disease cloud for loading from a saved game
EEE3DDisease::EEE3DDisease()
    : EEEffect3D(kEffect3DDisease), mSound(NULL)
{
}

// FUNCTION: EXE 0x4ebf1d
// Destroys the cloud: stops its sound, unloads its graphic and deletes its orbitals
EEE3DDisease::~EEE3DDisease()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    for (long i = 0; i < this->mOrbitalCount; i++)
        delete this->mOrbitals[i];
}

// FUNCTION: EXE 0x4ebf81
// Stops the cloud's sound, if playing, and releases it
void EEE3DDisease::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4ebfa6
// Drops the cloud's reference to its sound
void EEE3DDisease::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE3DDisease::DrawPass2()
{
    return 0;
}

long EEE3DDisease::DrawPass4()
{
    return 0;
}

long EEE3DDisease::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4ebfd0
// Queues the cloud in render queue 3 under its texture, snapshotting its frame, age, position and orbital angles
// NEAR: the original keeps the result in [ebp-8] and inQueues in ebx, and calls mOrbitals[i]'s _Ptrit::operator+ out
// of line (more inline pressure than ours; EE_LOST_DEBUG 1..20 does not reproduce both)
long EEE3DDisease::Queue(EEEffectQueue* inQueues)
{
    long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawAge = this->mAge;
        this->mTransformation.GetPosition(this->mLocation);
        for (i = 0; i < this->mOrbitalCount; i++)
            this->mAngles[i] = this->mOrbitals[i]->mAngle;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4ec098
// Loads the graphic, starts its animation 13, copies the graphic's orbitals and moves the cloud onto the unit
// mFrame is assigned before mTime (the original stores mTime after the mStartFrames[13] load)
long EEE3DDisease::SetUp()
{
    // LOST-DEBUG: none independent: the stand-in at the end makes begin()/operator+ of the orbital copy out of line

    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[13];
    this->mTime = 0.0f;
    this->mAge = 0.0f;
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    this->SetTransformation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mSound = NULL;
    return theResult;
    EE_LOST_DEBUG(6);   // LOST-DEBUG: inline-budget stand-in: the smallest n that makes this function exact (end of body)
}

// FUNCTION: EXE 0x4ec19d
// Draws the cloud's model at its position with each orbital turned to its angle, fading out over the last tenth
// of its life; returns the draw result
long EEE3DDisease::DrawPass5()
{
    // LOST-DEBUG: none independent: 14 units make mAngles[i]/mOrbitals[i] call operator+ out of line
    EE_LOST_DEBUG(14);
    GETransformation theTransformation;
    GETransformation theOrientation;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    Fn10FA01();
    const GE3DPoint& theLocation = this->mLocation;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsCurrentFrame(this->mDrawFrame);
    theModel->SetGraphicsScale(theScale);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        theOrientation.SetOrientationYPR(this->mAngles[i], 0.0f, 0.0f);
        this->mOrbitals[i]->mModel->SetGraphicsPreModelTransform(theOrientation);
    }
    theModel->PrepareTransforms();
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    long theResult;
    if (this->mDrawAge >= 0.9f) {
        theModel->SetOpacity((1.0f - this->mDrawAge) * 10.0f);
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
void EEE3DDisease::PrepareQueue()
{
}

// FUNCTION: EXE 0x4ec3aa
// Reloads the graphic and its orbitals and reads time, frame and age; returns the first read error
long EEE3DDisease::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    long theResult = inFile.Read(this->mTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mFrame);
    // Stop at the first read error instead of letting the age read replace it.
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAge);
    return theResult;
}

// FUNCTION: EXE 0x4ec487
// Writes the base 3D effect (result ignored), then time, frame and age; returns the first write error
long EEE3DDisease::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mFrame);
    if (theResult == 0)
        theResult = outFile.Write(this->mAge);
    return theResult;
}

// FUNCTION: EXE 0x4ec4d7
// Ages the cloud over its duration on its living unit, advances animation 13 and the orbitals and updates its
// positional sound; returns 7 (remove) when expired or the unit is dead
long EEE3DDisease::Step()
{
    GE3DPoint theLocation;
    long theResult = 0;
    unsigned long theOldFrame = this->mFrame;
    unsigned long theFrame = theOldFrame;
    float theTime = this->mTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[13];
    unsigned long theFrameCount = this->mGraphicObject->mFrameCounts[13];
    bool theStart;
    if (theFrame == this->mGraphicObject->mStartFrames[0] && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    if (this->mUnit->mHitPoints > 0) {
        this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
        if (this->mAge <= 1.0f) {
            this->SetTransformation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
            theTime = gPhysics->GetTickTime() + theTime;
            if (theTime >= (float)theFrameTime) {
                theFrame = UMFloatToLong(theTime / (float)theFrameTime);
                theTime -= (float)(theFrame * theFrameTime);
                theFrame = (theFrame + theOldFrame) % theFrameCount;
            }
            this->mTime = theTime;
            this->mFrame = theFrame;
            for (long i = 0; i < this->mOrbitalCount; i++)
                this->mOrbitals[i]->Update();
            bool theChanged;
            if (theStart || theFrame != theOldFrame)
                theChanged = true;
            else
                theChanged = false;
            this->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
            this->SetSound(Fn1487A3(theLocation, this->mGraphicObject, 0, this->mSound, theFrame, theChanged));
            if (this->mSound)
                this->mSound->Dereference();
        } else
            theResult = 7;
    } else
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4ec6bd
// Replaces the cloud's sound, releasing the old one and referencing the new one
void EEE3DDisease::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
