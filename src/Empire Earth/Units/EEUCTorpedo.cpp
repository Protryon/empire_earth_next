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
#include "ee/EEUGAttackGround.h"
#include "ee/EEUCMissile.h"
#include "ee/EEUCTorpedo.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"

// FUNCTION: EXE 0x61b163
// When the torpedo has reached ground at or above sea level, gives it an attack-ground goal there and returns
// true
bool EEUCTorpedo::CheckGroundImpact(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GE3DPoint theTarget;
    bool theResult = false;
    inUnit->GetPosition(thePosition);
    ::gWorld.mMap->GetGroundPoint(thePosition, theTarget);
    if (thePosition.GetZ() <= theTarget.GetZ() && theTarget.GetZ() >= 0.0f) {
        inUnit->SetGoal(new EEUGAttackGround(theTarget, false), 0, 0);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x61b1f7
// Answers hit-ground with the ground impact check and ignores hit-water (0); other triggers go to the missile
// class
long EEUCTorpedo::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    case kTriggerProjectileHitGround:
        if (this->CheckGroundImpact(inUnit))
            theResult = 1;
        break;
    case kTriggerProjectileHitWater:
        break;
    default:
        theResult = EEUCMissile::EvaluateCondition(inUnit, inEvent);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x61b232
// Flies the torpedo as a missile and keeps it from rising above the water plane (height 0)
void EEUCTorpedo::Update(EEComplexUnit* inUnit)
{
    EEUCMissile::Update(inUnit);
    if (!(inUnit->mPhysicalModel.GetLocation().GetZ() > 0.0f))
        return;
    GETransformation theTransformation(false);
    theTransformation = inUnit->mPhysicalModel.GetLocation();
    theTransformation.SetZ(0.0f);
    inUnit->SetTransformation(theTransformation, true);
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
