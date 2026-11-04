// C:\EEX\Empire Earth\Empire Earth\EEAmbientSound.h (RECONSTRUCTION; file name inferred).
// One ambient sound of the EEAmbientSoundManager (vtable 0x441df8; sizeof 0x4c: the manager's vector strides 0x4c).
// Its own members (default ctor 0x124b34, dtor 0x124ba1, Init 0x124bb4, 0x124c16/0x124c46/0x124c72/0x124ca9/0x124d4e)
// are in the unit before EEAmbientSoundManager.cpp; the copy ctor 0x1270ec / operator= 0x127251 are implicit
// (EEAmbientSoundManager.cpp's COMDATs), the inline members below are emitted there too. Members by offset from the
// manager's uses and Init's copy of the 0x98-byte record (provisional names).
#ifndef EE_EEAMBIENTSOUND_H
#define EE_EEAMBIENTSOUND_H

class EESoundObject;
class SESMInstanceNode;

// the 0x98-byte record Init copies from (EEAmbientSoundManager::Load reads them from the file)
struct EEAmbientSoundRecord {
    unsigned char _u00[0x68];               // +0x00
    long mIndex;                            // +0x68 (the slot; < 0: unused)
    long mSoundID;                          // +0x6c
    float mMaximumVolume;                   // +0x70
    float mStartVolume;                     // +0x74
    float mFadeUp;                          // +0x78 (Init copies it to +0x28 with a plain mov)
    float mFadeDown;                        // +0x7c (to +0x2c)
    float mProbability;                     // +0x80
    unsigned char _u84[0xc];                // +0x84
    long mIntervalSeconds;                  // +0x90
    unsigned char _u94[0x4];                // +0x94
};
static_assert(sizeof(EEAmbientSoundRecord) == 152, "on-disk record: 152 bytes on every target");

class EEAmbientSound {
public:
    EEAmbientSound();                       // 0x124b34
    virtual ~EEAmbientSound();              // 0x124ba1
    void Init(const EEAmbientSoundRecord& inRecord);   // 0x124bb4
    void FadeIn();                          // 0x124c16 (fade up)
    void FadeOut();                         // 0x124c46 (fade down)
    bool LoadSound();                       // 0x124c72 (start: loads the sound object; whether there is one)
    void PlayAtVolume(float inVolume);      // 0x124ca9 (plays/updates at inVolume; 0 or less stops)
    void UnloadSound();                     // 0x124d4e
    // inline (COMDATs 0x1272c6.. in EEAmbientSoundManager.cpp)
    void ReleaseInstance()
    {
        if (this->mInstance != NULL)
            this->mInstance->Dereference();
        this->mInstance = NULL;
    }
    void SetTargetVolume(float inVolume)
    {
        if (inVolume <= this->mMaximumVolume) {
            if (inVolume < 0.0f)
                inVolume = 0.0f;
        } else
            inVolume = this->mMaximumVolume;
        this->mTargetVolume = inVolume;
    }
    void Stop()
    {
        if (this->mInstance != NULL) {
            gSoundManager->Stop(this->mInstance, true);
            this->ReleaseInstance();
        }
        this->mVolume = 0.0f;
        this->mTargetVolume = 0.0f;
    }
    void UpdateFade()
    {
        if (this->mVolume != this->mTargetVolume) {
            if (this->mVolume < this->mTargetVolume)
                this->FadeIn();
            else
                this->FadeOut();
        }
    }

    EESoundObject* mSound;                  // +0x04 (its sample id is passed to SESoundManager::Play)
    long mSoundID;                          // +0x08
    SESMInstanceNode* mInstance;            // +0x0c
    long mSoundIndex;                       // +0x10
    float mProbability;                     // +0x14
    float mVolume;                          // +0x18
    float mTargetVolume;                    // +0x1c
    float mMaximumVolume;                   // +0x20
    float mStartVolume;                     // +0x24
    // +0x28..+0x3c floats (the ctor's fldz/fld1) (provisional names)
    float mFadeUp;                          // +0x28 (record +0x78: FadeIn multiplies mVolume by it)
    float mFadeDown;                        // +0x2c (record +0x7c: FadeOut)
    float mPan;                             // +0x30 (0; PlayAtVolume's pan)
    float mPitch;                           // +0x34 (1; PlayAtVolume's pitch)
    float mF38;                             // +0x38 (0)
    float mF3C;                             // +0x3c (1)
    long mInterval;                         // +0x40 (ms)
    long mNextTime;                         // +0x44
    bool mIsLooping;                        // +0x48
};

#endif
