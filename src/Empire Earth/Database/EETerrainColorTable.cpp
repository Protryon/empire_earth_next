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

int64_t EECore10OpC(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kSurfaceSystemMemory == 4 && kPixelFormatA8R8G8B8 == 0 && kImageFilePNG == 4 &&
              sizeof(GESurfaceDescription) == 9 * sizeof(unsigned long) &&
              offsetof(GESurfaceDescription, mIs3DRenderable) == 8 * sizeof(unsigned long), "gamecpp db_media.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define SURF ((GESurface*)p)

// the colour table's C++ calls (gamecpp db_media.rs; p the surface or the colour): one statement each; the end of
// lane B's chain
int64_t EECore10OpB9(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DcCreateSurface: return gRasterizer->GetScreen(gScreenIndex)->CreateSurface(*(GESurfaceDescription*)a[0], (GESurface**)a[1]);
    case O::DcVerify: ESVERIFY_AT(a[0] != 0, a[1]);
        break;
    case O::DcFindFile: return gTextureManager->GetTexturePath()->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::DcSurfaceLoad: return SURF->Load(*(FSFileSpec*)a[0], kImageFilePNG);
    case O::DcLock: SURF->Lock();
        break;
    case O::DcUnlock: SURF->Unlock();
        break;
    case O::DcDereference: SURF->Dereference();
        break;
    case O::DcMemory: return P64(SURF->GetMemoryAddress());
    case O::DcBytesPerRow: return (int64_t)SURF->GetBytesPerRow();
    case O::DcWidth: return (int64_t)SURF->GetWidth();
    case O::DcHeight: return (int64_t)SURF->GetHeight();
    case O::DcNew: return P64(new EETerrainColorTable);
    case O::DcDelete: delete (EETerrainColorTable*)p;
        break;
    case O::DcSetColor: ((EETerrainColorTable*)p)->mColor = GEColor(f[0], f[1], f[2]);
        break;
    case O::DcResize: EETerrainColorTable::sTable.resize((unsigned long)a[0]);
        break;
    case O::DcCount: return (int64_t)EETerrainColorTable::sTable.size();
    case O::DcAt: return P64(EETerrainColorTable::sTable[a[0]]);
    case O::DcSet: EETerrainColorTable::sTable[a[0]] = (EETerrainColorTable*)a[1];
        break;
    default: return EECore10OpC(inOp, p, a, f);
    }
    return 0;
}
