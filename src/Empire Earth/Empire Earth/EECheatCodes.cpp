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

// (round 10, lane D: the cheat codes' cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
