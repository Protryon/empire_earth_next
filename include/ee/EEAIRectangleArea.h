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
    // (EEAIRectangleArea.cpp 0x2c79f..0x2cc87): ctor 0x2c79f, ??_G 0x2cc65 (a user-declared empty dtor)
    EEAIRectangleArea() : EeAStarRectRecord() {}
    virtual ~EEAIRectangleArea() {}
    // +0x20 0x2c939: EEAIArea's random point; when its tile is off the map, a random one of the tiles on the map
    virtual U2DPoint<long> GetRandomPoint();
    // +0x2c 0x2c929 (ret 0xc: two ignored dword arguments): EEAIArea::GetRandomPoint() (provisional name)
    virtual U2DPoint<long> GetBaseRandomPoint(long inArg1, long inArg2);
    // 0x2c7df: counts the rectangle's tiles per continent category; the continent with most of them (provisional name)
    unsigned short UpdateContinents();
    EeAStarRectRecord* Record() { return this; }
    const EeAStarRectRecord* Record() const { return this; }
    const U2DRectangle<long>& GetRect() const { return reinterpret_cast<const U2DRectangle<long>&>(this->rect); }
    void SetRect(const U2DRectangle<long>& inRect) { reinterpret_cast<U2DRectangle<long>&>(this->rect) = inRect; }
    long GetScenarioAreaID() const { return this->scenario_area_id; }
    void SetScenarioAreaID(long inID) { this->scenario_area_id = inID; }
    long GetNumLandTiles() const { return this->num_land_tiles; }
    long GetNumWaterTiles() const { return this->num_water_tiles; }
    long GetNumSpaceTiles() const { return this->num_space_tiles; }
    void SetNumLandTiles(long inCount) { this->num_land_tiles = inCount; }
    void SetNumWaterTiles(long inCount) { this->num_water_tiles = inCount; }
    void SetNumSpaceTiles(long inCount) { this->num_space_tiles = inCount; }
    unsigned short GetContinentID() const { return this->continent_id; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.Rect(this->GetRect());
        ioWriter.I32(this->scenario_area_id);
        ioWriter.I32(this->num_land_tiles);
        ioWriter.I32(this->num_water_tiles);
        ioWriter.I32(this->num_space_tiles);
        ioWriter.U16(this->continent_id);
        ioWriter.U16(this->id);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        return ioReader.Rect(reinterpret_cast<U2DRectangle<long>&>(this->rect)) && ioReader.I32(this->scenario_area_id)
            && ioReader.I32(this->num_land_tiles) && ioReader.I32(this->num_water_tiles) && ioReader.I32(this->num_space_tiles)
            && ioReader.U16(this->continent_id) && ioReader.U16(this->id);
    }
};

#endif
