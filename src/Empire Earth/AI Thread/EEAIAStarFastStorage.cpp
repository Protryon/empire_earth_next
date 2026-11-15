// C:\EEX\Empire Earth\AI Thread\EEAIAStarFastStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarFastStorage, vtable 0x439e9c). The A* storage with a node map: 0x15c36..0x15dae (EEAIAStarContinentStorage's
// Virtual10 ends at 0x15c36, EEAIAStarGraphContinentalChainGoal's ctor starts at 0x15dae). ctor, ??_G (after its first
// user), dtor, Virtual10. Port: AddClosed/RemoveClosed and the node map are Rust (ee_astar storage.rs).
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"

// FUNCTION: EXE 0x415c36
// Creates the A* storage of a node-map kind
EEAIAStarFastStorage::EEAIAStarFastStorage(EeAStarStorageKind inKind)
    : EEAIAStarStorage(inKind)
{
}

// SYNTHETIC: EXE 0x415c4c
// (the scalar deleting destructor)

// FUNCTION: EXE 0x415c68
// Destroys the storage
EEAIAStarFastStorage::~EEAIAStarFastStorage()
{
}

// FUNCTION: EXE 0x415d1c
// Sizes the node map and tile flags to the grid and clears both
void EEAIAStarFastStorage::PrepareGrid(long inWidth, long inHeight)
{
    this->mWidth = inWidth;
    this->mHeight = inHeight;
    ee_astar_prepare_grid(this->mCore, inWidth, inHeight, EeAStarGridKind::Fast, false);
}
