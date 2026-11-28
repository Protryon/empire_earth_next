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
    EECivilizationManager();
    virtual ~EECivilizationManager() {}
    const EECivilizationEntry& GetCivilization(unsigned long inIndex)
    {
        return *reinterpret_cast<const EECivilizationEntry*>(ee_tech_civ_civilization((uint32_t)inIndex));
    }   // (the original's vector's [])
    bool LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus)
    {
        return ee_tech_civ_category_level((uint32_t)(unsigned long)(inUpgrade >> kCivUpgradeLevelBits), (uint32_t)(unsigned long)(inUpgrade & kCivUpgradeLevelMask), outValue, outBonus);
    }
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
