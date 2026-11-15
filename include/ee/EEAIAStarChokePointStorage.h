// C:\EEX\Empire Earth\AI Thread\EEAIAStarChokePointStorage.h (RECONSTRUCTION; file name inferred). RTTI
// EEAIAStarChokePointStorage > EEAIAStarTerrainStorage; vtable 0x439e3c (EEAIAStarChokePointStorage.cpp
// 0x1540d..0x155c0): ctor 0x1540d (+0x60 = 0 in the init list), ??_G 0x15423, dtor 0x1543f (delete[] +0x60),
// Virtual10 0x1545a. Requires ee/EEAIAStarTerrainStorage.h.
#ifndef EE_EEAIASTARCHOKEPOINTSTORAGE_H
#define EE_EEAIASTARCHOKEPOINTSTORAGE_H

class EEAIAStarChokePointStorage : public EEAIAStarTerrainStorage {
public:
    EEAIAStarChokePointStorage();           // 0x1540d
    virtual ~EEAIAStarChokePointStorage();  // 0x1543f
    // +0x10 0x1545a: sizes the arrays for an inWidth x inHeight search (a same-size call only clears the part the last
    // search touched, grown by one tile)
    virtual void PrepareGrid(long inWidth, long inHeight);

    unsigned long* mChokePointIDs;          // +0x60 (one dword per tile, zeroed when allocated; type inferred)
};

#endif
