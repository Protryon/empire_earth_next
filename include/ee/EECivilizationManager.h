// EECivilizationManager.h (RECONSTRUCTION; file name inferred). RTTI EECivilizationManager (vtable 0x44bfd4, one slot:
// the deleting dtor); Civilization Builder/EECivilizationManager.cpp. The game's instance is gCivilizationManager
// (0x52a3a0; Empire Earth.cpp's init chain constructs it; provisional name). Only what the decompiled units use.
#ifndef EE_EECIVILIZATIONMANAGER_H
#define EE_EECIVILIZATIONMANAGER_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include <string>
#include <vector>
class FSFile;

#include "ee/EECivUpgrade.h"   // EECivUpgrade: (category << 16) | level

class EECBUIListBox;
class EEPlayer;
class UString;
class UWideString;

// One civilization of the list (0x78 bytes: the manager's vector +0x5210, copied by GetCivilizations; operator+
// 0x17a95e scales by 0x78). Names are provisional. The body is visible to every
// includer (array members cost no `$E` number; the old guard changed nothing).
struct EECivilizationEntry;
struct EECivilizationEntry {
    // inline (EEUIEPDCS's file helper 0x1e61db: with this expansion VC6 refuses the two earlier vector operator[]s as
    // the original does; provisional name)
    const char* GetFileName() const { return this->mFileName; }

    int32_t mUnusedDatabaseID;              // +0x00
    int32_t mIndex;                         // +0x04
    char mFileName[100];                   // +0x08 the civilization's file (EEUIEPDCS: the selected entry's file)
    int32_t mChecksum;                     // +0x6c actual281bc5 filename/checksum record
    int32_t mNameID;                        // +0x70 string id of the name (gResource->GetString)
    int32_t mEraNameID;                     // +0x74
};
static_assert(sizeof(EECivilizationEntry) == 120, "on-disk record: 120 bytes on every target");

// Actual serialized600-byte category row,35 records at manager+4. Names provisional.
struct EECivilizationUpgradeValue { float mValue; float mBonus; };
struct EECivilizationCategoryEntry {
    int32_t mDatabaseID;                   // +00, loader preserves/increments
    int32_t mIndex;                        // +04, loader preserves/increments
    char mLabel[100];                       // +0x008 the category name (the game never reads it; the text's label)
    int32_t mNameID;                       // +6c, actual name reader281c92
    int32_t mCost;                         // +70, actual long281873
    int32_t mUnitSet;                      // +74, actual loader writes102
    EECivilizationUpgradeValue mLevels[60];// +78, actual1..<61 float-pair enumeration
};
static_assert(sizeof(EECivilizationCategoryEntry) == 600, "on-disk record: 600 bytes on every target");

// Whole in Rust (ee_tech civ.rs, civ_apply.rs; civilization builder lane, round 2): the tables (the categories and
// their levels and ids, the powers' category, the premade civilizations and eras) and every query and apply; the
// class keeps its name and methods (forwarders in EECivilizationManager.cpp, the host of the Rust simulation).
class EECivilizationManager {
public:
    long Load(FSFile* inFile); // original-backed 0x281514; caller1414a3, FSFile IO proof
    long LoadPremadeCivilizations(FSFile* inFile); // original-backed 0x281672; caller1414a3, FSFile IO proof
    EECivilizationManager();
    virtual ~EECivilizationManager();
    // (EEUIEPDCS*; provisional names): 0x2818f9 every premade civilization (sorted by index); 0x28199b DeleteAllEntries,
    // then a header per era and a line per premade civilization of that era
    void GetCivilizations(std::vector<EECivilizationEntry>& outCivs);
    void GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType);
    void FillCivilizationList(EECBUIListBox* inList);
    unsigned long GetNumCivilizations();    // actual17a522
    const EECivilizationEntry& GetCivilization(unsigned long inIndex);   // (the original's vector's [])
    unsigned long GetEraCount();            // the distinct era name ids, in order
    long GetEra(unsigned long inIndex);
    // an upgrade id is (category << 16) | level (EECivUpgrade); the categories are those of dbcivilization (any
    // number; the original's table had 0x23 entries of 0x258 bytes) plus the special powers category
    long GetExtraCost(EECivUpgrade inUpgrade);    // 0x281873 (the category entry's +0x74: the upgrade's cost)
    bool GetCategoryNameID(unsigned long inCategory, long* outNameID);  // 0x281c92 (false past the categories)
    bool LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus);
    long CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList);  // 0x28189e (how many of inList share inUpgrade's category)
    void SplitAttribute(EECivUpgrade inUpgrade, unsigned long* outCategory, unsigned long* outIndex);
    bool GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue, float* outBonus);
    bool GetCategory(unsigned long inCategory, EECivilizationCategoryEntry** outEntry);   // 0x281d79 (the record in place)
    unsigned long GetCategoryCount();       // mCategoryCount (+0x522c)
    std::string GetCategoryID(unsigned long inCategory);   // dbcivilization.yaml's id (empty from the binary table)
    unsigned long MakeUpgrade(unsigned long inCategory, unsigned long inIndex);   // 0x281d9c (ecx unused)
    unsigned long MakePowerUpgrade(unsigned long inIndex);   // 0x281db1 (the powers' category Load appends; ecx unused)
    // a category's levels (1 .. this; the record's 60, or as many as dbcivilization.yaml gives)
    unsigned long GetLevelCount(unsigned long inCategory) const;
    // an attribute level's name (civbuilder.yaml name) and whether it applies a civ power; the level of the
    // attribute that applies a power (by power id; the original: the id itself)
    static long LevelNameID(unsigned long inLevel);
    static bool IsPowerLevel(unsigned long inLevel);
    static unsigned long PowerLevel(unsigned long inPowerID);
    // the legacy (category << 6) | level form (.civ files, premade checksums): false when it does not fit
    static EECivUpgrade FromLegacy(unsigned long inLegacy);
    static bool ToLegacy(EECivUpgrade inUpgrade, unsigned long& outLegacy);
    // the keyed form `<category id>.<attribute id>` (keyed .civ files); false without ids (the binary tables)
    bool UpgradeKey(EECivUpgrade inUpgrade, std::string& outKey) const;
    bool FindUpgradeKey(const std::string& inKey, EECivUpgrade& outUpgrade) const;
    std::vector<EECivUpgrade> GetUpgrades();      // 0x281ded (every upgrade of every category, in order)
    // (provisional names)
    bool MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName, std::vector<EECivUpgrade>& inUpgrades,
                  long* outNameID);   // 0x281bc5 (premade civilization checksum test)
    bool ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer);  // 0x281e4d (the level on every class of the category's unit set)
};

extern EECivilizationManager gCivilizationManager;
// EECivilizationManager.cpp's two init-chain strings, read by the civilization file unit (0x27fdfe..; extern, so the
// definitions have external linkage)
extern const UString kCivKey;               // 0x5654d0 ".civ"
extern const UString kCivilizationsKey;     // 0x5654c0 "Civilizations"

// Civilization Builder helpers (cdecl; provisional names): 0x27ffc0 the civilizations'
// directory (gStartupDirectory-based when inX); 0x280840 loads civilization inName from inDirectory (FSPath::FindFile
// in gDataPath), returns keseOK on success
UString EEGetCivilizationDirectory(bool inX);
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades);

#endif
