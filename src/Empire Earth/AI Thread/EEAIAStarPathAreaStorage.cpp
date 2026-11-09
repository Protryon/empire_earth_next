// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathAreaStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathAreaStorage, vtable 0x439fdc). The path-area search's node storage: 0x19072..0x190bf (the previous
// unit's COMDATs end at 0x19072, EEAIAStarPathfindingGoal's code starts at 0x190bf). ctor, ??_G (after its first
// user), dtor, then Virtual10 (ICF-folded into EEAIAStarPathSectorStorage's identical 0x1b829, so no address of its own;
// the path sectors read this storage's mP60 through EEAIAStarPathSectorGoal::mPathAreaStorage).
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEAIAStarPathAreaStorage.h"

// FUNCTION: EXE 0x419072
// Creates the path-area search storage with no per-tile area id grid yet
EEAIAStarPathAreaStorage::EEAIAStarPathAreaStorage()
    : mPathAreaIDs(NULL)
{
}

// SYNTHETIC: EXE 0x419088
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4190a4
// Destroys the storage, freeing its per-tile path area ids
EEAIAStarPathAreaStorage::~EEAIAStarPathAreaStorage()
{
    delete[] this->mPathAreaIDs;
}

// (no address: ICF-folded into EEAIAStarPathSectorStorage::PrepareGrid 0x1b829, the same code)
// Sizes the node arrays and the per-tile path-area ids for an inWidth x inHeight search (reallocating on a size
// change) and clears the node and tile flags, only the last search's dirty bounds plus one when it can
void EEAIAStarPathAreaStorage::PrepareGrid(long inWidth, long inHeight)
{
    // the id grid is C++ (its readers index it); the flags and the node map are Rust, cleared there from the dirty
    // bounds grown by one (all of them when this grid was just allocated)
    bool theResized = false;
    long theOldWidth = this->mWidth;
    long theOldHeight = this->mHeight;
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    if (this->mPathAreaIDs == NULL || theOldWidth != inWidth || theOldHeight != inHeight) {
        delete[] this->mPathAreaIDs;
        this->mPathAreaIDs = new unsigned short[this->mHeight * this->mWidth];
        memset(this->mPathAreaIDs, 0, this->mHeight * this->mWidth * sizeof(unsigned short));
        theResized = true;
    }
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Dirty, theResized);
}
