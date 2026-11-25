// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEMusicObject.cpp (name from RTTI EEMusicObject; no __FILE__).
// 0xd9b2b..0xda3be: the songs of the database. The statics' init chains first (the per-epoch song lists 0x4e8568,
// the per-epoch shuffled lists 0x4e8470, the random generator 0x4e8460, EEDatabase.h's lock 0x4e8458), then the
// "New Music.txt" reader, the class (ctor, ??_G, dtor, LoadTable, ClearAll, GetRandom, LoadScore, UnloadScore) and the
// EEMedia thunks. The statics are plain globals here (the original's are class statics with guarded destructors).
// Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "ee/EEMusicObject.h"
#include "ee/EEDbRecords.h"
#include "ee/EERules.h"
#include "rust/game/preload_media.h"

URandomIntegers gMusicRandom;

// Creates an unloaded song from its record (name + ".sgt", reverb, epochs; the space epoch copies nano's flag: the
// record and its copy are ee_data tables.rs')
EEMusicObject::EEMusicObject(const unsigned char* inRecord) : EeDataMediaUse()
{
    UString theName;
    EEDbCopyName(theName, ee_data_music_from_record(inRecord, this));
    this->mFileName = (const char*)theName;
    this->mFileName += ".sgt";
    this->mScoreID = -1;
}

// This unit's database lock (the songs' uses: preload_media.rs)
TSCriticalSection* EEMusicDatabaseLock()
{
    return sDatabaseLock.mLock;
}

// Releases every song and empties all epoch song lists and shuffled lists
void EEMusicObject::ClearAll()
{
    ee_data_songs_clear([](void* inSong) { static_cast<EEMusicObject*>(inSong)->Dereference(); });
}

// Returns the next song of the epoch's shuffled order, reshuffling when it runs out; NULL if the epoch has none
EEMusicObject* EEMusicObject::GetRandom(long inEpoch)
{
    return static_cast<EEMusicObject*>(ee_data_songs_random(inEpoch,
        [](void*, long inMinimum, long inMaximum) { return gMusicRandom.Generate(inMinimum, inMaximum); }, NULL));
}

// (0x4da37a: vector<EEMusicObject*>::push_back, ICF-shared)
