// C:\EEX\Empire Earth\AI Thread\EECPASGUHero.cpp (file name inferred: no __FILE__; RTTI class EECPASGUHero,
// vtable 0x43936c). An attack sub group unit kind (mType 9) over the prophet's: 0x6391..0x6547 (EECPASGUHelicopter's
// dtor ends at 0x6391, EECPASGUMechPriest's ctor starts at 0x6547). The first ctor sets the kind in the body; ??_G
// follows it (its first user); the load ctor, the dtor and the slots are out of line.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPASGUHero.h"


// FUNCTION: EXE 0x406391
// Creates the hero entry for a unit in the sub-group
EECPASGUHero::EECPASGUHero(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPASGUProphet(inUnit, inSubGroup)
{
    this->mType = kASGUKindHero;
}

// SYNTHETIC: EXE 0x4063b4
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4063d0
// Creates an empty hero entry for the sub-group (no unit yet)
EECPASGUHero::EECPASGUHero(EECPAttackSubGroup* inSubGroup) : EECPASGUProphet(inSubGroup)
{
    this->mType = kASGUKindHero;
}

// FUNCTION: EXE 0x4063ef
// Destroys the hero entry (base members only)
EECPASGUHero::~EECPASGUHero()
{
}

// FUNCTION: EXE 0x4063fa
// Switches state the prophet way
void EECPASGUHero::SetState(long inState, unsigned long inTime)
{
    EECPASGUProphet::SetState(inState, inTime);
}

// FUNCTION: EXE 0x40640a
// Returns true if the hero is in danger the base way or should retreat
bool EECPASGUHero::IsInDanger()
{
    bool theResult = EECPAttackSubGroupUnit::IsInDanger();
    if (!theResult)
        theResult = this->ShouldRetreat();
    return theResult;
}

// FUNCTION: EXE 0x40641f
// Returns true below 25% hit points if a target that can hurt the hero is within 7 tiles, or its squared distance
// is within twice the target's range (the original compares a squared distance with a plain one)
bool EECPASGUHero::ShouldRetreat()
{
    bool theResult;
    if ((float)this->mUnit->mHitPoints / this->mUnit->mStats->mMaxHitPoints >= 0.25f)
        theResult = false;
    else {
        theResult = false;
        for (long i = this->mSubGroup->mTargets.size() - 1; i >= 0; i--) {
            EEComplexUnit* theTarget = this->mSubGroup->mTargets[i]->mUnit;
            EEUnitClass* theClass = this->mUnit->mUnitClass;
            long theTargetKind = theTarget->mUnitClass->mAmmoType;
            long theKind = theClass->mFamily;
            if (!theClass->CanBeAttacked() || gU4E8708[theKind][theTargetKind] == 0.0f)
                continue;
            float theDistance = this->mUnit->DistanceSquaredTo(theTarget);
            if (theDistance <= 49.0f || theDistance <= theTarget->mStats->mRange * theTarget->mStats->mRange)   // squared (was range + range)
                theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4064fd
// Tests a condition: has-target uses the prophet test while casting a calamity, else the plain unit test;
// others go to the prophet class
bool EECPASGUHero::CheckCondition(long inState, unsigned long inTime)
{
    bool theResult;
    switch (inState) {
    case kASGUConditionHasTarget:
        if (this->mUnit->mActiveGoal && this->mUnit->mActiveGoal->mKind == kGoalCastCalamity)
            theResult = EECPASGUProphet::CheckCondition(kASGUConditionHasTarget, inTime);
        else
            theResult = EECPAttackSubGroupUnit::CheckCondition(kASGUConditionHasTarget, inTime);
        break;
    default:
        theResult = EECPASGUProphet::CheckCondition(inState, inTime);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x40653b
// Updates the hero the prophet way
void EECPASGUHero::Update(unsigned long inTime)
{
    EECPASGUProphet::Update(inTime);
}


// ICF-FOLDED: EXE 0x407efc
// Looks for a calamity target the prophet way; failing that, a hero able to attack looks for a plain target
// (no address of its own: the same body as EECPASGUMechProphet::Retarget, which ICF kept; [sib 31])
bool EECPASGUHero::Retarget()
{
    bool theResult = EECPASGUProphet::Retarget();
    if (!theResult && this->mUnit->mUnitClass->FindAction(kActionInitialAttackState) != NULL)
        theResult = EECPAttackSubGroupUnit::Retarget();
    return theResult;
}
