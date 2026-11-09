// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarBoundedStorage, vtable 0x439cf4). The bounded search's node storage: 0x10bc7..0x10d5d (the
// EEAIAStarBoundedPathGoal unit ends at 0x10bc7, EEAIAStarChokePointGoal's code starts at 0x10d5d). ctor, ??_G (after
// its first user), dtor, Virtual10 (Rust: ee_astar storage.rs, GridKind::Bounded).
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"

// FUNCTION: EXE 0x410bc7
// Creates an empty node storage for the bounded path search
EEAIAStarBoundedStorage::EEAIAStarBoundedStorage()
    : EEAIAStarFastStorage(EeAStarStorageKind::Bounded)
{
}

// SYNTHETIC: EXE 0x410be7
// (the scalar deleting destructor)

// FUNCTION: EXE 0x410c03
// Destroys the storage (base members only)
EEAIAStarBoundedStorage::~EEAIAStarBoundedStorage()
{
}

// FUNCTION: EXE 0x410c0e
// Sizes the node and tile-flag grids and clears them, only the span covering the last search bounds plus 2 when they
// were set; then forgets the bounds
void EEAIAStarBoundedStorage::PrepareGrid(long inWidth, long inHeight)
{
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Bounded, false);
}
