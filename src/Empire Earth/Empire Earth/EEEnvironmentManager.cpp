// C:\EEX\Empire Earth\Empire Earth\EEEnvironmentManager.cpp (name from RTTI EEEnvironmentManager; no __FILE__).
// 0x12ed4e..0x12f4b2: the weather manager (ctor, ??_G, dtor, the storm test, CreateDefault, the update, Load,
// RepairPointers, Save, the effect switch, the storm list, the wind). EEEMStorm.cpp ends at 0x12ed4e, EEGameOptions.cpp
// starts at 0x12f4b2. Unit flags: the exe's module set (provisional names).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/U2DRectangle.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEStormType.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp weather.rs)

// Creates the weather manager, disabled, with no storms, calm wind and at most three storms at a time (the wind's
// limits: ee_game gamecpp weather.rs)
EEEnvironmentManager::EEEnvironmentManager()
    : FSPersistent(kPersistentEnvironment), mState(0)
{
    ee_gamecpp_env_init(this);
}

// Destroys the weather manager, releasing and deleting every storm
EEEnvironmentManager::~EEEnvironmentManager()
{
    ee_gamecpp_env_delete_storms(this);
}

// Returns whether a tile lies inside a storm: rain or hurricane storms and/or snow or blizzard storms, as asked
bool EEEnvironmentManager::IsInStorm(const U2DPoint<long>& inPoint, bool inTypes13, bool inTypes24)
{
    return ee_gamecpp_env_is_in_storm(this, &inPoint, inTypes13, inTypes24);
}

// Creates a new default weather manager (the persistent-object factory)
FSPersistent* EEEnvironmentManager::CreateDefault() const
{
    return new EEEnvironmentManager;
}

// Advances the storms and the wind, clears a finished state and schedules the next weather step by game speed
void EEEnvironmentManager::Step(unsigned long inTime)
{
    ee_gamecpp_env_step(this, inTime);
}

// Reads the storms, state, wind and timing from a saved game; returns the first read error
long EEEnvironmentManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_env_load(this, &inFile);
}

// Reconnects the loaded storms after a load, deleting the ones that fail to restore; returns 0
long EEEnvironmentManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    return ee_gamecpp_env_repair(this);
}

// Writes the storms, state, wind and timing to a saved game; returns the first write error
long EEEnvironmentManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_env_save(this, &outFile);
}

// Turns the weather on or off: off deletes every storm and unloads the rain and snow effects, on loads them
void EEEnvironmentManager::SetEnabled(bool inEnabled)
{
    ee_gamecpp_env_set_enabled(this, inEnabled);
}

void EEEnvironmentManager::FnE123D()
{
}

// (round 10, lane D: the environment manager's cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
