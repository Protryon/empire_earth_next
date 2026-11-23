//! The data pre-loading thread's media objects' bodies (world lane round 2): the graphic object table's dump and
//! delete and the turret part lookup (`EEGraphicObject`), the graphic effects' links, dump, unload and random graphic
//! (`EEGraphicEffect`), the sound table's dump and unload and a one-shot play (`EESoundObject`), the music table's
//! "New Music.txt" override and its epochs' songs (`EEMusicObject`). The C++ classes stay (their members are what the
//! game reads); each C++ call is one operation ([`EePreloadMediaCpp`]) answered by a one-statement case of
//! `ee_preload_mediacpp` (`src/Empire Earth/Data Pre-Loading Thread/EESoundObject.cpp`).

use std::cell::{Cell, RefCell};
use std::ffi::{c_long, c_ulong, c_void};

use ee_base::fs::{CppFile, FsFile};
use ee_data::media::{EeDataMediaLock, EeDataMediaStep, EeDataMediaUse};

use ee_data::tables::{EeDataGraphicEffectFields, EeDataGraphicEffectLinks};
use ee_render::gfx::geffect_table as table;

/// The media objects' calls (p: the object a call names).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EePreloadMediaCpp {
    /// `fprintf(*(FSFileSpec*)p, "%s", a0)`
    FilePrint = 1,
    /// `gMediaTable.size()`
    MediaTableLen,
    /// `gMediaTable[a0]`
    MediaTableAt,
    /// `delete gMediaTable[a0]`
    MediaTableDelete,
    /// `gMediaTable.resize(0)`
    MediaTableClear,
    /// the graphic object's `mMediaCount`
    GraphicMediaCount,
    /// the graphic object's `*mFileName` into the sink a0
    GraphicFileName,
    /// `mTurretParts.size()`
    TurretCount,
    /// `mTurretParts[a0]->mModel`
    TurretModel,
    /// the effect's linked effect a0 (0..2) = a1
    EffectSetLinked,
    EffectId,
    EffectDereference,
    /// the effect's `mGraphicObject`
    EffectGraphic,
    EffectKind,
    /// `gPhysics->GetRandom(a0, a1)`
    PhysicsRandom,
    /// `gSoundObjects.size()`
    SoundCount,
    SoundAt,
    /// `delete gSoundObjects[a0]`
    SoundDelete,
    /// `gSoundObjects.resize(0, NULL)`
    SoundsClear,
    SoundMediaCount,
    /// the sound's `mName` into the sink a0
    SoundName,
    /// `mSampleID = gSoundManager->Load(mName, mCategory, mMaxInstances)`
    SoundLoad,
    /// `gSoundManager->Play(mSampleID, f0, f1, f2, false, NULL)`
    SoundPlay,
    /// `gSoundManager->RegisterOneShotSound(mSampleID, a0)`
    SoundRegisterOneShot,
    /// `gSoundManager->Unload(mSampleID)`
    SoundUnload,
    /// `new FSFileSpec`
    SpecNew,
    SpecDelete,
    /// `SetVolume(gStartupVolume)`
    SpecSetStartupVolume,
    /// the music folder ("<startup volume><startup directory>Data\Music\")
    SpecSetMusicDirectory,
    /// `GetFirstFile(a0, *(FSFileSpec*)a1)`
    SpecGetFirstFile,
    /// `FOpen("r")`
    SpecOpenRead,
    SpecClose,
    /// `fscanf(file, "%255s\n", (char*)a0)`
    ScanName,
    /// `fscanf(file, "%f\n", (float*)a0)`
    ScanFloat,
    /// `EEGetRules().mEpochs.size()`
    EpochCount,
    /// `EEGetRules().mEpochs[a0].mMusic.size()`
    EpochMusicLen,
    /// `EEGetRules().mEpochs[a0].mMusic[a1]`
    EpochMusicAt,
    /// `((EEMusicObject*)p)->Reference()`
    SongReference,
    SongDereference,
    // ---- the tables' loads and the media uses (round 12)
    /// `((FSFile*)p)->Open(kFileAccessRead)`
    FileOpenRead,
    /// `((FSFile*)p)->Close()`
    FileClose,
    /// `new UString` (a scratch name)
    NameNew,
    NameDelete,
    /// `((UString*)p)->Load(*(FSFile*)a0)`
    NameLoad,
    /// `gSoundObjects.resize(a0, NULL)`
    SoundsResize,
    /// `new EESoundObject`
    SoundNew,
    /// `static_cast<EeDataSoundFields*>(p)`
    SoundFields,
    /// `mName.Load(*(FSFile*)a0)`
    SoundNameLoad,
    /// `gSoundObjects[a0] = a1`
    SoundSet,
    /// `static_cast<EeDataMediaUse*>(p)`
    SoundUse,
    /// `mSampleID = a0`
    SoundSetSample,
    /// the sound unit's `sDatabaseLock.mLock`
    SoundLock,
    /// the music unit's `sDatabaseLock.mLock` (`EEMusicDatabaseLock()`)
    MusicLock,
    /// `((TSCriticalSection*)p)->Enter()`
    LockEnter,
    LockLeave,
    /// `PlatSleep(1)`
    Sleep1,
    /// `gMediaTable.resize(a0, NULL)`
    MediaTableResize,
    /// `gMediaTable[a0] = a1`
    MediaTableSet,
    /// `new EEGraphicObject`
    GraphicNew,
    /// `static_cast<EeDataGraphicObjectFields*>(p)`
    GraphicFields,
    /// the name slot a0 (0 mFileName, 1 mTextureName, 2 + 2j mTextureNames[j], 3 + 2j mModelNames[j], 30
    /// mShadowModelName, 31 mShadowTextureName) = `new UString(*(UString*)a1)`
    GraphicName,
    /// `(a0 ? mSounds2 : mSounds1)[a1] = a2`
    GraphicSetSound,
    /// `(a0 ? mSound2Frames : mSound1Frames)[a1] = -1`
    GraphicNoFrames,
    /// `mMediaIndex`
    GraphicMediaIndex,
    /// `new EEGraphicEffect(*(EeDataGraphicEffectFields*)a0, EEMedia::Get(a1))`
    EffectNew,
    /// `new EEMusicObject((const unsigned char*)a0)`
    MusicNew,
    /// `gMusicRandom.SetSeed(TMGetTime())`
    MusicSeed,
    /// `EEGetRules().SetMusicFromFlags(a0, (bool*)a1, 14)`
    RulesMusicFromFlags,
    /// `static_cast<EeDataMediaUse*>(p)`
    MusicUse,
    /// `gConductor->GetConductor().LoadScore(mFileName, *(long*)a0)`
    MusicLoadScore,
    /// `mScoreID = a0`
    MusicSetScore,
    /// `gConductor->GetConductor().UnloadScore(mScoreID)`
    MusicUnloadScore,
    /// `delete *<the name slot a0>` (as GraphicName)
    GraphicNameDelete,
    /// `mMediaCount = 1`, then the `UnloadMedia` virtual (a graphic object, a0 0, or a sound, 1)
    MediaForceUnload,
    /// the graphic's part lists (a0: 0 mOrbitals, 1 mTurretParts, 2 mWheels): `size()`, `delete [a1]`, `clear()`
    PartsCount,
    PartDelete,
    PartsClear,
}

use EePreloadMediaCpp as O;

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use super::EePreloadMediaCpp;
    extern "C" {
        // EESoundObject.cpp
        pub fn ee_preload_mediacpp(op: EePreloadMediaCpp, p: usize, a: *const i64, f: *const f32) -> i64;
    }
}

/// The tests run no C++: a call is a bug of the test.
#[cfg(test)]
mod sys {
    use super::EePreloadMediaCpp;
    pub unsafe fn ee_preload_mediacpp(op: EePreloadMediaCpp, _: usize, _: *const i64, _: *const f32) -> i64 {
        panic!("no C++ in the tests ({op:?})")
    }
}

type P = usize;

fn hf(op: O, p: P, a: &[i64], f: &[f32]) -> i64 {
    let mut ai = [0i64; 4];
    ai[..a.len()].copy_from_slice(a);
    let mut fi = [0f32; 4];
    fi[..f.len()].copy_from_slice(f);
    // SAFETY: the host's operation over the C++ objects the caller holds
    unsafe { sys::ee_preload_mediacpp(op, p, ai.as_ptr(), fi.as_ptr()) }
}

fn h(op: O, p: P, a: &[i64]) -> i64 {
    hf(op, p, a, &[])
}

/// The text sink the C++ fills (`ee_preload_media_text_push`).
pub struct EePreloadMediaText(Vec<u8>);

/// Appends `length` bytes to a sink.
///
/// # Safety
/// `sink` is a sink the caller handed out; `bytes` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_media_text_push(sink: *mut EePreloadMediaText, bytes: *const u8, length: usize) {
    if length != 0 {
        // SAFETY: the caller's guarantee
        unsafe { (*sink).0.extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
    }
}

fn text(op: O, p: P) -> Vec<u8> {
    let mut sink = EePreloadMediaText(Vec::new());
    h(op, p, &[&mut sink as *mut EePreloadMediaText as usize as i64]);
    sink.0
}

/// `fprintf` of one line the Rust side formatted.
fn print(file: P, line: &[u8]) {
    let mut line = line.to_vec();
    line.push(0);
    h(O::FilePrint, file, &[line.as_ptr() as usize as i64]);
}

/// `UIsNone32`: the sound manager's "none".
fn is_none32(value: i64) -> bool {
    value as u32 == 0xffff_ffff
}

// ---- EEGraphicObject

/// `EEDumpGraphicObjectTable`: each graphic object still loaded, with its file name and use count.
pub fn dump_graphic_objects(file: P) {
    print(file, b"\n\n**********Graphic Object Table*********\n\n");
    for i in 0..h(O::MediaTableLen, 0, &[]) {
        let object = h(O::MediaTableAt, 0, &[i]) as P;
        if object != 0 {
            let count = h(O::GraphicMediaCount, object, &[]) as c_long;
            if count > 0 {
                let name = text(O::GraphicFileName, object);
                print(file, &media_count_line(&name, b" (filename)", count));
            }
        }
    }
}

/// "\t<name><what> has a media count of <count>\n" (the dumps' `%s ... %ld`).
pub fn media_count_line(name: &[u8], what: &[u8], count: c_long) -> Vec<u8> {
    [b"\t", name, what, format!(" has a media count of {count}\n").as_bytes()].concat()
}

/// `EEDeleteMediaTable`: every entry deleted, the table emptied.
pub fn delete_media_table() {
    for i in 0..h(O::MediaTableLen, 0, &[]) {
        h(O::MediaTableDelete, 0, &[i]);
    }
    h(O::MediaTableClear, 0, &[]);
}

/// `FindTurretPart`: the index of the turret part on `model` (0, the index out) or 3.
pub fn find_turret_part(object: P, model: P, index: &mut c_ulong) -> c_long {
    for i in 0..h(O::TurretCount, object, &[]) {
        if h(O::TurretModel, object, &[i]) as P == model {
            *index = i as c_ulong;
            return 0;
        }
    }
    3
}

// ---- EEGraphicEffect

/// The follow-on effects of each effect by the table's indices (an index past the table links none).
pub fn link_effects(effects: &[P], links: &[EeDataGraphicEffectLinks]) {
    for (effect, link) in effects.iter().zip(links) {
        if *effect == 0 {
            continue;
        }
        for (k, &index) in link.linked.iter().enumerate() {
            let target = effects.get(index as usize).copied().unwrap_or(0);
            h(O::EffectSetLinked, *effect, &[k as i64, target as i64]);
        }
    }
}

/// `DumpIDs`: each effect used without being preloaded (its recorded id read as a table index, as the original).
pub fn dump_effect_ids(file: P) {
    // SAFETY: a count only (no room)
    let mut ids: Vec<c_ulong> = vec![0; unsafe { table::ee_gfx_geffect_ids(std::ptr::null_mut(), 0) }];
    // SAFETY: room for every id
    unsafe { table::ee_gfx_geffect_ids(ids.as_mut_ptr(), ids.len()) };
    print(file, b"\n\nMEDIA DUMP OF ALL GRAPHIC EFFECTS THAT WERE NOT LOADED CORRECTLY\n\n");
    for id in ids {
        let effect = table::ee_gfx_geffect_get(id) as P;
        let id = h(O::EffectId, effect, &[]) as c_long;
        print(
            file,
            format!(
                "The Graphic Effect {id} (name not available in release) was used without being preloaded, notify a \
                 programmer about this.\n"
            )
            .as_bytes(),
        );
    }
}

/// `EEGraphicEffect::UnloadTable`: every effect in the table dereferenced, the table emptied.
pub fn unload_effects() {
    for i in 0..table::ee_gfx_geffect_count() {
        let effect = table::ee_gfx_geffect_get(i) as P;
        if effect != 0 {
            h(O::EffectDereference, effect, &[]);
        }
    }
    // SAFETY: no entries
    unsafe { table::ee_gfx_geffect_table_set(std::ptr::null(), 0) };
}

/// `GetMedia`: the effect's graphic, else a random one of its kind (`kind_count`, `kind_at`: the kind's table).
pub fn effect_media(effect: P, kind_count: impl Fn(c_long) -> c_long, kind_at: impl Fn(c_long, c_long) -> P) -> P {
    let graphic = h(O::EffectGraphic, effect, &[]) as P;
    if graphic != 0 {
        return graphic;
    }
    let kind = h(O::EffectKind, effect, &[]) as c_long;
    let count = kind_count(kind);
    let pick = h(O::PhysicsRandom, 0, &[0, i64::from(count.wrapping_sub(1))]) as c_long;
    kind_at(kind, pick)
}

// ---- EESoundObject

/// `EESoundObject::Dump`: each sound still loaded, with its use count.
pub fn dump_sounds(file: P) {
    print(file, b"\n\n**********Sound Object Table*********\n\n");
    for i in 0..h(O::SoundCount, 0, &[]) {
        let sound = h(O::SoundAt, 0, &[i]) as P;
        if sound != 0 {
            let count = h(O::SoundMediaCount, sound, &[]) as c_long;
            if count > 0 {
                print(file, &media_count_line(&text(O::SoundName, sound), b"", count));
            }
        }
    }
}

/// `EESoundObject::UnloadTable`: every sound deleted, the table emptied.
pub fn unload_sounds() {
    for i in 0..h(O::SoundCount, 0, &[]) {
        h(O::SoundDelete, 0, &[i]);
    }
    h(O::SoundsClear, 0, &[]);
}

/// `EESoundObject::Play(volume, pan, pitch)`: loads the sample and plays it once (registered as a one-shot; the
/// sample unloaded again when it does not play); 9 when it fails.
pub fn play_sound(sound: P, volume: f32, pan: f32, pitch: f32) -> c_long {
    let named = !text(O::SoundName, sound).is_empty();
    if !named || is_none32(h(O::SoundLoad, sound, &[])) {
        return 9;
    }
    let handle = hf(O::SoundPlay, sound, &[], &[volume, pan, pitch]);
    if is_none32(handle) {
        h(O::SoundUnload, sound, &[]);
        return 9;
    }
    h(O::SoundRegisterOneShot, sound, &[handle]);
    0
}

// ---- EEMusicObject

/// `ReadNewMusic`: "Data\Music\New Music.txt" (a score name and four reverb values) as a music record into `out`;
/// the result of the first failing file call, else 0.
///
/// # Safety
/// `out` has room for a music record.
pub unsafe fn read_new_music(out: *mut u8) -> c_long {
    let directory = h(O::SpecNew, 0, &[]) as P;
    h(O::SpecSetStartupVolume, directory, &[]);
    h(O::SpecSetMusicDirectory, directory, &[]);
    let file = h(O::SpecNew, 0, &[]) as P;
    let pattern = b"New Music.txt\0";
    let mut result = h(O::SpecGetFirstFile, directory, &[pattern.as_ptr() as usize as i64, file as i64]) as c_long;
    if result == 0 {
        result = h(O::SpecOpenRead, file, &[]) as c_long;
        if result == 0 {
            let mut name = [0u8; 0x100];
            let mut reverb = [0f32; 4];
            h(O::ScanName, file, &[name.as_mut_ptr() as usize as i64]);
            for value in reverb.iter_mut() {
                h(O::ScanFloat, file, &[value as *mut f32 as usize as i64]);
            }
            // SAFETY: the name is NUL-terminated (fscanf's %255s at most 255 bytes); `out` is the caller's record
            unsafe { ee_data::tables::ee_data_new_music_record(name.as_ptr().cast(), reverb.as_ptr(), out) };
            result = h(O::SpecClose, file, &[]) as c_long;
        }
    }
    h(O::SpecDelete, file, &[]);
    h(O::SpecDelete, directory, &[]);
    result
}

/// Each epoch's songs from the rules' rows (rows outside `songs` skipped), each added song referenced; then the
/// table's own references dropped.
pub fn add_epoch_songs(songs: &[P]) {
    for epoch in 0..h(O::EpochCount, 0, &[]) {
        for i in 0..h(O::EpochMusicLen, 0, &[epoch]) {
            let row = h(O::EpochMusicAt, 0, &[epoch, i]) as c_long;
            if row < 0 || row as usize >= songs.len() {
                continue;
            }
            let song = songs[row as usize];
            ee_data::tables::ee_data_songs_add(epoch as usize, song as *mut c_void);
            h(O::SongReference, song, &[]);
        }
    }
    for &song in songs {
        h(O::SongDereference, song, &[]);
    }
}

/// The one song of "New Music.txt" in every epoch (referenced once per epoch, then its own reference dropped).
pub fn add_song_to_every_epoch(song: P) {
    for epoch in 0..h(O::EpochCount, 0, &[]) {
        ee_data::tables::ee_data_songs_add(epoch as usize, song as *mut c_void);
        h(O::SongReference, song, &[]);
    }
    h(O::SongDereference, song, &[]);
}

// ---- the tables' loads (EEDbReadLeadRecords' shape: the count, then each record after its lead strings)

/// Reads a counted table from an open file: the u32 count, `begin(count)` (false: no records), then for each record
/// `lead(index)` and `each(index, bytes)` (a failed read still hands the buffer on). Returns the last read status.
pub fn read_records<F: FsFile>(
    file: &mut F,
    size: usize,
    mut begin: impl FnMut(u32) -> bool,
    mut lead: impl FnMut(u32),
    mut each: impl FnMut(u32, &[u8]),
) -> c_long {
    let mut record = vec![0u8; size];
    let mut count = 0u32;
    let mut result = file.read_u32(&mut count) as c_long;
    if result == 0 && begin(count) {
        for i in 0..count {
            lead(i);
            result = file.read_bytes(&mut record).0 as c_long;
            each(i, &record);
        }
    }
    result
}

/// [`read_records`] over the C++ file `file`, opened for reading first and closed after.
fn read_table(
    file: P,
    size: usize,
    begin: impl FnMut(u32) -> bool,
    lead: impl FnMut(u32),
    each: impl FnMut(u32, &[u8]),
) -> c_long {
    h(O::FileOpenRead, file, &[]);
    let result = read_records(&mut CppFile(file as *mut c_void), size, begin, lead, each);
    h(O::FileClose, file, &[]);
    result
}

/// `EESoundObject::Get`: the sound at a table index, NULL past the end.
fn sound_get(index: c_ulong) -> P {
    if (index as u64) < h(O::SoundCount, 0, &[]) as u64 {
        h(O::SoundAt, 0, &[index as i64]) as P
    } else {
        0
    }
}

/// `EESoundObject::LoadTable`: each sound record after its sample's name.
pub fn load_sounds(file: P) -> c_long {
    let sound = Cell::new(0 as P);
    read_table(
        file,
        ee_data::tables::ee_data_sound_record_size(),
        |count| {
            h(O::SoundsResize, 0, &[i64::from(count)]);
            true
        },
        |_| {
            sound.set(h(O::SoundNew, 0, &[]) as P);
            h(O::SoundNameLoad, sound.get(), &[file as i64]);
        },
        |index, record| {
            let fields = h(O::SoundFields, sound.get(), &[]) as *mut ee_data::tables::EeDataSoundFields;
            // SAFETY: a whole record; the fields are the new sound's base
            unsafe { ee_data::tables::ee_data_sound_from_record(record.as_ptr(), fields) };
            h(O::SoundSet, 0, &[i64::from(index), sound.get() as i64]);
        },
    )
}

/// The graphic object's name slots in the file's order (after one skipped name).
const GRAPHIC_NAMES: i64 = 32;

/// `EEGraphicObject::LoadTable`: each graphic record after its 33 names (a skipped one, the file, the texture, the
/// 14 parts' texture and model, the shadow's model and texture); a record with no media index is deleted again.
pub fn load_graphic_objects(file: P) -> c_long {
    let object = Cell::new(0 as P);
    let name = h(O::NameNew, 0, &[]) as P;
    let result = read_table(
        file,
        ee_data::tables::ee_data_graphic_object_record_size(),
        |count| {
            h(O::MediaTableResize, 0, &[i64::from(count)]);
            true
        },
        |index| {
            object.set(h(O::GraphicNew, 0, &[]) as P);
            h(O::MediaTableSet, 0, &[i64::from(index), object.get() as i64]);
            h(O::NameLoad, name, &[file as i64]);
            for slot in 0..GRAPHIC_NAMES {
                h(O::NameLoad, name, &[file as i64]);
                h(O::GraphicName, object.get(), &[slot, name as i64]);
            }
        },
        |index, record| {
            let o = object.get();
            let fields = h(O::GraphicFields, o, &[]) as *mut ee_data::tables::EeDataGraphicObjectFields;
            // SAFETY: a whole record; the fields are the new object's base
            let sounds = unsafe { ee_data::tables::ee_data_graphic_object_from_record(record.as_ptr(), fields) };
            for k in 0..sounds.sound1.len() {
                for (which, id) in [(0, sounds.sound1[k]), (1, sounds.sound2[k])] {
                    let sound = sound_get(id);
                    h(O::GraphicSetSound, o, &[which, k as i64, sound as i64]);
                    if sound == 0 {
                        h(O::GraphicNoFrames, o, &[which, k as i64]);
                    }
                }
            }
            if h(O::GraphicMediaIndex, o, &[]) == 0 {
                h(O::MediaTableDelete, 0, &[i64::from(index)]);
                h(O::MediaTableSet, 0, &[i64::from(index), 0]);
            }
        },
    );
    h(O::NameDelete, name, &[]);
    result
}

/// `EEGraphicEffect::LoadTable`: the effects (rows that are not effects stay NULL), their follow-on links, the table.
pub fn load_effects(file: P) -> c_long {
    let effects: RefCell<Vec<P>> = RefCell::new(Vec::new());
    let links: RefCell<Vec<EeDataGraphicEffectLinks>> = RefCell::new(Vec::new());
    let result = read_table(
        file,
        ee_data::tables::ee_data_graphic_effect_record_size(),
        |count| {
            effects.borrow_mut().resize(count as usize, 0);
            links.borrow_mut().resize(count as usize, EeDataGraphicEffectLinks::default());
            true
        },
        |_| {},
        |index, record| {
            let mut fields = EeDataGraphicEffectFields::default();
            let mut links = links.borrow_mut();
            let link = &mut links[index as usize];
            // SAFETY: a whole record; the outputs are locals
            let is_effect =
                unsafe { ee_data::tables::ee_data_graphic_effect_from_record(record.as_ptr(), &mut fields, link) };
            let effect = if is_effect {
                let at = &fields as *const EeDataGraphicEffectFields as usize as i64;
                h(O::EffectNew, 0, &[at, link.media as i64]) as P
            } else {
                0
            };
            effects.borrow_mut()[index as usize] = effect;
        },
    );
    let effects = effects.into_inner();
    link_effects(&effects, &links.into_inner());
    // SAFETY: `effects` holds its length of effects (or NULLs)
    unsafe { table::ee_gfx_geffect_table_set(effects.as_ptr().cast(), effects.len()) };
    result
}

/// `EEMusicObject::LoadTable`: the songs by epoch (a "New Music.txt" override plays in every epoch instead of the
/// table); the rules' per-epoch music from each row's flags (a no-op when epochs.yaml names it).
pub fn load_music(file: P) -> c_long {
    let size = ee_data::tables::ee_data_music_record_size();
    let mut record = vec![0u8; size];
    ee_data::tables::ee_data_songs_reset(h(O::EpochCount, 0, &[]) as usize);
    h(O::MusicSeed, 0, &[]);
    // SAFETY: room for a music record
    let mut result = unsafe { read_new_music(record.as_mut_ptr()) };
    if result != 0 {
        let songs: RefCell<Vec<P>> = RefCell::new(Vec::new());
        result = read_table(
            file,
            size,
            |_| true,
            |_| {},
            |index, row| {
                songs.borrow_mut().push(h(O::MusicNew, 0, &[row.as_ptr() as usize as i64]) as P);
                let mut flags = [false; 14];
                // SAFETY: a whole record; room for its 14 flags
                unsafe { ee_data::tables::ee_data_music_epoch_flags(row.as_ptr(), flags.as_mut_ptr()) };
                h(O::RulesMusicFromFlags, 0, &[i64::from(index), flags.as_ptr() as usize as i64]);
            },
        );
        add_epoch_songs(&songs.into_inner());
    } else {
        add_song_to_every_epoch(h(O::MusicNew, 0, &[record.as_ptr() as usize as i64]) as P);
    }
    result
}

// ---- the graphic object's and the sound's destructors, the graphic's part lists

/// `~EEGraphicObject`: its media unloaded if loaded, its names freed.
pub fn graphic_free(object: P) {
    if h(O::GraphicMediaCount, object, &[]) as c_ulong != 0 {
        h(O::MediaForceUnload, object, &[0]);
    }
    for slot in 0..GRAPHIC_NAMES {
        h(O::GraphicNameDelete, object, &[slot]);
    }
}

/// `~EESoundObject`: its sample unloaded if loaded.
pub fn sound_free(sound: P) {
    if h(O::SoundMediaCount, sound, &[]) as c_ulong != 0 {
        h(O::MediaForceUnload, sound, &[1]);
    }
}

/// The graphic's orbitals, turret parts and wheels deleted and their lists emptied (ee_gfx gobj_media.rs'
/// `clear_parts`).
pub extern "C" fn clear_parts(object: *mut c_void) {
    let object = object as P;
    for list in 0..3 {
        for i in 0..h(O::PartsCount, object, &[list]) {
            h(O::PartDelete, object, &[list, i]);
        }
        h(O::PartsClear, object, &[list]);
    }
}

/// [`clear_parts`] for the C++ host table.
#[no_mangle]
pub extern "C" fn ee_preload_graphic_clear_parts(object: *mut c_void) {
    clear_parts(object)
}

/// # Safety
/// `object` is a live `EEGraphicObject` (its `EEMedia` address).
#[no_mangle]
pub unsafe extern "C" fn ee_preload_graphic_free(object: *mut c_void) {
    graphic_free(object as P)
}

/// # Safety
/// `sound` is a live `EESoundObject`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_sound_free(sound: *mut c_void) {
    sound_free(sound as P)
}

// ---- the media uses (ee/EEMediaUse.h's acquire and release over the unit's lock)

extern "C" fn lock_enter(lock: *mut c_void) {
    h(O::LockEnter, lock as P, &[]);
}

extern "C" fn lock_leave(lock: *mut c_void) {
    h(O::LockLeave, lock as P, &[]);
}

extern "C" fn lock_sleep() {
    h(O::Sleep1, 0, &[]);
}

fn media_lock(op: O) -> EeDataMediaLock {
    EeDataMediaLock { lock: h(op, 0, &[]) as *mut c_void, enter: lock_enter, leave: lock_leave, sleep: lock_sleep }
}

fn step(context: *mut c_void, call: extern "C" fn(*mut c_void) -> c_long) -> EeDataMediaStep {
    EeDataMediaStep { context, call: Some(call) }
}

const NO_STEP: EeDataMediaStep = EeDataMediaStep { context: std::ptr::null_mut(), call: None };

fn acquire(usage: O, object: P, lock: O, load: EeDataMediaStep, reset_on_failure: bool) -> c_long {
    let usage = h(usage, object, &[]) as *mut EeDataMediaUse;
    // SAFETY: the object's live use; the steps outlive the call
    unsafe { ee_data::media::ee_data_media_acquire(usage, &media_lock(lock), &load, &NO_STEP, reset_on_failure) }
}

fn release(usage: O, object: P, lock: O, unload: EeDataMediaStep) {
    let usage = h(usage, object, &[]) as *mut EeDataMediaUse;
    // SAFETY: as `acquire`
    unsafe { ee_data::media::ee_data_media_release(usage, &media_lock(lock), &unload) }
}

extern "C" fn sound_load_step(sound: *mut c_void) -> c_long {
    if text(O::SoundName, sound as P).is_empty() || is_none32(h(O::SoundLoad, sound as P, &[])) {
        9
    } else {
        0
    }
}

extern "C" fn sound_unload_step(sound: *mut c_void) -> c_long {
    h(O::SoundUnload, sound as P, &[]);
    h(O::SoundSetSample, sound as P, &[0]);
    1
}

/// `EESoundObject::LoadMedia`: the sample loaded on first use; 9 when it has no name or does not load.
pub fn sound_load_media(sound: P) -> c_long {
    acquire(O::SoundUse, sound, O::SoundLock, step(sound as *mut c_void, sound_load_step), false)
}

/// `EESoundObject::UnloadMedia`: the last use unloads the sample; 0.
pub fn sound_unload_media(sound: P) -> c_long {
    release(O::SoundUse, sound, O::SoundLock, step(sound as *mut c_void, sound_unload_step));
    0
}

extern "C" fn music_load_step(song: *mut c_void) -> c_long {
    let mut id: c_long = 0;
    let result = h(O::MusicLoadScore, song as P, &[&mut id as *mut c_long as usize as i64]) as c_long;
    if result == 0 {
        h(O::MusicSetScore, song as P, &[i64::from(id)]);
    }
    result
}

/// The music unload's song and the conductor's answer.
struct MusicUnload {
    song: P,
    result: c_long,
}

extern "C" fn music_unload_step(context: *mut c_void) -> c_long {
    // SAFETY: `music_unload_score`'s local
    let unload = unsafe { &mut *(context as *mut MusicUnload) };
    unload.result = h(O::MusicUnloadScore, unload.song, &[]) as c_long;
    if unload.result == 5 {
        return 0;
    }
    h(O::MusicSetScore, unload.song, &[-1]);
    1
}

/// `EEMusicObject::LoadScore`: the score loaded on first use (a failed load leaves the count at 0).
pub fn music_load_score(song: P) -> c_long {
    acquire(O::MusicUse, song, O::MusicLock, step(song as *mut c_void, music_load_step), true)
}

/// `EEMusicObject::UnloadScore`: the last use unloads the score unless the conductor is busy with it (5).
pub fn music_unload_score(song: P) -> c_long {
    let mut unload = MusicUnload { song, result: 0 };
    let context = &mut unload as *mut MusicUnload as *mut c_void;
    release(O::MusicUse, song, O::MusicLock, step(context, music_unload_step));
    unload.result
}

// ---- the C++ forwards

/// # Safety
/// `file` is a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_dump_graphic_objects(file: *mut c_void) {
    dump_graphic_objects(file as P)
}

#[no_mangle]
pub extern "C" fn ee_preload_delete_media_table() {
    delete_media_table()
}

/// # Safety
/// `object` is a live `EEGraphicObject`; `index` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_find_turret_part(
    object: *mut c_void,
    model: *const c_void,
    index: *mut c_ulong,
) -> c_long {
    let mut found = 0;
    let result = find_turret_part(object as P, model as P, &mut found);
    if result == 0 {
        // SAFETY: the caller's guarantee
        unsafe { *index = found };
    }
    result
}

/// # Safety
/// `file` is a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_dump_effect_ids(file: *mut c_void) {
    dump_effect_ids(file as P)
}

#[no_mangle]
pub extern "C" fn ee_preload_unload_effects() {
    unload_effects()
}

/// # Safety
/// `effect` is a live `EEGraphicEffect`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_effect_media(effect: *mut c_void) -> *mut c_void {
    effect_media(
        effect as P,
        |kind| table::ee_gfx_geffect_kind_count(kind) as c_long,
        |kind, at| table::ee_gfx_geffect_kind_at(kind, at) as P,
    ) as *mut c_void
}

/// # Safety
/// `file` is a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_dump_sounds(file: *mut c_void) {
    dump_sounds(file as P)
}

#[no_mangle]
pub extern "C" fn ee_preload_unload_sounds() {
    unload_sounds()
}

/// # Safety
/// `sound` is a live `EESoundObject`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_play_sound(sound: *mut c_void, volume: f32, pan: f32, pitch: f32) -> c_long {
    play_sound(sound as P, volume, pan, pitch)
}

/// # Safety
/// `file` is a live `FSFile` (the table's).
#[no_mangle]
pub unsafe extern "C" fn ee_preload_load_sounds(file: *mut c_void) -> c_long {
    load_sounds(file as P)
}

/// # Safety
/// As [`ee_preload_load_sounds`].
#[no_mangle]
pub unsafe extern "C" fn ee_preload_load_graphic_objects(file: *mut c_void) -> c_long {
    load_graphic_objects(file as P)
}

/// # Safety
/// As [`ee_preload_load_sounds`].
#[no_mangle]
pub unsafe extern "C" fn ee_preload_load_effects(file: *mut c_void) -> c_long {
    load_effects(file as P)
}

/// # Safety
/// As [`ee_preload_load_sounds`].
#[no_mangle]
pub unsafe extern "C" fn ee_preload_load_music(file: *mut c_void) -> c_long {
    load_music(file as P)
}

/// # Safety
/// `sound` is a live `EESoundObject`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_sound_load_media(sound: *mut c_void) -> c_long {
    sound_load_media(sound as P)
}

/// # Safety
/// As [`ee_preload_sound_load_media`].
#[no_mangle]
pub unsafe extern "C" fn ee_preload_sound_unload_media(sound: *mut c_void) -> c_long {
    sound_unload_media(sound as P)
}

/// # Safety
/// `song` is a live `EEMusicObject`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_music_load_score(song: *mut c_void) -> c_long {
    music_load_score(song as P)
}

/// # Safety
/// As [`ee_preload_music_load_score`].
#[no_mangle]
pub unsafe extern "C" fn ee_preload_music_unload_score(song: *mut c_void) -> c_long {
    music_unload_score(song as P)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_dump_lines_are_the_printf_texts() {
        assert_eq!(media_count_line(b"a.cem", b" (filename)", 3), b"\ta.cem (filename) has a media count of 3\n");
        assert_eq!(media_count_line(b"boom", b"", -1), b"\tboom has a media count of -1\n");
    }

    #[test]
    fn none_is_all_ones_in_32_bits() {
        assert!(is_none32(0xffff_ffff));
        assert!(is_none32(-1));
        assert!(!is_none32(0));
        assert!(!is_none32(0x1_0000_0000));
    }

    #[test]
    fn effects_without_entries_link_nothing() {
        // no effect in the table: no C++ call
        let links = [
            EeDataGraphicEffectLinks { media: 0, linked: [0, 1, 2] },
            EeDataGraphicEffectLinks { media: 0, linked: [5, 5, 5] },
        ];
        link_effects(&[0, 0], &links);
        link_effects(&[], &[]);
    }

    /// A table file in memory: the count, then records (no lead strings).
    struct Mem(Vec<u8>, usize);

    impl FsFile for Mem {
        fn read_u32(&mut self, value: &mut u32) -> i32 {
            if self.1 + 4 > self.0.len() {
                return 1;
            }
            *value = u32::from_le_bytes(self.0[self.1..self.1 + 4].try_into().unwrap());
            self.1 += 4;
            0
        }
        fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
            let n = out.len().min(self.0.len() - self.1);
            out[..n].copy_from_slice(&self.0[self.1..self.1 + n]);
            self.1 += n;
            (if n == out.len() { 0 } else { 1 }, n as u32)
        }
    }

    #[test]
    fn a_table_is_its_count_then_each_record_after_its_lead() {
        let mut file = Mem(vec![2, 0, 0, 0, 1, 2, 3, 4, 5, 6], 0);
        let mut seen = Vec::new();
        let mut leads = Vec::new();
        let result = read_records(
            &mut file,
            3,
            |n| {
                assert_eq!(n, 2);
                true
            },
            |i| leads.push(i),
            |i, r| seen.push((i, r.to_vec())),
        );
        assert_eq!(result, 0);
        assert_eq!(leads, [0, 1]);
        assert_eq!(seen, [(0, vec![1, 2, 3]), (1, vec![4, 5, 6])]);
    }

    #[test]
    fn a_short_record_is_still_handed_on_and_its_status_returned() {
        let mut file = Mem(vec![2, 0, 0, 0, 1, 2, 3, 4], 0);
        let mut seen = Vec::new();
        let result = read_records(&mut file, 3, |_| true, |_| {}, |i, r| seen.push((i, r.to_vec())));
        assert_eq!(result, 1);
        // the buffer keeps the last record's bytes past a short read
        assert_eq!(seen, [(0, vec![1, 2, 3]), (1, vec![4, 2, 3])]);
    }

    #[test]
    fn no_count_or_a_refused_begin_reads_no_record() {
        let mut file = Mem(vec![1, 0], 0);
        assert_eq!(read_records(&mut file, 3, |_| panic!("no count"), |_| {}, |_, _| panic!()), 1);
        let mut file = Mem(vec![1, 0, 0, 0, 9, 9, 9], 0);
        assert_eq!(read_records(&mut file, 3, |_| false, |_| panic!(), |_, _| panic!()), 0);
        assert_eq!(file.1, 4);
    }
}
