// C:\EEX\Empire Earth\Calamities\EEAEMindControlFunctor.cpp (file name inferred: no __FILE__; the class name, as the
// sibling functor units). The mind-control area effect's functor (RTTI EEAEMindControlFunctor, vtable 0x43c5ec):
// 0xac891..0xac991. Its empty Apply (0xac98e) is also EEAEExtraDamageFunctor's (ICF). Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EETechCapture.h"

extern EEEffectManager*& gEffectManager;

// FUNCTION: EXE 0x4ac891
// Creates the mind-control area functor for a unit set and mode, with the effect shown on caught units
EEAEMindControlFunctor::EEAEMindControlFunctor(EEGraphicEffect* inEffect, unsigned long inValue, long inMode)
    : EEAreaEffectFunctor(NULL, NULL, 0, 0, inValue, inMode)
{
    this->mEffect = inEffect;
}

// FUNCTION: EXE 0x4ac8ba
// Destroys the functor and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x4ac8d6
// Destroys the functor (nothing of its own to free)
EEAEMindControlFunctor::~EEAEMindControlFunctor()
{
}

// FUNCTION: EXE 0x4ac8e1
// Shows the mind-control effect on a caught unit, disables it (reason 0x1a) and halts it
void EEAEMindControlFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionInfect, inUnit->mUnitID);   // CONT (TEST ONLY)
    if (this->mEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, this->mEffect, NULL, false, 0, 0);
    inUnit->mComplexUnit->SetDisabled(true, 0x1a);
    inUnit->mComplexUnit->Halt();
}

// FUNCTION: EXE 0x4ac918
// Re-enables a unit when the mind control ends; a living one first loses its build targets, goals and
// orders and goes idle
// (handing the unit back to its owner is not done here)
void EEAEMindControlFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionCure, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (theUnit == NULL)
        return;
    if (theUnit->mHitPoints > 0) {
        theUnit->ClearBuildTargets();
        for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
            theUnit->ClearGoalQueue(i);
        theUnit->ClearGoals(0);
        theUnit->SetDisabled(false, 0);
        theUnit->StartAction(kActionIdle);
        inUnit->mUnitClass->ActionChanged(theUnit);
    } else {
        theUnit->SetDisabled(false, 0);
    }
}

// FUNCTION: EXE 0x4ac98e
// Does nothing each update (also the extra-damage functor's Apply)
void EEAEMindControlFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
}
