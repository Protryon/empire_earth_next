// C:\EEX\Empire Earth\Graphics Thread\EEE3DIcon.cpp (no __FILE__, no key chain; name from the RTTI class).
// 0xf2046..0xf2d93: both ctors .. UpdatePosition, the vector<float>::push_back COMDAT. Unit flags: the exe's module
// set.
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
#include "ee/EEE3DIcon.h"

// FUNCTION: EXE 0x4f2046
// Creates a 3D icon that follows a unit, or stays at a location when one is given
EEE3DIcon::EEE3DIcon(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, const GE3DPoint* inLocation, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    if (inLocation) {
        this->mFollowUnit = false;
        this->mLocation = *inLocation;
    } else
        this->mFollowUnit = true;
    this->mGrowing = false;
    this->mScroll = 0.0f;
    this->mGraphicObject = NULL;
}

// SYNTHETIC: EXE 0x4f20f2
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f210e
// Creates an empty icon of a kind for loading from a saved game
EEE3DIcon::EEE3DIcon(long inKind)
    : EEEffect3D(inKind)
{
    this->Clear();
    this->mScroll = 0.0f;
}

// FUNCTION: EXE 0x4f2177
// Destroys the icon, unloading its graphic and deleting its orbitals
EEE3DIcon::~EEE3DIcon()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    for (long i = 0; i < this->mOrbitalCount; i++)
        delete this->mOrbitals[i];
}

long EEE3DIcon::DrawPass2()
{
    return 0;
}

long EEE3DIcon::DrawPass4()
{
    return 0;
}

long EEE3DIcon::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4f21e5
// Queues the icon in render queue 4 with its transform, orbital angles, frame, opacity (faded in over the
// effect's tenth parameter) and scroll; returns 10 for an unbuilt market
// NEAR: the original calls the vectors' _Ptrit::operator+ out of line (inline budget) and keeps the opacity on the
// FP stack
long EEE3DIcon::Queue(EEEffectQueue* inQueues)
{
    long i;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    long theResult = 0;
    if (!theEffect->mFlag2)
        theResult = EEEffect3D::Queue(inQueues);
    if (theResult != 0)
        return theResult;
    EEComplexUnit* theUnit;
    if (this->mUnit && (theUnit = dynamic_cast<EEComplexUnit*>(this->mUnit)) != 0 && theUnit->mUnitClass->mUnitType == kUnitTypeMarketCP
        && !theUnit->mIsBuilt && !theUnit->IsFoundation())
        theResult = 10;
    else {
        this->mDrawTransformation = this->mTransformation;
        for (i = 0; i < this->mOrbitalCount; i++)
            this->mAngles[i] = this->mOrbitals[i]->mAngle;
        this->mDrawFrame = this->mFrame;
        float theOpacity = theEffect->GetParam8();
        this->mOpacity = theOpacity;
        if (theEffect->mParam10 > 0.0f && theEffect->mParam10 > this->mAge)
            this->mOpacity = (theOpacity - theEffect->GetParam9()) * (this->mAge / theEffect->mParam10) + theEffect->GetParam9();
        this->mDrawScroll = this->mScroll;
        if (this->mFrameCount > 0 && this->mFrameTime != 0)
            this->mFraction = this->mTime / this->mFrameTime;
        else
            this->mFraction = 0.0f;
        GETexture* theTexture;
        theTexture = theEffect->mGraphicObject ? theEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DIcon::PrepareQueue()
{
}

// FUNCTION: EXE 0x4f238a
// Resets the icon's age, scale, animation and orbitals (deleting its orbitals)
void EEE3DIcon::Clear()
{
    this->mAge = 0.0f;
    this->mScale = 0.0f;
    this->mGraphicObject = NULL;
    this->mGrowing = false;
    // The icon owns its orbital copies: delete them before emptying the list.
    for (std::vector<EEOrbital*>::iterator theOrbital = this->mOrbitals.begin(); theOrbital != this->mOrbitals.end(); theOrbital++)
        delete *theOrbital;
    this->mOrbitals.clear();
    this->mTime = 0.0f;
    this->mOrbitalCount = 0;
    this->mFrame = 0;
    this->mFrameCount = 0;
    this->mFraction = 0.0f;
    this->mFrameTime = 0;
}

// FUNCTION: EXE 0x4f23e2
// Clears the icon, loads its graphic, starts animation 13, copies the orbitals, places it and sets its scale; returns 0
long EEE3DIcon::SetUp()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    float theRadius = theEffect->mSize;
    EEGraphicObject* thePrevious = this->mGraphicObject;
    this->Clear();
    this->mGraphicObject = theEffect->mGraphicObject;
    this->mGraphicObject->LoadMedia();
    // Release the media of a graphic loaded by an earlier set-up (after the new load, so shared media stays loaded).
    if (thePrevious)
        thePrevious->UnloadMedia();
    this->mFrame = this->mGraphicObject->mStartFrames[13];
    this->mFrameCount = this->mGraphicObject->mFrameCounts[13];
    this->mFrameTime = this->mGraphicObject->mFrameTimes[13];
    this->mTime = 0.0f;
    this->mOrbitalCount = this->mGraphicObject->GetOrbitalCount();
    this->mAngles.resize(this->mOrbitalCount, 0.0f);
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals.push_back(new EEOrbital(*this->mGraphicObject->GetOrbital(i)));
    this->SetBoundingRadius(theRadius);
    this->UpdatePosition();
    this->mScale = theEffect->GetSize();
    return 0;
}

// FUNCTION: EXE 0x4f24e3
// Reads the base 3D effect, the age, the follow-unit flag and, for a fixed icon, its location; returns the first error
long EEE3DIcon::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        theResult = inFile.Read(this->mAge);
        if (theResult == 0) {
            theResult = inFile.Read(this->mFollowUnit);
            if (theResult == 0 && !this->mFollowUnit)
                theResult = this->mLocation.Load(inFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4f2536
// After a load, relinks the pointers, reloads the graphic and scale, copies the orbitals and restarts animation 13
long EEE3DIcon::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0) {
        this->mGraphicObject = this->GetGraphicObject();
        theResult = this->mGraphicObject->LoadMedia();
        if (this->mGraphicEffect)
            this->mScale = this->mGraphicEffect->GetSize();
        this->mOrbitalCount = this->GetGraphicObject()->GetOrbitalCount();
        this->mAngles.resize(this->mOrbitalCount, 0.0f);
        for (long i = 0; i < this->mOrbitalCount; i++)
            this->mOrbitals.push_back(new EEOrbital(*this->GetGraphicObject()->GetOrbital(i)));
        this->mFrame = this->mGraphicObject->mStartFrames[13];
        this->mFrameCount = this->mGraphicObject->mFrameCounts[13];
        this->mFrameTime = this->mGraphicObject->mFrameTimes[13];
        this->mTime = 0.0f;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f2637
// Moves a fixed icon to a location (icons that follow a unit ignore it)
void EEE3DIcon::SetLocation(const GE3DPoint& inLocation)
{
    if (!this->mFollowUnit)
        this->mLocation = inLocation;
}

// FUNCTION: EXE 0x4f2658
// Draws the icon's model interpolated between frames: alpha blended with z write (flag 1), else additive, lit and
// facing the camera; scrolls the texture for the draw when the effect asks
// NEAR: the original calls the vectors' _Ptrit::operator+ out of line (inline budget: two more frame temporaries)
// and keeps the effect in ebx, the screen in memory
long EEE3DIcon::DrawPass6()
{
    GE3DPoint theFogPoint;
    GETransformation theTransformation(this->mDrawTransformation);
    GETransformation theOrientation;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    GEModel* theModel = this->GetGraphicObject()->mModels[theEffect->mIntParam1];
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    bool theAdditive = theEffect->mFlag1;
    std::vector<float> theCoordinates;
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    if (theEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(theEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    if (theAdditive) {
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        Fn10F71C();
        gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    } else {
        gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
        Fn10F7C8();
        theFogPoint.SetX(this->mDrawTransformation.GetX());
        theFogPoint.SetY(this->mDrawTransformation.GetY());
        theFogPoint.SetZ(this->mDrawTransformation.GetZ());
        float theValue;
        Fn11A130(theFogPoint, theFog, theValue);
        gRasterizer->SetAmbientColor(GEColor(theValue, theValue, theValue), theScreen);
        theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, 0.0f);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    }
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theModel->SetGraphicsCurrentFrame(this->mDrawFrame);
    theModel->SetGraphicsNextFrame((this->mDrawFrame + 1) % this->mFrameCount);
    theModel->SetFractionalFramePosition(this->mFraction);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(this->mScale);
    theModel->SetOpacity(this->mOpacity);
    theModel->PrepareTransforms();
    long i;
    for (i = 0; i < this->mOrbitalCount; i++) {
        theOrientation.SetOrientationYPR(this->mAngles[i], 0.0f, 0.0f);
        this->mOrbitals[i]->mModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
        this->mOrbitals[i]->mModel->SetGraphicsPreModelTransform(theOrientation);
    }
    if (theEffect->mIntParam2 > 0) {
        for (unsigned long j = 0; j < theModel->GetVertexArray(0)->GetSize(); j++) {
            GETextureCoordinate theCoordinate;
            theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, j);
            float theU = theCoordinate.GetU();
            theCoordinates.push_back(theU);
            theU += this->mDrawScroll;
            theCoordinate.SetU(theU);
            theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, j);
        }
    }
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    gRasterizer->SetFog(theFog, theScreen);
    if (!(theEffect->mIntParam2 > 0))
        return theResult;
    for (unsigned long j = 0; j < theModel->GetVertexArray(0)->GetSize(); j++) {
        GETextureCoordinate theCoordinate;
        theModel->GetVertexArray(0)->GetTextureCoordinate(theCoordinate, j);
        theCoordinate.SetU(theCoordinates[j]);
        theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, j);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f2a57
// Writes the base 3D effect, age, follow flag and fixed location
long EEE3DIcon::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0) {
        theResult = outFile.Write(this->mAge);
        if (theResult == 0) {
            theResult = outFile.Write(this->mFollowUnit);
            if (theResult == 0 && !this->mFollowUnit)
                theResult = this->mLocation.Save(outFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4f2aaa
// Ages the icon (7 once a set duration is up), pulses its scale between the start and full size, scrolls and
// animates it, updates the orbitals and its position
// NEAR: the original keeps the rate in memory and reloads gPhysics; registers
long EEE3DIcon::Step()
{
    long theResult = 0;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    float theTime = this->mTime;
    float theRate = theEffect->mParam7;
    unsigned long theFrame = this->mFrame;
    this->mAge += gPhysics->GetTickTime();
    if (this->mAge >= theEffect->mDuration && theEffect->mDuration != 0)
        theResult = 7;
    if (this->mGrowing) {
        if (theEffect->mSize > this->mScale)
            this->mScale += theRate;
        if (theEffect->mSize <= this->mScale) {
            this->mGrowing = false;
            this->mScale = theEffect->GetSize();
        }
    } else {
        if (theEffect->mStartSize < this->mScale)
            this->mScale -= theRate;
        if (theEffect->mStartSize >= this->mScale) {
            this->mGrowing = true;
            this->mScale = theEffect->mStartSize;
        }
    }
    theTime += gPhysics->mSpeed * 1000.0f;
    if (theEffect->mIntParam2 > 0) {
        float theScroll = this->mScroll += theEffect->mParam11 * gPhysics->GetTickTime() / 1000.0f;
        if (theScroll > 1.0f)
            this->mScroll = theScroll - 1.0f;
        if (this->mScroll < 0.0f)
            this->mScroll += 1.0f;
    }
    if (theTime >= (float)this->mFrameTime) {
        long theCount = UMFloatToLong(theTime / (float)this->mFrameTime);
        theTime -= (float)(theCount * this->mFrameTime);
        theFrame = (theCount + this->mFrame) % this->mFrameCount;
    }
    this->mTime = theTime;
    this->mFrame = theFrame;
    for (long i = 0; i < this->mOrbitalCount; i++)
        this->mOrbitals[i]->Update();
    this->SetBoundingRadius(this->mScale);
    this->UpdatePosition();
    return theResult;
}

// FUNCTION: EXE 0x4f2c95
// Places the icon at its unit (raised by the unit's effect height while alive) or its location, plus the offset
void EEE3DIcon::UpdatePosition()
{
    GETransformation theTransformation;
    GE3DPoint theLocation;
    GE3DPoint theOffset;
    theOffset.SetX(this->mGraphicEffect->mParam1);
    theOffset.SetY(this->mGraphicEffect->mParam2);
    theOffset.SetZ(this->mGraphicEffect->mParam3);
    if (this->mFollowUnit) {
        EEUnit* theUnit = this->mUnit;
        if (theUnit) {
            theUnit->GetPosition(theLocation);
            if (theUnit->mTileNode && theUnit->mHitPoints > 0)
                theLocation.SetZ(theLocation.GetZ() + theUnit->GetComplexUnit()->GetEffectHeight(false));
        }
    } else
        theLocation = this->mLocation;
    theLocation += theOffset;
    theTransformation.SetPosition(theLocation);
    this->SetTransformation(theTransformation);
}

// FUNCTION: EXE 0x4f2d67
// Appends a float to a std::vector<float> (a COMDAT from <vector>, first used by DrawPass6)
