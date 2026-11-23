//! The data pre-loading thread's media objects' bodies (world lane round 2): the graphic object table's dump and
//! delete and the turret part lookup (`EEGraphicObject`), the graphic effects' links, dump, unload and random graphic
//! (`EEGraphicEffect`), the sound table's dump and unload and a one-shot play (`EESoundObject`), the music table's
//! "New Music.txt" override and its epochs' songs (`EEMusicObject`). The C++ classes stay (their members are what the
//! game reads); each C++ call is one operation ([`EePreloadMediaCpp`]) answered by a one-statement case of
//! `ee_preload_mediacpp` (`src/Empire Earth/Data Pre-Loading Thread/EESoundObject.cpp`).

use std::ffi::{c_long, c_ulong, c_void};

use ee_data::tables::EeDataGraphicEffectLinks;
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
/// `effects` holds `count` effects (or NULLs) and `links` `count` link triples.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_link_effects(
    effects: *const *mut c_void,
    links: *const EeDataGraphicEffectLinks,
    count: usize,
) {
    if count == 0 {
        return;
    }
    // SAFETY: the caller's guarantee
    let (effects, links) =
        unsafe { (std::slice::from_raw_parts(effects.cast::<P>(), count), std::slice::from_raw_parts(links, count)) };
    link_effects(effects, links)
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
/// `out` has room for a music record.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_read_new_music(out: *mut u8) -> c_long {
    // SAFETY: the caller's guarantee
    unsafe { read_new_music(out) }
}

/// # Safety
/// `songs` holds `count` songs.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_add_epoch_songs(songs: *const *mut c_void, count: usize) {
    let songs = if count == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's guarantee
        unsafe { std::slice::from_raw_parts(songs.cast::<P>(), count) }
    };
    add_epoch_songs(songs)
}

/// # Safety
/// `song` is a live `EEMusicObject`.
#[no_mangle]
pub unsafe extern "C" fn ee_preload_add_song_to_every_epoch(song: *mut c_void) {
    add_song_to_every_epoch(song as P)
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
}
