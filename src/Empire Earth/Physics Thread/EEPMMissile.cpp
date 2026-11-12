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

// 1/pi is the shared literal __real@4@3ffda2f9830000000000 (0x4428f4; EEPMNaval/EEPMSubmergedNaval Virtual34 use it too)
static const float kMissileMinimumFactor = 0.25f;    // 0x4428f0 (unit-local const; provisional name)

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
// Speeds the missile up along its heading (capped at its top speed) and moves it by one tick
void EEPMMissile::Accelerate(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GEVector theForward;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    EECombatFlightRecordIn theCaptureIn = {};   // TEST ONLY (the CBMA capture)
    memcpy(theCaptureIn.location, theTransformation.mMatrix, sizeof(theCaptureIn.location));
    theCaptureIn.step = theStep;
    theCaptureIn.speed = inUnit->GetVelocity();
    theCaptureIn.max_speed = theStats->GetMaxVelocity();
    theCaptureIn.accel = theStats->GetAcceleration();
    float theSpeed = theStats->GetAcceleration() * theStep + inUnit->GetVelocity();
    if (theStats->GetMaxVelocity() < theSpeed)
        theSpeed = theStats->GetMaxVelocity();
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theTransformation.Apply(theForward, theMove);
    theTransformation.Translate(theMove);
    if (EESimCaptureRecord::Active()) {
        EECombatFlightRecordOut theCaptureOut = {};
        theCaptureOut.moved = 1;
        theCaptureOut.speed = theSpeed;
        memcpy(theCaptureOut.matrix, theTransformation.mMatrix, sizeof(theCaptureOut.matrix));
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'A'), theCaptureIn, theCaptureOut);
    }
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theTileX, &theTileY);
}

// FUNCTION: EXE 0x55b103
// Turns the missile toward its drop target by at most its turn rate per tick, slows it the further it
// points off target (down to 1/4 of top speed) and moves it by one tick
void EEPMMissile::Home(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget;
    GEQuaternion theCurrent(inUnit->mPhysicalModel.GetLocation());
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    GEQuaternion theWanted;
    GEQuaternion theResult;
    GEVector theDirection;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theMove;
    float theStep = gPhysics->mTickSeconds;
    float theMaxSpeed = theStats->GetMaxVelocity();
    float theSpeed = inUnit->mVelocity;
    float theTurn = theStats->GetMaxAngularVelocity() * theStep;
    EEUnit* theDropTarget;
    EEFindDropLocation(inUnit, &theDropTarget, &theTarget, NULL);
    EECombatFlightRecordIn theCaptureIn = {};   // TEST ONLY (the CBMH capture)
    memcpy(theCaptureIn.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(theCaptureIn.location));
    theCaptureIn.target[0] = theTarget.GetX();
    theCaptureIn.target[1] = theTarget.GetY();
    theCaptureIn.target[2] = theTarget.GetZ();
    theCaptureIn.step = theStep;
    theCaptureIn.speed = theSpeed;
    theCaptureIn.max_speed = theMaxSpeed;
    theCaptureIn.accel = theStats->GetAcceleration();
    theCaptureIn.max_angular = theStats->GetMaxAngularVelocity();
    theDirection.mX = theTarget.GetX() - inUnit->mPhysicalModel.GetLocation().GetX();
    theDirection.mY = theTarget.GetY() - inUnit->mPhysicalModel.GetLocation().GetY();
    theDirection.mZ = theTarget.GetZ() - inUnit->mPhysicalModel.GetLocation().GetZ();
    theDirection.Normalize();
    float theHeading = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    GETransformation theAim(false);
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f;
    theAim.SetOrientationYPR(theHeading, thePitch, 0.0f);
    theAim.SetPosition(0.0f, 0.0f, 0.0f);
    theAim.SetElement(3, 0, 0.0f);
    theAim.SetElement(3, 1, 0.0f);
    theAim.SetElement(3, 2, 0.0f);
    theAim.SetElement(3, 3, 1.0f);
    theWanted.ConvertFrom(theAim);
    float theDiff = UMAbs(theWanted.GetX() - theCurrent.GetX());
    float theAbs = UMAbs(theWanted.GetY() - theCurrent.GetY());
    if (theAbs > theDiff)
        theDiff = theAbs;
    theAbs = UMAbs(theWanted.GetZ() - theCurrent.GetZ());
    if (theAbs > theDiff)
        theDiff = theAbs;
    theAbs = UMAbs(theWanted.GetW() - theCurrent.GetW());
    if (theAbs > theDiff)
        theDiff = theAbs;
    float theT;
    if (theDiff > theTurn)
        theT = theTurn / theDiff;
    else
        theT = 1.0f;
    theCurrent.Slerp(theWanted, theT, theResult);
    GETransformation theNew(false);
    theResult.ConvertTo(theNew);
    theNew.SetPosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                       inUnit->mPhysicalModel.GetLocation().GetZ());
    theNew.Apply(theForward, theMove);
    float theDot = theMove.mZ * theDirection.mZ + theMove.mY * theDirection.mY + theMove.mX * theDirection.mX;
    if (theDot > 1.0f)
        theDot = 1.0f;
    else if (theDot < -1.0f)
        theDot = -1.0f;
    float theAngle = UMArcCos(theDot) * 0.31830987f;
    float theFactor = 1.0f - theAngle * theAngle;
    if (theFactor < kMissileMinimumFactor)
        theFactor = kMissileMinimumFactor;
    float theLimit = theFactor * theMaxSpeed;
    if (theSpeed < theLimit) {
        theSpeed = theStats->GetAcceleration() * theStep + theSpeed;
        if (theSpeed > theLimit)
            theSpeed = theLimit;
    } else {
        theSpeed = theSpeed - theStats->GetAcceleration() * theStep;
        if (theSpeed < theLimit)
            theSpeed = theLimit;
    }
    inUnit->SetVelocity(theSpeed);
    theForward.mY = theSpeed * theStep;
    theNew.Apply(theForward, theMove);
    theNew.Translate(theMove);
    if (EESimCaptureRecord::Active()) {
        EECombatFlightRecordOut theCaptureOut = {};
        theCaptureOut.moved = 1;
        theCaptureOut.speed = theSpeed;
        memcpy(theCaptureOut.matrix, theNew.mMatrix, sizeof(theCaptureOut.matrix));
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'M', 'H'), theCaptureIn, theCaptureOut);
    }
    long theTileX;
    long theTileY;
    EEUpdateUnitTile(&theNew, inUnit, &theTileX, &theTileY);
}
