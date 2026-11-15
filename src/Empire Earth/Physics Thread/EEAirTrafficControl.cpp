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

// Creates the player's air traffic control with no airports and no air cargo capacity
EEAirTrafficControl::EEAirTrafficControl(EEPlayer* inPlayer)
    : mPlayer(inPlayer), mRust(ee_units_atc_new())
{
}

// Destroys the air traffic control, releasing every queued plane and every airport
EEAirTrafficControl::~EEAirTrafficControl()
{
    ee_units_atc_free(this->mRust, NULL, &EEATCHost);
}

void EEAirTrafficControl::AddCargoWeight(EEComplexUnit* inUnit) { this->Call(EE_UNITS_ATC_CALL_ADD_CARGO_WEIGHT, inUnit); }

void EEAirTrafficControl::AddAirport(EEComplexUnit* inAirport) { this->Call(EE_UNITS_ATC_CALL_ADD_AIRPORT, inAirport); }

long EEAirTrafficControl::QueueRequest(EEComplexUnit* inAirport, EEComplexUnit* inUnit, long inKind)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_QUEUE_REQUEST, inAirport, inUnit, inKind);
}

long EEAirTrafficControl::GetFreeCargoWeight(EEComplexUnit* inAirport)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_FREE_CARGO_WEIGHT, inAirport);
}

bool EEAirTrafficControl::RemoveFromTakeoffQueues(EEComplexUnit* inUnit)
{
    return this->Call(EE_UNITS_ATC_CALL_REMOVE_FROM_TAKEOFF_QUEUES, inUnit) != 0;
}

bool EEAirTrafficControl::ClearAirportQueue(EEComplexUnit* inAirport, long inKind)
{
    return this->Call(EE_UNITS_ATC_CALL_CLEAR_AIRPORT_QUEUE, inAirport, NULL, inKind) != 0;
}

// Writes the air cargo totals and every airport's landing and takeoff queues to the out-of-sync dump
void EEAirTrafficControl::Dump(FILE* inFile)
{
    ee_units_atc_call(this->mRust, inFile, &EEATCHost, EE_UNITS_ATC_CALL_DUMP, NULL, NULL, 0);
}

// Returns the squared distance in tiles between the two units (atc.rs tile_distance_squared for the air traffic)
float EEUnit::GetTileDistanceSquared(EEUnit* inOther)
{
    long theDeltaY = this->mTileY - inOther->mTileY;
    long theDeltaX = this->mTileX - inOther->mTileX;
    return (float)(theDeltaX * theDeltaX + theDeltaY * theDeltaY);
}

long EEAirTrafficControl::GetChecksum() { return (long)this->Call(EE_UNITS_ATC_CALL_CHECKSUM, NULL); }

unsigned long EEAirTrafficControl::GetQueueLength(EEComplexUnit* inAirport)
{
    return (unsigned long)this->Call(EE_UNITS_ATC_CALL_QUEUE_LENGTH, inAirport);
}

long EEAirTrafficControl::GetQueuePosition(EEComplexUnit* inUnit, long inWhich)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_QUEUE_POSITION, inUnit, NULL, inWhich);
}

// The file's primitives for Load and Save (EESavePrimitive) and a unit's id for Save
static int32_t EEATCFile(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    return EESavePrimitive(*static_cast<FSFile*>(inFile), inOp, io);
}

static unsigned long EEATCUnitID(void* inUnit)
{
    return static_cast<EEComplexUnit*>(inUnit)->GetUnitID();
}

// Reads the airports, their queued planes (as unit ids) and the air cargo totals; returns the first read error
long EEAirTrafficControl::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    return ee_units_atc_load(this->mRust, inFile, &EEATCFile);
}

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

void EEAirTrafficControl::RemoveCargoWeight(EEComplexUnit* inUnit) { this->Call(EE_UNITS_ATC_CALL_REMOVE_CARGO_WEIGHT, inUnit); }

void EEAirTrafficControl::RemoveAirport(EEComplexUnit* inAirport) { this->Call(EE_UNITS_ATC_CALL_REMOVE_AIRPORT, inAirport); }

void EEAirTrafficControl::RemovePlane(EEComplexUnit* inAirport, EEComplexUnit* inUnit)
{
    this->Call(EE_UNITS_ATC_CALL_REMOVE_PLANE, inAirport, inUnit);
}

long EEAirTrafficControl::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return (long)this->Call(EE_UNITS_ATC_CALL_REPAIR_POINTERS, NULL);
}

// Writes the airports, their queued planes (unit id and request) and the air cargo totals; returns the first error
long EEAirTrafficControl::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    return ee_units_atc_save(this->mRust, inFile, &EEATCFile, &EEATCUnitID);
}

EEComplexUnit* EEAirTrafficControl::FindNearestAirport(EEComplexUnit* inUnit)
{
    return reinterpret_cast<EEComplexUnit*>((intptr_t)this->Call(EE_UNITS_ATC_CALL_FIND_NEAREST_AIRPORT, inUnit));
}

void EEAirTrafficControl::Update() { this->Call(EE_UNITS_ATC_CALL_UPDATE, NULL); }

// The original's BlocksTakeoffs (ICF-folded into 0x4bb118), UpdateLandings 0x550277 and UpdateTakeoffs 0x550487:
// atc.rs update_landings and update_takeoffs. The std::vector / std::deque / sort COMDATs of the original's members
// (0x54ea04, 0x550549..0x551be8) are gone with them.
