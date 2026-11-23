// C:\EEX\Empire Earth\Empire Earth\EECheatCodes.cpp (name from RTTI EECheatCodes; no __FILE__). The cheat codes:
// the table, the chat line's parse, the lookup, the code list and the dispatch are ee_app cheats.rs (RUST_PLAN R6
// "app and world"); their host (the strings, the chat log, the camera and each cheat's world walk) is ee_game gamecpp
// cheats.rs over this unit's one-statement cases.
#include <windows.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EECheatCodes.h"
#include <utility>
#include "rust/sim/app.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp cheats.rs)

// (the table's host, each cheat's work and the camera's goto and find: ee_game gamecpp cheats.rs)

// Creates the cheat code table from the string resources
EECheatCodes::EECheatCodes()
{
    this->mRust = ee_gamecpp_cheats_new();
}

// Destroys the cheat code table
EECheatCodes::~EECheatCodes()
{
    ee_app_cheats_free(this->mRust);
}

// Runs a cheat code for a player (when cheats are enabled): resources, reveal/explore, epochs, instant build,
// research, healing, power, refuel, win/lose, kill units, or the code list
void EECheatCodes::ApplyCheatCode(long inCode, long inPlayerID, long inParameter)
{
    ee_gamecpp_cheats_apply(this->mRust, inCode, inPlayerID, inParameter);
}

// Parses a chat line: "goto x y" and "find [player] id" move the camera; otherwise strips a trailing number as
// the parameter and looks the rest up as a cheat code
void EECheatCodes::ParseCheatCode(const UWideString& inText, long& outCode, long& outParameter)
{
    ee_gamecpp_cheats_parse(this->mRust, (const char*)inText, &outCode, &outParameter);
}

// ------------------------------------------------------------------------- round 10, lane D: the cheat codes

int64_t EECore10OpD5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EECivilization.cpp
double EECore10OpD5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kUnitCategoryResource == 1 && kTechEpochPrehistoric == 0, "gamecpp cheats.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define UN ((EEUnit*)p)
#define PL ((EEPlayer*)p)

// the C++ calls and members of the cheats' bodies in Rust (gamecpp cheats.rs), one statement each
int64_t EECore10OpD4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DcStrBytes: return P64((const char*)*(UWideString*)p);
    case O::DcMultiplayer: return gCommunications->GetIsMultiplayerSession();
    case O::DcCheatsEnabled: return gWorld.mCheatsEnabled;
    case O::DcNumPlayers: return (int64_t)gWorld.mNumPlayers;
    case O::DcChatLog: gUserInterface->AddChatLogEntry(UWideString((const char*)a[0]), 0);
        break;
    case O::DcClosestTile: return gWorld.mMap != NULL && gWorld.mMap->GetClosestTile((GE3DPoint*)a[0], (long*)a[1], (long*)a[2]) != NULL;
    case O::DcWorldUnit: return P64(static_cast<EeUnitsRtsUnitFields*>(gWorld.GetUnit((unsigned long)a[0])));
    case O::DcStats: return P64(static_cast<EeUnitsUnitClassUpgradeFields*>(UN->mStats));
    case O::DcIsViewed: return PL->mViewedUnit == (EEUnit*)a[0];
    case O::DcActiveContains: return PL->mActiveGroup->Contains((EEComplexUnit*)a[0]);
    case O::DcCxKill: ((EEComplexUnit*)p)->Kill(0, 0, 0);
        break;
    case O::DcPlayerKillUnit: PL->KillUnit((EEUnit*)a[0], false);
        break;
    case O::DcExplore: gWorld.mMap->ExploreUnseen(PL);
        break;
    case O::DcNewMakeVisible: return P64(new EEMRecord(kMessageMakeWorldVisible));
    case O::DcSetOriginatingSlot: ((EEMRecord*)p)->SetOriginatingSlot(((EEPlayer*)a[0])->mSlot);
        break;
    case O::DcProcessInput: gCommunications->ProcessLocalUserInput((EEMRecord*)p, kSlotNone);
        break;
    case O::DcClassCount: return EEUnitClassCount();
    case O::DcPlayerUpgradeCount: return (int64_t)PL->mUnitClassUpgrades.size();
    case O::DcPlayerUpgrade: return P64(PL->mUnitClassUpgrades[a[0]]);
    case O::DcCanImprove: return PL->CanImprove((long)a[0], (unsigned long)a[1], NULL);
    case O::DcClassMaxLevel: return ((EEUnitClass*)p)->mImprovements->mMaxLevel;
    case O::DcUpgradeLevel: return ((EEUnitClassUpgrade*)p)->mImprovement.GetLevel((unsigned long)a[0]);
    case O::DcApplyImprovement: PL->ApplyImprovement((long)a[0], (unsigned long)a[1], false);
        break;
    case O::DcEraseQueued: PL->EraseQueuedImprovements((long)a[0]);
        break;
    case O::DcEpochCount: return EEEpochCount();
    case O::DcCurrentEpoch: return PL->mTechnologies->mCurrentEpoch;
    case O::DcSetEpoch: PL->SetEpoch((long)a[0]);
        break;
    case O::DcSetHitPoints: UN->SetHitPoints((long)a[0]);
        break;
    case O::DcEndGame: gPhysics->EndGame(a[0] != 0);
        break;
    case O::DcAddResource: PL->AddResource((long)a[0], (long)a[1]);
        break;
    // the app profile's cheat sweep (TEST ONLY)
    case O::DcHaveCheats: return gCheatCodes != NULL;
    case O::DcParse: gCheatCodes->ParseCheatCode(UWideString((const char*)a[0]), *(long*)a[1], *(long*)a[2]);
        break;
    case O::DcApply: gCheatCodes->ApplyCheatCode((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::DcSwapCheatsEnabled: return std::exchange(gWorld.mCheatsEnabled, a[0] != 0);
    default: return EECore10OpD5(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpD4F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpD5F(inOp, p, a, f);
}
