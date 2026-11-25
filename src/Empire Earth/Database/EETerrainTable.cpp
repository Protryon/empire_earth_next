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



// Creates an empty terrain: no textures, not water, colour type -1 (ee_data tables.rs; the C++ members' initializers)
EETerrainTable::EETerrainTable() : EeDataMediaUse()
{
    ee_data_terrain_default(this);
}

// Loads the terrains from db\dbterrain.dat (each record after its texture file name) numbering their textures, and
// notes the first named one as the default terrain; returns the last read status
long EETerrainTable::Load(FSFileSpec* inFile)
{
    return ee_gamecpp_db_terrain_load(inFile);
}

// Releases every terrain's textures, destroys the terrains and empties the table (the texture arrays are not freed)
void EETerrainTable::Unload()
{
    ee_gamecpp_db_terrain_unload();
}

// Writes each referenced terrain and each terrain with a list-box preview to a media dump file
void EETerrainTable::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    ee_gamecpp_db_terrain_dump(&inFile);
}

#include "ee/EEUnit.h"

// Adds a reference to the terrain's textures; the first one loads them and attaches them (columns mirrored) to the
// mesh's texture set, later callers wait for that load
void EETerrainTable::Load(GETerrainMesh& ioMesh)
{
    ee_gamecpp_db_terrain_mesh(this, &ioMesh, true);
}

// Loads one terrain texture and sets its terrain stage modes (clamped; linear mip filter when trilinear is on)
// No NULL test: the original references the result directly
GETexture* EETerrainTable::LoadTexture(const UString& inFile)
{
    return (GETexture*)ee_gamecpp_db_terrain_texture(this, (const char*)inFile);
}

// Loads every texture of the terrain's grid (its file name numbered _n, from the last down) into its texture array
void EETerrainTable::LoadTextures()
{
    ee_gamecpp_db_terrain_textures(this);
}

// Loads each terrain's list-box preview texture (its file name with _1) and references the button of its first unit
// class (the terrain list of EEUIFSETerrain)
void EELoadTerrainPreviews()
{
    ee_gamecpp_db_terrain_previews(true);
}

// Attaches the terrain's loaded textures (columns mirrored) to a texture set, once any load has finished
void EETerrainTable::AttachTextures(GETextureSet* inTextureSet)
{
    ee_gamecpp_db_terrain_attach(this, inTextureSet);
}

// Drops a reference to the terrain's textures; the last one detaches them from the mesh and releases them
void EETerrainTable::Unload(GETerrainMesh& ioMesh)
{
    ee_gamecpp_db_terrain_mesh(this, &ioMesh, false);
}

// Releases each terrain's preview texture and its first unit class's button reference
void EEReleaseTerrainPreviews()
{
    ee_gamecpp_db_terrain_previews(false);
}

// Loads the cliff terrains from db\dbcliffterrain.dat (empty slots for negative indices) and notes the first one;
// returns the last read status
long EECliffTerrainTable::Load(FSFileSpec* inFile)
{
    return ee_gamecpp_db_cliff_load(inFile);
}

// Destroys every cliff terrain and empties the cliff table; returns 0
long EECliffTerrainTable::Unload()
{
    return ee_gamecpp_db_cliff_unload();
}

// Returns the terrain at an index, or NULL when out of range or unused (negative id)
EETerrainTable* EETerrainTable::Get(long inIndex)
{
    if ((unsigned long)inIndex < EETerrainTable::sTable.size() && EETerrainTable::sTable[inIndex]->mID >= 0)
        return EETerrainTable::sTable[inIndex];
    return NULL;
}

// Destroys the terrain (members only)
EETerrainTable::~EETerrainTable()
{
}

// ------------------------------------------------------------------------- round 10, lane B: the terrain tables' C++


static_assert(kStageOpModulate == 2 && kAddressingClamp == 0 && kMipFilterNone == 0 && kMipFilterPoint == 1 &&
              kMipFilterLinear == 2, "gamecpp db_media.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define TT ((EETerrainTable*)p)
#define CLIFF ((EECliffTerrainTable*)p)

