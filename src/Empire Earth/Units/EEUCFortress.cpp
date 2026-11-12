// C:\EEX\Empire Earth\Units\EEUCFortress.cpp (file name inferred: no __FILE__; the class name). The fortress (a land
// transport that garrisons; unit type 0x313 is the teleporter): 0x21013b..0x21048f (after
// EEUCBuildingPriest.cpp; a globals chain opens the next unit at 0x21048f). Unit flags: the exe's module set. Its ctor
// is inline (EEUnitClass.cpp's factory).
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
#include "lle/U2DPoint.h"

#include "ee/EEUnitUpdateGolden.h"   // TEST ONLY (round 13's golden)
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCLandTransport.h"
#include "ee/EEUCFortress.h"
#include "ee/EEUCBuilding.h"

// 0x4c8fa4 (.data, a word): the faith a teleporter spends per full unload (provisional name)
unsigned short gFortressUnloadCost = 100;
// 0x446ff4 is EEUCBuilding.cpp's -2.0f (VirtualA0 reads the same word: the pooled literal)

// FUNCTION: EXE 0x61013b
// Finds the meeting tiles for loading like any transport
void EEUCFortress::FindRendezvousTiles(std::vector<EEComplexUnit*>* inUnits, EEComplexUnit* inTransport,
                             U2DPoint<long>* outTransportTile, U2DPoint<long>* outUnitTile)
{
    EEUCTransport::FindRendezvousTiles(inUnits, inTransport, outTransportTile, outUnitTile);
}

// FUNCTION: EXE 0x610153
// Unloads where the fortress stands; a teleporter unloads at the destination
void EEUCFortress::FindUnloadTiles(EEComplexUnit* inTransport, U2DPoint<long>* inDestination,
                             U2DPoint<long>* outUnloadTile, U2DPoint<long>* outFirstCargoTile, U2DPoint<long>* outTile)
{
    if (this->mUnitType != kUnitTypeTeleporter) {
        *outTile = EETileCoordinate(inTransport->mTileX, inTransport->mTileY);
        *outFirstCargoTile = *outTile;
        *outUnloadTile = *outFirstCargoTile;
    } else {
        *outUnloadTile = *inDestination;
        *outFirstCargoTile = *outUnloadTile;
        *outTile = *outFirstCargoTile;
    }
}

// FUNCTION: EXE 0x6101b5
// Takes a garrisoned unit's population cost off the player's count (unless inArg)
void EEUCFortress::CargoAdded(EEComplexUnit* inUnit, EEComplexUnit* inCargo, bool inArg)
{
    if (!inArg)
        inUnit->mPlayer->mPopulationCount -= inCargo->mUnitClass->GetPopulationCost();
}

// FUNCTION: EXE 0x6101da
// Gives a removed unit's population cost back to the player's count
void EEUCFortress::CargoRemoved(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost();
}

// FUNCTION: EXE 0x6101f8
// Returns whether the player's population has room for the unit's cost
bool EEUCFortress::HasRoom(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    EEPlayer* thePlayer = inUnit->mPlayer;
    EEUnitClass* theClass = inCargo->mUnitClass;
    bool theResult = thePlayer->GetTotalPopulation(false, false) + theClass->GetPopulationCost() <= thePlayer->GetPopulationLimit(0);
    return theResult;
}

// FUNCTION: EXE 0x610235
// Returns whether a tile can take the cargo: within unload range for a fortress, visible for a teleporter, and
// allowed by the transport rules
bool EEUCFortress::CanUnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    if (this->mUnitType != kUnitTypeTeleporter) {
        GE2DPoint theCenter(inX + 0.5f, inY + 0.5f);
        float theRange = inTransport->mUnitClass->mUnloadRange;
        return inTransport->DistanceSquared2D(theCenter) <= theRange * theRange && EEUCLandTransport::CanUnloadAt(inTransport, inX, inY);
    }
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    return theTile != NULL && theTile->IsVisibleTo(inTransport->GetPlayer()) && EEUCTransport::CanUnloadAt(inTransport, inX, inY);
}

// FUNCTION: EXE 0x61030a
// Sets up a new fortress as built, with a 1 s regeneration timer
void EEUCFortress::UnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetBuilt(0, 1, 1);
    EEUnitClass::UnitCreated(inUnit);
    inUnit->SetRegenerationTimer(1000);
}

// FUNCTION: EXE 0x61033d
// Adds the fortress to the map regions as a building
void EEUCFortress::AddToRegion(EEUnit* inUnit)
{
    EEAddBuildingRegion(inUnit);
}

// FUNCTION: EXE 0x61034a
// Removes the fortress from the map regions as a building
void EEUCFortress::RemoveFromRegion(EEUnit* inUnit)
{
    EERemoveBuildingRegion(inUnit);
}

// FUNCTION: EXE 0x610357
// Answers the fall-finished trigger once it has sunk below -2; other triggers go to the common unit class
long EEUCFortress::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerDyingAnimationFinished:
        if (inUnit->mPhysicalModel.GetLocation().GetZ() < -2.0f)
            theResult = 1;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x610391
// Unloads at a tile; a teleporter needs 100 faith and spends it in proportion to the cargo unloaded, else warns
// its owner (event 0x274e)
void EEUCFortress::UnloadAt(EEComplexUnit* inTransport, long inX, long inY)
{
    if (inTransport->mUnitClass->mUnitType == kUnitTypeTeleporter) {
        if (inTransport->GetPower() >= gFortressUnloadCost) {
            unsigned long theBefore = inTransport->mUnitClass->mTransportCapacity - inTransport->mCargoRoom;
            EEUCLandTransport::UnloadAt(inTransport, inX, inY);
            long theCapacity = inTransport->mUnitClass->mTransportCapacity;
            unsigned long theAfter = theCapacity - inTransport->mCargoRoom;
            if (theBefore != theAfter)
            {
                float theFraction = (float)(theBefore - theAfter) / theCapacity;
                inTransport->SetPower(inTransport->GetPower() - UMFloatToLong(theFraction * gFortressUnloadCost));
            }
        } else {
            U2DPoint<long> theTile(inX, inY);
            inTransport->mPlayer->mEvents.AddEvent(theTile, 2, inTransport->GetPlayer(), 0x274e, false);
        }
    } else
        EEUCLandTransport::UnloadAt(inTransport, inX, inY);
}

// FUNCTION: EXE 0x610465
// Updates the fortress as a transport; a teleporter also regains faith
void EEUCFortress::Update(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mUnitType == kUnitTypeTeleporter)
        (EEUN(REGENERATE_POWER), this->RegeneratePower(inUnit));
    EEUCTransport::Update(inUnit);
}

// Gives an unloaded unit's population cost back to the player's count (ICF copy: +0x1c's body is identical to
// Virtual14; LINK kept 0x6101da for both slots)
void EEUCFortress::CargoUnloaded(EEComplexUnit* inUnit, EEComplexUnit* inCargo)
{
    inUnit->mPlayer->mPopulationCount += inCargo->mUnitClass->GetPopulationCost();
}

// ICF-FOLDED: EXE 0x60f2ae
// Adds the building to every tile of its footprint
// (no address of its own: the same body as EEUCBuilding::AddToTile, which ICF kept)
void EEUCFortress::AddToTile(EEUnit* inUnit)
{
    EEAddToFootprintTiles(inUnit);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCFortress::IsLandBuilding()
{
    return true;
}

// ICF-FOLDED: EXE 0x60fb78
// Removes the building from every tile of its footprint
// (no address of its own: the same body as EEUCBuilding::RemoveFromTile, which ICF kept)
void EEUCFortress::RemoveFromTile(EEUnit* inUnit)
{
    EERemoveFromFootprintTiles(inUnit);
}

// ICF-FOLDED: EXE 0x557269
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCFortress::UpdateEffects(class EEComplexUnit *)
{
}
