// C:\EEX\Empire Earth\AI Thread\EEAIAStarFastStorage.h (RECONSTRUCTION; file name inferred).
// RTTI EEAIAStarFastStorage > EEAIAStarStorage; vtable 0x439e9c (EEAIAStarFastStorage.cpp 0x15c36..0x160b7): ctor
// 0x15c36 (+0x4c = 0 in the init list), ??_G 0x15c4c, dtor 0x15c68 (delete[] +0x4c), Add, Remove, Virtual10 0x15d1c.
// The storage with a node per tile (the fast closed list, the node map): Rust (ee_astar, its Bounded and Terrain
// kinds). Requires ee/EEAIAStarStorage.h.
#ifndef EE_EEAIASTARFASTSTORAGE_H
#define EE_EEAIASTARFASTSTORAGE_H
#include "ee/EEAIKindIDs.h"
#include "rust/ee_astar.h"

class EEAIAStarFastStorage : public EEAIAStarStorage {
public:
    explicit EEAIAStarFastStorage(EeAStarStorageKind inKind);   // 0x15c36
    virtual ~EEAIAStarFastStorage();        // 0x15c68
    // +0x10 0x15d1c: sizes the node arrays for an inWidth x inHeight search, then clears them
    virtual void PrepareGrid(long inWidth, long inHeight);
};

#endif
