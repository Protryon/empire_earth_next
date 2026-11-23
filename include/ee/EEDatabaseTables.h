// EEDatabaseTables.h (RECONSTRUCTION; file name inferred; provisional member names). The database tables
// Empire Earth.cpp keeps as globals (its init chains 0x13ebbd..0x13ed71). Each has RTTI with one vtable slot (the
// deleting dtor). Only what those definitions need: the ctor/dtor shapes the chains show (an inline ctor/dtor stores
// the vptr in the chain stub itself; an out-of-line one is called). Requires <vector>, lle/UString.h, lle/GEColor.h
// (EETerrainTable's members; GETerrainMesh/GETexture are only named).
#ifndef EE_EEDATABASETABLES_H
#define EE_EEDATABASETABLES_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include <vector>

#include "ee/EEWorldConstants.h"
#include "ee/EEGlobalConstants.h"
#include "rust/data/tables.h"
#include "rust/sim/combat.h"
#include "ee/EEDbList.h"
#include "rust/data/media.h"

class FSFileSpec;
class EEUnitClass;
class GETerrainMesh;
class GETexture;
class GETextureSet;

// RTTI EEToHitTable (vtable 0x4420c0; ??_G 0x13ebd2 after its global's ctor stub). Inline ctor and dtor.
class EEToHitTable {
public:
    virtual ~EEToHitTable() {}
    // 0xdcaad: db\dbweapontohit.dat's 6 x 6 factors (percent to fraction) into gWeaponToHitFactors (ee_data
    // tables.rs); a table of another size is not read. Returns the last read status (requires ee/EEDbRecords.h)
    template <class F> static long Load(F* inFile)
    {
        return EEDbReadRecords(inFile, ee_data_to_hit_record_size(), [](unsigned long inCount) { return inCount == 6; },
            [](unsigned long inIndex, const unsigned char* inRecord) { ee_data_to_hit_load(inIndex, inRecord); });
    }
    // 0xdbe44 (ret 0x10): the chance of weapon kind inKind against inClass (EECPAttackSubGroupUnit 0x63aef)
    // (provisional name)
    float GetAttackFactor(long inKind, EEUnitClass* inClass, unsigned long inPlayer, unsigned long inTargetPlayer);
};

// RTTI EEComputerPlayerBehaviorTable (vtable 0x4420d0; ??_G 0x13edb0). Inline ctor and dtor.
class EEComputerPlayerBehaviorTable {
public:
    virtual ~EEComputerPlayerBehaviorTable() {}
};

// An element of EEWorldTable's vector: one raw 0x1c-byte record of db\dbworld.dat (95 of them; EEWorldTable's loader
// 0xdcb79 reads each with one Read(&theEntry, 0x1c) into a local and copies it into mEntries[i]; resize 0xdcc33 takes
// the element by value, erase/copy 0xdd0f2/0xdd136 move it with `rep movsd` (7 dwords) and destroy nothing: a POD with
// implicit members). Field types from the readers and the file's values:
// +0x00/+0x04 are read with fld (entry 42 +0x00 = 1.5f, entry 81 = 1.0f / 1.5f), +0x10/+0x14 as unsigned integers
// (the lobby's unit limit, entry 26: 50 / 1200, unsigned divide and compares). +0x08 is the record's 1-based
// database id (gaps: 35 -> 36), +0x0c its index. Member names are provisional.
struct EEWorldTableEntry {
    float mFloatValue;                      // +0x00
    float mFloatValue2;                     // +0x04
    int32_t mUnusedDatabaseID;              // +0x08
    int32_t mUnusedIndex;                   // +0x0c
    int32_t mLongValue;                     // +0x10 (signed: EECPIntelligence 0x480f9b `fidiv dword [eax+0x10]`; TYPE CENSUS)
    uint32_t mLongValue2;                   // +0x14
    int32_t mUnused;                        // +0x18 (entry 31: 1, entry 48: 0x50200; otherwise 0)
};
static_assert(sizeof(EEWorldTableEntry) == 28, "on-disk record: 28 bytes on every target");

// RTTI EEWorldTable (vtable 0x43f810; ctor 0xdcb53 out of line, in EEUnitBehavior.cpp's range; ??_G 0xdd087). The
// dtor is inline: the global's atexit stub 0x13eca6 stores the vptr, clears the vector, then destroys it.
// Its only instance is gWorldTable (0x52fca0). The original read the constants as mEntries[k].<field>: the
// inline operator[]'s begin() is `mov eax, [0x52fca8]` (mEntries._First; what earlier rounds called "gDatabase"),
// its op+ `imul 0x1c` (refused at some sites: 0x84d7b; the out-of-line operator[] 0x1ae53b, `mov ecx, 0x92fca4`).
// Since the game's leaves round 2 the rows are ee_data's (tables.rs EeDataWorldEntry); the class is their readers.
extern float& gGameStateGameSpeed;          // 0x52fb54: entry 42's float, cached after the load (EEGlobals.cpp)
class EEWorldTable {
public:
    // 0xdcb79: db\dbworld.dat's rows (the reads stop at the first error), then the game speed cached; the first
    // read error (requires ee/EEDbRecords.h)
    template <class F> long Load(F* inFile)
    {
        ee_data_world_clear();
        long theResult = EEDbReadRecords(inFile, ee_data_world_record_size(),
            [](unsigned long inCount) { ee_data_world_begin(inCount); return true; },
            [](unsigned long inIndex, const unsigned char* inRecord) { ee_data_world_load(inIndex, inRecord); }, true);
        gGameStateGameSpeed = this->GetFloatValue(kWorldGameSpeedDefault);
        return theResult;
    }
    void Clear() { ee_data_world_clear(); }

    // Inline accessors (const: the out-of-line copies compute the element address before the load). Where VC6
    // refused them they are COMDATs called with ecx = &gWorldTable; inlined elsewhere (begin() folds to [0x52fca8]).
    // A constant is named by its id (ee/EEWorldConstants.h, generated from dbworld's ids): a raw row number does
    // not compile. F, F2, L, L2: the four values of a row (docs/DATA_FORMATS.md 8.3).
    float GetFloatValue(EEWorldConstant inID) const { return ee_data_world_entry((uint32_t)inID).mFloatValue; }
    float GetFloatValue2(EEWorldConstant inID) const { return ee_data_world_entry((uint32_t)inID).mFloatValue2; }
    unsigned long GetLongValue(EEWorldConstant inID) const { return ee_data_world_entry((uint32_t)inID).mLongValue; }
    unsigned long GetLongValue2(EEWorldConstant inID) const { return ee_data_world_entry((uint32_t)inID).mLongValue2; }
};

extern EEWorldTable gWorldTable;            // 0x52fca0 (Empire Earth.cpp)

// RTTI EETerrainTable (vtable 0x43f7e4; ctor 0xdb245, dtor 0xdbc7d, ??_G 0xdbc61). The record's members are
// EeDataTerrainFields (rust/crates/ee_data tables.rs: the record and its copy), the base; the mesh textures' use
// count (mMediaCount, mIsLoading) is EeDataMediaUse (media.rs).
class EETerrainTable : public EeDataTerrainFields, public EeDataMediaUse {
public:
    EETerrainTable();
    virtual ~EETerrainTable();

    // (from EESELoadThread's Handle408 0x172433; provisional names)
    // 0xdbc39 (cdecl): the loaded entry inIndex of the static table, 0 past its end or when the entry's id is negative
    static EETerrainTable* Get(long inIndex);
    static long Load(FSFileSpec* inFile);   // 0xdb2ce (db\dbterrain.dat)
    static void Unload();
    static void DumpAllLoadedMedia(FSFileSpec& inFile);
    void Load(GETerrainMesh& ioMesh);       // 0xdb5c8 (counted by mLoadCount; sets the mesh's texture sets)
    void Unload(GETerrainMesh& ioMesh);
    GETexture* LoadTexture(const UString& inFile);
    void LoadTextures();                    // 0xdb762 (Load's worker, outside the lock; provisional name)
    void AttachTextures(GETextureSet* inTextureSet);  // 0xdb96c (EEMap::RegisterTerrainSets hands it the registered set) (provisional name)
    static unsigned long GetNumCliffTerrains() { return EETerrainTable::sCliffTable.size(); }
    static unsigned long GetNumTables() { return EETerrainTable::sTable.size(); }
    unsigned long GetNumUnitClasses() const { return this->mUnitClasses.size(); }
    const long& GetUnitClass(unsigned long inIndex) const { return this->mUnitClasses[inIndex]; }

    UString mName;
    UString mTextureName;                   // the lead string: the texture file, numbered _1 .. _n
    GEColor mColor;
    GETexture* mPreviewTexture = NULL;      // the terrain list's preview (EELoadTerrainPreviews)
    GETexture** mTextures = NULL;           // [mColumns * row + column]
    long mFirstTexture = 0;                 // the textures numbered over the table
    long mTextureSetID;                     // the mesh's texture set index
    std::vector<long> mUnitClasses;         // ambient unit classes (gUnitClassTable indices; Handle408 loads their media)
    static std::vector<EETerrainTable*> sTable;
    static std::vector<class EECliffTerrainTable*> sCliffTable;
};

// RTTI EECliffTerrainTable (vtable 0x43f7f0; ctor 0xdbafe, ??_G 0xdbca2): db\dbcliffterrain.dat. The record's
// members are EeDataCliffTerrainFields, the base (provisional names)
class EECliffTerrainTable : public EeDataCliffTerrainFields {
public:
    EECliffTerrainTable() : EeDataCliffTerrainFields() {}
    virtual ~EECliffTerrainTable() {}
    static long Load(FSFileSpec* inFile);   // 0xdbb22 (records with a negative index stay NULL)
    static long Unload();                   // 0xdbbfb (returns 0)

    UString mName;
};

// RTTI EETerrainTypeTable (vtable 0x43f7f8; ctor 0xdbd11, ??_G 0xdbe27). Inline dtor.
// mColorSetID (EEMap sets it) and mID..mIsInEditor are EeDataTerrainTypeFields (rust/crates/ee_data tables.rs: the
// record, its copy and the list), the base
class EETerrainTypeTable : public EeDataTerrainTypeFields {
public:
    // 0xdbd2e: db\dbterrainType.dat into the Rust list; the last read status (requires ee/EEDbRecords.h)
    template <class F> static long Load(F* inFile) { return EEDbLoadList(inFile, EeDataList::TerrainTypes); }
    static void Unload() { EETerrainTypeTable::sTable.clear(); }
    // 0xdbf11: the damage tables (ee_combat damage_table.rs: one row of 0x44 factors per unit family, percent to
    // fraction, and the two twins scaled by world constant 0x59's values; CBDT); the last read status (provisional name)
    template <class F> static long LoadDamageTables(F* inFile)
    {
        std::vector<int32_t> thePercents;
        unsigned long theCount = 0;
        bool theCounted = false;
        ee_combat_damage_table_clear();
        long theResult = EEDbReadRecords(inFile, ee_data_damage_table_record_size(),
            [&](unsigned long inCount) {
                theCount = inCount;
                theCounted = true;
                thePercents.resize(inCount * EE_COMBAT_DAMAGE_COLUMNS);
                return true;
            },
            [&](unsigned long inIndex, const unsigned char* inRecord) {
                ee_data_damage_table_from_record(inRecord, &thePercents[inIndex * EE_COMBAT_DAMAGE_COLUMNS]);
            });
        if (theCounted)
            ee_combat_damage_table_load(thePercents.empty() ? NULL : (const int32_t(*)[EE_COMBAT_DAMAGE_COLUMNS])&thePercents[0],
                                        (uint32_t)theCount, kHeightEpsilon, gWorldTable.GetFloatValue(kWorldDifficultyDamageScale),
                                        gWorldTable.GetFloatValue2(kWorldDifficultyDamageScale));
        return theResult;
    }
    static void ClearDamageTables() { ee_combat_damage_table_clear(); }
    static constexpr EEDbList<EETerrainTypeTable, EeDataList::TerrainTypes> sTable{};   // Rust's list (ee/EEDbList.h)
    // inline (out-of-line COMDATs 0x11e22b / 0x11e24e after their first user in EEMap.cpp) (provisional names)
    static EETerrainTypeTable* Get(unsigned long inIndex) { return inIndex < EETerrainTypeTable::sTable.size() ? EETerrainTypeTable::sTable[inIndex] : NULL; }
    static unsigned long GetCount() { return EETerrainTypeTable::sTable.size(); }
};

// db\dbrandommap's and db\dbterraingraytextures' records are ee_data tables.rs' (the game reads neither).

// Original-backed terrain preparation/cleanup, cdecl noargs; caller/callee proof.
void EELoadTerrainPreviews();
void EEReleaseTerrainPreviews();

#endif
