//! The driver's C ABI (rust/game/sim_driver.h), called by Empire Earth/EESim.cpp (TEST ONLY).

use std::ffi::CStr;
use std::io::BufReader;
use std::os::raw::{c_char, c_int, c_long};
use std::time::Instant;

use super::driver::{self, EeSimDriverEvents};
use super::host::{CppGame, Game};
use super::options::{self, Command, Options, Process};

/// The driver's answer to [`ee_sim_driver_main`] when the game goes on (the game's own entry next).
pub const EE_SIM_DRIVER_CONTINUE: c_int = -1;

/// EE-SIM's entry: the command line parsed, the driver made over the host, the output files opened, the switches
/// and the environment set -> [`EE_SIM_DRIVER_CONTINUE`], or the exit code (0 after --help, 64 a bad command
/// line, 66 a file that cannot be opened).
///
/// # Safety
/// `argv` holds `argc` NUL-ended texts.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_main(argc: c_int, argv: *const *const c_char) -> c_int {
    let start = Instant::now();
    let (game, events) = (CppGame, EeSimDriverEvents::game());
    let args: Vec<String> = (1..argc.max(1) as usize)
        // SAFETY: argv's entries are NUL-ended texts
        .map(|i| unsafe { CStr::from_ptr(*argv.add(i)) }.to_string_lossy().into_owned())
        .collect();
    let mut o = Options::default();
    let mut p = Process::default();
    let error = |text: String| game.write(2, format!("EE-SIM: {text}\n").as_bytes(), true);
    match options::parse_command_line(&args, &mut o, &mut p, &mut |v| game.rules_add(v), &mut |d| {
        let _ = std::fs::create_dir(d);
    }) {
        Command::Run => {}
        Command::Help => {
            game.write(1, options::USAGE.as_bytes(), true);
            return 0;
        }
        Command::Bad(e, code) => {
            error(e);
            return code;
        }
    }
    let games: Option<Box<dyn std::io::BufRead + Send>> = if p.games.is_empty() {
        None
    } else if p.games == "-" {
        Some(Box::new(BufReader::new(std::io::stdin())))
    } else {
        match std::fs::File::open(&p.games) {
            Ok(f) => Some(Box::new(BufReader::new(f))),
            Err(_) => {
                error(format!("cannot read --games {}", p.games));
                return 66;
            }
        }
    };
    let (data, config, ai_trace, games_name) = (p.data.clone(), p.config.clone(), p.ai_trace, p.games.clone());
    let d = match driver::install(Box::new(game), events, o, p, games, start) {
        Ok(d) => d,
        Err(e) => {
            CppGame.write(2, format!("EE-SIM: {e}
").as_bytes(), true);
            return 66;
        }
    };
    d.start_process(&data, &config, ai_trace, &games_name);
    // the game's debug strings to the driver; on Windows the fault report (POSIX: EESim.cpp's signal handler)
    ee_base::plat_core::sim::PlatDebugSetHandler(Some(ee_sim_driver_debug));
    #[cfg(windows)]
    // SAFETY: a NUL-ended text
    unsafe {
        super::fault::ee_sim_driver_fault_install(c"EE-SIM".as_ptr())
    };
    // Linux, where a bench machine has no display: SDL's dummy video driver (the hidden window needs none, and
    // nothing is presented; macOS keeps Cocoa: its windows are Metal windows, which the dummy driver refuses). The
    // GPU device the renderer still creates is any Vulkan adapter, on a machine without a GPU Mesa's lavapipe, kept
    // to one rasterizer thread (it starts one per core, and a bench runs one simulation per core). The environment
    // wins over both.
    #[cfg(all(unix, not(target_os = "macos")))]
    for (name, value) in [("SDL_VIDEO_DRIVER", "dummy"), ("LP_NUM_THREADS", "1")] {
        if std::env::var_os(name).is_none() {
            super::stdio::set_env(name, value);
        }
    }
    EE_SIM_DRIVER_CONTINUE
}

/// The driver's thread started (EERun, after the engine and the threads are up, before the UI loop).
#[no_mangle]
pub extern "C" fn ee_sim_driver_start() {
    if let Some(d) = driver::driver() {
        std::thread::spawn(move || d.drive());
    }
}

/// After every communications tick (gPlatSimAfterTick: the physics thread, under the world's write lock).
#[no_mangle]
pub extern "C" fn ee_sim_driver_after_tick() {
    if let Some(d) = driver::driver() {
        d.after_tick();
    }
}

/// After every physics tick of the headless loop (--dump-tick; gPhysicsAfterTickHook).
#[no_mangle]
pub extern "C" fn ee_sim_driver_tick_dump() {
    if let Some(d) = driver::driver() {
        d.tick_dump();
    }
}

/// A debug string of the game (PlatDebugSetHandler).
///
/// # Safety
/// `text` is NULL or NUL ended.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_debug(text: *const c_char) {
    if text.is_null() {
        return;
    }
    if let Some(d) = driver::driver() {
        // SAFETY: the caller's contract
        d.debug(unsafe { CStr::from_ptr(text) }.to_bytes());
    }
}

/// The game's own start-up returned before the driver ended the run (it was quit, or failed to start): the report
/// and the exit.
#[no_mangle]
pub extern "C" fn ee_sim_driver_game_ended(result: c_int) {
    if let Some(d) = driver::driver() {
        d.write_report(4, "the game ended before the simulation (start-up failed?)");
        d.quit(if result != 0 { result } else { 4 });
    }
}

/// The PTs per second and the autosave period the run's settings take (EESimPreferences).
///
/// # Safety
/// The out values are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_driver_preferences(pt_rate: *mut c_long, autosave_ms: *mut c_long) {
    let (pt, autosave) = driver::driver().map_or((40, 0), |d| d.preferences());
    // SAFETY: the caller's contract
    unsafe {
        *pt_rate = pt;
        *autosave_ms = autosave;
    }
}
