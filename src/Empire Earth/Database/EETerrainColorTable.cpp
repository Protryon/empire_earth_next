// C:\EEX\Empire Earth\Database\EETerrainColorTable.cpp (__FILE__ 0x4bcd50). The terrain colour palette:
// 0xdae63..0xdb169 (the class static table's init chain first; EEStartingResources ends at 0xdae63, the next unit's
// chain starts at 0xdb169). Unit flags: the exe's module set. Data: the vtable 0x43f7d8, the scale 0x43f7dc.
// port (game core lane, round 10): the load and the unload are Rust (ee_game gamecpp db_media.rs); their C++ calls
// are EECore10OpB9's cases (the asserts keep this file's name and the original's lines).
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EmpireEarth.h"
#include "ee/EETerrainColorTable.h"
#include "rust/game/gamecpp.h"

std::vector<EETerrainColorTable*> EETerrainColorTable::sTable;

// Creates an empty terrain colour entry
EETerrainColorTable::EETerrainColorTable()
{
}

// Loads the terrain colours from the pixels of textures\ColorPalette.png (16 per row; each component / 255); returns
// the texture load status
long EETerrainColorTable::Load()
{
    return ee_gamecpp_db_colors_load();
}

// Destroys every terrain colour and empties the table
void EETerrainColorTable::Unload()
{
    ee_gamecpp_db_colors_unload();
}

// ------------------------------------------------------------------------- round 10, lane B: the colours' C++


static_assert(kSurfaceSystemMemory == 4 && kPixelFormatA8R8G8B8 == 0 && kImageFilePNG == 4 &&
              sizeof(GESurfaceDescription) == 9 * sizeof(unsigned long) &&
              offsetof(GESurfaceDescription, mIs3DRenderable) == 8 * sizeof(unsigned long), "gamecpp db_media.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define SURF ((GESurface*)p)

