// C:\EEX\Empire Earth\Persistent\EEGameStateLoader.cpp (file name inferred: no __FILE__, no RTTI).
// 0x14a009..0x14ac35: the saved-game loader (EEGameStateLoader, ee/EEGameStateLoader.h; EEGameState::Create and
// EESaveLoad 0x14d4fd use it), EELoadPersistent, the scenario flag GetIsLoadingScenario, EEGameState's map/effect/scenario
// setters and the COMDATs first used here. EEPCalamityHelper.cpp / EEPChecksum.cpp come before it,
// EEPMultiplayerHelper.cpp after. Unit flags: the exe module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include <vector>
#include <utility>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NESynchronousClient.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGameState.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "lle/FSChunk.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECommunications.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEPersistentTemplates.h"
#include "ee/EEPChecksum.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESaveLoad.h"
#include "ee/EEGameStateLoader.h"
#include "ee/EEPersistentTag.h"
#include "rust/game/gamecpp.h"

void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362 (EEWorld.cpp's prototype)

// 0x531a40: set while Load reads a scenario (GetIsLoadingScenario's byte; provisional name)
static bool sLoadingScenario;

// Creates a loader that restores a saved game into the given game state
EEGameStateLoader::EEGameStateLoader(EEGameState* inState)
{
    this->mState = inState;
}

// Returns whether a file name ends in the scenario extension (case-insensitive)
bool EEIsScenarioFile(const UString& inFile)
{
    UString theFile;
    UString theExtension;
    bool theResult = false;
    theFile = inFile;
    theFile.ToLower();
    theExtension = theFile.GetSubstring(theFile.GetLength() - gScenarioExtension.GetLength(),
                                        gScenarioExtension.GetLength());
    theExtension.ToLower();
    if (theExtension.Equals(gScenarioExtension))
        theResult = true;
    return theResult;
}

// Returns whether a scenario file is being loaded right now
bool GetIsLoadingScenario()
{
    return sLoadingScenario;
}

// The loader's bodies are ee_game gamecpp persist_save.rs (game core lane, round 9): Load, LoadGame, the chunks'
// restore in tag order, the players' move into the game state, one chunk read out of a file and the map's default
// centre; the methods forward, and their C++ calls and members are EECore9OpC's cases (the scenario flag's here).

// Loads a saved game or scenario file into memory and restores the game state from it; returns whether it worked
bool EEGameStateLoader::Load(FSFile& inFile)
{
    return ee_gamecpp_loader_load(this->mState, &inFile);
}

// Restores the game state from a saved header and its chunks; on failure reports "Load game file is corrupt."
// and clears the state; returns whether it worked
bool EEGameStateLoader::LoadGame(FSFile& inHeaderFile, FSFile& inFile)
{
    return ee_gamecpp_loader_load_game(this->mState, &inHeaderFile, &inFile);
}

// Reads one chunk (by tag) out of a saved game file without loading the rest; returns the read error
long EELoadPersistent(const UString& inFile, long inTagID, FSPersistent*& outPersistent)
{
    return ee_gamecpp_ps_load_persistent(&inFile, inTagID, reinterpret_cast<void**>(&outPersistent));
}

// Centres the camera on the middle of the map, if there is one
void EEGameState::SetDefaultMapCenter()
{
    ee_gamecpp_st_default_map_center(this);
}

// Replaces the game state's effect manager, deleting the old one
void EEGameState::SetEffectManager(EEEffectManager* inManager)
{
    delete std::exchange(this->mEffectManager, inManager);
}

// Replaces the game state's map, deleting the old one
void EEGameState::SetMap(EEMap* inMap)
{
    delete std::exchange(this->mMap, inMap);
}

// Replaces the game state's scenario manager, deleting the old one
void EEGameState::SetScenarioManager(EEScenarioManager* inManager)
{
    delete std::exchange(this->mScenarioManager, inManager);
}

// ------------------------------------------------------------------------- the loader's C++ (gamecpp persist_save.rs)

int64_t EEPHelpersOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEPHelpersOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// The loader's C++ (EEPWorldHelper.cpp's EEPWorldOp default): one statement each
int64_t EEPLoaderOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PsSetLoadingScenario: sLoadingScenario = EEIsScenarioFile(((FSFile*)p)->GetFile());
        break;
    case O::PsClearLoadingScenario: sLoadingScenario = false;
        break;
    default: return EEPHelpersOp(inOp, p, a, f);
    }
    return 0;
}

double EEPLoaderOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EEPHelpersOpF(inOp, p, a, f);
}
