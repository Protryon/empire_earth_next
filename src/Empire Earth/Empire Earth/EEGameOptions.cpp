// C:\EEX\Empire Earth\Empire Earth\EEGameOptions.cpp (__FILE__ 0x4bf9c4, the ctors' CreateReadWriteLock asserts).
// 0x12f4b2..0x1304e4: the empty player slot's `$E` chain, EEGameOptionsPlayer, EEGameOptions and
// the two table-size helpers; EEGameState::Clear 0x1304e4 starts the next unit. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include <vector>
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGameVariant.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EERulesWorld.h"

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
