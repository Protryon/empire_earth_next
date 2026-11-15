// C:\EEX\Empire Earth\Empire Earth\EEAmbientSoundManager.h (RECONSTRUCTION; file name inferred). The ambient sounds
// (EEAmbientSoundManager.cpp 0x124ee2..0x1273d7; the ctor 0x124ee2 runs from Empire Earth's init chain 0x13ee5a). The
// game's instance is gAmbientSoundManager (0x52f5d0). Only what the
// game settings events need.
#ifndef EE_EEAMBIENTSOUNDMANAGER_H
#define EE_EEAMBIENTSOUNDMANAGER_H

#include <vector>

class EEAmbientSound;
class FSFile;

// the day phases (EEAmbientSoundManager.cpp's .rdata 0x441e0c/0x441e10; provisional names)
static const float kEEAmbientDawnTime = 0.955f;
static const float kEEAmbientDuskTime = 0.452f;

class EEUnit;
class EEComplexUnit;
// the ambient sounds' world analyzer (+0x04 of the manager; its vtable is stored by the manager's ctor) (provisional
// name)
// name from that vtable's RTTI
class EEAmbientWorldAnalyzer {
public:
    EEAmbientWorldAnalyzer() { this->Clear(); }
    virtual ~EEAmbientWorldAnalyzer() {}   // inline (its ??_G 0x12735f is an EEAmbientSoundManager.cpp COMDAT)
    void Clear();                           // 0x124ec1 (zeroes the counts)
    // 0x124e80 (ret 4): counts a placed object whose class +0x268 is 3 (EERenderObject::FillObject) (provisional name)
    void CountTree(EEUnit* inUnit);
    void CountUnit(EEComplexUnit* inUnit);  // 0x124d63 (ret 4; EERenderObject 0x10d47c): counts the unit into +0x04..+0x1c
    void AddTree(EEUnit* inUnit);           // 0x124e99 (ret 4; CountTree's callee): +0x08, then the forest's +0x68

    long mNumAirRaiders;                    // +0x04
    long mNumTrees;                         // +0x08
    long mNumCitizens;                      // +0x0c
    long mNumAttackers;                     // +0x10
    long mNumFarms;                         // +0x14
    long mNumGoats;                         // +0x18
    long mNumEagles;                        // +0x1c
    long mL20;                              // +0x20
    long mL24;                              // +0x24
    long mL28;                              // +0x28
    long mForestType;                       // +0x2c (0x124e99: the first forest's +0x68; was the manager's mL30) (provisional name)
};
class EEAmbientSoundManager {
public:
    EEAmbientSoundManager();                // 0x124ee2 (Empire Earth.cpp's chain; stores EEAmbientWorldAnalyzer's vtable)
    virtual ~EEAmbientSoundManager();       // 0x124f5d
    // 0x12572e: stores inEnabled at +0x64 and starts or stops the sounds when it changes (the settings pass the
    // "Ambient Sounds" check box; provisional name)
    void SetEnabled(bool inEnabled);
    void StopSounds();                      // 0x1257bc (walks its 0x4c-byte sound entries; EEStopAllSounds 0x140ceb calls it first; provisional name)
    void Restart();                         // 0x125800 (after SetEnabled; provisional name)
    void Pause();                           // 0x1252b4 (after StopSounds; provisional name)
    // EERenderObject::FillRenderLists's per-frame calls (provisional names and layout)
    // inline: its COMDAT 0x111709 follows its first user's unit (EERenderObject.cpp); true: the sounds may be updated
    bool IsUpdateDue() { return this->mFrameCount >= this->mNextUpdateFrame && this->mIsEnabled == true && !this->mIsPaused; }
    void Update();                          // 0x125685 (when IsUpdateDue was true)
    // (provisional names)
    void UpdateState(float inTimeOfDay, long inTime);         // 0x124f83 (picks mState, runs its handler)
    long Load(FSFile& inFile);                                // 0x125012 (the 0x98-byte sound records)
    void UpdateDay(float inTimeOfDay, long inTime);           // 0x1250ae (state 2)
    void UpdateDawn(float inTimeOfDay, long inTime);          // 0x125231 (state 1)
    float RandomRange(float inLow, float inHigh);             // 0x125267
    long RandomInteger(long inLow, long inHigh);              // 0x125281
    void UpdateNight(float inTimeOfDay, long inTime);         // 0x1252c2 (state 4)
    void UpdateDusk(float inTimeOfDay, long inTime);          // 0x1254c8 (state 3)
    void ClearSounds();                                       // 0x1257f8
    void UpdateAnimals(float inTimeOfDay, long inTime);       // 0x12580b
    bool UpdateWater(float inTimeOfDay, long inTime, bool inForce, float inRatio);   // 0x125d0d
    void UpdateTerrain(float inTimeOfDay, long inTime);       // 0x125fcc
    // inline (COMDATs 0x12737c, 0x127385 at the end of EEAmbientSoundManager.cpp)
    float Random() { return this->mRandom.Generate(); }
    void CheckState(float inTimeOfDay, long inTime)
    {
        long theState;
        if (inTimeOfDay < 0.5f)
            theState = inTimeOfDay < kEEAmbientDuskTime ? 3 : 2;
        else
            theState = inTimeOfDay < kEEAmbientDawnTime ? 4 : 1;
        if (theState != this->mState)
            this->UpdateState(inTimeOfDay, inTime);
    }

    EEAmbientWorldAnalyzer mAnalyzer;       // +0x04 (the ctor stores its vtable)
    URandom mRandom;                        // +0x34 (seed 1)
    std::vector<EEAmbientSound> mSounds;    // +0x44 (0x4c-byte entries)
    long mState;                            // +0x54 (1..4)
    long mFrameCount;                       // +0x58
    long mNextUpdateFrame;                  // +0x5c
    long mUpdateInterval;                   // +0x60 (10)
    bool mIsEnabled;                        // +0x64 (SetEnabled's)
    bool mIsPaused;                         // +0x65
};

extern EEAmbientSoundManager gAmbientSoundManager;  // 0x52f5d0

#endif
