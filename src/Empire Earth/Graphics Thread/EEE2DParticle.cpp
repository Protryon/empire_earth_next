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
// (the draws are ee_gfx effect_state.rs's)
long EEE2DParticle::SetUp()
{
    EeGfxParticleSetUp theSetUp = {(int32_t)this->mGraphicEffect->mIntParam1, this->mGraphicEffect->GetParam2(),
                                   this->mGraphicEffect->GetParam3()};
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    ee_gfx_effect_particle_set_up(this->mRecord, &theSetUp);
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4e4be3
// Resets the particle's motion, age, look and timing to zero, with no graphic
void EEE2DParticle::Clear()
{
    ee_gfx_effect_particle_clear(this->mRecord);
    this->mGraphicObject = NULL;
}

// The move's world reads (both particles) (provisional name)
EeGfxParticleTick EEParticleTick(EEGraphicEffect* inEffect, const GE3DPoint& inPosition)
{
    EeGfxParticleTick theTick;
    const GEVector& theWind = gWorld.mWindDirection;
    theTick.now = gPhysics->mGameClock;
    theTick.game_over = gPhysics->mGameOver;
    theTick.wind.x = theWind.GetX();
    theTick.wind.y = theWind.GetY();
    theTick.wind.z = theWind.GetZ();
    theTick.position = *reinterpret_cast<const EeGe3DPoint*>(&inPosition);
    theTick.duration = (uint32_t)inEffect->mDuration;
    theTick.param4 = inEffect->mParam4;
    theTick.param8 = inEffect->mParam8;
    theTick.param9 = inEffect->GetParam9();
    theTick.param10 = inEffect->GetParam10();
    theTick.param11 = inEffect->mParam11;
    theTick.flag2 = inEffect->mFlag2;
    return theTick;
}

// FUNCTION: EXE 0x4e4c31
// Moves the particle by the game time passed: gravity, drag, wind and lift, bouncing off the ground when the
// effect asks (ee_gfx effect_state.rs); returns 7 (remove) once it is past its duration or off the map
long EEE2DParticle::Update()
{
    EeGfxParticleTick theTick = EEParticleTick(this->mGraphicEffect, this->mPosition);
    EeGfxParticleGround theGround = EEParticleGround();
    return ee_gfx_effect_particle_update(this->mRecord, &theTick, &theGround);
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
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    bool theLoaded = false;
    long theResult = ee_gfx_effect_load_particle(this->mRecord, &theFile, (uint32_t)inMajorVersion,
                                                 (uint32_t)inMinorVersion, &theLoaded);
    if (theLoaded) {
        this->mDrawPosition = this->mPosition;
        this->mDuration = this->mGraphicEffect->mDuration;
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
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_particle(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4e520d
// Copies the drawn position back after a move; returns 7 (remove) once the particle's duration is over
long EEE2DParticle::Step()
{
    bool theMove = false;
    long theResult = ee_gfx_effect_particle_step(this->mRecord, gPhysics->mGameClock, &theMove);
    if (theMove)
        this->SetPosition(this->mDrawPosition);
    return theResult;
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
