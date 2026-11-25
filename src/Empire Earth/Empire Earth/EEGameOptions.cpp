// C:\EEX\Empire Earth\Empire Earth\EEGameOptions.cpp (__FILE__ 0x4bf9c4, the ctors' CreateReadWriteLock asserts).
// 0x12f4b2..0x1304e4: the empty player slot's `$E` chain, EEGameOptionsPlayer, EEGameOptions and
// the two table-size helpers; EEGameState::Clear 0x1304e4 starts the next unit. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include <vector>
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEDatabaseTables.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// (0x529b38, the empty slot every constructor and SetDefaults copied into the sixteen players: a default slot,
// ee_game gamecpp options.rs's EoSlotEmpty)

#include "ee/EEStartingResources.h"
#include "rust/game/gamecpp.h"   // round 11, lane B (ee_game gamecpp options.rs)

// Creates an empty, closed player slot with no name, colour, team or starting citizens
EEGameOptionsPlayer::EEGameOptionsPlayer()
{
    ee_app_options_player_default(this);
    this->mName = gEmptyString;
}

// Creates default game options (random epochs, normal speed and map size, unit limit 50, three wonders to win)
// with sixteen empty player slots (ee_game gamecpp options.rs; the limits, the strings and the slots)
EEGameOptions::EEGameOptions() { ee_gamecpp_options_init(this); }

// Creates a copy of other game options, with its own lock (the copy keeps the defaults' mB407: operator= leaves it)
EEGameOptions::EEGameOptions(const EEGameOptions& inOther) { ee_gamecpp_options_copy(this, &inOther); }

// Destroys the game options, releasing their lock
EEGameOptions::~EEGameOptions()
{
    TSScheduler::DestroyReadWriteLock(this->mLock);
}

// Copies other game options, replacing each out-of-range setting (load type, resources, variant, epochs, map,
// size, difficulty, speed, wonders, unit limit) with a default (ee_app game_options.rs assign); copies names, flags
// and all sixteen slots as they are. A new game's map type must be a known generator's, else the first one's.
EEGameOptions& EEGameOptions::operator=(const EEGameOptions& inOther)
{
    ee_gamecpp_options_assign(this, &inOther);
    return *this;
}

// Returns the map's width and height in tiles for the map size setting (0 x 0 for an unknown size)
// (gameoptions.yaml map_sizes: 42, 58, 75, 105, 160, 220 in the original)
void EEGameOptions::GetMapDimensions(long& outWidth, long& outHeight) { ee_gamecpp_options_map_dimensions(this, &outWidth, &outHeight); }

// Returns the campaign scenario's map name when the options name a campaign with a checksum and a map, else ""
bool EEGameOptions::GetCampaignMapName(UWideString& outName) { return ee_gamecpp_options_campaign_map_name(this, &outName); }

// Returns whether civilizations may be bought: not in the scenario editor and not in a scenario game
bool EEGameOptions::CanBuyCivilization() { return ee_gamecpp_options_can_buy_civilization(this); }

// Resets the options to the lobby's defaults (all resources, random epochs, first map type, cheats on, victory off,
// the preferred difficulty) with sixteen empty player slots
void EEGameOptions::SetDefaults() { ee_gamecpp_options_set_defaults(this); }

// Reads the game options and the sixteen slots from a saved game (ee_app game_options.rs load); returns the first
// read error
long EEGameOptions::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_options_load(this, inMajorVersion, inMinorVersion, &inFile);
}

// Writes the sixteen player slots and the game options to a saved game (ee_app game_options.rs save); returns the
// first write error
long EEGameOptions::Save(FSFile& outFile) { return ee_gamecpp_options_save(this, &outFile); }

// Returns the number of game variants
unsigned long EEGetNumberOfGameVariants()
{
    return gGameVariants.size();
}

// Returns the number of starting resource levels
unsigned long EEGetNumberOfResourceLevels()
{
    return EEStartingResources::sTable.size();
}
