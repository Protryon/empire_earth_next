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
extern const UString kDbMusicFile;              // 0x8e7f60
extern const UString kDbObjectsFile;            // 0x8e7f50
extern const UString kDbGraphicsFile;           // 0x8e7f40
extern const UString kDbSoundsFile;             // 0x8e7f30
extern const UString kDbButtonsFile;            // 0x8e7f20
extern const UString kDbTechTreeFile;           // 0x8e7f10
extern const UString kDbEffectsFile;            // 0x8e7f00
extern const UString kDbEventsFile;             // 0x8e7ef0
extern const UString kDbFamilyFile;             // 0x8e7ee0
extern const UString kDbColorTableFile;         // 0x8e7ed0
extern const UString kDbAIUnitTargetingFile;    // 0x8e7ec0
extern const UString kDbTerrainFile;            // 0x8e7eb0
extern const UString kDbTerrainColorFile;       // 0x8e7ea0
extern const UString kDbTerrainTypeFile;        // 0x8e7e90
extern const UString kDbWorldFile;              // 0x8e7e80
extern const UString kDbCivilizationFile;       // 0x8e7e60
extern const UString kDbGfxEffectsFile;         // 0x8e7e50
extern const UString kDbWeaponToHitFile;        // 0x8e7e30
extern const UString kDbCalamityFile;           // 0x8e7e20
extern const UString kDbUnitSetFile;            // 0x8e7e10
extern const UString kDbAreaEffectFile;         // 0x8e7e00
extern const UString kDbFontsFile;              // 0x8e7df0
extern const UString kDbUpgradeFile;            // 0x8e7de0
extern const UString kDbCPBehaviorFile;         // 0x8e7dd0
extern const UString kDbAmbientSoundsFile;      // 0x8e7dc0
extern const UString kDbAnimalsFile;            // 0x8e7db0
extern const UString kDbUnitBehaviorFile;       // 0x8e7da0
extern const UString kDbStartingResourcesFile;  // 0x8e7d90
extern const UString kDbGameVariantFile;        // 0x8e7d80
extern const UString kDbPremadeCivsFile;        // 0x8e7d70
extern const UString kDbCliffTerrainFile;       // 0x8e7d60
extern const UString kDbCivPowersFile;          // 0x8e7d50

#endif
