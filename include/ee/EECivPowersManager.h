// EECivPowersManager.h (RECONSTRUCTION; file name inferred). RTTI EECivPowersManager (vtable 0x44c004, one slot: the
// deleting dtor); Civilization Builder/EECivPowersManager.cpp. The game's instance is gCivPowersManager (0x52a310;
// Empire Earth.cpp's init chain constructs it; provisional name). Only what the decompiled units use.
#ifndef EE_EECIVPOWERSMANAGER_H
#define EE_EECIVPOWERSMANAGER_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include "ee/EECivPowerIDs.h"
#include "rust/game/screens.h"
#include "rust/sim/tech.h"
class FSFile;
class UWideString;

// One civilization power record (0x98 bytes: EECivPowersManager::GetPower copies 0x26 dwords; provisional name;
// layout inferred)
// the record is Rust's (ee_sim tech civ.rs EeTechCivPower: the fields and the size assert)
typedef EeTechCivPower EECivPower;

class EEPlayer;

// Whole in Rust (ee_tech civ.rs; civilization builder lane, round 2): the records (dbcivpowers) and the queries; the
// grant of a power (the original's ApplyPower 0x2832a3) is ee_tech civ_apply.rs'. Forwarders in
// EECivilizationManager.cpp.
class EECivPowersManager {
public:
    // Reads a civilization power's name from the string table
    long GetName(long inID, UWideString& outName);   // 0x2831b3 (gResource->GetString(inID + 0x800c))
    // Reads the civilization power records (appended)
    long Load(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 2); } // original-backed 0x283124; caller1414a3, FSFile IO proof
    bool GetPower(long inID, EECivPower& outPower)
    {
        return ee_tech_civ_power((int32_t)inID, &outPower);
    }   // 0x2831d0 (inID - 0x26 indexes the records)
    // (an upgrade whose attribute applies the power: civbuilder.yaml; the original compared the level with the id)
    bool HasPower(EEPlayer* inPlayer, long inID) { return ee_screens_civm_has_power((uintptr_t)inPlayer, inID); }   // 0x283248 (whether inPlayer's civilization has power inID; EEPlayer 0x1654be; provisional name)
    EECivPower* GetPowerRecord(long inID)
    {
        return reinterpret_cast<EECivPower*>(ee_tech_civ_power_record((int32_t)inID));
    }  // 0x283235 (&record[inID - 0x26]; EEUCProjectile::ActionChanged) (provisional name)
    unsigned long GetNumPowers() { return ee_tech_civ_power_count(); }           // (the original's mPowers.size())
    EECivPowersManager() {}
    virtual ~EECivPowersManager() {}
};

extern EECivPowersManager gCivPowersManager;

#endif
