// C:\EEX\Empire Earth\Graphics Thread\EEE3DParticle.cpp (file name inferred: no __FILE__; real key/RTTI
// and static-pool chains delimit original unit 0xf4bf3..0xf5d94). Prior Nuclear ends at0xf4bf3;
// registered Emitter key follows at0xf5d94. 34 original functions/4513B, including real pool COMDATs.
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
#include "ee/EEE3DParticle.h"

// SYNTHETIC: EXE 0x4f4bf3
// SYNTHETIC: EXE 0x4f4bfd
// SYNTHETIC: EXE 0x4f4c0e
// SYNTHETIC: EXE 0x4f4c1a
const UString EEE3DParticle::sData("EEE3DParticle-Data");
// SYNTHETIC: EXE 0x4f4c36
// SYNTHETIC: EXE 0x4f4c40
// SYNTHETIC: EXE 0x4f4c4a
// SYNTHETIC: EXE 0x4f4c56
UMemoryPool<EEE3DParticle> EEE3DParticle::sPool;

// FUNCTION: EXE 0x4f4c7b
// Creates a 3D particle effect of the given kind, cleared and with no references
EEE3DParticle::EEE3DParticle(long inKind)
    : EEEffect3D(inKind)
{
    this->Reinit();
    this->Clear();
    this->mReferenceCount = 0;
}

// FUNCTION: EXE 0x4f4cc8
// Destroys the particle, unloading its graphic object's media
EEE3DParticle::~EEE3DParticle()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

// FUNCTION: EXE 0x4f4cfa
// Sets the chunk size and chunk limit of the particle memory pool; returns whether it succeeded
bool EEE3DParticle::SetPoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return EEE3DParticle::sPool.SetSize(inChunkSize, inMaxChunks, 1);
}

long EEE3DParticle::DrawPass2() { return 0; }
long EEE3DParticle::DrawPass4() { return 0; }
long EEE3DParticle::DrawPass5() { return 0; }
void EEE3DParticle::PrepareQueue() {}

// FUNCTION: EXE 0x4f4d0f
// Updates the particle and queues it in render queue 4 under its effect's texture; returns 7 once expired
long EEE3DParticle::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
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

// FUNCTION: EXE 0x4f4d77
// Takes a particle from the pool for a graphic effect, placed at a point at rest, with the effect's duration
EEE3DParticle* EEE3DParticle::Create(GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect)
{
    GETransformation theTransformation;
    EEE3DParticle* theParticle = EEE3DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(NULL, inGraphicEffect);
    theParticle->Clear();
    theTransformation.SetPosition(inLocation);
    theParticle->SetTransformation(theTransformation);
    theParticle->mPosition = inLocation;
    theParticle->mLastTime = gPhysics->GetGameClock();
    theParticle->mDuration = inGraphicEffect->mDuration;
    return theParticle;
}

// FUNCTION: EXE 0x4f4e27
// Takes a particle from the pool for a graphic effect, placed at the line's start and moving along its direction
EEE3DParticle* EEE3DParticle::Create(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect)
{
    GETransformation theTransformation;
    EEE3DParticle* theParticle = EEE3DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(NULL, inGraphicEffect);
    theParticle->Clear();
    theTransformation.SetPosition(inLine.GetStart());
    theParticle->SetTransformation(theTransformation);
    theParticle->mPosition = inLine.GetStart();
    theParticle->mVelocity = inLine.GetDirection();
    theParticle->mLastTime = gPhysics->GetGameClock();
    theParticle->mDuration = inGraphicEffect->mDuration;
    return theParticle;
}

// FUNCTION: EXE 0x4f4eec
// Takes a cleared particle from the pool with no graphic effect
EEE3DParticle* EEE3DParticle::Create()
{
    EEE3DParticle* theParticle = EEE3DParticle::sPool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit();
    theParticle->Clear();
    return theParticle;
}

// FUNCTION: EXE 0x4f4f18
// Dumps the particle pool's statistics and frees the pool
void EEE3DParticle::ReleasePool()
{
    EEE3DParticle::sPool.DumpStats(false);
    EEE3DParticle::sPool.Release();
}

// FUNCTION: EXE 0x4f4f30
// Unloads the particle's graphic object media, then releases the base effect's resources
void EEE3DParticle::ReleaseResources()
{
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    EEEffect3D::ReleaseResources();
}

// FUNCTION: EXE 0x4f4f56
// Starts the particle: random yaw/pitch/roll unless the effect's int param 1 is set, size from the effect,
// brightness random in [param 2 - param 3, param 2 + param 3] clamped to 0..1; loads its media
long EEE3DParticle::SetUp()
{
    EeGfxParticleSetUp theSetUp = {(int32_t)this->mGraphicEffect->mIntParam1, this->mGraphicEffect->GetParam2(),
                                   this->mGraphicEffect->GetParam3()};
    ee_gfx_effect_particle_set_up(this->mRecord, &theSetUp);
    this->mBoundingRadius = this->mGraphicEffect->GetSize();
    this->mGraphicObject = this->GetGraphicObject();
    return this->mGraphicObject->LoadMedia();
}

// FUNCTION: EXE 0x4f503c
// Resets the particle's motion, age, orientation, brightness and timing to zero and drops its graphic object
void EEE3DParticle::Clear()
{
    ee_gfx_effect_particle_clear(this->mRecord);
    this->mGraphicObject = NULL;
}

// FUNCTION: EXE 0x4f50b2
// Advances the particle by the elapsed game time: drag, gravity, wind drift and rise, bouncing off the terrain when
// enabled (frozen over an invalid tile); returns 7 once older than the effect's duration
long EEE3DParticle::Update()
{
    GE3DPoint theLocation(this->mTransformation.GetX(), this->mTransformation.GetY(), this->mTransformation.GetZ());
    EeGfxParticleTick theTick = EEParticleTick(this->mGraphicEffect, theLocation);
    EeGfxParticleGround theGround = EEParticleGround();
    return ee_gfx_effect_particle_update(this->mRecord, &theTick, &theGround);
}

// FUNCTION: EXE 0x4f53a3
// Releases the particle's resources and returns it to the pool
void EEE3DParticle::LastDereference()
{
    this->ReleaseResources();
    this->mReferenceCount = 0;
    EEE3DParticle::sPool.Delete(this);
}

// FUNCTION: EXE 0x4f53bc
// Reads the particle (velocity, age, orientation, brightness, and from version 0.218 its clock) from a saved game;
// returns the first read error
long EEE3DParticle::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    bool theLoaded = false;
    long theResult = ee_gfx_effect_load_particle(this->mRecord, &theFile, (uint32_t)inMajorVersion,
                                                 (uint32_t)inMinorVersion, &theLoaded);
    if (theLoaded) {
        this->mTransformation.GetPosition(this->mPosition);
        this->mDuration = this->mGraphicEffect->mDuration;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f556b
// Draws the particle model with the effect's colour and growing size, alpha blended and fading with age, or
// additively lit when the effect's flag 1 is set; restores the model colour, opacity and fog
long EEE3DParticle::DrawPass6()
{
    GEColor theColor(this->mGraphicEffect->GetParam5(), this->mGraphicEffect->GetParam6(), this->mGraphicEffect->GetParam7());
    unsigned long theScreen = gScreenIndex;
    GEModel* theModel = this->mGraphicObject->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    float theSize = (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mLifeFraction + this->mGraphicEffect->GetStartSize();
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    if (!this->mGraphicEffect->mFlag1) {
        gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
        SetUnitLighting();
        theModel->SetOpacity((1.0f - this->mLifeFraction) * this->mGraphicEffect->GetParam1());
        GEColor theAmbient(this->mBrightness, this->mBrightness, this->mBrightness);
        gRasterizer->SetAmbientColor(theAmbient, theScreen);
    } else {
        float theLight;
        GetFogFactor(this->mPosition, theFog, theLight);
        gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
        SetWhiteLighting();
        theModel->SetOpacity(this->mGraphicEffect->GetParam1());
        float theValue = (1.0f - this->mLifeFraction) * this->mBrightness * theLight;
        GEColor theAmbient(theValue, theValue, theValue);
        gRasterizer->SetAmbientColor(theAmbient, theScreen);
    }
    theModel->GetMaterial(0)->SetColor(theColor);
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    GETransformation theTransformation(false);
    theTransformation.SetOrientationYPR(this->mYaw, this->mPitch, this->mRoll);
    theTransformation.SetPosition(this->mPosition);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theSize);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    theModel->GetMaterial(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f));
    theModel->SetOpacity(1.0f);
    gRasterizer->SetFog(theFog, theScreen);
    return theResult;
}

// FUNCTION: EXE 0x4f57fc
// Writes the particle's velocity, age, orientation, brightness and clock; returns the first write error
long EEE3DParticle::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_particle(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4f588d
// Returns 7 once the particle has outlived its duration; otherwise moves its transformation to its new position
long EEE3DParticle::Step()
{
    bool theMove = false;
    long theResult = ee_gfx_effect_particle_step(this->mRecord, gPhysics->GetGameClock(), &theMove);
    if (theMove) {
        GETransformation theTransformation;
        theTransformation.SetPosition(this->mPosition);
        this->SetTransformation(theTransformation);
    }
    return theResult;
}

// Naturally emitted UMemoryPool<EEE3DParticle> family and vector deleting destructor.
// TEMPLATE: EXE 0x4f5912
// TEMPLATE: EXE 0x4f594c
// TEMPLATE: EXE 0x4f596e
// TEMPLATE: EXE 0x4f59db
// TEMPLATE: EXE 0x4f5a3e
// TEMPLATE: EXE 0x4f5b2b
// TEMPLATE: EXE 0x4f5c38
// SYNTHETIC: EXE 0x4f5cc0
// TEMPLATE: EXE 0x4f5d21
