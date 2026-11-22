// EECivilizationManager.h (RECONSTRUCTION; file name inferred). RTTI EECivilizationManager (vtable 0x44bfd4, one slot:
// the deleting dtor); Civilization Builder/EECivilizationManager.cpp. The game's instance is gCivilizationManager
// (0x52a3a0; Empire Earth.cpp's init chain constructs it; provisional name). Only what the decompiled units use.
#ifndef EE_EECIVILIZATIONMANAGER_H
#define EE_EECIVILIZATIONMANAGER_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include <string>
#include <vector>
#include "rust/game/screens.h"
#include "rust/sim/tech.h"
class FSFile;

#include "ee/EECivUpgrade.h"   // EECivUpgrade: (category << 16) | level

class EECBUIListBox;
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
    // Reads the categories, then appends the civ powers' category (civbuilder.yaml's powers_category)
    long Load(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 0); } // original-backed 0x281514; caller1414a3, FSFile IO proof
    // Reads the premade civilizations, sorted by index, and their eras
    long LoadPremadeCivilizations(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 1); } // original-backed 0x281672; caller1414a3, FSFile IO proof
    EECivilizationManager();
    virtual ~EECivilizationManager();
    // (EEUIEPDCS*; provisional names): 0x2818f9 every premade civilization (sorted by index); 0x28199b DeleteAllEntries,
    // then a header per era and a line per premade civilization of that era
    void GetCivilizations(std::vector<EECivilizationEntry>& outCivs)
    {
        ee_screens_civm_civilizations((uintptr_t)&outCivs, 0, true);
    }
    void GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType)
    {
        ee_screens_civm_civilizations((uintptr_t)&outCivs, (int32_t)inType, false);
    }
    // Fills a list box with one header per era and a line per premade civilization of that era (civ_manager_calls.rs)
    void FillCivilizationList(EECBUIListBox* inList) { ee_screens_civ_fill_list(inList); }
    unsigned long GetNumCivilizations() { return ee_tech_civ_civilization_count(); }    // actual17a522
    const EECivilizationEntry& GetCivilization(unsigned long inIndex)
    {
        return *reinterpret_cast<const EECivilizationEntry*>(ee_tech_civ_civilization((uint32_t)inIndex));
    }   // (the original's vector's [])
    unsigned long GetEraCount() { return ee_tech_civ_era_count(); }            // the distinct era name ids, in order
    long GetEra(unsigned long inIndex) { return ee_tech_civ_era((uint32_t)inIndex); }
    // an upgrade id is (category << 16) | level (EECivUpgrade); the categories are those of dbcivilization (any
    // number; the original's table had 0x23 entries of 0x258 bytes) plus the special powers category
    long GetExtraCost(EECivUpgrade inUpgrade) { return ee_tech_civ_extra_cost(inUpgrade); }    // 0x281873 (the category entry's +0x74: the upgrade's cost)
    bool GetCategoryNameID(unsigned long inCategory, long* outNameID)
    {
        return ee_screens_civm_category_name_id((uint32_t)inCategory, outNameID);
    }  // 0x281c92 (false past the categories)
    bool LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus)
    {
        return this->GetCategoryLevel(inUpgrade >> kCivUpgradeLevelBits, inUpgrade & kCivUpgradeLevelMask, outValue, outBonus);
    }
    long CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList)
    {
        return ee_tech_civ_count_in_category(inUpgrade, inList->data(), inList->size());
    }  // 0x28189e (how many of inList share inUpgrade's category)
    void SplitAttribute(EECivUpgrade inUpgrade, unsigned long* outCategory, unsigned long* outIndex)
    {
        ee_screens_civm_split(inUpgrade, outCategory, outIndex);
    }
    bool GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue, float* outBonus)
    {
        return ee_tech_civ_category_level((uint32_t)inCategory, (uint32_t)inIndex, outValue, outBonus);
    }
    bool GetCategory(unsigned long inCategory, EECivilizationCategoryEntry** outEntry)
    {
        return ee_screens_civm_category((uint32_t)inCategory, (void**)outEntry);
    }   // 0x281d79 (the record in place)
    unsigned long GetCategoryCount() { return ee_tech_civ_category_count(); }       // mCategoryCount (+0x522c)
    unsigned long MakeUpgrade(unsigned long inCategory, unsigned long inIndex)
    {
        return (inCategory << kCivUpgradeLevelBits) + (inIndex & kCivUpgradeLevelMask);
    }   // 0x281d9c (ecx unused)
    unsigned long MakePowerUpgrade(unsigned long inIndex) { return ee_tech_civ_power_upgrade((uint32_t)inIndex); }   // 0x281db1 (the powers' category Load appends; ecx unused)
    // a category's levels (1 .. this; the record's 60, or as many as dbcivilization.yaml gives)
    unsigned long GetLevelCount(unsigned long inCategory) const
    {
        return ee_tech_civ_level_count((uint32_t)inCategory);
    }
    // an attribute level's name (civbuilder.yaml name) and whether it applies a civ power; the level of the
    // attribute that applies a power (by power id; the original: the id itself)
    static long LevelNameID(unsigned long inLevel) { return ee_tech_civ_level_name_id((uint32_t)inLevel); }
    static bool IsPowerLevel(unsigned long inLevel) { return ee_tech_civ_is_power_level((uint32_t)inLevel); }
    static unsigned long PowerLevel(unsigned long inPowerID) { return ee_tech_civ_power_level((uint32_t)inPowerID); }
    // the legacy (category << 6) | level form (.civ files, premade checksums): false when it does not fit
    static EECivUpgrade FromLegacy(unsigned long inLegacy) { return ee_tech_civ_from_legacy((uint32_t)inLegacy); }
    static bool ToLegacy(EECivUpgrade inUpgrade, unsigned long& outLegacy)
    {
        return ee_screens_civm_to_legacy(inUpgrade, &outLegacy);
    }
    // the keyed form `<category id>.<attribute id>` (keyed .civ files); false without ids (the binary tables)
    bool UpgradeKey(EECivUpgrade inUpgrade, std::string& outKey) const;
    bool FindUpgradeKey(const std::string& inKey, EECivUpgrade& outUpgrade) const
    {
        return ee_tech_civ_find_upgrade_key(reinterpret_cast<const uint8_t*>(inKey.data()), inKey.size(), &outUpgrade);
    }
    std::vector<EECivUpgrade> GetUpgrades();      // 0x281ded (every upgrade of every category, in order)
    // (provisional names)
    bool MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName, std::vector<EECivUpgrade>& inUpgrades, long* outNameID)
    {
        return ee_screens_civm_matches((const char*)inFile, (const char*)inName, (uint32_t)inName.GetLength(), inUpgrades.data(), inUpgrades.size(), outNameID);
    }   // 0x281bc5 (premade civilization checksum test)
    // Applies an upgrade for a player to every unit class of its category's unit set; false without a level value
    bool ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer)
    {
        return ee_screens_civm_apply_upgrade(inUpgrade, inPlayer);
    }  // 0x281e4d (the level on every class of the category's unit set)
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
