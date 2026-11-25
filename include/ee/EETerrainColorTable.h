// C:\EEX\Empire Earth\Database\EETerrainColorTable.h (RECONSTRUCTION; layout proven: EETerrainColorTable sizeof 0x10 =
// its new-site size, ctor 0xdaeac exact in EETerrainColorTable.cpp; names are provisional). One
// colour of the terrain palette (textures\ColorPalette.png) and the static table of them. RTTI EETerrainColorTable
// (no base), vtable 0x43f7d8 (one slot: the deleting dtor 0xdb14c); sizeof 0x10 (Load's `new`). Defined by
// EETerrainColorTable.cpp 0xdae63..0xdb169. Requires <vector>, lle/GEColor.h.
#ifndef EE_EETERRAINCOLORTABLE_H
#define EE_EETERRAINCOLORTABLE_H

#include "rust/game/gamecpp.h"
class EETerrainColorTable {
public:
    // Creates an empty terrain colour entry
    EETerrainColorTable() {  }
    virtual ~EETerrainColorTable() {}                           // ??_G 0xdb14c (stores the vptr)

    // Loads the terrain colours from the pixels of textures\ColorPalette.png (16 per row; each component / 255); returns
    // the texture load status
    static long Load() { return ee_gamecpp_db_colors_load(); }
    // Destroys every terrain colour and empties the table
    static void Unload() { ee_gamecpp_db_colors_unload(); }

    GEColor mColor;                                             // +0x04 (the ctor's three fld1 are GEColor's
                                                                // inline default ctor: the vptr store follows)

    static std::vector<EETerrainColorTable*> sTable;            // 0x8e86a8 (guard byte 0x8e86a0 bit 1)
    // inline (out-of-line COMDATs 0x11e208 / 0x11e1f0 after their first user, EEMap::Fn11DD95) (provisional names)
    static EETerrainColorTable* Get(unsigned long inIndex) { return inIndex < EETerrainColorTable::sTable.size() ? EETerrainColorTable::sTable[inIndex] : NULL; }
    static unsigned long GetCount() { return EETerrainColorTable::sTable.size(); }
};

#endif
