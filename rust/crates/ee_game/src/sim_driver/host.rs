//! What EE-SIM's driver asks of the game (TEST ONLY): the trait the driver calls, so the driver's logic runs over a
//! fake game in the unit tests, and the game's own answers ([`CppGame`]: sim_host.rs and the other modules; the C++
//! table EESim.cpp filled, its world lock, after-load dump, extensions, cancel and wonder count, is Rust's).

use std::ffi::{c_void, CString};
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

/// The game as a [`Game`] (each call takes the locks it needs).
pub struct CppGame;

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
        super::sim_host::world_lock(op)
    }
    fn prepare_human(&self, human_out: bool) {
        super::sim_host::prepare_human(human_out)
    }
    fn dump_after_load(&self, path: &str) {
        super::sim_host::dump_after_load(path)
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
        super::sim_host::extension(which).to_string()
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
        super::sim_host::cancel_game()
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
        ee_sim::app::capture::ee_app_capture_active()
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
        unsafe { crate::cp_engine::ee_cp_engine_choice_by_name(c(name).as_ptr()) as c_long }
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
        super::sim_host::wonders(player)
    }
    fn unit_value(&self, player: u64, technology: c_long, unit_type: c_ulong) -> c_long {
        super::sim_host::unit_value(player, technology, unit_type)
    }
}
