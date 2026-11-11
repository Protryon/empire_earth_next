// C:\EEX\Low-Level Engine\Sound Engine\SEConductor.h (RECONSTRUCTION). Music score playback, on dmusic (a
// DirectMusic reimplementation: segments, styles, bands, DLS and its own synthesizer; src/ThirdParty/dmusic) instead
// of Microsoft's DirectMusic. The performance renders into the mixer's output (SEMixer::SetMusicSource). Every
// SEMixer embeds one (SEMixer::GetConductor). The score table and the decisions (what loads, plays and transitions,
// the volume curve) are Rust: rust/crates/ee_sound conductor.rs (docs/RUST_PLAN.md R3.6); this class keeps dmusic's
// objects, the scores, the scalar members the game reads inline and the lock. Requires lle/LowLevelEngine.h.
#ifndef LLE_SECONDUCTOR_H
#define LLE_SECONDUCTOR_H

#include <vector>

struct DmLoader;
struct DmPerformance;
class SEMixer;
class SEScore;
class TSCriticalSection;

struct EeConductor;                         // the score table (rust/crates/ee_sound conductor.rs)

class LLE_API SEConductor {
    friend class SEMixer;
    friend class SEScore;
public:
    unsigned long GetChannelGroupCount() { return this->mChannelGroupCount; }
    unsigned long GetConductorState() const { return this->mConductorState; }
    unsigned long GetIntensity() const { return this->mIntensity; }
    const UWideString& GetMusicFolder() const { return this->mMusicFolder; }
    unsigned long GetReverbState() const { return this->mReverbState; }
    float GetVolume() const { return this->mVolume; }
    long LoadScore(const UWideString& inName, long& outID) { return this->LoadScore(inName, outID, false); }
    // the events (0 segment aborted, 1 almost ended, 2 ended, 3 started, 4 music started, 5 music stopped): kept for
    // the interface; dmusic reports none, and the game registers no callback
    void SetNotificationCallback(unsigned long inEvent, void (*inCallback)(SEConductor&, long))
        { this->mCallbacks[inEvent] = inCallback; }

    long GetCurrentlyPlayingScore();
    SEScore* GetScore(long inID) const;
    long GetScoreID(SEScore* inScore) const;
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
    // the C++ side the Rust conductor reaches (extern "C" ee_conductorcpp_*, SEConductor.cpp) and the copy of the
    // scalar members around each Rust call: a nested class, so that it shares this class's access
    struct Rust;
private:
    void SetMixer(SEMixer* inMixer) { this->mMixer = inMixer; }

    SEConductor();
    virtual ~SEConductor();
    long InitializeConductor();
    long LoadScore(const UWideString& inName, long& outID, bool inSecondary);
    long UninitializeConductor();
    static void Render(void* inConductor, float* outFrames, unsigned long inCount);
    static void* Resolve(void* inConductor, const char* inFile, size_t* outLength);

    EeConductor* mRust;                     // the score table (names, ids, load counts, the loaded SEScore*)
    UWideString mMusicFolder;               // the folder the score files (.sgt, .sty, .dls) are read from
    void (*mCallbacks[6])(SEConductor&, long);
    TSCriticalSection* mLock;
    SEMixer* mMixer;
    DmPerformance* mPerformance;            // while running
    DmLoader* mLoader;                      // while running
    float mVolume;                          // 1.0
    unsigned long mConductorState;          // 0 stopped, 1 running
    unsigned long mReverbState;             // 1 (kept; the synthesizer has no reverb)
    long mNextScoreID;                      // wraps -1 to 0
    long mPlayingID;                        // the score last started (-1: none)
    unsigned long mIntensity;               // the master groove level
    unsigned long mChannelGroupCount;       // 1000 (kept for the interface)
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
