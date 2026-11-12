// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphTerrainStorage.h (RECONSTRUCTION, STUB; file name inferred). RTTI
// EEAIAStarGraphTerrainStorage > EEAIAStarGraphStorage; vtable 0x43a2c8 (??_G 0x285c1; slots
// +0x04..+0x10 are the base's). Only the base is declared (its own code is in another unit's range). Requires
// ee/EEAIAStarGraphStorage.h.
//
// Port: its AddOpen / UpdateOpen / RemoveBestOpen (the fast open list: Push 0x16bb5 / nothing / Pop 0x16be1, which the
// graph machine's steps 0x27434/0x274a3 called directly) are the Rust storage's Terrain kind (ee_astar graph.rs).
#ifndef EE_EEAIASTARGRAPHTERRAINSTORAGE_H
#define EE_EEAIASTARGRAPHTERRAINSTORAGE_H
#include "rust/ee_astar.h"

class EEAIAStarGraphTerrainStorage : public EEAIAStarGraphStorage {
public:
    EEAIAStarGraphTerrainStorage() : EEAIAStarGraphStorage(EeAStarGraphKind::Terrain) {}
    // a user dtor: ??_G 0x285c1 stores this vtable before calling the base dtor
    virtual ~EEAIAStarGraphTerrainStorage() {}
};

#endif
