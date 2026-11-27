// C:\EEX\Empire Earth\AI Thread\EEHerd.h (RECONSTRUCTION; file name inferred). A herd of wild animals that is hunted or
// driven as one AI area (RTTI EEHerd > EEAIArea @0; vtable 0x43c2e8, 11 slots: EEAIArea's, with its own ??_G 0xa19e3
// and Create override 0xa2775; slot +0x20 is EETown's 0xac203 by ICF). EEHerd.cpp 0xa1910..0xa543d: the save key
// "EEHerd-SaveLoadData".
// Port (the AI Thread cluster, round 2): the herd is Rust's whole (ee_sim::cp herd.rs): its members are the record
// EeHerdFields the class derives from (the original's names and meanings; the units, the looks and a loaded game's
// tiles are the record's Rust lists), every body Rust's over the herd's host (EEHerd.cpp ee_cp_herd_host). The class
// keeps its vtable slots and the members its callers use, as forwarders.
// Requires ee/EEAIArea.h.
#ifndef EE_EEHERD_H
#define EE_EEHERD_H

#include <vector>
#include "rust/sim/cp.h"             // EeHerdFields

class EEComplexUnit;
class EEAnimal;
class EEUnit;
class EEPlayer;
class FSFile;
int32_t EESaveFilePrimitive(void* inFile, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io);   // EEPlayer.h

class EEHerd : public EEAIArea, public EeHerdFields {
public:
    static const UString sData;             // "EEHerd-SaveLoadData" (guarded chain 0xa191a)

    EEHerd() { ee_cp_herd_construct_default(static_cast<void*>(this), this); }
    // 0xa19ff (ret 0xc): the player, the animal index (EEAnimal::Get), the herd id
    EEHerd(EEPlayer* inPlayer, long inAnimal, unsigned short inID) { ee_cp_herd_construct(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inPlayer, inAnimal, inID); }
    ~EEHerd();                      // +0x00 0xa1b1e (??_G 0xa19e3)
    // +0x08 0xa2775 (ret 4): EEAIArea::Create, then (unless a load) the territory widened by a random margin; PickLeader
    void Create(const std::vector<U2DPoint<long> >& inTiles) { ee_cp_herd_create(static_cast<void*>(this), this, reinterpret_cast<const EeHerdPoint*>(inTiles.data()), inTiles.size()); }
    // +0x20: EETown's body (ICF-folded into EETown::GetRandomPoint 0xac203): a random hull point on the map
    U2DPoint<long> GetRandomPoint() { EeHerdPoint p = ee_cp_herd_random_point(this, this); return U2DPoint<long>(p.mX, p.mY); }

    bool AddUnit(EEComplexUnit* inUnit) { return ee_cp_herd_add_unit(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit); }
    // 0xa1d0f: the unit was attacked: flee or fight back
    void HandleUnitHit(EEComplexUnit* inUnit) { ee_cp_herd_handle_unit_hit(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit); }
    void AttackBack(EEComplexUnit* inUnit, bool inAll) { ee_cp_herd_attack_back(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit, inAll); }
    // 0xa4263: may inUnit join
    bool CanTake(EEComplexUnit* inUnit, bool inCheckRange) { return ee_cp_herd_can_take(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit, inCheckRange); }
    bool HasUnit(EEComplexUnit* inUnit) { return ee_cp_herd_has_unit(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit); }
    bool RemoveUnit(EEComplexUnit* inUnit) { return ee_cp_herd_remove_unit(static_cast<void*>(this), this, (uint64_t)(uintptr_t)inUnit); }
    // 0xa33bd: the per-state update
    void Update(unsigned long inTime) { ee_cp_herd_update(static_cast<void*>(this), this, inTime); }
    static bool IsAnimalOnMap(EEUnit* inUnit);
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
        { return ee_cp_herd_load(this, this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion, &inFile, &EESaveFilePrimitive); }
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
        { return ee_cp_herd_save(this, this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion, &outFile, &EESaveFilePrimitive); }
    long RepairPointers(long inMajorVersion, long inMinorVersion) { return ee_cp_herd_repair_pointers(static_cast<void*>(this), this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion); }

    EEAnimal* GetAnimal() const { return reinterpret_cast<EEAnimal*>((uintptr_t)this->mAnimal); }
};

#endif
