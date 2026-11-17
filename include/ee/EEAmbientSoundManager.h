// C:\EEX\Empire Earth\Empire Earth\EEAmbientSoundManager.h (RECONSTRUCTION; file name inferred). The ambient sounds
// (EEAmbientSoundManager.cpp 0x124ee2..0x1273d7; the ctor 0x124ee2 runs from Empire Earth's init chain 0x13ee5a). The
// game's instance is gAmbientSoundManager (0x52f5d0). Only what the
// game settings events need.
#ifndef EE_EEAMBIENTSOUNDMANAGER_H
#define EE_EEAMBIENTSOUNDMANAGER_H

#include <vector>
#include "rust/game/app.h"

class EEAmbientSound;
class FSFile;

class EEUnit;
class EEComplexUnit;
// the ambient sounds' world analyzer (+0x04 of the manager; its vtable is stored by the manager's ctor) (provisional
// name)
// name from that vtable's RTTI
// The counts are EeAppAmbientCounts (rust/ee_app.h; ee_app ambient.rs: mNumAirRaiders, mNumTrees, mNumCitizens,
// mNumAttackers, mNumFarms, mNumGoats, mNumEagles, mL20, mL24, mL28, mForestType (the first forest's +0x68)), the base.
class EEAmbientWorldAnalyzer : public EeAppAmbientCounts {
public:
    EEAmbientWorldAnalyzer() { this->Clear(); this->mForestType = 0; }
    virtual ~EEAmbientWorldAnalyzer() {}   // inline (its ??_G 0x12735f is an EEAmbientSoundManager.cpp COMDAT)
    void Clear();                           // 0x124ec1 (zeroes the counts)
    // 0x124e80 (ret 4): counts a placed object whose class +0x268 is 3 (EERenderObject::FillObject) (provisional name)
    void CountTree(EEUnit* inUnit);
    void CountUnit(EEComplexUnit* inUnit);  // 0x124d63 (ret 4; EERenderObject 0x10d47c): counts the unit into +0x04..+0x1c
    void AddTree(EEUnit* inUnit);           // 0x124e99 (ret 4; CountTree's callee): +0x08, then the forest's +0x68
};
// The manager's plain members are EeAppAmbientFields (ee_app ambient.rs: mState 1..4, mFrameCount, mNextUpdateFrame,
// mUpdateInterval (10), mIsEnabled, mIsPaused), the base; every decision is Rust over EEAmbientSoundManager.cpp's host.
class EEAmbientSoundManager : public EeAppAmbientFields {
public:
    EEAmbientSoundManager();                // 0x124ee2 (Empire Earth.cpp's chain; stores EEAmbientWorldAnalyzer's vtable)
    virtual ~EEAmbientSoundManager();
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
    long Load(FSFile& inFile);              // 0x125012 (the 0x98-byte sound records)
    void ClearSounds();
    float Random() { return this->mRandom.Generate(); }   // inline (COMDAT 0x12737c)

    EEAmbientWorldAnalyzer mAnalyzer;       // +0x04 (the ctor stores its vtable)
    URandom mRandom;                        // +0x34 (seed 1)
    std::vector<EEAmbientSound> mSounds;    // +0x44 (0x4c-byte entries)
};

extern EEAmbientSoundManager gAmbientSoundManager;

#endif
