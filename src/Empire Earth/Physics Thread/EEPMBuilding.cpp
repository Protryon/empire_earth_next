// C:\EEX\Empire Earth\Physics Thread\EEPMBuilding.cpp (file name inferred: no __FILE__; RTTI EEPMBuilding; one unit per
// physics-model class, alphabetical link order). Range 0x1587c7..0x158a24; split out of a single EEPMLand.cpp
// (each unit ends with its own inline-COMDAT cluster).
#include <windows.h>
#include <vector>
#include <math.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/EEUnitKind.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEPhysicsModel.h"

// NEAR: gate tail: dead dynamic_cast result and zero register
// FUNCTION: EXE 0x5587c7
// Sinks a collapsing building: tilts it (not airports), jitters it within its tile and lowers it; the
// first part of a gate also sinks its last linked carrier and last linked part
void EEPMBuilding::Step2(EEComplexUnit* inUnit)
{
    EEUnitStoreModel& theModel = inUnit->mPhysicalModel;   // (its SetLocation reports to the unit store)
    GETransformation theTransformation(theModel.GetLocation());
    GE3DPoint theLocation(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theResult;
    float theStep = gPhysics->mTickSeconds;
    if (inUnit->mUnitClass->mUnitType != kUnitTypeAirport) {
        theTransformation.Apply(theForward, theResult);
        float theHeading = UMArcTan2(-theResult.mX, theResult.mY);
        float thePitch = UMArcCos(-theResult.mZ) - 1.5707964f - theStep * 0.03f;
        theTransformation.SetOrientationYPR(theHeading, thePitch, 0.0f);
    }
    theLocation.mX += (0.5f - gPhysics->GetRandom()) * kHeightEpsilon;
    theLocation.SetY((0.5f - gPhysics->GetRandom()) * kHeightEpsilon + theLocation.GetY());
    theLocation.SetZ(theLocation.GetZ() - theStep * 0.13f);
    float theTileX = (float) UMFloor(theLocation.GetX());
    if (theTileX == (float) UMFloor(inUnit->mPhysicalModel.GetLocation().GetX())) {
        float theTileY = (float) UMFloor(theLocation.GetY());
        if (theTileY == (float) UMFloor(inUnit->mPhysicalModel.GetLocation().GetY()))
            theTransformation.SetPosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    }
    theModel.SetLocation(theTransformation);
    if (inUnit->mUnitClass->mCategory != kUnitCategoryGate)
        return;
    EEUCGate* theGate = EEUnitClassCast<EEUCGate>(inUnit->mUnitClass);
    if (inUnit->mPreviousPart == NULL && inUnit->mNextPart != NULL) {
        this->Step2(EEUnitClassCast<EEUCGate>(inUnit->GetUnitClass())->GetLastLinkedCarrier(inUnit));
        this->Step2(EEUnitClassCast<EEUCGate>(inUnit->GetUnitClass())->GetLastLinked(inUnit));
    }
}

void EEPMBuilding::Collide(EEComplexUnit* inUnit)
{
    EEPhysicsModel::Collide(inUnit);
}
