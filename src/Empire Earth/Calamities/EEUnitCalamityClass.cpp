// C:\EEX\Empire Earth\Calamities\EEUnitCalamityClass.cpp (file name inferred: no __FILE__; sorts after
// EEPlagueFunctor.cpp). The calamity classes cast on a unit: 0xb70cc..0xb71b5. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEMouse.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitPicker.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b70cc
// Creates a calamity of the class on the target unit for the caster, located at the target's tile corner, and resets
// it for its start
EECalamity* EEUnitCalamityClass::CreateCalamity(EEUnit* inTargetUnit, EEUnit* inCasterUnit)
{
    EECalamity* theCalamity = new EECalamity(this, inCasterUnit);
    theCalamity->SetTargetUnit(inTargetUnit);
    theCalamity->mTargetLocation = GE3DPoint((float)inTargetUnit->GetTileX(), (float)inTargetUnit->GetTileY(), 0.0f);
    this->Reset(theCalamity);
    return theCalamity;
}

// FUNCTION: EXE 0x4b712a
// Returns the id of the unit under the mouse, -1 if none
long EEUnitCalamityClass::GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool)
{
    long theUnitID = -1;
    EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
    if (theUnit != NULL) {
        theUnitID = theUnit->mUnitID;
        theUnit->Dereference();
    }
    return theUnitID;
}

// FUNCTION: EXE 0x4b7157
// Checks the base cast rules, then refuses (1) unless the target unit exists and is a valid target
long EEUnitCalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX,
                                    unsigned long inY, bool inCheck, EEUnit* inTargetUnit)
{
    // the rules are Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit,
                             EECalamityCanCastAt(this, EeCalamityCastMode::Unit, inUnit, inX, inY, inCheck, inTargetUnit));
}

// ICF-FOLDED: EXE 0x63c6cd
// (`return 3`)
long EEUnitCalamityClass::GetTargetKind()
{
    return 3;
}
