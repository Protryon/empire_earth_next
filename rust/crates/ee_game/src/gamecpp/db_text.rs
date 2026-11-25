//! The db tables' text forms served to the loaders (game core lane, round 10; was EEDataTextFile.cpp): a table's
//! `.dat` name becomes its text form (`.csv` / `.yaml`) found on the data path, compiled by ee_data into the record
//! stream the loaders read and served as a C++ `FSMemoryFile`; the row ids the computer players' scripts name
//! (dbobjects, dbtechtree); the rules documents' load through the same compiler content; the content dump
//! (`EE_DUMP_CONTENT`). A table that cannot be served stops the game with its name (RUST_PLAN D9). Each C++ call is a
//! case of EEDataTextFile.cpp's `EECore10OpB5`; the functions forward here (`ee_gamecpp_db_*`).

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr, CString};
use std::io::Write;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Mutex;

use ee_data::content::text_extension;
use ee_data::ffi::{
    ee_data_bytes_free, ee_data_compile, ee_data_content_new, ee_data_dump_table, ee_data_rules_dump,
    ee_data_rules_load, ee_data_sink_write, EeDataBytes, EeDataContent, EeDataRules, EeDataSink,
};

use super::{h, hp, ptr, O, P};

/// `kErrorOK`, `kErrorFailed`.
const DX_OK: i64 = 0;
const DX_FAILED: c_long = 9;

/// The path and folder the text tables are read from (the path of the current lookup): the `FSPath*` and the folder.
static TEXT_PATH: Mutex<(P, Vec<u8>)> = Mutex::new((0, Vec::new()));
/// The one compiler content of every table (made on first use, never freed: the C++'s function static).
static CONTENT: AtomicPtr<EeDataContent> = AtomicPtr::new(std::ptr::null_mut());

/// Bytes the ee_data C interface returned, taken (and freed).
fn take(bytes: &mut EeDataBytes) -> Vec<u8> {
    let out = if bytes.ptr.is_null() || bytes.len == 0 {
        Vec::new()
    } else {
        // SAFETY: a buffer of ee_data's, `len` bytes
        unsafe { std::slice::from_raw_parts(bytes.ptr, bytes.len) }.to_vec()
    };
    // SAFETY: the buffer came from ee_data and is freed once
    unsafe { ee_data_bytes_free(std::mem::replace(bytes, EeDataBytes::empty())) };
    out
}

/// Bytes as a C string (cut at a NUL, as the C++'s `c_str` is read).
fn c_text(bytes: &[u8]) -> CString {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    CString::new(&bytes[..end]).unwrap_or_default()
}

/// A path's file: the `FSFileSpec*` it found, or 0.
fn find_file(path: P, name: &[u8]) -> P {
    let c = c_text(name);
    let mut spec: P = 0;
    if h(O::DxFindFile, path, &[c.as_ptr() as usize as i64, ptr(&mut spec)]) != DX_OK {
        return 0;
    }
    spec
}

/// A whole file's bytes; None when it cannot be read.
fn read_whole_file(spec: P) -> Option<Vec<u8>> {
    if h(O::DxOpen, spec, &[]) != DX_OK {
        return None;
    }
    let mut length: c_ulong = 0;
    let mut ok = h(O::DxLength, spec, &[ptr(&mut length)]) == DX_OK;
    let mut data = Vec::new();
    if ok {
        data.resize(length as usize, 0u8);
        let mut read: c_ulong = 0;
        ok = length == 0
            || (h(O::DtFileReadRecord, spec, &[data.as_mut_ptr() as usize as i64, length as i64, ptr(&mut read)])
                == DX_OK
                && read == length);
    }
    h(O::DxClose, spec, &[]);
    ok.then_some(data)
}

/// The content's reader: a table's text through the current path and folder.
unsafe extern "C" fn read_text_table(_: *mut c_void, table: *const c_char, sink: *mut EeDataSink) -> bool {
    // SAFETY: the content's NUL-terminated table name
    let table = unsafe { CStr::from_ptr(table) }.to_bytes();
    let (path, directory) = TEXT_PATH.lock().unwrap().clone();
    let Some(extension) = text_extension(table) else {
        return false;
    };
    if path == 0 {
        return false;
    }
    let mut name = directory;
    name.extend_from_slice(table);
    name.extend_from_slice(extension.as_bytes());
    let spec = find_file(path, &name);
    if spec == 0 {
        return false;
    }
    let text = read_whole_file(spec);
    h(O::DxDelete, spec, &[]);
    match text {
        Some(t) => {
            // SAFETY: the sink the content gave this call
            unsafe { ee_data_sink_write(sink, t.as_ptr(), t.len()) };
            true
        }
        None => false,
    }
}

/// The compiler content every table and the rules are read through.
fn content() -> *mut EeDataContent {
    let c = CONTENT.load(Ordering::Acquire);
    if !c.is_null() {
        return c;
    }
    // SAFETY: the reader is a plain function, callable for the process's life
    let made = unsafe { ee_data_content_new(read_text_table, std::ptr::null_mut()) };
    CONTENT.store(made, Ordering::Release);
    made
}

/// The current lookup's path and folder.
fn set_text_path(path: P, directory: &[u8]) {
    *TEXT_PATH.lock().unwrap() = (path, directory.to_vec());
}

/// `EE_DUMP_CONTENT`'s file (unset or empty: none).
fn dump_file() -> Option<std::ffi::OsString> {
    std::env::var_os("EE_DUMP_CONTENT").filter(|f| !f.is_empty())
}

fn append(file: &std::ffi::OsStr, bytes: &[u8]) {
    if let Ok(mut f) = std::fs::OpenOptions::new().append(true).create(true).open(file) {
        let _ = f.write_all(bytes);
    }
}

/// EE_DUMP_CONTENT: a served table's content dump appended (its line, or the dump's error as a string).
fn dump_content(table: &[u8], data: &[u8]) {
    let Some(file) = dump_file() else {
        return;
    };
    let name = c_text(table);
    let mut out = EeDataBytes::empty();
    let mut err = EeDataBytes::empty();
    // SAFETY: the name is NUL-terminated, the data readable, the outputs writable
    let ok = unsafe { ee_data_dump_table(name.as_ptr(), data.as_ptr(), data.len(), &mut out, &mut err) };
    let mut json = if ok {
        take(&mut out)
    } else {
        let mut j = b"\"".to_vec();
        j.extend_from_slice(table);
        j.extend_from_slice(b"\":\"");
        j.extend(take(&mut err));
        j.push(b'"');
        j
    };
    json.push(b'\n');
    append(&file, &json);
}

/// A db table that cannot be served: the message box and the verification failure; NULL and kErrorFailed.
fn failed(message: &[u8], out: *mut P) -> c_long {
    let c = c_text(message);
    h(O::DxMessage, 0, &[c"Empire Earth: db text table".as_ptr() as usize as i64, c.as_ptr() as usize as i64]);
    h(O::DxVerify, 0, &[c.as_ptr() as usize as i64]);
    // SAFETY: the caller's out pointer
    unsafe { *out = 0 };
    DX_FAILED
}

/// The folder and the lowercase stem of a db file name ("db\dbWorld.dat" -> "db\", "dbworld").
fn split_name(name: &[u8]) -> (Vec<u8>, Vec<u8>) {
    let slash = name.iter().rposition(|&c| c == b'\\' || c == b'/');
    let directory = slash.map_or(Vec::new(), |s| name[..=s].to_vec());
    let mut stem = name[directory.len()..].to_vec();
    if stem.len() > 4 && stem[stem.len() - 4..].eq_ignore_ascii_case(b".dat") {
        stem.truncate(stem.len() - 4);
    }
    stem.make_ascii_lowercase(); // the loaders spell some names in mixed case
    (directory, stem)
}

/// The db tables' file names in start-up order (startup.rs TABLES_1..3, index for index). They were EEDbObject.cpp's
/// `kDb*File` UStrings (`kDbStartingResourcesFile` .. `kDbPremadeCivsFile`): the keys the loaders ask
/// [`ee_gamecpp_db_find_file`] for, which serves the text form under the stem (the `.dat` and the case go).
pub(crate) const DB_TABLE_FILES: [&CStr; 26] = [
    c"db\\dbStartingResources.dat",
    c"db\\dbUnitBehavior.dat",
    c"db\\dbsounds.dat",
    c"db\\dbgraphics.dat",
    c"db\\dbgfxeffects.dat",
    c"db\\dbmusic.dat",
    c"db\\dbworld.dat",
    c"db\\dbfamily.dat",
    c"db\\dbButtons.dat",
    c"db\\dbtechtree.dat",
    c"db\\dbAnimals.dat",
    c"db\\dbcolortable.dat",
    c"db\\dbaiunittargeting.dat",
    c"db\\dbterrain.dat",
    c"db\\dbterrainType.dat",
    c"db\\dbcliffterrain.dat",
    c"db\\dbcivpowers.dat",
    c"db\\dbcivilization.dat",
    c"db\\dbweapontohit.dat",
    c"db\\dbcalamity.dat",
    c"db\\dbunitset.dat",
    c"db\\dbareaeffect.dat",
    c"db\\dbupgrade.dat",
    c"db\\dbcpbehavior.dat",
    c"db\\dbGameVariant.dat",
    c"db\\dbpremadecivs.dat",
];
/// `kDbEventsFile`
pub(crate) const DB_EVENTS_FILE: &CStr = c"db\\dbevents.dat";
/// `kDbEffectsFile`
pub(crate) const DB_EFFECTS_FILE: &CStr = c"db\\dbeffects.dat";
/// `kDbAmbientSoundsFile`
pub(crate) const DB_AMBIENT_SOUNDS_FILE: &CStr = c"db\\dbambientsounds.dat";

/// `EEDbFindFile(gDataPath, name, file)`: the table served into `file`; the error.
pub(crate) fn find_in_data_path(name: &CStr, file: &mut P) -> i64 {
    // SAFETY: the game's data path, set before the tables load (a read of the pointer's storage)
    let path = unsafe { *std::ptr::addr_of!(super::globals::gDataPath) };
    ee_gamecpp_db_find_file(path, name.as_ptr(), file as *mut P as *mut *mut c_void) as i64
}

/// EEDbFindFile: the table's text form compiled and served as a new memory file (kErrorOK), or the game stopped.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_find_file(path: *mut c_void, name: *const c_char, out: *mut *mut c_void) -> c_long {
    let path = path as P;
    let out = out as *mut P;
    // SAFETY: the UString's NUL-terminated text
    let name = unsafe { CStr::from_ptr(name) }.to_bytes().to_vec();
    let (directory, stem) = split_name(&name);
    let Some(extension) = text_extension(&stem) else {
        let mut m = name.clone();
        m.extend_from_slice(b": no text form of this db table");
        return failed(&m, out);
    };
    let mut file = directory.clone();
    file.extend_from_slice(&stem);
    file.extend_from_slice(extension.as_bytes());
    let spec = find_file(path, &file);
    if spec == 0 {
        let mut m = file.clone();
        m.extend_from_slice(b": the db table is missing");
        return failed(&m, out);
    }
    h(O::DxDelete, spec, &[]);
    set_text_path(path, &directory);
    let table = c_text(&stem);
    let mut data = EeDataBytes::empty();
    let mut err = EeDataBytes::empty();
    // SAFETY: the content is live, the name NUL-terminated, the outputs writable
    if !unsafe { ee_data_compile(content(), table.as_ptr(), &mut data, &mut err) } {
        let mut m = file.clone();
        m.extend_from_slice(b": ");
        m.extend(take(&mut err)); // a broken text table
        return failed(&m, out);
    }
    let data = take(&mut data);
    // the ids the computer players' scripts name (dbobjects, dbtechtree; the other tables: nothing)
    // SAFETY: the content is live, the name NUL-terminated
    unsafe { ee_sim::ai::names_db::ee_ai_names_capture(content() as *mut c_void, table.as_ptr()) };
    dump_content(&stem, &data);
    // SAFETY: the caller's out pointer
    unsafe { *out = hp(O::DxMemoryFile, 0, &[data.as_ptr() as usize as i64, data.len() as i64]) };
    DX_OK as c_long
}

fn rules() -> *mut EeDataRules {
    hp(O::DxRulesHandle, 0, &[]) as *mut EeDataRules
}

/// EELoadRules: the rules documents (next to the db tables, db\combat.yaml ...) read into the game's rules through
/// the same content; a broken one stops the game with its message.
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_load_rules(path: *mut c_void) {
    set_text_path(path as P, b"db\\");
    let mut err = EeDataBytes::empty();
    // SAFETY: the rules and the content are live, the error writable
    if unsafe { ee_data_rules_load(rules(), content(), &mut err) } {
        h(O::DxRulesRefresh, 0, &[]);
        return;
    }
    let mut m = b"db\\".to_vec();
    m.extend(take(&mut err));
    let c = c_text(&m);
    h(O::DxMessage, 0, &[c"Empire Earth: db rules".as_ptr() as usize as i64, c.as_ptr() as usize as i64]);
    h(O::DxVerify, 0, &[c.as_ptr() as usize as i64]);
}

/// EEDumpRules: EE_DUMP_CONTENT's rules dump (after the db tables are loaded).
#[no_mangle]
pub extern "C" fn ee_gamecpp_db_dump_rules() {
    let Some(file) = dump_file() else {
        return;
    };
    let mut out = EeDataBytes::empty();
    // SAFETY: the rules are live, the output writable
    unsafe { ee_data_rules_dump(rules(), &mut out) };
    append(&file, &take(&mut out));
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_db_name_splits_into_its_folder_and_lowercase_stem() {
        assert_eq!(split_name(b"db\\dbWorld.dat"), (b"db\\".to_vec(), b"dbworld".to_vec()));
        assert_eq!(split_name(b"dbmusic.DAT"), (Vec::new(), b"dbmusic".to_vec()));
        assert_eq!(split_name(b"a/b/x.dat"), (b"a/b/".to_vec(), b"x".to_vec()));
        assert_eq!(split_name(b"db\\.dat"), (b"db\\".to_vec(), b".dat".to_vec()));
    }

    #[test]
    fn the_text_forms_are_ee_datas() {
        assert_eq!(text_extension(b"dbworld"), Some(".csv"));
        assert_eq!(text_extension(b"combat"), Some(".yaml"));
        assert_eq!(text_extension(b"nothing"), None);
        assert_eq!(c_text(b"a\0b").as_bytes(), b"a");
    }

    #[test]
    fn taking_empty_bytes_is_empty() {
        assert!(take(&mut EeDataBytes::empty()).is_empty());
    }

    #[test]
    fn the_table_files_have_text_forms() {
        // every name is db\<stem>.dat with a stem ee_data knows a text form of; no name twice
        let mut seen = std::collections::BTreeSet::new();
        for name in DB_TABLE_FILES.iter().chain([DB_EVENTS_FILE, DB_EFFECTS_FILE, DB_AMBIENT_SOUNDS_FILE].iter()) {
            let (directory, stem) = split_name(name.to_bytes());
            assert_eq!(directory, b"db\\");
            assert!(text_extension(&stem).is_some(), "{name:?}");
            assert!(seen.insert(stem));
        }
        assert_eq!(DB_TABLE_FILES[0].to_bytes(), b"db\\dbStartingResources.dat");
        assert_eq!(DB_TABLE_FILES[25].to_bytes(), b"db\\dbpremadecivs.dat");
    }
}
