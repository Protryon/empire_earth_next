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
// nose along the arc; a meteor flies straight at its top speed, without gravity
// Fields: start point (+0x10c), yaw/pitch (+0x188/+0x18c), the launcher class's launch speed (else its own) for
// +0x290 ms of flight under the class's gravity (+0x13c); a type 0x325 flies straight at its speed
void EEPMProjectile::Step0(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition = inUnit->mPhysicalModel.GetLocation().GetPosition();
    GE3DPoint theStart = inUnit->mProjectileStart;
    GEVector theDirection(0.0f, 0.0f, 0.0f);
    GEVector theMove(0.0f, 0.0f, 0.0f);
    long theTileX;
    long theTileY;
    float theYaw = inUnit->mOrientation.mYaw;
    inUnit->mInitialContactLocation = thePosition;
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
    theDirection.mY = theArc.move_y;
    theDirection.mZ = theArc.move_z;
    GETransformation theTransformation(false);
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theTransformation.SetPosition(0.0f, 0.0f, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theDirection, theMove);
    theStart += theMove;
    theTransformation.SetPosition(theStart.GetX(), theStart.GetY(), theStart.GetZ());
    theTransformation.SetOrientationYPR(theYaw, theArc.pitch, 0.0f);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (the CBPJ capture)
        EECombatFlightRecordIn theCaptureIn = {};
        EECombatFlightRecordOut theCaptureOut = {};
        theCaptureIn.start[0] = inUnit->mProjectileStart.GetX();
        theCaptureIn.start[1] = inUnit->mProjectileStart.GetY();
        theCaptureIn.start[2] = inUnit->mProjectileStart.GetZ();
        theCaptureIn.yaw = theYaw;
        theCaptureIn.move_y = theArc.move_y;
        theCaptureIn.move_z = theArc.move_z;
        theCaptureIn.pitch = theArc.pitch;
        theCaptureOut.moved = 1;
        memcpy(theCaptureOut.matrix, theTransformation.mMatrix, sizeof(theCaptureOut.matrix));
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'J'), theCaptureIn, theCaptureOut);
    }
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// NEAR: frame layout (the original keeps the step in the dead inUnit slot; 0x94 vs our 0xa0)
// FUNCTION: EXE 0x55d259
// Moves the projectile one tick along its heading and pulls its nose down by its class's gravity; only
// while it is at or below height 0 and above the ground point
// Class gravity +0x13c; the height-0 test is the original's (bytes 0x55d2e4: `fcomp 0.0`, `ja` returns)
void EEPMProjectile::Step2(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition = theTransformation.GetPosition();
    GE3DPoint theGround;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GEVector theMove;
    long theTileX;
    long theTileY;
    float theGravity = inUnit->mUnitClass->mAcceleration;
    float theStep = gPhysics->mTickSeconds;
    float theSpeed = inUnit->mVelocity;
    // TEST ONLY (the CBPS capture): the inputs as ee_combat takes them, the new location (the old one when unmoved)
    EECombatFlightRecordIn theCaptureIn = {};
    EECombatFlightRecordOut theCaptureOut = {};
    memcpy(theCaptureIn.location, theTransformation.mMatrix, sizeof(theCaptureIn.location));
    memcpy(theCaptureOut.matrix, theTransformation.mMatrix, sizeof(theCaptureOut.matrix));
    theCaptureIn.gravity = theGravity;
    theCaptureIn.step = theStep;
    theCaptureIn.speed = theSpeed;
    if (thePosition.GetZ() > 0.0f) {
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'S'), theCaptureIn, theCaptureOut);
        return;
    }
    ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
    theCaptureIn.ground_read = 1;
    theCaptureIn.ground_z = theGround.GetZ();
    if (thePosition.GetZ() <= theGround.GetZ()) {
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'S'), theCaptureIn, theCaptureOut);
        return;
    }
    theTransformation.Apply(theForward, theDirection);
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    theForward = GEVector(0.0f, theSpeed / UMCos(thePitch) * 0.1f, 0.0f);
    theTransformation.Apply(theForward, theDirection);
    theMove = GEVector(theDirection.GetX() * theStep, theDirection.GetY() * theStep, theDirection.GetZ() * theStep);
    thePosition += theMove;
    theTransformation.SetPosition(thePosition);
    theDirection.mZ = theDirection.GetZ() - theStep * theGravity * 0.2f;
    theDirection.Normalize();
    float theDelta = UMArcCos(-theDirection.GetZ()) - 1.5707964f - thePitch;
    theTransformation.SetOrientationYPR(theYaw, thePitch + theDelta, 0.0f);
    theCaptureOut.moved = 1;
    memcpy(theCaptureOut.matrix, theTransformation.mMatrix, sizeof(theCaptureOut.matrix));
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'P', 'S'), theCaptureIn, theCaptureOut);
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

