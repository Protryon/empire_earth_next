// EEPhysicsModel.h (RECONSTRUCTION; file name inferred). A unit class's physics model (RTTI EEPhysicsModel, vtable
// 0x442848; the twelve EEPM* classes derived from it, empty singletons: gPhysicsModels[] (0x531d58..0x531d84) held one
// `new` of each, EEUnitClass +0x110 points at one). The code was Physics Thread/EEPhysicsModel.cpp and EEPM*.cpp,
// range 0x5543b7..0x56066d.
//
// New (the physics models' lane, round 1): the models are whole in Rust (ee_units pm.rs, pm_ground.rs, pm_air.rs:
// every body and each kind's virtual dispatch, as the vtables chose them). The singletons stayed: one EEPhysicsModel
// per kind, its index (ee_units EE_UNITS_PM_*: 0 land .. 11 spaceship) the only member. Rust calls back into the C++
// through EEPhysicsModel.cpp's ee_units_pm_host_* functions (the unit's location, path, goals, the map, the effects).
#ifndef EE_EEPHYSICSMODEL_H
#define EE_EEPHYSICSMODEL_H

class EEComplexUnit;
class GE3DPoint;

class EEPhysicsModel {
public:
    explicit EEPhysicsModel(long inKind) : mKind(inKind) {}

    // 0x156897: the step of the unit's physics action (0..5), then the after-step work (the collisions, the push)
    void Step(EEComplexUnit* inUnit);
    // the action-0 step alone (EE-SIM's missile sweep flies a missile with it)
    void Step0(EEComplexUnit* inUnit);
    // +0x60 of the airplane models: the displayed bank for the turn rate (the airplane classes' death roll)
    float ComputeRoll(EEComplexUnit* inUnit, float inValue);
    // an EEPMAirplane (the airplane or the air animal model: the classes' dynamic_casts)
    bool IsAirplane() const { return this->mKind == 1 || this->mKind == 10; }

    long mKind;                             // the gPhysicsModels[] index
};

// 0x4de374 (cdecl; the crashing airplane's class +0xcc effect at inLocation) (provisional name; argument types
// inferred)
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation);
// 0x63f2c0 (cdecl; clamps ioLocation's z between the unit class's -mCruiseAltitude and the ground; the submarine
// model's dive depth) (provisional name)
void EEAdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation);
void EEInitPhysicsModels();     // 0x1543b7 (creates the 12 singletons gPhysicsModels[], reads world table entry 79)
void EEFreePhysicsModels();

#endif
