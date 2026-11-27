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
// The fields are Rust's (ee_sim tech civ.rs EeTechCivEntry, with the size assert).
struct EECivilizationEntry : EeTechCivEntry {
    // inline (EEUIEPDCS's file helper 0x1e61db: with this expansion VC6 refuses the two earlier vector operator[]s as
    // the original does; provisional name)
    const char* GetFileName() const { return this->mFileName; }
};

// Actual serialized600-byte category row,35 records at manager+4. Names provisional.
// The records are Rust's (ee_sim tech civ.rs EeTechCivLevel, EeTechCivCategory, with the size assert).
typedef EeTechCivCategory EECivilizationCategoryEntry;

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
    virtual ~EECivilizationManager() {}
    // (EEUIEPDCS*; provisional names): 0x2818f9 every premade civilization (sorted by index); 0x28199b DeleteAllEntries,
    // then a header per era and a line per premade civilization of that era
    void GetCivilizations(std::vector<EECivilizationEntry>& outCivs)
    {
        ee_screens_civm_civilizations((uintptr_t)&outCivs, 0, true);
    }
    // Fills a list box with one header per era and a line per premade civilization of that era (civ_manager_calls.rs)
    void FillCivilizationList(EECBUIListBox* inList) { ee_screens_civ_fill_list(inList); }
    unsigned long GetNumCivilizations() { return ee_tech_civ_civilization_count(); }    // actual17a522
    const EECivilizationEntry& GetCivilization(unsigned long inIndex)
    {
        return *reinterpret_cast<const EECivilizationEntry*>(ee_tech_civ_civilization((uint32_t)inIndex));
    }   // (the original's vector's [])
    // an upgrade id is (category << 16) | level (EECivUpgrade); the categories are those of dbcivilization (any
    // number; the original's table had 0x23 entries of 0x258 bytes) plus the special powers category
    bool GetCategoryNameID(unsigned long inCategory, long* outNameID)
    {
        return ee_screens_civm_category_name_id((uint32_t)inCategory, outNameID);
    }  // 0x281c92 (false past the categories)
    bool LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus)
    {
        return this->GetCategoryLevel(inUpgrade >> kCivUpgradeLevelBits, inUpgrade & kCivUpgradeLevelMask, outValue, outBonus);
    }
    void SplitAttribute(EECivUpgrade inUpgrade, unsigned long* outCategory, unsigned long* outIndex)
    {
        ee_screens_civm_split(inUpgrade, outCategory, outIndex);
    }
    bool GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue, float* outBonus)
    {
        return ee_tech_civ_category_level((uint32_t)inCategory, (uint32_t)inIndex, outValue, outBonus);
    }
    unsigned long MakePowerUpgrade(unsigned long inIndex) { return ee_tech_civ_power_upgrade((uint32_t)inIndex); }   // 0x281db1 (the powers' category Load appends; ecx unused)
    // an attribute level's name (civbuilder.yaml name) and whether it applies a civ power; the level of the
    // attribute that applies a power (by power id; the original: the id itself)
    static long LevelNameID(unsigned long inLevel) { return ee_tech_civ_level_name_id((uint32_t)inLevel); }
    static bool IsPowerLevel(unsigned long inLevel) { return ee_tech_civ_is_power_level((uint32_t)inLevel); }
    static unsigned long PowerLevel(unsigned long inPowerID) { return ee_tech_civ_power_level((uint32_t)inPowerID); }
    // Applies an upgrade for a player to every unit class of its category's unit set; false without a level value
    bool ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer)
    {
        return ee_screens_civm_apply_upgrade(inUpgrade, inPlayer);
    }  // 0x281e4d (the level on every class of the category's unit set)
};

extern EECivilizationManager gCivilizationManager;
// EECivilizationManager.cpp's init-chain string, read by the civilization file unit (0x27fdfe..; extern, so the
// definition has external linkage; the other, kCivKey 0x5654d0 ".civ", is civ_load.rs')
extern const UString kCivilizationsKey;     // 0x5654c0 "Civilizations"

// Civilization Builder helpers (cdecl; provisional names): 0x27ffc0 the civilizations'
// directory (gStartupDirectory-based when inX); 0x280840 loads civilization inName from inDirectory (FSPath::FindFile
// in gDataPath), returns keseOK on success
UString EEGetCivilizationDirectory(bool inX);
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades);

#endif
