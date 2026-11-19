// C:\EEX\Empire Earth\Graphics Thread\EEEffectManager.h (RECONSTRUCTION; file name inferred). The graphics
// thread's effects (vtable 0x4411c0; EEEffectManager.cpp 0x1054ec..0x107616 and EEGraphicalGoalManager.cpp
// 0x107616..0x10972b): since the graphics + LLE cluster's second round a Rust object (ee_gfx ffi_manager.rs: the
// effects by handle, the list, the destruction queue, the goal markers, the entry points' plans and registrations,
// Load, Save, RepairPointers, Update and its TEST ONLY captures). This class is its shell: an FSRestorable for the
// saves, and the effects' one entry point for the game's other code (the Create methods build the entry's
// arguments; ee_gfx effect_ctor.rs's constructors make the effect). The game holds one through `gEffectManager`.
#ifndef EE_EEEFFECTMANAGER_H
#define EE_EEEFFECTMANAGER_H

#include <vector>
#include "lle/FSRestorable.h"
#include "ee/EEEffect.h"

class EEGraphicEffect;
class EEUnit;
class GE3DLine;
class GETransformation;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

class EEEffectManager : public FSRestorable {
public:
    EEEffectManager() : mRust(ee_gfx_manager_new()) {}
    EEEffectManager(const EEEffectManager&) = delete;
    EEEffectManager& operator=(const EEEffectManager&) = delete;
    virtual ~EEEffectManager() { ee_gfx_manager_free(this->mRust); }   // every effect released, then the markers
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
    {
        EeGfxEffectFile theFile = EEEffectFile(inFile);
        return ee_gfx_manager_load(this->mRust, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
    }
    virtual long RepairPointers(unsigned long, unsigned long) { return ee_gfx_manager_repair(this->mRust); }
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
    {
        EeGfxEffectFile theFile = EEEffectFile(outFile);
        return ee_gfx_manager_save(this->mRust, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
    }
    // a unit swap (EEPhysics): every effect on inOld moves to inNew
    void ReplaceUnit(EEUnit* inOld, EEUnit* inNew) { ee_gfx_manager_replace_unit(this->mRust, U(inOld), U(inNew)); }
    void Update() { ee_gfx_manager_update(this->mRust); }        // EEPhysics::Tick, every tick
    void ClearEffects() { ee_gfx_manager_clear(this->mRust); }
    // the terrain changed in the tile area (EESEAPaintElevation): the farms rebuilt
    void ElevationChanged(long inLeft, long inTop, long inRight, long inBottom)
        { ee_gfx_manager_elevation_changed(this->mRust, inLeft, inTop, inRight, inBottom); }
    EEEffect* GetEffect(EEEffectHandle inHandle)   // 0 if none
        { return reinterpret_cast<EEEffect*>(ee_gfx_manager_find(this->mRust, inHandle.mValue)); }
    long DestroyEffect(EEEffectHandle inHandle) { ee_gfx_manager_destroy(this->mRust, inHandle.mValue); return 0; }
    unsigned long Count() const { return ee_gfx_manager_count(this->mRust, false); }
    unsigned long DestroyedCount() const { return ee_gfx_manager_count(this->mRust, true); }

    // The entry points (each returns 10 for a kind it does not make or one without the living units it needs, else
    // the set-up's result; outHandle, when given, gets the new effect's handle): on a unit (inKeepsUnit: the light
    // glow's; inTagPoint: the particle emitter's; inGoalIndex: the command confirmation's goal queue)
    long CreateUnitEffect(EEUnit* inUnit, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, bool inKeepsUnit,
                          long inTagPoint, long inGoalIndex)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::Unit, inEffect);
        a.unit = U(inUnit);
        a.keeps_unit = inKeepsUnit;
        a.tag_point = (int32_t)inTagPoint;
        a.goal_index = (uint32_t)inGoalIndex;
        return this->Create(a, outHandle);
    }
    // from a unit to a unit (muzzle blast, winter breath, tracers, shells, projectile, tracer beam)
    long CreateEffectBetweenUnits(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::BetweenUnits, inEffect);
        a.unit = U(inFrom);
        a.to_unit = U(inTo);
        return this->Create(a, outHandle);
    }
    // from a unit to a point (muzzle blast, projectile, citizen beam, tracer beam, confirmation, pulse cannon ball)
    long CreateEffect(EEUnit* inUnit, const GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle,
                      long inGoalIndex)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::UnitToPoint, inEffect);
        a.unit = U(inUnit);
        a.at = P(inLocation);
        a.goal_index = (uint32_t)inGoalIndex;
        return this->Create(a, outHandle);
    }
    long CreatePlacedEffect(const GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::Placed, inEffect);
        a.at = P(inLocation);
        return this->Create(a, outHandle);
    }
    // the water ripple and the battlefield decal at a transformation
    long CreateTransformationEffect(const GETransformation& inTransformation, EEGraphicEffect* inEffect,
                                    EEEffectHandle* outHandle)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::Transformation, inEffect);
        a.transformation = *reinterpret_cast<const EeGeTransformation*>(&inTransformation);
        return this->Create(a, outHandle);
    }
    // a particle, particle emitter or multi-effect along a line
    // an earthquake, 3D projectile or marching line between two points (other kinds: nothing, 0)
    long CreateEffectBetween(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inEffect,
                             EEEffectHandle* outHandle)
    {
        EeGfxEffectArgs a = Args(EeGfxEffectEntry::Between, inEffect);
        a.at = P(inFrom);
        a.to = P(inTo);
        return this->Create(a, outHandle);
    }
    // rain or snow over a set of tiles (other kinds: nothing, 0; EEEffectHost.cpp)
    long CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                             EEEffectHandle* outHandle);

    EeGfxEffectManager* mRust;

private:
    static uint64_t U(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
    static EeGe3DPoint P(const GE3DPoint& inPoint) { return EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ}; }
    static EeGfxEffectArgs Args(EeGfxEffectEntry inEntry, EEGraphicEffect* inEffect)
    {
        EeGfxEffectArgs a = {};
        a.entry = inEntry;
        a.graphic_effect = U(inEffect);
        return a;
    }
    long Create(const EeGfxEffectArgs& inArgs, EEEffectHandle* outHandle)
        { return ee_gfx_manager_create(this->mRust, &inArgs, outHandle != NULL ? &outHandle->mValue : NULL); }
};

extern EEEffectManager*& gEffectManager;

// TEST ONLY (EE-SIM --effect-sweep; tools/sim_capture.py --profile effects): every swept effect kind made in the
// game's world at its end, stepped, saved and loaded (ee_gfx effect_sweep.rs)
void EEEffectCaptureSweep();

#endif
