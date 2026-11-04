// C:\EEX\Empire Earth\Graphics Thread\EEE3DHurricane.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DHurricane-Data"). 0xf1600..0xf2046: the key chain, both ctors .. Virtual4C (StopSound and SetSound fold into
// EEE3DVirus's). Unit flags: the exe's module set.
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
#include "ee/EEE3DHurricane.h"
#include "LostDebug.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4f1600
// SYNTHETIC: EXE 0x4f160a
// SYNTHETIC: EXE 0x4f161b
// SYNTHETIC: EXE 0x4f1627
const UString EEE3DHurricane::sData("EEE3DHurricane-Data");

// FUNCTION: EXE 0x4f1643
// Creates a hurricane on a unit, with no graphic or sound yet
EEE3DHurricane::EEE3DHurricane(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(inUnit, inGraphicEffect, kEffect3DHurricane), mGraphicObject(NULL), mSound(NULL)
{
}

// SYNTHETIC: EXE 0x4f16bf
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f16db
// Creates an empty hurricane for loading from a saved game
EEE3DHurricane::EEE3DHurricane()
    : EEEffect3D(kEffect3DHurricane), mSound(NULL)
{
}

// FUNCTION: EXE 0x4f1734
// Destroys the hurricane: stops its sound, unloads its graphic and deletes its orbitals
EEE3DHurricane::~EEE3DHurricane()
{
    this->StopSound();
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    for (long i = 0; i < this->mOrbitalCount; i++)
        delete this->mOrbitals[i];
}

// StopSound (stops and releases a playing sound): ICF-folded into EEE3DVirus's 0xfdd43
void EEE3DHurricane::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4f1798
// Drops the hurricane's reference to its sound
void EEE3DHurricane::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

long EEE3DHurricane::DrawPass2()
{
    return 0;
}

long EEE3DHurricane::DrawPass4()
{
    return 0;
}

long EEE3DHurricane::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4f17c2
// Queues the hurricane in render queue 4, snapshotting its frame, scale, position and orbital angles
// NEAR: the original calls mOrbitals[i]'s _Ptrit::operator+ out of line (inline budget, as EEE3DDisease)
long EEE3DHurricane::Queue(EEEffectQueue* inQueues)
{
    long i;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawFrame = this->mFrame;
        this->mDrawScale = this->mScale;
        GE3DPoint& theLocation = this->mLocation;
        theLocation.SetX(this->mTransformation.GetX());
        theLocation.SetY(this->mTransformation.GetY());
        theLocation.SetZ(this->mTransformation.GetZ());
        for (i = 0; i < this->mOrbitalCount; i++)
            this->mAngles[i] = this->mOrbitals[i]->mAngle;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f188a
// Picks a random type-6 tag point of the effect's graphic around the unit; leaves the point unchanged if there is none
// NEAR: registers only (the point's operator[])
void EEE3DHurricane::GetSpawnPoint(GE3DPoint& outPoint)
{
    GETransformation theLocation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    EETagPointList* theList = this->mGraphicEffect->mGraphicObject->GetTagPoints(0, 6);
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theFrame = this->mFrame;
    if (theList == NULL || theList->mPoints.empty())
        return;
    unsigned long theIndex = UMFloatToLong(EEEffect::GetRandom() * theList->mPoints.size());
    if (theIndex == theList->mPoints.size())
        theIndex = theList->mPoints.size() - 1;
    theList->Scale(theLocation, theScale);
    theList->mPoints[theIndex]->GetPoint(theFrame, theLocation, outPoint);
}

// FUNCTION: EXE 0x4f1953
// Loads the graphic, starts animation 13 at a tenth of full size, copies the orbitals and moves onto the unit
long EEE3DHurricane::SetUp()
{
    // LOST-DEBUG: none independent: as EEE3DDisease::SetUp, the orbital copy calls begin() and
    // operator+ out of line
    EE_LOST_DEBUG(12);
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[13];
    this->mTime = 0.0f;
    this->mSpawnTime = 0.0f;
    this->mDrawFrame = 0;
    this->mScale = 0.1f;
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    this->SetTransformation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    return theResult;
}

// FUNCTION: EXE 0x4f1a61
// Draws the hurricane's model with its orbitals turned and clockwise culling, then restores the culling
long EEE3DHurricane::DrawPass6()
{
    GETransformation theTransformation;
    GETransformation theOrientation;
    GEModel* theModel = this->mGraphicObject->mModel;
    float theScale = this->mGraphicEffect->mSize;
    unsigned long theScreen = gScreenIndex;
    unsigned long theCulling = gRasterizer->GetCulling();
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetCulling(kCullCW);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    Fn10FA01();
    const GE3DPoint& theLocation = this->mLocation;
    theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsCurrentFrame(this->mDrawFrame);
    theModel->SetGraphicsScale(theScale * this->mDrawScale);
    for (long i = 0; i < this->mOrbitalCount; i++) {
        theOrientation.SetOrientationYPR(this->mAngles[i], 0.0f, 0.0f);
        this->mOrbitals[i]->mModel->SetGraphicsPreModelTransform(theOrientation);
    }
    theModel->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetCulling(theCulling);
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DHurricane::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f1c17
// Reloads the graphic and its orbitals and reads time, frame, spawn timer, age and scale; returns the first error
long EEE3DHurricane::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
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
    theResult = inFile.Read(this->mSpawnTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mAge);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mScale);
    return theResult;
}

// FUNCTION: EXE 0x4f1d1c
// Writes the base 3D effect (result ignored), time, frame, spawn timer, age and scale; returns the first error
long EEE3DHurricane::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mFrame);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mSpawnTime);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult == 0)
        theResult = outFile.Write(this->mScale);
    return theResult;
}

// FUNCTION: EXE 0x4f1d90
// Grows the hurricane on its unit, spawns the linked effect at random tag points, animates it and its sound;
// shrinks it once the unit has no hit points and returns 7 (remove) at a tenth of full size
// NEAR: frame slots (one more 4-byte local in the original) and the spawn's effect loads
long EEE3DHurricane::Step()
{
    GE3DPoint theLocation;
    long theResult = 0;
    GE3DPoint theSpawnPoint;
    unsigned long theOldFrame = this->mFrame;
    unsigned long theFrame = theOldFrame;
    float theTime = this->mTime;
    float theElapsed = gPhysics->mTickTime;
    unsigned long theFrameTime = this->mGraphicObject->mFrameTimes[13];
    unsigned long theFrameCount = this->mGraphicObject->mFrameCounts[13];
    bool theStart;
    // The effect plays animation 13: its first frame is that animation's start frame.
    if (theFrame == this->mGraphicObject->mStartFrames[13] && theTime == 0.0f)
        theStart = true;
    else
        theStart = false;
    if (this->mUnit->mHitPoints == 0) {
        if (this->mScale <= 0.1f)
            theResult = 7;
        else
            this->mScale = max(0.1f, this->mScale - theElapsed / 2000.0f);
    } else {
        this->mAge += theElapsed / this->mGraphicEffect->mDuration;
        if (this->mAge < this->mGraphicEffect->mDuration * 0.3f)
            this->mScale = min(1.0f, theElapsed / 2000.0f + this->mScale);
        else
            this->mScale = 1.0f;
        this->SetTransformation(this->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
        theTime = theElapsed + theTime;
        this->mSpawnTime -= theElapsed;
        if (this->mSpawnTime <= 0.0f && this->mGraphicEffect->mLinkedEffect1) {
            this->GetSpawnPoint(theSpawnPoint);
            EEGraphicEffect* theSpawn = this->mGraphicEffect->mLinkedEffect1;
            theResult = gEffectManager->CreatePlacedEffect(theSpawnPoint, theSpawn, NULL);
            this->mSpawnTime = EEEffect::GetRandomSpread(this->mGraphicEffect->mIntParam2, this->mGraphicEffect->mIntParam1);
        }
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
    }
    return theResult;
}

// SetSound (replaces and references the sound): ICF-folded into EEE3DVirus's 0xfe52c
void EEE3DHurricane::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}
