// New (port), TEST ONLY: the app and world lane's records of the map, the mini map and the statistics (ee/EEAppCapture.h;
// tools/sim_capture.py). Reads the objects; the sweeps (EE-SIM --app-sweep: ee_game gamecpp cheats.rs, weather.rs)
// play cheat lines and start storms as a player would.
#include <windows.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEPhysics.h"
#include "ee/EECheatCodes.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEStormType.h"
#include "rust/render/ge.h"
#include "rust/game/app.h"   // the map records
#include "rust/sim/units.h"
#include "rust/game/gamecpp.h"   // round 10, lane D: the storm and cheat sweeps; round 11, lane B: the statistics
#include "ee/EETechCapture.h"
#include "ee/EEAppCapture.h"

// MAPS, MAPC: Rust (ee_game app map_capture.rs)
static void EEAppCaptureSightImpl(unsigned int inKind, EEPlayer* inPlayer, float inX, float inY, float inRadius,
                                  bool inShared)
{
    if (EESimCaptureRecord::Active() && gWorld.mMap != NULL)
        ee_app_capture_sight(inKind, inPlayer != NULL ? (uint32_t)inPlayer->mPlayerID : 0xffffffffu, inX, inY, inRadius,
                             inShared, gWorld.mMap->mTerrainMesh.GetRust());
}

// EEAppMapScope's construction (whether it is the outermost) and destruction (the record)
static bool AppMapScopeEnter()
{
    return ee_app_capture_map_enter();
}

static void AppMapScopeLeave(EEMap* inMap, bool inOuter, unsigned int inKind, const long* inArgs, const long* inRect)
{
    ee_app_capture_map_leave(inOuter, (uint64_t)(uintptr_t)inMap, inMap != NULL ? inMap->mTerrainMesh.GetRust() : NULL,
                             inKind, inArgs, inRect);
}

// STRM, the storm sweep and the cheat sweep's C++ calls (CHET, CHEF): ee_game gamecpp weather.rs, cheats.rs

// WSTA, the statistics sweep (EEWorld::RecordStatistics, the end-of-game scores): ee_game gamecpp stats_capture.rs

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EEAppMapScopeEnter = AppMapScopeEnter, EEAppMapScopeLeave = AppMapScopeLeave,
                                      EEAppCaptureCheatSweep = ee_gamecpp_capture_cheat_sweep,
                                      EEAppCaptureSight = EEAppCaptureSightImpl,
                                      EEAppCaptureStatisticsSweep = ee_gamecpp_capture_statistics_sweep,
                                      EEAppCaptureStormSweep = ee_gamecpp_capture_storm_sweep, true);
