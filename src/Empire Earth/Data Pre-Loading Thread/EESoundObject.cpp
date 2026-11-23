// C:\EEX\Empire Earth\Data Pre-Loading Thread\EESoundObject.cpp (name from RTTI EESoundObject; no __FILE__).
// 0xda56d..0xda984: the sounds of the database. The sound table's init chain (gSoundObjects, 0x4e8678) and
// EEDatabase.h's lock chain (0x4e8670) first, then the class (ctor, ??_G, dtor, LoadTable, UnloadTable, Dump,
// LoadMedia, the two Play overloads, UnloadMedia). EERandomMapTable.cpp ends at 0xda56d, EEStartingResources.cpp
// starts at 0xda984. Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/RTSGraphics.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "ee/EESoundObject.h"
#include "ee/EEDbRecords.h"
#include "ee/EEMediaUse.h"
#include <algorithm>
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMusicObject.h"
#include "ee/EERules.h"
#include "ee/EEPhysics.h"
#include "rust/game/preload_media.h"

std::vector<EESoundObject*> gSoundObjects;

// Creates an empty, unloaded sound with no sample
EESoundObject::EESoundObject() : EeDataSoundFields(), EeDataMediaUse()
{
    this->mSampleID = 0;
}

// Destroys the sound, unloading its sample if it is loaded
EESoundObject::~EESoundObject()
{
    if (this->mMediaCount > 0) {
        this->mMediaCount = 1;
        this->UnloadMedia();
    }
}

// Reads the sound table (each record after its sample's name; the record and its copy are ee_data tables.rs':
// instance limit, category, looping, volume percent); returns the last read result
long EESoundObject::LoadTable(FSFile* inFile)
{
    EESoundObject* theSound = NULL;
    return EEDbReadLeadRecords(inFile, ee_data_sound_record_size(),
        [](unsigned long inCount) { gSoundObjects.resize(inCount, NULL); return true; },
        [&](unsigned long) {
            theSound = new EESoundObject;
            theSound->mName.Load(*inFile);
        },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            ee_data_sound_from_record(inRecord, theSound);
            gSoundObjects[inIndex] = theSound;
        });
}

// Deletes every sound of the table and empties it
void EESoundObject::UnloadTable()
{
    ee_preload_unload_sounds();
}

// Writes to the file each sound still loaded, with its use count (preload_media.rs)
void EESoundObject::Dump(FSFileSpec& inFile)
{
    ee_preload_dump_sounds(&inFile);
}

// Loads the sound's sample on first use (later uses wait for the load: ee/EEMediaUse.h); returns 9 on failure
long EESoundObject::LoadMedia()
{
    return EEMediaAcquire(this, sDatabaseLock.mLock, [this]() -> long {
        if (this->mName.GetLength() <= 0)
            return 9;
        this->mSampleID = gSoundManager->Load(this->mName, this->mCategory, this->mMaxInstances);
        return !UIsNone32(this->mSampleID) ? 0 : 9;
    });
}

// Sets the sound's volume (folded with EEContagionHelper::SetFunctor in the original: the store to +0x14; a float
// argument and a pointer argument are passed in different registers on x64)
void EESoundObject::SetVolume(float inVolume)
{
    this->mVolume = inVolume;
}

// Plays the sound once at full volume, centred, normal pitch
void EESoundObject::Play()
{
    this->Play(1.0f, 0.0f, 1.0f);
}

// Loads the sample and plays it once as a one-shot at the given volume, pan and pitch; returns 9 on failure
long EESoundObject::Play(float inVolume, float inPan, float inPitch)
{
    return ee_preload_play_sound(this, inVolume, inPan, inPitch);
}

// Counts one use down; on the last use unloads the sample. Returns 0
long EESoundObject::UnloadMedia()
{
    EEMediaRelease(this, sDatabaseLock.mLock, [this] {
        gSoundManager->Unload(this->mSampleID);
        this->mSampleID = 0;
    });
    return 0;
}

// A text into the Rust side's sink
static void EEPreloadMediaText(int64_t inSink, const char* inText)
{
    ee_preload_media_text_push((EePreloadMediaText*)inSink, (const uint8_t*)inText, strlen(inText));
}

extern std::vector<EEMedia*> gMediaTable;   // (EEGraphicObject.cpp)

#define GO(x) static_cast<EEGraphicObject*>((EEMedia*)(x))
#define GFX(x) ((EEGraphicEffect*)(x))
#define SO(x) ((EESoundObject*)(x))
#define SPEC(x) ((FSFileSpec*)(x))

// The media objects' calls (ee_game preload_media.rs EePreloadMediaCpp): p the object named, a the integers, f the
// floats
extern "C" int64_t ee_preload_mediacpp(EePreloadMediaCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EePreloadMediaCpp O;
    switch (inOp) {
    case O::FilePrint: fprintf(*SPEC(p), "%s", (const char*)a[0]);
        break;
    case O::MediaTableLen: return (int64_t)gMediaTable.size();
    case O::MediaTableAt: return (int64_t)(intptr_t)gMediaTable[(size_t)a[0]];
    case O::MediaTableDelete: delete gMediaTable[(size_t)a[0]];
        break;
    case O::MediaTableClear: gMediaTable.resize(0);
        break;
    case O::GraphicMediaCount: return GO(p)->mMediaCount;
    case O::GraphicFileName: EEPreloadMediaText(a[0], (const char*)*GO(p)->mFileName);
        break;
    case O::TurretCount: return (int64_t)((EEGraphicObject*)p)->mTurretParts.size();
    case O::TurretModel: return (int64_t)(intptr_t)((EEGraphicObject*)p)->mTurretParts[(size_t)a[0]]->mModel;
    case O::EffectSetLinked: *(a[0] == 0 ? &GFX(p)->mLinkedEffect1 : a[0] == 1 ? &GFX(p)->mLinkedEffect2 : &GFX(p)->mLinkedEffect3) = GFX(a[1]);
        break;
    case O::EffectId: return GFX(p)->mID;
    case O::EffectDereference: GFX(p)->Dereference();
        break;
    case O::EffectGraphic: return (int64_t)(intptr_t)static_cast<EEMedia*>(GFX(p)->mGraphicObject);
    case O::EffectKind: return GFX(p)->mKind;
    case O::PhysicsRandom: return gPhysics->GetRandom((long)a[0], (long)a[1]);
    case O::SoundCount: return (int64_t)gSoundObjects.size();
    case O::SoundAt: return (int64_t)(intptr_t)gSoundObjects[(size_t)a[0]];
    case O::SoundDelete: delete gSoundObjects[(size_t)a[0]];
        break;
    case O::SoundsClear: gSoundObjects.resize(0, NULL);
        break;
    case O::SoundMediaCount: return SO(p)->mMediaCount;
    case O::SoundName: EEPreloadMediaText(a[0], (const char*)SO(p)->mName);
        break;
    case O::SoundLoad: return (int64_t)(SO(p)->mSampleID = gSoundManager->Load(SO(p)->mName, SO(p)->mCategory, SO(p)->mMaxInstances));
    case O::SoundPlay: return (int64_t)gSoundManager->Play(SO(p)->mSampleID, f[0], f[1], f[2], false, NULL);
    case O::SoundRegisterOneShot: gSoundManager->RegisterOneShotSound(SO(p)->mSampleID, (unsigned long)a[0]);
        break;
    case O::SoundUnload: gSoundManager->Unload(SO(p)->mSampleID);
        break;
    case O::SpecNew: return (int64_t)(intptr_t)new FSFileSpec;
    case O::SpecDelete: delete SPEC(p);
        break;
    case O::SpecSetStartupVolume: SPEC(p)->SetVolume(gStartupVolume);
        break;
    case O::SpecSetMusicDirectory: SPEC(p)->SetDirectory(gStartupDirectory + "Data\Music\\");   // (sic: "\M" is an M)
        break;
    case O::SpecGetFirstFile: return SPEC(p)->GetFirstFile((const char*)a[0], *SPEC(a[1]));
    case O::SpecOpenRead: return SPEC(p)->FOpen("r");
    case O::SpecClose: return SPEC(p)->Close();
    case O::ScanName: return fscanf((FILE*)*SPEC(p), "%255s\n", (char*)a[0]);
    case O::ScanFloat: return fscanf((FILE*)*SPEC(p), "%f\n", (float*)a[0]);
    case O::EpochCount: return (int64_t)EEGetRules().mEpochs.size();
    case O::EpochMusicLen: return (int64_t)EEGetRules().mEpochs[(size_t)a[0]].mMusic.size();
    case O::EpochMusicAt: return EEGetRules().mEpochs[(size_t)a[0]].mMusic[(size_t)a[1]];
    case O::SongReference: ((EEMusicObject*)p)->Reference();
        break;
    case O::SongDereference: ((EEMusicObject*)p)->Dereference();
        break;
    }
    return 0;
}
