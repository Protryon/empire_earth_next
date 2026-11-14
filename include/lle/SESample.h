// C:\EEX\Low-Level Engine\Sound Engine\SESample.h (RECONSTRUCTION). A sound sample bound to the mixer that was
// active when it was made: the mixer's wave data and its playing state. Whole in Rust (rust/crates/ee_sound
// objects.rs, LLE lane round 6): an SESample* is an EeSoundSample*; the members are defined in SESoundManager.cpp.
// The sound manager makes and deletes samples; the mixer plug-in duplicates them (Duplicate, the original's copy
// constructor) and fills them. The "SetThe*" setters store without telling the mixer (the plug-in uses them).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESAMPLE_H
#define LLE_SESAMPLE_H

#include "lle/SEMixer.h"

class LLE_API SESample {
public:
    // a copy of the sample's state without its wave data and channel
    static SESample* Duplicate(const SESample& inSample);
    void* GetData() const;
    const char* GetFilename() const;
    bool GetIsLooping() const;
    unsigned long GetLengthInBytes() const;
    unsigned long GetLengthInSamples() const;
    unsigned long GetMixerChannel() const;   // -1: not on a channel
    float GetPan() const;
    float GetPitch() const;
    unsigned long GetSampleBits() const;
    unsigned long GetSampleOffset();
    unsigned long GetSampleRate() const;
    float GetVolume() const;
    long LoadWaveData(FSFileSpec* inSpec);
    void SetIsLooping(bool inLooping);
    long SetOffset(unsigned long inOffset);
    long SetPan(float inPan);
    long SetPitch(float inPitch);
    long SetVolume(float inVolume);

    void SetData(void* inData);
    void SetFilename(const char* inFilename);
    void SetLengthInBytes(unsigned long inLength);
    void SetLengthInSamples(unsigned long inLength);
    void SetMixerChannel(unsigned long inChannel);
    void SetSampleBits(unsigned long inBits);
    void SetSampleRate(unsigned long inRate);
    void SetTheIsLooping(bool inLooping);
    void SetThePan(float inPan);
    void SetThePitch(float inPitch);
    void SetTheVolume(float inVolume);
private:
    SESample() = delete;
    ~SESample() = delete;
};

#endif
