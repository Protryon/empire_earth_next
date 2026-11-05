// EECivPowersManager.h (RECONSTRUCTION; file name inferred). RTTI EECivPowersManager (vtable 0x44c004, one slot: the
// deleting dtor); Civilization Builder/EECivPowersManager.cpp. The game's instance is gCivPowersManager (0x52a310;
// Empire Earth.cpp's init chain constructs it; provisional name). Only what the decompiled units use.
#ifndef EE_EECIVPOWERSMANAGER_H
#define EE_EECIVPOWERSMANAGER_H
#include "ee/EECivPowerIDs.h"
class FSFile;
class UWideString;

// One civilization power record (0x98 bytes: EECivPowersManager::GetPower copies 0x26 dwords; provisional name;
// layout inferred)
struct EECivPower {
    char mLabel[100];                      // +0x00 the power's name, NUL-terminated (dbcivpowers.dat: "Flaming
                                           //  Arrows" .. "Just In Time Manufacturing"; only copied whole)
    long mUnusedDatabaseID;                // +0x64
    long mID;                              // +0x68
    float mBonus;                           // +0x6c (EECivilizationManager::Load copies it as a level bonus: a dword move into a float)
    long mUnitSet;                         // +0x70
    long mTechnology;                      // +0x74 (EEMResearchTechnology compares it with the node's +0x5c)
    long mMiningLimit;                     // +0x78
    long mChance;                          // +0x7c
    long mUnused1;                         // +0x80
    float mAmount;                          // +0x84
    long mUnused2;                         // +0x88
    long mUnused3;                         // +0x8c
    long mUnused4;                         // +0x90
    long mUnitType;                        // +0x94
};
static_assert(sizeof(EECivPower) == 152, "on-disk record: 152 bytes on every target");

class EEPlayer;

class EECivPowersManager {
public:
    long GetName(long inID, UWideString& outName);   // 0x2831b3 (gResource->GetString(inID + 0x800c))
    // 0x2832a3: GetPower(inID), then Research its +0x74 technology for inPlayer and the per-power extras
    void ApplyPower(EEPlayer* inPlayer, unsigned long inID);
    long Load(FSFile* inFile); // original-backed 0x283124; caller1414a3, FSFile IO proof
    bool GetPower(long inID, EECivPower& outPower);   // 0x2831d0 (inID - 0x26 indexes a vector of records)
    bool HasPower(EEPlayer* inPlayer, long inID);   // 0x283248 (whether inPlayer's civilization has power inID; EEPlayer 0x1654be; provisional name)
    EECivPower* GetPowerRecord(long inID);  // 0x283235 (&record[inID - 0x26]; EEUCProjectile::ActionChanged) (provisional name)
    EECivPowersManager();               // 0x2830d0
    virtual ~EECivPowersManager();      // 0x283116

    // +0x04 (ctor 0x2830d0: the vector, then clear(); Load pushes 0x98-byte records; GetPower/GetPowerRecord index it
    // by inID - 0x26)
    std::vector<EECivPower> mPowers;
};

extern EECivPowersManager gCivPowersManager;    // 0x52a310

#endif
