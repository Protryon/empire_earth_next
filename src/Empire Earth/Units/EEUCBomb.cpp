// C:\EEX\Empire Earth\Units\EEUCBomb.cpp (file name inferred: no __FILE__; the class name). The bomb's unit class:
// 0x20e2cb..0x20e58d (after EEUCBasicPriest.cpp; EEUCBuilding.cpp follows). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory). Order: VirtualAC (the drop), VirtualB8 with its function-static colour's
// empty atexit dtor, then EEUnitClass::LaunchStoredMissile (the carrier drops its first bomb; EEUnitClass::FireShot calls it).
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUCProjectile.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCBomb.h"

// FUNCTION: EXE 0x60e2cb
// Drops the bomb from its carrier: aims it along the carrier's heading, pitched down by the launcher's tilt (0 to
// -90 degrees), with the launch speed's horizontal part; always returns true
bool EEUCBomb::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    float theSpeed = inUnit->mPlayer->GetUpgrade(inUnit->mLauncherClass->mUnitType)->mLaunchSpeed;
    theTransformation.Apply(theForward, theDirection);
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    float theTilt = inUnit->mLauncherClass->mLaunchPitch;
    float thePitch = UMArcCos(-theDirection.GetZ()) - 1.5707964f + theTilt;
    if (thePitch > 0.0f)
        thePitch = 0.0f;
    else if (thePitch < -1.5707964f)
        thePitch = -1.5707964f;
    float theVelocity = UMCos(thePitch) * theSpeed;
    inUnit->SetFlightTime(0);
    inUnit->mOrientation = GEOrientation(theYaw, thePitch, 0.0f);
    inUnit->SetProjectileStart(thePosition);
    inUnit->SetVelocity(theVelocity);
    theTransformation.SetOrientationYPR(theYaw, thePitch, 0.0f);
    long theX;
    long theY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    return true;
}

// FUNCTION: EXE 0x60e446
// Flies the bomb; an SAS explosive (unit type 0x305) makes its target blink red every half second while it attacks
void EEUCBomb::Update(EEComplexUnit* inUnit)
{
    static GEColor sColor(0.9f, 0.0f, 0.2f);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeBombForSASExplosiveExpert && inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
        EEUnit* theUnit = static_cast<EEUGAttackUnit*>(inUnit->mGoalState)->mTarget;
        EEComplexUnit* theTarget = dynamic_cast<EEComplexUnit*>(theUnit);
        if (theTarget != NULL) {
            unsigned long theTime = inUnit->mFlightTime % 1000;
            if (theTime > 500) {
                theTarget->SetIsTinted(true);
                theTarget->SetColor(sColor);
            } else
                theTarget->SetIsTinted(false);
        }
    }
    EEUCProjectile::Update(inUnit);
}

// SYNTHETIC: EXE 0x60e50b
// (sColor's empty atexit destructor)

// FUNCTION: EXE 0x60e50c
// Has a carrier drop its first cargo unit (a bomb) at its target and set it moving
void EEUnitClass::LaunchStoredMissile(EEComplexUnit* inUnit)
{
    if (!(inUnit->GetNumCargoUnits() > 0))
        return;
    GE3DPoint theLocation;
    EEUnit* theTarget;
    EEFindDropLocation(inUnit, &theTarget, &theLocation, NULL);
    EEComplexUnit* theCargo = inUnit->mCargo[0];
    inUnit->UnloadCargoNow(theCargo);
    this->LaunchShot(inUnit, theTarget, theLocation, theCargo);
    theCargo->SetAnimation(0xd, -1, 0);
    theCargo->StartAction(kActionAdvance);
    theCargo->mUnitClass->ActionChanged(theCargo);
}
