// C:\EEX\Low-Level Engine\Sound Engine\SEScore.cpp
// A music score on dmusic (see lle/SEScore.h): whole in Rust (rust/crates/ee_sound objects.rs).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/SEScore.h"
#include "rust/ee_sound.h"

#define RUST reinterpret_cast<const EeSoundScore*>(this)

// how many times the score repeats (the game loops a score with a large count)
void SEScore::SetRepeatCount(unsigned long inCount) { ee_sound_score_set_repeat_count(RUST, (uint32_t)inCount); }
