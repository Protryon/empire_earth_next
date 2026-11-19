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

class EEHerd : public EEAIArea, public EeHerdFields {
public:
    static const UString sData;             // "EEHerd-SaveLoadData" (guarded chain 0xa191a)

    EEHerd();
    // 0xa19ff (ret 0xc): the player, the animal index (EEAnimal::Get), the herd id
    EEHerd(EEPlayer* inPlayer, long inAnimal, unsigned short inID);
    virtual ~EEHerd();                      // +0x00 0xa1b1e (??_G 0xa19e3)
    // +0x08 0xa2775 (ret 4): EEAIArea::Create, then (unless a load) the territory widened by a random margin; PickLeader
    virtual void Create(const std::vector<U2DPoint<long> >& inTiles);
    // +0x20: EETown's body (ICF-folded into EETown::GetRandomPoint 0xac203): a random hull point on the map
    virtual U2DPoint<long> GetRandomPoint();

    bool AddUnit(EEComplexUnit* inUnit);
    void HandleUnitHit(EEComplexUnit* inUnit);  // 0xa1d0f: the unit was attacked: flee or fight back
    void AttackBack(EEComplexUnit* inUnit, bool inAll);
    bool CanTake(EEComplexUnit* inUnit, bool inCheckRange);  // 0xa4263: may inUnit join
    bool HasUnit(EEComplexUnit* inUnit);
    bool RemoveUnit(EEComplexUnit* inUnit);
    void Update(unsigned long inTime);      // 0xa33bd: the per-state update
    static bool IsAnimalOnMap(EEUnit* inUnit);
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    long RepairPointers(long inMajorVersion, long inMinorVersion);
    void GetUnits(std::vector<EEComplexUnit*>& outUnits);   // mUnits, in order

    EEAnimal* GetAnimal() const { return reinterpret_cast<EEAnimal*>((uintptr_t)this->mAnimal); }
    EEPlayer* GetPlayer() const { return reinterpret_cast<EEPlayer*>((uintptr_t)this->mPlayer); }
};

#endif
