// C:\EEX\Empire Earth\Physics Thread\EEPMMissile.cpp (file name inferred: no __FILE__; RTTI EEPMMissile; one unit per
// physics-model class, alphabetical link order). Range 0x15aff0..0x15b473; split out of a single EEPMLand.cpp
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
#include "lle/GEQuaternion.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EECombatFormulas.h"

// 1/pi (0x4428f4) and kMissileMinimumFactor (0x4428f0, 0.25) are ee_combat's (flight.rs)

// FUNCTION: EXE 0x55aff0
// Records the missile's position as its contact point, then flies it straight on while its boost time
// lasts, else homes it on its target
void EEPMMissile::Step0(EEComplexUnit* inUnit)
{
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    if (inUnit->mFlightTime > 0)
        this->Accelerate(inUnit);
    else
        this->Home(inUnit);
}

// FUNCTION: EXE 0x55b041
// Speeds the missile up along its heading (capped at its top speed) and moves it by one tick (new: ee_combat's
// missile_accelerate)
void EEPMMissile::Accelerate(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EeCombatFlightIn in = {};
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->GetVelocity();
    in.max_speed = theStats->GetMaxVelocity();
    in.accel = theStats->GetAcceleration();
    EeCombatFlightOut theOut = ee_combat_missile_accelerate(&in);
    inUnit->SetVelocity(theOut.speed);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'A'), in, theOut);
    GETransformation theTransformation(false);
    memcpy(theTransformation.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// FUNCTION: EXE 0x55b103
// Turns the missile toward its drop target by at most its turn rate per tick, slows it the further it
// points off target (down to 1/4 of top speed) and moves it by one tick (new: ee_combat's missile_home; the drop
// target stays here)
void EEPMMissile::Home(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEUnit* theDropTarget;
    EEFindDropLocation(inUnit, &theDropTarget, &theTarget, NULL);
    EeCombatFlightIn in = {};
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.target[0] = theTarget.GetX();
    in.target[1] = theTarget.GetY();
    in.target[2] = theTarget.GetZ();
    in.step = gPhysics->mTickSeconds;
    in.speed = inUnit->mVelocity;
    in.max_speed = theStats->GetMaxVelocity();
    in.accel = theStats->GetAcceleration();
    in.max_angular = theStats->GetMaxAngularVelocity();
    EeCombatFlightOut theOut = ee_combat_missile_home(&in);
    inUnit->SetVelocity(theOut.speed);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'H'), in, theOut);
    GETransformation theNew(false);
    memcpy(theNew.mMatrix, theOut.matrix, sizeof(theOut.matrix));
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theNew, inUnit, &theTileX, &theTileY);
}
