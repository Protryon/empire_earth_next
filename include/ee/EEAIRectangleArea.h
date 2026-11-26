// C:\EEX\Empire Earth\...\EEAIRectangleArea.h (RECONSTRUCTION; file path inferred). An AI map area
// (RTTI EEAIRectangleArea > EEAIArea @0; vtable 0x43a540, 12 slots). Only the base is declared: EECPILocation::Update 0x80a57 reads the
// EEAIArea serial of what EEAIContinentManager's lookup returns. Requires ee/EEAIArea.h, lle/U2DRectangle.h.
#ifndef EE_EEAIRECTANGLEAREA_H
#define EE_EEAIRECTANGLEAREA_H
#include "rust/sim/astar.h"

// Port: the members are the Rust record EeAStarRectRecord (ee_sim::astar area_kinds.rs), the private base: +0x68 the
// rectangle, +0x78 the scenario area, +0x7c/+0x80/+0x84 the tiles by continent category (EECPCombatManager 0x47306c
// walks the rectangle when its count is positive, unless +0x88 is the continent asked for), +0x88 the continent,
// +0x8a its mRectangleAreas index (0x1f3a0)
class EEAIRectangleArea : public EEAIArea, private EeAStarRectRecord {
public:
    // a Rust object (ee_sim::astar objects.rs): made and freed there, never by a C++ constructor
    static EEAIRectangleArea* New() { return (EEAIRectangleArea*)(uintptr_t)ee_astar_object_new(EeAStarCmTable::RectangleAreas); }
    static void Delete(EEAIRectangleArea* inObject) { ee_astar_object_delete(EeAStarCmTable::RectangleAreas, (uint64_t)(uintptr_t)inObject); }
    // +0x20 0x2c939: EEAIArea's random point; when its tile is off the map, a random one of the tiles on the map
    U2DPoint<long> GetRandomPoint() { return EEAIArea::PointOf(ee_astar_rect_random_point((uint64_t)(uintptr_t)this)); }
    EeAStarRectRecord* Record() { return this; }
    const EeAStarRectRecord* Record() const { return this; }
    const U2DRectangle<long>& GetRect() const { return reinterpret_cast<const U2DRectangle<long>&>(this->rect); }
    void SetRect(const U2DRectangle<long>& inRect) { reinterpret_cast<U2DRectangle<long>&>(this->rect) = inRect; }
    unsigned short GetContinentID() const { return this->continent_id; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }

private:
    EEAIRectangleArea();
    ~EEAIRectangleArea();
};

#endif
