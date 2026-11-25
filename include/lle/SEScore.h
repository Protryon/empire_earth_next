// C:\EEX\Low-Level Engine\Sound Engine\SEScore.h (RECONSTRUCTION). One music score: a DirectMusic segment (.sgt) as
// loaded by the music engine (rust/crates/ee_audio/src/dmusic). The original also made a secondary score for every
// motif of the segment's style; the engine has no motifs, and the game never plays them, so a score has no secondary scores. Whole
// in Rust (rust/crates/ee_audio/src/sound objects.rs, LLE lane round 6): an SEScore* is an EeSoundScore*, made and deleted by
// the conductor; the member is an inline forwarder.
#ifndef LLE_SESCORE_H
#define LLE_SESCORE_H
#include "lle/LLEForward.h"

#include "rust/audio/sound.h"

class LLE_API SEScore : URustView {
public:
    // how many times the score repeats (the game loops a score with a large count)
    void SetRepeatCount(unsigned long inCount) { ee_sound_score_set_repeat_count(Rust(), (uint32_t)inCount); }
    const EeSoundScore* Rust() const { return reinterpret_cast<const EeSoundScore*>(this); }
};

#endif
