// C:\EEX\Empire Earth\Scenario Manager\EESMArea.cpp (file name inferred: no __FILE__): 0x1ba02f..0x1ba89f,
// one scenario area. Unit flags: the exe module set. The numbers are the Rust record EeScenarioAreaFields and every
// body is Rust's (ee_scenario area_fields.rs, entry.rs over area.rs; the simulation capture's SMAR records with them).
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EESMArea.h"

// Returns the area's centre tile, computing and caching it once: rectangle centre, continent centre, or the hull
// centre of the town holding the anchor (else the anchor)
U2DPoint<long> EESMArea::GetCenterTile()
{
    U2DPoint<long> theResult;
    ee_scenario_en_area_center_tile(this, &theResult.mX);
    return theResult;
}
