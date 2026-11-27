//! The game's entry wiring that was left in Empire Earth.cpp and EECommandLine.cpp (game core round 9): the data
//! checksum of a directory's files (EEGetFileChecksum), the network's start and the game version's checksum suffix
//! (EEInitializeNetwork), and the command line's commands (EEParseCommandLine: hostgame, joinbyip, update_saved_games,
//! loadgame, balancer; the client and the main menu without one). Each C++ call or member is a case of Empire
//! Earth.cpp's `EEStartupOp` ([`super::EeGameCpp`]); the C++ functions forward here.
//!
//! The checksum runs at every start (EE-SIM too: the oracle starts the network); the command line's no-command path
//! at every game's start (the tour, gates.sh); its commands only from a command line no gate passes.

#![allow(clippy::unnecessary_cast, non_snake_case)]

use std::ffi::{c_char, c_long, c_ulong, CStr, CString};

use ee_base::ustring::{nested_tokenize, tokenize};

use super::global_objects as gobj;
use super::{h, hp, pa, ptr, Owned, O, P};

/// `kErrorOK` / `keseOK`.
const ENTRY_OK: i64 = 0;
/// The checksum's read size.
const ENTRY_CHUNK: usize = 0x800;
/// EEInitializeNetwork's checksum directories under the start-up directory (were Empire Earth.cpp's
/// `kNetworkFolders`).
const NET_EXE: &CStr = c"";
const NET_DB: &CStr = c"Data\\Db\\";
const NET_TAI: &CStr = c"Data\\Unit AI Scripts\\";
const NET_MODELS: &CStr = c"Data\\Models\\";
/// The file name the checksum skips, encrypted (was Empire Earth.cpp's `gU530BE0`, 0x530be0).
const ENTRY_EXCLUDED: &[u8] = b"B7B226DDB7B226DDBCD9344624C93DE6C4A90DD4B7B20375";

/// `spec->SetDirectory(gStartupDirectory + folder)`.
fn set_network_folder(spec: P, folder: &CStr) {
    h(O::UsgSetDirectory, spec, &[pa(folder.as_ptr() as P)]);
}

/// `UEDecryptUString(gU530BE0, name)`: the skipped file's name (the text up to its first NUL, as the UString sink
/// kept it).
pub fn entry_excluded_name() -> CString {
    unsafe extern "C" fn sink(context: *mut std::ffi::c_void, bytes: *const u8, length: usize) {
        // SAFETY: the decryption's output, valid for the call; the context is the Vec below
        unsafe { (*(context as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
    }
    let mut text = Vec::new();
    // SAFETY: a constant input; the sink appends to `text`
    unsafe {
        ee_base::codec::ffi::ee_ue_decrypt_ustring(
            ENTRY_EXCLUDED.as_ptr(),
            ENTRY_EXCLUDED.len(),
            sink,
            &mut text as *mut Vec<u8> as *mut std::ffi::c_void,
        )
    };
    let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    CString::new(&text[..end]).unwrap_or_default()
}
/// The join-or-host action's kinds (EEAJoinOrHostAGame).
const ENTRY_HOST: i64 = 0;
const ENTRY_JOIN_BY_IP: i64 = 2;

/// A C string the C++ reads for the call's duration.
fn cstr(s: &[u8]) -> CString {
    CString::new(s.iter().copied().filter(|&b| b != 0).collect::<Vec<u8>>()).unwrap_or_default()
}

/// The checksum's sum of a file's bytes (wrapping, as the C++'s unsigned long).
pub fn entry_byte_sum(sum: c_ulong, bytes: &[u8]) -> c_ulong {
    bytes.iter().fold(sum, |s, &b| s.wrapping_add(b as c_ulong))
}

/// EEGetFileChecksum: the checksum over the directory's files matching the pattern, in file name order (one encrypted
/// name skipped): each file's bytes, or with `use_file_times` its write time's high word plus its size. A file that
/// cannot be opened is skipped; a read that returns nothing ends that file (the port's fix of two endless loops).
///
/// # Safety
/// `directory` is a live FSFileSpec, `pattern` a live UString.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_entry_file_checksum(
    directory: *mut std::ffi::c_void,
    pattern: *const std::ffi::c_void,
    use_file_times: c_long,
) -> c_ulong {
    let directory = directory as P;
    let file = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let mut checksum: c_ulong = 0;
    if h(O::EntrySpecFirstFile, directory, &[pa(pattern as P), pa(file.0)]) != ENTRY_OK {
        return checksum;
    }
    let excluded = entry_excluded_name();
    let files = Owned::new(O::EntrySpecVecNew, O::EntrySpecVecDelete);
    loop {
        if h(O::EntrySpecFileIsNot, file.0, &[pa(excluded.as_ptr() as P)]) != 0 {
            h(O::EntrySpecVecPush, files.0, &[pa(file.0)]);
        }
        if h(O::EntrySpecNextFile, directory, &[pa(file.0)]) != ENTRY_OK {
            break;
        }
    }
    h(O::EntrySpecVecSort, files.0, &[]);
    let mut buffer = vec![0u8; ENTRY_CHUNK];
    let count = h(O::EntrySpecVecSize, files.0, &[]);
    for i in 0..count {
        h(O::EntrySpecVecGet, files.0, &[i, pa(file.0)]);
        if (use_file_times as u8) != 0 {
            checksum = checksum.wrapping_add(time_and_size(file.0));
            continue;
        }
        if h(O::DxOpen, file.0, &[]) != ENTRY_OK {
            continue;
        }
        let mut length: c_ulong = 0;
        h(O::DxLength, file.0, &[ptr(&mut length)]);
        while length > 0 {
            let amount = length.min(ENTRY_CHUNK as c_ulong);
            let mut read: c_ulong = 0;
            h(O::DtFileReadRecord, file.0, &[pa(buffer.as_mut_ptr() as P), amount as i64, ptr(&mut read)]);
            if read == 0 || read > amount {
                break;
            }
            checksum = entry_byte_sum(checksum, &buffer[..read as usize]);
            length -= read;
        }
        h(O::FileClose, file.0, &[]);
    }
    checksum
}

/// cbindgen:ignore
#[cfg(not(test))]
mod file_sys {
    use std::ffi::c_char;
    extern "C" {
        // platform/PlatFile.h (ee_base plat_sys)
        pub fn PlatFileOpen(path: *const c_char, access: u32) -> *mut std::ffi::c_void;
        pub fn PlatFileGetWriteTime(file: *mut std::ffi::c_void, time: *mut u64) -> i32;
        pub fn PlatFileGetSize(file: *mut std::ffi::c_void) -> u32;
        pub fn PlatFileClose(file: *mut std::ffi::c_void) -> i32;
    }
}
#[cfg(not(test))]
use file_sys::*;
#[cfg(test)]
unsafe fn PlatFileOpen(_: *const c_char, _: u32) -> *mut std::ffi::c_void {
    panic!("no platform in the tests")
}
#[cfg(test)]
unsafe fn PlatFileGetWriteTime(_: *mut std::ffi::c_void, _: *mut u64) -> i32 {
    panic!("no platform in the tests")
}
#[cfg(test)]
unsafe fn PlatFileGetSize(_: *mut std::ffi::c_void) -> u32 {
    panic!("no platform in the tests")
}
#[cfg(test)]
unsafe fn PlatFileClose(_: *mut std::ffi::c_void) -> i32 {
    panic!("no platform in the tests")
}

/// A file's write time's high word plus its size (GetFileAttributesEx's ftLastWriteTime.dwHighDateTime, nFileSizeHigh
/// and nFileSizeLow; files stay below 4 GB); 0 when it cannot be opened.
fn time_and_size(file: P) -> c_ulong {
    let path = Owned::new(O::UStrNew, O::UStrFree);
    h(O::EntrySpecFullPath, file, &[pa(path.0)]);
    let text = hp(O::EntryUStrData, path.0, &[]) as *const c_char;
    // SAFETY: the path's terminated text (the UString lives to the end of the call); the platform's file
    unsafe {
        let native = PlatFileOpen(text, 1);
        if native.is_null() {
            return 0;
        }
        let mut time = 0u64;
        PlatFileGetWriteTime(native, &mut time);
        let sum = ((time >> 32) as c_ulong).wrapping_add(PlatFileGetSize(native) as c_ulong);
        PlatFileClose(native);
        sum
    }
}

/// `sscanf(text, "%ld", &value)`: the value when the text starts (after white space) with a number, else `previous`.
pub fn entry_scan_long(text: &[u8], previous: c_long) -> c_long {
    let mut i = 0;
    while i < text.len() && (text[i] == b' ' || (b'\t'..=b'\r').contains(&text[i])) {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let start = i;
    let mut value: c_long = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = value.wrapping_mul(10).wrapping_add((text[i] - b'0') as c_long);
        i += 1;
    }
    if i == start {
        return previous;
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

/// The game version's suffix: the checksum's low 13 bits (`".%04ld"`).
pub fn entry_version_suffix(checksum: u32) -> String {
    format!(".{:04}", checksum & 0x1fff)
}

/// The checksum of the start-up directory's files of a pattern (one EEGetFileChecksum).
fn folder_checksum(file: P, folder: &CStr, pattern: &[u8]) -> u32 {
    set_network_folder(file, folder);
    let pattern = cstr(pattern);
    let text = Owned::new(O::UStrNew, O::UStrFree);
    h(O::EntryUStrSet, text.0, &[pa(pattern.as_ptr() as P)]);
    // SAFETY: a live FSFileSpec and UString of this call
    unsafe { ee_gamecpp_entry_file_checksum(file as *mut _, text.0 as *const _, 0) as u32 }
}

/// EEInitializeNetwork: the network engine started and its message types registered, then the checksum of the exe,
/// the db tables, the unit AI scripts, the models, the computer players' AIs and the archives' timestamps, its low 13
/// bits appended to the game version.
#[no_mangle]
pub extern "C" fn ee_gamecpp_entry_initialize_network() {
    let file = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    let directory = Owned::new(O::FileSpecNew, O::FxFileSpecDelete);
    set_network_folder(directory.0, NET_EXE);
    let result = h(O::EntryNeInitialize, directory.0, &[]);
    h(O::Verify, 0, &[(result == ENTRY_OK) as i64, 1578]);
    h(O::EntryRegisterNeMessages, 0, &[]);
    h(O::EntryRegisterEeMessages, 0, &[]);
    let result = i64::from(ee_base::util::system::ee_util_register_shutdown(empty_shutdown));
    h(O::Verify, 0, &[(result == ENTRY_OK) as i64, 1586]);
    h(O::FileSpecSetVolumeStartup, file.0, &[]);
    ee_base::time::ee_time_get();
    // the game's code is all in its exe (the original summed the engine, Dinkum and TCP Protocol DLLs)
    let mut exe = [0u8; 4096];
    // SAFETY: the buffer and its size
    let named = unsafe { ee_base::plat_core::path::PlatPathGetExecutableName(exe.as_mut_ptr().cast(), 260) };
    let exe_name: &[u8] = if named != 0 {
        // SAFETY: the platform wrote a terminated name
        unsafe { CStr::from_ptr(exe.as_ptr() as *const c_char) }.to_bytes()
    } else {
        b""
    };
    let mut checksum = folder_checksum(file.0, NET_EXE, exe_name);
    // the db tables, which the game reads only as text (EEDbFindFile; the original summed the .dat files here)
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_DB, b"*.csv"));
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_DB, b"*.yaml"));
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_TAI, b"*.tai"));
    // the models (glTF only since D9: GEFindModelFile)
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_MODELS, b"*.glb"));
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_MODELS, b"*.gltf"));
    checksum = checksum.wrapping_add(folder_checksum(file.0, NET_MODELS, b"*.bin"));
    // port: the computer players' AIs (Data\ai: the manifest ais.json and every Lua script), so a joiner whose AI
    // folder differs from the host's is refused like any other data mismatch (kConnectionBadChecksum)
    checksum = checksum.wrapping_add(crate::cp_engine::ee_cp_engine_folder_hash());
    let tag = Owned::new(O::StrNew, O::StrFree);
    let mut timestamp: c_long = 0;
    let archives = h(O::EntryArchiveCount, 0, &[]);
    for i in 0..archives {
        if h(O::EntryArchiveTimestamp, tag.0, &[i]) != 0 {
            // SAFETY: the tag's text, read before the next C++ call
            let text = unsafe { CStr::from_ptr(hp(O::EntryStrData, tag.0, &[]) as *const c_char) }.to_bytes();
            timestamp = entry_scan_long(text, timestamp);
            checksum = checksum.wrapping_add(timestamp as u32);
        }
    }
    ee_base::time::ee_time_get();
    crate::gamecpp::globals::set!(gBinaryChecksum, checksum);
    h(O::EntryVersionTrim, 0, &[]);
    let suffix = cstr(entry_version_suffix(checksum).as_bytes());
    h(O::EntryVersionAppend, 0, &[pa(suffix.as_ptr() as P)]);
}

/// A command line's command: the first argument token (lowered) starts with it (EEIsCommand).
pub fn entry_is_command(argument: &[u8], command: &[u8]) -> bool {
    argument.len() >= command.len() && argument[..command.len()].eq_ignore_ascii_case(command)
}

/// The tokens of `text` between `separators` as byte strings (UString::Tokenize; NestedTokenize with `"` quotes).
pub fn entry_tokens(text: &[u8], separators: &[u8], quoted: bool) -> Vec<Vec<u8>> {
    let pairs = if quoted { nested_tokenize(text, separators, b'"', b'"') } else { tokenize(text, separators) };
    pairs
        .into_iter()
        .map(|(first, last)| {
            let (first, end) = (first as usize, (last as usize).wrapping_add(1));
            if first < end && end <= text.len() {
                text[first..end].to_vec()
            } else {
                Vec::new()
            }
        })
        .collect()
}

/// What the command line asks (EEParseCommandLine's matches, in order).
#[derive(Debug, PartialEq, Eq)]
pub enum EntryCommand {
    Host(Vec<u8>, Vec<u8>),
    JoinByIp(Vec<u8>, Vec<u8>, i32),
    UpdateSavedGames,
    LoadGame(Vec<u8>),
    Balancer,
}

/// C's `atoi`.
fn atoi(text: &[u8]) -> i32 {
    entry_scan_long(text, 0) as i32
}

/// The command line's commands: the arguments split at `-` (quoted ones kept whole), each matched against the
/// commands; a command whose quoted arguments do not count right is ignored.
pub fn entry_commands(line: &[u8]) -> Vec<EntryCommand> {
    let mut out = Vec::new();
    for argument in entry_tokens(line, b"-", true) {
        let tokens = || entry_tokens(&argument, b"\"", false);
        if entry_is_command(&argument, b"hostgame") {
            let t = tokens();
            if t.len() == 4 {
                out.push(EntryCommand::Host(t[1].clone(), t[3].clone()));
            }
        } else if entry_is_command(&argument, b"joinbyip") {
            let t = tokens();
            if t.len() == 6 {
                out.push(EntryCommand::JoinByIp(t[1].clone(), t[3].clone(), atoi(&t[5])));
            }
        } else if entry_is_command(&argument, b"update_saved_games") {
            out.push(EntryCommand::UpdateSavedGames);
        } else if entry_is_command(&argument, b"loadgame") {
            let t = tokens();
            if t.len() == 2 {
                out.push(EntryCommand::LoadGame(t[1].clone()));
            }
        } else if entry_is_command(&argument, b"balancer") {
            let t = tokens();
            if t.len() == 2 {
                out.push(EntryCommand::Balancer);
            }
        }
    }
    out
}

/// A new client as the communications, its threads started (the command line's client).
fn start_client(tries: Option<i32>) {
    h(O::EntryNewClient, 0, &[]);
    if let Some(tries) = tries {
        h(O::EntryClientSetTries, 0, &[tries as i64]);
    }
    h(O::EntryClientStartThreads, 0, &[]);
}

/// "hostgame" / "joinbyip": a client, the player's name, the main menu and its multiplayer screen, then the join or
/// host action posted.
fn host_or_join(kind: i64, player: &[u8], other: &[u8], tries: Option<i32>) {
    h(O::EntryDeleteCommunicationsKeep, 0, &[]);
    crate::gamecpp::globals::set!(gCommunications, core::ptr::null_mut());
    start_client(tries);
    let player = cstr(player);
    let other = cstr(other);
    h(O::EntrySetPlayerName, 0, &[pa(player.as_ptr() as P)]);
    if kind == ENTRY_HOST {
        h(O::EntrySetHostGameName, 0, &[pa(other.as_ptr() as P)]);
    }
    h(O::EntryPushMainMenu, 0, &[]);
    let event = hp(O::EntryNewMultiPlayerEvent, 0, &[]);
    h(O::EntryProcessOnMainMenu, event, &[]);
    h(O::EntryDereferenceEvent, event, &[]);
    let action = hp(O::EntryNewJoinOrHost, 0, &[kind, pa(player.as_ptr() as P), pa(other.as_ptr() as P)]);
    h(O::EntryPostAction, action, &[]);
    h(O::EntryDereferenceAction, action, &[]);
}

/// "loadgame": the saved game started on a local server (or a client when it cannot be opened), then the main menu.
fn load_game(name: &[u8]) {
    h(O::EntryDeleteCommunicationsKeep, 0, &[]);
    crate::gamecpp::globals::set!(gCommunications, core::ptr::null_mut());
    let name = cstr(name);
    let mut file: P = 0;
    if h(O::EntryOpenSavedGame, 0, &[pa(name.as_ptr() as P), ptr(&mut file)]) == ENTRY_OK {
        h(O::EntryOptionsWriteLock, 0, &[]);
        h(O::LobbyOptionsSetDefaults, 0, &[]);
        h(O::EntryOptionsLoadType, 0, &[2]);
        h(O::EntryOptionsMapTypeOf, file, &[]);
        h(O::EntryOptionsWriteUnlock, 0, &[]);
        h(O::FxFileSpecDelete, file, &[]);
        h(O::EntryNotMultiplayer, 0, &[]);
        h(O::EntryNewServer, 0, &[]);
        h(O::EntryServerStartThreads, 0, &[]);
        h(O::EntryServerNoLoadResponses, 0, &[]);
        h(O::EntrySetGameStateLoadType, 0, &[2]);
        h(O::EntryServerStopPing, 0, &[]);
        h(O::EntryServerLoadState, 0, &[1]);
        h(O::EntryNotInEditor, 0, &[]);
        // kMessageStartLoading {1, TMGetTime()} to the local input
        let values = [1, i64::from(ee_base::time::ee_time_get())].map(super::mrecord::EeGamecppMrValue::integer);
        let message = super::mrecord::new_record(ee_sim::message_ids::EEMessageID::kMessageStartLoading as u8, &values);
        super::net::global().local_input(message as *mut std::ffi::c_void, 0xff);
    } else {
        start_client(None);
    }
    h(O::EntryPushMainMenu, 0, &[]);
}

/// EEParseCommandLine: the command line's commands run in order; without one a client and the main menu. False when
/// the game should quit (update_saved_games).
///
/// # Safety
/// `line` is a terminated C string.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_entry_parse_command_line(line: *const c_char) -> bool {
    let line = if line.is_null() { &[][..] } else { CStr::from_ptr(line).to_bytes() };
    let commands = entry_commands(line);
    let mut result = true;
    for command in &commands {
        match command {
            EntryCommand::Host(player, game) => host_or_join(ENTRY_HOST, player, game, None),
            EntryCommand::JoinByIp(player, address, tries) => {
                host_or_join(ENTRY_JOIN_BY_IP, player, address, Some(*tries))
            }
            EntryCommand::UpdateSavedGames => {
                super::saved_games::ee_gamecpp_update_saved_games();
                result = false;
            }
            EntryCommand::LoadGame(name) => load_game(name),
            EntryCommand::Balancer => {}
        }
    }
    if commands.is_empty() {
        h(O::EntryDeleteCommunicationsKeep, 0, &[]);
        start_client(None);
        h(O::EntryPushMainMenu, 0, &[]);
    }
    result
}

/// The platform's window system and paths (ee_base: platform/PlatWindow.h, PlatMessage.h, PlatPath.h).
/// cbindgen:ignore
#[cfg(not(test))]
mod run_sys {
    use std::ffi::c_char;
    extern "C" {
        pub fn PlatWindowSetAppName(name: *const c_char, icon_resource: u32);
        pub fn PlatWindowSystemStart() -> i32;
        pub fn PlatWindowSystemStop();
        pub fn PlatWindowLastError() -> *const c_char;
        pub fn PlatMessageShow(title: *const c_char, text: *const c_char, flags: u32);
        pub fn PlatPathEnterDataRoot() -> i32;
    }
}

/// The tests run no platform: a call is a bug of the test.
/// cbindgen:ignore
#[cfg(test)]
#[allow(non_snake_case)]
mod run_sys {
    use std::ffi::c_char;
    pub unsafe fn PlatWindowSetAppName(_: *const c_char, _: u32) {
        panic!("no platform in the tests")
    }
    pub unsafe fn PlatWindowSystemStart() -> i32 {
        panic!("no platform in the tests")
    }
    pub unsafe fn PlatWindowSystemStop() {
        panic!("no platform in the tests")
    }
    pub unsafe fn PlatWindowLastError() -> *const c_char {
        panic!("no platform in the tests")
    }
    pub unsafe fn PlatMessageShow(_: *const c_char, _: *const c_char, _: u32) {
        panic!("no platform in the tests")
    }
    pub unsafe fn PlatPathEnterDataRoot() -> i32 {
        panic!("no platform in the tests")
    }
}
use run_sys::*;

/// `PLAT_MESSAGE_ERROR`.
const ENTRY_MESSAGE_ERROR: u32 = 0x10;
/// `S_OK`, `S_FALSE`.
const ENTRY_S_OK: i64 = 0;
const ENTRY_S_FALSE: i64 = 1;

/// EEEmptyShutdown: the network's shutdown routine does nothing.
extern "C" fn empty_shutdown(_shutdown: bool) {}

/// A Rust panic's line (was EERust.cpp's EERustPanicLine): a panic aborts the process and a GUI exe has no stderr,
/// so the line goes to the debugger's output (the harness logs it), stderr, and the game log when a game is running.
///
/// # Safety
/// `line` is a terminated C string.
unsafe extern "C" fn panic_line(line: *const c_char) {
    use std::io::Write;
    // SAFETY: the caller's contract
    let mut text = unsafe { CStr::from_ptr(line) }.to_bytes().to_vec();
    text.push(b'\n');
    let text = CString::new(text).unwrap_or_default();
    // SAFETY: a terminated string of this call
    unsafe { ee_base::plat_core::message::PlatDebugOutput(text.as_ptr()) };
    let mut stderr = std::io::stderr();
    let _ = stderr.write_all(text.as_bytes());
    let _ = stderr.flush();
    if crate::game_log::ee_game_log_on() {
        // SAFETY: as above
        unsafe { crate::game_log::ee_game_log_line(text.as_ptr()) };
    }
}

/// EERun: the game's run from its entry point (WinMain, main): the Rust library's start, the fixture's switches, the
/// single instance, the window system, the engine's initialisation, the splash screen around the start-up, the menus,
/// the threads, the command line and the user interface's run, then the shutdown. 1 when the window system cannot
/// start, else 0.
///
/// # Safety
/// `command_line` is a terminated C string; `instance` the module's.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_entry_run(instance: *mut std::ffi::c_void, command_line: *const c_char) -> i32 {
    // SAFETY: panic_line is callable from any thread for the process' life
    unsafe { crate::ee_rust_init(panic_line) }; // first: a panic anywhere after this reaches a log
                                                // TEST ONLY: the fixture's configure step, which EETestFixture.cpp registers at static initialization (nothing
                                                // unless EE_TEST_UI is set)
    let configure = crate::gamecpp::globals::get!(gEETestFixtureConfigure);
    if !configure.is_null() {
        // SAFETY: a `void (*)()` the C++ stored
        unsafe { core::mem::transmute::<*mut core::ffi::c_void, extern "C" fn()>(configure)() };
    }
    let mut splash = super::start_screen::StartScreen::new(instance as usize);
    let sim = ee_base::plat_core::sim::gPlatSimMode.get() != 0;
    // the simulation: any number of copies at once
    if sim || h(O::RunInstanceMutex, 0, &[]) != 0 {
        // the window system: the window class "Empire Earth Expansion" with the game's icon, its events pumped by the
        // UI thread's TSThread::Receive (this thread: EEUserInterface::Run runs on it)
        PlatWindowSetAppName(c"Empire Earth Expansion".as_ptr(), 0x66);
        if PlatWindowSystemStart() == 0 {
            PlatMessageShow(c"Empire Earth".as_ptr(), PlatWindowLastError(), ENTRY_MESSAGE_ERROR);
            return 1;
        }
        super::window_pump::install();
        let com = h(O::RunComInit, 0, &[]);
        h(O::Verify, 0, &[(com == ENTRY_S_OK || com == ENTRY_S_FALSE) as i64, 3600]);
        ee_base::util::fp::ee_util_fp_set_precision(
            ee_base::lle_enums::UMFloatingPointPrecision::kFPPrecision53 as c_ulong,
        );
        // (the original's UMFastMath::BuildSqrtTable here was empty: the square root table was not built)
        // (the original's first command-line pass here only chose whether to play the intro movies)
        h(O::RunSetInstance, 0, &[pa(instance as P)]);
        ee_base::error::ee_error_initialize();
        // the game's directory is the data root (the executable's directory, or EE_DATA_ROOT), wherever it was started
        // from: its Data, Users and config.yaml are found relative to it (the original used the current directory)
        PlatPathEnterDataRoot();
        h(O::RunFsInitialize, 0, &[]);
        let resource = h(O::RunNewResource, 0, &[pa(instance as P)]);
        h(O::Verify, 0, &[resource, 3632]);
        super::preferences::ee_gamecpp_prefs_init_defaults();
        let loaded = h(O::RunPreferencesLoad, 0, &[]);
        h(O::Verify, 0, &[(loaded == ENTRY_OK) as i64, 3637]);
        if sim {
            sim_preferences();
        }
        super::startup::ee_gamecpp_startup_network_settings();
        super::startup::ee_gamecpp_startup_data_paths();
        // the original tests the constant 1 here
        if !sim {
            splash.start(); // the simulation shows no splash window
        }
        let mut initialized = super::startup::ee_gamecpp_startup_part1();
        if !sim {
            splash.stop();
        }
        if initialized {
            super::startup::ee_gamecpp_startup_menu_screen();
            super::startup::ee_gamecpp_startup_part2();
            crate::fixture::ffi::ee_fixture_start(); // TEST ONLY: the UI fixture's thread, only when EE_TEST_UI is set
            if sim {
                crate::sim_driver::ffi::ee_sim_driver_start(); // TEST ONLY: the simulation driver's thread
            }
            if ee_gamecpp_entry_parse_command_line(command_line) {
                initialized = h(O::RunUserInterface, 0, &[pa(command_line as P)]) == ENTRY_OK;
                super::startup::ee_gamecpp_startup_clear_game();
                ee_base::util::system::ee_util_shutdown(initialized, false);
                h(O::DataPathDereference, 0, &[]);
                h(O::CivPathDereference, 0, &[]);
            }
        } else {
            ee_base::util::system::ee_util_shutdown(initialized, false);
            h(O::DataPathDereference, 0, &[]);
            h(O::CivPathDereference, 0, &[]);
        }
        h(O::RunComUninit, 0, &[]);
        PlatWindowSystemStop();
    }
    drop(splash);
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_byte_sum_wraps_as_an_unsigned_long() {
        assert_eq!(entry_byte_sum(0, &[1, 2, 255]), 258);
        assert_eq!(entry_byte_sum(c_ulong::MAX, &[2]), 1);
    }

    #[test]
    fn the_excluded_name_and_the_network_folders() {
        // the original's encrypted constant decrypts to a file name: text without a NUL, no path
        let name = entry_excluded_name();
        assert!(!name.as_bytes().is_empty());
        assert!(!name.as_bytes().contains(&b'\\'));
        assert_eq!(NET_EXE.to_bytes(), b"");
        assert_eq!(NET_DB.to_bytes(), b"Data\\Db\\");
        assert_eq!(NET_TAI.to_bytes(), b"Data\\Unit AI Scripts\\");
        assert_eq!(NET_MODELS.to_bytes(), b"Data\\Models\\");
    }

    #[test]
    fn scan_long_is_sscanf_percent_ld() {
        assert_eq!(entry_scan_long(b"  1234x", 7), 1234);
        assert_eq!(entry_scan_long(b"-12", 7), -12);
        assert_eq!(entry_scan_long(b"abc", 7), 7);
        assert_eq!(entry_scan_long(b"", 9), 9);
    }

    #[test]
    fn the_version_suffix_is_thirteen_bits_four_digits() {
        assert_eq!(entry_version_suffix(5), ".0005");
        assert_eq!(entry_version_suffix(0x2000 + 1234), ".1234");
        assert_eq!(entry_version_suffix(0x1fff), ".8191");
    }

    #[test]
    fn the_command_line_commands() {
        assert!(entry_commands(b"").is_empty());
        assert_eq!(
            entry_commands(b"-hostgame \"Max\" \"My Game\""),
            vec![EntryCommand::Host(b"Max".to_vec(), b"My Game".to_vec())]
        );
        assert_eq!(
            entry_commands(b"-JOINBYIP \"Max\" \"10.0.0.2\" \"3\""),
            vec![EntryCommand::JoinByIp(b"Max".to_vec(), b"10.0.0.2".to_vec(), 3)]
        );
        assert_eq!(entry_commands(b"-loadgame \"a b.ees\""), vec![EntryCommand::LoadGame(b"a b.ees".to_vec())]);
        assert_eq!(entry_commands(b"-update_saved_games"), vec![EntryCommand::UpdateSavedGames]);
        // a quoted argument keeps its dash; a wrong count is ignored
        assert_eq!(entry_commands(b"-loadgame \"x-y\""), vec![EntryCommand::LoadGame(b"x-y".to_vec())]);
        assert!(entry_commands(b"-hostgame \"Max\"").is_empty());
        assert!(entry_commands(b"-nothing").is_empty());
    }
}

/// EESimPreferences (TEST ONLY; was Empire Earth.cpp's): the settings the simulation runs with, over config.yaml
/// (after gGamePreferences.Load): a window (hidden: DX7Screen::CreateAWindow), the harness's pins (32-bit, Min = Max
/// PTs per Second, the driver's rate), the driver's autosave period, no music, no v-sync.
fn sim_preferences() {
    let (pt_rate, autosave) = crate::sim_driver::driver::driver().map_or((40, 0), |d| d.preferences());
    let settings = gobj::prefs() + gobj::PREFS_SETTINGS;
    // SAFETY: gGamePreferences.mSettings' plain members (ee_game app settings.rs), on the main thread at start-up
    let s = unsafe { &mut *(settings as *mut crate::app::settings::EeAppSettingsFields) };
    s.mFullScreen = false;
    s.mGameBitDepth = 32;
    s.mTextureBitDepth = 32;
    s.mGameWindowWidth = 800;
    s.mGameWindowHeight = 600;
    s.mMinPTsPerSecond = pt_rate;
    s.mMaxPTsPerSecond = pt_rate;
    s.mAutoSaveInMilliseconds = autosave as std::ffi::c_ulong;
    s.mMusicEnabled = false;
    s.mWaitForVSync = false;
}
