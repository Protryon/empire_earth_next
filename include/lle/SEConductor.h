// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.h (RECONSTRUCTION). Music score playback, on dmusic (a
// DirectMusic reimplementation: segments, styles, bands, DLS and its own synthesizer; src/ThirdParty/dmusic) instead
// of Microsoft's DirectMusic. The performance renders into the mixer's output (SEMixer::SetMusicSource). Every
// SEMixer has one (SEMixer::GetConductor). Whole in Rust (rust/crates/ee_audio/src/sound conductorobj.rs, the decisions
// conductor.rs; LLE lane round 6): an SEConductor* is an EeSoundConductor* inside its mixer; the members are
// defined in SEConductor.cpp. Requires lle/LowLevelEngine.h.
#ifndef LLE_SECONDUCTOR_H
#define LLE_SECONDUCTOR_H

class SEScore;

class LLE_API SEConductor {
public:
    unsigned long GetConductorState() const;
    float GetVolume() const;

    SEScore* GetScore(long inID) const;
    long LoadScore(const UWideString& inName, long& outID);
    long Play(long inID);
    long Queue(long inID);
    long SetChannelGroupCount(unsigned long inCount);
    long SetConductorState(unsigned long inState);
    long SetIntensity(unsigned long inIntensity, bool inImmediate);
    long SetMusicFolder(const UWideString& inFolder);
    long SetReverb(unsigned long inReverb);
    long SetReverbStyle(float inGain, float inHighFreqRTRatio, float inMix, float inTime);
    long SetVolume(float inVolume);
    long Stop();
    long TransitionTo(long inID, unsigned long inFlags, unsigned long inEmbellishment);
    long UnloadScore(long inID);
private:
    SEConductor() = delete;
    ~SEConductor() = delete;
};

// SEConductor::TransitionTo inCommand: the transition pattern (dmusic embellishment) the conductor plays
enum SETransitionCommand {
    kMusicTransitionGroove = 0,     // DmEmbellishment_GROOVE (default)
    kMusicTransitionFill = 1,       // DmEmbellishment_FILL
    kMusicTransitionBreak = 2,      // DmEmbellishment_BREAK
    kMusicTransitionIntro = 3,      // DmEmbellishment_INTRO
    kMusicTransitionEnd = 4,        // DmEmbellishment_END
    kMusicTransitionEndAndIntro = 5,  // end only (dmusic has no end-and-intro yet)
};

// SEConductor::TransitionTo inFlags: the boundary (DirectMusic's composer flags; dmusic transitions on a measure)
enum SETransitionFlags {
    kTransitionFlagMeasure = 0,
    kTransitionFlagModulate = 1,
    kTransitionFlagLong = 2,
};

#endif
