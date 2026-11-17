// C:\EEX\Empire Earth\Empire Earth\EEAmbientSound.h (RECONSTRUCTION; file name inferred).
// One ambient sound of the EEAmbientSoundManager (vtable 0x441df8; sizeof 0x4c: the manager's vector strides 0x4c).
// Its own members (default ctor 0x124b34, dtor 0x124ba1, Init 0x124bb4, 0x124c16/0x124c46/0x124c72/0x124ca9/0x124d4e)
// are in the unit before EEAmbientSoundManager.cpp; the copy ctor 0x1270ec / operator= 0x127251 are implicit
// (EEAmbientSoundManager.cpp's COMDATs), the inline members below are emitted there too. Members by offset from the
// manager's uses and Init's copy of the 0x98-byte record (provisional names).
#ifndef EE_EEAMBIENTSOUND_H
#define EE_EEAMBIENTSOUND_H

#include <stdint.h>   // int32_t/uint32_t: the on-disk fields' fixed widths
#include "rust/game/app.h"

class EESoundObject;
class SESMInstanceNode;

// the 0x98-byte record Init copies from (EEAmbientSoundManager::Load reads them from the file)
struct EEAmbientSoundRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mIndex;                         // +0x68 (the slot; < 0: unused)
    int32_t mSoundID;                       // +0x6c
    float mMaximumVolume;                   // +0x70
    float mStartVolume;                     // +0x74
    float mFadeUp;                          // +0x78 (Init copies it to +0x28 with a plain mov)
    float mFadeDown;                        // +0x7c (to +0x2c)
    float mProbability;                     // +0x80
    unsigned char _u84[0xc];                // +0x84
    int32_t mIntervalSeconds;               // +0x90
    unsigned char _u94[0x4];                // +0x94
};
static_assert(sizeof(EEAmbientSoundRecord) == 152, "on-disk record: 152 bytes on every target");

// The plain members are EeAppAmbientSoundFields (rust/ee_app.h; ee_app ambient.rs: mSoundID, mSoundIndex, mProbability,
// mVolume, mTargetVolume, mMaximumVolume, mStartVolume, mFadeUp, mFadeDown, mPan, mPitch, mF38, mF3C, mInterval (ms),
// mNextTime, mIsLooping), the base; the fades, the play and the stop are Rust (EEAmbientSoundManager.cpp's host).
class EEAmbientSound : public EeAppAmbientSoundFields {
public:
    EEAmbientSound();
    virtual ~EEAmbientSound();
    void Init(const EEAmbientSoundRecord& inRecord);
    // inline (COMDATs 0x1272c6.. in EEAmbientSoundManager.cpp)
    void ReleaseInstance()
    {
        if (this->mInstance != NULL)
            this->mInstance->Dereference();
        this->mInstance = NULL;
    }

    EESoundObject* mSound;                  // +0x04 (its sample id is passed to SESoundManager::Play)
    SESMInstanceNode* mInstance;            // +0x0c
};

#endif
