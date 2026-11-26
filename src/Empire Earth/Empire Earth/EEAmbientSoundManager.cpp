// C:\EEX\Empire Earth\Empire Earth\EEAmbientSoundManager.cpp (name from RTTI EEAmbientSoundManager; no __FILE__).
// 0x124ee2..0x1273d7: the ambient sounds (ctor, ??_G, dtor, the day-phase handlers, Load, the per-frame update and
// its animal/water/terrain parts), then the COMDATs: vector<EEAmbientSound>, EEAmbientSound's implicit copy members
// and inline members, the world analyzer's ??_G, the manager's inline Random/CheckState. The sound slots are indices
// into mSounds (7 wind, 9/10/14/18 animals, 11/15/19 terrain, 20..27 water, forest and space).
// Also EEAmbientSound.cpp's 0x124a88..0x124ee2 (the ambient sound and the world analyzer's counters; the unit is
// folded in here).
//
// The ambient sounds are whole in Rust (rust/crates/ee_game/src/app ambient.rs, app and world lane round 1): the manager's, the
// sounds' and the analyzer's plain members are records the classes derive from, every decision is Rust over the host
// (the sound objects and instances, the random generator, the world's reads; ee_game gamecpp ambient_io.rs, game
// core lane round 11, with Load and the analyzer's counting of the rendered units). This unit keeps the
// constructors and the forwarders.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/SESoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAmbientSound.h"
#include "ee/EEAmbientSoundManager.h"

// 0x524a88..0x524b34: the original's two EE-level trigonometry caller logs ("UMCos EE-Level", "UMSin EE-Level",
// ESCallerLog, never called), deleted with the class (LLE lane round 1)

// the host (ee_app ambient_ffi.rs): ee_game gamecpp ambient_io.rs; the context is the manager
#define EE_AMBIENT_HOST this, &this->mAnalyzer, this, ee_gamecpp_ambient_host

// Creates the ambient sound manager, disabled and unpaused, updating every 10 frames
EEAmbientSoundManager::EEAmbientSoundManager() : mRandom(1)
{
    ee_app_ambient_default(this);
}

// Destroys the manager, stopping and discarding every ambient sound
EEAmbientSoundManager::~EEAmbientSoundManager()
{
    this->StopSounds();
    this->ClearSounds();
}

// Stops every ambient sound and pauses the manager
void EEAmbientSoundManager::Pause()
{
    ee_app_ambient_stop_sounds(EE_AMBIENT_HOST, true);
}

// Runs one ambient update: day phase, terrain and animal sounds, the fades, then resets the world counters
void EEAmbientSoundManager::Update()
{
    ee_app_ambient_update(EE_AMBIENT_HOST);
}

// Enables or disables the ambient sounds: loads every sound object, or stops and unloads them (slot 0 is skipped)
void EEAmbientSoundManager::SetEnabled(bool inEnabled)
{
    ee_app_ambient_set_enabled(EE_AMBIENT_HOST, inEnabled);
}

// Stops every ambient sound
void EEAmbientSoundManager::StopSounds()
{
    ee_app_ambient_stop_sounds(EE_AMBIENT_HOST, false);
}

// Discards every ambient sound
void EEAmbientSoundManager::ClearSounds()
{
    this->mSounds.clear();
}

// Unpauses the manager, running the next update straight away
void EEAmbientSoundManager::Restart()
{
    this->mIsPaused = false;
    this->mFrameCount = this->mNextUpdateFrame;
}
