// C:\EEX\Low-Level Engine\Sound Engine\SESample.h (RECONSTRUCTION). A sound sample bound to the mixer that was
// active when it was created. All of SESample is inline: SEConductor.cpp emits it (0x4c29a..0x4c47e) and there is
// no SESample.cpp. The "SetThe*" setters store without telling the mixer (the mixer plug-in uses them).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_SESAMPLE_H
#define LLE_SESAMPLE_H

#include "lle/SEMixer.h"

class LLE_API SESample {                    // proven sizeof 0x3c (mixer SPIMDuplicateSample: operator new(0x3c))
public:
#ifdef LOWLEVELENGINE_EXPORTS
    // 0x4c29a / 0x4c2d7: members assigned in the body in this order (store order)
    SESample() : mFilename(0x40)
    {
        this->mVolume = 1.0f;
        this->mMixerChannel = -1;
        this->mPan = 0.0f;
        this->mMixer = SEMixer::sActiveMixer;
        this->mData = NULL;
        this->mLengthInBytes = 0;
        this->mPitch = 1.0f;
        this->mLengthInSamples = 0;
        this->mSampleBits = 0;
        this->mSampleRate = 0;
        this->mIsLooping = false;
    }
#else
    SESample();
#endif
    SESample(const SESample& inSample) : mFilename(inSample.mFilename)
    {
        mMixer = inSample.mMixer;
        mData = NULL;
        mMixerChannel = -1;
        mVolume = inSample.mVolume;
        mPan = inSample.mPan;
        mPitch = inSample.mPitch;
        mLengthInBytes = inSample.mLengthInBytes;
        mLengthInSamples = inSample.mLengthInSamples;
        mSampleBits = inSample.mSampleBits;
        mSampleRate = inSample.mSampleRate;
        mIsLooping = inSample.mIsLooping;
    }
    ~SESample() { this->mMixer->DeleteWaveData(this); }
    void* GetData() const { return this->mData; }
    const UString& GetFilename() const { return this->mFilename; }
    bool GetIsLooping() const { return this->mIsLooping; }
    unsigned long GetLengthInSamples() const { return this->mLengthInSamples; }
    unsigned long GetMixerChannel() const { return this->mMixerChannel; }
    float GetPan() const { return this->mPan; }
    float GetPitch() const { return this->mPitch; }
    unsigned long GetSampleOffset() { return this->mMixer->GetSampleOffset(this); }
    unsigned long GetSampleRate() const { return this->mSampleRate; }
    float GetVolume() const { return this->mVolume; }
    long LoadWaveData(FSFileSpec* inSpec) { return this->mMixer->LoadWaveData(inSpec, this); }
    void SetIsLooping(bool inLooping) { this->mMixer->SetSampleLooping(this, inLooping); }
    long SetOffset(unsigned long inOffset) { return this->mMixer->SetSampleOffset(this, inOffset); }
    long SetPan(float inPan) { return this->mMixer->SetSamplePan(this, inPan); }
    long SetPitch(float inPitch) { return this->mMixer->SetSamplePitch(this, inPitch); }
    long SetVolume(float inVolume) { return this->mMixer->SetSampleVolume(this, inVolume); }

    void SetData(void* inData) { this->mData = inData; }
    void SetFilename(const UString& inFilename) { this->mFilename = inFilename; }
    void SetLengthInBytes(unsigned long inLength) { this->mLengthInBytes = inLength; }
    void SetLengthInSamples(unsigned long inLength) { this->mLengthInSamples = inLength; }
    void SetMixerChannel(unsigned long inChannel) { this->mMixerChannel = inChannel; }
    void SetSampleBits(unsigned long inBits) { this->mSampleBits = inBits; }
    void SetSampleRate(unsigned long inRate) { this->mSampleRate = inRate; }
    void SetTheIsLooping(bool inLooping) { this->mIsLooping = inLooping; }
    void SetThePan(float inPan) { this->mPan = inPan; }
    void SetThePitch(float inPitch) { this->mPitch = inPitch; }
    void SetTheVolume(float inVolume) { this->mVolume = inVolume; }
private:
    UString mFilename;                      // +0x00
    void* mData;                            // +0x10 the mixer's wave data (the Miles mixer: a SampleData)
    SEMixer* mMixer;                        // +0x14
    float mPan;                             // +0x18
    float mPitch;                           // +0x1c
    float mVolume;                          // +0x20
    unsigned long mMixerChannel;            // +0x24 (-1: not on a channel)
    unsigned long mLengthInBytes;           // +0x28
    unsigned long mLengthInSamples;         // +0x2c
    unsigned long mSampleBits;              // +0x30
    unsigned long mSampleRate;              // +0x34
    bool mIsLooping;                        // +0x38
};

#endif
