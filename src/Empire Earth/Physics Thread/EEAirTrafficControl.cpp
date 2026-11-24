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

// port (the units' state lane, round 1): the class is Rust's whole (ee_units atc.rs: the bodies over this host;
// atc_save.rs: Load and Save; atc_views.rs: the units' plain reads in place). The host is the units' world calls; the
// Rust state is borrowed anew around each, as a call here may come back into the same control (a plane's Kill takes
// its weight off the used cargo; a rerouted plane's ActionChanged queues it at its new airport).
static int64_t EEATCHost(void* inContext, int32_t inOp, void* inUnit, void* inOther, EeUnitsAtcIo* io)
{
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = static_cast<EEComplexUnit*>(inOther);
    switch (inOp) {
    case EE_UNITS_ATC_REFERENCE: theUnit->Reference(); return 0;
    case EE_UNITS_ATC_DEREFERENCE: theUnit->Dereference(); return 0;
    case EE_UNITS_ATC_SET_CLEARANCE: theUnit->SetB342(io->a != 0); return 0;
    case EE_UNITS_ATC_FIND_PASSENGER: return (int64_t)(intptr_t)theUnit->FindPassenger(io->a);
    case EE_UNITS_ATC_CLEAR_EXITING: theUnit->SetExitingUnitID(-1); return 0;
    case EE_UNITS_ATC_KILL: theUnit->Kill(1, 0, 0); return 0;
    case EE_UNITS_ATC_SET_HOME_BASE: theUnit->SetHomeBase(theOther); return 0;
    case EE_UNITS_ATC_REQUEST_LANDING: theUnit->StartAction(kActionRequestLanding); return 0;
    case EE_UNITS_ATC_ACTION_CHANGED: theUnit->GetUnitClass()->ActionChanged(theUnit); return 0;
    case EE_UNITS_ATC_RESOLVE: return (int64_t)(intptr_t)EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((uint32_t)io->a));
    case EE_UNITS_ATC_DUMP_LINE: fputs(static_cast<const char*>(inUnit), static_cast<FILE*>(inContext)); return 0;
    }
    return 0;
}

// Creates a player's air traffic control with no airports and no air cargo capacity
EeUnitsAtc* EEATCNew()
{
    return ee_units_atc_new(&EEATCHost);
}

// Returns the squared distance in tiles between the two units (atc.rs tile_distance_squared)

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
long EEATCLoad(EeUnitsAtc* inATC, FSFile* inFile)
{
    return ee_units_atc_load(inATC, inFile, &EEATCFile);
}

// Writes the airports, their queued planes (unit id and request) and the air cargo totals; returns the first error
long EEATCSave(EeUnitsAtc* inATC, FSFile* inFile)
{
    return ee_units_atc_save(inATC, inFile, &EEATCFile, &EEATCUnitID);
}

// Hands replaced aircraft's cargo weight and queue entries to their replacements
void EEATCReplaceUnits(EeUnitsAtc* inATC, std::vector<EEPUnitSwap>& inSwaps)
{
    ee_units_atc_replace(inATC, reinterpret_cast<const EeAStarUnit*>(inSwaps.data()), inSwaps.size());
}

// The original's BlocksTakeoffs (ICF-folded into 0x4bb118), UpdateLandings 0x550277 and UpdateTakeoffs 0x550487:
// atc.rs update_landings and update_takeoffs. The std::vector / std::deque / sort COMDATs of the original's members
// (0x54ea04, 0x550549..0x551be8) are gone with them.
