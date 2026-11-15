// C:\EEX\Empire Earth\Database\EEDbObject.h (RECONSTRUCTION; file name inferred; provisional names). One
// record of db\dbobjects.dat (RTTI EEDbObject, no base; vtable 0x43e514, one slot: the ??_G 0xd61ce that
// EEDatabase.cpp emits; sizeof 0x7bc from EEDatabase::Load's new). EEDbObject.cpp (0xd61f7..0xd68c7) defines the db
// file name keys (34 init chains) and Load. The ctor and dtor are inline (a UString at +0x7ac, vptr after it).
#ifndef EE_EEDBOBJECT_H
#define EE_EEDBOBJECT_H

class FSFileSpec;

class EEDbObject {
public:
    EEDbObject() {}
    virtual ~EEDbObject() {}

    long Load(FSFileSpec* inFile);          // 0xd689b: the 0x7a8-byte record, read whole

    char mRecord[0x7a8];                    // +0x004 (undecoded)
    UString mName;                          // +0x7ac (not touched by Load)
};

// The db file names (EEDbObject.cpp, in definition order; Empire Earth.cpp reads them). Names are provisional.
extern const UString kDbMusicFile;
extern const UString kDbObjectsFile;
extern const UString kDbGraphicsFile;
extern const UString kDbSoundsFile;
extern const UString kDbButtonsFile;
extern const UString kDbTechTreeFile;
extern const UString kDbEffectsFile;
extern const UString kDbEventsFile;
extern const UString kDbFamilyFile;
extern const UString kDbColorTableFile;
extern const UString kDbAIUnitTargetingFile;
extern const UString kDbTerrainFile;
extern const UString kDbTerrainColorFile;
extern const UString kDbTerrainTypeFile;
extern const UString kDbWorldFile;
extern const UString kDbCivilizationFile;
extern const UString kDbGfxEffectsFile;
extern const UString kDbWeaponToHitFile;
extern const UString kDbCalamityFile;
extern const UString kDbUnitSetFile;
extern const UString kDbAreaEffectFile;
extern const UString kDbFontsFile;
extern const UString kDbUpgradeFile;
extern const UString kDbCPBehaviorFile;
extern const UString kDbAmbientSoundsFile;
extern const UString kDbAnimalsFile;
extern const UString kDbUnitBehaviorFile;
extern const UString kDbStartingResourcesFile;
extern const UString kDbGameVariantFile;
extern const UString kDbPremadeCivsFile;
extern const UString kDbCliffTerrainFile;
extern const UString kDbCivPowersFile;

#endif
