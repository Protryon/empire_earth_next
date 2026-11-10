// C:\EEX\Empire Earth\Database\EEDbObject.cpp (name from RTTI EEDbObject; no __FILE__). 0xd61f7..0xd68c7: the 34 db
// file name keys (one init chain each, in definition order), then EEDbObject::Load. EEDatabase.cpp ends at 0xd61f7
// (EEDbObject's ??_G), EEGameVariant.cpp starts at 0xd68c7. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"

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

// FUNCTION: EXE 0x4d689b
// Reads the record's raw bytes from a file; returns 9 when there is no file
long EEDbObject::Load(FSFileSpec* inFile)
{
    unsigned long theRead = 0;
    long theResult = 9;
    if (inFile != NULL)
        theResult = inFile->Read(this->mRecord, sizeof(this->mRecord), theRead);
    return theResult;
}

// SYNTHETIC: EXE 0x4d61f7
// SYNTHETIC: EXE 0x4d6201
// SYNTHETIC: EXE 0x4d6212
// SYNTHETIC: EXE 0x4d621e
// SYNTHETIC: EXE 0x4d6229
// SYNTHETIC: EXE 0x4d6233
// SYNTHETIC: EXE 0x4d6244
// SYNTHETIC: EXE 0x4d6250
// SYNTHETIC: EXE 0x4d625b
// SYNTHETIC: EXE 0x4d6265
// SYNTHETIC: EXE 0x4d6276
// SYNTHETIC: EXE 0x4d6282
// SYNTHETIC: EXE 0x4d628d
// SYNTHETIC: EXE 0x4d6297
// SYNTHETIC: EXE 0x4d62a8
// SYNTHETIC: EXE 0x4d62b4
// SYNTHETIC: EXE 0x4d62bf
// SYNTHETIC: EXE 0x4d62c9
// SYNTHETIC: EXE 0x4d62da
// SYNTHETIC: EXE 0x4d62e6
// SYNTHETIC: EXE 0x4d62f1
// SYNTHETIC: EXE 0x4d62fb
// SYNTHETIC: EXE 0x4d630c
// SYNTHETIC: EXE 0x4d6318
// SYNTHETIC: EXE 0x4d6323
// SYNTHETIC: EXE 0x4d632d
// SYNTHETIC: EXE 0x4d633e
// SYNTHETIC: EXE 0x4d634a
// SYNTHETIC: EXE 0x4d6355
// SYNTHETIC: EXE 0x4d635f
// SYNTHETIC: EXE 0x4d6370
// SYNTHETIC: EXE 0x4d637c
// SYNTHETIC: EXE 0x4d6387
// SYNTHETIC: EXE 0x4d6391
// SYNTHETIC: EXE 0x4d63a2
// SYNTHETIC: EXE 0x4d63ae
// SYNTHETIC: EXE 0x4d63b9
// SYNTHETIC: EXE 0x4d63c3
// SYNTHETIC: EXE 0x4d63d4
// SYNTHETIC: EXE 0x4d63e0
// SYNTHETIC: EXE 0x4d63eb
// SYNTHETIC: EXE 0x4d63f5
// SYNTHETIC: EXE 0x4d6406
// SYNTHETIC: EXE 0x4d6412
// SYNTHETIC: EXE 0x4d641d
// SYNTHETIC: EXE 0x4d6427
// SYNTHETIC: EXE 0x4d6438
// SYNTHETIC: EXE 0x4d6444
// SYNTHETIC: EXE 0x4d644f
// SYNTHETIC: EXE 0x4d6459
// SYNTHETIC: EXE 0x4d646a
// SYNTHETIC: EXE 0x4d6476
// SYNTHETIC: EXE 0x4d6481
// SYNTHETIC: EXE 0x4d648b
// SYNTHETIC: EXE 0x4d649c
// SYNTHETIC: EXE 0x4d64a8
// SYNTHETIC: EXE 0x4d64b3
// SYNTHETIC: EXE 0x4d64bd
// SYNTHETIC: EXE 0x4d64ce
// SYNTHETIC: EXE 0x4d64da
// SYNTHETIC: EXE 0x4d64e5
// SYNTHETIC: EXE 0x4d64ef
// SYNTHETIC: EXE 0x4d6500
// SYNTHETIC: EXE 0x4d650c
// SYNTHETIC: EXE 0x4d6517
// SYNTHETIC: EXE 0x4d6521
// SYNTHETIC: EXE 0x4d6532
// SYNTHETIC: EXE 0x4d653e
// SYNTHETIC: EXE 0x4d6549
// SYNTHETIC: EXE 0x4d6553
// SYNTHETIC: EXE 0x4d6564
// SYNTHETIC: EXE 0x4d6570
// SYNTHETIC: EXE 0x4d657b
// SYNTHETIC: EXE 0x4d6585
// SYNTHETIC: EXE 0x4d6596
// SYNTHETIC: EXE 0x4d65a2
// SYNTHETIC: EXE 0x4d65ad
// SYNTHETIC: EXE 0x4d65b7
// SYNTHETIC: EXE 0x4d65c8
// SYNTHETIC: EXE 0x4d65d4
// SYNTHETIC: EXE 0x4d65df
// SYNTHETIC: EXE 0x4d65e9
// SYNTHETIC: EXE 0x4d65fa
// SYNTHETIC: EXE 0x4d6606
// SYNTHETIC: EXE 0x4d6611
// SYNTHETIC: EXE 0x4d661b
// SYNTHETIC: EXE 0x4d662c
// SYNTHETIC: EXE 0x4d6638
// SYNTHETIC: EXE 0x4d6643
// SYNTHETIC: EXE 0x4d664d
// SYNTHETIC: EXE 0x4d665e
// SYNTHETIC: EXE 0x4d666a
// SYNTHETIC: EXE 0x4d6675
// SYNTHETIC: EXE 0x4d667f
// SYNTHETIC: EXE 0x4d6690
// SYNTHETIC: EXE 0x4d669c
// SYNTHETIC: EXE 0x4d66a7
// SYNTHETIC: EXE 0x4d66b1
// SYNTHETIC: EXE 0x4d66c2
// SYNTHETIC: EXE 0x4d66ce
// SYNTHETIC: EXE 0x4d66d9
// SYNTHETIC: EXE 0x4d66e3
// SYNTHETIC: EXE 0x4d66f4
// SYNTHETIC: EXE 0x4d6700
// SYNTHETIC: EXE 0x4d670b
// SYNTHETIC: EXE 0x4d6715
// SYNTHETIC: EXE 0x4d6726
// SYNTHETIC: EXE 0x4d6732
// SYNTHETIC: EXE 0x4d673d
// SYNTHETIC: EXE 0x4d6747
// SYNTHETIC: EXE 0x4d6758
// SYNTHETIC: EXE 0x4d6764
// SYNTHETIC: EXE 0x4d676f
// SYNTHETIC: EXE 0x4d6779
// SYNTHETIC: EXE 0x4d678a
// SYNTHETIC: EXE 0x4d6796
// SYNTHETIC: EXE 0x4d67a1
// SYNTHETIC: EXE 0x4d67ab
// SYNTHETIC: EXE 0x4d67bc
// SYNTHETIC: EXE 0x4d67c8
// SYNTHETIC: EXE 0x4d67d3
// SYNTHETIC: EXE 0x4d67dd
// SYNTHETIC: EXE 0x4d67ee
// SYNTHETIC: EXE 0x4d67fa
// SYNTHETIC: EXE 0x4d6805
// SYNTHETIC: EXE 0x4d680f
// SYNTHETIC: EXE 0x4d6820
// SYNTHETIC: EXE 0x4d682c
// SYNTHETIC: EXE 0x4d6837
// SYNTHETIC: EXE 0x4d6841
// SYNTHETIC: EXE 0x4d6852
// SYNTHETIC: EXE 0x4d685e
// SYNTHETIC: EXE 0x4d6869
// SYNTHETIC: EXE 0x4d6873
// SYNTHETIC: EXE 0x4d6884
// SYNTHETIC: EXE 0x4d6890
