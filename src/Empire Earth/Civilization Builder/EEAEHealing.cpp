// C:\EEX\Empire Earth\Civilization Builder\EEAEHealing.cpp (name from RTTI EEAEHealing; folder inferred: no
// __FILE__). 0x27ace1..0x27adb0: the healing area effect (vtable 0x44baec): ctor, ??_G, Virtual08, Virtual18,
// Virtual48. The base's `return false` slots +0x2c..+0x34 are ICF-folded at 0x27acf9 here. EEAEManager.cpp starts at
// 0x27adb0. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"

#include "ee/EEUnit.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEHealing.h"

extern bool& gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)

// FUNCTION: EXE 0x67ace1
// Creates a healing area effect from its template
EEAEHealing::EEAEHealing(EEAreaEffectTemplate* inTemplate)
    : EEAreaEffect(inTemplate)
{
}

// FUNCTION: EXE 0x67acf9
// Leaves every modifier unchanged (returns false); folded with the base's other `return false` slots
// (EEAEHealing::Modify: inline in ee/EEAEHealing.h, emitted with the vtable)

// SYNTHETIC: EXE 0x67acfe
// (the scalar deleting destructor; 0x22 bytes)

// FUNCTION: EXE 0x67ad20
// Tells whether a repairable unit is inside the effect's area (the base area test)
bool EEAEHealing::Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint)
{
    bool theResult = false;
    if (inUnit->IsRepairable())
        theResult = EEAreaEffect::Covers(inPlayer, inUnit, inPoint);
    return theResult;
}

// FUNCTION: EXE 0x67ad4f
// Returns the template's level as the healing strength
long EEAEHealing::GetStrength()
{
    return this->mTemplate->mLevel;
}

// FUNCTION: EXE 0x67ad56
// Heals a living unit by level per second of elapsed time (at least 1 hit point, capped at its maximum);
// does nothing during a cinematic
void EEAEHealing::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    long theHitPoints = inUnit->mHitPoints;
    if (theHitPoints <= 0 || gInCinematic)
        return;
    long theHeal = UMFloatToLong(this->mTemplate->mLevel * (inElapsed * 0.001f));
    if (theHeal <= 1)
        theHeal = 1;
    theHitPoints += theHeal;
    long theMax = inUnit->mStats->mMaxHitPoints;
    if (theHitPoints >= theMax)
        theHitPoints = theMax;
    inUnit->SetHitPoints(theHitPoints);
}
