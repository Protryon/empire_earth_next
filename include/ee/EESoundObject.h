// C:\EEX\Empire Earth\Database\EESoundObject.h (RECONSTRUCTION; file name inferred). One sound of
// db\dbsounds.dat (vtable 0x43f7c0; EESoundObject.cpp around 0xda600). Field names are provisional; offsets proven by
// the
// calamity units (sound playback). Requires ee/EEMedia.h.
#ifndef EE_EESOUNDOBJECT_H
#define EE_EESOUNDOBJECT_H

#include "rust/data/tables.h"
#include "rust/data/media.h"
#include "rust/game/preload_media.h"

class EESoundObject;
class FSFile;
class FSFileSpec;
extern std::vector<EESoundObject*> gSoundObjects;   // 0x4e867c (the sound table; provisional name; needs <vector>)

// The record's members (mVolume, mMaxInstances, mCategory, mL24, mLooping) are EeDataSoundFields
// (rust/crates/ee_data tables.rs: the record and its copy), the second base; the use count (mMediaCount,
// mIsLoading) is EeDataMediaUse (media.rs), the third.
class EESoundObject : public EEMedia, public EeDataSoundFields, public EeDataMediaUse {
public:
    // the table of every sound by index (inline; its out-of-line COMDAT 0x525d sits in EEAUpgradeImprovement.cpp's
    // range after its first user; 0 past the end) (provisional name)
    static EESoundObject* Get(unsigned long inIndex)
    {
        if (inIndex < gSoundObjects.size())
            return gSoundObjects[inIndex];
        return NULL;
    }
    // inline accessors: the calamity classes pass their values from registers and FP temporaries
    // the unit EESoundObject.cpp (0xda56d..0xda984)
    EESoundObject() : EeDataSoundFields(), EeDataMediaUse() { this->mSampleID = 0; }
    // ??_G 0xda638, dtor 0xda654 (unloads when still counted)
    virtual ~EESoundObject() { ee_preload_sound_free(this); }
    // EEMedia +0x04 0xda7e6 (counts a use; 9: no such sample)
    virtual long LoadMedia() { return ee_preload_sound_load_media(this); }
    // EEMedia +0x08 0xda92e
    virtual long UnloadMedia() { return ee_preload_sound_unload_media(this); }
    // 0xda680 (db\dbsounds.dat)
    static long LoadTable(FSFile* inFile) { return ee_preload_load_sounds(inFile); }
    static void UnloadTable() { ee_preload_unload_sounds(); }
    // 0xda787 (fprintf to the spec's stream; EEWorld::DumpMedia)
    static void Dump(FSFileSpec& inFile) { ee_preload_dump_sounds(&inFile); }
    bool GetLooping() const { return this->mLooping; }
    unsigned long GetSampleID() const { return this->mSampleID; }
    float GetVolume() const { return this->mVolume; }
    // 0xb62d3 (ICF-folded with EEContagionHelper::SetFunctor: the store to +0x14; the debug sound panel 0x2545c0) (provisional name)
    void SetVolume(float inVolume) { this->mVolume = inVolume; }
    // 0xda891: plays it at (1, 0, 1) through 0xda8a9 (provisional name)
    void Play() { this->Play(1.0f, 0.0f, 1.0f); }
    // 0xda8a9 (EEUGUnloadTransport::Unload) (provisional name)
    long Play(float inVolume, float inPan, float inPitch) { return ee_preload_play_sound(this, inVolume, inPan, inPitch); }

    UString mName;                          // +0x04 (the ctor's member ctor)
    unsigned long mSampleID;                // +0x18 SESoundManager sample id
};

// dbsounds' sounds by table index (EESoundID): Rust's enum (ee_sim game_ids.rs), emitted back as a plain C++ enum with its evidence comments.
#include "rust/sim/game_ids.h"

#endif
