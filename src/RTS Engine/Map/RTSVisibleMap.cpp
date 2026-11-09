// C:\EEX\RTS Engine\Map\RTSVisibleMap.cpp (no __FILE__). Only the class's six save-key chains are in the exe
// (0x1248f6..0x124a88); the unit's functions were not linked. EEAmbientSound.cpp's header-scope ESCallerLog chains
// ("UMCos EE-Level", "UMSin EE-Level") open the next unit at 0x124a88. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/RTSVisibleMap.h"

// chain at 0x5248f6 (wrapper)
//   object 0x9298e8  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ (guard 0x9298e0 bit 1)
const UString RTSVisibleMap::sTileHeight("RTSVisibleMap-TileHeight");
// SYNTHETIC: EXE 0x5248f6   (wrapper)
// SYNTHETIC: EXE 0x524900   (ctor)
// SYNTHETIC: EXE 0x524911   (registrar)
// SYNTHETIC: EXE 0x52491d   (dtor)

// chain at 0x524939 (wrapper)
//   object 0x9298d0  (guard 0x9298e0 bit 2)
const UString RTSVisibleMap::sTileWidth("RTSVisibleMap-TileWidth");
// SYNTHETIC: EXE 0x524939   (wrapper)
// SYNTHETIC: EXE 0x524943   (ctor)
// SYNTHETIC: EXE 0x524954   (registrar)
// SYNTHETIC: EXE 0x524960   (dtor)

// chain at 0x52497c (wrapper)
//   object 0x9298c0  (guard 0x9298e0 bit 4)
const UString RTSVisibleMap::sTileData("RTSVisibleMap-TileData");
// SYNTHETIC: EXE 0x52497c   (wrapper)
// SYNTHETIC: EXE 0x524986   (ctor)
// SYNTHETIC: EXE 0x524997   (registrar)
// SYNTHETIC: EXE 0x5249a3   (dtor)

// chain at 0x5249bf (wrapper)
//   object 0x9298b0  (guard 0x9298e0 bit 8)
const UString RTSVisibleMap::sAllLOS("RTSVisibleMap-AllLOS");
// SYNTHETIC: EXE 0x5249bf   (wrapper)
// SYNTHETIC: EXE 0x5249c9   (ctor)
// SYNTHETIC: EXE 0x5249da   (registrar)
// SYNTHETIC: EXE 0x5249e6   (dtor)

// chain at 0x524a02 (wrapper)
//   object 0x9298a0  (guard 0x9298e0 bit 0x10)
const UString RTSVisibleMap::sMapCreated("RTSVisibleMap-MapCreated");
// SYNTHETIC: EXE 0x524a02   (wrapper)
// SYNTHETIC: EXE 0x524a0c   (ctor)
// SYNTHETIC: EXE 0x524a1d   (registrar)
// SYNTHETIC: EXE 0x524a29   (dtor)

// chain at 0x524a45 (wrapper)
//   object 0x929890  (guard 0x9298e0 bit 0x20)
const UString RTSVisibleMap::sNumTilesSaved("RTSVisibleMap-NumTilesSaved");

// 0x52988c: written by RTSMap's ctor, read by EETile::Virtual14 (code xrefs 0x121522, 0x121e17). Placed here by the .bss layout: RTSTile's
// .bss is 0x529850..0x529888 (three UStrings + guard, 8-aligned), this unit's statics run 0x529890..0x5298f8 in reverse
// definition order, so 0x529888/0x52988c are this unit's last definitions (0x529888 has no reader) (provisional name)
float gMapTileSize;
// SYNTHETIC: EXE 0x524a45   (wrapper)
// SYNTHETIC: EXE 0x524a4f   (ctor)
// SYNTHETIC: EXE 0x524a60   (registrar)
// SYNTHETIC: EXE 0x524a6c   (dtor)
