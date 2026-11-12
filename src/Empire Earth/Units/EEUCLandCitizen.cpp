// C:\EEX\Empire Earth\Units\EEUCLandCitizen.cpp (file name inferred: no __FILE__; the class name).
// The land citizen's unit
// class: 0x216c48..0x216d4b (after EEUCLandable.cpp; EEUCLandMilitaryHero.cpp follows). One unit per EEUC* class in
// alphabetical order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory); VirtualB8,
// then EEComplexUnit::ContinueProduction after its first user.
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

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEUCLandCitizen.h"

// FUNCTION: EXE 0x616c48
// Advances a land citizen each tick (physics, effects, timers, animation, attack frames, rotting), continues its
// production, then does its citizen work
void EEUCLandCitizen::Update(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    (EEUN(LAST_HIT_POINTS), inUnit->SetLastHitPoints(inUnit->mHitPoints));
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        (EEUN(PHYSICS_STEP), this->mPhysicsModel->Step(inUnit));
    (EEUN(EFFECTS), this->UpdateEffects(inUnit));
    (EEUN(RELOAD_TIMER), inUnit->UpdateReloadTimer());
    (EEUN(TARGET_SEARCH_TIMER), inUnit->UpdateTargetSearchTimer());
    (EEUN(CAMOUFLAGE_TIMER), this->UpdateCamouflageTimer(inUnit));
    long theAttack = (EEUN(ANIMATE), this->Animate(inUnit));
    (EEUN(REFRESH_SOUND), this->RefreshSound(inUnit));
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    switch (theCommand) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        (EEUNA(FIRE_ON_ATTACK_FRAME, theAttack, theFrame), this->FireOnAttackFrame(inUnit, theAttack, theFrame));
        break;
    case kActionUnitRotting: {
        (EEUN(ROTTING), inUnit->UpdateRottingAwayTime());
        break;
    }
    }
    (EEUN(CONTINUE_PRODUCTION), inUnit->ContinueProduction());
    (EEUN(UPDATE_CITIZEN), this->UpdateCitizen(inUnit));
}

// FUNCTION: EXE 0x616d2d
// Starts the next production when nothing is being produced and the build queue is not empty
void EEComplexUnit::ContinueProduction()
{
    if (this->mProductionTime == 0 && this->GetBuildQueueSize() != 0)
        this->StartProduction();
}
