// C:\EEX\Empire Earth\...\EEAIForest.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIForest > EEAIArea @0; vtable 0x43a42c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAIFOREST_H
#define EE_EEAIFOREST_H
#include "ee/EEAIArea.h"
#include "rust/sim/records.h"

// Port: the members are the Rust record EeAStarForestRecord (ee_sim::astar area_kinds.rs), the private base:
// +0x68 the resource kind, +0x6c/+0x70/+0x74 the trees by kind 1/2/3, +0x78 (zeroed), +0x7c the next renewal
// (unsigned deadline, 0x22a3fd), +0x80 its continent, +0x82 its id (the continent manager's mForests index)
class EEAIForest : public EEAIArea, private EeAStarForestRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    bool RegrowTrees(const U2DPoint<long>& inTile, long inRadius, long inCount) { return ee_astar_forest_regrow((uint64_t)(uintptr_t)this, inTile.mX, inTile.mY, inRadius, inCount); }
    // +0x28 0x29e17: EEAIArea's, then the removed tile's resource kind (EEAIHasResourceUnit) leaves its count
    EeAStarForestRecord* Record() { return this; }
    const EeAStarForestRecord* Record() const { return this; }
    long GetResourceKind() const { return this->resource_kind; }
    unsigned long GetNextRenewal() const { return this->next_renewal; }
    void SetNextRenewal(unsigned long inTime) { this->next_renewal = inTime; }
    unsigned short GetID() const { return this->id; }

private:
    EEAIForest();
    ~EEAIForest();
};

#endif
