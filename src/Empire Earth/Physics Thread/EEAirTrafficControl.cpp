// C:\EEX\Empire Earth\Physics Thread\EEAirTrafficControl.cpp (name from RTTI EEAirTrafficControl and the Dump strings;
// no __FILE__). 0x14e839..0x151c28: the key chain ("EEAirTrafficControl Data"), the class in address order, then the
// std::vector / std::deque COMDATs its members instantiate. EEPhysics.cpp starts at 0x151c28. Unit flags: the exe's
// module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEPhysics.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/EEUnitKind.h"

const UString EEAirTrafficControl::sData("EEAirTrafficControl Data");
// SYNTHETIC: EXE 0x54e839   (wrapper)
// SYNTHETIC: EXE 0x54e843   (ctor)
// SYNTHETIC: EXE 0x54e854   (registrar)
// SYNTHETIC: EXE 0x54e860   (dtor)

// port (R5b.4, round 30): the class is Rust's whole (ee_units atc.rs: the bodies in their order over this host;
// atc_save.rs: Load and Save). The host is the units' reads and the world calls; the Rust state is borrowed anew
// around each, as a call here may come back into the same control (a plane's Kill takes its weight off the used
// cargo; a rerouted plane's ActionChanged queues it at its new airport).
static int64_t EEATCHost(void* inContext, int32_t inOp, void* inUnit, void* inOther, EeUnitsAtcIo* io)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = static_cast<EEComplexUnit*>(inOther);
    switch (inOp) {
    case EE_UNITS_ATC_WEIGHT: return theUnit->GetUnitClass()->mWeight;
    case EE_UNITS_ATC_TRANSPORT_CAPACITY: return theUnit->GetUnitClass()->mTransportCapacity;
    case EE_UNITS_ATC_REFERENCE: theUnit->Reference(); return 0;
    case EE_UNITS_ATC_DEREFERENCE: theUnit->Dereference(); return 0;
    case EE_UNITS_ATC_SET_CLEARANCE: theUnit->SetB342(io->a != 0); return 0;
    case EE_UNITS_ATC_CLEARANCE: return theUnit->mIsClearanceGranted ? 1 : 0;
    case EE_UNITS_ATC_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_ATC_CARGO_ROOM: return theUnit->mCargoRoom;
    case EE_UNITS_ATC_IN_CARGO:
        for (unsigned long i = 0; i < theUnit->GetNumCargoUnits(); i++)
            if ((EEUnit*)theUnit->mCargo[i] == theOther)
                return 1;
        return 0;
    case EE_UNITS_ATC_FAMILY: return theUnit->GetUnitClass()->mFamily;
    case EE_UNITS_ATC_COMMAND_TYPE: return theUnit->GetCommandType();
    case EE_UNITS_ATC_RELEASE_EXITING:   // ClearAirportQueue: the airport's exiting unit when it is the dropped plane
        if (theUnit->mExitingUnitID != -1 && theOther == theUnit->FindPassenger(theUnit->mExitingUnitID))
            theUnit->SetExitingUnitID(-1);
        return 0;
    case EE_UNITS_ATC_TILE: io->a = theUnit->mTileX; io->b = theUnit->mTileY; return 0;
    case EE_UNITS_ATC_LOCATION: {
        const GETransformation& theLocation = theUnit->mPhysicalModel.GetLocation();
        io->p[0] = theLocation.GetX();
        io->p[1] = theLocation.GetY();
        io->p[2] = theLocation.GetZ();
        return 0;
    }
    case EE_UNITS_ATC_AIR_TRAFFIC_TYPE: return theUnit->GetUnitClass()->mAirTrafficType;
    case EE_UNITS_ATC_CRUISE_ALTITUDE: io->f = theUnit->GetUnitClass()->GetCruiseAltitude(); return 0;
    case EE_UNITS_ATC_IN_CONTAINER: return theUnit->mContainer != NULL ? 1 : 0;
    case EE_UNITS_ATC_KILL: theUnit->Kill(1, 0, 0); return 0;
    case EE_UNITS_ATC_REROUTE:
        theUnit->SetHomeBase(theOther);
        theUnit->StartAction(kActionRequestLanding);
        theUnit->GetUnitClass()->ActionChanged(theUnit);
        return 0;
    case EE_UNITS_ATC_IS_AIRCRAFT:
        return theUnit->GetUnitClass()->mCategory == kUnitCategoryAirplane || theUnit->GetUnitClass()->mCategory == kUnitCategoryPlanetSpaceship;
    case EE_UNITS_ATC_HOME_BASE: return (int64_t)(intptr_t)theUnit->mHomeBase;
    case EE_UNITS_ATC_UNIT_ID: return (int64_t)theUnit->GetUnitID();
    case EE_UNITS_ATC_RESOLVE: return (int64_t)(intptr_t)EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((uint32_t)io->a));
    case EE_UNITS_ATC_DUMP_LINE: fputs(static_cast<const char*>(inUnit), static_cast<FILE*>(inContext)); return 0;
    }
    return 0;
}

int64_t EEAirTrafficControl::Call(int32_t inCall, EEComplexUnit* inUnit, EEComplexUnit* inOther, long inValue)
{
    return ee_units_atc_call(this->mRust, NULL, &EEATCHost, inCall, inUnit, inOther, inValue);
}

// FUNCTION: EXE 0x54e87c
// Creates the player's air traffic control with no airports and no air cargo capacity
EEAirTrafficControl::EEAirTrafficControl(EEPlayer* inPlayer)
    : mPlayer(inPlayer), mRust(ee_units_atc_new())
{
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x54e8b7

// FUNCTION: EXE 0x54e8d3
// Destroys the air traffic control, releasing every queued plane and every airport
EEAirTrafficControl::~EEAirTrafficControl()
{
    ee_units_atc_free(this->mRust, NULL, &EEATCHost);
}

// FUNCTION: EXE 0x54e9a8
void EEAirTrafficControl::AddCargoWeight(EEComplexUnit* inUnit) { this->Call(EE_UNITS_ATC_CALL_ADD_CARGO_WEIGHT, inUnit); }

// FUNCTION: EXE 0x54e9bb
void EEAirTrafficControl::AddAirport(EEComplexUnit* inAirport) { this->Call(EE_UNITS_ATC_CALL_ADD_AIRPORT, inAirport); }

// FUNCTION: EXE 0x54ea34
long EEAirTrafficControl::QueueRequest(EEComplexUnit* inAirport, EEComplexUnit* inUnit, long inKind)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_QUEUE_REQUEST, inAirport, inUnit, inKind);
}

// FUNCTION: EXE 0x54eaf7
long EEAirTrafficControl::GetFreeCargoWeight(EEComplexUnit* inAirport)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_FREE_CARGO_WEIGHT, inAirport);
}

// FUNCTION: EXE 0x54eb2c (GetRoom: ee_units atc.rs room)

// FUNCTION: EXE 0x54ec8c
bool EEAirTrafficControl::RemoveFromTakeoffQueues(EEComplexUnit* inUnit)
{
    return this->Call(EE_UNITS_ATC_CALL_REMOVE_FROM_TAKEOFF_QUEUES, inUnit) != 0;
}

// FUNCTION: EXE 0x54ed30
bool EEAirTrafficControl::ClearAirportQueue(EEComplexUnit* inAirport, long inKind)
{
    return this->Call(EE_UNITS_ATC_CALL_CLEAR_AIRPORT_QUEUE, inAirport, NULL, inKind) != 0;
}

// FUNCTION: EXE 0x54eed0 (RemoveFromEntry: atc.rs remove_from_entry)
// FUNCTION: EXE 0x54f033 (DropDeadPlanes: atc.rs drop_dead_planes)

// FUNCTION: EXE 0x54f15d
// Writes the air cargo totals and every airport's landing and takeoff queues to the out-of-sync dump
void EEAirTrafficControl::Dump(FILE* inFile)
{
    ee_units_atc_call(this->mRust, inFile, &EEATCHost, EE_UNITS_ATC_CALL_DUMP, NULL, NULL, 0);
}

// FUNCTION: EXE 0x54f32f (GetCandidates: atc.rs candidates)

// FUNCTION: EXE 0x54f455
// Returns the squared distance in tiles between the two units (atc.rs tile_distance_squared for the air traffic)
float EEUnit::GetTileDistanceSquared(EEUnit* inOther)
{
    long theDeltaY = this->mTileY - inOther->mTileY;
    long theDeltaX = this->mTileX - inOther->mTileX;
    return (float)(theDeltaX * theDeltaX + theDeltaY * theDeltaY);
}

// FUNCTION: EXE 0x54f47d
long EEAirTrafficControl::GetChecksum() { return (long)this->Call(EE_UNITS_ATC_CALL_CHECKSUM, NULL); }

// FUNCTION: EXE 0x54f5fa
unsigned long EEAirTrafficControl::GetQueueLength(EEComplexUnit* inAirport)
{
    return (unsigned long)this->Call(EE_UNITS_ATC_CALL_QUEUE_LENGTH, inAirport);
}

// FUNCTION: EXE 0x54f61c
long EEAirTrafficControl::GetQueuePosition(EEComplexUnit* inUnit, long inWhich)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_QUEUE_POSITION, inUnit, NULL, inWhich);
}

// FUNCTION: EXE 0x54f6d8 (IsLandingDone: the plane's container, in atc.rs update_landings)
// FUNCTION: EXE 0x54f6e9 (IsTakeOffDone: atc.rs is_takeoff_done)

// The file's primitives for Load and Save (EESavePrimitive) and a unit's id for Save
static int32_t EEATCFile(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    return EESavePrimitive(*static_cast<FSFile*>(inFile), inOp, io);
}

static unsigned long EEATCUnitID(void* inUnit)
{
    return static_cast<EEComplexUnit*>(inUnit)->GetUnitID();
}

// FUNCTION: EXE 0x54f75c
// Reads the airports, their queued planes (as unit ids) and the air cargo totals; returns the first read error
long EEAirTrafficControl::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    return ee_units_atc_load(this->mRust, inFile, &EEATCFile);
}

// FUNCTION: EXE 0x54f88d
// Hands replaced aircraft's cargo weight and queue entries to their replacements
void EEAirTrafficControl::ReplaceUnits(std::vector<EEPUnitSwap>& inSwaps)
{
    std::vector<void*> theOld;
    std::vector<void*> theNew;
    for (size_t i = 0; i < inSwaps.size(); i++) {
        theOld.push_back(inSwaps[i].mOld);
        theNew.push_back(inSwaps[i].mNew);
    }
    ee_units_atc_replace_units(this->mRust, NULL, &EEATCHost, theOld.data(), theNew.data(), theOld.size());
}

// FUNCTION: EXE 0x54fa2b (SendPlanesAway: atc.rs send_planes_away)

// FUNCTION: EXE 0x54fc39
void EEAirTrafficControl::RemoveCargoWeight(EEComplexUnit* inUnit) { this->Call(EE_UNITS_ATC_CALL_REMOVE_CARGO_WEIGHT, inUnit); }

// FUNCTION: EXE 0x54fc4c
void EEAirTrafficControl::RemoveAirport(EEComplexUnit* inAirport) { this->Call(EE_UNITS_ATC_CALL_REMOVE_AIRPORT, inAirport); }

// FUNCTION: EXE 0x54fcc1
void EEAirTrafficControl::RemovePlane(EEComplexUnit* inAirport, EEComplexUnit* inUnit)
{
    this->Call(EE_UNITS_ATC_CALL_REMOVE_PLANE, inAirport, inUnit);
}

// FUNCTION: EXE 0x54fcf4
long EEAirTrafficControl::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_REPAIR_POINTERS, NULL);
}

// FUNCTION: EXE 0x54fead
// Writes the airports, their queued planes (unit id and request) and the air cargo totals; returns the first error
long EEAirTrafficControl::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    return ee_units_atc_save(this->mRust, inFile, &EEATCFile, &EEATCUnitID);
}

// FUNCTION: EXE 0x55014c
EEComplexUnit* EEAirTrafficControl::FindNearestAirport(EEComplexUnit* inUnit)
{
    return reinterpret_cast<EEComplexUnit*>((intptr_t)this->Call(EE_UNITS_ATC_CALL_FIND_NEAREST_AIRPORT, inUnit));
}

// FUNCTION: EXE 0x5501d4
void EEAirTrafficControl::Update() { this->Call(EE_UNITS_ATC_CALL_UPDATE, NULL); }

// The original's BlocksTakeoffs (ICF-folded into 0x4bb118), UpdateLandings 0x550277 and UpdateTakeoffs 0x550487:
// atc.rs update_landings and update_takeoffs. The std::vector / std::deque / sort COMDATs of the original's members
// (0x54ea04, 0x550549..0x551be8) are gone with them.
