// C:\EEX\Empire Earth\Database\EEDbObject.cpp (name from RTTI EEDbObject; no __FILE__). 0xd61f7..0xd68c7: the 34 db
// file name keys (one init chain each, in definition order; EEDbObject::Load went: the records are ee_data's). EEDatabase.cpp ends at 0xd61f7
// (EEDbObject's ??_G), EEGameVariant.cpp starts at 0xd68c7. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/game/gamecpp.h"   // round 10 (ee_game gamecpp)

#include "ee/EEDbObject.h"

const UString kDbMusicFile("db\\dbmusic.dat");
const UString kDbObjectsFile("db\\dbobjects.dat");
const UString kDbGraphicsFile("db\\dbgraphics.dat");
const UString kDbSoundsFile("db\\dbsounds.dat");
const UString kDbButtonsFile("db\\dbButtons.dat");
const UString kDbTechTreeFile("db\\dbtechtree.dat");
const UString kDbEffectsFile("db\\dbeffects.dat");
const UString kDbEventsFile("db\\dbevents.dat");
const UString kDbFamilyFile("db\\dbfamily.dat");
const UString kDbColorTableFile("db\\dbcolortable.dat");
const UString kDbAIUnitTargetingFile("db\\dbaiunittargeting.dat");
const UString kDbTerrainFile("db\\dbterrain.dat");
const UString kDbTerrainColorFile("db\\dbterrainColor.dat");
const UString kDbTerrainTypeFile("db\\dbterrainType.dat");
const UString kDbWorldFile("db\\dbworld.dat");
const UString kDbCivilizationFile("db\\dbcivilization.dat");
const UString kDbGfxEffectsFile("db\\dbgfxeffects.dat");
const UString kDbWeaponToHitFile("db\\dbweapontohit.dat");
const UString kDbCalamityFile("db\\dbcalamity.dat");
const UString kDbUnitSetFile("db\\dbunitset.dat");
const UString kDbAreaEffectFile("db\\dbareaeffect.dat");
const UString kDbFontsFile("db\\dbfonts.dat");
const UString kDbUpgradeFile("db\\dbupgrade.dat");
const UString kDbCPBehaviorFile("db\\dbcpbehavior.dat");
const UString kDbAmbientSoundsFile("db\\dbambientsounds.dat");
const UString kDbAnimalsFile("db\\dbAnimals.dat");
const UString kDbUnitBehaviorFile("db\\dbUnitBehavior.dat");
const UString kDbStartingResourcesFile("db\\dbStartingResources.dat");
const UString kDbGameVariantFile("db\\dbGameVariant.dat");
const UString kDbPremadeCivsFile("db\\dbpremadecivs.dat");
const UString kDbCliffTerrainFile("db\\dbcliffterrain.dat");
const UString kDbCivPowersFile("db\\dbcivpowers.dat");

// ------------------------------------------------------------------------- round 10, lane B: the Database, the Tech Tree, Persistent

double EECore10OpCF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EECore10OpB2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETerrainTable.cpp

// the C++ calls and members of lane B's bodies in Rust (gamecpp db_media.rs, db_text.rs, tech_load.rs, tech_capture.rs, persist_more.rs;
// EeGameCpp's round 10 lane B section) are the cases of a chain of switches beside the statics they read:
// EETerrainTable.cpp (B2) -> EEButtonObject.cpp (B3) -> EEAnimal.cpp (B4) -> EEDataTextFile.cpp (B5) ->
// EETechCapture.cpp (B6) -> EETechTreeEpoch.cpp (B7) -> EEPCalamityHelper.cpp (B8) -> EETerrainColorTable.cpp (B9) -> lane C
int64_t EECore10OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpB2(inOp, p, a, f);
}

double EECore10OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpCF(inOp, p, a, f);
}
