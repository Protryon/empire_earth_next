// C:\EEX\Empire Earth\Scenario Manager\EESMArea.cpp (file name inferred: no __FILE__): 0x1ba02f..0x1ba89f,
// one scenario area. Unit flags: the exe module set. The numbers are the Rust record EeScenarioAreaFields and every
// body is Rust's (ee_scenario area_fields.rs, entry.rs over area.rs; the simulation capture's SMAR records with them).
#include <windows.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EESMArea.h"
#include "ee/EEScenarioHost.h"

// Creates an unnamed rectangle-mode area with no rectangle or anchor and no cached centre tile
EESMArea::EESMArea() : EeScenarioAreaFields(ee_scenario_area_defaults()) {}

// Destroys the area (members only)
EESMArea::~EESMArea()
{
}

// Returns whether the unit is in the area (no anchor: the unit is off the map; negative anchor: it has no complex
// unit or that stands on the unit's tile; else by tile); false for no unit
bool EESMArea::ContainsUnit(EEUnit* inUnit)
{
    return ee_scenario_en_area_contains_unit(this, inUnit);
}

// Returns whether a tile is in the area: inside the rectangle, on the anchor's continent, or inside the hull of a
// non-Gaia town that contains the anchor
bool EESMArea::ContainsTile(const U2DPoint<long>& inTile)
{
    return ee_scenario_en_area_contains_tile(this, inTile.mX, inTile.mY);
}

// Copies another area's name, text, rectangle, anchor, select mode and ID (not its tile cache)
void EESMArea::Copy(const EESMArea& inArea)
{
    ee_scenario_en_area_copy(this, (void*)&inArea);
}

// Returns the area's centre tile, computing and caching it once: rectangle centre, continent centre, or the hull
// centre of the town holding the anchor (else the anchor)
U2DPoint<long> EESMArea::GetCenterTile()
{
    U2DPoint<long> theResult;
    ee_scenario_en_area_center_tile(this, &theResult.mX);
    return theResult;
}

// Copies the area's tiles out, rebuilding the cache when empty or unused for 3000 ticks; every cached read
// restarts the timer
void EESMArea::GetTiles(std::vector<U2DPoint<long> >& outTiles)
{
    ee_scenario_en_area_tiles(this, &outTiles);
}

// Reads the area's name, text, anchor (as floats), rectangle, flags, select mode, tile time and ID; returns the first
// error (ee_scenario save.rs area_load)
long EESMArea::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_scenario_en_area_io(this, &inFile, false);
}

// Writes the area's name, text, anchor, rectangle, flags, select mode, tile time and ID; returns the first error
long EESMArea::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_scenario_en_area_io(this, &outFile, true);
}
