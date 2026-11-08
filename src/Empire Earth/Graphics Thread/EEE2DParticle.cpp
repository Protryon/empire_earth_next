// C:\EEX\Empire Earth\Graphics Thread\EEE2DParticle.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DParticle-Data"). 0xe4729..0xe56d0: the key chain, the pool's chain, the ctor .. Step, then
// UMemoryPool<EEE2DParticle>'s COMDATs (and the particle's ??_E); EEE2DPlasmaPulse.cpp follows.
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
#include <typeinfo>
#include "lle/GE3DLine.h"
#include "lle/UMemoryPool.h"
#include "ee/EEE2DParticle.h"

// SYNTHETIC: EXE 0x4e4729
// SYNTHETIC: EXE 0x4e4733
// SYNTHETIC: EXE 0x4e4744
// SYNTHETIC: EXE 0x4e4750
const UString EEE2DParticle::sData("EEE2DParticle-Data");

// SYNTHETIC: EXE 0x4e476c
// SYNTHETIC: EXE 0x4e4776
// SYNTHETIC: EXE 0x4e4780
// SYNTHETIC: EXE 0x4e478c
UMemoryPool<EEE2DParticle> EEE2DParticle::sPool;

// FUNCTION: EXE 0x4e47b1
// Creates a cleared pooled particle of the given kind with no references
EEE2DParticle::EEE2DParticle(long inKind)
    : EEEffect2D(0, inKind, false)
{
    this->Reinit(NULL, false);
    this->Clear();
    this->mReferenceCount = 0;
}

// FUNCTION: EXE 0x4e4800
// Destroys the particle, unloading its graphic's media
EEE2DParticle::~EEE2DParticle()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

// FUNCTION: EXE 0x4e481e
// Sizes the shared particle pool (particles per chunk, maximum chunks); returns whether it succeeded
bool EEE2DParticle::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return EEE2DParticle::sPool.SetSize(inChunkSize, inMaxChunks, 1);
}

long EEE2DParticle::DrawPass2()
{
    return 0;
}

long EEE2DParticle::DrawPass4()
{
    return 0;
}

long EEE2DParticle::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e4833
// Moves the particle (Update) and queues it in render queue 4 under its texture; returns 7 (remove) once it dies
long EEE2DParticle::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        theResult = this->Update();
        if (theResult == 0) {
            GETexture* theTexture;
            theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
            inQueues[4][theTexture].push_back(this);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e489b
// Takes a particle from the pool for a unit's effect, placed at the unit (or its tile centre on the ground)
EEE2DParticle* EEE2DParticle::Create(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
{
    GE3DPoint theLocation;
    EEE2DParticle* theParticle;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    theParticle = EEE2DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(inUnit, inGraphicEffect, gEffect2DModel, false);
    theParticle->Clear();
    if (theComplexUnit) {
        const GETransformation& theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
        theLocation.SetX(theTransformation.GetX());
        theLocation.SetY(theTransformation.GetY());
        theLocation.SetZ(theTransformation.GetZ());
    } else {
        theLocation.SetX(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileX);
        theLocation.SetY(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileY);
        if (gWorld.GetMap()->mTerrainMesh.GetIsValidTile(inUnit->mTileX, inUnit->mTileY))
            theLocation.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        else
            theLocation.SetZ(0.0f);
    }
    theParticle->SetPosition(theLocation);
    theParticle->mDrawPosition = theLocation;
    theParticle->mLastTime = gPhysics->GetGameClock();
    theParticle->mDuration = inGraphicEffect->mDuration;
    return theParticle;
}

// FUNCTION: EXE 0x4e49c7
// Takes a particle from the pool for an effect at a point
EEE2DParticle* EEE2DParticle::Create(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
{
    EEE2DParticle* theParticle = EEE2DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(NULL, inGraphicEffect, gEffect2DModel, false);
    theParticle->Clear();
    theParticle->SetPosition(inPosition);
    theParticle->mDrawPosition = inPosition;
    theParticle->mLastTime = gPhysics->GetGameClock();
    theParticle->mDuration = inGraphicEffect->mDuration;
    return theParticle;
}

// FUNCTION: EXE 0x4e4a4a
// Takes a particle from the pool for an effect, starting at the line's start with its direction as velocity
EEE2DParticle* EEE2DParticle::Create(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect)
{
    EEE2DParticle* theParticle = EEE2DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(NULL, inGraphicEffect, gEffect2DModel, false);
    theParticle->Clear();
    theParticle->SetPosition(inLine.GetStart());
    theParticle->mDrawPosition = inLine.GetStart();
    theParticle->mVelocity = inLine.GetDirection();
    theParticle->mLastTime = gPhysics->GetGameClock();
    theParticle->mDuration = inGraphicEffect->mDuration;
    return theParticle;
}

// FUNCTION: EXE 0x4e4adf
// Takes an empty particle from the pool for loading from a saved game
EEE2DParticle* EEE2DParticle::Create()
{
    EEE2DParticle* theParticle = EEE2DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(gEffect2DModel, false);
    theParticle->Clear();
    return theParticle;
}

// FUNCTION: EXE 0x4e4b13
// Logs the particle pool's statistics and frees the pool
void EEE2DParticle::ReleasePool()
{
    EEE2DParticle::sPool.DumpStats(false);
    EEE2DParticle::sPool.Release();
}

// FUNCTION: EXE 0x4e4b2b
// Unloads the particle's graphic and releases the base effect's resources
void EEE2DParticle::ReleaseResources()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    EEEffect2D::ReleaseResources();
}

void EEE2DParticle::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e4b47
// Starts the particle at age 0 with a random rotation (mode 0) and a random brightness in parameter 2 +- 3,
// clamped to 0..1, and loads its graphic; returns the load result
long EEE2DParticle::SetUp()
{
    long theMode = this->mGraphicEffect->mIntParam1;
    this->mAge = 0.0f;
    this->mRotation = theMode == 0 ? EEEffect::GetRandom() * 6.2831855f : 0.0f;
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    float theMinimum = this->mGraphicEffect->GetParam2() - this->mGraphicEffect->GetParam3();
    float theMaximum = this->mGraphicEffect->GetParam2() + this->mGraphicEffect->GetParam3();
    this->mBrightness = EEEffect::GetRandom() * (theMaximum - theMinimum) + theMinimum;
    this->mBrightness = max(this->mBrightness, 0.0f);
    this->mBrightness = min(this->mBrightness, 1.0f);
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e4be3
// Resets the particle's motion, age, look and timing to zero, with no graphic
void EEE2DParticle::Clear()
{
    this->mVelocity.SetX(0.0f);
    this->mVelocity.SetY(0.0f);
    this->mVelocity.SetZ(0.0f);
    this->mDrawPosition.SetX(0.0f);
    this->mDrawPosition.SetY(0.0f);
    this->mDrawPosition.SetZ(0.0f);
    this->mGraphicObject = NULL;
    this->mDrawAge = 0.0f;
    this->mAge = 0.0f;
    this->mRotation = 0.0f;
    this->mBrightness = 0.0f;
    this->mLastTime = 0.0f;
    this->mDuration = 0.0f;
    this->mMoved = false;
}

// NEAR: x87 order of the drag/wind terms and two frame slots
// FUNCTION: EXE 0x4e4c31
// Moves the particle by the game time passed: gravity, drag, wind and lift, bouncing off the ground when the
// effect asks; returns 7 (remove) once it is past its duration or off the map
long EEE2DParticle::Update()
{
    long theResult = 0;
    float theNow = gPhysics->mGameClock;
    float theElapsed = theNow - this->mLastTime;
    if ((theElapsed < -0.0001f || theElapsed > 0.0001f) && !gPhysics->mGameOver) {
        this->mLastTime = theNow;
        this->mAge += theElapsed;
        if (this->mAge <= this->mGraphicEffect->mDuration) {
            float theFall = -this->mGraphicEffect->mParam8;
            GEVector theWind = gWorld.mWindDirection;
            GE3DPoint thePosition(this->mPosition);
            GEVector theNormal;
            float theStep = theElapsed / 1000.0f;
            float theDrag = -this->mGraphicEffect->mParam11;
            GEVector theForce(this->mVelocity.GetX() * theDrag, this->mVelocity.GetY() * theDrag, this->mVelocity.GetZ() * theDrag);
            theFall += theForce.GetZ();
            this->mVelocity.SetX(theForce.GetX() * theStep + this->mVelocity.GetX());
            this->mVelocity.SetY(theForce.GetY() * theStep + this->mVelocity.GetY());
            this->mVelocity.SetZ(theFall * theStep + this->mVelocity.GetZ());
            thePosition.SetX(this->mGraphicEffect->GetParam9() * theWind.GetX() * theStep + this->mVelocity.GetX() * theStep
                             + thePosition.GetX());
            thePosition.SetY(this->mGraphicEffect->GetParam9() * theWind.GetY() * theStep + this->mVelocity.GetY() * theStep
                             + thePosition.GetY());
            thePosition.SetZ(this->mGraphicEffect->GetParam9() * theWind.GetZ() * theStep + this->mGraphicEffect->mParam4 * theStep
                             + this->mVelocity.GetZ() * theStep + thePosition.GetZ());
            if (this->mGraphicEffect->mFlag2) {
                GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
                unsigned long theX = UMFloorUnsigned(thePosition.GetX());
                unsigned long theY = UMFloorUnsigned(thePosition.GetY());
                if (!theMesh.GetIsValidTile(theX, theY)) {
                    theResult = 7;
                } else {
                    float theHeight = theMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY());
                    if (thePosition.GetZ() <= theHeight && !theMesh.GetIsSpaceTile(theX, theY)) {
                        theMesh.GetMeshNormal(thePosition.GetX(), thePosition.GetY(), theNormal);
                        if (theNormal.DotProduct(this->mVelocity) < 0.0f) {
                            thePosition.SetZ(theHeight);
                            float theBounce = theNormal.DotProduct(this->mVelocity) * (-1.0f - this->mGraphicEffect->GetParam10())
                                              / theNormal.DotProduct(theNormal);
                            this->mVelocity.SetX(theNormal.mX * theBounce + this->mVelocity.GetX());
                            this->mVelocity.SetY(theNormal.mY * theBounce + this->mVelocity.GetY());
                            this->mVelocity.SetZ(theNormal.mZ * theBounce + this->mVelocity.GetZ());
                        }
                    }
                }
            }
            if (theResult == 0) {
                this->mDrawAge = this->mAge / this->mGraphicEffect->mDuration;
                this->mDrawPosition = thePosition;
                this->mMoved = true;
            }
        } else {
            theResult = 7;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e4f32
// Releases the particle's resources and returns it to the pool
void EEE2DParticle::LastDereference()
{
    this->ReleaseResources();
    this->mReferenceCount = 0;
    EEE2DParticle::sPool.Delete(this);
}

// FUNCTION: EXE 0x4e4f4b
// Reloads the graphic and reads velocity, age, rotation, brightness and (saves after 0.217) the last update time
// Returns the first read error; the base class result is ignored
long EEE2DParticle::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult;
    if (inMajorVersion <= 0 && inMinorVersion <= 0xd9) {
        theResult = this->mVelocity.Load(inFile);
        if (theResult == 0) {
            theResult = inFile.Read(this->mAge);
            if (theResult == 0) {
                theResult = inFile.Read(this->mRotation);
                if (theResult == 0) {
                    theResult = inFile.Read(this->mBrightness);
                    if (theResult == 0) {
                        this->mLastTime = 0.0f;
                        this->mDrawPosition = this->mPosition;
                        this->mDuration = this->mGraphicEffect->mDuration;
                        this->mMoved = false;
                    }
                }
            }
        }
    } else {
        theResult = this->mVelocity.Load(inFile);
        if (theResult == 0) {
            theResult = inFile.Read(this->mAge);
            if (theResult == 0) {
                theResult = inFile.Read(this->mRotation);
                if (theResult == 0) {
                    theResult = inFile.Read(this->mBrightness);
                    if (theResult == 0) {
                        theResult = inFile.Read(this->mLastTime);
                        this->mDrawPosition = this->mPosition;
                        this->mDuration = this->mGraphicEffect->mDuration;
                        this->mMoved = false;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4e5078
// Draws the particle as a coloured sprite growing from the start size and fading with age (two sprite styles by
// flag 1); returns 0
long EEE2DParticle::DrawPass6()
{
    if (!this->mGraphicEffect->mFlag1) {
        float theReference = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f;
        float theAlpha = this->mGraphicEffect->GetParam1();
        float theStartSize = this->mGraphicEffect->GetStartSize();
        float theEndSize = this->mGraphicEffect->GetSize();
        float theFrom = this->mGraphicEffect->GetStartSize();
        GEColor theColor(this->mGraphicEffect->GetParam5(), this->mGraphicEffect->GetParam6(), this->mGraphicEffect->GetParam7());
        AddBlendedSprite(this->mDrawPosition, theColor, this->mRotation, (theEndSize - theFrom) * this->mDrawAge + theStartSize,
                 (1.0f - this->mDrawAge) * theAlpha, this->mBrightness, theReference);
    } else {
        float theReference = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f;
        float theAlpha = this->mGraphicEffect->GetParam1();
        float theStartSize = this->mGraphicEffect->GetStartSize();
        float theEndSize = this->mGraphicEffect->GetSize();
        float theFrom = this->mGraphicEffect->GetStartSize();
        GEColor theColor(this->mGraphicEffect->GetParam5(), this->mGraphicEffect->GetParam6(), this->mGraphicEffect->GetParam7());
        AddGlowSprite(this->mDrawPosition, theColor, this->mRotation, (theEndSize - theFrom) * this->mDrawAge + theStartSize,
                 (1.0f - this->mDrawAge) * theAlpha, this->mBrightness, theReference);
    }
    return 0;
}

// FUNCTION: EXE 0x4e517c
// Syncs the position to the drawn one, then writes velocity, age, rotation, brightness and last update time
// after the base state; returns the first write error
long EEE2DParticle::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mPosition != this->mDrawPosition)
        this->SetPosition(this->mDrawPosition);
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mVelocity.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mAge);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mRotation);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mBrightness);
    if (theResult == 0)
        theResult = outFile.Write(this->mLastTime);
    return theResult;
}

// FUNCTION: EXE 0x4e520d
// Copies the drawn position back after a move; returns 7 (remove) once the particle's duration is over
long EEE2DParticle::Step()
{
    if (gPhysics->mGameClock - this->mLastTime + this->mAge <= this->mDuration) {
        if (this->mMoved) {
            this->mMoved = false;
            this->SetPosition(this->mDrawPosition);
        }
        return 0;
    }
    return 7;
}

// The pool's COMDATs (UMemoryPool<EEE2DParticle>, lle/UMemoryPool.h) and the particle's ??_E follow:
// FUNCTION: EXE 0x4e524e
// FUNCTION: EXE 0x4e5288
// FUNCTION: EXE 0x4e52aa
// FUNCTION: EXE 0x4e5317
// FUNCTION: EXE 0x4e537a
// FUNCTION: EXE 0x4e5467
// FUNCTION: EXE 0x4e5574
// FUNCTION: EXE 0x4e55fc
// FUNCTION: EXE 0x4e565d
// UMemoryPool<EEE2DParticle>'s ctor, ??_G, Delete, DumpStats, New, SetSize, chunk ctor and AddChunk, and the
// particle's vector deleting destructor (template members from lle/UMemoryPool.h)
