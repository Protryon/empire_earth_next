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

// (round 10, lane D: the environment manager's cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
