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

// Destroys the song (nothing of its own to free)
EEMusicObject::~EEMusicObject()
{
}

// Files every song of the music table under the epochs it plays in; a New Music.txt override replaces
// the table with that one song. Returns the last read result
// The epochs and their songs are the rules' (epochs.yaml music, EERules.cpp); without epochs.yaml they are the
// table's per-epoch flags, as in the original (the space age plays the nano age's songs).
long EEMusicObject::LoadTable(FSFile* inFile)
{
    return ee_preload_load_music(inFile);   // (preload_media.rs; the override: Data\Music\New Music.txt)
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

// Loads the song's score on first use (counting later uses, which wait for the load); returns the load result
long EEMusicObject::LoadScore()
{
    return ee_preload_music_load_score(this);   // (preload_media.rs)
}

// Counts one use down; on the last use unloads the score unless the conductor reports it busy (5)
long EEMusicObject::UnloadScore()
{
    return ee_preload_music_unload_score(this);   // (preload_media.rs)
}

// (0x4da37a: vector<EEMusicObject*>::push_back, ICF-shared)

// Loads the song's score (media interface)
// EEMedia's slots: `this` arrives as the EEMedia part (add ecx, -8), then a tail jump
long EEMusicObject::LoadMedia()
{
    return this->LoadScore();
}

// Unloads the song's score (media interface)
long EEMusicObject::UnloadMedia()
{
    return this->UnloadScore();
}
