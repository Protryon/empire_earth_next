//! The scenario editor's load thread (`EESELoadThread`: its constructor, destructor, thread procedure, message
//! dispatch and handlers) and the instructions screen's text export and import (`EESEIAExport`, `EESEIAImport`,
//! the export and import buttons' events and the two questions' answers), from `Scenario Editor/EESELoadThread.cpp`
//! and `EESEIAExport.cpp` (the scenario editor lane's round 4). The C++ classes keep their members and lifetimes;
//! every body is here, composed of single C++ statements (`EE_SCREENS_SELTX_*`, `ee_screenscpp_seltx`) in the C++'s
//! order. The texts are built here (the language strings, the loading lines' `sprintf`, the import's version
//! header); the FPU checks and the sleep are ee_base's and the edit box's line the toolkit's (`EE_SCREENS_SELTX_R_*`,
//! answered in Rust by the adapter).

// (the casts of the C `unsigned long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_ulong, c_void};

use crate::screens::hud_direct::{atoi, resource_string};

// ---- the statements: x(op, p, a): p0 the load thread or the action / event's form, p1 a handle, p2 a text (in: its
// bytes, a2 their count; out: a buffer of a3 bytes, the answer the text's whole length)

/// The constructor's: `mIsDone = a0 != 0`, `mEditor = p1`, `ESVERIFY_AT(CreateEvent(mThreadCreated / mThreadDone /
/// mMessageQueueDone) == keseOK)`, `CreateThread(mThread, Main, this)`, `mThread->SetPriority(a0)`,
/// `mThreadCreated->Wait(infinite)`, `mThread->SetName(gScenarioEditorThreadName)`, `mChangeLoadText = new
/// EESEAChangeLoadText`.
pub const EE_SCREENS_SELTX_SET_DONE: u32 = 0;
pub const EE_SCREENS_SELTX_SET_EDITOR: u32 = 1;
pub const EE_SCREENS_SELTX_CREATE_EVENT_CREATED: u32 = 2;
pub const EE_SCREENS_SELTX_CREATE_EVENT_DONE: u32 = 3;
pub const EE_SCREENS_SELTX_CREATE_EVENT_QUEUE: u32 = 4;
pub const EE_SCREENS_SELTX_CREATE_THREAD: u32 = 5;
pub const EE_SCREENS_SELTX_SET_PRIORITY: u32 = 6;
pub const EE_SCREENS_SELTX_WAIT_CREATED: u32 = 7;
pub const EE_SCREENS_SELTX_SET_NAME: u32 = 8;
pub const EE_SCREENS_SELTX_NEW_LOAD_TEXT: u32 = 9;
/// The destructor's: `mChangeLoadText->Dereference()`, `DestroyEvent` of the three events, `DestroyThread`.
pub const EE_SCREENS_SELTX_LOAD_TEXT_RELEASE: u32 = 10;
pub const EE_SCREENS_SELTX_DESTROY_EVENT_CREATED: u32 = 11;
pub const EE_SCREENS_SELTX_DESTROY_EVENT_DONE: u32 = 12;
pub const EE_SCREENS_SELTX_DESTROY_EVENT_QUEUE: u32 = 13;
pub const EE_SCREENS_SELTX_DESTROY_THREAD: u32 = 14;
/// Main's: `mThreadCreated->Set()`, `mThread->SetPriority(PLAT_PRIORITY_ABOVE_NORMAL)`, `mIsDone`, `mThread->Receive`
/// into Main's locals (the message at p1, the two parameters at p2), `delete this`.
pub const EE_SCREENS_SELTX_SIGNAL_CREATED: u32 = 15;
pub const EE_SCREENS_SELTX_PRIORITY_ABOVE_NORMAL: u32 = 16;
pub const EE_SCREENS_SELTX_IS_DONE: u32 = 17;
pub const EE_SCREENS_SELTX_RECEIVE: u32 = 18;
pub const EE_SCREENS_SELTX_DELETE: u32 = 19;
/// The messages': `gUserInterface->LeaveScenarioEditor()`, `StartScenarioTest()`, `StartScenarioTest2()`,
/// `PostEvent(p1, NULL)`, p1's `Dereference()`, `ShowLoadedObject(p1)`, `mMessageQueueDone->Set()`.
pub const EE_SCREENS_SELTX_LEAVE_EDITOR: u32 = 20;
pub const EE_SCREENS_SELTX_START_TEST: u32 = 21;
pub const EE_SCREENS_SELTX_START_TEST2: u32 = 22;
pub const EE_SCREENS_SELTX_POST_EVENT: u32 = 23;
pub const EE_SCREENS_SELTX_DEREFERENCE: u32 = 24;
pub const EE_SCREENS_SELTX_SHOW_LOADED: u32 = 25;
pub const EE_SCREENS_SELTX_QUEUE_DONE: u32 = 26;
/// The handlers': `gWorld.mLock`'s `ReadLock()`, `ReadUnlock()`, `WriteLock()`, `WriteUnlock()`; `gWorld.mPlayers[a0]`;
/// `mChangeLoadText->Post(the text)`; player p1's `LoadAllClassMedia()`, `UnloadAllClassMedia()`, its
/// `mTechnologies->Get(a0)`'s `LoadMedia(p1)` / `UnloadMedia(p1)`.
pub const EE_SCREENS_SELTX_READ_LOCK: u32 = 30;
pub const EE_SCREENS_SELTX_READ_UNLOCK: u32 = 31;
pub const EE_SCREENS_SELTX_WRITE_LOCK: u32 = 32;
pub const EE_SCREENS_SELTX_WRITE_UNLOCK: u32 = 33;
pub const EE_SCREENS_SELTX_PLAYER: u32 = 34;
pub const EE_SCREENS_SELTX_POST_TEXT: u32 = 35;
pub const EE_SCREENS_SELTX_LOAD_ALL_MEDIA: u32 = 36;
pub const EE_SCREENS_SELTX_UNLOAD_ALL_MEDIA: u32 = 37;
pub const EE_SCREENS_SELTX_TECH_LOAD_MEDIA: u32 = 38;
pub const EE_SCREENS_SELTX_TECH_UNLOAD_MEDIA: u32 = 39;
/// `EETerrainTable::Get(a0)`, `gWorld.mMap != NULL`, `EETerrainTable::Get(a0)->mNameID`, terrain p1's
/// `Load(gWorld.mMap->mTerrainMesh)` / `Unload(...)`, `GetNumUnitClasses()`, `GetUnitClass(a0)`.
pub const EE_SCREENS_SELTX_TERRAIN: u32 = 40;
pub const EE_SCREENS_SELTX_MAP_EXISTS: u32 = 41;
pub const EE_SCREENS_SELTX_TERRAIN_NAME_ID: u32 = 42;
pub const EE_SCREENS_SELTX_TERRAIN_LOAD: u32 = 43;
pub const EE_SCREENS_SELTX_TERRAIN_UNLOAD: u32 = 44;
pub const EE_SCREENS_SELTX_TERRAIN_CLASSES: u32 = 45;
pub const EE_SCREENS_SELTX_TERRAIN_CLASS: u32 = 46;
/// `gUnitClassTable[a0]`; player p1's `SetUnitClassMedia(a0, 0x7f)`, `LoadUnitClassButtons(a0)`,
/// `UnloadUnitClassMedia(a0)`, `UnloadUnitClassButtons(a0)`; class p1's `mNameStringID`, `mUnitType`,
/// `IsLandBuilding()`; `EEGhostUnit::Create(mEditor->GetMouseLocation(), a0, player p1, a1 != 0 (not a land
/// building) ? the unit tab's angle (`ee_screens_seun_angle`) : 0, false)`.
pub const EE_SCREENS_SELTX_CLASS: u32 = 50;
pub const EE_SCREENS_SELTX_SET_CLASS_MEDIA: u32 = 51;
pub const EE_SCREENS_SELTX_LOAD_CLASS_BUTTONS: u32 = 52;
pub const EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA: u32 = 53;
pub const EE_SCREENS_SELTX_UNLOAD_CLASS_BUTTONS: u32 = 54;
pub const EE_SCREENS_SELTX_CLASS_NAME_ID: u32 = 55;
pub const EE_SCREENS_SELTX_CLASS_TYPE: u32 = 56;
pub const EE_SCREENS_SELTX_CLASS_LAND_BUILDING: u32 = 57;
pub const EE_SCREENS_SELTX_GHOST: u32 = 59;
/// Player p1's `ReloadTechTree(a0 != 0)`, `gWorld.FlushQueuedPlacements()`, its `AddPendingUnits(0, 0)`,
/// `RemoveKilledUnits()`, `ReleasePendingUnits()`.
pub const EE_SCREENS_SELTX_RELOAD_TECH: u32 = 60;
pub const EE_SCREENS_SELTX_FLUSH_PLACEMENTS: u32 = 61;
pub const EE_SCREENS_SELTX_ADD_PENDING: u32 = 62;
pub const EE_SCREENS_SELTX_REMOVE_KILLED: u32 = 63;
pub const EE_SCREENS_SELTX_RELEASE_PENDING: u32 = 64;

/// The text actions' (p1 a file spec `FSFileSpec*`): the action p0's file and its validate flag (answered in Rust by
/// the adapter from the action's [`EeScreensSeltInfo`]); p1's `FOpen("w")` / `FOpen("r")`, `Close()`, `feof`, `fgets`
/// into the buffer (0x200 bytes; the answer: not NULL); `gUIManager->GetScenarioEditorForm()->RefreshCurrentGroup()`
/// / `ResumeCurrentGroup()`; the `"{Version_%ld}\n\n"` header's `fprintf` (as an `unsigned long`) of a0 (the adapter
/// passes [`EE_SCREENS_SELT_TEXT_VERSION`]); the version (answered by the adapter).
pub const EE_SCREENS_SELTX_EXPORT_FILE: u32 = 70;
pub const EE_SCREENS_SELTX_IMPORT_FILE: u32 = 71;
pub const EE_SCREENS_SELTX_IMPORT_VALIDATE: u32 = 72;
pub const EE_SCREENS_SELTX_FOPEN_W: u32 = 73;
pub const EE_SCREENS_SELTX_FOPEN_R: u32 = 74;
pub const EE_SCREENS_SELTX_CLOSE: u32 = 75;
pub const EE_SCREENS_SELTX_FEOF: u32 = 76;
pub const EE_SCREENS_SELTX_FGETS: u32 = 77;
pub const EE_SCREENS_SELTX_REFRESH_GROUP: u32 = 78;
pub const EE_SCREENS_SELTX_RESUME_GROUP: u32 = 79;
pub const EE_SCREENS_SELTX_WRITE_HEADER: u32 = 80;
pub const EE_SCREENS_SELTX_TEXT_VERSION: u32 = 81;
/// The manager's: `ExportText(*p1)`, `ImportTriggerText(*p1, a0, a1 != 0)`, `mImportErrors.size()`,
/// `mImportErrors[a0]` (out), `UWideString(p1->GetFile()) != mExportFileName`, `mExportFileName = p1->GetFile()`.
pub const EE_SCREENS_SELTX_EXPORT_TEXT: u32 = 82;
pub const EE_SCREENS_SELTX_IMPORT_TEXT: u32 = 83;
pub const EE_SCREENS_SELTX_ERROR_COUNT: u32 = 84;
pub const EE_SCREENS_SELTX_ERROR: u32 = 85;
pub const EE_SCREENS_SELTX_NAME_CHANGED: u32 = 86;
pub const EE_SCREENS_SELTX_SET_EXPORT_NAME: u32 = 87;
/// The messages: `gUIManager->DisplayMessage(the text, 1, NULL, NULL, true)`, the form p0's manager's; the
/// question with the text and answer a0 (0 the export's, 1 the import's) carrying p1, `gUIManager`'s and the form
/// p0's manager's.
pub const EE_SCREENS_SELTX_MESSAGE: u32 = 90;
pub const EE_SCREENS_SELTX_FORM_MESSAGE: u32 = 91;
pub const EE_SCREENS_SELTX_QUESTION: u32 = 92;
pub const EE_SCREENS_SELTX_FORM_QUESTION: u32 = 93;
/// The specs and answers: `new FSFileSpec`, `new FSFileSpec(*p1)`, `delete p1`, p1's `SetVolume(gStartupVolume)`,
/// `SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\")`, `SetFile(the text)`; the import info (once `new
/// EESEImportInfo{*p1, a0 != 0}`) and its `delete`, the posts of the export action (p1 a spec, copied) and of the
/// import action (p1 an info, copied): answered by the adapter over `SPEC_COPY`, `SPEC_DELETE` and
/// [`EE_SCREENS_SELTX_POST_TEXT_ACTION`].
pub const EE_SCREENS_SELTX_SPEC_NEW: u32 = 100;
pub const EE_SCREENS_SELTX_SPEC_COPY: u32 = 101;
pub const EE_SCREENS_SELTX_SPEC_DELETE: u32 = 102;
pub const EE_SCREENS_SELTX_SPEC_VOLUME: u32 = 103;
pub const EE_SCREENS_SELTX_SPEC_DIRECTORY: u32 = 104;
pub const EE_SCREENS_SELTX_SPEC_FILE: u32 = 105;
pub const EE_SCREENS_SELTX_INFO_NEW: u32 = 106;
pub const EE_SCREENS_SELTX_INFO_DELETE: u32 = 107;
pub const EE_SCREENS_SELTX_POST_EXPORT: u32 = 108;
pub const EE_SCREENS_SELTX_POST_IMPORT: u32 = 109;
/// `gUserInterface->PostAction(new EESEARust(a0, p1))`: a text action (`EE_SCREENS_SEW_TEXT_*`) on its info.
pub const EE_SCREENS_SELTX_POST_TEXT_ACTION: u32 = 5000;
/// The trigger text files' version (`{Version_1}`; the original's `gSETextVersion` 0x4c2cb8, .data = 1).
pub const EE_SCREENS_SELT_TEXT_VERSION: i64 = 1;
/// The instructions screen's export and import buttons' events (`EESEERust` kinds; once `EESEIEExport` and
/// `EESEIEImport`, vtables 0x442bd4 and 0x442be8): their `ProcessEvent` is `export_event` / `import_event`.
pub const EE_SCREENS_SEE_TEXT_EXPORT: u32 = 5000;
pub const EE_SCREENS_SEE_TEXT_IMPORT: u32 = 5001;
/// The text actions (`EESEARust` kinds over an [`EeScreensSeltInfo`]; once `EESEIAExport`, vtable 0x442bb0, and
/// `EESEIAImport`, vtable 0x442bc0): their `Virtual4` is `export` / `import`.
pub const EE_SCREENS_SEW_TEXT_EXPORT: u32 = 5000;
pub const EE_SCREENS_SEW_TEXT_IMPORT: u32 = 5001;

/// A text action's or the import question's payload (once the actions' `mFile`/`mValidate` and `EESEImportInfo`,
/// new 0x4c): the action's own copy of the file spec and whether the import only validates.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensSeltInfo {
    pub spec: usize,
    pub validate: bool,
}

/// The statements answered in Rust by the adapter: `UMGetFloatingPointPrecision() == kFPPrecision53`,
/// `UMSetFloatingPointPrecision(kFPPrecision53)`, `UMGetFloatingPointRoundingMode()`,
/// `UMSetFloatingPointRoundingMode(0)`, `PlatSleep(1)`; the form p0's control a0 (an edit box)'s `GetText(0)` (out).
pub const EE_SCREENS_SELTX_R_FP_IS53: u32 = 900;
pub const EE_SCREENS_SELTX_R_FP_SET53: u32 = 901;
pub const EE_SCREENS_SELTX_R_FP_ROUNDING: u32 = 902;
pub const EE_SCREENS_SELTX_R_FP_ROUND0: u32 = 903;
pub const EE_SCREENS_SELTX_R_SLEEP: u32 = 904;
pub const EE_SCREENS_SELTX_R_EDIT_TEXT: u32 = 905;

// ---- the numbers
/// The messages (`kThreadMessageQuit`, `kLoadThread*`).
const MSG_QUIT: c_ulong = 0x401;
const MSG_GHOST: c_ulong = 0x402;
const MSG_RELOAD_TECH: c_ulong = 0x403;
const MSG_START_TEST: c_ulong = 0x404;
const MSG_START_TEST2: c_ulong = 0x405;
const MSG_LOAD_EPOCHS: c_ulong = 0x406;
const MSG_UNLOAD_EPOCHS: c_ulong = 0x407;
const MSG_LOAD_TERRAIN: c_ulong = 0x408;
const MSG_UNLOAD_TERRAIN: c_ulong = 0x409;
const MSG_QUEUE_DONE: c_ulong = 0x40a;
const MSG_POST_EVENT: c_ulong = 0x40b;
const MSG_SHOW_LOADED: c_ulong = 0x40c;
/// The strings (ee/EEStringIDs.h; EESELoadThread.cpp asserts them).
pub const EE_SCREENS_SELTX_IDS_LOADING_EPOCH_FOR_PLAYER: i64 = 30297;
pub const EE_SCREENS_SELTX_IDS_LOADING_FOR_PLAYER: i64 = 30298;
pub const EE_SCREENS_SELTX_IDS_LOADING_TERRAIN: i64 = 31527;
pub const EE_SCREENS_SELTX_IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE: i64 = 10015;
pub const EE_SCREENS_SELTX_IDS_REPLACE_FILE_ON_DISK: i64 = 10018;
pub const EE_SCREENS_SELTX_IDS_FILE_NAME_NOT_FOUND: i64 = 10019;
pub const EE_SCREENS_SELTX_IDS_FILE_ERROR: i64 = 10024;
pub const EE_SCREENS_SELTX_IDS_FILE_EXPORTED_SUCCESSFULLY: i64 = 10025;
pub const EE_SCREENS_SELTX_IDS_EXPORTED_FILE_NAME_HAS_CHANGED: i64 = 10026;
pub const EE_SCREENS_SELTX_IDS_FILE_IMPORTED_SUCCESSFULLY: i64 = 10031;
pub const EE_SCREENS_SELTX_IDS_IMPORTING_FILE_REPLACES_ALL_TEXT: i64 = 10032;
pub const EE_SCREENS_SELTX_IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING: i64 = 10033;
pub const EE_SCREENS_SELTX_IDS_LCL_FILE_HAS_INVALID_VERSION: i64 = 10057;
/// The instructions screen's localization edit box (`kSEInstructionsLocalizationEditBox`).
pub const EE_SCREENS_SELTX_LOCALIZATION_EDIT: i64 = 0x45b;
/// `fgets`' buffer (0x200 bytes).
const LINE_BUFFER: usize = 0x200;

/// One statement: `x(op, p, a)` -> its answer.
pub type Seltx<'a> = dyn FnMut(u32, [usize; 3], [i64; 4]) -> i64 + 'a;

fn st(x: &mut Seltx, op: u32, p0: usize, p1: usize, a0: i64) -> i64 {
    x(op, [p0, p1, 0], [a0, 0, 0, 0])
}

/// A statement taking a text (p2 its bytes, a2 their count).
fn st_text(x: &mut Seltx, op: u32, p0: usize, p1: usize, a0: i64, text: &[u8]) -> i64 {
    x(op, [p0, p1, text.as_ptr() as usize], [a0, 0, text.len() as i64, 0])
}

/// A statement answering a text into a buffer (p2, a3 its size; the answer the whole length).
fn read_text(x: &mut Seltx, op: u32, p0: usize, p1: usize, a0: i64) -> Vec<u8> {
    let mut buffer = vec![0u8; 256];
    loop {
        let n = x(op, [p0, p1, buffer.as_mut_ptr() as usize], [a0, 0, 0, buffer.len() as i64]).max(0) as usize;
        if n <= buffer.len() {
            buffer.truncate(n);
            return buffer;
        }
        buffer = vec![0u8; n];
    }
}

/// `gResource->GetString(id, out)`: the string, or `out` left as it was when there is none.
fn get_string(id: i64, out: &mut Vec<u8>) {
    if let Some(s) = resource_string(id) {
        *out = s.to_vec();
    }
}

/// One argument of [`c_format`].
pub enum EeScreensSeltArg<'a> {
    Text(&'a [u8]),
    Number(i64),
}

/// The CRT's `sprintf` over the conversions the loading lines use: `%s`, `%d` / `%i` / `%ld` (the `int`), `%u` /
/// `%lu`, `%%`; any other conversion is copied as written.
pub fn c_format(format: &[u8], args: &[EeScreensSeltArg]) -> Vec<u8> {
    let mut out = Vec::new();
    let mut next = args.iter();
    let mut i = 0;
    while i < format.len() {
        let c = format[i];
        if c != b'%' {
            out.push(c);
            i += 1;
            continue;
        }
        let mut j = i + 1;
        while j < format.len() && format[j] == b'l' {
            j += 1;
        }
        match format.get(j) {
            Some(b'%') if j == i + 1 => out.push(b'%'),
            Some(b's') => {
                if let Some(EeScreensSeltArg::Text(t)) = next.next() {
                    out.extend_from_slice(t);
                }
            }
            Some(b'd') | Some(b'i') => {
                if let Some(EeScreensSeltArg::Number(n)) = next.next() {
                    out.extend_from_slice((*n as i32).to_string().as_bytes());
                }
            }
            Some(b'u') => {
                if let Some(EeScreensSeltArg::Number(n)) = next.next() {
                    out.extend_from_slice((*n as u32).to_string().as_bytes());
                }
            }
            Some(_) => out.extend_from_slice(&format[i..=j]),
            None => {
                out.extend_from_slice(&format[i..]);
                j = format.len();
            }
        }
        i = j + 1;
    }
    out
}

// ------------------------------------------------------------------------------------------------ the load thread

/// `EESELoadThread::EESELoadThread(editor, priority)`.
pub fn construct(x: &mut Seltx, t: usize, editor: usize, priority: i64) {
    st(x, EE_SCREENS_SELTX_SET_DONE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_SET_EDITOR, t, editor, 0);
    st(x, EE_SCREENS_SELTX_CREATE_EVENT_CREATED, t, 0, 0);
    st(x, EE_SCREENS_SELTX_CREATE_EVENT_DONE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_CREATE_EVENT_QUEUE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_CREATE_THREAD, t, 0, 0);
    st(x, EE_SCREENS_SELTX_SET_PRIORITY, t, 0, priority);
    st(x, EE_SCREENS_SELTX_WAIT_CREATED, t, 0, 0);
    st(x, EE_SCREENS_SELTX_SET_NAME, t, 0, 0);
    st(x, EE_SCREENS_SELTX_NEW_LOAD_TEXT, t, 0, 0);
}

/// `EESELoadThread::~EESELoadThread()`.
pub fn destroy(x: &mut Seltx, t: usize) {
    st(x, EE_SCREENS_SELTX_LOAD_TEXT_RELEASE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_DESTROY_EVENT_CREATED, t, 0, 0);
    st(x, EE_SCREENS_SELTX_DESTROY_EVENT_DONE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_DESTROY_EVENT_QUEUE, t, 0, 0);
    st(x, EE_SCREENS_SELTX_DESTROY_THREAD, t, 0, 0);
}

/// `EESELoadThread::Main`: the messages handled with 53-bit precision until told to quit, then the thread object
/// deleted.
pub fn main(x: &mut Seltx, t: usize) -> c_ulong {
    st(x, EE_SCREENS_SELTX_SIGNAL_CREATED, t, 0, 0);
    st(x, EE_SCREENS_SELTX_PRIORITY_ABOVE_NORMAL, t, 0, 0);
    st(x, EE_SCREENS_SELTX_R_FP_SET53, t, 0, 0);
    while st(x, EE_SCREENS_SELTX_IS_DONE, t, 0, 0) == 0 {
        let mut message: c_ulong = 0;
        let mut parameters = [core::ptr::null_mut::<c_void>(); 2];
        let p = [t, &mut message as *mut c_ulong as usize, parameters.as_mut_ptr() as usize];
        let result = x(EE_SCREENS_SELTX_RECEIVE, p, [0; 4]);
        if st(x, EE_SCREENS_SELTX_R_FP_IS53, t, 0, 0) == 0 {
            st(x, EE_SCREENS_SELTX_R_FP_SET53, t, 0, 0);
        }
        if st(x, EE_SCREENS_SELTX_R_FP_ROUNDING, t, 0, 0) != 0 {
            st(x, EE_SCREENS_SELTX_R_FP_ROUND0, t, 0, 0);
        }
        if result == 0 {
            handle_message(x, t, message, parameters[0] as usize, parameters[1] as usize);
        }
        st(x, EE_SCREENS_SELTX_R_SLEEP, t, 0, 0);
    }
    st(x, EE_SCREENS_SELTX_DELETE, t, 0, 0);
    0
}

/// `EESELoadThread::HandleMessage`.
pub fn handle_message(x: &mut Seltx, t: usize, message: c_ulong, p1: usize, p2: usize) {
    match message {
        MSG_QUIT => {
            st(x, EE_SCREENS_SELTX_LEAVE_EDITOR, t, 0, 0);
            st(x, EE_SCREENS_SELTX_SET_DONE, t, 0, 1);
        }
        MSG_GHOST => handle_402(x, t, p1 as c_ulong, p2 as c_ulong),
        MSG_LOAD_EPOCHS => handle_406(x, t, p1 as c_ulong, p2 as c_ulong),
        MSG_RELOAD_TECH => handle_403(x, t, p1 as c_ulong, p2 != 0),
        MSG_START_TEST => {
            st(x, EE_SCREENS_SELTX_START_TEST, t, 0, 0);
            st(x, EE_SCREENS_SELTX_SET_DONE, t, 0, 1);
        }
        MSG_POST_EVENT => {
            st(x, EE_SCREENS_SELTX_POST_EVENT, t, p1, 0);
            st(x, EE_SCREENS_SELTX_DEREFERENCE, t, p1, 0);
        }
        MSG_LOAD_TERRAIN => handle_408(x, t, p1 as c_ulong, false),
        MSG_SHOW_LOADED => {
            st(x, EE_SCREENS_SELTX_SHOW_LOADED, t, p1, 0);
            st(x, EE_SCREENS_SELTX_DEREFERENCE, t, p1, 0);
        }
        MSG_START_TEST2 => {
            st(x, EE_SCREENS_SELTX_START_TEST2, t, 0, 0);
            st(x, EE_SCREENS_SELTX_SET_DONE, t, 0, 1);
        }
        MSG_UNLOAD_EPOCHS => handle_407(x, t, p1 as c_ulong, p2 as c_ulong),
        MSG_UNLOAD_TERRAIN => handle_408(x, t, p1 as c_ulong, true),
        MSG_QUEUE_DONE => {
            st(x, EE_SCREENS_SELTX_QUEUE_DONE, t, 0, 0);
        }
        _ => {}
    }
}

/// The player `index` read under the world's read lock.
fn locked_player(x: &mut Seltx, t: usize, index: c_ulong) -> usize {
    st(x, EE_SCREENS_SELTX_READ_LOCK, t, 0, 0);
    let player = st(x, EE_SCREENS_SELTX_PLAYER, t, 0, index as i64) as usize;
    st(x, EE_SCREENS_SELTX_READ_UNLOCK, t, 0, 0);
    player
}

fn post_text(x: &mut Seltx, t: usize, text: &[u8]) {
    st_text(x, EE_SCREENS_SELTX_POST_TEXT, t, 0, 0, text);
}

/// The epoch `epoch`'s title string id (`EEEpochTitleID`).
fn epoch_title(epoch: c_ulong) -> i64 {
    ee_sim::tech::tech_ids::ee_tech_epoch_title_id(epoch as i32) as i64
}

/// `Handle406`: the player's unit media and the first `count` epochs' tech media loaded, "Loading <epoch>" shown.
pub fn handle_406(x: &mut Seltx, t: usize, player_index: c_ulong, count: c_ulong) {
    let (mut format, mut text, mut name) = (Vec::new(), Vec::new(), Vec::new());
    let player = locked_player(x, t, player_index);
    get_string(EE_SCREENS_SELTX_IDS_LOADING_EPOCH_FOR_PLAYER, &mut format);
    let line =
        |name: &[u8]| c_format(&format, &[EeScreensSeltArg::Text(name), EeScreensSeltArg::Number(player_index as i64)]);
    if !format.is_empty() {
        get_string(epoch_title(0), &mut name);
        text = line(&name);
    }
    post_text(x, t, &text);
    st(x, EE_SCREENS_SELTX_LOAD_ALL_MEDIA, t, player, 0);
    for i in 0..count {
        if !format.is_empty() {
            get_string(epoch_title(i), &mut name);
            text = line(&name);
        }
        post_text(x, t, &text);
        st(x, EE_SCREENS_SELTX_TECH_LOAD_MEDIA, t, player, i as i64);
    }
    post_text(x, t, b"");
}

/// `Handle408`: a terrain type and its unit classes' media and buttons loaded ("Loading <terrain>" shown) or
/// unloaded for player 0.
pub fn handle_408(x: &mut Seltx, t: usize, terrain_index: c_ulong, unload: bool) {
    let player = st(x, EE_SCREENS_SELTX_PLAYER, t, 0, 0) as usize;
    let terrain = st(x, EE_SCREENS_SELTX_TERRAIN, t, 0, terrain_index as i64) as usize;
    if terrain == 0 || st(x, EE_SCREENS_SELTX_MAP_EXISTS, t, 0, 0) == 0 {
        return;
    }
    let classes = |x: &mut Seltx| st(x, EE_SCREENS_SELTX_TERRAIN_CLASSES, t, terrain, 0) as c_ulong;
    if !unload {
        let (mut format, mut name) = (Vec::new(), Vec::new());
        get_string(EE_SCREENS_SELTX_IDS_LOADING_TERRAIN, &mut format);
        let name_id = st(x, EE_SCREENS_SELTX_TERRAIN_NAME_ID, t, 0, terrain_index as i64);
        get_string(name_id, &mut name);
        post_text(x, t, &c_format(&format, &[EeScreensSeltArg::Text(&name)]));
        st(x, EE_SCREENS_SELTX_TERRAIN_LOAD, t, terrain, 0);
        if classes(x) > 0 {
            let mut i = 0;
            while i < classes(x) {
                let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                if st(x, EE_SCREENS_SELTX_CLASS, t, 0, class) != 0 {
                    let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                    st(x, EE_SCREENS_SELTX_SET_CLASS_MEDIA, t, player, class);
                    let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                    st(x, EE_SCREENS_SELTX_LOAD_CLASS_BUTTONS, t, player, class);
                }
                i += 1;
            }
        }
        post_text(x, t, b"");
    } else {
        st(x, EE_SCREENS_SELTX_TERRAIN_UNLOAD, t, terrain, 0);
        if classes(x) > 0 {
            let mut i = 0;
            while i < classes(x) {
                let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                if st(x, EE_SCREENS_SELTX_CLASS, t, 0, class) != 0 {
                    let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                    st(x, EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA, t, player, class);
                    let class = st(x, EE_SCREENS_SELTX_TERRAIN_CLASS, t, terrain, i as i64);
                    st(x, EE_SCREENS_SELTX_UNLOAD_CLASS_BUTTONS, t, player, class);
                }
                i += 1;
            }
        }
    }
}

/// `Handle402`: a unit class's media loaded for a player, its placement ghost made at the mouse (turned as the unit
/// tab's unless it is a land building), the media unloaded again.
pub fn handle_402(x: &mut Seltx, t: usize, class_index: c_ulong, player_index: c_ulong) {
    let (mut format, mut name, mut text) = (Vec::new(), Vec::new(), Vec::new());
    st(x, EE_SCREENS_SELTX_READ_LOCK, t, 0, 0);
    let class = st(x, EE_SCREENS_SELTX_CLASS, t, 0, class_index as i64) as usize;
    let player = st(x, EE_SCREENS_SELTX_PLAYER, t, 0, player_index as i64) as usize;
    st(x, EE_SCREENS_SELTX_READ_UNLOCK, t, 0, 0);
    if class == 0 || player == 0 {
        return;
    }
    get_string(EE_SCREENS_SELTX_IDS_LOADING_FOR_PLAYER, &mut format);
    get_string(st(x, EE_SCREENS_SELTX_CLASS_NAME_ID, t, class, 0), &mut name);
    if !format.is_empty() {
        text = c_format(&format, &[EeScreensSeltArg::Text(&name), EeScreensSeltArg::Number(player_index as i64)]);
    }
    post_text(x, t, &text);
    let unit_type = st(x, EE_SCREENS_SELTX_CLASS_TYPE, t, class, 0);
    st(x, EE_SCREENS_SELTX_SET_CLASS_MEDIA, t, player, unit_type);
    let turned = st(x, EE_SCREENS_SELTX_CLASS_LAND_BUILDING, t, class, 0) == 0;
    x(EE_SCREENS_SELTX_GHOST, [t, player, 0], [unit_type, turned as i64, 0, 0]);
    st(x, EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA, t, player, unit_type);
    post_text(x, t, b"");
}

/// `Handle403`: the player's tech tree reloaded under the world's write lock, its pending and killed units settled.
pub fn handle_403(x: &mut Seltx, t: usize, player_index: c_ulong, flag: bool) {
    st(x, EE_SCREENS_SELTX_WRITE_LOCK, t, 0, 0);
    let player = st(x, EE_SCREENS_SELTX_PLAYER, t, 0, player_index as i64) as usize;
    st(x, EE_SCREENS_SELTX_RELOAD_TECH, t, player, flag as i64);
    st(x, EE_SCREENS_SELTX_FLUSH_PLACEMENTS, t, 0, 0);
    st(x, EE_SCREENS_SELTX_ADD_PENDING, t, player, 0);
    st(x, EE_SCREENS_SELTX_REMOVE_KILLED, t, player, 0);
    st(x, EE_SCREENS_SELTX_RELEASE_PENDING, t, player, 0);
    st(x, EE_SCREENS_SELTX_WRITE_UNLOCK, t, 0, 0);
}

/// `Handle407`: the player's unit media and the first `count` epochs' tech media unloaded.
pub fn handle_407(x: &mut Seltx, t: usize, player_index: c_ulong, count: c_ulong) {
    let player = locked_player(x, t, player_index);
    st(x, EE_SCREENS_SELTX_UNLOAD_ALL_MEDIA, t, player, 0);
    for i in 0..count {
        st(x, EE_SCREENS_SELTX_TECH_UNLOAD_MEDIA, t, player, i as i64);
    }
}

// ------------------------------------------------------------------------------------------- the text actions

fn message(x: &mut Seltx, text: &[u8]) {
    st_text(x, EE_SCREENS_SELTX_MESSAGE, 0, 0, 0, text);
}

fn string(id: i64) -> Vec<u8> {
    let mut s = Vec::new();
    get_string(id, &mut s);
    s
}

/// `EESEIAExport::Virtual4`: the scenario's text written to the chosen file under its version header, the success
/// (and a changed file name) or a file error shown.
pub fn export(x: &mut Seltx, action: usize) {
    let file = st(x, EE_SCREENS_SELTX_EXPORT_FILE, action, 0, 0) as usize;
    let mut error = Vec::new();
    if st(x, EE_SCREENS_SELTX_FOPEN_W, 0, file, 0) == 0 {
        st(x, EE_SCREENS_SELTX_REFRESH_GROUP, 0, 0, 0);
        if st(x, EE_SCREENS_SELTX_WRITE_HEADER, 0, file, 0) != 0 {
            if st(x, EE_SCREENS_SELTX_EXPORT_TEXT, 0, file, 0) != 0 {
                get_string(EE_SCREENS_SELTX_IDS_FILE_ERROR, &mut error);
                message(x, &error);
            } else {
                let mut text = string(EE_SCREENS_SELTX_IDS_FILE_EXPORTED_SUCCESSFULLY);
                if st(x, EE_SCREENS_SELTX_NAME_CHANGED, 0, file, 0) != 0 {
                    get_string(EE_SCREENS_SELTX_IDS_EXPORTED_FILE_NAME_HAS_CHANGED, &mut error);
                    text.push(b'\n');
                    text.extend_from_slice(&error);
                }
                st(x, EE_SCREENS_SELTX_SET_EXPORT_NAME, 0, file, 0);
                message(x, &text);
            }
            st(x, EE_SCREENS_SELTX_CLOSE, 0, file, 0);
            return;
        }
    }
    get_string(EE_SCREENS_SELTX_IDS_FILE_ERROR, &mut error);
    message(x, &error);
}

/// `EESEExportTextAnswer`: the export action posted for the spec, the spec freed.
pub fn export_answer(x: &mut Seltx, spec: usize) {
    st(x, EE_SCREENS_SELTX_POST_EXPORT, 0, spec, 0);
    st(x, EE_SCREENS_SELTX_SPEC_DELETE, 0, spec, 0);
}

/// `EESEImportTextAnswer`: the import action posted for the info's file, the info freed.
pub fn import_answer(x: &mut Seltx, info: usize) {
    st(x, EE_SCREENS_SELTX_POST_IMPORT, 0, info, 0);
    st(x, EE_SCREENS_SELTX_INFO_DELETE, 0, info, 0);
}

/// The spec of `data\Scenarios\<the edit box's text>.lcl`, or `None` (and the message shown) without a name.
fn scenario_spec(x: &mut Seltx, form: usize, spec: usize) -> bool {
    let mut name = read_text(x, EE_SCREENS_SELTX_R_EDIT_TEXT, form, 0, EE_SCREENS_SELTX_LOCALIZATION_EDIT);
    if name.is_empty() {
        st_text(
            x,
            EE_SCREENS_SELTX_FORM_MESSAGE,
            form,
            0,
            0,
            &string(EE_SCREENS_SELTX_IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE),
        );
        return false;
    }
    name.extend_from_slice(b".lcl");
    st(x, EE_SCREENS_SELTX_SPEC_VOLUME, 0, spec, 0);
    st(x, EE_SCREENS_SELTX_SPEC_DIRECTORY, 0, spec, 0);
    st_text(x, EE_SCREENS_SELTX_SPEC_FILE, 0, spec, 0, &name);
    true
}

/// `EESEIEExport::ProcessEvent`: the export to `data\Scenarios\<name>.lcl`, asking first when the file exists; the
/// open's result (nonzero when the file was new).
pub fn export_event(x: &mut Seltx, form: usize) -> i64 {
    let spec = st(x, EE_SCREENS_SELTX_SPEC_NEW, 0, 0, 0) as usize;
    let mut result = 0;
    if scenario_spec(x, form, spec) {
        result = st(x, EE_SCREENS_SELTX_FOPEN_R, 0, spec, 0);
        if result == 0 {
            st(x, EE_SCREENS_SELTX_CLOSE, 0, spec, 0);
            let copy = st(x, EE_SCREENS_SELTX_SPEC_COPY, 0, spec, 0) as usize;
            st_text(
                x,
                EE_SCREENS_SELTX_FORM_QUESTION,
                form,
                copy,
                0,
                &string(EE_SCREENS_SELTX_IDS_REPLACE_FILE_ON_DISK),
            );
        } else {
            let copy = st(x, EE_SCREENS_SELTX_SPEC_COPY, 0, spec, 0) as usize;
            export_answer(x, copy);
        }
    }
    st(x, EE_SCREENS_SELTX_SPEC_DELETE, 0, spec, 0);
    result
}

/// `EESEIEImport::ProcessEvent`: asks to confirm the import of `data\Scenarios\<name>.lcl` (validating first);
/// the open's result.
pub fn import_event(x: &mut Seltx, form: usize) -> i64 {
    let spec = st(x, EE_SCREENS_SELTX_SPEC_NEW, 0, 0, 0) as usize;
    let mut result = 0;
    if scenario_spec(x, form, spec) {
        result = st(x, EE_SCREENS_SELTX_FOPEN_R, 0, spec, 0);
        if result == 0 {
            let info = st(x, EE_SCREENS_SELTX_INFO_NEW, 0, spec, 1) as usize;
            let text = string(EE_SCREENS_SELTX_IDS_IMPORTING_FILE_REPLACES_ALL_TEXT);
            x(EE_SCREENS_SELTX_FORM_QUESTION, [form, info, text.as_ptr() as usize], [1, 0, text.len() as i64, 0]);
            st(x, EE_SCREENS_SELTX_CLOSE, 0, spec, 0);
        } else {
            st_text(x, EE_SCREENS_SELTX_FORM_MESSAGE, form, 0, 0, &string(EE_SCREENS_SELTX_IDS_FILE_NAME_NOT_FOUND));
        }
    }
    st(x, EE_SCREENS_SELTX_SPEC_DELETE, 0, spec, 0);
    result
}

/// The `{Version_n}` header's number: the text after the first `{Version_` up to the next `}`, `None` without both.
pub fn header_version(line: &[u8]) -> Option<u32> {
    const TAG: &[u8] = b"{Version_";
    let start = line.windows(TAG.len()).position(|w| w == TAG)? + TAG.len();
    let rest = &line[start..];
    let end = rest.iter().position(|&c| c == b'}')?;
    Some(atoi(&rest[..end]) as u32)
}

/// `EESEIAImport::Virtual4`: the trigger text read from the file after its version header is checked; a validation
/// run lists the errors and asks to import anyway; the file closed on every path.
pub fn import(x: &mut Seltx, action: usize) {
    let file = st(x, EE_SCREENS_SELTX_IMPORT_FILE, action, 0, 0) as usize;
    let mut line = Vec::new();
    if st(x, EE_SCREENS_SELTX_FOPEN_R, 0, file, 0) == 0 && st(x, EE_SCREENS_SELTX_FEOF, 0, file, 0) == 0 {
        let mut buffer = [0u8; LINE_BUFFER];
        let read =
            x(EE_SCREENS_SELTX_FGETS, [0, file, buffer.as_mut_ptr() as usize], [0, 0, 0, LINE_BUFFER as i64]) != 0;
        line = if read {
            buffer[..buffer.iter().position(|&c| c == 0).unwrap_or(LINE_BUFFER)].to_vec()
        } else {
            Vec::new()
        };
        if let Some(version) = header_version(&line) {
            if version <= st(x, EE_SCREENS_SELTX_TEXT_VERSION, 0, 0, 0) as u32 {
                let validate = st(x, EE_SCREENS_SELTX_IMPORT_VALIDATE, action, 0, 0) != 0;
                let failed =
                    x(EE_SCREENS_SELTX_IMPORT_TEXT, [0, file, 0], [version as i64, validate as i64, 0, 0]) != 0;
                if failed {
                    if validate {
                        get_string(EE_SCREENS_SELTX_IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING, &mut line);
                        line.extend_from_slice(b"\n\n");
                        let mut i = 0;
                        while i < st(x, EE_SCREENS_SELTX_ERROR_COUNT, 0, 0, 0) {
                            line.extend_from_slice(&read_text(x, EE_SCREENS_SELTX_ERROR, 0, 0, i));
                            line.extend_from_slice(b"\n\n");
                            i += 1;
                        }
                        let info = st(x, EE_SCREENS_SELTX_INFO_NEW, 0, file, 0) as usize;
                        x(EE_SCREENS_SELTX_QUESTION, [0, info, line.as_ptr() as usize], [1, 0, line.len() as i64, 0]);
                    }
                } else {
                    message(x, &string(EE_SCREENS_SELTX_IDS_FILE_IMPORTED_SUCCESSFULLY));
                }
                st(x, EE_SCREENS_SELTX_RESUME_GROUP, 0, 0, 0);
                st(x, EE_SCREENS_SELTX_CLOSE, 0, file, 0);
                return;
            }
        }
        get_string(EE_SCREENS_SELTX_IDS_LCL_FILE_HAS_INVALID_VERSION, &mut line);
    } else {
        get_string(EE_SCREENS_SELTX_IDS_FILE_ERROR, &mut line);
    }
    st(x, EE_SCREENS_SELTX_CLOSE, 0, file, 0);
    message(x, &line);
}

#[cfg(test)]
#[path = "editor_selt_tests.rs"]
mod tests;
