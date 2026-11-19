//! The computer player's engine per computer seat and the AI manifest (`ee/EECPEngine.h`; docs/AI_LUA_PLAN.md §5 S0,
//! "AI manifest"), whole in Rust since the game's leaves round 1 (EECPEngine.cpp): the manifest's files (EE_AI_SCRIPTS's
//! folder, TEST ONLY, before Data\ai), its read once per process (the document's checks are ee_sim ai manifest.rs),
//! the lobby's choices, EE_AI_ENGINE's overrides (TEST ONLY), the resolve of random (ee_sim ai seat.rs), the AI
//! folder's hash and the engine's log lines. The seats' own state (EEComputerPlayer::mEngine, the next state's AI id)
//! stays with the computer player (EECPNext.cpp).

use std::ffi::{c_char, CStr};
use std::sync::OnceLock;

use ee_sim::ai::manifest::{self, Entry};
use ee_sim::ai::seat;

/// The lobby's choices (`EECPEngineChoice`).
const CHOICE_CLASSIC: u64 = 0;
const CHOICE_RANDOM: u64 = 1;
const CHOICE_FIRST_AI: u64 = 2;
/// The manifest's entries past this are not offered to the resolve (`kEECPMaxAIs`).
const MAX_AIS: usize = 64;

/// An AI of the manifest as C++ reads it (`EECPAIEntry`): the texts point into the process's manifest.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeCpAiEntry {
    /// `[a-z0-9_]+`, unique
    pub id: *const u8,
    pub id_len: usize,
    /// the lobby's text (UTF-8)
    pub name: *const u8,
    pub name_len: usize,
    /// the Lua module (dots are folders under Data/ai)
    pub script: *const u8,
    pub script_len: usize,
    /// the lobby's Random may pick it
    pub random: bool,
    /// "sees_all"
    pub sees_all: bool,
    /// "fills_garrison"
    pub fills_garrison: bool,
    /// "classic": the lobby's Classic plays it (the first so marked)
    pub classic: bool,
}

// SAFETY: the entries point into the manifest, read once and never changed or freed
unsafe impl Send for EeCpAiEntry {}
// SAFETY: as above
unsafe impl Sync for EeCpAiEntry {}

struct Ais {
    entries: Vec<Entry>,
    c: Vec<EeCpAiEntry>,
}

// ---- the platform: the game's file paths (PlatFOpen), the folder walk (PlatFind*), the C library's environment

/// cbindgen:ignore
mod sys {
    use std::ffi::{c_char, c_void};

    #[repr(C)]
    pub(super) struct PlatFindData {
        pub(super) name: [c_char; 260],
        pub(super) attributes: u32,
    }

    pub(super) const PLAT_ATTR_DIRECTORY: u32 = 0x10;

    extern "C" {
        pub(super) fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub(super) fn PlatFindFirst(pattern: *const c_char, out: *mut PlatFindData) -> *mut c_void;
        pub(super) fn PlatFindNext(find: *mut c_void, out: *mut PlatFindData) -> i32;
        pub(super) fn PlatFindClose(find: *mut c_void);
        pub(super) fn fread(buffer: *mut c_void, size: usize, count: usize, stream: *mut c_void) -> usize;
        pub(super) fn ferror(stream: *mut c_void) -> i32;
        pub(super) fn fclose(stream: *mut c_void) -> i32;
        pub(super) fn getenv(name: *const c_char) -> *const c_char;
    }

    #[cfg(not(test))]
    extern "C" {
        /// `ESDebugOut("%s", text)` (EECPNext.cpp): the Error System's filters and the debugger.
        pub(super) fn ee_cp_engine_debug_out(text: *const c_char);
    }
}
#[cfg(not(test))]
use sys::ee_cp_engine_debug_out;
use sys::PLAT_ATTR_DIRECTORY;
use sys::{fclose, ferror, fread, getenv, PlatFOpen, PlatFindClose, PlatFindData, PlatFindFirst, PlatFindNext};

/// The game's path form in the system's (`EECPAINative`: `\` as `/` off Windows), NUL ended.
fn native(path: &[u8]) -> Vec<u8> {
    let mut p: Vec<u8> = path.iter().map(|&c| if cfg!(windows) || c != b'\\' { c } else { b'/' }).collect();
    p.push(0);
    p
}

/// `EECPAIReadFile`: the whole file in binary mode (None: it does not open or a read fails).
fn read_file(path: &[u8]) -> Option<Vec<u8>> {
    let p = native(path);
    // SAFETY: NUL-ended path and mode
    let f = unsafe { PlatFOpen(p.as_ptr().cast(), c"rb".as_ptr()) };
    if f.is_null() {
        return None;
    }
    let mut out = Vec::new();
    let mut buffer = [0u8; 4096];
    loop {
        // SAFETY: the open stream, the buffer's room
        let n = unsafe { fread(buffer.as_mut_ptr().cast(), 1, buffer.len(), f) };
        if n == 0 {
            break;
        }
        out.extend_from_slice(&buffer[..n]);
    }
    // SAFETY: the open stream, closed once
    let ok = unsafe { ferror(f) } == 0;
    // SAFETY: as above
    unsafe { fclose(f) };
    ok.then_some(out)
}

/// `EECPAIFileExists`
fn file_exists(path: &[u8]) -> bool {
    let p = native(path);
    // SAFETY: NUL-ended path and mode
    let f = unsafe { PlatFOpen(p.as_ptr().cast(), c"rb".as_ptr()) };
    if f.is_null() {
        return false;
    }
    // SAFETY: the open stream
    unsafe { fclose(f) };
    true
}

/// The C library's value of an environment variable (as the C++'s `getenv`: EE-SIM sets some with `_putenv_s`).
fn env(name: &CStr) -> Option<Vec<u8>> {
    // SAFETY: a NUL-ended name; the value is copied at once
    let v = unsafe { getenv(name.as_ptr()) };
    if v.is_null() {
        return None;
    }
    // SAFETY: the C library's NUL-ended value
    Some(unsafe { CStr::from_ptr(v) }.to_bytes().to_vec())
}

/// EE_AI_SCRIPTS's folder with a trailing separator (empty: unset).
fn override_folder() -> &'static [u8] {
    static DIR: OnceLock<Vec<u8>> = OnceLock::new();
    DIR.get_or_init(|| match env(c"EE_AI_SCRIPTS") {
        Some(mut d) if !d.is_empty() => {
            if !matches!(d.last(), Some(b'\\' | b'/')) {
                d.push(b'\\');
            }
            d
        }
        _ => Vec::new(),
    })
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

// ---- the log

/// `EECPEngineLog`'s line of an already formatted text (`%s`: up to its first NUL, cut at 899 bytes as the C++'s
/// 900-byte buffer): "EE-AI engine <text>" to the debug output, the game log when one is open, EE-SIM's stderr.
pub fn log(text: &[u8]) {
    let text = &text[..text.iter().position(|&c| c == 0).unwrap_or(text.len())];
    let text = &text[..text.len().min(899)];
    let mut line = cat(&[b"EE-AI engine ", text, b"\n"]);
    line.push(0);
    #[cfg(not(test))]
    // SAFETY: a NUL-ended line
    unsafe {
        ee_cp_engine_debug_out(line.as_ptr().cast())
    };
    if crate::game_log::ee_game_log_on() {
        // SAFETY: a NUL-ended line
        unsafe { crate::game_log::ee_game_log_line(line.as_ptr().cast()) };
    }
    line.pop();
    if ee_base::plat_core::sim::gPlatSimMode.get() != 0 {
        use std::io::Write;
        let mut e = std::io::stderr().lock();
        let _ = e.write_all(b"SIM ");
        let _ = e.write_all(&line);
        let _ = e.flush();
    }
}

/// `EECPEngineLog` of a NUL-ended text (C++ formats it first).
///
/// # Safety
/// `text` is NUL ended.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_engine_log(text: *const c_char) {
    if !text.is_null() {
        // SAFETY: the caller's contract
        log(unsafe { CStr::from_ptr(text) }.to_bytes());
    }
}

// ---- the manifest

/// The relative path of a script module (`.` as folders, `.lua`).
fn script_path(module: &[u8]) -> Vec<u8> {
    let mut p: Vec<u8> = module.iter().map(|&c| if c == b'.' { b'\\' } else { c }).collect();
    p.extend_from_slice(b".lua");
    p
}

/// `EECPAIScriptExists`: the script's file is in EE_AI_SCRIPTS's folder or Data\ai.
fn script_exists(module: &[u8]) -> bool {
    let rel = script_path(module);
    let o = override_folder();
    (!o.is_empty() && file_exists(&cat(&[o, &rel]))) || file_exists(&cat(&[b"Data\\ai\\", &rel]))
}

/// The summary line's list of ids ("none" when empty; "(not random)" after an AI the lobby's Random skips).
pub fn id_list(entries: &[Entry]) -> Vec<u8> {
    let mut ids = Vec::new();
    for a in entries {
        if !ids.is_empty() {
            ids.push(b',');
        }
        ids.extend_from_slice(&a.id);
        if !a.random {
            ids.extend_from_slice(b"(not random)");
        }
    }
    if ids.is_empty() {
        ids.extend_from_slice(b"none");
    }
    ids
}

fn read_manifest() -> Vec<Entry> {
    let o = override_folder();
    let (path, text) = match (!o.is_empty()).then(|| read_file(&cat(&[o, b"ais.json"]))).flatten() {
        Some(t) => (cat(&[o, b"ais.json"]), t),
        None => match read_file(b"Data\\ai\\ais.json") {
            Some(t) => (b"Data\\ai\\ais.json".to_vec(), t),
            None => {
                log(b"no Data/ai/ais.json: the lobby offers only classic and random");
                return Vec::new();
            }
        },
    };
    let m = manifest::read(&text, &path, &mut |module| script_exists(module));
    for l in &m.log {
        log(l);
    }
    if !m.listed {
        return m.entries; // (the document is not a manifest: logged)
    }
    let count = m.entries.len().to_string();
    let hash = format!("{:08x}", folder_hash());
    let line = cat(&[
        cs(&path),
        b": ",
        count.as_bytes(),
        b" AIs (",
        cs(&id_list(&m.entries)),
        b") folder hash ",
        hash.as_bytes(),
    ]);
    log(&line);
    m.entries
}

/// A text as `%s` prints it: up to its first NUL.
fn cs(text: &[u8]) -> &[u8] {
    &text[..text.iter().position(|&c| c == 0).unwrap_or(text.len())]
}

fn ais() -> &'static Ais {
    static AIS: OnceLock<Ais> = OnceLock::new();
    AIS.get_or_init(|| {
        let entries = read_manifest();
        let c = entries
            .iter()
            .map(|e| EeCpAiEntry {
                id: e.id.as_ptr(),
                id_len: e.id.len(),
                name: e.name.as_ptr(),
                name_len: e.name.len(),
                script: e.script.as_ptr(),
                script_len: e.script.len(),
                random: e.random,
                sees_all: e.sees_all,
                fills_garrison: e.fills_garrison,
                classic: e.classic,
            })
            .collect();
        Ais { entries, c }
    })
}

/// `EECPClassicAI`: the manifest's first AI marked "classic".
pub fn classic() -> Option<usize> {
    ais().entries.iter().position(|a| a.classic)
}

/// `EECPAIByChoice`: choice 0 the "classic" AI, 2 + k the entry k; None for random or out of range.
pub fn by_choice(choice: u64) -> Option<usize> {
    if choice == CHOICE_CLASSIC {
        return classic();
    }
    let k = choice.checked_sub(CHOICE_FIRST_AI)? as usize;
    (k < ais().entries.len()).then_some(k)
}

/// `EECPAIByID`
pub fn by_id(id: &[u8]) -> Option<usize> {
    ais().entries.iter().position(|a| a.id == id)
}

/// `EECPChoiceByName`: classic and random need no manifest (it is read at its first use).
pub fn choice_by_name(name: &[u8]) -> i64 {
    let choice = seat::choice_by_name(name, &[]);
    if choice == CHOICE_CLASSIC as i64 || choice == CHOICE_RANDOM as i64 {
        return choice;
    }
    let ids: Vec<&[u8]> = ais().entries.iter().map(|a| a.id.as_slice()).collect();
    seat::choice_by_name(name, &ids)
}

fn entry_ptr(k: Option<usize>) -> *const EeCpAiEntry {
    k.map_or(std::ptr::null(), |k| &ais().c[k] as *const EeCpAiEntry)
}

/// `EECPEngineChoiceCount`: 2 + the manifest's entries.
#[no_mangle]
pub extern "C" fn ee_cp_engine_choice_count() -> u64 {
    CHOICE_FIRST_AI + ais().entries.len() as u64
}

/// The manifest's entries (EE-SIM's list of ids).
#[no_mangle]
pub extern "C" fn ee_cp_engine_ai_count() -> usize {
    ais().entries.len()
}

/// The manifest's entry `k` (NULL past them).
#[no_mangle]
pub extern "C" fn ee_cp_engine_ai_at(k: usize) -> *const EeCpAiEntry {
    entry_ptr((k < ais().entries.len()).then_some(k))
}

/// `EECPAIByChoice` (NULL: random or out of range; choice 0: the "classic" AI, NULL when the manifest marks none).
#[no_mangle]
pub extern "C" fn ee_cp_engine_by_choice(choice: u64) -> *const EeCpAiEntry {
    entry_ptr(by_choice(choice))
}

/// `EECPClassicAI` (NULL: none).
#[no_mangle]
pub extern "C" fn ee_cp_engine_classic() -> *const EeCpAiEntry {
    entry_ptr(classic())
}

/// `EECPAIByID` (NULL: not in the manifest).
///
/// # Safety
/// `id` points to `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_engine_by_id(id: *const u8, len: usize) -> *const EeCpAiEntry {
    let id = if id.is_null() || len == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(id, len) }
    };
    entry_ptr(by_id(id))
}

/// `EECPChoiceByName` (-1: none).
///
/// # Safety
/// `name` is NUL ended.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_engine_choice_by_name(name: *const c_char) -> i64 {
    // SAFETY: the caller's contract
    choice_by_name(unsafe { CStr::from_ptr(name) }.to_bytes())
}

// ---- the folder's hash

/// FNV-1a 32 of `data` from `h`; `skip_cr` drops carriage returns (the same scripts written on Windows and elsewhere
/// hash alike).
pub fn hash_bytes(mut h: u32, data: &[u8], skip_cr: bool) -> u32 {
    for &c in data {
        if skip_cr && c == b'\r' {
            continue;
        }
        h ^= u32::from(c);
        h = h.wrapping_mul(16_777_619);
    }
    h
}

/// Every ais.json (the root's) and *.lua under `folder` (game form, trailing backslash), relative paths with `\`.
fn list_files(folder: &[u8], relative: &[u8], depth: u32, out: &mut Vec<Vec<u8>>) {
    if depth > 8 {
        return;
    }
    let pattern = native(&cat(&[folder, relative, b"*"]));
    let mut data = PlatFindData { name: [0; 260], attributes: 0 };
    // SAFETY: a NUL-ended pattern, a writable entry
    let find = unsafe { PlatFindFirst(pattern.as_ptr().cast(), &mut data) };
    if find.is_null() {
        return;
    }
    loop {
        // SAFETY: the platform's NUL-ended name
        let name = unsafe { CStr::from_ptr(data.name.as_ptr()) }.to_bytes().to_vec();
        if name != b"." && name != b".." {
            if data.attributes & PLAT_ATTR_DIRECTORY != 0 {
                list_files(folder, &cat(&[relative, &name, b"\\"]), depth + 1, out);
            } else {
                let lower = name.to_ascii_lowercase();
                let lua = lower.len() > 4 && lower.ends_with(b".lua");
                if lua || (relative.is_empty() && lower == b"ais.json") {
                    out.push(cat(&[relative, &name]));
                }
            }
        }
        // SAFETY: the open search
        if unsafe { PlatFindNext(find, &mut data) } != 0 {
            break;
        }
    }
    // SAFETY: the open search, closed once
    unsafe { PlatFindClose(find) };
}

/// The files' hash order: (the relative path with `/` and in lower case, the path), sorted.
pub fn hash_order(files: Vec<Vec<u8>>) -> Vec<(Vec<u8>, Vec<u8>)> {
    let mut keyed: Vec<(Vec<u8>, Vec<u8>)> = files
        .into_iter()
        .map(|f| (f.iter().map(|&c| if c == b'\\' { b'/' } else { c.to_ascii_lowercase() }).collect(), f))
        .collect();
    keyed.sort();
    keyed
}

fn hash_folder(folder: &[u8], mut h: u32) -> u32 {
    let mut files = Vec::new();
    list_files(folder, b"", 0, &mut files);
    for (key, path) in hash_order(files) {
        let Some(text) = read_file(&cat(&[folder, &path])) else { continue };
        let mut k = key.clone();
        k.push(0);
        h = hash_bytes(h, &k, false);
        h = hash_bytes(h, &text, true);
    }
    h
}

/// `EECPAIFolderHash`: ais.json and every .lua of Data\ai, then of EE_AI_SCRIPTS's folder (part of the checksum a
/// joiner must match), once.
pub fn folder_hash() -> u32 {
    static HASH: OnceLock<u32> = OnceLock::new();
    *HASH.get_or_init(|| {
        let h = hash_folder(b"Data\\ai\\", 2_166_136_261);
        let o = override_folder();
        if o.is_empty() {
            h
        } else {
            hash_folder(o, h)
        }
    })
}

/// `EECPAIFolderHash`
#[no_mangle]
pub extern "C" fn ee_cp_engine_folder_hash() -> u32 {
    folder_hash()
}

// ---- the resolve

/// EE_AI_ENGINE (TEST ONLY): the choices in turn by player id from player 1 (empty: unset); an unknown name is logged
/// and skipped.
fn overrides() -> &'static [i64] {
    static CHOICES: OnceLock<Vec<i64>> = OnceLock::new();
    CHOICES.get_or_init(|| {
        let mut out = Vec::new();
        let Some(value) = env(c"EE_AI_ENGINE").filter(|v| !v.is_empty()) else { return out };
        for name in value.split(|&c| c == b',') {
            let choice = choice_by_name(name);
            if choice >= 0 {
                out.push(choice);
            } else {
                log(&cat(&[b"EE_AI_ENGINE: unknown engine '", name, b"' ignored"]));
            }
        }
        out
    })
}

/// `EECPEngineResolve`: the choice a computer player of lobby slot `slot` plays with (seat.rs).
#[no_mangle]
pub extern "C" fn ee_cp_engine_resolve(choice: u64, seed: u64, slot: u64, player_id: u64) -> u64 {
    let o = overrides();
    let random: Vec<bool> = ais().entries.iter().take(MAX_AIS).map(|a| a.random).collect();
    seat::resolve(choice, seed, slot, player_id, o, &random)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry(id: &str, random: bool) -> Entry {
        Entry { id: id.as_bytes().to_vec(), random, ..Default::default() }
    }

    #[test]
    fn the_summary_lists_ids_and_marks_the_unrandom() {
        assert_eq!(id_list(&[]), b"none");
        assert_eq!(id_list(&[entry("boom", true), entry("classic_lua", false)]), b"boom,classic_lua(not random)");
    }

    #[test]
    fn scripts_are_lua_files_under_folders() {
        assert_eq!(script_path(b"lib.boom"), b"lib\\boom.lua");
        assert_eq!(native(b"Data\\ai\\x")[..9], if cfg!(windows) { *b"Data\\ai\\x" } else { *b"Data/ai/x" });
    }

    #[test]
    fn the_hash_is_fnv1a_without_carriage_returns() {
        assert_eq!(hash_bytes(2_166_136_261, b"", false), 2_166_136_261);
        assert_eq!(hash_bytes(2_166_136_261, b"a", false), 0xe40c_292c);
        assert_eq!(hash_bytes(2_166_136_261, b"a\r", true), 0xe40c_292c);
        assert_ne!(hash_bytes(2_166_136_261, b"a\r", false), 0xe40c_292c);
    }

    #[test]
    fn files_hash_in_lower_case_slash_order() {
        let order = hash_order(vec![b"b.lua".to_vec(), b"Lib\\A.lua".to_vec(), b"ais.json".to_vec()]);
        let keys: Vec<&[u8]> = order.iter().map(|(k, _)| k.as_slice()).collect();
        assert_eq!(keys, [&b"ais.json"[..], b"b.lua", b"lib/a.lua"]);
        assert_eq!(order[2].1, b"Lib\\A.lua");
    }

    #[test]
    fn classic_and_random_need_no_manifest() {
        assert_eq!(choice_by_name(b"classic"), 0);
        assert_eq!(choice_by_name(b"random"), 1);
    }
}
