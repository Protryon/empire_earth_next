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
#include "ee/EEE2DParticle.h"
#include "ee/EEE2DPulseWave.h"
#include "ee/EEE2DSmokePlume.h"
#include "ee/EEE2DSmokeMuzzleBlast.h"
#include "ee/EEE2DWinterBreath.h"
#include "ee/EEE2DLinearProjectile.h"
#include "ee/EEE2DWaterRipple.h"
#include "ee/EEE2DVortex.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "ee/EEE3DIcon.h"
#include "ee/EEE3DNuclearExplosion.h"
#include "ee/EEE3DAdditivePass.h"
#include "ee/EEE3DHurricane.h"
#include "ee/EEE3DDisease.h"
#include "ee/EEE3DSmokeGenerator.h"
#include "ee/EEE3DFarm.h"
#include "ee/EEE3DEnergyWall.h"
#include "ee/EEE3DDamageVent.h"
#include "ee/EEE3DParticleEmitter.h"
#include "ee/EEE3DMultiEffect.h"
#include "ee/EEEDamagedEffect.h"
#include "ee/EEE3DRust.h"
#include "ee/EEE3DCommandConfirmation.h"
#include "ee/EEE3DVirus.h"
#include "ee/EEE3DScreenFlash.h"
#include "ee/EEE3DEnergyWave.h"
#include "ee/EEE3DTracerBeam.h"
#include "ee/EEE3DLinearProjectile.h"
#include "ee/EEE3DEarthquake.h"
#include "ee/EEE3DMarchingLine.h"
#include "ee/EEE3DVolcano.h"
#include "ee/EEE3DWaterSplash.h"
#include "ee/EEE3DGoal.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEE3DSnow.h"
#include "ee/EEE3DRain.h"
#include "ee/EEE3DParticle.h"
#include "ee/EESimCapture.h"
#include "ee/EESaveGameFiles.h"
#include "lle/FSMemoryFile.h"


// SYNTHETIC: EXE 0x5054ec
// SYNTHETIC: EXE 0x5054f6
// SYNTHETIC: EXE 0x505507
// SYNTHETIC: EXE 0x505513
// the guarded atexit dtor says a class static
const UString EEEffectManager::sData("EEEffectManager-Data");

// The manager's containers (the last handle given out, 0x4e8e48 in the original, saved first; the effects by handle;
// the effect list, newest first, which Update steps in its order; the destruction queue) are ee_gfx effect_store.rs's,
// its algorithms (which effect an entry point creates, the registration, AddEffect 0x1055d0, RemoveEffect 0x1072d0,
// DestroyQueued 0x1067ce, ClearEffects, RepairPointers', Load's, Save's and Update's walks) effect_manager.rs's
// (RUST_PLAN R4.4): the effects are opaque pointers there, and the C++ below constructs them and answers its calls
static EEEffect* EEStoreEffect(void* inEffect)
{
    return static_cast<EEEffect*>(inEffect);
}

// The effects and goal markers as effect_manager.rs calls them (the context: the manager, the save's versions)
struct EEEffectManagerOps {
    EEEffectManager* mManager;
    unsigned long mMajor;
    unsigned long mMinor;
    unsigned long mFirst;
    unsigned long mSecond;

    static EEEffectManagerOps& Of(void* inContext) { return *static_cast<EEEffectManagerOps*>(inContext); }
    static FSFile& File(const EeGfxEffectFile* inFile) { return *static_cast<FSFile*>(inFile->context); }
    static EeGfxEffectRecord* Record(void*, void* inEffect) { return EEStoreEffect(inEffect)->mRecord; }
    static void Reference(void*, void* inEffect) { EEStoreEffect(inEffect)->Reference(); }
    static void Release(void*, void* inEffect) { EEStoreEffect(inEffect)->Release(); }
    static int32_t SetUp(void*, void* inEffect) { return (int32_t)EEStoreEffect(inEffect)->SetUp(); }
    static int32_t Step(void*, void* inEffect) { return (int32_t)EEStoreEffect(inEffect)->Step(); }
    static int32_t Repair(void*, void* inEffect) { return (int32_t)EEStoreEffect(inEffect)->RepairPointers(); }
    static bool AboveDetail(void*, void* inEffect)
    {
        return (unsigned long)EEStoreEffect(inEffect)->mGraphicEffect->mDetailLevel > gEffectDetail;
    }
    static void* NewLoaded(void*, uint32_t inKind);
    static int32_t Load(void* inContext, void* inEffect, const EeGfxEffectFile* inFile)
    {
        return (int32_t)EEStoreEffect(inEffect)->Load(Of(inContext).mMajor, Of(inContext).mMinor, File(inFile));
    }
    static int32_t Save(void* inContext, void* inEffect, const EeGfxEffectFile* inFile)
    {
        return (int32_t)EEStoreEffect(inEffect)->Save(Of(inContext).mMajor, Of(inContext).mMinor, File(inFile));
    }
    static void GoalsUpdate(void* inContext) { Of(inContext).mManager->mGoals.Update(); }
    static int32_t GoalsLoad(void* inContext, const EeGfxEffectFile* inFile)
    {
        return (int32_t)Of(inContext).mManager->mGoals.Load(Of(inContext).mMajor, Of(inContext).mMinor, File(inFile));
    }
    static int32_t GoalsRepair(void* inContext)
    {
        return (int32_t)Of(inContext).mManager->mGoals.RepairPointers(Of(inContext).mFirst, Of(inContext).mSecond);
    }

    EeGfxEffectManagerHost Host()
    {
        EeGfxEffectManagerHost theHost = {this,      Record,   Reference, Release,     SetUp, Step,     Repair,
                                          AboveDetail, NewLoaded, Load,   Save, GoalsUpdate, GoalsLoad, GoalsRepair};
        return theHost;
    }
};

// The load-time effect of a saved kind (the original's Load switch; null for a kind without one: 0x11 and unknown
// kinds read nothing more)
void* EEEffectManagerOps::NewLoaded(void*, uint32_t inKind)
{
    EEEffect* theEffect = NULL;
    switch (inKind) {
    case kEffectDamaged: theEffect = new EEEDamagedEffect(); break;
    case kEffect3DParticle: theEffect = EEE3DParticle::Create(); break;
    case kEffect3DParticleEmitter: theEffect = new EEE3DParticleEmitter(0x3a); break;
    case kEffect2DParticle: theEffect = EEE2DParticle::Create(); break;
    case kEffect3DDestructionSmoke: theEffect = new EEE3DSmokeGenerator(0x1e); break;
    case kEffect2DVelocityParticle: theEffect = new EEE2DSmokePlume(EE_GFX_EFFECT_KIND_VELOCITY_PARTICLE); break;
    case kEffect3DWaterSplash: theEffect = new EEE3DWaterSplash(); break;
    case kEffect3DDamageSmoke: theEffect = new EEE3DSmokeGenerator(0x2c); break;
    case kEffect3DSmokeGenerator: theEffect = new EEE3DSmokeGenerator(0x2b); break;
    case kEffect2DExplosion: theEffect = new EEE2DExplosion(); break;
    case kEffect2DSmokePlume: theEffect = new EEE2DSmokePlume(0xb); break;
    case kEffect2DShockwave: theEffect = new EEE2DShockwave(); break;
    case kEffect2DFire: theEffect = new EEE2DFire(); break;
    case kEffect2DMuzzleFlash: theEffect = new EEE2DMuzzleFlash(); break;
    case kEffect2DImpact: theEffect = new EEE2DImpact(6); break;
    case kEffect2DImpactBlood: theEffect = new EEE2DImpactBlood(); break;
    case kEffect2DExhaustTrail: theEffect = new EEE2DExhaustTrail(); break;
    case kEffect2DPlasmaPulse: theEffect = new EEE2DPlasmaPulse(); break;
    case kEffect2DLightGlow: theEffect = new EEE2DLightGlow(); break;
    case kEffect2DLightning: theEffect = new EEE2DLightning(); break;
    case kEffect2DFireTrail: theEffect = new EEE2DFireTrail(); break;
    case kEffect2DSquareMarchingIcons: theEffect = new EEE2DMarchingIcons(EE_GFX_EFFECT_KIND_SQUARE_ICONS); break;
    case kEffect2DCircularMarchingIcons: theEffect = new EEE2DMarchingIcons(EE_GFX_EFFECT_KIND_CIRCULAR_ICONS); break;
    case kEffect2DPulseWave: theEffect = new EEE2DPulseWave(); break;
    case kEffect2DSmokeMuzzleBlast: theEffect = new EEE2DSmokeMuzzleBlast(); break;
    case kEffect2DWinterBreath: theEffect = new EEE2DWinterBreath(); break;
    case kEffect2DLinearProjectile: theEffect = new EEE2DLinearProjectile(); break;
    case kEffect2DWaterRipple: theEffect = new EEE2DWaterRipple(); break;
    case kEffect3DEarthquake: theEffect = new EEE3DEarthquake(); break;
    case kEffect3DTracers: theEffect = new EEE3DRust(0x21); break;
    case kEffect3DVolcano: theEffect = new EEE3DVolcano(); break;
    case kEffect3DDisease: theEffect = new EEE3DDisease(); break;
    case kEffect3DFarm: theEffect = new EEE3DFarm(); break;
    case kEffect3DHurricane: theEffect = new EEE3DHurricane(); break;
    case kEffect3DLinearProjectile: theEffect = new EEE3DLinearProjectile(); break;
    case kEffect3DRain: theEffect = new EEE3DRain(); break;
    case kEffect3DSnow: theEffect = new EEE3DSnow(); break;
    case kEffect3DShells: theEffect = new EEE3DRust(0x26); break;
    case kEffect3DAdditivePass: theEffect = new EEE3DAdditivePass(); break;
    case kEffect3DNuclearExplosion: theEffect = new EEE3DNuclearExplosion(); break;
    case kEffect3DIcon: theEffect = new EEE3DIcon(2); break;
    case kEffect3DMarchingLine: theEffect = new EEE3DMarchingLine(); break;
    case kEffect3DGoal: theEffect = new EEE3DGoal(); break;
    case kEffect3DTeleport: theEffect = new EEE3DRust(0x3f); break;
    case kEffect3DTimeWarp: theEffect = new EEE3DRust(0x44); break;
    case kEffect3DTracerBeam: theEffect = new EEE3DTracerBeam(0x40); break;
    case kEffect3DPulseCannonBall: theEffect = new EEE3DRust(0x43); break;
    case kEffect3DShield: theEffect = new EEE3DRust(kEffect3DShield); break;
    case kEffect2DVortex: theEffect = new EEE2DVortex(); break;
    case kEffect3DBlastWave: theEffect = new EEE3DRust(0x18); break;
    case kEffect3DVirus: theEffect = new EEE3DVirus(); break;
    case kEffect2DBattlefieldAmbient: theEffect = new EEE2DBattlefieldAmbient(); break;
    case kEffect3DCitizenBeam: theEffect = new EEE3DRust(0x33); break;
    case kEffect3DEnergyWall: theEffect = new EEE3DEnergyWall(0x34); break;
    case kEffect3DDamageVent: theEffect = new EEE3DDamageVent(); break;
    case kEffect3DScreenFlash: theEffect = new EEE3DScreenFlash(); break;
    case kEffect3DEnergyWave: theEffect = new EEE3DEnergyWave(); break;
    default: break;
    }
    return theEffect;
}

// effect_manager.rs's choice for an entry point and a graphic effect (the detail test, the living units its kind
// needs, the result codes, the multi-effect unsaved)
static EeGfxEffectPlan EEPlanEffect(EeGfxEffectEntry inEntry, EEGraphicEffect* inEffect, EEUnit* inFrom, EEUnit* inTo)
{
    return ee_gfx_effects_plan(inEntry, (int32_t)inEffect->mKind, (unsigned long)inEffect->mDetailLevel <= gEffectDetail,
                               inFrom && inFrom->mComplexUnit, inTo && inTo->mComplexUnit);
}

// The registration of an entry point's new effect (null: none, 0): unsaved when planned so, set up, added under a new
// handle (even when the set-up fails), its handle to outHandle; SetUp's result
static long EERegisterEffect(EEEffectManager* inManager, EEEffect* inEffect, const EeGfxEffectPlan& inPlan,
                             EEEffectHandle* outHandle)
{
    EEEffectManagerOps theOps = {inManager, 0, 0, 0, 0};
    EeGfxEffectManagerHost theHost = theOps.Host();
    return ee_gfx_effects_create(inManager->mStore, &theHost, inEffect, inPlan.saved,
                                 outHandle ? &outHandle->mValue : NULL);
}

// FUNCTION: EXE 0x50552f
// Creates an effect manager with no effects and no queued destructions
EEEffectManager::EEEffectManager()
{
    this->mStore = ee_gfx_effects_new();
}

// SYNTHETIC: EXE 0x50558c
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5055a8
// Destroys the manager, releasing every effect
EEEffectManager::~EEEffectManager()
{
    this->ClearEffects();
    ee_gfx_effects_free(this->mStore);
    this->mStore = NULL;
}

// FUNCTION: EXE 0x50562c
// Creates, sets up and registers the effect of the graphic effect's kind on a unit (kinds that need a living unit
// return 10 without one); returns 10 for other kinds, else SetUp's result (added even when SetUp fails)
long EEEffectManager::CreateUnitEffect(EEUnit* inUnit, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, bool inKeepsUnit,
                                       long inTagPoint, long inGoalIndex)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Unit, inEffect, inUnit, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case 4: theEffect = new EEE2DExplosion(inUnit, inEffect); break;
    case 5: theEffect = new EEE2DFire(inUnit, inEffect); break;
    case 9: theEffect = new EEE2DMuzzleFlash(inUnit, inEffect); break;
    case 6: theEffect = new EEE2DImpact(inUnit, inEffect, 6); break;
    case 7: theEffect = new EEE2DImpactBlood(inUnit, inEffect); break;
    case 18: theEffect = new EEE2DShockwave(inUnit, inEffect); break;
    case 3: theEffect = new EEE2DExhaustTrail(inUnit, inEffect); break;
    case 14: theEffect = new EEE2DFireTrail(inUnit, inEffect); break;
    case 10: theEffect = new EEE2DPlasmaPulse(inUnit, inEffect); break;
    case 13: theEffect = new EEE2DLightGlow(inUnit, inEffect, inKeepsUnit); break;
    case 8: theEffect = new EEE2DLightning(inUnit, inEffect); break;
    case 16: theEffect = new EEE2DMarchingIcons(inUnit, inEffect, EE_GFX_EFFECT_KIND_CIRCULAR_ICONS); break;
    case 15: theEffect = new EEE2DMarchingIcons(inUnit, inEffect, EE_GFX_EFFECT_KIND_SQUARE_ICONS); break;
    case 2: theEffect = new EEE3DIcon(inUnit, inEffect, 0, 2); break;
    case 0x18: theEffect = new EEE3DRust(inUnit, inEffect, 0x18); break;
    case 0x28: theEffect = new EEE3DNuclearExplosion(inUnit, inEffect); break;
    case 0x27: theEffect = new EEE3DAdditivePass(inUnit, inEffect); break;
    case 0x24: theEffect = new EEE3DHurricane(inUnit, inEffect); break;
    case 0x23: theEffect = new EEE3DDisease(inUnit, inEffect); break;
    case 0x1e: theEffect = EEE3DSmokeGenerator::CreateDestructionSmoke(inUnit, inEffect); break;
    case 0x29: theEffect = new EEE3DFarm(inUnit, inEffect); break;
    case 0x34: theEffect = new EEE3DEnergyWall(inUnit, inEffect, 0x34); break;
    case 0x2f: theEffect = new EEE2DPulseWave(inUnit, inEffect); break;
    case 0x2e: theEffect = EEE2DParticle::Create(inUnit, inEffect); break;
    case 0x2c: theEffect = new EEE3DSmokeGenerator(inUnit, inEffect, 0x2c); break;
    case 0x2b: theEffect = new EEE3DSmokeGenerator(inUnit, inEffect, 0x2b); break;
    case 0x35: theEffect = new EEE3DDamageVent(inUnit, inEffect); break;
    case 0x41: theEffect = new EEE3DCommandConfirmation(inUnit, inEffect, inGoalIndex, 0x41); break;
    case 0x3f: theEffect = new EEE3DRust(inUnit, inEffect, 0x3f); break;
    case 0x3e: theEffect = new EEEDamagedEffect(inUnit, inEffect); break;
    case 0x3b: theEffect = new EEE3DMultiEffect(inUnit, inEffect, 0x3b); break;
    case 0x3a: theEffect = new EEE3DParticleEmitter(inUnit, inEffect, 0x3a, inTagPoint); break;
    case 0x42: theEffect = new EEE3DVirus(inUnit, inEffect); break;
    case 0x47: theEffect = new EEE3DEnergyWave(inUnit, inEffect); break;
    case 0x46: theEffect = new EEE3DScreenFlash(inUnit, inEffect); break;
    case 0x45: theEffect = new EEE3DRust(inUnit, inEffect, kEffect3DShield); break;
    case 0x44: theEffect = new EEE3DRust(inUnit, inEffect, 0x44); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// SYNTHETIC: EXE 0x505dff
// (EEE2DImpactBlood's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x505e21
// Creates, sets up and registers a two-unit effect (muzzle blast, winter breath, tracers, shells, projectile,
// tracer beam); returns 10 for other kinds or missing units, else SetUp's result
long EEEffectManager::CreateEffectBetweenUnits(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inEffect,
                                               EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::BetweenUnits, inEffect, inFrom, inTo);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect2DSmokeMuzzleBlast: theEffect = new EEE2DSmokeMuzzleBlast(inFrom, inTo, inEffect); break;
    case kEffect2DWinterBreath: theEffect = new EEE2DWinterBreath(inFrom, inEffect); break;
    case kEffect3DTracers: theEffect = new EEE3DRust(inFrom, inTo, inEffect, 0x21); break;
    case kEffect3DShells: theEffect = new EEE3DRust(inFrom, inTo, inEffect, 0x26); break;
    case kEffect2DLinearProjectile: theEffect = new EEE2DLinearProjectile(inFrom, inTo, inEffect); break;
    case kEffect3DTracerBeam: theEffect = new EEE3DTracerBeam(inFrom, inTo, inEffect, 0x40); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// SYNTHETIC: EXE 0x506016
// (EEE3DShells's scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x506038
// Creates, sets up and registers a unit-to-point effect (muzzle blast, projectile, citizen beam, tracer beam,
// command confirmation, pulse cannon ball); returns 10 for other kinds or a missing unit, else SetUp's result
long EEEffectManager::CreateEffect(EEUnit* inUnit, GE3DPoint& inLocation, EEGraphicEffect* inEffect,
                                   EEEffectHandle* outHandle, long inUnknown)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::UnitToPoint, inEffect, inUnit, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect2DSmokeMuzzleBlast: theEffect = new EEE2DSmokeMuzzleBlast(inUnit, inLocation, inEffect); break;
    case kEffect2DLinearProjectile: theEffect = new EEE2DLinearProjectile(inUnit, inLocation, inEffect); break;
    case kEffect3DCitizenBeam: theEffect = new EEE3DRust(inUnit, inLocation, inEffect, 0x33); break;
    case kEffect3DTracerBeam: theEffect = new EEE3DTracerBeam(inUnit, inLocation, inEffect, 0x40); break;
    case kEffect3DCommandConfirmation:
        theEffect = new EEE3DCommandConfirmation(inUnit, inLocation, inEffect, inUnknown, 0x41);
        break;
    case kEffect3DPulseCannonBall: theEffect = new EEE3DRust(inUnit, inLocation, inEffect, 0x43); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x5061e9
// Creates, sets up and registers an effect of the graphic effect's kind at a map point; returns 10 for kinds
// without a point version, else SetUp's result
long EEEffectManager::CreatePlacedEffect(GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Placed, inEffect, NULL, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case 2: theEffect = new EEE3DIcon(0, inEffect, &inLocation, 2); break;
    case 6: theEffect = new EEE2DImpact(inLocation, inEffect, 6); break;
    case 8: theEffect = new EEE2DLightning(inLocation, inEffect); break;
    case 0xb: theEffect = new EEE2DSmokePlume(inLocation, inEffect, 0xb); break;
    case 0x12: theEffect = new EEE2DShockwave(inLocation, inEffect); break;
    case 0x18: theEffect = new EEE3DRust(inLocation, inEffect, 0x18); break;
    case 0x1b: theEffect = new EEE3DWaterSplash(inLocation, inEffect); break;
    case 0x20: theEffect = new EEE3DVolcano(inLocation, inEffect); break;
    case 0x22: theEffect = new EEE2DWaterRipple(inLocation, inEffect); break;
    case 0x2b: theEffect = new EEE3DSmokeGenerator(inLocation, inEffect, 0x2b); break;
    case 0x2e: theEffect = EEE2DParticle::Create(inLocation, inEffect); break;
    case 0x31: theEffect = new EEE2DVortex(inLocation, inEffect); break;
    case 0x32: theEffect = new EEE2DBattlefieldAmbient(inLocation, inEffect); break;
    case 0x39: theEffect = EEE3DParticle::Create(inLocation, inEffect); break;
    case 0x3a: theEffect = new EEE3DParticleEmitter(inLocation, inEffect, 0x3a); break;
    case 0x3b: theEffect = new EEE3DMultiEffect(inLocation, inEffect, 0x3b); break;
    case 0x3c: theEffect = new EEE3DGoal(0, inEffect, &inLocation); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x5064e6
// Creates, sets up and registers a water ripple or battlefield decal at a transformation; returns 10 for other
// kinds, else SetUp's result
long EEEffectManager::CreateTransformationEffect(const GETransformation& inTransformation, EEGraphicEffect* inEffect,
                                                 EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Transformation, inEffect, NULL, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect2DWaterRipple: theEffect = new EEE2DWaterRipple(inTransformation, inEffect); break;
    case kEffect2DBattlefieldAmbient: theEffect = new EEE2DBattlefieldAmbient(inTransformation, inEffect); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x506596
// Creates, sets up and registers a particle, particle emitter or multi-effect along a line; returns 10 for other
// kinds, else SetUp's result
long EEEffectManager::CreateLineEffect(GE3DLine& inLine, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Line, inEffect, NULL, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect2DVelocityParticle: theEffect = new EEE2DSmokePlume(inLine, inEffect); break;
    case kEffect2DParticle: theEffect = EEE2DParticle::Create(inLine, inEffect); break;
    case kEffect3DParticle: theEffect = EEE3DParticle::Create(inLine, inEffect); break;
    case kEffect3DParticleEmitter: theEffect = new EEE3DParticleEmitter(inLine, inEffect, 0x3a); break;
    case kEffect3DMultiEffect: theEffect = new EEE3DMultiEffect(inLine, inEffect, 0x3b); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x50667c
// Creates, sets up and registers an earthquake, 3D projectile or marching line between two points; other kinds
// create nothing and return 0
long EEEffectManager::CreateEffectBetween(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Between, inEffect, NULL, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect3DEarthquake: theEffect = new EEE3DEarthquake(inFrom, inTo, inEffect); break;
    case kEffect3DLinearProjectile: theEffect = new EEE3DLinearProjectile(inFrom, inTo, inEffect); break;
    case kEffect3DMarchingLine: theEffect = new EEE3DMarchingLine(inFrom, inTo, inEffect); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x50673e
// Creates, sets up and registers rain or snow over a set of tiles; other kinds create nothing and return 0
long EEEffectManager::CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    EeGfxEffectPlan thePlan = EEPlanEffect(EeGfxEffectEntry::Weather, inEffect, NULL, NULL);
    if (!thePlan.create)
        return thePlan.result;
    EEEffect* theEffect = NULL;
    switch (inEffect->mKind) {
    case kEffect3DRain: theEffect = new EEE3DRain(inTiles, inEffect); break;
    case kEffect3DSnow: theEffect = new EEE3DSnow(inTiles, inEffect); break;
    }
    return EERegisterEffect(this, theEffect, thePlan, outHandle);
}

// FUNCTION: EXE 0x506852
// Returns the effect with the given handle, or null
EEEffect* EEEffectManager::GetEffect(EEEffectHandle inHandle)
{
    if (inHandle == kNoEffectHandle)
        return NULL;
    return EEStoreEffect(ee_gfx_effects_find(this->mStore, inHandle.mValue));
}

// FUNCTION: EXE 0x506889
// Unlinks and releases every effect and empties the handle index
void EEEffectManager::ClearEffects()
{
    EEEffectManagerOps theOps = {this, 0, 0, 0, 0};
    EeGfxEffectManagerHost theHost = theOps.Host();
    ee_gfx_effects_clear(this->mStore, &theHost);
}

// FUNCTION: EXE 0x5068c6
// Queues an effect for destruction at the next update; returns 0
long EEEffectManager::DestroyEffect(EEEffectHandle inHandle)
{
    ee_gfx_effects_queue_destroy(this->mStore, inHandle.mValue);
    return 0;
}

// FUNCTION: EXE 0x5068d8
// After a load, repairs every effect's pointers, dropping effects whose unit is gone or that are above the
// detail setting, then the goal markers'; returns the first other error
long EEEffectManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    EEEffectManagerOps theOps = {this, 0, 0, inFirst, inSecond};
    EeGfxEffectManagerHost theHost = theOps.Host();
    return ee_gfx_effects_repair(this->mStore, &theHost);
}

// FUNCTION: EXE 0x506978
// Reads the handle counter, then each effect by kind (old saves give handles anew when needed), the goal markers
// and the destruction queue; returns the first read error (unknown kinds are not skipped)
long EEEffectManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffectManagerOps theOps = {this, inMajorVersion, inMinorVersion, 0, 0};
    EeGfxEffectManagerHost theHost = theOps.Host();
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return ee_gfx_effects_load(this->mStore, &theHost, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
}

// FUNCTION: EXE 0x507305
// Writes the handle counter, the saved effects (kind, then data), and the destruction queue; an effect's write
// error can be overwritten by the next effect's result
long EEEffectManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffectManagerOps theOps = {this, inMajorVersion, inMinorVersion, 0, 0};
    EeGfxEffectManagerHost theHost = theOps.Host();
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effects_save(this->mStore, &theHost, &theFile);
}

// FUNCTION: EXE 0x507430
// Moves every effect on the old unit to the new one
void EEEffectManager::ReplaceUnit(EEUnit* inOld, EEUnit* inNew)
{
    for (EEEffect* theEffect = this->First(); theEffect; theEffect = this->Next(theEffect))
        theEffect->ReplaceUnit(inOld, inNew);
}

// TEST ONLY (tools/sim_capture.py EFMU): the manager's state after every Update, folded: the last handle, the
// index's, the list's and the destruction queue's sizes and an FNV-1a 64 of the index (handle, the effect's kind and
// own handle, in handle order), the list (handle and kind from the head) and the queue (R4.4, sixth round)
static void EECaptureEffectManager(EEEffectManager* inManager)
{
    if (!EESimCaptureRecord::Active())
        return;
    unsigned long long theHash = 0xcbf29ce484222325ull;
    struct Fold {
        static void Word(unsigned long long& ioHash, unsigned int inValue)
        {
            for (int i = 0; i < 4; i++) {
                ioHash ^= (inValue >> (i * 8)) & 0xff;
                ioHash *= 0x100000001b3ull;
            }
        }
    };
    unsigned int theListCount = 0;
    uint32_t theHandle;
    EEEffect* theEffect;
    for (uint64_t c = 0; (theEffect = EEStoreEffect(ee_gfx_effects_at_or_after(inManager->mStore, c, &theHandle)));
         c = (uint64_t)theHandle + 1) {
        Fold::Word(theHash, (unsigned int)theHandle);
        Fold::Word(theHash, (unsigned int)theEffect->mKind);
        Fold::Word(theHash, (unsigned int)theEffect->mHandle.mValue);
    }
    for (theEffect = inManager->First(); theEffect; theEffect = inManager->Next(theEffect)) {
        Fold::Word(theHash, (unsigned int)theEffect->mHandle.mValue);
        Fold::Word(theHash, (unsigned int)theEffect->mKind);
        theListCount++;
    }
    for (unsigned long i = 0; i < inManager->DestroyedCount(); i++)
        Fold::Word(theHash, (unsigned int)ee_gfx_effects_destroyed_at(inManager->mStore, i));
    EESimCaptureRecord theRecord((unsigned int)'E' | ((unsigned int)'F' << 8) | ((unsigned int)'M' << 16) |
                                 ((unsigned int)'U' << 24));
    theRecord.U32((unsigned int)ee_gfx_effects_last_handle(inManager->mStore));
    theRecord.U32((unsigned int)inManager->Count());
    theRecord.U32(theListCount);
    theRecord.U32((unsigned int)inManager->DestroyedCount());
    theRecord.U32((unsigned int)theHash);
    theRecord.U32((unsigned int)(theHash >> 32));
    theRecord.Commit();
}

// TEST ONLY (tools/sim_capture.py EFST): every effect's state after every Update (R4.4, seventh round): the effects'
// random seed, then per effect of the index in handle order its handle, kind, player and the local-only and saved
// flags, and a saved effect's own Save bytes (the base classes' and the leaf's state as a save writes it), folded
// into the record's count, length and an FNV-1a 64
static void EECaptureEffectStates(EEEffectManager* inManager)
{
    static const unsigned int kTag = (unsigned int)'E' | ((unsigned int)'F' << 8) | ((unsigned int)'S' << 16) |
                                     ((unsigned int)'T' << 24);
    if (!EESimCaptureRecord::Active() || !EESimCaptureRecord::Keeps(kTag))
        return;
    FSMemoryFile theFile;
    unsigned int theCount = 0;
    uint32_t theHandle;
    EEEffect* theEffect;
    for (uint64_t c = 0; (theEffect = EEStoreEffect(ee_gfx_effects_at_or_after(inManager->mStore, c, &theHandle)));
         c = (uint64_t)theHandle + 1) {
        theFile.Write((uint32_t)theHandle);
        theFile.Write((int32_t)theEffect->mKind);
        theFile.Write((int32_t)theEffect->mPlayer);
        theFile.Write(theEffect->mIsLocalOnly);
        theFile.Write(theEffect->mIsSaved);
        if (theEffect->mIsSaved)
            theEffect->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
        theCount++;
    }
    unsigned long theLength = 0;
    theFile.GetLength(theLength);
    const unsigned char* theData = theFile.GetData();
    unsigned long long theHash = 0xcbf29ce484222325ull;
    for (unsigned long i = 0; i < theLength; i++) {
        theHash ^= theData[i];
        theHash *= 0x100000001b3ull;
    }
    EESimCaptureRecord theRecord(kTag);
    theRecord.U32((unsigned int)EEEffectRandomSeed());
    theRecord.U32(theCount);
    theRecord.U32((unsigned int)theLength);
    theRecord.U32((unsigned int)theHash);
    theRecord.U32((unsigned int)(theHash >> 32));
    theRecord.Commit();
}

// FUNCTION: EXE 0x507450
// Updates the goal markers, destroys the queued effects, then steps every effect, removing those that finish
// (Step returns 7)
void EEEffectManager::Update()
{
    EEEffectManagerOps theOps = {this, 0, 0, 0, 0};
    EeGfxEffectManagerHost theHost = theOps.Host();
    ee_gfx_effects_update(this->mStore, &theHost);
    EECaptureEffectManager(this);
    EECaptureEffectStates(this);
}

// FUNCTION: EXE 0x5074c9
// Tells every effect that the terrain height changed in a tile rectangle
void EEEffectManager::ElevationChanged(long inLeft, long inTop, long inRight, long inBottom)
{
    EEEffect* theEffect = this->First();
    while (theEffect) {
        EEEffect* theNext = this->Next(theEffect);
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
