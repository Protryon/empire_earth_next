// C:\EEX\Empire Earth\Units\EEUCTorpedo.cpp (file name inferred: no __FILE__; the class name). The torpedo's unit class:
// 0x21b163..0x21b27a (after EEUCSubmergedNaval.cpp; EEUCTransport.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline; +0xe4 is ICF-folded (0x29059d: false).
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

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUCMissile.h"
#include "ee/EEUCTorpedo.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"

// FUNCTION: EXE 0x61b163
// When the torpedo has reached ground at or above sea level, gives it an attack-ground goal there and returns
// true
bool EEUCTorpedo::CheckGroundImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's torpedo_ground (impact.rs)
    GE3DPoint thePosition;
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_TORPEDO_GROUND;
    inUnit->GetPosition(thePosition);
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_GROUND)
        inUnit->SetGoal(EEUGRust::NewAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x61b1f7
// Answers hit-ground with the ground impact check and ignores hit-water (0); other triggers go to the missile
// class
long EEUCTorpedo::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    // ee_combat goals.rs condition (CBTC); the missile's test is the host's base read
    return EECombatCondition(EE_COMBAT_GOAL_TORPEDO, inUnit, inEvent, EECombatShotTarget(inUnit), NULL, this);
}

// FUNCTION: EXE 0x61b232
// Flies the torpedo as a missile and keeps it from rising above the water plane (height 0)
void EEUCTorpedo::Update(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope(2);
    uint32_t theBefore = EECombatCommandOf(inUnit);
    // ee_combat shots.rs update (the missile's, then the water plane) over EEUCProjectile.cpp's host; CBSO records the outcome
    EECombatShotUpdate(EE_COMBAT_SHOT_TORPEDO, this, inUnit);
    if (theScope.mOuter)
        EECombatCaptureShot(EE_COMBAT_SO_TORPEDO_UPDATE, inUnit, theBefore);
}

// ICF-FOLDED: EXE 0x69059d
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
bool EEUCTorpedo::CheckWaterImpact(class EEComplexUnit *)
{
    return false;
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCTorpedo::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
