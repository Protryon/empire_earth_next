// C:\EEX\Empire Earth\Physics Thread\EEPMProjectile.cpp (file name inferred: no __FILE__; RTTI EEPMProjectile; one unit per
// physics-model class, alphabetical link order). Range 0x15d015..0x15d44f; split out of a single EEPMLand.cpp
// (each unit ends with its own inline-COMDAT cluster).
#include <windows.h>
#include <vector>
#include <math.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EECombatFormulas.h"

// NEAR: frame/float slot order; the original calls vector<long>::operator[] out of line (0x11e2a5) for the first lookup
// FUNCTION: EXE 0x55d015
// Places the launched projectile on its ballistic arc from the launch point for its flight time so far,
// nose along the arc; a meteor flies straight at its top speed, without gravity (new: the arc and the placement are
// ee_combat's arc and projectile_launch)
// Fields: start point (+0x10c), yaw/pitch (+0x188/+0x18c), the launcher class's launch speed (else its own) for
// +0x290 ms of flight under the class's gravity (+0x13c); a type 0x325 flies straight at its speed
void EEPMProjectile::Step0(EEComplexUnit* inUnit)
{
    long theTileX;
    long theTileY;
    inUnit->mInitialContactLocation = inUnit->mPhysicalModel.GetLocation().GetPosition();
    EeCombatArcIn theIn = {};
    theIn.flight_time_ms = (int32_t)inUnit->mFlightTime;
    theIn.gravity = inUnit->mUnitClass->mAcceleration;
    theIn.pitch = inUnit->mOrientation.mPitch;
    theIn.meteor = inUnit->mUnitClass->mUnitType == kUnitTypeMeteor;
    if (theIn.meteor) {
        theIn.max_velocity = inUnit->GetMaxVelocity();
    } else {
        theIn.launcher_speed = inUnit->mPlayer->mUnitClassUpgrades[inUnit->mLauncherClass->mUnitType]->mLaunchSpeed;
        if (!(theIn.launcher_speed > 0.001f))   // read only for the fallback
            theIn.own_speed = inUnit->mPlayer->mUnitClassUpgrades[inUnit->mUnitClass->mUnitType]->mLaunchSpeed;
    }
    EeCombatArcOut theArc = EECombatArc(theIn);
    // the placement on the arc is ee_combat's projectile_launch (flight.rs)
    EeCombatFlightIn in = {};
    in.start[0] = inUnit->mProjectileStart.GetX();
    in.start[1] = inUnit->mProjectileStart.GetY();
    in.start[2] = inUnit->mProjectileStart.GetZ();
    in.yaw = inUnit->mOrientation.mYaw;
    in.move_y = theArc.move_y;
    in.move_z = theArc.move_z;
    in.pitch = theArc.pitch;
    EeCombatFlightOut theOut = ee_combat_projectile_launch(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'J'), in, theOut);
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// NEAR: frame layout (the original keeps the step in the dead inUnit slot; 0x94 vs our 0xa0)
// FUNCTION: EXE 0x55d259
// Moves the projectile one tick along its heading and pulls its nose down by its class's gravity; only
// while it is at or below height 0 and above the ground point (new: ee_combat's projectile_step)
// Class gravity +0x13c; the height-0 test is the original's (bytes 0x55d2e4: `fcomp 0.0`, `ja` returns)
void EEPMProjectile::Step2(EEComplexUnit* inUnit)
{
    // the tick is ee_combat's projectile_step (flight.rs); the ground point is read only at or below height 0
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    long theTileX;
    long theTileY;
    EeCombatFlightIn in = {};
    memcpy(in.location, theLocation.mMatrix, sizeof(in.location));
    in.gravity = inUnit->mUnitClass->mAcceleration;
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->mVelocity;
    if (!(theLocation.GetZ() > 0.0f)) {
        GE3DPoint thePosition = theLocation.GetPosition();
        GE3DPoint theGround;
        ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
        in.ground_read = 1;
        in.ground_z = theGround.GetZ();
    }
    EeCombatFlightOut theOut = ee_combat_projectile_step(&in);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'S'), in, theOut);
    if (!theOut.moved)
        return;
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}
