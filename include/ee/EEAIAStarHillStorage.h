// C:\EEX\Empire Earth\AI Thread\EEAIAStarHillStorage.h (RECONSTRUCTION; file name inferred).
// RTTI EEAIAStarHillStorage > EEAIAStarTerrainStorage; vtable 0x439f64 (EEAIAStarHillStorage.cpp 0x171b5..0x173cc):
// ctor 0x171b5 (+0x60 = 0 in the init list), ??_G 0x171cb, dtor 0x171e7 (delete[] +0x60 through the element's ??_E,
// ICF-folded into EEAIAStarContinentTile's 0x17207), Virtual10 0x17239. Requires ee/EEAIAStarTerrainStorage.h.
#ifndef EE_EEAIASTARHILLSTORAGE_H
#define EE_EEAIASTARHILLSTORAGE_H

// One per tile (4 bytes; new[] keeps a count, so the dtor is user-declared; the ctor zeroes +0 and +2 as words).
// Names are provisional.
struct EEAIAStarHillTile {
    EEAIAStarHillTile() : mContinentID(0), mID(0) {}
    ~EEAIAStarHillTile() {}

    unsigned short mContinentID;            // +0x00 (the hill goal's continent)
    unsigned short mID;                     // +0x02 (the hill id)
};

class EEAIAStarHillStorage : public EEAIAStarTerrainStorage {
public:
    EEAIAStarHillStorage();                 // 0x171b5
    virtual ~EEAIAStarHillStorage();        // 0x171e7
    // +0x10 0x17239: sizes the tile arrays for an inWidth x inHeight search (a same-size call only clears the part the
    // last search touched, grown by one tile)
    virtual void PrepareGrid(long inWidth, long inHeight);

    EEAIAStarHillTile* mTiles60;            // +0x60 (one per tile)
};

#endif
