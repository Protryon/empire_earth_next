// C:\EEX\Empire Earth\Persistent\EEPPlayerHelper.cpp (__FILE__ 0x4c17c0, in CreatePlayer's and the player-type
// ESVERIFYs). The saved players: 0x14b22d..0x14b7a5. Unit flags: the exe's module set.
// Data: the vtable 0x442288; the four class static keys share the guard byte 0x531b00.
// CreatePlayer and GetPlayerType are file statics (cdecl), each emitted right before the function that follows its
// first user.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/RTSWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EECPEngine.h"
#include "rust/game/gamecpp.h"


// The saved players' bodies are ee_game gamecpp persist_parts.rs (game core lane, round 9): the constructors, the
// checksum, the loaded index and players, Load (each player with its type; a computer player's engine, AI id, the
// next engine's state and the per-unit Lua state) and Save. The methods forward; their C++ calls and members are
// EEPHelpersOp's cases below (CreatePlayer and GetPlayerType were file statics: cases and Rust now).

// Creates an empty saved-players chunk for loading (three player slots, local player 0)
EEPPlayerHelper::EEPPlayerHelper()
    : FSPersistent(kPersistentPlayers)
{
    ee_gamecpp_pph_init(this);
}

// Creates the saved-players chunk for saving from the world's existing players, slot count and current player
EEPPlayerHelper::EEPPlayerHelper(EEWorld& inWorld)
    : FSPersistent(kPersistentPlayers)
{
    ee_gamecpp_pph_from_world(this, &inWorld);
}

// Creates an empty saved-players chunk for loading (persistent factory)
FSPersistent* EEPPlayerHelper::CreateDefault() const
{
    return new EEPPlayerHelper;
}

// Adds every player's checksum and the local player index to a running checksum
void EEPPlayerHelper::GetChecksum(ESChecksum& ioChecksum) const
{
    ee_gamecpp_pph_checksum(this, &ioChecksum);
}

// Returns the loaded local player index; error 13 unless the chunk was loaded
long EEPPlayerHelper::GetLocalPlayerIndex(long& outIndex)
{
    return ee_gamecpp_pph_local_index(this, &outIndex);
}

// Returns the loaded players; error 13 unless the chunk was loaded
long EEPPlayerHelper::GetPlayers(std::vector<EEPlayer*>& outPlayers)
{
    return ee_gamecpp_pph_players(this, &outPlayers);
}

// Reads the slot count, the local player index and each player (human or computer) from a saved game, then fills
// each player's ally list with the allies that share line of sight; returns the first read error
long EEPPlayerHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_pph_load(this, inMajorVersion, inMinorVersion, &inFile);
}

// Writes the player count, slot count, local player index and each player with its type to a save; fails for a
// loading chunk or no players
long EEPPlayerHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_pph_save(this, inMajorVersion, inMinorVersion, &outFile);
}

// Does nothing (no address: empty, ICF-folded into 0x157269; EEGameStateLoader::RestorePlayers 0x14a98d calls it
// with its EEGameState: `push [esi]; mov ecx, [ebp+8] (the EEPPlayerHelper*); call 0x157269`, a direct thiscall on
// the helper; provisional name)
void EEPPlayerHelper::Fn157269(EEGameState* inState)
{
}

// ------------------------------------------------------------------------- the saved players' C++ (gamecpp persist_parts.rs)


static_assert(kEECPEngineClassic == 0 && kEECPEngineNext == 1, "gamecpp persist_parts.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define PPH ((EEPPlayerHelper*)p)
#define PL ((EEPlayer*)p)
#define CP ((EEComputerPlayer*)p)
#define STR (*(std::string*)p)
#define BYTES std::string((const char*)a[0], (size_t)a[1])

