// EEDatabaseTables.h (RECONSTRUCTION; file name inferred; provisional member names). The database tables
// Empire Earth.cpp keeps as globals (its init chains 0x13ebbd..0x13ed71). Each has RTTI with one vtable slot (the
// deleting dtor). Only what those definitions need: the ctor/dtor shapes the chains show (an inline ctor/dtor stores
// the vptr in the chain stub itself; an out-of-line one is called). Requires <vector>, lle/UString.h, lle/GEColor.h
// (EETerrainTable's members; GETerrainMesh/GETexture are only named).
#ifndef EE_EEDATABASETABLES_H
#define EE_EEDATABASETABLES_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths

#include "ee/EEWorldConstants.h"

class FSFileSpec;
class EEUnitClass;
class GETerrainMesh;
class GETexture;
class GETextureSet;

// RTTI EEToHitTable (vtable 0x4420c0; ??_G 0x13ebd2 after its global's ctor stub). Inline ctor and dtor.
class EEToHitTable {
public:
    virtual ~EEToHitTable() {}
    static long Load(FSFileSpec* inFile);   // 0xdcaad (db\dbweapontohit.dat: 6 x 6 into gToHitTable)
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
class EEWorldTable {
public:
    EEWorldTable();                         // 0xdcb53
    virtual ~EEWorldTable() { this->mEntries.clear(); }
    long Load(FSFileSpec* inFile);          // 0xdcb79 (db\dbworld.dat)
    void Clear();                           // 0xdcc2b (the ctor calls it)

    // Inline accessors (const: the out-of-line copies compute the element address before the load). Where VC6
    // refused them they are COMDATs called with ecx = &gWorldTable; inlined elsewhere (begin() folds to [0x52fca8]).
    // A constant is named by its id (ee/EEWorldConstants.h, generated from dbworld's ids): a raw row number does
    // not compile. F, F2, L, L2: the four values of a row (docs/DATA_FORMATS.md 8.3).
    float GetFloatValue(EEWorldConstant inID) const { return this->mEntries[inID].mFloatValue; }
    float GetFloatValue2(EEWorldConstant inID) const { return this->mEntries[inID].mFloatValue2; }  // 0x61de8
    unsigned long GetLongValue(EEWorldConstant inID) const { return this->mEntries[inID].mLongValue; }  // 0xcd4b3
    unsigned long GetLongValue2(EEWorldConstant inID) const { return this->mEntries[inID].mLongValue2; }  // 0x178554

private:
    std::vector<EEWorldTableEntry> mEntries;    // +0x04 (_First +0x08 = 0x52fca8)
};

extern EEWorldTable gWorldTable;            // 0x52fca0 (Empire Earth.cpp)

// RTTI EETerrainTable (vtable 0x43f7e4; ctor 0xdb245, dtor 0xdbc7d, ??_G 0xdbc61)
class EETerrainTable {
public:
    EETerrainTable();                       // 0xdb245
    virtual ~EETerrainTable();              // 0xdbc7d

    // (from EESELoadThread's Handle408 0x172433; provisional names)
    // 0xdbc39 (cdecl): the loaded entry inIndex of the static table (vector at 0x8e86d8), 0 past its end or when the
    // entry's +0x50 is negative
    static EETerrainTable* Get(long inIndex);
    static long Load(FSFileSpec* inFile);   // 0xdb2ce (db\dbterrain.dat)
    static void Unload();                   // 0xdb4c4
    static void DumpAllLoadedMedia(FSFileSpec& inFile);   // 0xdb551
    void Load(GETerrainMesh& ioMesh);       // 0xdb5c8 (counted by +0x48; sets the mesh's texture sets)
    void Unload(GETerrainMesh& ioMesh);     // 0xdb9e3
    GETexture* LoadTexture(const UString& inFile);   // 0xdb69d (provisional name)
    void LoadTextures();                    // 0xdb762 (Load's worker, outside the lock; provisional name)
    void AttachTextures(GETextureSet* inTextureSet);  // 0xdb96c (EEMap::RegisterTerrainSets hands it the registered set) (provisional name)
    // inline (out-of-line COMDAT 0x11e1d8 after its first user, EEMap::Load) (provisional name)
    static unsigned long GetNumCliffTerrains() { return EETerrainTable::sCliffTable.size(); }
    // inline (out-of-line COMDAT 0x1a3ef8 after its first user, EEUIFSEMap's ctor) (provisional name)
    static unsigned long GetNumTables() { return EETerrainTable::sTable.size(); }
    unsigned long GetNumUnitClasses() const { return this->mUnitClasses.size(); }
    const long& GetUnitClass(unsigned long inIndex) const { return this->mUnitClasses[inIndex]; }

    // members (layout from the ctor 0xdb245: two UStrings, GEColor's three fld1, the vector's _Buy(0), +0x68 = -1
    // before the vptr store (init list), the rest zeroed in the body; sizeof 0x8c: Load's `new`)
    UString mName;                          // +0x04
    UString mTextureName;                   // +0x14
    GEColor mColor;                         // +0x24
    GETexture* mPreviewTexture;             // +0x30 actual Terrain Virtual18 texture receiver; constructor0/reader!=0
    GETexture** mTextures;                  // +0x34 (Load: [+0x60 * row + column])
    long mFirstTexture;                     // +0x38
    long mNumTextures;                      // +0x3c
    long mTextureSetID;                     // +0x40 (Load: the mesh's texture set index)
    long mWaterAnimationPeriod;             // +0x44
    unsigned long mLoadCount;               // +0x48 (Load increments it; Unload 0xdba81 tests it unsigned: jbe)
    long mTextureID;                        // +0x4c
    long mID;                               // +0x50 (Get: negative = no entry)
    long mNameID;                           // +0x54 (gResource string id of the name; Handle408)
    long mTextureTileCoverage;              // +0x58
    long mTerrainType;                      // +0x5c
    long mColumns;                          // +0x60 (Load)
    long mRows;                             // +0x64 (Load)
    long mColorType;                        // +0x68 (= -1)
    std::vector<long> mUnitClasses;         // +0x6c unit classes (gUnitClassTable indices; Handle408 loads their media)
    long mAmbientUnitChance;                // +0x7c
    long mWaterDepth;                       // +0x80 (untouched by the ctor)
    bool mIsWater;                          // +0x84
    bool mIsInEditor;                       // +0x85
    bool mHasAlpha;                         // +0x86
    bool mIsAmbientsOnly;                   // +0x87 (untouched)
    volatile bool mIsLoading;               // +0x88 (set while Load runs; polled by other threads)
    // the two tables share the guard byte 0x8e86d4 (bits 1, 2); only their unit sees them
    static std::vector<EETerrainTable*> sTable;             // 0x8e86d8
    static std::vector<class EECliffTerrainTable*> sCliffTable;   // 0x8e86c0
};

// RTTI EECliffTerrainTable (vtable 0x43f7f0; ctor 0xdbafe, ??_G 0xdbca2; sizeof 0x24): db\dbcliffterrain.dat
// (provisional names)
class EECliffTerrainTable {
public:
    EECliffTerrainTable();                  // 0xdbafe
    virtual ~EECliffTerrainTable() {}
    static long Load(FSFileSpec* inFile);   // 0xdbb22 (records with a negative +0x68 stay NULL)
    static long Unload();                   // 0xdbbfb (returns 0)

    UString mName;                          // +0x04
    long mFirstTerrain;                     // +0x14
    long mLastTerrain;                      // +0x18
    long mL1C;                              // +0x1c
    bool mB20;                              // +0x20
};

// RTTI EETerrainTypeTable (vtable 0x43f7f8; ctor 0xdbd11, ??_G 0xdbe27). Inline dtor.
class EETerrainTypeTable {
public:
    EETerrainTypeTable();                   // 0xdbd11
    virtual ~EETerrainTypeTable() {}
    static long Load(FSFileSpec* inFile);   // 0xdbd2e (db\dbterrainType.dat)
    static void Unload();                   // 0xdbde7
    static long LoadDamageTables(FSFileSpec* inFile);  // 0xdbf11 (gDamageTable and its two twins; provisional name)
    static void ClearDamageTables();        // 0xdc5bb
    // members from the ctor: +0x08..+0x18 = 0, +0x1c = false; sizeof 0x20 (new site)
    long mColorSetID;                       // +0x04 (untouched by the ctor)
    long mID;                               // +0x08
    long mFirstColor;                       // +0x0c
    long mLastColor;                        // +0x10
    long mNameID;                           // +0x14
    long mL18;                              // +0x18
    bool mIsInEditor;                       // +0x1c
    static std::vector<EETerrainTypeTable*> sTable;  // 0x8e86f8 (guard 0x8e86f0 bit 1; only its unit sees it)
    // inline (out-of-line COMDATs 0x11e22b / 0x11e24e after their first user in EEMap.cpp) (provisional names)
    static EETerrainTypeTable* Get(unsigned long inIndex) { return inIndex < EETerrainTypeTable::sTable.size() ? EETerrainTypeTable::sTable[inIndex] : NULL; }
    static unsigned long GetCount() { return EETerrainTypeTable::sTable.size(); }
};

// RTTI EERandomMapTable (vtable 0x43f7b8; ctor 0xda407, ??_G 0xda550). Inline dtor.
class EERandomMapTable {
public:
    EERandomMapTable();                     // 0xda407
    virtual ~EERandomMapTable() {}
    static long Load(FSFileSpec* inFile);   // 0xda47b (db\dbrandommap.dat)
    static void Unload();                   // 0xda510
    // members from the ctor: longs 0, eighteen floats 0.0f; sizeof 0x60 (new site)
    long mL04;                              // +0x04
    long mL08;                              // +0x08
    float mF0C;                             // +0x0c
    float mF10;                             // +0x10
    float mF14;                             // +0x14
    float mF18;                             // +0x18
    float mF1C;                             // +0x1c
    float mF20;                             // +0x20
    float mF24;                             // +0x24
    float mF28;                             // +0x28
    float mF2C;                             // +0x2c
    float mF30;                             // +0x30
    float mF34;                             // +0x34
    float mF38;                             // +0x38
    float mF3C;                             // +0x3c
    float mF40;                             // +0x40
    float mF44;                             // +0x44
    float mF48;                             // +0x48
    float mF4C;                             // +0x4c
    float mF50;                             // +0x50
    long mL54;                              // +0x54
    long mL58;                              // +0x58
    long mL5C;                              // +0x5c
    static std::vector<EERandomMapTable*> sTable;   // 0x8e8660 (guard 0x8e8658 bit 1; only its unit sees it)
};

// Original-backed terrain preparation/cleanup, cdecl noargs; caller/callee proof.
void EELoadTerrainPreviews();
void EEReleaseTerrainPreviews();

#endif
