// C:\EEX\Empire Earth\Units\EEUCPlanetSpaceship.cpp (file name inferred: no __FILE__; the class name). The planet
// spaceship's unit class: 0x218e34..0x219143 (after EEUCNavalTransport.cpp; EEUCProjectile.cpp's +0xe0 0x219143
// follows). Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); one own slot, +0x74.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCPlanetSpaceship.h"

// 0x447330: the height a landed spaceship's mInitialContactHeading is reset to (one .rdata copy per unit: a named const)
static const float kPlanetSpaceshipNoHeight = -10000.0f;

// FUNCTION: EXE 0x618e34
// Handles a planet spaceship's new action: circling (random direction, flying height, drops non-explore goals),
// holding pattern over its home airport (dies without one), circling the controller, dying; others to the base
void EEUCPlanetSpaceship::ActionChanged(EEComplexUnit* inUnit)
{
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    EEAirTrafficControl* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionCircle:
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        if ((long)inUnit->mWaypoints.size() > 0)
            inUnit->DropPath();
        if (gPhysics->mRandom.Generate() < 0.5f)
            inUnit->SetCurrentPhysicsOrder(0);
        else
            inUnit->SetCurrentPhysicsOrder(1);
        inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        inUnit->mInitialContactHeading = kPlanetSpaceshipNoHeight;
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind != kGoalExplore)
            inUnit->ClearGoals(0);
        break;
    case kActionHoldingPattern:
        inUnit->SetCurrentPhysicsAction(3);
        if (inUnit->mHomeBase == NULL) {
            if (theControl->GetNumSlots() > 0)
                inUnit->SetHomeBase(theControl->GetSlot(0).mAirport);
            else
                inUnit->Kill(true, false, false);
        }
        if (inUnit->mHomeBase != NULL) {
            inUnit->DropPath();
            theTarget.SetX(inUnit->mHomeBase->mTileX + 1.5f);
            theTarget.SetY(inUnit->mHomeBase->GetTileY() + 1.5f);
            theTarget.SetZ(this->GetCruiseAltitude());
            this->PathToward(inUnit, thePosition, theTarget, true);
            inUnit->SetIsCommandable(false);
            if (inUnit->GetNextWaypointPoint(thePosition) == 0)
                inUnit->SetHeight(this->GetHeightAt(inUnit, thePosition));
        }
        break;
    case kActionCircleAirtrafficController:
        inUnit->SetCurrentPhysicsAction(3);
        inUnit->SetHasArrived(false);
        inUnit->SetF220(0.0f);
        break;
    case kActionDying: {
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(4, -1, 0);
        inUnit->DropPath();
        if (this->mDeathEffect != NULL)
            gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0);
        break;
    }
    default:
        EEUCLandable::ActionChanged(inUnit);
        break;
    }
}

