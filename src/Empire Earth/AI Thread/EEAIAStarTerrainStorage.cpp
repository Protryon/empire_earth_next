// C:\EEX\Empire Earth\AI Thread\EEAIAStarTerrainStorage.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarTerrainStorage, vtable 0x43a108). The terrain searches' node storage: 0x1c666..0x1c740 (EEAIAStarStorage.cpp
// ends at 0x1c666; EEAIChokePoint's ctor starts at 0x1c740). ctor, ??_G (after its first user), dtor. Port: Push,
// Reset and Pop are Rust (ee_astar storage.rs: terrain_add_open, terrain_reset, terrain_remove_best_open).
#include <windows.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"

// FUNCTION: EXE 0x41c666
// Creates the terrain flood-fill storage with the whole grid marked dirty (the Rust storage's constructor)
EEAIAStarTerrainStorage::EEAIAStarTerrainStorage()
    : EEAIAStarFastStorage(EeAStarStorageKind::Terrain)
{
}

// SYNTHETIC: EXE 0x41c68d
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41c6a9
// Destroys the storage (bases only)
EEAIAStarTerrainStorage::~EEAIAStarTerrainStorage()
{
}
