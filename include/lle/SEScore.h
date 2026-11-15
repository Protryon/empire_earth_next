// C:\EEX\Low-Level Engine\Sound Engine\SEScore.h (RECONSTRUCTION). One music score: a DirectMusic segment (.sgt) as
// loaded by dmusic (src/ThirdParty/dmusic). The original also made a secondary score for every motif of the
// segment's style; dmusic has no motifs, and the game never plays them, so a score has no secondary scores. Whole
// in Rust (rust/crates/ee_sound objects.rs, LLE lane round 6): an SEScore* is an EeSoundScore*, made and deleted by
// the conductor; the members are defined in SEScore.cpp.
#ifndef LLE_SESCORE_H
#define LLE_SESCORE_H

class LLE_API SEScore {
public:
    bool GetIsSecondaryScore() const;
    unsigned long GetRepeatCount();
    SEScore* GetSecondaryScore(unsigned long inIndex) const { return NULL; }   // there are none
    unsigned long GetSecondaryScoreCount() const { return 0; }
    void SetRepeatCount(unsigned long inCount);
private:
    SEScore() = delete;
    ~SEScore() = delete;
};

#endif
