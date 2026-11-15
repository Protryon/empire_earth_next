// C:\EEX\Empire Earth\AI Thread\EEAIAStarContinentStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarContinentStorage, vtable 0x439e84). The continent search's node storage: 0x15a47..0x15c36
// (EEAIAStarChokePointStorage.cpp ends at 0x15a47; EEAIAStarFastStorage's ctor starts at 0x15c36). ctor, ??_G (after
// its first user), dtor, Virtual10.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarContinentStorage.h"

// FUNCTION: EXE 0x415a47
// Creates the continent search storage with no continent tile grid
EEAIAStarContinentStorage::EEAIAStarContinentStorage()
    : mTiles60(NULL)
{
}

// SYNTHETIC: EXE 0x415a5d
// (the scalar deleting destructor)

// FUNCTION: EXE 0x415a79
// Destroys the storage, freeing its continent tile grid
EEAIAStarContinentStorage::~EEAIAStarContinentStorage()
{
    delete[] this->mTiles60;
}

// NEAR: as EEAIAStarPathSectorStorage::PrepareGrid 0x1b829 (the same shape; register choice of inWidth/inHeight)
// FUNCTION: EXE 0x415a99
// Sizes the continent tile, node and flag grids (reallocating on a size change) and clears the node and flag
// grids, only the span covering the dirty bounds plus 1 when unchanged; then forgets the dirty bounds
void EEAIAStarContinentStorage::PrepareGrid(long inWidth, long inHeight)
{
    // the id grid is C++ (its readers index it); the flags and the node map are Rust, cleared there from the dirty
    // bounds grown by one (all of them when this grid was just allocated)
    bool theResized = false;
    long theOldWidth = this->mWidth;
    long theOldHeight = this->mHeight;
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    if (this->mTiles60 == NULL || theOldWidth != inWidth || theOldHeight != inHeight) {
        delete[] this->mTiles60;
        this->mTiles60 = new EEAIAStarContinentTile[this->mHeight * this->mWidth];
        memset(this->mTiles60, 0, this->mHeight * this->mWidth * 2);
        theResized = true;
    }
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Dirty, theResized);
}
