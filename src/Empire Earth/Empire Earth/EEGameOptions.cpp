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
#include "rust/game/gamecpp.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// (0x529b38, the empty slot every constructor and SetDefaults copied into the sixteen players: a default slot,
// ee_game gamecpp options.rs's EoSlotEmpty)

#include "ee/EEStartingResources.h"
#include "rust/game/app.h"

// Creates an empty, closed player slot with no name, colour, team or starting citizens
EEGameOptionsPlayer::EEGameOptionsPlayer()
{
    ee_app_options_player_default(this);
    this->mName = gEmptyString;
}

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

// out of line from ee/EEGameOptions.h (its Rust function headers stay out of it)
EEGameOptions::EEGameOptions() { ee_gamecpp_options_init(this); }
EEGameOptions::EEGameOptions(const EEGameOptions& inOther) { ee_gamecpp_options_copy(this, &inOther); }

long EEGameOptions::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{ return ee_gamecpp_options_load(this, inMajorVersion, inMinorVersion, &inFile); }

long EEGameOptions::Save(FSFile& outFile) { return ee_gamecpp_options_save(this, &outFile); }
bool EEGameOptions::CanBuyCivilization() { return ee_gamecpp_options_can_buy_civilization(this); }
