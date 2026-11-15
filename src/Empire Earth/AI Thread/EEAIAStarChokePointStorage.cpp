// C:\EEX\Empire Earth\AI Thread\EEAIAStarChokePointStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarChokePointStorage, vtable 0x439e3c). The choke point search's node storage: 0x1540d..0x155c0
// (EEAIAStarChokePointGoal.cpp ends at 0x1540d; EEAIAStarContinentGoal's code starts at 0x155c0). ctor, ??_G (after
// its first user), dtor, Virtual10.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarChokePointStorage.h"

// FUNCTION: EXE 0x41540d
// Creates the choke point search storage with no choke point id grid
EEAIAStarChokePointStorage::EEAIAStarChokePointStorage()
    : mChokePointIDs(NULL)
{
}

// SYNTHETIC: EXE 0x415423
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41543f
// Destroys the storage, freeing its choke point id grid
EEAIAStarChokePointStorage::~EEAIAStarChokePointStorage()
{
    delete[] this->mChokePointIDs;
}

// NEAR: as EEAIAStarPathSectorStorage::PrepareGrid 0x1b829 (the same shape without the node-array test)
// FUNCTION: EXE 0x41545a
// Sizes the id, node and tile-flag grids (reallocating, ids zeroed, on a size change) and clears the node and flag
// grids, only the span covering the dirty bounds plus 1 when unchanged; then forgets the dirty bounds
void EEAIAStarChokePointStorage::PrepareGrid(long inWidth, long inHeight)
{
    // the id grid is C++ (its readers index it); the flags and the node map are Rust, cleared there from the dirty
    // bounds grown by one (all of them when this grid was just allocated)
    bool theResized = false;
    long theOldWidth = this->mWidth;
    long theOldHeight = this->mHeight;
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    if (this->mChokePointIDs == NULL || theOldWidth != inWidth || theOldHeight != inHeight) {
        delete[] this->mChokePointIDs;
        this->mChokePointIDs = new unsigned long[this->mHeight * this->mWidth];
        memset(this->mChokePointIDs, 0, this->mHeight * this->mWidth * sizeof(unsigned long));
        theResized = true;
    }
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Dirty, theResized);
}
