// C:\EEX\Empire Earth\AI Thread\EEAIAStarTerrainStorage.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarTerrainStorage > EEAIAStarFastStorage; vtable 0x43a108 (EEAIAStarTerrainStorage.cpp 0x1c666..0x1d846):
// ctor 0x1c666 (the +0x50 bounds' inline ctor, then Reset 0x1c6ee), ??_G 0x1c68d, dtor 0x1c6a9
// (no members to destroy: the vptr, then the base dtor). Slot +0x10 is the base's (0x15d1c). The flood fills' storage:
// the open list a stack; it and the dirty bounds are Rust (ee_astar's Terrain kind; Get/SetDirtyBounds).
// Requires ee/EEAIAStarFastStorage.h.
#ifndef EE_EEAIASTARTERRAINSTORAGE_H
#define EE_EEAIASTARTERRAINSTORAGE_H

class EEAIAStarTerrainStorage : public EEAIAStarFastStorage {
public:
    EEAIAStarTerrainStorage();              // 0x1c666
    virtual ~EEAIAStarTerrainStorage();     // 0x1c6a9
};

#endif
