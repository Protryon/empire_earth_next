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
    /// `EE_SIM_DRIVER_*_LOCK` on `gWorld.mLock`
    pub lock: unsafe extern "C" fn(i32),
    /// `EEAfterLoad`: the state dump of the file at the loaded game's first tick
    pub dump_after_load: unsafe extern "C" fn(*const c_char),
    /// `gSavedGameExtension` (0) or `gScenarioExtension` (1)
    pub extension: unsafe extern "C" fn(i32, *mut c_char, usize) -> usize,
    /// `gUserInterface->PostCancelGame()`
    pub cancel_game: unsafe extern "C" fn(),
    /// `EECPChoiceByName` (-1: none)
    pub engine_choice: unsafe extern "C" fn(*const c_char) -> c_long,
    /// the world's sample reads the views cannot make: the player's wonder count and a unit class's cost (the tech
    /// node of `technology`, the class's unit type), every resource summed
    pub wonders: unsafe extern "C" fn(u64) -> c_long,
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
        super::sim_host::top_form()
    }
    fn top_form_class(&self) -> String {
        super::sim_host::top_form_class()
    }
    fn click(&self, form: u32, control: u32) -> i32 {
        super::sim_host::click(form, control)
    }
    fn combo_lines(&self, control: c_long) -> Option<(Vec<String>, c_long)> {
        super::sim_host::combo_lines(control)
    }
    fn combo_select(&self, control: c_long, index: c_ulong) -> bool {
        super::sim_host::combo_select(control, index)
    }
    fn list_lines(&self, which: i32) -> Option<(Vec<String>, c_ulong)> {
        super::sim_host::list_lines(which)
    }
    fn list_select(&self, which: i32, index: c_ulong) -> bool {
        super::sim_host::list_select(which, index)
    }
    fn control_with_event(&self, form: u32, code: u32) -> c_long {
        super::sim_host::control_with_event(form, code)
    }
    fn physics(&self) -> EeSimDriverPhysics {
        super::sim_host::physics()
    }
    fn editor_up(&self) -> bool {
        super::sim_host::editor_up()
    }
    fn lock(&self, op: i32) {
        unsafe { (self.0.lock)(op) }
    }
    fn prepare_human(&self, human_out: bool) {
        super::sim_host::prepare_human(human_out)
    }
    fn dump_after_load(&self, path: &str) {
        unsafe { (self.0.dump_after_load)(c(path).as_ptr()) }
    }
    fn checksum_fields(&self) -> String {
        super::dump::checksum_text()
    }
    fn dump_state(&self, path: &str) {
        super::dump::dump_state(path)
    }
    fn request_save(&self, name: &str) {
        super::sim_host::request_save(name)
    }
    fn extension(&self, which: i32) -> String {
        text_of(|b, n| unsafe { (self.0.extension)(which, b, n) })
    }
    fn set_run(&self, run: bool) {
        #[cfg(not(test))]
        ee_base::plat_core::sim::gPlatSimRun.set(i32::from(run));
        #[cfg(test)]
        let _ = run;
    }
    fn set_after_tick(&self, on: bool) {
        // the driver's after-tick on the communications ticks, or none
        #[cfg(not(test))]
        ee_base::plat_core::sim::gPlatSimAfterTick.set(if on {
            Some(super::ffi::ee_sim_driver_after_tick)
        } else {
            None
        });
        #[cfg(test)]
        let _ = on;
    }
    fn set_switches(&self, s: &EeSimDriverSwitches) {
        super::sim_host::set_switches(s)
    }
    fn no_human_defeat_hits(&self, op: c_long) -> c_long {
        super::sim_host::no_human_defeat_hits(op)
    }
    fn cancel_game(&self) {
        unsafe { (self.0.cancel_game)() }
    }
    fn editor_step(&self, words: &[String]) -> super::editor::Step {
        super::sed_steps::step(words)
    }
    fn flare(&self, player: c_ulong) -> EeSimDriverSpot {
        super::sim_host::flare(player)
    }
    fn volcano(&self, player: c_ulong) -> EeSimDriverSpot {
        super::sim_host::volcano(player)
    }
    fn volcano_boulders(&self) -> Vec<c_ulong> {
        super::sim_host::volcano_boulders()
    }
    fn capture_active(&self) -> bool {
        super::sim_ops::h(super::sim_ops::EeSimDriverSim::CaptureActive, 0, &[]) != 0
    }
    fn sweep(&self, kind: i32, arg: c_long) {
        super::sim_host::sweep(kind, arg)
    }
    fn rules_add(&self, text: &str) -> Result<(), String> {
        super::sim_host::rules_add(text)
    }
    fn rules_problems(&self) -> Vec<String> {
        super::sim_host::rules_problems()
    }
    fn engine_choice(&self, name: &str) -> c_long {
        unsafe { (self.0.engine_choice)(c(name).as_ptr()) }
    }
    fn engine_ids(&self) -> Vec<String> {
        super::sim_host::engine_ids()
    }
    fn set_env(&self, name: &str, value: &str) {
        super::stdio::set_env(name, value)
    }
    fn dup_stderr(&self) -> i32 {
        super::stdio::dup_stderr()
    }
    fn set_stderr(&self, file: Option<&str>, fd: i32) {
        super::stdio::set_stderr(file, fd)
    }
    fn write(&self, stream: i32, bytes: &[u8], flush: bool) {
        super::stdio::write(stream, bytes, flush)
    }
    fn rng_trace(&self, ms: c_long, path: &str) {
        super::stdio::rng_trace(ms, path)
    }
    fn tick_hook(&self) {
        // --dump-tick: the physics' after-tick hook (gPhysicsAfterTickHook, gamecpp globals.rs) to the driver's
        #[cfg(not(test))]
        let hook: extern "C" fn() = super::ffi::ee_sim_driver_tick_dump;
        #[cfg(not(test))]
        // SAFETY: the hook pointer, set before the game's physics runs
        unsafe {
            *std::ptr::addr_of_mut!(crate::gamecpp::globals::gPhysicsAfterTickHook) = hook as *mut c_void
        };
    }
    fn quit(&self, code: i32) -> ! {
        super::stdio::quit(code)
    }
    fn wonders(&self, player: u64) -> c_long {
        unsafe { (self.0.wonders)(player) }
    }
    fn unit_value(&self, player: u64, technology: c_long, unit_type: c_ulong) -> c_long {
        super::sim_host::unit_value(player, technology, unit_type)
    }
}
