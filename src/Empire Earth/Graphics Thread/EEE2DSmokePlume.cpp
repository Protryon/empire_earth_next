// C:\EEX\Empire Earth\Graphics Thread\EEE2DSmokePlume.cpp (no __FILE__; name from the RTTI class and its key
// "EEE2DSmokePlume-Data"). 0xe6ef2..0xe72fa: the key chain, both ctors .. Step; EEE2DSquareMarchingIcons.cpp
// follows. New (port, RUST_PLAN R4.4): also the original's EEE2DVelocityParticle.cpp (0xe753d..0xe773c) as kind 0xc;
// the saved state, the constructor's draws, the tick, load and save are ee_gfx effect_state.rs's.
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
#include "ee/EEE2DSmokePlume.h"

// SYNTHETIC: EXE 0x4e6ef2
// SYNTHETIC: EXE 0x4e6efc
// SYNTHETIC: EXE 0x4e6f0d
// SYNTHETIC: EXE 0x4e6f19
const UString EEE2DSmokePlume::sData("EEE2DSmokePlume-Data");

// FUNCTION: EXE 0x4e6f35
// Creates a smoke puff of the given kind at a point with a random rotation and a random brightness in
// parameter 1 +- 2, clamped to 0..1
EEE2DSmokePlume::EEE2DSmokePlume(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, inKind, false)
{
    static const EeGe3DPoint kNoVelocity = {0.0f, 0.0f, 0.0f};
    this->mGraphicObject = NULL;
    this->mDrawAge = 0.0f;
    ee_gfx_effect_plume_new(this->mRecord, reinterpret_cast<const EeGe3DPoint*>(&inPosition),
                            this->mGraphicEffect->GetParam1(), this->mGraphicEffect->GetParam2(), &kNoVelocity);
}

// FUNCTION: EXE 0x4e7580
// Creates a moving smoke puff (kind 12) at the line's start with the line's direction as its velocity (the
// original's EEE2DVelocityParticle)
EEE2DSmokePlume::EEE2DSmokePlume(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect)
    : EEEffect2D(0, inGraphicEffect, gEffect2DModel, EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE, false)
{
    GE3DPoint theStart = inLine.GetStart();
    GEVector theDirection = inLine.GetDirection();
    EeGe3DPoint theVelocity = {theDirection.GetX(), theDirection.GetY(), theDirection.GetZ()};
    this->mGraphicObject = NULL;
    this->mDrawAge = 0.0f;
    ee_gfx_effect_plume_new(this->mRecord, reinterpret_cast<const EeGe3DPoint*>(&theStart),
                            this->mGraphicEffect->GetParam1(), this->mGraphicEffect->GetParam2(), &theVelocity);
}

// SYNTHETIC: EXE 0x4e7004
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4e7020
// Creates an empty smoke puff of the given kind for loading from a saved game
EEE2DSmokePlume::EEE2DSmokePlume(long inKind)
    : EEEffect2D(gEffect2DModel, inKind, false), mGraphicObject(NULL)
{
}

// FUNCTION: EXE 0x4e705e
// Destroys the puff, unloading its graphic's media
EEE2DSmokePlume::~EEE2DSmokePlume()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
}

long EEE2DSmokePlume::DrawPass2()
{
    return 0;
}

long EEE2DSmokePlume::DrawPass4()
{
    return 0;
}

long EEE2DSmokePlume::DrawPass5()
{
    return 0;
}

// FUNCTION: EXE 0x4e707c
// Queues the puff in render queue 4 under its texture for this screen, snapshotting its age and drifting point
long EEE2DSmokePlume::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect2D::Queue(inQueues);
    if (theResult == 0) {
        this->mDrawAge = this->mAge;
        this->mDrawPosition = this->mStart;
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

void EEE2DSmokePlume::PrepareQueue()
{
}

// FUNCTION: EXE 0x4e70ef
// Loads the puff's graphic and sets its position and bounding radius (the effect's size); returns the load result
long EEE2DSmokePlume::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    this->SetPosition(this->mStart);
    this->SetBoundingRadius(this->mGraphicEffect->GetSize());
    return theResult;
}

// FUNCTION: EXE 0x4e711d
// Draws the puff as a smoke sprite growing from the start size and fading out with age; returns 0
long EEE2DSmokePlume::DrawPass6()
{
    float theReference = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mAlphaReference : 0.0f;
    GEColor theColor;
    AddBlendedSprite(this->mDrawPosition, theColor, this->mRotation,
             (this->mGraphicEffect->GetSize() - this->mGraphicEffect->GetStartSize()) * this->mDrawAge + this->mGraphicEffect->GetStartSize(),
             (1.0f - this->mDrawAge) * this->mGraphicEffect->GetAlpha(), this->mBrightness, theReference);
    return 0;
}

// FUNCTION: EXE 0x4e718e
// Reloads the puff's graphic and reads its point, age, rotation and brightness; returns the first read error
// (the base class result is ignored)
// (ee_gfx effect_state.rs; the velocity particle then its velocity, whose result it returns)
long EEE2DSmokePlume::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_effect_load_plume(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4e71f1
// Writes the puff's point, age, rotation and brightness; returns the first write error (base result ignored)
// (ee_gfx effect_state.rs)
long EEE2DSmokePlume::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save_plume(this->mRecord, &theFile);
}

// FUNCTION: EXE 0x4e7243
// Ages the puff and, unless flag 1 is set, drifts its drawn point with the wind and rises (the effect position
// itself is not moved); returns 7 (remove) once its life is over
// (ee_gfx effect_state.rs; the velocity particle 0xe7646: moved by its damped velocity, the wind and a fading rise,
// the effect's position following the puff)
long EEE2DSmokePlume::Step()
{
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    const GEVector& theWind = gWorld.mWindDirection;
    EeGfxPlumeTick theTick;
    theTick.tick_seconds = gPhysics->GetTickSeconds();
    theTick.tick_time = gPhysics->GetTickTime();
    theTick.wind.x = theWind.GetX();
    theTick.wind.y = theWind.GetY();
    theTick.wind.z = theWind.GetZ();
    theTick.duration = (uint32_t)theEffect->mDuration;
    theTick.int_param1 = (int32_t)theEffect->mIntParam1;
    theTick.param3 = theEffect->GetParam3();
    theTick.flag1 = theEffect->mFlag1;
    bool theMoved = false;
    long theResult = ee_gfx_effect_plume_step(this->mRecord, &theTick, &theMoved);
    if (theMoved)
        this->SetPosition(this->mStart);
    return theResult;
}
