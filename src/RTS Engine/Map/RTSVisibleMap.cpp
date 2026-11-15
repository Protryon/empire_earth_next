// C:\EEX\RTS Engine\Map\RTSVisibleMap.cpp (no __FILE__). Only the class's six save-key chains are in the exe
// (0x1248f6..0x124a88); the unit's functions were not linked. EEAmbientSound.cpp's header-scope caller-log chains
// (deleted) opened the next unit at 0x124a88. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/RTSVisibleMap.h"

// chain at 0x5248f6 (wrapper)
//   object 0x9298e8  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ (guard 0x9298e0 bit 1)
const UString RTSVisibleMap::sTileHeight("RTSVisibleMap-TileHeight");

// chain at 0x524939 (wrapper)
//   object 0x9298d0  (guard 0x9298e0 bit 2)
const UString RTSVisibleMap::sTileWidth("RTSVisibleMap-TileWidth");

// chain at 0x52497c (wrapper)
//   object 0x9298c0  (guard 0x9298e0 bit 4)
const UString RTSVisibleMap::sTileData("RTSVisibleMap-TileData");

// chain at 0x5249bf (wrapper)
//   object 0x9298b0  (guard 0x9298e0 bit 8)
const UString RTSVisibleMap::sAllLOS("RTSVisibleMap-AllLOS");

// chain at 0x524a02 (wrapper)
//   object 0x9298a0  (guard 0x9298e0 bit 0x10)
const UString RTSVisibleMap::sMapCreated("RTSVisibleMap-MapCreated");

// chain at 0x524a45 (wrapper)
//   object 0x929890  (guard 0x9298e0 bit 0x20)
const UString RTSVisibleMap::sNumTilesSaved("RTSVisibleMap-NumTilesSaved");

// 0x52988c: written by RTSMap's ctor, read by EETile::Virtual14 (code xrefs 0x121522, 0x121e17). Placed here by the .bss layout: RTSTile's
// .bss is 0x529850..0x529888 (three UStrings + guard, 8-aligned), this unit's statics run 0x529890..0x5298f8 in reverse
// definition order, so 0x529888/0x52988c are this unit's last definitions (0x529888 has no reader) (provisional name)
float gMapTileSize;
