// C:\EEX\Empire Earth\Units\EEUCNavalBuilding.cpp (file name inferred: no __FILE__; the class name). The naval building's
// unit class (a dock): 0x218158..0x218490 (after EEUCNaval.cpp; EEUCNavalCitizen.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline. Virtual74/78/88 are ICF-folded into EEUCSpaceBuilding.cpp's copies; VirtualA0/E0
// here are also EEUCSpaceBuilding's (ICF).
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
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEUnitUpdate.h"
#include "rust/ee_units.h"
#include "ee/EEUnitKind.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGRust.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCNavalBuilding.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EmpireEarth.h"

// FUNCTION: EXE 0x618158
// Puts a dock's location on the water plane (z 0)
void EEUCNavalBuilding::GroundLocation(GE3DPoint& ioLocation)
{
    ioLocation.SetZ(0.0f);
}

// FUNCTION: EXE 0x618164
// Picks the first fully healed docked ship that should leave (a human's ship whose own or the dock's rally goal is an
// attack, move or explore goal; any ship with a goal for a computer player); returns whether one was picked
bool EEUCNavalBuilding::SelectUnitToRelease(EEComplexUnit* inUnit)
{
    bool theResult = false;
    unsigned long theCount = inUnit->GetNumCargoUnits();
    for (unsigned long i = 0; !theResult && i < theCount; i++) {
        EEComplexUnit* theCargo = inUnit->mCargo[i];
        if (theCargo->mHitPoints != theCargo->mStats->mMaxHitPoints)
            continue;
        bool theReady = false;
        if (theCargo->mPlayer->IsHuman()) {
            if (theCargo->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = (theCargo->mGoalQueues.begin() + 1)->GetFront();
                if (EEGoalCast<EEUGAttackUnit>(theGoal) == 0 &&
                    EEGoalCast<EEUGLocation>(theGoal) == 0 &&
                    EEExploreGoalCast(theGoal) == 0)
                    theGoal = NULL;
                if (theGoal != NULL)
                    theReady = true;
            }
            if (!theReady && inUnit->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = (inUnit->mGoalQueues.begin() + 1)->GetFront();
                if (EEGoalCast<EEUGAttackUnit>(theGoal) == 0 &&
                    EEGoalCast<EEUGLocation>(theGoal) == 0 &&
                    EEExploreGoalCast(theGoal) == 0)
                    theGoal = NULL;
                if (theGoal != NULL)
                    theReady = true;
            }
        }
        if (theReady || (inUnit->mPlayer->mComputerPlayer != NULL && theCargo->mGoalState != NULL)) {
            inUnit->SetExitingUnitID(theCargo->mUnitID);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x618320
// Returns whether the dock's transformed bounding box lies wholly below the water line
bool EEUCNavalBuilding::HasSunk(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    const GEPhysicalModel& thePhysicalModel = inUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    inUnit->GetPosition(thePosition);
    GETransformation theTransformation(false);
    thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                         theTransformation);
    float theScale = thePhysicalModel.GetScale();
    theTransformation.Scale(theScale, theScale, theScale);
    theTransformation.SetX(thePosition.GetX());
    theTransformation.SetY(thePosition.GetY());
    theTransformation.SetZ(thePosition.GetZ());
    theBox = theModel->GetBoundingBox(inUnit->mPhysicalModel.GetFrame());
    theBox.Transform(theTransformation, theTransformedBox);
    return theTransformedBox.GetTopRightBack().GetZ() < 0.0f ? true : false;
}

// FUNCTION: EXE 0x6183f8
// Answers the dock triggers: unit has a rally goal, ship ready to launch, sunk after dying; others to the building
long EEUCNavalBuilding::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theHandled;
    switch (inEvent) {
    case kTriggerFighterHasGoal:
        theHandled = inUnit->GetQueuedGoal(1) != NULL;
        break;
    case kTriggerAirplaneAvailableToLaunch:
        theHandled = this->SelectUnitToRelease(inUnit);
        break;
    case kTriggerDyingAnimationFinished:
        theHandled = this->HasSunk(inUnit);
        break;
    default:
        theResult = EEUCBuilding::EvaluateCondition(inUnit, inEvent);
        theHandled = false;
        break;
    }
    if (theHandled)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x61845a
// Steps the dock's physics and sound, then updates it as a building
void EEUCNavalBuilding::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6, the thirteenth round): the sequence is Rust (ee_units update.rs), the leaves
    // EEUnitUpdate.cpp's host calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_NAVAL_BUILDING, this, inUnit);
}

// ICF-FOLDED: EXE 0x618605
// Creates a fishing boat on a tile at sea level
// (no address of its own: the same body as EEUCNavalCitizen::CreateUnit, which ICF kept)
EEUnit* EEUCNavalBuilding::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ICF-FOLDED: EXE 0x618909
// Creates a priest ship at a placement on the water plane (z 0)
// (no address of its own: the same body as EEUCNavalPriest::CreateUnitAt, which ICF kept)
EEUnit* EEUCNavalBuilding::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ICF-FOLDED: EXE 0x61a144
// Returns whether the space building may stop: already sinking, a slow request, or at its next waypoint's tile
// (no address of its own: the same body as EEUCSpaceBuilding::HasArrived, which ICF kept)
bool EEUCNavalBuilding::HasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theLocation;
    bool theResult = false;
    if (inUnit->mCollisionState == 8 || inValue <= 0.05f)
        theResult = true;
    else {
        inUnit->GetNextWaypointPoint(theLocation);
        if (inUnit->GetTileX() == UMFloor(theLocation.GetX()) && inUnit->GetTileY() == UMFloor(theLocation.GetY()))
            theResult = true;
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x4ac9f3
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
bool EEUCNavalBuilding::IsLandBuilding()
{
    return false;
}

// ICF-FOLDED: EXE 0x618490
// Returns the tile under a location along the camera line, at sea level
// (no address of its own: the same body as EEUCNavalCitizen::LocationToTile, which ICF kept)
void EEUCNavalBuilding::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// ICF-FOLDED: EXE 0x61a129
// Sets up a new space building as a building, already built
// (no address of its own: the same body as EEUCSpaceBuilding::UnitCreated, which ICF kept)
void EEUCNavalBuilding::UnitCreated(EEComplexUnit* inUnit)
{
    EEUCBuilding::UnitCreated(inUnit);
    inUnit->SetBuilt(1, 0, 1);
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCNavalBuilding::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
