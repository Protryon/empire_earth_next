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
    this->mWind = 1.25e-06f;
    this->mWindTarget = 1.25e-06f;
    this->mWindRate = 1.3333333e-06f;
    this->mNextStep = 0;
    this->mIsEnabled = false;
    this->mInterval = 5000;
    this->mMaxStorms = 3;
    this->mF40 = 0.25f;
    this->mF44 = 0.75f;
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

// ------------------------------------------------------------------------- round 10, lane D: the environment manager

int64_t EECore10OpD4(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EECheatCodes.cpp
double EECore10OpD4F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define EM ((EEEnvironmentManager*)p)

// the C++ calls and members of the environment manager's bodies in Rust (gamecpp weather.rs): p the manager unless
// noted, one statement each
int64_t EECore10OpD3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DeManager: return P64(gWorld.mEnvironmentManager);
    case O::DeNewStorm: return P64(new EEEMStorm((long)a[0], EM));
    case O::DeStormPush: EM->mStorms.push_back((EEEMStorm*)a[0]);
        break;
    case O::DeStormCount: return (int64_t)EM->mStorms.size();
    case O::DeStormAt: return P64(EM->mStorms[a[0]]);
    case O::DeStormSet: EM->mStorms[a[0]] = (EEEMStorm*)a[1];
        break;
    case O::DeStormErase: EM->mStorms.erase(EM->mStorms.begin() + a[0]);
        break;
    case O::DeStormResize: EM->mStorms.resize((size_t)a[0], 0);
        break;
    case O::DeStormClear: EM->mStorms.clear();
        break;
    case O::DeStormDelete: delete (EEEMStorm*)p;
        break;
    case O::DeState: return P64(&EM->mState);
    case O::DeWind: return P64(&EM->mWind);
    case O::DeWindTarget: return P64(&EM->mWindTarget);
    case O::DeWindRate: return P64(&EM->mWindRate);
    case O::DeNextStep: return P64(&EM->mNextStep);
    case O::DeInterval: return P64(&EM->mInterval);
    case O::DeMaxStorms: return P64(&EM->mMaxStorms);
    case O::DeL50: return P64(&EM->mL50);
    case O::DeEnabled: return P64(&EM->mIsEnabled);
    case O::DeC18: return P64(&EM->mC18);
    case O::DeC24: return P64(&EM->mC24);
    case O::DeColorLoad: return ((GEColor*)p)->Load(*(FSFile*)a[0]);
    case O::DeColorSave: return ((GEColor*)p)->Save(*(FSFile*)a[0]);
    default: return EECore10OpD4(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpD3F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpD4F(inOp, p, a, f);
}
