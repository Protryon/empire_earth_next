// C:\EEX\Low-Level Engine\Sound Engine\SESample.h (RECONSTRUCTION). A sound sample bound to the mixer that was
// active when it was made: the mixer's wave data and its playing state. Whole in Rust (rust/crates/ee_audio/src/sound
// objects.rs, LLE lane round 6): an SESample* is an EeSoundSample*; the members are inline forwarders.
// The sound manager makes and deletes samples; the mixer plug-in duplicates them (Duplicate, the original's copy
// constructor) and fills them. The "SetThe*" setters store without telling the mixer (the plug-in uses them).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESAMPLE_H
#define LLE_SESAMPLE_H
#include "lle/LLEForward.h"

#include "lle/SEMixer.h"

// a none (-1) or paused (-2) id or channel keeps all bits set on LP64, as the C++ stored it (`= -1`)
inline unsigned long EeSoundWide(uint32_t inValue)
{
    return inValue >= 0xfffffffeu ? (unsigned long)(long)(int32_t)inValue : (unsigned long)inValue;
}

class LLE_API SESample : URustView {
public:
    // a copy of the sample's state without its wave data and channel
    static SESample* Duplicate(const SESample& inSample) { return reinterpret_cast<SESample*>(ee_sound_sample_duplicate(reinterpret_cast<const EeSoundSample*>(&inSample))); }
    void* GetData() const { return ee_sound_sample_data(Rec()); }
    const char* GetFilename() const { return ee_sound_sample_filename(Rec()); }
    bool GetIsLooping() const { return ee_sound_sample_is_looping(Rec()); }
    unsigned long GetLengthInSamples() const { return ee_sound_sample_length_in_samples(Rec()); }
    unsigned long GetMixerChannel() const { return EeSoundWide(ee_sound_sample_mixer_channel(Rec())); }   // -1: not on a channel
    float GetPan() const { return ee_sound_sample_pan(Rec()); }
    float GetPitch() const { return ee_sound_sample_pitch(Rec()); }
    unsigned long GetSampleRate() const { return ee_sound_sample_rate(Rec()); }
    float GetVolume() const { return ee_sound_sample_volume(Rec()); }
    void SetIsLooping(bool inLooping) { ee_sound_sample_set_is_looping(Rec(), inLooping); }
    long SetOffset(unsigned long inOffset) { return ee_sound_sample_set_offset(Rec(), (uint32_t)inOffset); }
    long SetPan(float inPan) { return ee_sound_sample_set_pan(Rec(), inPan); }
    long SetPitch(float inPitch) { return ee_sound_sample_set_pitch(Rec(), inPitch); }
    long SetVolume(float inVolume) { return ee_sound_sample_set_volume(Rec(), inVolume); }

    void SetData(void* inData) { ee_sound_sample_set_data(Rec(), inData); }
    void SetFilename(const char* inFilename) { ee_sound_sample_set_filename(Rec(), inFilename); }
    void SetLengthInBytes(unsigned long inLength) { ee_sound_sample_set_length_in_bytes(Rec(), (uint32_t)inLength); }
    void SetLengthInSamples(unsigned long inLength) { ee_sound_sample_set_length_in_samples(Rec(), (uint32_t)inLength); }
    void SetMixerChannel(unsigned long inChannel) { ee_sound_sample_set_mixer_channel(Rec(), (uint32_t)inChannel); }
    void SetSampleBits(unsigned long inBits) { ee_sound_sample_set_bits(Rec(), (uint32_t)inBits); }
    void SetSampleRate(unsigned long inRate) { ee_sound_sample_set_rate(Rec(), (uint32_t)inRate); }
    void SetTheIsLooping(bool inLooping) { ee_sound_sample_set_the_is_looping(Rec(), inLooping); }
    void SetThePan(float inPan) { ee_sound_sample_set_the_pan(Rec(), inPan); }
    void SetThePitch(float inPitch) { ee_sound_sample_set_the_pitch(Rec(), inPitch); }
    void SetTheVolume(float inVolume) { ee_sound_sample_set_the_volume(Rec(), inVolume); }
    EeSoundSample* Rec() const { return reinterpret_cast<EeSoundSample*>(const_cast<SESample*>(this)); }
};

#endif
