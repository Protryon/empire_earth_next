// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEMusicObject.h (RECONSTRUCTION; file name inferred; provisional
// names). A song of the database (RTTI EEMusicObject > UReferenceCounter, EEMedia@+8; vtables 0x43f7a0 / 0x43f7b0).
// EEMusicObject.cpp 0xd9b2b..0xda3be. Only what the decompiled units use. Requires
// lle/UReferenceCounter.h, ee/EEMedia.h.
#ifndef EE_EEMUSICOBJECT_H
#define EE_EEMUSICOBJECT_H

class FSFile;
struct EEMusicRecord;

class EEMusicObject : public UReferenceCounter, public EEMedia {
public:
    EEMusicObject(EEMusicRecord* inRecord);         // 0xd9e0c
    virtual ~EEMusicObject();                       // ??_G 0xd9f1c (EEMedia's thunk 0xda3a6), dtor 0xd9f38
    virtual long LoadMedia();                       // EEMedia +0x04 0xda3ae
    virtual long UnloadMedia();                     // EEMedia +0x08 0xda3b6
    static long ReadNewMusic(EEMusicRecord* outRecord);   // 0xd9c6b (Data\Music\New Music.txt)
    static long LoadTable(FSFile* inFile);          // 0xd9f6c
    static void ClearAll();                         // 0xda0b9
    static EEMusicObject* GetRandom(long inEpoch);  // 0xda116 (a random song of the epoch's list)
    long LoadScore();                       // 0xda251 (loads the score into gConductor once)
    long UnloadScore();                     // 0xda315 (5: try again)
    float GetReverbGain() const { return this->mReverbGain; }
    float GetReverbHighFreqRTRatio() const { return this->mReverbHighFreqRTRatio; }
    float GetReverbMix() const { return this->mReverbMix; }
    float GetReverbTime() const { return this->mReverbTime; }

    UWideString mFileName;                  // +0x0c (passed to the conductor's LoadScore)
    long mScoreID;                          // +0x20 (-1 when unloaded)
    float mReverbGain;                      // +0x24 (SetReverbStyle's arguments, in order)
    float mReverbHighFreqRTRatio;           // +0x28
    float mReverbMix;                       // +0x2c
    float mReverbTime;                      // +0x30
    unsigned long mState;                   // +0x34 (1: loaded; more: use count)
    bool mInEpoch[15];                      // +0x38
    volatile bool mIsLoading;               // +0x47 (a load is under way; `mov byte [+0x47], 0` stays a move: volatile)

    // each epoch's songs and their shuffled play order (0x4e8568 / 0x4e8470: [15] arrays in the original): one list
    // per epoch of the rules (epochs.yaml music, or dbmusic's flags; ee/EERules.h)
    static std::vector<std::vector<EEMusicObject*>> sSongs;
    static std::vector<std::vector<EEMusicObject*>> sShuffled;
};

#endif
