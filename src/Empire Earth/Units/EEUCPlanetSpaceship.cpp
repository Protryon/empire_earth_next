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

#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
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
    // New (R5b.4, the fourteenth round): the decision is Rust (ee_units actions.rs planet_spaceship), its operations
    // EEUnitUpdate.cpp's host calls
    EEUnitClassActionChanged(EE_UNITS_ACTION_PLANET_SPACESHIP, this, inUnit);
}

// New (R5b.4, the fourteenth round): EEUCPlanetSpaceship::ActionChanged's holding pattern over its home airport (dies
// without one; the original's statements; EEUnitUpdate.cpp's EE_UNITS_ACT_HOLDING_PATTERN 1 calls it)
void EEPlanetSpaceshipHoldingPattern(EEUnitClass* inClass, EEComplexUnit* inUnit)
{
    EEUCPlanetSpaceship* theClass = static_cast<EEUCPlanetSpaceship*>(inClass);
    GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
    EEAirTrafficControl* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY(),
                          inUnit->mPhysicalModel.GetLocation().GetZ());
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
        theTarget.SetZ(theClass->GetCruiseAltitude());
        theClass->PathToward(inUnit, thePosition, theTarget, true);
        inUnit->SetIsCommandable(false);
        if (inUnit->GetNextWaypointPoint(thePosition) == 0)
            inUnit->SetHeight(theClass->GetHeightAt(inUnit, thePosition));
    }
}

