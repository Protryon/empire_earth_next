//! What EE-SIM's driver asks of the game (TEST ONLY): one table of C++ functions (Empire Earth/EESim.cpp fills it)
//! and the trait the driver calls, so the driver's logic runs over a fake game in the unit tests.

use std::ffi::{c_void, CStr, CString};
use std::os::raw::{c_char, c_long, c_ulong};

/// A sink for lines: `push(sink, text)` per line.
pub type EeSimDriverPushFn = unsafe extern "C" fn(*mut c_void, *const c_char);

/// The physics' clock and state (`gPhysics`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeSimDriverPhysics {
    /// `gPhysics != NULL`
    pub present: bool,
    pub timer_running: bool,
    pub game_over: bool,
    pub game_ms: c_long,
    pub pt: c_ulong,
}

/// A flare's or a volcano's target: the player's first living building and what was done there.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeSimDriverSpot {
    pub found: bool,
    pub x: c_long,
    pub y: c_long,
    /// the flare's allies told; the volcano's cast (1 or 0)
    pub count: c_ulong,
}

/// The platform switches EE-SIM sets (platform/PlatSim.h).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeSimDriverSwitches {
    pub seed_set: i32,
    pub seed: u32,
    pub no_human_defeat: i32,
    /// -1: any free port (kPlatSimAnyPort)
    pub port_offset: i32,
    pub scenario_dialogs: bool,
}

/// The sweeps and capture hooks of the game (`kind`, ee/EESimCapture.h and the sweeps EESim.cpp calls).
pub const EE_SIM_DRIVER_SWEEP_FIVE_MINUTES: i32 = 1;
pub const EE_SIM_DRIVER_SWEEP_MESSAGES: i32 = 2;
pub const EE_SIM_DRIVER_SWEEP_CALAMITIES: i32 = 3;
pub const EE_SIM_DRIVER_SWEEP_UNITS: i32 = 4;
pub const EE_SIM_DRIVER_SWEEP_MINUTE: i32 = 5;
pub const EE_SIM_DRIVER_SWEEP_CHEATS: i32 = 6;
pub const EE_SIM_DRIVER_SWEEP_STORMS: i32 = 7;
/// the end of a game: `arg` 0 only the flush; else 1 + the flags (2 effect, 4 group, 8 app, 16 missile sweep)
pub const EE_SIM_DRIVER_SWEEP_END: i32 = 8;
/// `EESimCaptureRecord::Hold(arg != 0)`
pub const EE_SIM_DRIVER_SWEEP_HOLD: i32 = 9;
/// --scenario: a shown dialog message ends now
pub const EE_SIM_DRIVER_SWEEP_DIALOG: i32 = 10;

/// The lists a load or scenario screen shows (`which`).
pub const EE_SIM_DRIVER_LIST_LOAD: i32 = 0;
pub const EE_SIM_DRIVER_LIST_SCENARIO: i32 = 1;
pub const EE_SIM_DRIVER_LIST_CAMPAIGN: i32 = 2;

/// The world lock (`op`).
pub const EE_SIM_DRIVER_WRITE_LOCK: i32 = 0;
pub const EE_SIM_DRIVER_WRITE_UNLOCK: i32 = 1;
pub const EE_SIM_DRIVER_READ_LOCK: i32 = 2;
pub const EE_SIM_DRIVER_READ_UNLOCK: i32 = 3;

/// The C++ side (EESim.cpp). Every function is called with the documented locks; texts are NUL ended, buffers are
/// written NUL ended within `cap` and return the full length.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeSimDriverHost {
    /// the form on top's id (0: none)
    pub top_form: unsafe extern "C" fn() -> u32,
    /// the form on top's class name (the wait's timeout message)
    pub top_form_class: unsafe extern "C" fn(*mut c_char, usize) -> usize,
    /// the control's trigger fired: 0, 1 the form is not on top or has no such control, 2 the control has no trigger
    pub click: unsafe extern "C" fn(u32, u32) -> i32,
    /// the lobby combo's lines and selection; false when the control is not a combo on the lobby
    pub combo_lines: unsafe extern "C" fn(c_long, *mut c_void, EeSimDriverPushFn, *mut c_long) -> bool,
    /// the lobby combo's selection set; false when the lobby went away
    pub combo_select: unsafe extern "C" fn(c_long, c_ulong) -> bool,
    /// a screen's list (`EE_SIM_DRIVER_LIST_*`): its lines and selection; false when it is not on top
    pub list_lines: unsafe extern "C" fn(i32, *mut c_void, EeSimDriverPushFn, *mut c_ulong) -> bool,
    /// a screen's list's selection changed (as its keys do); false when it is not on top
    pub list_select: unsafe extern "C" fn(i32, c_ulong) -> bool,
    /// the control of the form on top whose trigger is the screen event `code` (-1: none, or another form)
    pub control_with_event: unsafe extern "C" fn(u32, u32) -> c_long,
    pub physics: unsafe extern "C" fn(*mut EeSimDriverPhysics),
    /// `gUIManager->mScenarioEditorForm` on top with a map
    pub editor_up: unsafe extern "C" fn() -> bool,
    /// `EE_SIM_DRIVER_*_LOCK` on `gWorld.mLock`
    pub lock: unsafe extern "C" fn(i32),
    /// a new game's idle human (under the write lock): the civilization flags; allied with every computer when true
    pub prepare_human: unsafe extern "C" fn(bool),
    /// `EEAfterLoad`: the state dump of the file at the loaded game's first tick
    pub dump_after_load: unsafe extern "C" fn(*const c_char),
    /// the checksum line's fields (after "CT n " or "LOADED "), its new line included
    pub checksum_fields: unsafe extern "C" fn(*mut c_char, usize) -> usize,
    /// the state dump into the file
    pub dump_state: unsafe extern "C" fn(*const c_char),
    /// a quiet save of the game under the file name (in data\Saved Games), as the autosave requests one
    pub request_save: unsafe extern "C" fn(*const c_char),
    /// `gSavedGameExtension` (0) or `gScenarioExtension` (1)
    pub extension: unsafe extern "C" fn(i32, *mut c_char, usize) -> usize,
    /// `gPlatSimRun`
    pub set_run: unsafe extern "C" fn(i32),
    /// `gPlatSimAfterTick` to the driver's after-tick (true) or none
    pub set_after_tick: unsafe extern "C" fn(bool),
    /// `gPlatSimMode` on and the switches
    pub set_switches: unsafe extern "C" fn(*const EeSimDriverSwitches),
    /// `gPlatSimNoHumanDefeatHits`; a negative argument reads, 0 clears
    pub no_human_defeat_hits: unsafe extern "C" fn(c_long) -> c_long,
    /// `gUserInterface->PostCancelGame()`
    pub cancel_game: unsafe extern "C" fn(),
    /// `EESimEditorStep`: one editor script line's command (its words) run as a step; its output lines pushed -> 0,
    /// 1 not a command, 2 not finished within a minute
    pub editor_step: unsafe extern "C" fn(*const *const c_char, usize, *mut c_void, EeSimDriverPushFn) -> i32,
    /// --flare: the human's flare at the player's first building, told to its allies
    pub flare: unsafe extern "C" fn(c_ulong, *mut EeSimDriverSpot),
    /// --volcano: a volcano cast 6 tiles off the player's first building
    pub volcano: unsafe extern "C" fn(c_ulong, *mut EeSimDriverSpot),
    /// the volcano boulders of every player's units: their ids, pushed as decimal text
    pub volcano_boulders: unsafe extern "C" fn(*mut c_void, EeSimDriverPushFn),
    /// `EESimCaptureRecord::Active()`
    pub capture_active: unsafe extern "C" fn() -> bool,
    /// a sweep or capture hook (`EE_SIM_DRIVER_SWEEP_*`)
    pub sweep: unsafe extern "C" fn(i32, c_long),
    /// `EERulesAddOverride`: true, else the problem into the buffer
    pub rules_add: unsafe extern "C" fn(*const c_char, *mut c_char, usize) -> bool,
    /// `EERulesOverrideProblems`
    pub rules_problems: unsafe extern "C" fn(*mut c_void, EeSimDriverPushFn),
    /// `EECPChoiceByName` (-1: none)
    pub engine_choice: unsafe extern "C" fn(*const c_char) -> c_long,
    /// the AI manifest's ids
    pub engine_ids: unsafe extern "C" fn(*mut c_void, EeSimDriverPushFn),
    /// the C library's environment (`_putenv_s` / `setenv`)
    pub set_env: unsafe extern "C" fn(*const c_char, *const c_char),
    /// a duplicate of the process's stderr descriptor
    pub dup_stderr: unsafe extern "C" fn() -> i32,
    /// stderr (descriptor 2) from the file (opened for writing) or, for NULL, back to the descriptor
    pub set_stderr: unsafe extern "C" fn(*const c_char, i32),
    /// bytes to the C library's stdout (1) or stderr (2), or to a descriptor (3 + fd); flushed when asked (stderr
    /// always)
    pub write: unsafe extern "C" fn(i32, *const u8, usize, bool),
    /// --rng-trace: from game ms `ms` into the file
    pub rng_trace: unsafe extern "C" fn(c_long, *const c_char),
    /// --dump-tick: the physics' after-tick hook to the driver's
    pub tick_hook: unsafe extern "C" fn(),
    /// the C library's streams and the capture flushed, then the process ended with the code
    pub quit: unsafe extern "C" fn(i32),
    /// the world's sample reads the views cannot make: the player's wonder count and a unit class's cost (the tech
    /// node of `technology`, the class's unit type), every resource summed
    pub wonders: unsafe extern "C" fn(u64) -> c_long,
    pub unit_value: unsafe extern "C" fn(u64, c_long, c_ulong) -> c_long,
}

/// The game as the driver sees it.
pub trait Game: Send + Sync {
    fn top_form(&self) -> u32;
    fn top_form_class(&self) -> String;
    fn click(&self, form: u32, control: u32) -> i32;
    fn combo_lines(&self, control: c_long) -> Option<(Vec<String>, c_long)>;
    fn combo_select(&self, control: c_long, index: c_ulong) -> bool;
    fn list_lines(&self, which: i32) -> Option<(Vec<String>, c_ulong)>;
    fn list_select(&self, which: i32, index: c_ulong) -> bool;
    fn control_with_event(&self, form: u32, code: u32) -> c_long;
    fn physics(&self) -> EeSimDriverPhysics;
    fn editor_up(&self) -> bool;
    fn lock(&self, op: i32);
    fn prepare_human(&self, human_out: bool);
    fn dump_after_load(&self, path: &str);
    fn checksum_fields(&self) -> String;
    fn dump_state(&self, path: &str);
    fn request_save(&self, name: &str);
    fn extension(&self, which: i32) -> String;
    fn set_run(&self, run: bool);
    fn set_after_tick(&self, on: bool);
    fn set_switches(&self, s: &EeSimDriverSwitches);
    fn no_human_defeat_hits(&self, op: c_long) -> c_long;
    fn cancel_game(&self);
    fn editor_step(&self, words: &[String]) -> super::editor::Step;
    fn flare(&self, player: c_ulong) -> EeSimDriverSpot;
    fn volcano(&self, player: c_ulong) -> EeSimDriverSpot;
    fn volcano_boulders(&self) -> Vec<c_ulong>;
    fn capture_active(&self) -> bool;
    fn sweep(&self, kind: i32, arg: c_long);
    fn rules_add(&self, text: &str) -> Result<(), String>;
    fn rules_problems(&self) -> Vec<String>;
    fn engine_choice(&self, name: &str) -> c_long;
    fn engine_ids(&self) -> Vec<String>;
    fn set_env(&self, name: &str, value: &str);
    fn dup_stderr(&self) -> i32;
    fn set_stderr(&self, file: Option<&str>, fd: i32);
    fn write(&self, stream: i32, bytes: &[u8], flush: bool);
    fn rng_trace(&self, ms: c_long, path: &str);
    fn tick_hook(&self);
    fn quit(&self, code: i32) -> !;
    fn wonders(&self, player: u64) -> c_long;
    fn unit_value(&self, player: u64, technology: c_long, unit_type: c_ulong) -> c_long;
}

unsafe extern "C" fn push_line(sink: *mut c_void, text: *const c_char) {
    // SAFETY: the sink is the Vec<String> the caller passed; the text is NUL ended or NULL
    let v = unsafe { &mut *(sink as *mut Vec<String>) };
    v.push(if text.is_null() { String::new() } else { unsafe { CStr::from_ptr(text) }.to_string_lossy().into_owned() });
}

fn c(text: &str) -> CString {
    CString::new(text.replace('\0', "")).expect("no NUL")
}

fn text_of(f: impl Fn(*mut c_char, usize) -> usize) -> String {
    let mut buffer = vec![0 as c_char; 512];
    let n = f(buffer.as_mut_ptr(), buffer.len());
    if n >= buffer.len() {
        buffer = vec![0 as c_char; n + 1];
        f(buffer.as_mut_ptr(), buffer.len());
    }
    // SAFETY: the host wrote a NUL-ended text within the buffer
    unsafe { CStr::from_ptr(buffer.as_ptr()) }.to_string_lossy().into_owned()
}

/// The C++ host's table as a [`Game`].
pub struct CppGame(pub EeSimDriverHost);

// SAFETY: the C++ functions are callable from any thread (each takes the locks it needs)
unsafe impl Send for CppGame {}
// SAFETY: as above
unsafe impl Sync for CppGame {}

// SAFETY (every call below): the C++ table's functions with live, NUL-ended texts, live out values and sinks of
// the type push_line takes
impl Game for CppGame {
    fn top_form(&self) -> u32 {
        unsafe { (self.0.top_form)() }
    }
    fn top_form_class(&self) -> String {
        text_of(|b, n| unsafe { (self.0.top_form_class)(b, n) })
    }
    fn click(&self, form: u32, control: u32) -> i32 {
        unsafe { (self.0.click)(form, control) }
    }
    fn combo_lines(&self, control: c_long) -> Option<(Vec<String>, c_long)> {
        let mut lines: Vec<String> = Vec::new();
        let mut selection: c_long = -1;
        let ok = unsafe {
            (self.0.combo_lines)(control, &mut lines as *mut Vec<String> as *mut c_void, push_line, &mut selection)
        };
        ok.then_some((lines, selection))
    }
    fn combo_select(&self, control: c_long, index: c_ulong) -> bool {
        unsafe { (self.0.combo_select)(control, index) }
    }
    fn list_lines(&self, which: i32) -> Option<(Vec<String>, c_ulong)> {
        let mut lines: Vec<String> = Vec::new();
        let mut selection: c_ulong = c_ulong::MAX;
        let ok = unsafe {
            (self.0.list_lines)(which, &mut lines as *mut Vec<String> as *mut c_void, push_line, &mut selection)
        };
        ok.then_some((lines, selection))
    }
    fn list_select(&self, which: i32, index: c_ulong) -> bool {
        unsafe { (self.0.list_select)(which, index) }
    }
    fn control_with_event(&self, form: u32, code: u32) -> c_long {
        unsafe { (self.0.control_with_event)(form, code) }
    }
    fn physics(&self) -> EeSimDriverPhysics {
        let mut p = EeSimDriverPhysics::default();
        unsafe { (self.0.physics)(&mut p) };
        p
    }
    fn editor_up(&self) -> bool {
        unsafe { (self.0.editor_up)() }
    }
    fn lock(&self, op: i32) {
        unsafe { (self.0.lock)(op) }
    }
    fn prepare_human(&self, human_out: bool) {
        unsafe { (self.0.prepare_human)(human_out) }
    }
    fn dump_after_load(&self, path: &str) {
        unsafe { (self.0.dump_after_load)(c(path).as_ptr()) }
    }
    fn checksum_fields(&self) -> String {
        text_of(|b, n| unsafe { (self.0.checksum_fields)(b, n) })
    }
    fn dump_state(&self, path: &str) {
        unsafe { (self.0.dump_state)(c(path).as_ptr()) }
    }
    fn request_save(&self, name: &str) {
        unsafe { (self.0.request_save)(c(name).as_ptr()) }
    }
    fn extension(&self, which: i32) -> String {
        text_of(|b, n| unsafe { (self.0.extension)(which, b, n) })
    }
    fn set_run(&self, run: bool) {
        unsafe { (self.0.set_run)(i32::from(run)) }
    }
    fn set_after_tick(&self, on: bool) {
        unsafe { (self.0.set_after_tick)(on) }
    }
    fn set_switches(&self, s: &EeSimDriverSwitches) {
        unsafe { (self.0.set_switches)(s) }
    }
    fn no_human_defeat_hits(&self, op: c_long) -> c_long {
        unsafe { (self.0.no_human_defeat_hits)(op) }
    }
    fn cancel_game(&self) {
        unsafe { (self.0.cancel_game)() }
    }
    fn editor_step(&self, words: &[String]) -> super::editor::Step {
        let texts: Vec<CString> = words.iter().map(|w| c(w)).collect();
        let pointers: Vec<*const c_char> = texts.iter().map(|t| t.as_ptr()).collect();
        let mut lines: Vec<String> = Vec::new();
        let code = unsafe {
            (self.0.editor_step)(
                pointers.as_ptr(),
                pointers.len(),
                &mut lines as *mut Vec<String> as *mut c_void,
                push_line,
            )
        };
        match code {
            0 => super::editor::Step::Done(lines),
            1 => super::editor::Step::Unknown,
            _ => super::editor::Step::Unfinished,
        }
    }
    fn flare(&self, player: c_ulong) -> EeSimDriverSpot {
        let mut s = EeSimDriverSpot::default();
        unsafe { (self.0.flare)(player, &mut s) };
        s
    }
    fn volcano(&self, player: c_ulong) -> EeSimDriverSpot {
        let mut s = EeSimDriverSpot::default();
        unsafe { (self.0.volcano)(player, &mut s) };
        s
    }
    fn volcano_boulders(&self) -> Vec<c_ulong> {
        let mut lines: Vec<String> = Vec::new();
        unsafe { (self.0.volcano_boulders)(&mut lines as *mut Vec<String> as *mut c_void, push_line) };
        lines.iter().filter_map(|l| l.parse().ok()).collect()
    }
    fn capture_active(&self) -> bool {
        unsafe { (self.0.capture_active)() }
    }
    fn sweep(&self, kind: i32, arg: c_long) {
        unsafe { (self.0.sweep)(kind, arg) }
    }
    fn rules_add(&self, text: &str) -> Result<(), String> {
        let mut buffer = vec![0 as c_char; 2048];
        if unsafe { (self.0.rules_add)(c(text).as_ptr(), buffer.as_mut_ptr(), buffer.len()) } {
            return Ok(());
        }
        Err(unsafe { CStr::from_ptr(buffer.as_ptr()) }.to_string_lossy().into_owned())
    }
    fn rules_problems(&self) -> Vec<String> {
        let mut lines: Vec<String> = Vec::new();
        unsafe { (self.0.rules_problems)(&mut lines as *mut Vec<String> as *mut c_void, push_line) };
        lines
    }
    fn engine_choice(&self, name: &str) -> c_long {
        unsafe { (self.0.engine_choice)(c(name).as_ptr()) }
    }
    fn engine_ids(&self) -> Vec<String> {
        let mut lines: Vec<String> = Vec::new();
        unsafe { (self.0.engine_ids)(&mut lines as *mut Vec<String> as *mut c_void, push_line) };
        lines
    }
    fn set_env(&self, name: &str, value: &str) {
        unsafe { (self.0.set_env)(c(name).as_ptr(), c(value).as_ptr()) }
    }
    fn dup_stderr(&self) -> i32 {
        unsafe { (self.0.dup_stderr)() }
    }
    fn set_stderr(&self, file: Option<&str>, fd: i32) {
        let file = file.map(c);
        unsafe { (self.0.set_stderr)(file.as_ref().map_or(std::ptr::null(), |f| f.as_ptr()), fd) }
    }
    fn write(&self, stream: i32, bytes: &[u8], flush: bool) {
        unsafe { (self.0.write)(stream, bytes.as_ptr(), bytes.len(), flush) }
    }
    fn rng_trace(&self, ms: c_long, path: &str) {
        unsafe { (self.0.rng_trace)(ms, c(path).as_ptr()) }
    }
    fn tick_hook(&self) {
        unsafe { (self.0.tick_hook)() }
    }
    fn quit(&self, code: i32) -> ! {
        unsafe { (self.0.quit)(code) };
        std::process::exit(code)
    }
    fn wonders(&self, player: u64) -> c_long {
        unsafe { (self.0.wonders)(player) }
    }
    fn unit_value(&self, player: u64, technology: c_long, unit_type: c_ulong) -> c_long {
        unsafe { (self.0.unit_value)(player, technology, unit_type) }
    }
}
