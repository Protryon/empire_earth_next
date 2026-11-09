// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedStorage.h (RECONSTRUCTION; file name inferred). The
// bounded search's node storage (RTTI EEAIAStarBoundedStorage > EEAIAStarFastStorage; vtable 0x439cf4; sizeof 0x60;
// EEAIAStarBoundedStorage.cpp 0x10bc7..0x10d5d): ctor 0x10bc7 (the +0x50 bounds' inline ctor), ??_G 0x10be7, dtor
// 0x10c03, Virtual10 0x10c0e. Its open list keeps the node map in step: Rust (ee_astar's Bounded kind).
// Requires ee/EEAIAStarFastStorage.h.
#ifndef EE_EEAIASTARBOUNDEDSTORAGE_H
#define EE_EEAIASTARBOUNDEDSTORAGE_H

class EEAIAStarBoundedStorage : public EEAIAStarFastStorage {
public:
    EEAIAStarBoundedStorage();
    virtual ~EEAIAStarBoundedStorage();
    // +0x10 0x10c0e: sizes the node arrays; when the last search's bounds are valid only those tiles, grown by two,
    // are cleared
    virtual void PrepareGrid(long inWidth, long inHeight);

    EEAIAStarBounds mSearchBounds;          // +0x50 (written by the goal and the path manager, read by nothing)
};

#endif
