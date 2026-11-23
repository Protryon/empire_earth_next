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
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPersistentTag.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"
#include "ee/EESaveGameFiles.h"
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

int64_t EEPHeaderOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kEECPEngineClassic == 0 && kEECPEngineNext == 1, "gamecpp persist_parts.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define PPH ((EEPPlayerHelper*)p)
#define PL ((EEPlayer*)p)
#define CP ((EEComputerPlayer*)p)
#define STR (*(std::string*)p)
#define BYTES std::string((const char*)a[0], (size_t)a[1])

// The saved players' C++ calls and members (EEGameStateLoader.cpp's EEPLoaderOp default): one statement each
int64_t EEPHelpersOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PphFields: return P64(&PPH->mLocalPlayerIndex);
    case O::PphPush: PPH->mPlayers.push_back((EEPlayer*)a[0]);
        break;
    case O::PphCopyPlayers: *(std::vector<EEPlayer*>*)a[0] = PPH->mPlayers;
        break;
    case O::PphPlayerChecksum: PL->GetChecksum(*(ESChecksum*)a[0]);
        break;
    case O::PphChecksumLocal: ((ESChecksum*)p)->AddChecksum("EEPPlayerHelper::mLocalPlayerIndex", (unsigned long)a[0]);
        break;
    case O::PphNewHuman: return P64(static_cast<EEPlayer*>(new EEHumanPlayer));
    case O::PphNewComputer: return P64(static_cast<EEPlayer*>(new EEComputerPlayer));
    case O::PphVerifyType3: ESVERIFY_AT(false, 251);
        break;
    case O::PphPlayerLoad: return PL->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PphPlayerSave: return PL->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PphComputerOf: return P64(PL->mComputerPlayer);
    case O::PphHasHuman: return PL->mHumanPlayer != NULL;
    case O::PphVerifyPlayerType: ESVERIFY_AT(a[0] != 0, 274);
        break;
    case O::PphCpSetEngineNext: CP->mEngine = kEECPEngineNext;
        break;
    case O::PphCpEngine: return CP->mEngine;
    case O::PphAiKnown: return EECPAIByID(BYTES) != NULL;
    case O::PphLogUnknownAi: EECPEngineLog("the saved game's player %lu plays the AI '%s', which Data/ai/ais.json does not list: the game cannot be loaded", PL->mPlayerID, BYTES.c_str());
        break;
    case O::PphCpSetAi: EECPNextSetAI(CP, BYTES);
        break;
    case O::PphCpLoadState: return EECPNextLoadState(CP, BYTES);
    case O::PphCpUnitsLoad: return EECPUnitsLoad(CP, BYTES);
    case O::PphHasUnitsLuaState: return EESaveHasUnitsLuaState((unsigned long)a[0], (unsigned long)a[1]);
    case O::PphIsAllied: return PL->IsAlliedWith((EEPlayer*)a[0]);
    case O::PphAddAlly: PL->mAllies.push_back((EEPlayer*)a[0]);
        break;
    case O::PphStrNew: return P64(new std::string);
    case O::PphStrDelete: delete &STR;
        break;
    case O::PphStrSize: return (int64_t)STR.size();
    case O::PphStrData: return P64(STR.data());
    case O::PphCpEngineAi: *(std::string*)a[0] = EECPEngineAI(CP);
        break;
    case O::PphClassicAiPresent: return EECPClassicAI() != NULL;
    case O::PphClassicAiId: *(std::string*)a[0] = EECPAIID(EECPClassicAI());
        break;
    case O::PphCpSaveState: EECPNextSaveState(CP, *(std::string*)a[0]);
        break;
    case O::PphCpUnitsSave: EECPUnitsSave(CP, *(std::string*)a[0]);
        break;
    default: return EEPHeaderOp(inOp, p, a, f);
    }
    return 0;
}

double EEPHelpersOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore9OpDF(inOp, p, a, f);
}
