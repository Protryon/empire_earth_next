// C:\EEX\Empire Earth\Graphics Thread\EEEffectManager.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffectManager-Data"). 0x1054ec..0x107616: the key chain, the ctor, the dtor, the effect factories (with the inline
// ctors/??_G of EEE2DImpactBlood and EEE3DShells), the list/map upkeep, Load/Save, then the map template COMDATs.
// Unit flags: the exe's module set.
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
#include <vector>
#include "lle/GE3DLine.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEWorld.h"
#include "ee/EEEffectManager.h"
#include "ee/EEE2DExplosion.h"
#include "ee/EEE2DFire.h"
#include "ee/EEE2DMuzzleFlash.h"
#include "ee/EEE2DImpact.h"
#include "ee/EEE2DImpactBlood.h"
#include "ee/EEE2DShockwave.h"
#include "ee/EEE2DExhaustTrail.h"
#include "ee/EEE2DFireTrail.h"
#include "ee/EEE2DPlasmaPulse.h"
#include "ee/EEE2DLightGlow.h"
#include "ee/EEE2DLightning.h"
#include "ee/EEE2DMarchingIcons.h"
#include "ee/EEE2DCircularMarchingIcons.h"
#include "ee/EEE2DSquareMarchingIcons.h"
#include "ee/EEE2DParticle.h"
#include "ee/EEE2DPulseWave.h"
#include "ee/EEE2DSmokePlume.h"
#include "ee/EEE2DSmokeMuzzleBlast.h"
#include "ee/EEE2DWinterBreath.h"
#include "ee/EEE2DLinearProjectile.h"
#include "ee/EEE2DWaterRipple.h"
#include "ee/EEE2DVortex.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEE2DVelocityParticle.h"
#include "ee/EEE3DIcon.h"
#include "ee/EEE3DBlastWave.h"
#include "ee/EEE3DNuclearExplosion.h"
#include "ee/EEE3DAdditivePass.h"
#include "ee/EEE3DHurricane.h"
#include "ee/EEE3DDisease.h"
#include "ee/EEE3DSmokeGenerator.h"
#include "ee/EEE3DDestructionSmoke.h"
#include "ee/EEE3DFarm.h"
#include "ee/EEE3DDamageSmoke.h"
#include "ee/EEE3DEnergyWall.h"
#include "ee/EEE3DDamageVent.h"
#include "ee/EEE3DParticleEmitter.h"
#include "ee/EEE3DMultiEffect.h"
#include "ee/EEEDamagedEffect.h"
#include "ee/EEE3DTeleport.h"
#include "ee/EEE3DCommandConfirmation.h"
#include "ee/EEE3DVirus.h"
#include "ee/EEE3DTimeWarp.h"
#include "ee/EEE3DShield.h"
#include "ee/EEE3DScreenFlash.h"
#include "ee/EEE3DEnergyWave.h"
#include "ee/EEE3DTracerBeam.h"
#include "ee/EEE3DTracers.h"
#include "ee/EEE3DCitizenBeam.h"
#include "ee/EEE3DLinearProjectile.h"
#include "ee/EEE3DEarthquake.h"
#include "ee/EEE3DMarchingLine.h"
#include "ee/EEE3DVolcano.h"
#include "ee/EEE3DWaterSplash.h"
#include "ee/EEE3DGoal.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEE3DSnow.h"
#include "ee/EEE3DRain.h"
#include "ee/EEE3DPulseCannonBall.h"
#include "ee/EEE3DParticle.h"
#include "LostDebug.h"

// SYNTHETIC: EXE 0x5054ec
// SYNTHETIC: EXE 0x5054f6
// SYNTHETIC: EXE 0x505507
// SYNTHETIC: EXE 0x505513
// the guarded atexit dtor says a class static
const UString EEEffectManager::sData("EEEffectManager-Data");

// the last handle given out (0x4e8e48; saved first by Save). A unit global, not a class static: as a second one in the
// header it flips the count canary 0x5e964a. External linkage: AddEffect 0x5055d0 increments
// it in memory and reloads it (inc [g]; mov esi, [g]), which VC6 emits only for a global it cannot keep in a register
unsigned long sEffectManagerHandle;

// FUNCTION: EXE 0x50552f
// Creates an effect manager with no effects and no queued destructions
EEEffectManager::EEEffectManager()
{
    this->mEffectsByHandle.clear();
    this->mDestroyedHandles.clear();
    this->mEffects = NULL;
}

// SYNTHETIC: EXE 0x50558c
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5055a8
// Destroys the manager, releasing every effect
EEEffectManager::~EEEffectManager()
{
    this->ClearEffects();
}

// FUNCTION: EXE 0x5055d0
// Links an effect at the head of the effect list and indexes it by handle (a new handle when asked, else its own),
// taking a reference
void EEEffectManager::AddEffect(EEEffect* inEffect, bool inNewHandle)
{
    unsigned long theHandle;
    if (inNewHandle) {
        sEffectManagerHandle++;
        theHandle = sEffectManagerHandle;
        inEffect->mHandle.mValue = theHandle;
    } else
        theHandle = inEffect->mHandle.mValue;
    inEffect->mNext = this->mEffects;
    inEffect->mPrevious = NULL;
    if (this->mEffects)
        this->mEffects->mPrevious = inEffect;
    this->mEffects = inEffect;
    inEffect->Reference();
    this->mEffectsByHandle.insert(std::map<unsigned long, EEEffect*>::value_type(theHandle, inEffect));
}

// FUNCTION: EXE 0x50562c
// Creates, sets up and registers the effect of the graphic effect's kind on a unit (kinds that need a living unit
// return 10 without one); returns 10 for other kinds, else SetUp's result (added even when SetUp fails)
long EEEffectManager::CreateUnitEffect(EEUnit* inUnit, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, bool inKeepsUnit,
                                       long inTagPoint, long inGoalIndex)
{
    EEEffect* theEffect = NULL;
    long theResult = 0;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (static_cast<unsigned long>(inEffect->mKind)) {
        case 4:
            theEffect = new EEE2DExplosion(inUnit, inEffect);
            break;
        case 5:
            theEffect = new EEE2DFire(inUnit, inEffect);
            break;
        case 9:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DMuzzleFlash(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 6:
            theEffect = new EEE2DImpact(inUnit, inEffect, 6);
            break;
        case 7:
            theEffect = new EEE2DImpactBlood(inUnit, inEffect);
            break;
        case 18:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DShockwave(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 3:
            theEffect = new EEE2DExhaustTrail(inUnit, inEffect);
            break;
        case 14:
            theEffect = new EEE2DFireTrail(inUnit, inEffect);
            break;
        case 10:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DPlasmaPulse(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 13:
            theEffect = new EEE2DLightGlow(inUnit, inEffect, inKeepsUnit);
            break;
        case 8:
            theEffect = new EEE2DLightning(inUnit, inEffect);
            break;
        case 16:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DCircularMarchingIcons(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 15:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DSquareMarchingIcons(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 2:
            theEffect = new EEE3DIcon(inUnit, inEffect, 0, 2);
            break;
        case 0x18:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DBlastWave(inUnit, inEffect, 0x18);
            else
                theResult = 10;
            break;
        case 0x28:
            theEffect = new EEE3DNuclearExplosion(inUnit, inEffect);
            break;
        case 0x27:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DAdditivePass(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x24:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DHurricane(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x23:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DDisease(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x1e:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DDestructionSmoke(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x29:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DFarm(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x34:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DEnergyWall(inUnit, inEffect, 0x34);
            else
                theResult = 10;
            break;
        case 0x2f:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DPulseWave(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x2e:
            theEffect = EEE2DParticle::Create(inUnit, inEffect);
            break;
        case 0x2c:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DDamageSmoke(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x2b:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DSmokeGenerator(inUnit, inEffect, 0x2b);
            else
                theResult = 10;
            break;
        case 0x35:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DDamageVent(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x41:
            theEffect = new EEE3DCommandConfirmation(inUnit, inEffect, inGoalIndex, 0x41);
            break;
        case 0x3f:
            theEffect = new EEE3DTeleport(inUnit, inEffect, 0x3f);
            break;
        case 0x3e:
            theEffect = new EEEDamagedEffect(inUnit, inEffect);
            break;
        case 0x3b:
            theEffect = new EEE3DMultiEffect(inUnit, inEffect, 0x3b);
            theEffect->mIsSaved = false;
            break;
        case 0x3a:
            theEffect = new EEE3DParticleEmitter(inUnit, inEffect, 0x3a, inTagPoint);
            break;
        case 0x42:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DVirus(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x47:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DEnergyWave(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x46:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DScreenFlash(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x45:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DShield(inUnit, inEffect);
            else
                theResult = 10;
            break;
        case 0x44:
            theEffect = new EEE3DTimeWarp(inUnit, inEffect, 0x44);
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect && theResult == 0) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x505dff
// (EEE2DImpactBlood's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x505e21
// Creates, sets up and registers a two-unit effect (muzzle blast, winter breath, tracers, shells, projectile,
// tracer beam); returns 10 for other kinds or missing units, else SetUp's result
long EEEffectManager::CreateEffectBetweenUnits(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inEffect,
                                               EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect2DSmokeMuzzleBlast:
            if (inFrom && inFrom->mComplexUnit && inTo && inTo->mComplexUnit)
                theEffect = new EEE2DSmokeMuzzleBlast(inFrom, inTo, inEffect);
            else
                theResult = 10;
            break;
        case kEffect2DWinterBreath:
            if (inFrom && inFrom->mComplexUnit)
                theEffect = new EEE2DWinterBreath(inFrom, inEffect);
            else
                theResult = 10;
            break;
        case kEffect3DTracers:
            if (inFrom && inFrom->mComplexUnit && inTo && inTo->mComplexUnit)
                theEffect = new EEE3DTracers(inFrom, inTo, inEffect, 0x21);
            else
                theResult = 10;
            break;
        case kEffect3DShells:
            theEffect = new EEE3DShells(inFrom, inTo, inEffect, 0x26);
            break;
        case kEffect2DLinearProjectile:
            if (inFrom && inFrom->mComplexUnit && inTo && inTo->mComplexUnit)
                theEffect = new EEE2DLinearProjectile(inFrom, inTo, inEffect);
            else
                theResult = 10;
            break;
        case kEffect3DTracerBeam:
            if (inFrom && inFrom->mComplexUnit && inTo && inTo->mComplexUnit)
                theEffect = new EEE3DTracerBeam(inFrom, inTo, inEffect, 0x40);
            else
                theResult = 10;
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect && theResult == 0) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// SYNTHETIC: EXE 0x506016
// (EEE3DShells's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x506038
// Creates, sets up and registers a unit-to-point effect (muzzle blast, projectile, citizen beam, tracer beam,
// command confirmation, pulse cannon ball); returns 10 for other kinds or a missing unit, else SetUp's result
long EEEffectManager::CreateEffect(EEUnit* inUnit, GE3DPoint& inLocation, EEGraphicEffect* inEffect,
                                   EEEffectHandle* outHandle, long inUnknown)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect2DSmokeMuzzleBlast:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DSmokeMuzzleBlast(inUnit, inLocation, inEffect);
            else
                theResult = 10;
            break;
        case kEffect2DLinearProjectile:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE2DLinearProjectile(inUnit, inLocation, inEffect);
            else
                theResult = 10;
            break;
        case kEffect3DCitizenBeam:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DCitizenBeam(inUnit, inLocation, inEffect, 0x33);
            else
                theResult = 10;
            break;
        case kEffect3DTracerBeam:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DTracerBeam(inUnit, inLocation, inEffect, 0x40);
            else
                theResult = 10;
            break;
        case kEffect3DCommandConfirmation:
            theEffect = new EEE3DCommandConfirmation(inUnit, inLocation, inEffect, inUnknown, 0x41);
            break;
        case kEffect3DPulseCannonBall:
            if (inUnit && inUnit->mComplexUnit)
                theEffect = new EEE3DPulseCannonBall(inUnit, inLocation, inEffect, 0x43, true);
            else
                theResult = 10;
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect && theResult == 0) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5061e9
// Creates, sets up and registers an effect of the graphic effect's kind at a map point; returns 10 for kinds
// without a point version, else SetUp's result
long EEEffectManager::CreatePlacedEffect(GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (static_cast<unsigned long>(inEffect->mKind)) {
        case 2:
            theEffect = new EEE3DIcon(0, inEffect, &inLocation, 2);
            break;
        case 6:
            theEffect = new EEE2DImpact(inLocation, inEffect, 6);
            break;
        case 8:
            theEffect = new EEE2DLightning(inLocation, inEffect);
            break;
        case 0xb:
            theEffect = new EEE2DSmokePlume(inLocation, inEffect, 0xb);
            break;
        case 0x12:
            theEffect = new EEE2DShockwave(inLocation, inEffect);
            break;
        case 0x18:
            theEffect = new EEE3DBlastWave(inLocation, inEffect, 0x18);
            break;
        case 0x1b:
            theEffect = new EEE3DWaterSplash(inLocation, inEffect);
            break;
        case 0x20:
            theEffect = new EEE3DVolcano(inLocation, inEffect);
            break;
        case 0x22:
            theEffect = new EEE2DWaterRipple(inLocation, inEffect);
            break;
        case 0x2b:
            theEffect = new EEE3DSmokeGenerator(inLocation, inEffect, 0x2b);
            break;
        case 0x2e:
            theEffect = EEE2DParticle::Create(inLocation, inEffect);
            break;
        case 0x31:
            theEffect = new EEE2DVortex(inLocation, inEffect);
            break;
        case 0x32:
            theEffect = new EEE2DBattlefieldAmbient(inLocation, inEffect);
            break;
        case 0x39:
            theEffect = EEE3DParticle::Create(inLocation, inEffect);
            break;
        case 0x3a:
            theEffect = new EEE3DParticleEmitter(inLocation, inEffect, 0x3a);
            break;
        case 0x3b:
            theEffect = new EEE3DMultiEffect(inLocation, inEffect, 0x3b);
            theEffect->mIsSaved = false;
            break;
        case 0x3c:
            theEffect = new EEE3DGoal(0, inEffect, &inLocation);
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5064e6
// Creates, sets up and registers a water ripple or battlefield decal at a transformation; returns 10 for other
// kinds, else SetUp's result
long EEEffectManager::CreateTransformationEffect(const GETransformation& inTransformation, EEGraphicEffect* inEffect,
                                                 EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect2DWaterRipple:
            theEffect = new EEE2DWaterRipple(inTransformation, inEffect);
            break;
        case kEffect2DBattlefieldAmbient:
            theEffect = new EEE2DBattlefieldAmbient(inTransformation, inEffect);
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x506596
// Creates, sets up and registers a particle, particle emitter or multi-effect along a line; returns 10 for other
// kinds, else SetUp's result
long EEEffectManager::CreateLineEffect(GE3DLine& inLine, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect2DVelocityParticle:
            theEffect = new EEE2DVelocityParticle(inLine, inEffect);
            break;
        case kEffect2DParticle:
            theEffect = EEE2DParticle::Create(inLine, inEffect);
            break;
        case kEffect3DParticle:
            theEffect = EEE3DParticle::Create(inLine, inEffect);
            break;
        case kEffect3DParticleEmitter:
            theEffect = new EEE3DParticleEmitter(inLine, inEffect, 0x3a);
            break;
        case kEffect3DMultiEffect:
            theEffect = new EEE3DMultiEffect(inLine, inEffect, 0x3b);
            theEffect->mIsSaved = false;
            break;
        default:
            theResult = 10;
            break;
        }
        if (theEffect) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x50667c
// Creates, sets up and registers an earthquake, 3D projectile or marching line between two points; other kinds
// create nothing and return 0
long EEEffectManager::CreateEffectBetween(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect3DEarthquake:
            theEffect = new EEE3DEarthquake(inFrom, inTo, inEffect);
            break;
        case kEffect3DLinearProjectile:
            theEffect = new EEE3DLinearProjectile(inFrom, inTo, inEffect);
            break;
        case kEffect3DMarchingLine:
            theEffect = new EEE3DMarchingLine(inFrom, inTo, inEffect);
            break;
        default:
            break;
        }
        if (theEffect) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x50673e
// Creates, sets up and registers rain or snow over a set of tiles; other kinds create nothing and return 0
long EEEffectManager::CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    long theResult = 0;
    EEEffect* theEffect = NULL;
    if (inEffect->mDetailLevel <= gEffectDetail) {
        switch (inEffect->mKind) {
        case kEffect3DRain:
            theEffect = new EEE3DRain(inTiles, inEffect);
            break;
        case kEffect3DSnow:
            theEffect = new EEE3DSnow(inTiles, inEffect);
            break;
        default:
            break;
        }
        if (theEffect) {
            theResult = theEffect->SetUp();
            this->AddEffect(theEffect, true);
            if (outHandle)
                *outHandle = theEffect->mHandle;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5067ce
// Destroys every queued effect that still exists and empties the queue; returns 3 or 9 for the last handle looked
// at (9: no handle)
long EEEffectManager::DestroyQueued()
{
    long theResult = 0;
    std::vector<unsigned long>::iterator it = this->mDestroyedHandles.begin();
    while (it != this->mDestroyedHandles.end()) {
        theResult = 9;
        if (*it != kNoEffectHandle.mValue) {
            theResult = 3;
            std::map<unsigned long, EEEffect*>::iterator theFound = this->mEffectsByHandle.find(*it);
            if (theFound != this->mEffectsByHandle.end()) {
                theFound->second->Release();
                this->RemoveEffect(theFound->second);
                this->mEffectsByHandle.erase(theFound);
            }
        }
        // unqueue every handle: handles of effects that already removed themselves (or of none) have nothing left
        // to destroy
        it = this->mDestroyedHandles.erase(it);
    }
    return theResult;
}

// FUNCTION: EXE 0x506852
// Returns the effect with the given handle, or null
EEEffect* EEEffectManager::GetEffect(EEEffectHandle inHandle)
{
    EEEffect* theEffect = NULL;
    if (inHandle != kNoEffectHandle) {
        std::map<unsigned long, EEEffect*>::iterator it = this->mEffectsByHandle.find(inHandle.mValue);
        if (it != this->mEffectsByHandle.end())
            theEffect = it->second;
    }
    return theEffect;
}

// FUNCTION: EXE 0x506889
// Unlinks and releases every effect and empties the handle index
void EEEffectManager::ClearEffects()
{
    std::map<unsigned long, EEEffect*>::iterator it = this->mEffectsByHandle.begin();
    while (it != this->mEffectsByHandle.end()) {
        it->second->Release();
        this->RemoveEffect(it->second);
        it = this->mEffectsByHandle.erase(it);
    }
}

// FUNCTION: EXE 0x5068c6
// Queues an effect for destruction at the next update; returns 0
long EEEffectManager::DestroyEffect(EEEffectHandle inHandle)
{
    this->mDestroyedHandles.push_back(inHandle.mValue);
    return 0;
}

// FUNCTION: EXE 0x5068d8
// After a load, repairs every effect's pointers, dropping effects whose unit is gone or that are above the
// detail setting, then the goal markers'; returns the first other error
long EEEffectManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    std::map<unsigned long, EEEffect*>::iterator it;
    long theResult = 0;
    for (it = this->mEffectsByHandle.begin(); it != this->mEffectsByHandle.end() && theResult == 0; ) {
        theResult = it->second->RepairPointers();
        if (theResult == 7 || it->second->mGraphicEffect->mDetailLevel > gEffectDetail) {
            it->second->Release();
            this->RemoveEffect(it->second);
            it = this->mEffectsByHandle.erase(it);
            theResult = 0;
        } else
            it++;
    }
    if (theResult == 0)
        theResult = this->mGoals.RepairPointers(inFirst, inSecond);
    return theResult;
}

// FUNCTION: EXE 0x506978
// Reads the handle counter, then each effect by kind (old saves give handles anew when needed), the goal markers
// and the destruction queue; returns the first read error (unknown kinds are not skipped)
long EEEffectManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theCount;
    unsigned long theKind;
    unsigned long theLastHandle;
    unsigned long theHandleCount;
    unsigned long theHandle;
    EEEffect* theEffect = NULL;
    long theResult = inFile.Read(sEffectManagerHandle);
    theLastHandle = sEffectManagerHandle;
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            for (long i = 0; i < theCount; i++) {
                if (theResult != kErrorOK)
                    return theResult;
                theResult = inFile.Read(theKind);
                if (theResult != kErrorOK)
                    return theResult;
                switch (theKind) {
                case kEffectDamaged:
                    theEffect = new EEEDamagedEffect();
                    break;
                case kEffect3DParticle:
                    theEffect = EEE3DParticle::Create();
                    break;
                case kEffect3DParticleEmitter:
                    theEffect = new EEE3DParticleEmitter(0x3a);
                    break;
                case kEffect2DParticle:
                    theEffect = EEE2DParticle::Create();
                    break;
                case kEffect3DDestructionSmoke:
                    theEffect = new EEE3DDestructionSmoke();
                    break;
                case kEffect2DVelocityParticle:
                    theEffect = new EEE2DVelocityParticle();
                    break;
                case kEffect3DWaterSplash:
                    theEffect = new EEE3DWaterSplash();
                    break;
                case kEffect3DDamageSmoke:
                    theEffect = new EEE3DDamageSmoke();
                    break;
                case kEffect3DSmokeGenerator:
                    theEffect = new EEE3DSmokeGenerator(0x2b);
                    break;
                case kEffect2DExplosion:
                    theEffect = new EEE2DExplosion();
                    break;
                case kEffect2DSmokePlume:
                    theEffect = new EEE2DSmokePlume(0xb);
                    break;
                case kEffect2DShockwave:
                    theEffect = new EEE2DShockwave();
                    break;
                case kEffect2DFire:
                    theEffect = new EEE2DFire();
                    break;
                case kEffect2DMuzzleFlash:
                    theEffect = new EEE2DMuzzleFlash();
                    break;
                case kEffect2DImpact:
                    theEffect = new EEE2DImpact(6);
                    break;
                case kEffect2DImpactBlood:
                    theEffect = new EEE2DImpactBlood();
                    break;
                case kEffect2DExhaustTrail:
                    theEffect = new EEE2DExhaustTrail();
                    break;
                case kEffect2DPlasmaPulse:
                    theEffect = new EEE2DPlasmaPulse();
                    break;
                case kEffect2DLightGlow:
                    theEffect = new EEE2DLightGlow();
                    break;
                case kEffect2DLightning:
                    theEffect = new EEE2DLightning();
                    break;
                case kEffect2DFireTrail:
                    theEffect = new EEE2DFireTrail();
                    break;
                case kEffect2DSquareMarchingIcons:
                    theEffect = new EEE2DSquareMarchingIcons();
                    break;
                case kEffect2DCircularMarchingIcons:
                    theEffect = new EEE2DCircularMarchingIcons();
                    break;
                case kEffect2DPulseWave:
                    theEffect = new EEE2DPulseWave();
                    break;
                case kEffect2DSmokeMuzzleBlast:
                    theEffect = new EEE2DSmokeMuzzleBlast();
                    break;
                case kEffect2DWinterBreath:
                    theEffect = new EEE2DWinterBreath();
                    break;
                case kEffect2DLinearProjectile:
                    theEffect = new EEE2DLinearProjectile();
                    break;
                case kEffect2DWaterRipple:
                    theEffect = new EEE2DWaterRipple();
                    break;
                case kEffect3DEarthquake:
                    theEffect = new EEE3DEarthquake();
                    break;
                case kEffect3DTracers:
                    theEffect = new EEE3DTracers(0x21);
                    break;
                case kEffect3DVolcano:
                    theEffect = new EEE3DVolcano();
                    break;
                case kEffect3DDisease:
                    theEffect = new EEE3DDisease();
                    break;
                case kEffect3DFarm:
                    theEffect = new EEE3DFarm();
                    break;
                case kEffect3DHurricane:
                    theEffect = new EEE3DHurricane();
                    break;
                case kEffect3DLinearProjectile:
                    theEffect = new EEE3DLinearProjectile();
                    break;
                case kEffect3DRain:
                    theEffect = new EEE3DRain();
                    break;
                case kEffect3DSnow:
                    theEffect = new EEE3DSnow();
                    break;
                case kEffect3DShells:
                    theEffect = new EEE3DShells(0x26);
                    break;
                case kEffect3DAdditivePass:
                    theEffect = new EEE3DAdditivePass();
                    break;
                case kEffect3DNuclearExplosion:
                    theEffect = new EEE3DNuclearExplosion();
                    break;
                case kEffect3DIcon:
                    theEffect = new EEE3DIcon(2);
                    break;
                case kEffect3DMarchingLine:
                    theEffect = new EEE3DMarchingLine();
                    break;
                case kEffect3DGoal:
                    theEffect = new EEE3DGoal();
                    break;
                case kEffect3DTeleport:
                    theEffect = new EEE3DTeleport(0x3f);
                    break;
                case kEffect3DTimeWarp:
                    theEffect = new EEE3DTimeWarp(0x44);
                    break;
                case kEffect3DTracerBeam:
                    theEffect = new EEE3DTracerBeam(0x40);
                    break;
                case kEffect3DPulseCannonBall:
                    theEffect = new EEE3DPulseCannonBall(0x43);
                    break;
                case kEffect3DShield:
                    theEffect = new EEE3DShield();
                    break;
                case kEffect2DVortex:
                    theEffect = new EEE2DVortex();
                    break;
                case kEffect3DBlastWave:
                    theEffect = new EEE3DBlastWave(0x18);
                    break;
                case kEffect3DVirus:
                    theEffect = new EEE3DVirus();
                    break;
                case kEffect2DBattlefieldAmbient:
                    theEffect = new EEE2DBattlefieldAmbient();
                    break;
                case kEffect3DCitizenBeam:
                    theEffect = new EEE3DCitizenBeam(0x33);
                    break;
                case kEffect3DEnergyWall:
                    theEffect = new EEE3DEnergyWall(0x34);
                    break;
                case kEffect3DDamageVent:
                    theEffect = new EEE3DDamageVent();
                    break;
                case kEffect3DScreenFlash:
                    theEffect = new EEE3DScreenFlash();
                    break;
                case kEffect3DEnergyWave:
                    theEffect = new EEE3DEnergyWave();
                    break;
                case 0x11:
                    break;
                }
                if (theEffect) {
                    theResult = theEffect->Load(inMajorVersion, inMinorVersion, inFile);
                    this->AddEffect(theEffect, inMajorVersion == 0 && inMinorVersion <= 0xc8
                                         && (theEffect->mHandle == kNoEffectHandle
                                             || theEffect->mHandle.mValue > theLastHandle));
                }
                theEffect = NULL;
            }
            if (theResult == kErrorOK) {
                theResult = this->mGoals.Load(inMajorVersion, inMinorVersion, inFile);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(theHandleCount);
                    for (unsigned long j = 0; j < theHandleCount && theResult == kErrorOK; j++) {
                        theResult = inFile.Read(theHandle);
                        if (theResult == kErrorOK && theHandle != kNoEffectHandle.mValue)
                            this->mDestroyedHandles.push_back(theHandle);
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5072a8
// Creates an empty velocity particle for loading (EEE2DVelocityParticle's inline default ctor, emitted out of line)

// FUNCTION: EXE 0x5072d0
// Unlinks an effect from the effect list and releases it
void EEEffectManager::RemoveEffect(EEEffect* inEffect)
{
    if (inEffect->mPrevious)
        inEffect->mPrevious->mNext = inEffect->mNext;
    if (inEffect->mNext)
        inEffect->mNext->mPrevious = inEffect->mPrevious;
    if (this->mEffects == inEffect)
        this->mEffects = inEffect->mNext;
    inEffect->Release();
}

// FUNCTION: EXE 0x507305
// Writes the handle counter, the saved effects (kind, then data), and the destruction queue; an effect's write
// error can be overwritten by the next effect's result
long EEEffectManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::map<unsigned long, EEEffect*>::iterator it;
    unsigned long theHandleCount = 0;
    long theCount = 0;
    unsigned long theKind;
    long theResult = outFile.Write(sEffectManagerHandle);
    if (theResult == 0) {
        for (it = this->mEffectsByHandle.begin(); it != this->mEffectsByHandle.end(); it++)
            if (it->second->mIsSaved)
                theCount++;
        theResult = outFile.Write(theCount);
        if (theResult == 0) {
            for (it = this->mEffectsByHandle.begin(); it != this->mEffectsByHandle.end(); it++) {
                if (!it->second->mIsSaved)
                    continue;
                theKind = it->second->mKind;
                theResult = outFile.Write(theKind);
                if (theResult == 0)
                    theResult = it->second->Save(inMajorVersion, inMinorVersion, outFile);
            }
            if (theResult == 0) {
                theHandleCount = this->mDestroyedHandles.size();
                theResult = outFile.Write(theHandleCount);
                for (unsigned long i = 0; i < theHandleCount && theResult == 0; i++)
                    theResult = outFile.Write(this->mDestroyedHandles[i]);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x507430
// Moves every effect on the old unit to the new one
void EEEffectManager::ReplaceUnit(EEUnit* inOld, EEUnit* inNew)
{
    for (EEEffect* theEffect = this->mEffects; theEffect; theEffect = theEffect->mNext)
        theEffect->ReplaceUnit(inOld, inNew);
}

// FUNCTION: EXE 0x507450
// Updates the goal markers, destroys the queued effects, then steps every effect, removing those that finish
// (Step returns 7)
void EEEffectManager::Update()
{
    this->mGoals.Update();
    this->DestroyQueued();
    EEEffect* theEffect = this->mEffects;
    while (theEffect) {
        EEEffect* theNext = theEffect->mNext;
        if (theEffect->Step() == 7) {
            EEEffectHandle theHandle = theEffect->mHandle;
            std::map<unsigned long, EEEffect*>::iterator it = this->mEffectsByHandle.find(theHandle.mValue);
            if (it != this->mEffectsByHandle.end())
                this->mEffectsByHandle.erase(it);
            this->RemoveEffect(theEffect);
            theEffect->Release();
        }
        theEffect = theNext;
    }
}

// FUNCTION: EXE 0x5074c9
// Tells every effect that the terrain height changed in a tile rectangle
void EEEffectManager::ElevationChanged(long inLeft, long inTop, long inRight, long inBottom)
{
    EEEffect* theEffect = this->mEffects;
    while (theEffect) {
        EEEffect* theNext = theEffect->mNext;
        theEffect->ElevationChanged(inLeft, inTop, inRight, inBottom);
        theEffect = theNext;
    }
}

// FUNCTION: EXE 0x5074f3
// Destroys a handle index (map<unsigned long, EEEffect*>'s destructor)

// FUNCTION: EXE 0x50751e
// Empties a handle index (map<unsigned long, EEEffect*>::clear)

// FUNCTION: EXE 0x507532
// Erases a range of a handle index (map<unsigned long, EEEffect*>::erase(first, last))

// FUNCTION: EXE 0x5075a3
// Frees a subtree of a handle index (map<unsigned long, EEEffect*>'s _Erase)

// FUNCTION: EXE 0x5075d5
// Steps a handle-index iterator back (map<unsigned long, EEEffect*>::const_iterator::_Dec)
