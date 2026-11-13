// C:\EEX\Empire Earth\Units\EEUCSpaceBuilding.cpp (file name inferred: no __FILE__; the class name). The space
// building's unit class: 0x219fe3..0x21a219 (after EEUCSeaAnimal.cpp; EEUCSpaceship.cpp follows). Unit flags: the
// exe's module set. Its ctor is inline. Virtual74/78/88 are also EEUCNavalBuilding's (ICF: these copies are kept);
// VirtualA0/E0 are EEUCNavalBuilding.cpp's (ICF).
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
#include "ee/EEUCBuilding.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUCSpaceBuilding.h"
#include "ee/EEUGRust.h"
#include "ee/EEUGLocation.h"

// FUNCTION: EXE 0x619fe3
// Puts a space building's location at the class's cruise altitude
void EEUCSpaceBuilding::GroundLocation(GE3DPoint& ioLocation)
{
    ioLocation.SetZ(this->mCruiseAltitude);
}

// FUNCTION: EXE 0x619ff3
// Returns the tile under a location along the camera line, at the cruise altitude
void EEUCSpaceBuilding::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    float theZ = this->mCruiseAltitude;
    theLine.SolveForZ(theZ, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x61a0a0
// Creates a space building at a placement, at the cruise altitude
EEUnit* EEUCSpaceBuilding::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                        EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(this->mCruiseAltitude);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x61a0dd
// Creates a space building on a tile, at the cruise altitude
EEUnit* EEUCSpaceBuilding::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(this->mCruiseAltitude);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x61a129
// Sets up a new space building as a building, already built
void EEUCSpaceBuilding::UnitCreated(EEComplexUnit* inUnit)
{
    EEUCBuilding::UnitCreated(inUnit);
    inUnit->SetBuilt(1, 0, 1);
}

// FUNCTION: EXE 0x61a144
// Returns whether the space building may stop: already sinking, a slow request, or at its next waypoint's tile
bool EEUCSpaceBuilding::HasArrived(EEComplexUnit* inUnit, float inValue)
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

// FUNCTION: EXE 0x61a1cb
// Steps the space building's physics and sound, regenerates it, then updates it as a building
void EEUCSpaceBuilding::Update(EEComplexUnit* inUnit)
{
    // New (R5b.4 leaf 6, the thirteenth round): the sequence is Rust (ee_units update.rs), the leaves
    // EEUnitUpdate.cpp's host calls
    EEUnitClassUpdate(EE_UNITS_UPDATE_SPACE_BUILDING, this, inUnit);
}

// ICF-FOLDED: EXE 0x6183f8
// Answers the dock triggers: unit has a rally goal, ship ready to launch, sunk after dying; others to the building
// (no address of its own: the same body as EEUCNavalBuilding::EvaluateCondition, which ICF kept)
long EEUCSpaceBuilding::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
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

// ICF-FOLDED: EXE 0x4b3257
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCSpaceBuilding::HasSunk(class EEComplexUnit *)
{
    return true;
}

// ICF-FOLDED: EXE 0x4ac9f3
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
bool EEUCSpaceBuilding::IsLandBuilding()
{
    return false;
}

// ICF-FOLDED: EXE 0x618164
// Picks the first fully healed docked ship that should leave (a human's ship whose own or the dock's rally goal is an
// attack, move or explore goal; any ship with a goal for a computer player); returns whether one was picked
// (no address of its own: the same body as EEUCNavalBuilding::SelectUnitToRelease, which ICF kept)
bool EEUCSpaceBuilding::SelectUnitToRelease(EEComplexUnit* inUnit)
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
                if (EEAttackUnitCast(theGoal) == 0 &&
                    EEGoalCast<EEUGLocation>(theGoal) == 0 &&
                    EEExploreGoalCast(theGoal) == 0)
                    theGoal = NULL;
                if (theGoal != NULL)
                    theReady = true;
            }
            if (!theReady && inUnit->GetQueueGoals(1).size() > 0) {
                EEUnitGoal* theGoal = (inUnit->mGoalQueues.begin() + 1)->GetFront();
                if (EEAttackUnitCast(theGoal) == 0 &&
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

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCSpaceBuilding::UpdateEffects(class EEComplexUnit *)
{
}
