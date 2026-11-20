// C:\EEX\Empire Earth\...\EEAIForest.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIForest > EEAIArea @0; vtable 0x43a42c, 11 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h.
#ifndef EE_EEAIFOREST_H
#define EE_EEAIFOREST_H
#include "rust/sim/astar.h"

// Port: the members are the Rust record EeAStarForestRecord (ee_sim::astar area_kinds.rs), the private base:
// +0x68 the resource kind, +0x6c/+0x70/+0x74 the trees by kind 1/2/3, +0x78 (zeroed), +0x7c the next renewal
// (unsigned deadline, 0x22a3fd), +0x80 its continent, +0x82 its id (the continent manager's mForests index)
class EEAIForest : public EEAIArea, private EeAStarForestRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    static EEAIForest* New() { return (EEAIForest*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::Forests); }
    static void Delete(EEAIForest* inObject) { ee_astar_object_delete(EeAStarCmTable::Forests, (uint64_t)(uintptr_t)inObject); }
    bool RegrowTrees(const U2DPoint<long>& inTile, long inRadius, long inCount);
    void UpdateResourceKind() { ee_astar_forest_update_resource_kind(this); }   // 0x29d69 (Rust)
    // +0x28 0x29e17: EEAIArea's, then the removed tile's resource kind (EEAIHasResourceUnit) leaves its count
    bool RemoveTile(const U2DPoint<long>& inPoint);
    EeAStarForestRecord* Record() { return this; }
    const EeAStarForestRecord* Record() const { return this; }
    long GetResourceKind() const { return this->resource_kind; }
    long GetNumResources1() const { return this->num_resources1; }
    long GetNumResources2() const { return this->num_resources2; }
    long GetNumResources3() const { return this->num_resources3; }
    void SetNumResources1(long inCount) { this->num_resources1 = inCount; }
    void SetNumResources2(long inCount) { this->num_resources2 = inCount; }
    void SetNumResources3(long inCount) { this->num_resources3 = inCount; }
    unsigned long GetNextRenewal() const { return this->next_renewal; }
    void SetNextRenewal(unsigned long inTime) { this->next_renewal = inTime; }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.I32(this->resource_kind);
        ioWriter.I32(this->num_resources1);
        ioWriter.I32(this->num_resources2);
        ioWriter.I32(this->num_resources3);
        ioWriter.I32(this->l78);
        ioWriter.U32(this->next_renewal);
        ioWriter.U16(this->continent_id);
        ioWriter.U16(this->id);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        return ioReader.I32(this->resource_kind) && ioReader.I32(this->num_resources1)
            && ioReader.I32(this->num_resources2) && ioReader.I32(this->num_resources3) && ioReader.I32(this->l78)
            && ioReader.U32(this->next_renewal) && ioReader.U16(this->continent_id) && ioReader.U16(this->id);
    }

private:
    EEAIForest();
    ~EEAIForest();
};

#endif
