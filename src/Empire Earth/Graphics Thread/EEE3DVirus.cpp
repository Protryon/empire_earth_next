// C:\EEX\Empire Earth\Graphics Thread\EEE3DVirus.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DVirus-Data"). 0xfdbab..0xfe553: the key chain, both ctors .. SetSound. Unit flags: the exe's module set.
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
#include "ee/EEE3DVirus.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4fdbab
// SYNTHETIC: EXE 0x4fdbb5
// SYNTHETIC: EXE 0x4fdbc6
// SYNTHETIC: EXE 0x4fdbd2
const UString EEE3DVirus::sData("EEE3DVirus-Data");

// FUNCTION: EXE 0x4fdbee
// Creates a virus effect on a unit, with no sound or graphic object yet
EEE3DVirus::EEE3DVirus(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DVirus), mSound(NULL)
{
    this->mGraphicObject = NULL;
}

// SYNTHETIC: EXE 0x4fdc6a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fdc86
// Creates an empty virus effect (for loading), with no sound
EEE3DVirus::EEE3DVirus()
    : EEEffect3D(kEffect3DVirus), mSound(NULL)
{
}

// FUNCTION: EXE 0x4fdcdf
// Destroys the virus effect, stopping its sound, unloading its media and deleting its orbitals
EEE3DVirus::~EEE3DVirus()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    for (long i = 0; i < this->mOrbitalCount; i++)
        delete this->mOrbitals[i];
}

// FUNCTION: EXE 0x4fdd43
// Stops the virus sound, if any, and releases it
void EEE3DVirus::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// ICF-folded into EEE3DHurricane's 0xf1798
void EEE3DVirus::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE3DVirus::DrawPass2()
{
    return 0;
}

long EEE3DVirus::DrawPass4()
{
    return 0;
}

long EEE3DVirus::DrawPass6()
{
    return 0;
}

// FUNCTION: EXE 0x4fdd68
// Snapshots the effect's frame, age, scroll, location and orbital angles for drawing and queues it in render queue 3
// NEAR: the original calls mOrbitals[i]'s _Ptrit::operator+ out of line (inline budget, as EEE3DDisease)
long EEE3DVirus::Queue(EEEffectQueue* inQueues)
{
    long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawAge = this->mAge;
        this->mDrawScroll = this->mScroll;
        GE3DPoint& theLocation = this->mLocation;
        theLocation.SetX(this->mTransformation.GetX());
        theLocation.SetY(this->mTransformation.GetY());
        theLocation.SetZ(this->mTransformation.GetZ());
        for (i = 0; i < this->mOrbitalCount; i++)
            this->mAngles[i] = this->mOrbitals[i]->mAngle;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[3][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4fde3c
// Loads the media, starts at the first frame of animation 13, copies the graphic object's orbitals and moves the effect
// onto the unit; returns the media load result
long EEE3DVirus::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[13];
    this->mTime = 0.0f;
    this->mAge = 0.0f;
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    this->FollowUnit();
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4fdf0e
// Draws the animated virus model with its texture scrolled and its orbitals turned, fading out over the last tenth of
// its life; restores the texture coordinates
// NEAR: the original calls the vectors' _Ptrit::operator+ out of line (inline budget: frame temporaries)
long EEE3DVirus::DrawPass5()
{
    std::vector<float> theCoordinates;
    GETransformation theTransformation;
    GETransformation theOrientation;
    GETextureCoordinate theCoordinate;
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    Fn10FA01();
    const GE3DPoint& theLocation = this->mLocation;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsCurrentFrame(this->mDrawFrame);
    theModel->SetGraphicsScale(theScale);
    for (unsigned long j = 0; j < theModel->GetVertexArray(0)->GetSize(); j++) {
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, j);
        float theU = theCoordinate.GetU();
        theCoordinates.push_back(theU);
        theU += this->mDrawScroll;
        theCoordinate.SetU(theU);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, j);
    }
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
    for (unsigned long k = 0; k < theModel->GetVertexArray(0)->GetSize(); k++) {
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, k);
        theCoordinate.SetU(theCoordinates[k]);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, k);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DVirus::PrepareQueue()
{
}

// FUNCTION: EXE 0x4fe1d3
// Reloads the media and orbitals, then reads the animation time, frame, age and scroll; returns the first read error
long EEE3DVirus::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    long theResult = inFile.Read(this->mTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mFrame);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mAge);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mScroll);
    return theResult;
}

// FUNCTION: EXE 0x4fe2c6
// Writes the animation time, frame, age and scroll; returns the first write error
long EEE3DVirus::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFrame);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mScroll);
    return theResult;
}

// FUNCTION: EXE 0x4fe328
// Ages the effect, scrolls its texture, follows the unit, advances animation 13 and the orbitals and updates the frame
// sound (positioned at the unit); returns 7 when the unit dies or the effect's time is up
long EEE3DVirus::Step()
{
    GE3DPoint theLocation;
    long theResult = 0;
    unsigned long theOldFrame = this->mFrame;
    float theTime = this->mTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[13];
    unsigned long theFrameCount = this->mGraphicObject->mFrameCounts[13];
    unsigned long theFrame = theOldFrame;
    bool theStart;
    // The effect plays animation 13: its first frame is that animation's start frame.
    if (theFrame == this->mGraphicObject->mStartFrames[13] && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    if (this->mUnit->mHitPoints > 0) {
        this->mAge += gPhysics->GetTickTime() / this->mGraphicEffect->mDuration;
        if (this->mAge <= 1.0f) {
            this->mScroll += this->mGraphicEffect->GetParam1() * gPhysics->GetTickTime();
            if (this->mScroll > 1.0f)
                this->mScroll -= 1.0f;
            if (this->mScroll < 0.0f)
                this->mScroll += 1.0f;
            this->FollowUnit();
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
            // Place the sound at the unit, not at the map origin.
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

// FUNCTION: EXE 0x4fe52c
// Replaces the virus sound, referencing the new one and releasing the old
void EEE3DVirus::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// ICF-folded into EEE3DBlastWave's 0xe904a
void EEE3DVirus::FollowUnit()
{
    GE3DPoint thePoint;
    GETransformation theLocation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    float theScale = this->mUnit->mComplexUnit->mPhysicalModel.GetScale();
    unsigned long theFrame = this->mUnit->mComplexUnit->mPhysicalModel.GetFrame();
    EETagPointList* theList = this->mUnit->mComplexUnit->GetModel()->GetTagPoints(this->mUnit->mComplexUnit->GetAnimation(), 0x12);
    if (theList != NULL && !theList->mPoints.empty()) {
        theList->Scale(theLocation, theScale);
        theList->mPoints.front()->GetPoint(theFrame, theLocation, thePoint);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
        this->SetTransformation(theTransformation);
    } else {
        this->SetTransformation(theLocation);
    }
}
