// C:\EEX\Empire Earth\Database\EETerrainTable.cpp (name from RTTI EETerrainTable; no __FILE__). 0xdb169..0xdbcc8:
// the two class static tables' init chains, the header lock's chain (Database\EEDatabase.h), the ctor, Load
// (db\dbterrain.dat), Unload, DumpAllLoadedMedia, the mesh functions 0xdb5c8..0xdba99, EECliffTerrainTable (ctor,
// Load, Unload), Get, the ??_Gs and the dtor. EETerrainColorTable.cpp ends at 0xdb169, EETerrainTypeTable.cpp's
// chain starts at 0xdbcc8. Unit flags: the exe's module set.
// port (game core lane, round 10): the tables' bodies are Rust (ee_game gamecpp db_media.rs: the loads, the textures'
// use count, the previews); the functions below forward, their C++ calls and members are EECore10OpB2's cases.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <utility>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETexture.h"

#include "ee/EEDatabaseTables.h"
#include "rust/game/gamecpp.h"

std::vector<EETerrainTable*> EETerrainTable::sTable;
std::vector<EECliffTerrainTable*> EETerrainTable::sCliffTable;

#include <EEDatabase.h>

// 0x4e86d0: the first loaded terrain's +0x50 (-1 until one with a name loads); 0x4e86e8: the first cliff index
// (provisional names)
extern unsigned long& gTexturePixelDepth;
// 0x5308b0: the terrain textures' mip levels to drop (LoadTexture; provisional name)
extern unsigned long& gTerrainMIPLevelsToDrop;
extern bool& gTrilinearFilter;               // 0x5308da (mip filtering on)
extern GETextureManager* gTextureManager;  // 0x531918 (ee/EmpireEarth.h)

long gDefaultTerrain;
long gFirstCliffTerrain;



#include "lle/GETerrainMesh.h"
#include "lle/GETextureSet.h"
#include "ee/EEUnit.h"

// Loads each terrain's list-box preview texture (its file name with _1) and references the button of its first unit
// class (the terrain list of EEUIFSETerrain)
void EELoadTerrainPreviews()
{
    ee_gamecpp_db_terrain_previews(true);
}

// Releases each terrain's preview texture and its first unit class's button reference
void EEReleaseTerrainPreviews()
{
    ee_gamecpp_db_terrain_previews(false);
}

// Returns the terrain at an index, or NULL when out of range or unused (negative id)
EETerrainTable* EETerrainTable::Get(long inIndex)
{
    if ((unsigned long)inIndex < EETerrainTable::sTable.size() && EETerrainTable::sTable[inIndex]->mID >= 0)
        return EETerrainTable::sTable[inIndex];
    return NULL;
}

// ------------------------------------------------------------------------- round 10, lane B: the terrain tables' C++


static_assert(kStageOpModulate == 2 && kAddressingClamp == 0 && kMipFilterNone == 0 && kMipFilterPoint == 1 &&
              kMipFilterLinear == 2, "gamecpp db_media.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define TT ((EETerrainTable*)p)
#define CLIFF ((EECliffTerrainTable*)p)

