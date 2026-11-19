// C:\EEX\Empire Earth\Calamities\EEContagionManager.h (RECONSTRUCTION; file name inferred). The world's contagion
// helpers (one per spreading calamity), by id. Method names from the calamity asserts
// ("::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID)", EECCAEExtraDamage.cpp).
// New (port): the manager and its helpers are Rust's whole (ee_sim::tech contagion_store.rs: the helper list, the
// helpers' units, ids and timers, every member function; calamities lane, round 4). This class is the C++ name the
// persistent chunk and the world hold, each member one call; EEContagionHelper is the Rust helper.
// Requires <vector>, <stdio.h> (FILE).
#ifndef EE_EECONTAGIONMANAGER_H
#define EE_EECONTAGIONMANAGER_H

#include "rust/sim/tech.h"   // EeContagionManager, EeContagionHelper (contagion_store.rs)

class EECalamity;
class EEContagionFunctor;
class EEUnit;
class FSFile;

typedef EeContagionHelper EEContagionHelper;

class EEContagionManager {
public:
    EEContagionManager() : mRust(ee_contagion_manager_new()) {}
    ~EEContagionManager() { ee_contagion_manager_free(this->mRust); }   // the list only (the original's)
    EEContagionManager(const EEContagionManager&) = delete;
    // the other's helpers (the same ones, as the original's vector copy), last id and loaded indexes
    EEContagionManager& operator=(const EEContagionManager& inOther)
    {
        ee_contagion_manager_assign(this->mRust, inOther.mRust);
        return *this;
    }
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void DumpOutOfSynch(FILE* inFile) { ee_contagion_manager_dump(this->mRust, (uint64_t)(uintptr_t)inFile); }
    long GetOutOfSynchChecksum() { return ee_contagion_manager_checksum(this->mRust); }
    // the loaded helpers' calamities by index (helpers whose calamity is gone are destroyed)
    long RestorePointers(std::vector<EECalamity*>& inCalamities);
    // the new helper's id (the plague passes its target unit, the area effects none)
    long CreateContagionHelper(EEContagionFunctor* inFunctor, EECalamity* inCalamity, EEUnit* inTargetUnit)
    {
        return ee_contagion_manager_create(this->mRust, (uint64_t)(uintptr_t)inFunctor, (uint64_t)(uintptr_t)inCalamity,
                                           (uint64_t)(uintptr_t)inTargetUnit);
    }
    bool DestroyContagionHelper(long inHelperID) { return ee_contagion_manager_destroy(this->mRust, inHelperID); }
    EEContagionHelper* GetContagionHelper(long inHelperID) { return ee_contagion_manager_helper(this->mRust, inHelperID); }
    void SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
    {
        ee_contagion_manager_swap_unit(this->mRust, (uint64_t)(uintptr_t)inOldUnit, (uint64_t)(uintptr_t)inNewUnit);
    }

    EeContagionManager* mRust;
};

// a helper's AddUnit and Update (its functor's numbers, its calamity class's immunity flag)
bool EEContagionHelperAddUnit(EEContagionHelper* inHelper, EEUnit* inUnit, bool inNotify);
void EEContagionHelperUpdate(EEContagionHelper* inHelper, float inElapsedTime);

#endif
