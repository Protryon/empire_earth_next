// C:\EEX\Empire Earth\...\EEAIPathArea.h (RECONSTRUCTION, STUB; file path inferred). A path area
// of the AI's continent map (RTTI EEAIPathArea > EEAIArea @0; ctor 0x18a03, vtable 0x439fac; its id +0x7a =
// EEAIContinentManager::NewPathAreaID()). EEAIPathSector::GetPathAreaAt 0x2bd4d calls the EEAIArea inline Contains
// (0xc6fd) on it; members only where a decompiled body reads them (gaps elsewhere). Requires ee/EEAIArea.h.
#ifndef EE_EEAIPATHAREA_H
#define EE_EEAIPATHAREA_H

// Port: the members are the Rust record EeAStarPathAreaRecord (ee_sim::astar area_kinds.rs), the private base:
// +0x68 the sector ids, +0x78 the continent, +0x7a the id (NewPathAreaID), +0x7c the kind (EEAIGetTileKind), +0x80
// the obstacle's unit (-1), +0x84..+0x8a the region ids by kind (GetContinent 0x22b86: kind 4, 1, 2, 3)
class EEAIPathArea : public EEAIArea, private EeAStarPathAreaRecord {
public:
    // inline ctor (COMDAT 0x18a03, first user EEAIAStarPathAreaGoal::CreateAreas; ??_G 0x18a58)
    EEAIPathArea() { ee_astar_path_area_init(this); }
    virtual ~EEAIPathArea() { ee_astar_path_area_free(this); }

    EeAStarPathAreaRecord* Record() { return this; }
    const EeAStarPathAreaRecord* Record() const { return this; }
    EEAIListView<unsigned short> GetSectorIDs() const
    {
        size_t theCount;
        const uint16_t* theIDs = ee_astar_path_area_sector_ids(this, &theCount);
        return EEAIListView<unsigned short>(theIDs, theCount);
    }
    unsigned short GetContinentID() const { return this->continent_id; }
    void SetContinentID(unsigned short inID) { this->continent_id = inID; }
    unsigned short GetID() const { return this->id; }
    void SetID(unsigned short inID) { this->id = inID; }
    long GetKind() const { return this->kind; }
    void SetKind(long inKind) { this->kind = inKind; }
    long GetUnitID() const { return this->unit_id; }
    void SetUnitID(long inID) { this->unit_id = inID; }
    unsigned short GetSurfaceRegionID() const { return this->surface_region; }
    unsigned short GetWaterBodyID() const { return this->water_body; }
    unsigned short GetLandMassID() const { return this->land_mass; }
    unsigned short GetSpaceRegionID() const { return this->space_region; }
    void SetSurfaceRegionID(unsigned short inID) { this->surface_region = inID; }
    void SetWaterBodyID(unsigned short inID) { this->water_body = inID; }
    void SetLandMassID(unsigned short inID) { this->land_mass = inID; }
    void SetSpaceRegionID(unsigned short inID) { this->space_region = inID; }
    // the members in a saved game (EEAIContinentSave.cpp's writer and reader), in this order
    template <class W> void WriteFields(W& ioWriter) const
    {
        ioWriter.Shorts(std::vector<unsigned short>(this->GetSectorIDs()));
        ioWriter.U16(this->continent_id);
        ioWriter.U16(this->id);
        ioWriter.I32(this->kind);
        ioWriter.I32(this->unit_id);
        ioWriter.U16(this->surface_region);
        ioWriter.U16(this->water_body);
        ioWriter.U16(this->land_mass);
        ioWriter.U16(this->space_region);
    }
    template <class R> bool ReadFields(R& ioReader)
    {
        std::vector<unsigned short> theIDs;
        if (!ioReader.Shorts(theIDs))
            return false;
        ee_astar_path_area_set_sector_ids(this, theIDs.empty() ? NULL : &theIDs[0], theIDs.size());
        return ioReader.U16(this->continent_id) && ioReader.U16(this->id) && ioReader.I32(this->kind)
            && ioReader.I32(this->unit_id) && ioReader.U16(this->surface_region) && ioReader.U16(this->water_body)
            && ioReader.U16(this->land_mass) && ioReader.U16(this->space_region);
    }
    // 0x2baf4: unless present (binary_search), push_back + sort (Rust)
    void AddSectorID(unsigned short inSectorID) { ee_astar_path_area_add_sector_id(this, inSectorID); }

private:
    EEAIPathArea(const EEAIPathArea&);
    EEAIPathArea& operator=(const EEAIPathArea&);
};

#endif
