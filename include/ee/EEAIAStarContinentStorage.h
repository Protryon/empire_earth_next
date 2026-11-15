// C:\EEX\Empire Earth\AI Thread\EEAIAStarContinentStorage.h (RECONSTRUCTION; file name inferred).
// RTTI EEAIAStarContinentStorage > EEAIAStarTerrainStorage; vtable 0x439e84 (EEAIAStarContinentStorage.cpp
// 0x15a47..0x15c36): ctor 0x15a47 (+0x60 = 0 in the init list), ??_G 0x15a5d, dtor 0x15a79 (delete[] +0x60 through the
// element's ??_E 0x17207), Virtual10 0x15a99. Requires ee/EEAIAStarTerrainStorage.h.
#ifndef EE_EEAIASTARCONTINENTSTORAGE_H
#define EE_EEAIASTARCONTINENTSTORAGE_H

// One per tile (4 bytes; new[] keeps a count, so the dtor is user-declared; the ctor zeroes +0 and +2). Names
// (provisional name)
struct EEAIAStarContinentTile {
    EEAIAStarContinentTile() : mKind(0), mID(0) {}
    ~EEAIAStarContinentTile() {}

    unsigned char mKind;                    // +0x00 (the continent goal's kind)
    unsigned short mID;                     // +0x02 (the continent id)
};

class EEAIAStarContinentStorage : public EEAIAStarTerrainStorage {
public:
    EEAIAStarContinentStorage();            // 0x15a47
    virtual ~EEAIAStarContinentStorage();   // 0x15a79
    // +0x10 0x15a99: sizes the tile arrays for an inWidth x inHeight search (a same-size call only clears the part the
    // last search touched, grown by one tile)
    virtual void PrepareGrid(long inWidth, long inHeight);

    EEAIAStarContinentTile* mTiles60;       // +0x60 (one per tile)
};

#endif
