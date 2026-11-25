//! The database's media tables in Rust (game core lane, round 10; was EETerrainTable.cpp, EEButtonObject.cpp,
//! EEAnimal.cpp): the db loaders' read of a counted record file, the terrains (their load, the mesh textures' use
//! count and attach, the list-box previews, the dump), the cliff terrains, the button images (their load, the
//! texture's use count and cell, the bitmaps, the reload) and the animal types (their load with their media and
//! sounds). The records and their copies are ee_data's (tables.rs), the use count pattern is ee_data's media.rs; each
//! C++ call or member is a case of the lane B chain (EETerrainTable.cpp's `EECore10OpB2`, EEButtonObject.cpp's
//! `EECore10OpB3`, EEAnimal.cpp's `EECore10OpB4`); the methods forward here (`ee_gamecpp_db_*`).
//!
//! The lock of each pattern is the C++ unit's own `sDatabaseLock` (one per includer of EEDatabase.h), answered by a
//! case of that unit.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::cell::Cell;
use std::ffi::{c_char, c_long, c_ulong, c_void, CStr, CString};

use ee_base::math::types::GeRectangle;
use ee_data::media::{self, EeDataMediaLock, EeDataMediaStep, EeDataMediaUse};
use ee_data::tables::{
    ee_data_animal_from_record, ee_data_animal_record_size, ee_data_button_cell, ee_data_button_cell_offset,
    ee_data_button_from_record, ee_data_button_record_size, ee_data_cliff_terrain_from_record,
    ee_data_cliff_terrain_record_size, ee_data_terrain_from_record, ee_data_terrain_record_size, terrain_texture_file,
    EeDataAnimalFields, EeDataAnimalMedia, EeDataButtonFields, EeDataButtonTexture, EeDataCliffTerrainFields,
    EeDataName, EeDataTerrainExtra, EeDataTerrainFields,
};
use ee_render::ge::texture_ffi::{ee_ge_unpack_blue, ee_ge_unpack_green, ee_ge_unpack_red};

use super::persistent::Pf;
use super::rast::{tex_mip_bias, tex_reference, tex_stage, TexStage};
use super::{cptr, h, hf, hp, pa, ptr, rec, O, P};

/// `kErrorOK`.
const DB_OK: i64 = 0;
/// LoadTexture's status for a missing button texture.
const DB_NO_TEXTURE: c_long = 3;
/// The texture stages' values (lle/GERenderState.h; static_assert'd in EETerrainTable.cpp).
const DB_STAGE_MODULATE: i64 = 2;
const DB_ADDRESSING_CLAMP: i64 = 0;
const DB_MIP_NONE: i64 = 0;
const DB_MIP_POINT: i64 = 1;
const DB_MIP_LINEAR: i64 = 2;
/// The colour palette (the original's .tga as PNG: RUST_PLAN D9).
const DB_PALETTE: &std::ffi::CStr = c"textures\\ColorPalette.png";
/// The terrain texture file name's room (the original's 256-byte buffer, its NUL included).
const DB_TEXTURE_NAME_ROOM: usize = 256;

// ---- the record files and the use count

/// EEDbReadLeadRecords: opens the file, reads the count, calls `begin(count)` (false: no records), then for every
/// record `lead(index)` and `each(index, bytes)`; a failed read still hands the buffer on, and with `stop_on_error`
/// the loop ends after that record. Returns the last read status.
pub(super) fn read_records(
    file: P,
    size: usize,
    begin: &mut dyn FnMut(c_ulong) -> bool,
    lead: &mut dyn FnMut(c_ulong),
    each: &mut dyn FnMut(c_ulong, *const u8),
    stop_on_error: bool,
) -> i64 {
    let mut record = vec![0u8; size];
    let mut count: c_ulong = 0;
    h(O::TkFileOpenRead, file, &[]);
    let mut result = Pf(file).read_ulong(&mut count);
    if result == DB_OK && begin(count) {
        for i in 0..count {
            lead(i);
            let mut read: c_ulong = 0;
            result = h(O::DtFileReadRecord, file, &[record.as_mut_ptr() as usize as i64, size as i64, ptr(&mut read)]);
            each(i, record.as_ptr());
            if stop_on_error && result != DB_OK {
                break;
            }
        }
    }
    h(O::TkFileClose, file, &[]);
    result
}

extern "C" fn lock_enter(lock: *mut c_void) {
    h(O::SsSectionEnter, 0, &[lock as usize as i64]);
}

extern "C" fn lock_leave(lock: *mut c_void) {
    h(O::SsSectionLeave, 0, &[lock as usize as i64]);
}

extern "C" fn lock_sleep() {
    ee_base::plat_core::time::PlatSleep(1);
}

/// A unit's `sDatabaseLock` (its `TSCriticalSection*`) as the use count's lock.
fn media_lock(lock: P) -> EeDataMediaLock {
    EeDataMediaLock { lock: lock as *mut c_void, enter: lock_enter, leave: lock_leave, sleep: lock_sleep }
}

extern "C" fn run_step<F: FnMut() -> c_long>(context: *mut c_void) -> c_long {
    // SAFETY: the context is the closure `step` was given, alive for the pattern's call
    unsafe { (*(context as *mut F))() }
}

/// A Rust closure as a step of the pattern (alive while the step is).
fn step<F: FnMut() -> c_long>(f: &mut F) -> EeDataMediaStep {
    EeDataMediaStep { context: f as *mut F as *mut c_void, call: Some(run_step::<F>) }
}

/// A C string the C++ answered (a UString's text), as bytes.
fn text_of(p: P) -> Vec<u8> {
    if p == 0 {
        return Vec::new();
    }
    // SAFETY: a NUL-terminated string the C++ owns, read before the next C++ call
    unsafe { CStr::from_ptr(p as *const c_char) }.to_bytes().to_vec()
}

/// Bytes as a C string for the C++ (cut at a NUL, as the C++'s `c_str` would be read).
fn c_text(bytes: &[u8]) -> CString {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    CString::new(&bytes[..end]).unwrap_or_default()
}

/// fprintf(file, "%s", text) on a media dump file.
fn print(file: P, text: &[u8]) {
    let c = c_text(text);
    h(O::DtFilePrint, file, &[c.as_ptr() as usize as i64]);
}

/// A slot of a C++ member the Rust reads and writes in place (its address case).
fn slot(op: O, object: P) -> *mut P {
    hp(op, object, &[]) as *mut P
}

fn get(s: *mut P) -> P {
    // SAFETY: a live member's address, used before the next C++ call that could free its object
    unsafe { *s }
}

fn set(s: *mut P, v: P) {
    // SAFETY: as get
    unsafe { *s = v }
}

fn use_count(usage: *mut EeDataMediaUse) -> c_ulong {
    // SAFETY: the object's use record (its base), read as the C++ dumps read it
    unsafe { (*usage).mMediaCount }
}

// ---- the terrains

fn terrain_fields(t: P) -> EeDataTerrainFields {
    // SAFETY: the terrain's record (EETerrainTable's base), copied out
    unsafe { *rec::<EeDataTerrainFields>(O::DtTerrainFields, t) }
}

fn terrain_use(t: P) -> *mut EeDataMediaUse {
    hp(O::DtTerrainUse, t, &[]) as *mut EeDataMediaUse
}

/// The terrain's texture array (`mTextures`, a C++ `GETexture**`).
fn textures(t: P) -> *mut P {
    get(slot(O::DtTerrainTextures, t)) as *mut P
}

fn texture_at(array: *mut P, i: c_long) -> P {
    // SAFETY: an index of the terrain's texture array (mColumns * mRows entries)
    unsafe { *array.offset(i as isize) }
}

/// EETerrainTable::Load(FSFileSpec*): the terrains read (each after its texture file name), their textures numbered
/// over the table, the first named one the default terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_load(file: *mut c_void) -> c_long {
    let file = file as P;
    let default = hp(O::DtDefaultTerrain, 0, &[]) as *mut c_long;
    // SAFETY: gDefaultTerrain's address
    unsafe { *default = -1 };
    let base = Cell::new(0 as c_long);
    let terrain = Cell::new(0 as P);
    read_records(
        file,
        ee_data_terrain_record_size(),
        &mut |count| {
            h(O::DtTerrainResize, 0, &[count as i64]);
            true
        },
        &mut |_| {
            let t = hp(O::DtTerrainNew, 0, &[]);
            terrain.set(t);
            // SAFETY: the new terrain's mFirstTexture
            unsafe { *(hp(O::DtTerrainFirstTexture, t, &[]) as *mut c_long) = base.get() };
            h(O::DtTerrainLoadTextureName, t, &[pa(file)]);
        },
        &mut |i, record| {
            let t = terrain.get();
            // SAFETY: an all-zero name (a null text of length 0) and colour, overwritten by the copy
            let mut extra: EeDataTerrainExtra = unsafe { std::mem::zeroed() };
            // SAFETY: a whole record; the terrain's record and the out value are live
            unsafe { ee_data_terrain_from_record(record, rec(O::DtTerrainFields, t), &mut extra) };
            h(O::DtTerrainCopyName, t, &[cptr(&extra.name)]);
            for k in 0..3 {
                if extra.first[k] > 0 && extra.last[k] > 0 {
                    let mut class = extra.first[k] as c_long;
                    while (class as c_ulong) <= extra.last[k] as c_ulong {
                        h(O::DtTerrainPushClass, t, &[class as i64]);
                        class = class.wrapping_add(1);
                    }
                }
            }
            hf(O::DtTerrainColor, t, &[], &extra.color);
            let fields = terrain_fields(t);
            // SAFETY: gDefaultTerrain's address
            if unsafe { *default } == -1 && h(O::DtTerrainNameLength, t, &[]) > 0 && fields.mID != -1 {
                // SAFETY: as above
                unsafe { *default = fields.mID };
            }
            h(O::DtTerrainSet, 0, &[i as i64, t as i64]);
            base.set(base.get().wrapping_add(fields.mNumTextures));
        },
        false,
    ) as c_long
}

/// EETerrainTable::Unload: every terrain's textures released, the terrains destroyed, the table emptied (the texture
/// arrays are not freed).
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_unload() {
    let mut j: c_ulong = 0;
    let count = h(O::DtTerrainCount, 0, &[]) as c_ulong;
    for i in 0..count {
        let t = hp(O::DtTerrainAt, 0, &[i as i64]);
        if t == 0 {
            continue;
        }
        let f = terrain_fields(t);
        let texture_count = f.mRows.wrapping_mul(f.mColumns) as c_ulong;
        let array = textures(t);
        if !array.is_null() && texture_count > 0 {
            while j < texture_count {
                h(O::DtTexDereference, texture_at(array, j as c_long), &[]);
                j += 1;
            }
        }
        j = 0;
        h(O::DtTerrainDelete, t, &[]);
    }
    h(O::DtTerrainResize, 0, &[0]);
}

/// EETerrainTable::DumpAllLoadedMedia: each referenced terrain and each with a list-box preview.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_dump(file: *mut c_void) {
    let file = file as P;
    print(file, b"\n\n**********Terrain Table*********\n\n");
    let count = h(O::DtTerrainCount, 0, &[]) as c_ulong;
    for i in 0..count {
        let t = hp(O::DtTerrainGet, 0, &[i as i64]);
        if t == 0 {
            continue;
        }
        let uses = use_count(terrain_use(t));
        if uses > 0 {
            let mut line = b"\t".to_vec();
            line.extend(text_of(hp(O::DtTerrainName, t, &[])));
            line.extend(format!(" has a media count of {}\n", uses as c_long).bytes());
            print(file, &line);
        }
        if get(slot(O::DtTerrainPreview, t)) != 0 {
            let mut line = b"\t".to_vec();
            line.extend(text_of(hp(O::DtTerrainName, t, &[])));
            line.extend(b" was loaded for list box display\n");
            print(file, &line);
        }
    }
}

/// Sets the terrain's textures (columns mirrored) into a texture set.
fn attach(t: P, set: P) {
    let f = terrain_fields(t);
    let array = textures(t);
    for y in 0..f.mRows {
        for x in 0..f.mColumns {
            let texture = texture_at(array, f.mColumns.wrapping_mul(y).wrapping_add(x));
            h(O::DtSetTexture, set, &[(f.mColumns - x - 1) as i64, y as i64, texture as i64]);
        }
    }
}

/// EETerrainTable::Load(GETerrainMesh&) (`load`) and Unload(GETerrainMesh&): the first reference loads the textures
/// and attaches them to the mesh's texture set; the last detaches and releases them.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_mesh(terrain: *mut c_void, mesh: *mut c_void, load: bool) {
    let t = terrain as P;
    let mesh = mesh as P;
    let lock = media_lock(hp(O::DtTerrainLock, 0, &[]));
    let usage = terrain_use(t);
    let texture_set = || hp(O::DtMeshTextureSet, mesh, &[h(O::DtTerrainTextureSetId, t, &[])]);
    if load {
        let mut load_step = || {
            ee_gamecpp_db_terrain_textures(t as *mut c_void);
            0
        };
        let mut after = || {
            attach(t, texture_set());
            0
        };
        media::acquire(usage, &lock, &step(&mut load_step), &step(&mut after), false);
    } else {
        let mut unload = || {
            let set = texture_set();
            let f = terrain_fields(t);
            for y in 0..f.mRows {
                for x in 0..f.mColumns {
                    h(O::DtSetTexture, set, &[x as i64, y as i64, 0]);
                }
            }
            let count = f.mColumns.wrapping_mul(f.mRows) as c_ulong;
            let array = textures(t);
            for i in 0..count {
                h(O::DtTexDereference, texture_at(array, i as c_long), &[]);
                // SAFETY: an index of the array
                unsafe { *array.offset(i as isize) = 0 };
            }
            h(O::DtTerrainDeleteTextures, t, &[]);
            1
        };
        media::release(usage, &lock, &step(&mut unload));
    }
}

/// EETerrainTable::AttachTextures: the loaded textures (columns mirrored) into a texture set, once any load is done.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_attach(terrain: *mut c_void, texture_set: *mut c_void) {
    let t = terrain as P;
    let lock = media_lock(hp(O::DtTerrainLock, 0, &[]));
    let usage = terrain_use(t);
    let mut attach_step = || {
        if use_count(usage) > 0 {
            attach(t, texture_set as P);
        }
        0
    };
    media::locked(usage, &lock, &step(&mut attach_step));
}

/// EETerrainTable::LoadTexture: one terrain texture with its terrain stage modes (clamped; a linear mip filter when
/// trilinear filtering is on). No NULL test: the original references the result directly.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_texture(terrain: *mut c_void, name: *const c_char) -> *mut c_void {
    let texture = hp(O::DtTerrainLoadTexture, terrain as P, &[name as usize as i64]);
    tex_reference(texture);
    h(O::DtTexSetFrame, texture, &[0]);
    tex_stage(texture, TexStage::AlphaMode, DB_STAGE_MODULATE);
    tex_stage(texture, TexStage::ColorMode, DB_STAGE_MODULATE);
    tex_stage(texture, TexStage::MinFilterMode, 1);
    tex_stage(texture, TexStage::MagFilterMode, 1);
    tex_mip_bias(texture, -1.0);
    tex_stage(texture, TexStage::TextureAddressingMode, DB_ADDRESSING_CLAMP);
    let mip = if h(O::DtTrilinearFilter, 0, &[]) != 0 { DB_MIP_LINEAR } else { DB_MIP_POINT };
    tex_stage(texture, TexStage::MipFilterMode, mip);
    texture as *mut c_void
}

/// A terrain texture's file: the terrain's texture name numbered _n before its extension (the original's 256-byte
/// buffer: at most 255 bytes kept).
fn texture_file(t: P, n: c_long) -> CString {
    let mut file = terrain_texture_file(&text_of(hp(O::DtTerrainTextureName, t, &[])), n);
    file.truncate(DB_TEXTURE_NAME_ROOM - 1);
    c_text(&file)
}

fn load_texture(t: P, n: c_long) -> P {
    let file = texture_file(t, n);
    ee_gamecpp_db_terrain_texture(t as *mut c_void, file.as_ptr()) as P
}

/// EETerrainTable::LoadTextures: every texture of the grid (numbered _n, from the last down) into a new array.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_textures(terrain: *mut c_void) {
    let t = terrain as P;
    let f = terrain_fields(t);
    let count = f.mRows.wrapping_mul(f.mColumns) as c_ulong;
    h(O::DtTerrainNewTextures, t, &[count as i64]);
    let array = textures(t);
    for (index, n) in (1..=count).rev().enumerate() {
        let texture = load_texture(t, n as c_long);
        // SAFETY: an index of the new array (count entries)
        unsafe { *array.add(index) = texture };
    }
}

/// EELoadTerrainPreviews (`load`) and EEReleaseTerrainPreviews: each terrain's list-box preview (its texture _1)
/// loaded or released, and its first unit class's button referenced or dereferenced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_terrain_previews(load: bool) {
    let count = h(O::DtTerrainCount, 0, &[]) as c_ulong;
    for i in 0..count {
        let t = hp(O::DtTerrainGet, 0, &[i as i64]);
        if t == 0 {
            continue;
        }
        let preview = slot(O::DtTerrainPreview, t);
        if load {
            set(preview, load_texture(t, 1));
        } else {
            h(O::DtTexDereference, get(preview), &[]);
            set(preview, 0);
        }
        if h(O::DtTerrainClassCount, t, &[]) <= 0 {
            continue;
        }
        let class = hp(O::TkClassAt, 0, &[h(O::DtTerrainClass0, t, &[])]);
        if class != 0 {
            let button = hp(O::DtClassButton, class, &[]);
            if load {
                button_acquire(button);
            } else {
                button_release(button);
            }
        }
    }
}

// ---- the cliff terrains

/// EECliffTerrainTable::Load: the cliff terrains (NULL for a row with a negative index); the first one noted.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_cliff_load(file: *mut c_void) -> c_long {
    let first = hp(O::DtFirstCliffTerrain, 0, &[]) as *mut c_long;
    let mut found = false;
    read_records(
        file as P,
        ee_data_cliff_terrain_record_size(),
        &mut |count| {
            h(O::DtCliffResize, 0, &[count as i64]);
            true
        },
        &mut |_| {},
        &mut |i, record| {
            let mut cliff = hp(O::DtCliffNew, 0, &[]);
            let mut name = EeDataName { text: std::ptr::null(), length: 0 };
            // SAFETY: a whole record; the cliff's record and the name are live
            if unsafe {
                ee_data_cliff_terrain_from_record(
                    record,
                    rec::<EeDataCliffTerrainFields>(O::DtCliffFields, cliff),
                    &mut name,
                )
            } {
                h(O::DtCliffCopyName, cliff, &[cptr(&name)]);
                if !found {
                    // SAFETY: gFirstCliffTerrain's address
                    unsafe { *first = i as c_long };
                    found = true;
                }
            } else {
                h(O::DtCliffDelete, cliff, &[]);
                cliff = 0;
            }
            h(O::DtCliffSet, 0, &[i as i64, cliff as i64]);
        },
        false,
    ) as c_long
}

/// EECliffTerrainTable::Unload: every cliff terrain destroyed, the table emptied; 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_cliff_unload() -> c_long {
    let count = h(O::DtCliffCount, 0, &[]) as c_ulong;
    for i in 0..count {
        h(O::DtCliffDelete, hp(O::DtCliffAt, 0, &[i as i64]), &[]);
    }
    h(O::DtCliffClear, 0, &[]);
    0
}

// ---- the button images

fn button_fields(b: P) -> EeDataButtonFields {
    // SAFETY: the button's record (EEButtonObject's base), copied out
    unsafe { *rec::<EeDataButtonFields>(O::DbtFields, b) }
}

fn button_use(b: P) -> *mut EeDataMediaUse {
    hp(O::DbtUse, b, &[]) as *mut EeDataMediaUse
}

/// `GETextureDescription` (lle/GETextureManager.h), member for member.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct DbTextureDescription {
    surface_volatility: c_ulong,
    frame_format: c_ulong,
    pixel_depth: c_ulong,
    mip_levels_to_drop: c_long,
    is_3d_renderable: bool,
    transparency: bool,
    has_alpha: bool,
    mip_maps: bool,
    use_texture_pack: bool,
}

/// One mip level dropped at 800 pixels wide or less.
fn button_mip_drop() -> c_long {
    if h(O::DbtScreenWidth, 0, &[]) as c_ulong > 800 {
        0
    } else {
        1
    }
}

/// The button's destructor: its texture released.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_free(button: *mut c_void) {
    let texture = get(slot(O::DbtTexture, button as P));
    if texture != 0 {
        h(O::DtTexDereference, texture, &[]);
    }
}

/// EEButtonObject::Load: the button images (stopping at the first read error).
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_load(file: *mut c_void) -> c_long {
    read_records(
        file as P,
        ee_data_button_record_size(),
        &mut |count| {
            h(O::DbtResize, 0, &[count as i64]);
            true
        },
        &mut |_| {},
        &mut |i, record| {
            let b = hp(O::DbtNew, 0, &[]);
            h(O::DbtSet, 0, &[i as i64, b as i64]);
            // SAFETY: a whole record; the button's record is live
            let name = unsafe { ee_data_button_from_record(record, rec(O::DbtFields, b)) };
            h(O::DbtCopyName, b, &[cptr(&name)]);
        },
        true,
    ) as c_long
}

/// EEButtonObject::Unload: every button image destroyed, the table emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_unload() {
    let count = h(O::DbtCount, 0, &[]) as c_ulong;
    for i in 0..count {
        h(O::DbtDelete, hp(O::DbtAt, 0, &[i as i64]), &[]);
    }
    h(O::DbtResize, 0, &[0]);
}

/// EEButtonObject::DumpAllLoadedMedia: each referenced button image.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_dump(file: *mut c_void) {
    let file = file as P;
    print(file, b"\n\n**********Button Database Table*********\n\n");
    let count = h(O::DbtCount, 0, &[]) as c_ulong;
    for i in 0..count {
        let b = hp(O::DbtAt, 0, &[i as i64]);
        if b != 0 && use_count(button_use(b)) > 0 {
            let mut line = b"\t".to_vec();
            line.extend(text_of(hp(O::DbtName, b, &[])));
            line.extend(format!(" (filename) has a media count of {}\n", use_count(button_use(b)) as c_long).bytes());
            print(file, &line);
        }
    }
}

fn button_acquire(b: P) -> c_long {
    let lock = media_lock(hp(O::DbtLock, 0, &[]));
    let mut load = || ee_gamecpp_db_button_load_texture(b as *mut c_void);
    let mut after = || 0;
    media::acquire(button_use(b), &lock, &step(&mut load), &step(&mut after), true)
}

fn button_release(b: P) -> c_long {
    let lock = media_lock(hp(O::DbtLock, 0, &[]));
    let mut unload = || {
        let texture = slot(O::DbtTexture, b);
        h(O::DtTexDereference, get(texture), &[]);
        set(texture, 0);
        1
    };
    media::release(button_use(b), &lock, &step(&mut unload));
    0
}

/// EEButtonObject::Acquire: a reference; the first loads the texture (a failed load leaves the count at 0).
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_acquire(button: *mut c_void) -> c_long {
    button_acquire(button as P)
}

/// EEButtonObject::Release: a reference dropped once any load is done, the texture released with the last; 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_release(button: *mut c_void) -> c_long {
    button_release(button as P)
}

/// EEButtonObject::LoadTexture: the texture (a mip level dropped at 800 pixels wide or less, alpha for ids below 50)
/// and its cell's offset; 3 when the texture is missing.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_load_texture(button: *mut c_void) -> c_long {
    let b = button as P;
    let pixel_depth = h(O::DbtPixelDepth, 0, &[]) as c_ulong;
    let mip_levels_to_drop = button_mip_drop();
    let fields = button_fields(b);
    let description = DbTextureDescription {
        surface_volatility: 1,
        frame_format: 0,
        pixel_depth,
        mip_levels_to_drop,
        is_3d_renderable: false,
        transparency: false,
        has_alpha: fields.mID < 50,
        mip_maps: false,
        use_texture_pack: true,
    };
    let texture_slot = slot(O::DbtTexture, b);
    set(texture_slot, hp(O::DbtLoadTexture, b, &[cptr(&description)]));
    let texture = get(texture_slot);
    if texture == 0 {
        return DB_NO_TEXTURE;
    }
    tex_reference(texture);
    tex_stage(texture, TexStage::MinFilterMode, 0);
    tex_stage(texture, TexStage::MagFilterMode, 0);
    tex_stage(texture, TexStage::MipFilterMode, DB_MIP_NONE);
    tex_mip_bias(texture, 0.0);
    let size = EeDataButtonTexture {
        width: h(O::DbtTexWidth, texture, &[]) as c_ulong,
        height: h(O::DbtTexHeight, texture, &[]) as c_ulong,
        drop: h(O::DbtTexMipDrop, texture, &[]) as c_ulong,
        surface_height: 0,
    };
    let mut cell = [0f32; 2];
    // SAFETY: the size and the two floats are live
    unsafe { ee_data_button_cell_offset(fields.mColumn, fields.mRow, &size, cell.as_mut_ptr()) };
    // SAFETY: the button's mCellU and mCellV
    unsafe {
        *(hp(O::DbtCellU, b, &[]) as *mut f32) = cell[0];
        *(hp(O::DbtCellV, b, &[]) as *mut f32) = cell[1];
    }
    DB_OK as c_long
}

/// EEButtonObject::SetFaceBitmap (`face`: inset by one texel; skipped without a texture or its surface) and
/// SetPressedBitmap: the button's cell on a bitmap scaled to a rectangle.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_bitmap(
    button: *mut c_void,
    face: bool,
    bitmap: *mut c_void,
    rectangle: *const c_void,
    screen: c_ulong,
) {
    let b = button as P;
    let bitmap = bitmap as P;
    let texture = get(slot(O::DbtTexture, b));
    if face && (texture == 0 || hp(O::DbtTexSurface, texture, &[]) == 0) {
        return;
    }
    // SAFETY: the button's mCellU and mCellV
    let (cell_u, cell_v) =
        unsafe { (*(hp(O::DbtCellU, b, &[]) as *const f32), *(hp(O::DbtCellV, b, &[]) as *const f32)) };
    let size = EeDataButtonTexture {
        width: h(O::DbtTexWidth, texture, &[]) as c_ulong,
        height: h(O::DbtTexHeight, texture, &[]) as c_ulong,
        drop: h(O::DbtTexMipDrop, texture, &[]) as c_ulong,
        surface_height: h(O::DbtTexSurfaceHeight, texture, &[]) as c_ulong,
    };
    h(O::DbtBitmapTexture, bitmap, &[texture as i64, screen as i64]);
    // SAFETY: the caller's rectangle (GERectangle's mirror)
    let r = unsafe { *(rectangle as *const GeRectangle) };
    // SAFETY: the size is live
    let cell = unsafe { ee_data_button_cell(&size, cell_u, cell_v, face, r.right - r.left, r.bottom - r.top) };
    hf(O::DbtBitmapCoordinates, bitmap, &[], &[cell.left, cell.top, cell.right, cell.bottom]);
    hf(O::DbtBitmapScale, bitmap, &[], &[cell.scale_x, cell.scale_y]);
}

/// EEButtonObject::Reload: every referenced button image's texture reloaded (as on its first load), under the lock;
/// the first error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_button_reload() -> c_long {
    let mut description = DbTextureDescription {
        surface_volatility: 1,
        frame_format: 0,
        pixel_depth: 16,
        mip_levels_to_drop: 0,
        is_3d_renderable: false,
        transparency: false,
        has_alpha: true,
        mip_maps: true,
        use_texture_pack: true,
    };
    let mut result = DB_OK;
    let lock = hp(O::DbtLock, 0, &[]);
    lock_enter(lock as *mut c_void);
    description.pixel_depth = h(O::DbtPixelDepth, 0, &[]) as c_ulong;
    description.mip_maps = false;
    description.mip_levels_to_drop = button_mip_drop();
    let count = h(O::DbtCount, 0, &[]) as c_ulong;
    let mut i: c_ulong = 0;
    while i < count && result == DB_OK {
        let b = hp(O::DbtAt, 0, &[i as i64]);
        if b != 0 && use_count(button_use(b)) > 0 {
            description.has_alpha = button_fields(b).mID < 50;
            result = h(O::DbtReloadTexture, b, &[cptr(&description)]);
        }
        i += 1;
    }
    lock_leave(lock as *mut c_void);
    result as c_long
}

/// EEButtonObject::GetHotKey (and the node's): the hot key of an index, NULL out of range.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_hot_key(index: c_long) -> *mut c_void {
    if index > 0 && index < ee_ui::hotkeyobj::ee_ui_hotkey_count() as c_long {
        return hp(O::DbtHotKey, 0, &[index as i64]) as *mut c_void;
    }
    std::ptr::null_mut()
}

// ---- the animal types

/// The animal type's destructor (`with_media`: its sounds and then its media released) and ClearSounds (its sounds
/// released, last first, and the list emptied).
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_animal_free(animal: *mut c_void, with_media: bool) {
    let a = animal as P;
    let count = h(O::DanSoundCount, a, &[]) as c_long;
    for i in (0..count).rev() {
        h(O::DanMediaUnload, hp(O::DanSoundAt, a, &[i as i64]), &[]);
    }
    h(O::DanSoundsClear, a, &[]);
    if with_media {
        let media = get(slot(O::DanMedia, a));
        if media != 0 {
            h(O::DanMediaUnload, media, &[]);
        }
    }
}

/// EEAnimal::Load: the animal types (any loaded ones unloaded first), each one's media and up to three sounds loaded.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_animal_load(file: *mut c_void) -> c_long {
    if h(O::DanCount, 0, &[]) > 0 {
        ee_gamecpp_db_animal_unload();
    }
    read_records(
        file as P,
        ee_data_animal_record_size(),
        &mut |count| {
            h(O::DanResize, 0, &[count as i64]);
            true
        },
        &mut |_| {},
        &mut |i, record| {
            let a = hp(O::DanNew, 0, &[]);
            let mut m = EeDataAnimalMedia::default();
            // SAFETY: a whole record; the animal's record and the media are live
            unsafe { ee_data_animal_from_record(record, rec::<EeDataAnimalFields>(O::DanFields, a), &mut m) };
            h(O::DanSet, 0, &[i as i64, a as i64]);
            if m.media >= 0 {
                let media = hp(O::DanMediaGet, 0, &[m.media as i64]);
                set(slot(O::DanMedia, a), media);
                if media != 0 {
                    h(O::DanMediaLoad, media, &[]);
                }
            }
            for k in 0..3 {
                let sound = if m.sounds[k] >= 0 { hp(O::DanMediaGet, 0, &[m.sounds[k] as i64]) } else { 0 };
                if sound != 0 {
                    h(O::DanMediaLoad, sound, &[]);
                    h(O::DanSoundPush, a, &[sound as i64]);
                }
            }
            for k in 0..4 {
                if m.list[k] > 0 {
                    h(O::DanListPush, a, &[m.list[k] as i64]);
                }
            }
        },
        false,
    ) as c_long
}

/// EEAnimal::Unload: every animal type destroyed (last first), the table emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_animal_unload() {
    let count = h(O::DanCount, 0, &[]) as c_long;
    for i in (0..count).rev() {
        h(O::DanDelete, hp(O::DanAt, 0, &[i as i64]), &[]);
    }
    h(O::DanClear, 0, &[]);
}

// ---- the terrain colours

/// `GESurfaceDescription` (lle/GESurface.h), member for member (static_assert'd in EETerrainColorTable.cpp).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
struct DbSurfaceDescription {
    kind: c_ulong,
    pixel_format: c_ulong,
    volatility: c_ulong,
    width: c_ulong,
    height: c_ulong,
    m14: c_ulong,
    mip_level_count: c_long,
    pixel_depth: c_ulong,
    is_3d_renderable: bool,
    transparency: bool,
    no_alpha: bool,
}

/// `kSurfaceSystemMemory`, `kPixelFormatA8R8G8B8`.
const DC_SYSTEM_MEMORY: c_ulong = 4;
const DC_A8R8G8B8: c_ulong = 0;
/// The colour palette's components to 0..1 (the original's constant 1/255).
const DC_COLOR_SCALE: f32 = 1.0 / 255.0;

fn verify(ok: bool, line: i64) {
    h(O::DcVerify, 0, &[ok as i64, line]);
}

/// EETerrainColorTable::Load: the terrain colours from the pixels of the colour palette texture (16 per row; each
/// component / 255); the texture load status.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_colors_load() -> c_long {
    let mut description = DbSurfaceDescription {
        kind: DC_SYSTEM_MEMORY,
        pixel_format: DC_A8R8G8B8,
        volatility: 0,
        width: 16,
        height: 16,
        m14: 0,
        mip_level_count: 0,
        pixel_depth: 32,
        is_3d_renderable: false,
        transparency: false,
        no_alpha: false,
    };
    let mut surface: P = 0;
    let mut result = h(O::DcCreateSurface, 0, &[ptr(&mut description), ptr(&mut surface)]);
    verify(result == DB_OK, 76);
    let mut file: P = 0;
    // the original's .tga as PNG (RUST_PLAN D9)
    result = h(O::DcFindFile, 0, &[DB_PALETTE.as_ptr() as usize as i64, ptr(&mut file)]);
    verify(result == DB_OK, 81);
    verify(file != 0, 82);
    result = h(O::DcSurfaceLoad, surface, &[pa(file)]);
    verify(result == DB_OK, 84);
    if file != 0 {
        h(O::DxDelete, file, &[]);
    }
    h(O::DcLock, surface, &[]);
    let pixels = hp(O::DcMemory, surface, &[]) as *const u32;
    let row_length = h(O::DcBytesPerRow, surface, &[]) as c_ulong / 4;
    if result == DB_OK {
        let count = (h(O::DcWidth, surface, &[]) as c_ulong).wrapping_mul(h(O::DcHeight, surface, &[]) as c_ulong);
        h(O::DcResize, 0, &[count as i64]);
        for i in 0..count {
            let color = hp(O::DcNew, 0, &[]);
            // SAFETY: a pixel of the locked 16-wide surface
            let pixel = unsafe { *pixels.add(((i / 16) * row_length + (i % 16)) as usize) } as c_ulong;
            let red = ee_ge_unpack_red(0, pixel) as f32 * DC_COLOR_SCALE;
            let green = ee_ge_unpack_green(DC_A8R8G8B8, pixel) as f32 * DC_COLOR_SCALE;
            let blue = ee_ge_unpack_blue(0, pixel) as f32 * DC_COLOR_SCALE;
            hf(O::DcSetColor, color, &[], &[red, green, blue]);
            h(O::DcSet, 0, &[i as i64, color as i64]);
        }
    }
    h(O::DcUnlock, surface, &[]);
    h(O::DcDereference, surface, &[]);
    result as c_long
}

/// EETerrainColorTable::Unload: every colour destroyed, the table emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_colors_unload() {
    let count = h(O::DcCount, 0, &[]) as c_ulong;
    for i in 0..count {
        h(O::DcDelete, hp(O::DcAt, 0, &[i as i64]), &[]);
    }
    h(O::DcResize, 0, &[0]);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    #[test]
    fn the_texture_description_is_the_cpps() {
        assert_eq!(offset_of!(DbTextureDescription, mip_levels_to_drop), 3 * size_of::<c_ulong>());
        assert_eq!(offset_of!(DbTextureDescription, is_3d_renderable), 4 * size_of::<c_long>());
        assert_eq!(offset_of!(DbTextureDescription, use_texture_pack), 4 * size_of::<c_long>() + 4);
    }

    #[test]
    fn the_surface_description_is_the_cpps() {
        assert_eq!(size_of::<DbSurfaceDescription>(), 9 * size_of::<c_ulong>());
        assert_eq!(offset_of!(DbSurfaceDescription, is_3d_renderable), size_of::<[c_ulong; 8]>());
        assert_eq!(DB_PALETTE.to_bytes(), b"textures\\ColorPalette.png");
        assert_eq!(DC_COLOR_SCALE, 1.0f32 / 255.0f32);
    }

    #[test]
    fn a_step_runs_its_closure_and_answers_its_status() {
        let mut runs = 0;
        let mut f = || {
            runs += 1;
            7
        };
        let s = step(&mut f);
        assert_eq!((s.call.unwrap())(s.context), 7);
        assert_eq!(runs, 1);
    }

    #[test]
    fn a_c_text_stops_at_a_nul_and_a_texture_name_keeps_255_bytes() {
        assert_eq!(c_text(b"ab\0cd").as_bytes(), b"ab");
        let mut long = terrain_texture_file(&[b'x'; 300], 1);
        long.truncate(DB_TEXTURE_NAME_ROOM - 1);
        assert_eq!(long.len(), 255);
        assert_eq!(text_of(0), b"");
    }

    #[test]
    #[should_panic(expected = "no C++")]
    fn a_read_is_a_host_call() {
        read_records(1, 4, &mut |_| true, &mut |_| {}, &mut |_, _| {}, false);
    }
}
