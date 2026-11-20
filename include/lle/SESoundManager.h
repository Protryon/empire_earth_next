// C:\EEX\Low-Level Engine\Sound Engine\SESoundManager.h (RECONSTRUCTION; SESoundManager.cpp 0x4f08e..0x532d9).
// Loads sounds into categories and plays instances of them on a mixer's channels; a scavenger thread retires
// finished instances. Whole in Rust (rust/crates/ee_audio/src/sound managerobj.rs, the decisions manager.rs; LLE lane round
// 6): an SESoundManager* is an EeSoundManager*, made by Create and deleted by Destroy (the original's constructor
// and destructor); the members are inline forwarders (the C++ the Rust calls
// back, ee_soundcpp_*, is in Sound Engine.cpp). Requires lle/LowLevelEngine.h.
#ifndef LLE_SESOUNDMANAGER_H
#define LLE_SESOUNDMANAGER_H

#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"

class FSPath;

class LLE_API SESoundManager {
public:
    static SESoundManager* Create(FSPath* inPath, SEMixer* inMixer, unsigned long inCategoryCount,
                                  unsigned long inChannelCount)
    {
        return reinterpret_cast<SESoundManager*>(ee_sound_manager_new(inPath, reinterpret_cast<EeSoundMixer*>(inMixer),
                                                                      (uint32_t)inCategoryCount, (uint32_t)inChannelCount));
    }
    static void Destroy(SESoundManager* inManager) { ee_sound_manager_free(reinterpret_cast<EeSoundManager*>(inManager)); }

    static void CleanupMemoryPoolAndDestroyCriticalSections() { ee_sound_pool_cleanup(); }
    static void DumpMemoryPoolInfo() {}     // empty in this build
    unsigned long GetHighestTimerDelay() const { return ee_sound_manager_mixer_value(CRust(), 0); }
    unsigned long GetInstanceID(SESMInstanceNode* inNode) { return EeSoundWide(ee_sound_manager_node_instance_id(Rust(), Node(inNode))); }
    bool GetIsLooping(unsigned long inInstanceID) { return ee_sound_manager_is_looping(Rust(), (uint32_t)inInstanceID); }
    bool GetIsLooping(SESMInstanceNode* inNode) { return ee_sound_manager_node_is_looping(Rust(), Node(inNode)); }
    bool GetIsMuted() { return ee_sound_manager_muted(Rust()); }
    bool GetIsPlaying(unsigned long inInstanceID) { return ee_sound_manager_is_playing(Rust(), (uint32_t)inInstanceID); }
    bool GetIsPlaying(SESMInstanceNode* inNode) { return ee_sound_manager_node_is_playing(Rust(), Node(inNode)); }
    float GetMasterVolume() const { return ee_sound_manager_master_volume(CRust()); }
    unsigned long GetMaxChannels() const { return ee_sound_manager_mixer_value(CRust(), 1); }
    float GetPan(unsigned long inInstanceID) { return ee_sound_manager_pan(Rust(), (uint32_t)inInstanceID); }
    float GetPan(SESMInstanceNode* inNode) { return ee_sound_manager_node_pan(Rust(), Node(inNode)); }
    FSPath* GetPath() const { return static_cast<FSPath*>(ee_sound_manager_path(CRust())); }
    float GetPitch(unsigned long inInstanceID) { return ee_sound_manager_pitch(Rust(), (uint32_t)inInstanceID); }
    float GetPitch(SESMInstanceNode* inNode) { return ee_sound_manager_node_pitch(Rust(), Node(inNode)); }
    unsigned long GetSampleID(unsigned long inInstanceID) { return EeSoundWide(ee_sound_manager_sample_id(Rust(), (uint32_t)inInstanceID)); }
    unsigned long GetSampleID(SESMInstanceNode* inNode) { return EeSoundWide(ee_sound_manager_node_sample_id(Rust(), Node(inNode))); }
    unsigned long GetSampleRate(unsigned long inInstanceID) { return ee_sound_manager_sample_rate(Rust(), (uint32_t)inInstanceID); }
    long GetTotalPlayTimeInMilli(unsigned long inSampleID) { return ee_sound_manager_total_play_time(Rust(), (uint32_t)inSampleID); }
    float GetVolume(unsigned long inInstanceID) { return ee_sound_manager_volume(Rust(), (uint32_t)inInstanceID); }
    // the node pool's chunk sizes: the pool is Rust's, sized as it grows (kept for the interface)
    static void InitializeSoundsPool(unsigned long inChunkSize, unsigned long inMaxChunks, unsigned long inChunkCount)
        {}
    unsigned long Load(const UString& inName, unsigned long inCategory, unsigned long inMaxInstances)
    {
        return EeSoundWide(ee_sound_manager_load(Rust(), (const char*)inName, (uint32_t)inCategory, (uint32_t)inMaxInstances));
    }
    long MixAhead() { return ee_sound_manager_mix_ahead(Rust()); }
    unsigned long Play(unsigned long inSampleID, float inVolume, float inPan, float inPitch, bool inLooping,
                       char* inDebugName)
    {
        return EeSoundWide(ee_sound_manager_play(Rust(), (uint32_t)inSampleID, inVolume, inPan, inPitch, inLooping));
    }
    SESMInstanceNode* PlayAndReturnReferencedNode(unsigned long inSampleID, float inVolume, float inPan, float inPitch,
                                                  bool inLooping, char* inDebugName)
    {
        return reinterpret_cast<SESMInstanceNode*>(
            ee_sound_manager_play_node(Rust(), (uint32_t)inSampleID, inVolume, inPan, inPitch, inLooping));
    }
    long RegisterOneShotSound(unsigned long inSampleID, unsigned long inInstanceID) { return ee_sound_manager_register_one_shot(Rust(), (uint32_t)inSampleID, (uint32_t)inInstanceID); }
    unsigned long SetCategoryPercentage(unsigned long inCategory, float inPercent) { return ee_sound_manager_set_category_percentage(Rust(), (uint32_t)inCategory, inPercent); }
    long SetCategoryPriority(unsigned long inCategory, unsigned long inPriority) { return ee_sound_manager_set_category_priority(Rust(), (uint32_t)inCategory, (uint32_t)inPriority); }
    void SetIsMuted(bool inMuted) { ee_sound_manager_set_muted(Rust(), inMuted); }
    long SetLatency(unsigned long inMilliseconds) { return ee_sound_manager_set_latency(Rust(), (uint32_t)inMilliseconds); }
    long SetMasterVolume(float inVolume) { return ee_sound_manager_set_master_volume(Rust(), inVolume); }
    long SetMaxChannels(unsigned long inCount) { return ee_sound_manager_set_max_channels(Rust(), (uint32_t)inCount); }
    long SetPan(unsigned long inInstanceID, float inPan) { return ee_sound_manager_set_pan(Rust(), (uint32_t)inInstanceID, inPan); }
    long SetPan(SESMInstanceNode* inNode, float inPan) { return ee_sound_manager_node_set(Rust(), Node(inNode), 1, inPan); }
    void SetPath(FSPath* inPath) { ee_sound_manager_set_path(Rust(), inPath); }
    long SetPitch(unsigned long inInstanceID, float inPitch) { return ee_sound_manager_set_pitch(Rust(), (uint32_t)inInstanceID, inPitch); }
    long SetPitch(SESMInstanceNode* inNode, float inPitch) { return ee_sound_manager_node_set(Rust(), Node(inNode), 2, inPitch); }
    long SetSoundParameters(unsigned long inInstanceID, float inVolume, float inPan, float inPitch) { return ee_sound_manager_set_parameters(Rust(), (uint32_t)inInstanceID, inVolume, inPan, inPitch); }
    long SetSoundParameters(SESMInstanceNode* inNode, float inVolume, float inPan, float inPitch) { return ee_sound_manager_node_set_parameters(Rust(), Node(inNode), inVolume, inPan, inPitch); }
    long SetVolume(unsigned long inInstanceID, float inVolume) { return ee_sound_manager_set_volume(Rust(), (uint32_t)inInstanceID, inVolume); }
    long SetVolume(SESMInstanceNode* inNode, float inVolume) { return ee_sound_manager_node_set(Rust(), Node(inNode), 0, inVolume); }
    long Stop(unsigned long inInstanceID, bool inLetFinish) { return ee_sound_manager_stop(Rust(), (uint32_t)inInstanceID, inLetFinish); }
    long Stop(SESMInstanceNode* inNode, bool inImmediately) { return ee_sound_manager_stop_node(Rust(), Node(inNode), inImmediately); }
    long StopAllOneShotSounds() { return ee_sound_manager_stop_all_one_shots(Rust()); }
    long Unload(unsigned long inSampleID) { return ee_sound_manager_unload(Rust(), (uint32_t)inSampleID); }
private:
    EeSoundManager* Rust() { return reinterpret_cast<EeSoundManager*>(this); }
    const EeSoundManager* Rust() const { return reinterpret_cast<const EeSoundManager*>(this); }
    const EeSoundManager* CRust() const { return reinterpret_cast<const EeSoundManager*>(this); }
    static EeSoundNode* Node(SESMInstanceNode* inNode) { return reinterpret_cast<EeSoundNode*>(inNode); }
    SESoundManager() = delete;
    ~SESoundManager() = delete;
};

#endif
