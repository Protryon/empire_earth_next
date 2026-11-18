//! EE-SIM's driver (TEST ONLY): the menus and the lobby driven as the harness drives them, the game's run on the
//! physics thread's after-tick, the samples, the end of the run, the report, and `--games`' batch of games in one
//! process. The game is reached only through [`Game`] (host.rs); the world's reads are its views.

use std::fs::File;
use std::io::{BufRead, BufWriter, Write};
use std::os::raw::{c_long, c_ulong};
use std::sync::{Condvar, Mutex, MutexGuard, OnceLock};
use std::time::{Duration, Instant};

use super::host::*;
use super::lobby::{self, PLAYER_TYPE, VISIBLE_SLOTS};
use super::options::{self, Options, Process};
use super::report::{self, Chosen, Defeat, PlayerSample, ReportInput, Sample};

/// The forms the driver goes through (their ids) and the screen events of their buttons (ee_screens' codes).
pub const FORM_MAIN: u32 = 12;
pub const FORM_SINGLE: u32 = 6;
pub const FORM_LOBBY: u32 = 26;
pub const FORM_GAME: u32 = 11;
pub const FORM_LOAD: u32 = 13;
pub const FORM_LOAD_SCENARIO: u32 = 4;
pub const FORM_PLAY_CAMPAIGN: u32 = 8;
pub const FORM_CAMPAIGN_SELECT: u32 = 33;
pub const FORM_SCENARIO_INTRO: u32 = 19;
pub const FORM_EDITORS: u32 = 9;

/// The screen events the driver clicks (EE_SCREENS_*, rust/game/screens.h), passed in by the C++ (the codes are
/// ee_screens' own).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeSimDriverEvents {
    pub sp_load_game: u32,
    pub spl_load: u32,
    pub pc_learning: u32,
    pub sp_campaign: u32,
    pub pc_campaign: [u32; 3],
    pub cs_start: u32,
    pub si_start: u32,
    pub editors_open: u32,
    pub editors_scenario_editor: u32,
    pub pgs_exit: u32,
    /// the post-game summary's form id
    pub form_post_game_summary: u32,
    /// the play scenario screen's Load button (kLoadGameLoadButton)
    pub load_scenario_button: u32,
}

const LOAD_NAME: &str = "EE-SIM load";
const SCENARIO_NAME: &str = "EE-SIM scenario";

#[derive(PartialEq, Clone, Copy)]
enum OutState {
    Preamble,
    Game,
    Between,
}

/// A file in the C library's text mode (`fopen(.., "w")`, the C++ driver's): each new line written as CR LF on
/// Windows.
pub struct TextFile(BufWriter<File>);

impl TextFile {
    /// The file made (emptied) for writing.
    pub fn create(path: &str) -> std::io::Result<TextFile> {
        File::create(path).map(|f| TextFile(BufWriter::new(f)))
    }
    /// The bytes, each new line as CR LF on Windows.
    pub fn write_all(&mut self, bytes: &[u8]) -> std::io::Result<()> {
        if cfg!(windows) {
            for part in bytes.split_inclusive(|&b| b == b'\n') {
                match part.strip_suffix(b"\n") {
                    Some(line) => {
                        self.0.write_all(line)?;
                        self.0.write_all(b"\r\n")?;
                    }
                    None => self.0.write_all(part)?,
                }
            }
            Ok(())
        } else {
            self.0.write_all(bytes)
        }
    }
    pub fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}

struct Out {
    trace: Option<TextFile>,
    log: Option<TextFile>,
    checksum: Option<TextFile>,
    state: OutState,
    preamble_trace: Vec<u8>,
    preamble_log: Vec<u8>,
    /// the game's load failure message ("" none)
    load_failure: String,
    /// --games: the process's stderr while a game's own is in place (-1 none)
    process_stderr: i32,
}

#[derive(Default)]
struct Run {
    finished: bool,
    outcome: String,
    samples: Vec<Sample>,
    defeats: Vec<Defeat>,
    results: Vec<c_ulong>,
    epoch_ms: Vec<Vec<c_long>>,
    next_sample_ms: c_long,
    standing_since: c_long,
    next_save: usize,
    next_dump: usize,
    volcano_cast_ms: c_long,
    volcano_second: c_long,
    volcano_shots: c_ulong,
    volcano_total: c_ulong,
    volcano_seen: Vec<c_ulong>,
}

/// The after-tick's schedules, kept over a batch's games (the C++ driver's function statics).
struct Ticks {
    next_sweep_ms: c_long,
    message_sweep_done: bool,
    next_cast_ms: c_long,
    casts: c_long,
    casts_aligned: bool,
    next_unit_sweep_ms: c_long,
    unit_sweeps: c_long,
    next_minimap_ms: c_long,
    app_sweeps: i32,
    swept: bool,
    checksum_lines: c_ulong,
    checksum_last_pt: c_ulong,
    next_tick_dump: usize,
}

struct Times {
    start: Instant,
    phase: Instant,
    game: Option<Instant>,
    phases: Vec<(String, f64)>,
}

/// The driver's state.
pub struct Driver {
    game: Box<dyn Game>,
    events: EeSimDriverEvents,
    options: Mutex<Options>,
    base: Options,
    process: Process,
    batch: bool,
    games: Mutex<Option<Box<dyn BufRead + Send>>>,
    out: Mutex<Out>,
    run: Mutex<Run>,
    done: Condvar,
    ticks: Mutex<Ticks>,
    times: Mutex<Times>,
    chosen: Mutex<Vec<Chosen>>,
    left_game: Mutex<bool>,
    game_index: Mutex<usize>,
}

static DRIVER: OnceLock<Driver> = OnceLock::new();

/// The driver (after [`install`]).
pub fn driver() -> Option<&'static Driver> {
    DRIVER.get()
}

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

fn sleep_ms(ms: u64) {
    std::thread::sleep(Duration::from_millis(ms));
}

fn new_run() -> Run {
    Run { standing_since: -1, volcano_cast_ms: -1, ..Run::default() }
}

/// The driver made from its parsed options, its output files opened.
pub fn install(
    game: Box<dyn Game>,
    events: EeSimDriverEvents,
    options: Options,
    process: Process,
    games: Option<Box<dyn BufRead + Send>>,
    start: Instant,
) -> Result<&'static Driver, String> {
    let batch = games.is_some();
    let open = TextFile::create;
    let mut out = Out {
        trace: None,
        log: None,
        checksum: None,
        state: OutState::Preamble,
        preamble_trace: Vec::new(),
        preamble_log: Vec::new(),
        load_failure: String::new(),
        process_stderr: -1,
    };
    if !batch && !options.trace.is_empty() {
        out.trace = Some(open(&options.trace).map_err(|_| format!("cannot write {}", options.trace))?);
    }
    if !batch && !options.log.is_empty() {
        out.log = open(&options.log).ok();
    }
    if !options.checksum_trace.is_empty() {
        out.checksum =
            Some(open(&options.checksum_trace).map_err(|_| format!("cannot write {}", options.checksum_trace))?);
    }
    if batch {
        out.process_stderr = game.dup_stderr();
    }
    let d = Driver {
        game,
        events,
        base: options.clone(),
        options: Mutex::new(options),
        process,
        batch,
        games: Mutex::new(games),
        out: Mutex::new(out),
        run: Mutex::new(new_run()),
        done: Condvar::new(),
        ticks: Mutex::new(Ticks {
            next_sweep_ms: 300000,
            message_sweep_done: false,
            next_cast_ms: 300000,
            casts: 0,
            casts_aligned: false,
            next_unit_sweep_ms: 300000,
            unit_sweeps: 0,
            next_minimap_ms: 60000,
            app_sweeps: 0,
            swept: false,
            checksum_lines: 0,
            checksum_last_pt: 0,
            next_tick_dump: 0,
        }),
        times: Mutex::new(Times { start, phase: start, game: None, phases: Vec::new() }),
        chosen: Mutex::new(Vec::new()),
        left_game: Mutex::new(false),
        game_index: Mutex::new(0),
    };
    DRIVER.set(d).map_err(|_| "the driver is installed twice".to_string())?;
    Ok(DRIVER.get().expect("set"))
}

/// A line's bytes with a new line after it unless it ends in one.
fn with_new_line(text: &[u8]) -> Vec<u8> {
    let mut v = text.to_vec();
    if v.last() != Some(&b'\n') {
        v.push(b'\n');
    }
    v
}

impl Driver {
    fn opt(&self) -> MutexGuard<'_, Options> {
        lock(&self.options)
    }

    // ------------------------------------------------------------------------------------------------ output

    /// A progress line on stderr ("[sim  12.3] ..."), unless --quiet.
    pub fn note(&self, text: &str) {
        if self.opt().quiet {
            return;
        }
        let seconds = lock(&self.times).start.elapsed().as_secs_f64();
        let line = format!("[sim {seconds:7.1}] {text}\n");
        let _out = lock(&self.out);
        self.game.write(2, line.as_bytes(), true);
    }

    /// A line on the process's stderr (the batch's markers), whatever the game's stderr is.
    pub fn marker(&self, text: &str) {
        let line = format!("{text}\n");
        let out = lock(&self.out);
        if out.process_stderr >= 0 {
            self.game.write(3 + out.process_stderr, line.as_bytes(), false);
            return;
        }
        self.game.write(2, line.as_bytes(), true);
    }

    /// A debug string of the game: EE-AI lines to the trace (stdout without one), everything to --log; before a
    /// batch's first game kept for every game's head, between games dropped.
    pub fn debug(&self, text: &[u8]) {
        let mut out = lock(&self.out);
        if text.starts_with(b"EE load failure: ") && out.load_failure.is_empty() {
            let mut f = String::from_utf8_lossy(&text[17..]).into_owned();
            while f.ends_with('\n') || f.ends_with('\r') {
                f.pop();
            }
            out.load_failure = if f.is_empty() { "(no message)".into() } else { f };
        }
        if self.batch && out.state != OutState::Game {
            if out.state == OutState::Preamble {
                if text.starts_with(b"EE-AI") {
                    let mut line = b"debugstring ".to_vec();
                    line.extend(with_new_line(text));
                    out.preamble_trace.extend(line);
                }
                let line = with_new_line(text);
                out.preamble_log.extend(line);
            }
            return;
        }
        if text.starts_with(b"EE-AI") {
            let mut line = b"debugstring ".to_vec();
            line.extend(with_new_line(text));
            match out.trace.as_mut() {
                Some(f) => {
                    let _ = f.write_all(&line);
                }
                None => self.game.write(1, &line, false),
            }
        }
        if let Some(f) = out.log.as_mut() {
            let _ = f.write_all(&with_new_line(text));
        }
    }

    fn flush_files(&self) {
        let mut guard = lock(&self.out);
        let out = &mut *guard;
        for f in [out.trace.as_mut(), out.log.as_mut(), out.checksum.as_mut()].into_iter().flatten() {
            let _ = f.flush();
        }
    }

    /// The end of the process: the files flushed, then the game's streams and the exit.
    pub fn quit(&self, code: i32) -> ! {
        self.flush_files();
        self.game.quit(code)
    }

    fn phase(&self, name: &str) {
        let mut t = lock(&self.times);
        let now = Instant::now();
        let seconds = (now - t.phase).as_secs_f64();
        t.phases.push((name.to_string(), seconds));
        t.phase = now;
    }

    // ------------------------------------------------------------------------------------------------ the report

    /// The report to --report (through a temporary file) or stdout.
    pub fn write_report(&self, code: i32, error: &str) {
        let options = self.opt().clone();
        let chosen = lock(&self.chosen).clone();
        let (wall, wall_game, phases) = {
            let t = lock(&self.times);
            (t.start.elapsed().as_secs_f64(), t.game.map(|g| g.elapsed().as_secs_f64()), t.phases.clone())
        };
        let hits = self.game.no_human_defeat_hits(-1);
        let text = {
            let run = lock(&self.run);
            report::report(&ReportInput {
                options: &options,
                chosen: &chosen,
                outcome: &run.outcome,
                error,
                wall_seconds: wall,
                wall_seconds_game: wall_game,
                phases: &phases,
                no_human_defeat_hits: hits,
                samples: &run.samples,
                defeats: &run.defeats,
                epoch_ms: &run.epoch_ms,
                exit_code: code,
            })
        };
        if options.report.is_empty() || options.report == "-" {
            let _out = lock(&self.out);
            self.game.write(1, text.as_bytes(), true);
        } else {
            let temporary = format!("{}.tmp", options.report);
            if std::fs::write(&temporary, text.as_bytes()).is_ok() {
                let _ = std::fs::remove_file(&options.report);
                let _ = std::fs::rename(&temporary, &options.report);
            }
        }
    }

    /// A failed run: the report with the error, the batch's end marker, then the exit (in a batch too: the
    /// process's state is unknown; tools/sim.py plays the rest in a new one).
    pub fn fail(&self, error: &str) -> ! {
        self.game.set_run(false);
        self.note(&format!("FAILED: {error}"));
        self.write_report(2, error);
        self.end_game_output(2);
        self.quit(2)
    }

    // ------------------------------------------------------------------------------------------------ the samples

    /// The world's sample (under the world lock: the physics thread's, or the driver's read lock).
    fn take_sample(&self) -> Sample {
        let p = self.game.physics();
        let wall = lock(&self.times).game.map_or(0.0, |g| g.elapsed().as_secs_f64());
        let mut s = Sample { game_ms: p.game_ms, pt: p.pt, wall, players: Vec::new() };
        // SAFETY: the caller holds the world (the physics thread inside its after-tick, or the read lock)
        let t = unsafe { ee_sim::world::WorldRead::begin() };
        let Some(w) = ee_sim::world::WorldView::global(&t) else { return s };
        let count = w.num_players() as usize;
        let current = w.current_player_index();
        for i in 1..count {
            let Some(player) = w.player(i) else { continue };
            s.players.push(sample_player(&*self.game, &player, i, current));
        }
        s
    }

    // ------------------------------------------------------------------------------------------------ the tick

    fn finish(&self, outcome: &str) {
        self.game.set_run(false);
        let mut run = lock(&self.run);
        if run.finished {
            return;
        }
        run.outcome = outcome.to_string();
        run.finished = true;
        self.done.notify_all();
    }

    /// The end of a game in the tick hook (the physics thread): the capture's end sweeps first, once.
    fn finish_game(&self, outcome: &str) {
        let sweep = {
            let mut ticks = lock(&self.ticks);
            let sweep = self.game.capture_active() && !ticks.swept;
            ticks.swept |= sweep;
            sweep
        };
        let arg = if sweep {
            let o = self.opt();
            1 + 2 * c_long::from(o.effect_sweep)
                + 4 * c_long::from(o.group_sweep)
                + 8 * c_long::from(o.app_sweep)
                + 16 * c_long::from(o.missile_sweep)
        } else {
            0
        };
        self.game.sweep(EE_SIM_DRIVER_SWEEP_END, arg);
        self.finish(outcome);
    }

    /// --checksum-trace: one line per communications tick that ran physics ticks.
    fn checksum_line(&self) {
        let p = self.game.physics();
        let mut ticks = lock(&self.ticks);
        if lock(&self.out).checksum.is_none() || (ticks.checksum_lines > 0 && p.pt == ticks.checksum_last_pt) {
            return;
        }
        ticks.checksum_last_pt = p.pt;
        ticks.checksum_lines += 1;
        let line = format!("CT {} {}", ticks.checksum_lines, self.game.checksum_fields());
        if let Some(f) = lock(&self.out).checksum.as_mut() {
            let _ = f.write_all(line.as_bytes());
        }
    }

    fn save_name(k: usize) -> String {
        format!("EE-SIM save {k}")
    }

    fn save_path(&self, name: &str) -> String {
        format!("data\\Saved Games\\{name}{}", self.game.extension(0))
    }

    /// --dump-tick: the state dumps after the first physics tick at or after their game ms.
    pub fn tick_dump(&self) {
        let ms = self.game.physics().game_ms;
        loop {
            let path = {
                let mut ticks = lock(&self.ticks);
                match self.process.tick_dumps.get(ticks.next_tick_dump) {
                    Some((at, path)) if ms >= *at => {
                        ticks.next_tick_dump += 1;
                        path.clone()
                    }
                    _ => return,
                }
            };
            self.game.dump_state(&path);
        }
    }

    /// After every communications tick, on the physics thread, under the world's write lock.
    pub fn after_tick(&self) {
        self.game.sweep(EE_SIM_DRIVER_SWEEP_DIALOG, 0);
        self.checksum_line();
        let physics = self.game.physics();
        let time = physics.game_ms;
        let options = self.opt().clone();
        // --save-at: the save requests, as EEPhysics::AutoSave sends one (quiet, no autosave trimmed)
        loop {
            let k = {
                let mut run = lock(&self.run);
                if run.next_save >= options.saves.len() || time < options.saves[run.next_save].0 {
                    break;
                }
                run.next_save += 1;
                run.next_save - 1
            };
            self.game.request_save(&format!("{}{}", Self::save_name(k), self.game.extension(0)));
            self.note(&format!("SAVE {k} requested at game ms {time} PT {}", physics.pt));
        }
        loop {
            let path = {
                let mut run = lock(&self.run);
                match self.process.dumps.get(run.next_dump) {
                    Some((at, path)) if time >= *at => {
                        run.next_dump += 1;
                        path.clone()
                    }
                    _ => break,
                }
            };
            self.game.dump_state(&path);
        }
        // SAFETY: the physics thread holds the world for write during its after-tick
        let t = unsafe { ee_sim::world::WorldRead::begin() };
        let world = ee_sim::world::WorldView::global(&t);
        let count = world.as_ref().map_or(0, |w| w.num_players() as usize);
        let current = world.as_ref().map_or(-1, |w| w.current_player_index());
        let player = |i: usize| world.as_ref().and_then(|w| w.player(i));
        {
            let mut notes = Vec::new();
            let mut run = lock(&self.run);
            if run.results.len() < count {
                run.results.resize(count, 0);
            }
            for i in 1..count {
                let p = player(i);
                let result: c_ulong = p.as_ref().map_or(0, |p| ee_sim::world_field!(p.saved(), mGameResult));
                let epoch = p.as_ref().and_then(|p| p.tech_tree()).map_or(-1, |t| t.current_epoch());
                if epoch >= 0 {
                    if run.epoch_ms.len() < count {
                        run.epoch_ms.resize(count, Vec::new());
                    }
                    let times = &mut run.epoch_ms[i];
                    if (times.len() as i64) <= epoch {
                        times.resize(epoch as usize + 1, -1);
                    }
                    // an advance reaches every epoch up to it (a skip counts for each)
                    for e in times.iter_mut().take(epoch as usize + 1) {
                        if *e < 0 {
                            *e = time;
                        }
                    }
                }
                if result != run.results[i] {
                    run.results[i] = result;
                    if result == 2 {
                        run.defeats.push(Defeat { player: i as c_ulong, game_ms: time, pt: physics.pt });
                    }
                    let what = match result {
                        1 => "victorious",
                        2 => "defeated",
                        _ => "?",
                    };
                    notes.push(format!("RESULT player {i} {what} at game ms {time} PT {}", physics.pt));
                }
            }
            drop(run);
            for n in notes {
                self.note(&n);
            }
        }
        let capture = self.game.capture_active();
        {
            let mut ticks = lock(&self.ticks);
            // the formation, combat and other sweeps of the simulation capture, every five game minutes
            if capture && time >= ticks.next_sweep_ms {
                ticks.next_sweep_ms += 300000;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_FIVE_MINUTES, 0);
                if !ticks.message_sweep_done {
                    ticks.message_sweep_done = true;
                    self.game.sweep(EE_SIM_DRIVER_SWEEP_MESSAGES, 0);
                }
            }
            // --calamities: every calamity class in turn, every five game minutes (a loaded game on the continued
            // game's schedule)
            if !ticks.casts_aligned {
                ticks.casts_aligned = true;
                if !options.load_save.is_empty() {
                    ticks.casts = time / 300000;
                    ticks.next_cast_ms = (ticks.casts + 1) * 300000;
                }
            }
            if options.calamities && time >= ticks.next_cast_ms {
                ticks.next_cast_ms += 300000;
                let n = ticks.casts;
                ticks.casts += 1;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_CALAMITIES, n);
            }
            if options.unit_sweep && time >= ticks.next_unit_sweep_ms {
                ticks.next_unit_sweep_ms += 300000;
                let n = ticks.unit_sweeps;
                ticks.unit_sweeps += 1;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_UNITS, n);
            }
            // the mini map's cells and the tile sight every game minute; --app-sweep's cheat lines at five game
            // minutes and storms at ten
            if capture && time >= ticks.next_minimap_ms {
                ticks.next_minimap_ms += 60000;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_MINUTE, 0);
            }
            if options.app_sweep && ticks.app_sweeps == 0 && time >= 300000 {
                ticks.app_sweeps = 1;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_CHEATS, 0);
            }
            if options.app_sweep && ticks.app_sweeps == 1 && time >= 600000 {
                ticks.app_sweeps = 2;
                self.game.sweep(EE_SIM_DRIVER_SWEEP_STORMS, 0);
            }
        }
        let due = time >= lock(&self.run).next_sample_ms;
        if due {
            let s = self.take_sample();
            let line = report::sample_line(&s);
            let pt = s.pt;
            {
                let mut run = lock(&self.run);
                run.samples.push(s);
                run.next_sample_ms += (options.sample_seconds * 1000.0) as c_long;
            }
            if !options.quiet {
                self.note(&format!("game s {} PT {pt}:{line}", time / 1000));
            }
        }
        // --flare: once, the idle human's flare at the player's first building, told to its allies
        if options.flare_ms >= 0 && time >= options.flare_ms {
            self.opt().flare_ms = -1;
            let s = if (options.flare_player as usize) < count {
                self.game.flare(options.flare_player)
            } else {
                EeSimDriverSpot::default()
            };
            if s.found {
                self.note(&format!(
                    "FLARE at {},{} (player {}) to {} allies at game ms {time}",
                    s.x, s.y, options.flare_player, s.count
                ));
            } else {
                self.note(&format!("FLARE: no building of player {}", options.flare_player));
            }
        }
        // --volcano: once, a volcano 6 tiles off the player's first building; then every game second the boulders
        if options.volcano_ms >= 0 && time >= options.volcano_ms {
            self.opt().volcano_ms = -1;
            let s = if (options.volcano_player as usize) < count {
                self.game.volcano(options.volcano_player)
            } else {
                EeSimDriverSpot::default()
            };
            if s.found {
                self.note(&format!(
                    "VOLCANO cast {} at {},{} (player {}) at game ms {time}",
                    s.count, s.x, s.y, options.volcano_player
                ));
                let mut run = lock(&self.run);
                run.volcano_cast_ms = time;
                run.volcano_second = time / 1000;
            } else {
                self.note(&format!("VOLCANO: no building of player {}", options.volcano_player));
            }
        }
        let cast = lock(&self.run).volcano_cast_ms >= 0;
        if cast {
            let boulders = self.game.volcano_boulders();
            let mut run = lock(&self.run);
            for id in boulders {
                if !run.volcano_seen.contains(&id) {
                    run.volcano_seen.push(id);
                    run.volcano_shots += 1;
                    run.volcano_total += 1;
                }
            }
            if time / 1000 > run.volcano_second {
                let line = format!(
                    "VOLCANO s {}: {} boulders launched (total {})",
                    run.volcano_second - run.volcano_cast_ms / 1000,
                    run.volcano_shots,
                    run.volcano_total
                );
                run.volcano_second = time / 1000;
                run.volcano_shots = 0;
                if run.volcano_seen.len() > 100000 {
                    run.volcano_seen.clear();
                }
                drop(run);
                self.note(&line);
            }
        }
        // the end of the run (tools/harness/ai_game.py's rules)
        if self.game.physics().game_over {
            self.finish_game("game-end");
            return;
        }
        if options.no_human_defeat {
            let mut alive = 0;
            let mut others_done = true;
            for i in 1..count {
                let Some(p) = player(i) else { continue };
                if i as i64 == current {
                    continue;
                }
                let end_count: c_long = ee_sim::world_field!(p.saved(), mEndOfGameCount);
                let result: c_ulong = ee_sim::world_field!(p.saved(), mGameResult);
                if end_count > 0 {
                    alive += 1;
                } else if result == 0 {
                    others_done = false;
                }
            }
            let mut run = lock(&self.run);
            if alive <= 1 && others_done {
                if run.standing_since < 0 {
                    run.standing_since = time;
                } else if time - run.standing_since >= 1000 {
                    // two harness samples apart
                    drop(run);
                    self.finish_game("last-computer-standing");
                    return;
                }
            } else {
                run.standing_since = -1;
            }
        } else {
            let mut all = count > 1;
            for i in 1..count {
                if let Some(p) = player(i) {
                    let result: c_ulong = ee_sim::world_field!(p.saved(), mGameResult);
                    if result == 0 {
                        all = false;
                    }
                }
            }
            if all {
                self.finish_game("game-end");
                return;
            }
        }
        if time >= (options.game_seconds * 1000.0) as c_long {
            self.finish_game("time-cap");
        }
    }

    // ------------------------------------------------------------------------------------------------ the menus

    fn wait_for(&self, what: &str, seconds: f64, mut test: impl FnMut() -> bool) {
        let end = Instant::now() + Duration::from_secs_f64(seconds);
        while !test() {
            if Instant::now() > end {
                let top = self.game.top_form();
                let class = self.game.top_form_class();
                self.fail(&format!("timed out waiting for {what} (top form {top} {class})"));
            }
            sleep_ms(50);
        }
    }

    fn wait_form(&self, form: u32, seconds: f64) {
        self.wait_for(&format!("form {form}"), seconds, || self.game.top_form() == form);
    }

    fn click(&self, form: u32, control: u32) {
        match self.game.click(form, control) {
            0 => {}
            1 => self.fail(&format!("click: form {form} control {control} is not on top")),
            _ => self.fail(&format!("click: control {control} has no trigger event")),
        }
    }

    fn select(&self, control: c_long, value: &str, label: &str) {
        let Some((mut lines, mut selection)) = self.game.combo_lines(control) else {
            self.fail(&format!("{label}: control {control} is not a combo on the lobby"))
        };
        if lines.is_empty() {
            // a combo the lobby has not filled yet (or is filling again from a lobby update)
            self.wait_for(&format!("{label}'s lines"), 60.0, || match self.game.combo_lines(control) {
                Some((l, s)) if !l.is_empty() => {
                    (lines, selection) = (l, s);
                    true
                }
                _ => false,
            });
        }
        let index = match lobby::resolve(value, &lines, label) {
            Ok(k) => k,
            Err(e) => self.fail(&e),
        };
        if selection != index as c_long {
            if !self.game.combo_select(control, index as c_ulong) {
                self.fail(&format!("{label}: the lobby went away"));
            }
            self.wait_for(&format!("{label} to stick"), 20.0, || {
                self.game.combo_lines(control).is_some_and(|(_, s)| s == index as c_long)
            });
            sleep_ms(200); // the lobby's update round trip through the local server
        }
        lock(&self.chosen).push(Chosen {
            control,
            label: label.to_string(),
            value: value.to_string(),
            line: lines[index].clone(),
            index: index as c_long,
        });
        self.note(&format!("LOBBY {label}: control {control} = {index} '{}'", lines[index]));
    }

    fn set_up_lobby(&self) {
        let options = self.opt().clone();
        let n = options.computers;
        let filled = |c: c_long| self.game.combo_lines(c).is_some_and(|(l, _)| !l.is_empty());
        self.wait_for("the lobby's combos", 60.0, || {
            // each combo there and filled: the slots' lines come with the local server's first lobby update
            (0..VISIBLE_SLOTS).all(|k| filled(PLAYER_TYPE + k)) && [177, 179, 207].iter().all(|&c| filled(c))
        });
        sleep_ms(1500); // the local server's readiness and queued lobby updates (the harness waits 3 s)
        let mut choice = |name: &str| {
            let k = self.game.engine_choice(name);
            if k >= 0 {
                return Ok(k);
            }
            let ids = self.game.engine_ids();
            let mut known = String::from("classic, random");
            for id in &ids {
                known += ", ";
                known += id;
            }
            Err(format!("--engine: '{name}' is no engine ({known}{})", if ids.is_empty() { "" } else { ", next" }))
        };
        let list = match lobby::lobby_options(&options, &mut choice) {
            Ok(l) => l,
            Err(e) => self.fail(&e),
        };
        self.select(PLAYER_TYPE, "#0", "slot 0 human");
        for s in 1..VISIBLE_SLOTS {
            self.select(PLAYER_TYPE + s, if s <= n { "Computer" } else { "Open" }, &format!("slot {s}"));
        }
        for (control, value) in &list {
            self.select(*control, value, &format!("control {control}"));
        }
        sleep_ms(1000);
        let chosen = lock(&self.chosen).clone();
        for c in &chosen {
            if !self.game.combo_lines(c.control).is_some_and(|(_, s)| s == c.index) {
                self.fail(&format!("lobby selection of control {} changed after setting it", c.control));
            }
        }
    }

    /// The line of `name` in a screen's list (None: not listed or not on top) and the list's selection.
    fn list_line(&self, which: i32, name: &str) -> (Option<usize>, c_ulong) {
        match self.game.list_lines(which) {
            Some((lines, selection)) => (lines.iter().position(|l| l == name), selection),
            None => (None, c_ulong::MAX),
        }
    }

    /// The screen's list's line of `name` selected, as its keys select one.
    fn select_listed(&self, which: i32, name: &str, listed: &str, stick: &str, gone: &str) {
        self.wait_for(listed, 60.0, || self.list_line(which, name).0.is_some());
        let line = self.list_line(which, name).0.unwrap_or(0);
        if !self.game.list_select(which, line as c_ulong) {
            self.fail(gone);
        }
        self.wait_for(stick, 20.0, || {
            let (l, s) = self.list_line(which, name);
            l == Some(line) && s == line as c_ulong
        });
        sleep_ms(200);
    }

    fn copy_file(from: &str, to: &str) -> bool {
        std::fs::copy(from, to).is_ok()
    }

    /// --load-save: the file staged as data\Saved Games\EE-SIM load, then the single-player form's Load Game, the
    /// file selected in the load screen's list, Load.
    fn load_from_menu(&self) {
        let load = self.opt().load_save.clone();
        let staged = self.save_path(LOAD_NAME);
        let _ = std::fs::remove_file(&staged);
        if !Self::copy_file(&load, &staged) {
            self.fail(&format!("--load-save: cannot copy {load} to {staged}"));
        }
        let button = self.game.control_with_event(FORM_SINGLE, self.events.sp_load_game);
        if button < 0 {
            self.fail("--load-save: no Load Game button on the single-player form");
        }
        self.click(FORM_SINGLE, button as u32);
        self.wait_form(FORM_LOAD, 120.0);
        self.select_listed(
            EE_SIM_DRIVER_LIST_LOAD,
            LOAD_NAME,
            "the load screen to list the file",
            "the load selection to stick",
            "--load-save: the load screen went away",
        );
        let button = self.game.control_with_event(FORM_LOAD, self.events.spl_load);
        if button < 0 {
            self.fail("--load-save: no Load button on the load screen");
        }
        self.note(&format!("UI load {load}"));
        self.click(FORM_LOAD, button as u32);
    }

    /// --scenario: the file staged as data\Scenarios\EE-SIM scenario, the lobby's game type Scenario, the file
    /// selected on the Play Scenario screen, Load, Start.
    fn scenario_from_lobby(&self) {
        let scenario = self.opt().scenario.clone();
        let staged = format!("data\\Scenarios\\{SCENARIO_NAME}{}", self.game.extension(1));
        let _ = std::fs::remove_file(&staged);
        if !Self::copy_file(&scenario, &staged) {
            self.fail(&format!("--scenario: cannot copy {scenario} to {staged}"));
        }
        self.click(FORM_SINGLE, 1);
        self.wait_form(FORM_LOBBY, 120.0);
        self.note("UI lobby (scenario)");
        self.wait_for("the lobby's game type box", 60.0, || {
            self.game.combo_lines(175).is_some_and(|(l, _)| l.len() > 2)
                && self.game.combo_lines(207).is_some_and(|(l, _)| !l.is_empty())
        });
        sleep_ms(1500);
        if !self.game.combo_select(175, 2) {
            self.fail("--scenario: no game type box on the lobby");
        }
        self.wait_form(FORM_LOAD_SCENARIO, 120.0);
        self.select_listed(
            EE_SIM_DRIVER_LIST_SCENARIO,
            SCENARIO_NAME,
            "the play scenario screen to list the file",
            "the scenario selection to stick",
            "--scenario: the play scenario screen went away",
        );
        self.note(&format!("UI load scenario {scenario}"));
        self.click(FORM_LOAD_SCENARIO, self.events.load_scenario_button);
        self.wait_form(FORM_LOBBY, 120.0);
        sleep_ms(3000); // the scenario's options and players through the local server
        self.click(FORM_LOBBY, 10);
        self.note("UI start (scenario)");
    }

    fn click_screen_event(&self, form: u32, code: u32, what: &str) {
        let mut button = -1;
        self.wait_for(what, 60.0, || {
            button = self.game.control_with_event(form, code);
            button >= 0
        });
        self.click(form, button as u32);
    }

    /// --campaign NAME [--scenario-index K]: a shipped campaign's scenario through the campaign screens.
    fn campaign_from_menu(&self) {
        let (name, index) = {
            let o = self.opt();
            (o.campaign.clone(), o.scenario_index)
        };
        let e = self.events;
        if name == "learning" {
            self.click_screen_event(FORM_SINGLE, e.pc_learning, "the Learn to Play button");
        } else {
            self.click_screen_event(FORM_SINGLE, e.sp_campaign, "the Campaign button");
            self.wait_form(FORM_PLAY_CAMPAIGN, 120.0);
            let code = match name.as_str() {
                "roman" => e.pc_campaign[0],
                "pacific" => e.pc_campaign[1],
                _ => e.pc_campaign[2],
            };
            self.click_screen_event(FORM_PLAY_CAMPAIGN, code, "the campaign's button");
        }
        self.wait_form(FORM_CAMPAIGN_SELECT, 120.0);
        let mut count = 0;
        self.wait_for("the campaign's scenario list", 60.0, || {
            match self.game.list_lines(EE_SIM_DRIVER_LIST_CAMPAIGN) {
                Some((l, _)) => {
                    count = l.len();
                    count > 0
                }
                None => false,
            }
        });
        sleep_ms(1000);
        if index >= 0 {
            if index as usize >= count {
                self.fail(&format!("--scenario-index {index} outside the campaign's {count} scenarios"));
            }
            if !self.game.list_select(EE_SIM_DRIVER_LIST_CAMPAIGN, index as c_ulong) {
                self.fail("--campaign: the scenario select form went away");
            }
            self.wait_for("the campaign scenario selection to stick", 20.0, || {
                self.game.list_lines(EE_SIM_DRIVER_LIST_CAMPAIGN).is_some_and(|(_, s)| s == index as c_ulong)
            });
            sleep_ms(500);
        }
        let (selection, text) = match self.game.list_lines(EE_SIM_DRIVER_LIST_CAMPAIGN) {
            Some((l, s)) => (s, l.get(s as usize).cloned().unwrap_or_else(|| "?".into())),
            None => (c_ulong::MAX, "?".into()),
        };
        self.note(&format!("UI campaign {name} scenario {selection} of {count}: {text}"));
        self.click_screen_event(FORM_CAMPAIGN_SELECT, e.cs_start, "the scenario select form's Start");
        self.wait_form(FORM_SCENARIO_INTRO, 120.0);
        sleep_ms(1000);
        self.click_screen_event(FORM_SCENARIO_INTRO, e.si_start, "the scenario intro's Start");
        self.note("UI start (campaign scenario)");
    }

    /// --editor-script: Editors, Scenario Editor, the editor form over its default map, the script, the exit.
    fn run_editor(&self) -> ! {
        let e = self.events;
        let button = self.game.control_with_event(FORM_MAIN, e.editors_open);
        if button < 0 {
            self.fail("--editor-script: no Editors button on the main menu");
        }
        self.click(FORM_MAIN, button as u32);
        self.wait_form(FORM_EDITORS, 120.0);
        let button = self.game.control_with_event(FORM_EDITORS, e.editors_scenario_editor);
        if button < 0 {
            self.fail("--editor-script: no Scenario Editor button on the editors' menu");
        }
        self.click(FORM_EDITORS, button as u32);
        self.note("UI scenario editor");
        // the physics runs until the editor form is up (it enters once a communications tick has run), then stops
        let mut last = 0;
        self.wait_for("the scenario editor", 600.0, || {
            let top = self.game.top_form();
            if top != last {
                self.note(&format!("UI form {top}"));
            }
            last = top;
            let p = self.game.physics();
            if top == FORM_GAME && p.present && p.timer_running {
                self.game.set_run(true);
            }
            self.game.editor_up()
        });
        self.game.set_run(false);
        sleep_ms(2000); // the editor's own posted work before the script's first step
        let (script, out) = {
            let o = self.opt();
            (o.editor_script.clone(), o.editor_out.clone())
        };
        if let Err(e) = self.game.run_editor(&script, &out) {
            self.fail(&e); // ("--editor-out: cannot write ..." or "--editor-script: ...")
        }
        self.note("EDITOR script done");
        lock(&self.run).outcome = "editor-script".into();
        self.write_report(0, "");
        self.quit(0)
    }

    // ------------------------------------------------------------------------------------------------ the driver

    /// The driver's state of one game, cleared before each game of a batch.
    fn reset_run(&self) {
        *lock(&self.run) = new_run();
        lock(&self.out).load_failure.clear();
        lock(&self.chosen).clear();
        self.game.no_human_defeat_hits(0);
        self.game.set_after_tick(false);
        lock(&self.times).game = None;
    }

    /// --games: the next game's options, files and stderr in place.
    fn begin_game(&self, index: usize, game: &Options, after_game: bool) {
        *lock(&self.game_index) = index;
        *self.opt() = game.clone();
        self.reset_run();
        self.game.set_switches(&self.switches());
        if after_game {
            // (the first game's wall time and phases count from the process's start)
            let mut t = lock(&self.times);
            let now = Instant::now();
            t.start = now;
            t.phase = now;
            t.phases.clear();
        }
        self.marker(&format!("EE-SIM GAME {index} BEGIN {}", game.id));
        let _out = lock(&self.out);
        if !game.err.is_empty() {
            self.game.set_stderr(Some(&game.err), -1);
        }
    }

    /// --games: the game's trace and log from here, headed by the start-up's lines.
    fn start_game_output(&self) {
        let o = self.opt().clone();
        let mut out = lock(&self.out);
        if !o.trace.is_empty() {
            out.trace = TextFile::create(&o.trace).ok();
        }
        if !o.log.is_empty() {
            out.log = TextFile::create(&o.log).ok();
        }
        let head = out.preamble_trace.clone();
        match out.trace.as_mut() {
            Some(f) => {
                let _ = f.write_all(&head);
            }
            None => self.game.write(1, &head, false),
        }
        let head = out.preamble_log.clone();
        if let Some(f) = out.log.as_mut() {
            let _ = f.write_all(&head);
        }
        out.state = OutState::Game;
    }

    /// --games: the game's files closed and the process's stderr back, then the end marker.
    fn end_game_output(&self, code: i32) {
        if !self.batch {
            return;
        }
        {
            let mut out = lock(&self.out);
            out.state = OutState::Between;
            self.game.write(1, b"", true);
            for mut f in [out.trace.take(), out.log.take()].into_iter().flatten() {
                let _ = f.flush();
            }
            if !self.opt().err.is_empty() && out.process_stderr >= 0 {
                self.game.set_stderr(None, out.process_stderr);
            }
        }
        let outcome = lock(&self.run).outcome.clone();
        let index = *lock(&self.game_index);
        let what = if code == 2 {
            "error".to_string()
        } else if outcome.is_empty() {
            "-".to_string()
        } else {
            outcome
        };
        self.marker(&format!("EE-SIM GAME {index} END {code} {what}"));
    }

    // (c_long and c_ulong are 64-bit off Windows)
    #[allow(clippy::unnecessary_cast)]
    fn switches(&self) -> EeSimDriverSwitches {
        let o = self.opt();
        EeSimDriverSwitches {
            seed_set: i32::from(o.load_save.is_empty()),
            seed: o.seed as u32,
            no_human_defeat: i32::from(o.no_human_defeat),
            port_offset: if o.port_offset >= 0 { o.port_offset as i32 } else { -1 },
            scenario_dialogs: !o.scenario.is_empty() || !o.campaign.is_empty(),
        }
    }

    /// The world's player count and map (the views; no lock: the driver polls).
    fn world_ready(&self, loaded: bool, expected: usize) -> bool {
        // SAFETY: a poll of two members between frames (the C++ driver read them unlocked as well)
        let t = unsafe { ee_sim::world::WorldRead::begin() };
        let Some(w) = ee_sim::world::WorldView::global(&t) else { return false };
        let n = w.num_players() as usize;
        (if loaded { n > 1 } else { n == expected }) && w.map().is_some()
    }

    /// The main menu to the end of the game, then the report -> the exit code (0, 3 at the wall timeout, 5 a save
    /// not copied).
    fn play_game(&self) -> i32 {
        let mut code = 0;
        let left = std::mem::replace(&mut *lock(&self.left_game), false);
        self.phase(if left { "leave" } else { "startup" });
        let options = self.opt().clone();
        for k in 0..options.saves.len() {
            let _ = std::fs::remove_file(self.save_path(&Self::save_name(k))); // a stage is reused
        }
        if self.game.top_form() == FORM_MAIN {
            // (a game left returns to the single player menu)
            self.note("UI main menu");
            if !options.editor_script.is_empty() {
                self.run_editor();
            }
            self.click(FORM_MAIN, 0);
        }
        self.wait_form(FORM_SINGLE, 120.0);
        let scenario = !options.scenario.is_empty() || !options.campaign.is_empty();
        let loaded = !options.load_save.is_empty() || scenario;
        if !options.campaign.is_empty() {
            self.campaign_from_menu();
        } else if scenario {
            self.scenario_from_lobby();
        } else if loaded {
            self.load_from_menu();
        } else {
            self.click(FORM_SINGLE, 1);
            self.wait_form(FORM_LOBBY, 120.0);
            self.note("UI lobby");
            self.phase("menus");
            self.set_up_lobby();
            self.click(FORM_LOBBY, 10);
            self.note("UI start");
            self.phase("lobby");
        }
        let expected = (options.computers + 2) as usize;
        self.wait_for("the game", 600.0, || {
            let failure = lock(&self.out).load_failure.clone();
            if !failure.is_empty() {
                self.fail(&format!("load failed: {failure}"));
            }
            let p = self.game.physics();
            self.game.top_form() == FORM_GAME && p.present && p.timer_running && self.world_ready(loaded, expected)
        });
        sleep_ms(500);
        self.game.lock(EE_SIM_DRIVER_WRITE_LOCK);
        let (count, current) = {
            // SAFETY: the driver holds the world's write lock
            let t = unsafe { ee_sim::world::WorldRead::begin() };
            let w = ee_sim::world::WorldView::global(&t);
            let count = w.as_ref().map_or(0, |w| w.num_players() as usize);
            let current = w.as_ref().map_or(-1, |w| w.current_player_index());
            if loaded {
                // the saved players carry their flags (the harness's loaded game: no civilization pre-empt)
                let mut results = vec![0; count];
                for (i, r) in results.iter_mut().enumerate().skip(1) {
                    if let Some(p) = w.as_ref().and_then(|w| w.player(i)) {
                        *r = ee_sim::world_field!(p.saved(), mGameResult);
                    }
                }
                lock(&self.run).results = results;
            }
            (count, current)
        };
        if loaded {
            let p = self.game.physics();
            let what = if !options.campaign.is_empty() {
                &options.campaign
            } else if scenario {
                &options.scenario
            } else {
                &options.load_save
            };
            self.note(&format!("LOADED {what}: game ms {} PT {}", p.game_ms, p.pt));
            let has_checksum = lock(&self.out).checksum.is_some();
            if has_checksum {
                // the loaded state (not a "CT" line: the comparisons skip it)
                let line = format!("LOADED {}", self.game.checksum_fields());
                if let Some(f) = lock(&self.out).checksum.as_mut() {
                    let _ = f.write_all(line.as_bytes());
                }
            }
            if !self.process.dump_load.is_empty() {
                self.game.dump_after_load(&self.process.dump_load);
            }
        } else {
            // the idle human as the harness's civilization pre-empt leaves it, out of the fighting unless
            // --keep-human
            self.game.prepare_human(options.human_out);
            lock(&self.run).results = vec![0; count];
        }
        self.game.lock(EE_SIM_DRIVER_WRITE_UNLOCK);
        self.note(&format!("GAME players {count} local {current}: running"));
        self.phase("loading");
        lock(&self.times).game = Some(Instant::now());
        self.game.set_after_tick(true);
        self.game.set_run(true);
        {
            let run = lock(&self.run);
            let timeout = Duration::from_secs_f64(options.wall_timeout.max(0.0));
            let (mut run, result) =
                self.done.wait_timeout_while(run, timeout, |r| !r.finished).unwrap_or_else(|e| e.into_inner());
            if result.timed_out() && !run.finished {
                self.game.set_run(false);
                run.outcome = "wall-timeout".into();
                code = 3;
            }
        }
        self.game.set_run(false);
        sleep_ms(50);
        if self.game.physics().present {
            // the final state (the physics thread has stopped between two communications ticks)
            self.game.lock(EE_SIM_DRIVER_READ_LOCK);
            let s = self.take_sample();
            self.game.lock(EE_SIM_DRIVER_READ_UNLOCK);
            let mut run = lock(&self.run);
            if run.samples.last().is_none_or(|l| l.game_ms != s.game_ms) {
                run.samples.push(s);
            }
        }
        let line = {
            let run = lock(&self.run);
            let last = run.samples.last();
            let mut units = String::new();
            for p in last.map_or(&[][..], |s| &s.players[..]) {
                units += &format!(" {}:{}/{}", p.index, p.population, p.victory_units);
            }
            let game = lock(&self.times).game.map_or(0.0, |g| g.elapsed().as_secs_f64());
            let (pt, ms) = last.map_or((0, 0), |s| (s.pt, s.game_ms));
            format!(
                "SIM {}: PT {pt} game s {} units{units}; {game:.1} wall s in game ({:.0}x real time)",
                run.outcome,
                ms / 1000,
                if last.is_some() && game > 0.0 { ms as f64 / 1000.0 / game } else { 0.0 }
            )
        };
        self.note(&line);
        let next_save = lock(&self.run).next_save;
        for (k, (ms, to)) in options.saves.iter().enumerate() {
            let from = self.save_path(&Self::save_name(k));
            if k >= next_save {
                self.note(&format!("SAVE {k} (game ms {ms}) not requested: the run ended before"));
            } else if !Self::copy_file(&from, to) {
                self.note(&format!("SAVE {k}: {from} not written or not copied to {to}"));
                if code == 0 {
                    code = 5;
                }
            }
        }
        self.phase("game");
        self.write_report(code, "");
        code
    }

    /// --games: the game left as a player leaves it, then the post-game summary's exit, back to the single player
    /// menu, settled.
    fn leave_game(&self) {
        *lock(&self.left_game) = true;
        self.game.cancel_game();
        let mut last = 0;
        let mut exited = false; // (the summary's exit once: a second close, still queued, would close the next form)
        let summary = self.events.form_post_game_summary;
        self.wait_for("the single player menu after the game", 120.0, || {
            let top = self.game.top_form();
            if top != last {
                self.note(&format!("UI form {top}"));
                last = top;
            }
            if top == summary && !exited {
                let exit = self.game.control_with_event(summary, self.events.pgs_exit);
                if exit >= 0 {
                    exited = true;
                    self.click(summary, exit as u32);
                }
            }
            (top == FORM_SINGLE || top == FORM_MAIN) && exited
        });
        // (the menu settled: the form on top for 300 ms; a click into a form still opening was lost)
        let mut since = Instant::now();
        let mut top = self.game.top_form();
        self.wait_for("the single player menu to settle", 30.0, || {
            let now = self.game.top_form();
            if now != top {
                top = now;
                since = Instant::now();
            }
            (top == FORM_SINGLE || top == FORM_MAIN) && since.elapsed() > Duration::from_millis(300)
        });
    }

    /// --games: the next game's line -> `Some(Ok)` a game, `Some(Err)` a bad line, None the end of the file.
    fn next_game(&self, index: usize) -> Option<Result<Options, String>> {
        let mut games = lock(&self.games);
        let file = games.as_mut()?;
        loop {
            let mut row = Vec::new();
            let n = file.read_until(b'\n', &mut row).unwrap_or(0);
            if n == 0 {
                return None;
            }
            if row.last() == Some(&b'\n') {
                row.pop();
            }
            let line = String::from_utf8_lossy(&row).into_owned();
            match options::game_of_line(&line, index, &self.base, &mut |d| {
                let _ = std::fs::create_dir(d);
            }) {
                Ok(None) => continue,
                Ok(Some(g)) => return Some(Ok(g)),
                Err(e) => return Some(Err(e)),
            }
        }
    }

    /// The driver's thread: the rules' overrides checked, the main menu awaited, then one game or the batch.
    pub fn drive(&self) -> ! {
        let problems = self.game.rules_problems();
        if !problems.is_empty() {
            if self.batch {
                let o = self.opt().clone();
                self.begin_game(0, &o, false);
            }
            self.fail(&format!("--set did not apply: {}", problems.join("; ")));
        }
        self.wait_form(FORM_MAIN, 300.0);
        if !self.batch {
            let code = self.play_game();
            self.quit(code);
        }
        let mut played = false; // a game ran and was not left yet
        let mut k = 0;
        while let Some(next) = self.next_game(k) {
            match next {
                Err(e) => self.marker(&format!("EE-SIM GAME {k} END 64 bad-options {e}")),
                Ok(game) => {
                    self.begin_game(k, &game, played);
                    if played {
                        self.leave_game(); // (a failure here is this game's: its report has the error)
                    }
                    played = true;
                    self.start_game_output();
                    let code = self.play_game();
                    self.end_game_output(code);
                    if code != 0 {
                        self.quit(code); // a wall timeout leaves a game running: the rest in a new process
                    }
                }
            }
            k += 1;
        }
        self.marker(&format!("EE-SIM BATCH END {k}"));
        self.quit(0)
    }

    /// The process's start: the platform switches, the environment and the start line (after [`install`]).
    pub fn start_process(&self, data: &str, config: &str, ai_trace: bool, games: &str) {
        let o = self.opt().clone();
        self.game.set_switches(&self.switches());
        if !data.is_empty() {
            self.game.set_env("EE_DATA_ROOT", data);
        }
        if !config.is_empty() {
            self.game.set_env("EE_CONFIG", config);
        }
        if ai_trace {
            self.game.set_env("EE_AI_TRACE", "1");
        }
        if !o.cheats.is_empty() {
            self.game.set_env("EE_AI_CHEATS", &o.cheats);
        }
        if !o.personality.is_empty() {
            self.game.set_env("EE_AI_PERSONALITY", &o.personality);
        }
        if o.effect_sweep || o.group_sweep {
            self.game.sweep(EE_SIM_DRIVER_SWEEP_HOLD, 1); // only the end sweep records (FinishGame)
        }
        if !self.process.tick_dumps.is_empty() {
            self.game.tick_hook();
        }
        if let Some((ms, path)) = &self.process.rng_trace {
            self.game.rng_trace(*ms, path);
        }
        let ports = if o.port_offset >= 0 {
            format!("{}/{}", 33334 + o.port_offset, 33335 + o.port_offset)
        } else {
            "any free".into()
        };
        if self.batch {
            self.note(&format!("EE-SIM games from {games}, ports {ports}"));
        } else {
            self.note(&format!(
                "EE-SIM seed {}, {} computers, cap {:.0} game s, ports {ports}",
                o.seed, o.computers, o.game_seconds
            ));
        }
    }

    /// The PTs per second and the autosave period (EESimPreferences).
    pub fn preferences(&self) -> (c_long, c_long) {
        let o = self.opt();
        (o.pt_rate, o.autosave_ms)
    }
}

/// The unit categories, families and types a sample counts (ee/EEUnitClassIDs.h).
mod kinds {
    pub const AMBIENT: u64 = 0;
    pub const RESOURCE: u64 = 1;
    pub const BUILDING: u64 = 2;
    pub const AIRPLANE: u64 = 3;
    pub const NAVAL: u64 = 4;
    pub const LAND_CITIZEN: u64 = 6;
    pub const HELICOPTER: u64 = 7;
    pub const PROJECTILE: u64 = 9;
    pub const SUBMERGED_NAVAL: u64 = 0xa;
    pub const HELICOPTER_TRANSPORT: u64 = 0x11;
    pub const NAVAL_TRANSPORT: u64 = 0x12;
    pub const NAVAL_BUILDING: u64 = 0x15;
    pub const NAVAL_CITIZEN: u64 = 0x16;
    pub const MINING_BUILDING: u64 = 0x18;
    pub const GATE: u64 = 0x19;
    pub const FORTRESS: u64 = 0x1d;
    pub const BUILDING_PRIEST: u64 = 0x1e;
    pub const SPACE_BUILDING: u64 = 0x20;
    pub const FAMILY_TOWERS: u64 = 0x25;
    pub const FAMILY_SPACE_TURRET: u64 = 0x3b;
}

/// The categories a sample counts as buildings.
pub fn is_building_category(c: u64) -> bool {
    use kinds::*;
    matches!(c, BUILDING | NAVAL_BUILDING | MINING_BUILDING | GATE | FORTRESS | BUILDING_PRIEST | SPACE_BUILDING)
}

/// One unit's part of a sample: its category, family, wall and tower flags, hit points and attack (None: no stats).
pub struct SampleUnit {
    pub category: u64,
    pub family: u64,
    pub wall: bool,
    pub tower: bool,
    pub hit_points: c_long,
    pub attack: Option<f32>,
}

/// A living unit counted into a player's sample (`value` its class's cost).
pub fn count_unit(p: &mut PlayerSample, u: &SampleUnit, value: c_long) -> bool {
    use kinds::*;
    if matches!(u.category, AMBIENT | RESOURCE | PROJECTILE) {
        return false; // the AI's path points (ambient units), resources, shots
    }
    if u.category == LAND_CITIZEN || u.category == NAVAL_CITIZEN {
        p.citizens += 1;
    } else if is_building_category(u.category) {
        p.buildings += 1;
        if u.family == FAMILY_TOWERS || u.family == FAMILY_SPACE_TURRET {
            p.towers += 1;
        }
        if u.wall && !u.tower {
            p.walls += 1;
        }
    } else {
        p.army += 1;
        if matches!(u.category, NAVAL | SUBMERGED_NAVAL | NAVAL_TRANSPORT) {
            p.naval += 1;
        } else if matches!(u.category, AIRPLANE | HELICOPTER | HELICOPTER_TRANSPORT) {
            p.air += 1;
        }
        if let Some(attack) = u.attack {
            p.strength = p.strength.wrapping_add(ee_sim::units::player::trunc_long(u.hit_points as f32 * attack));
        }
    }
    p.value = p.value.wrapping_add(value);
    true
}

fn sample_player(game: &dyn Game, player: &ee_sim::world::PlayerView, i: usize, current: i64) -> PlayerSample {
    let saved = player.saved();
    let tree = player.tech_tree();
    let caps: [c_long; 7] = ee_sim::world_field!(saved, mResourceCaps);
    let mut p = PlayerSample {
        index: i as c_ulong,
        human: i as i64 == current,
        epoch: tree.as_ref().map_or(-1, |t| t.current_epoch() as c_long),
        population: ee_sim::world_field!(player.saved(), mPopulationCount),
        victory_units: ee_sim::world_field!(player.saved(), mEndOfGameCount),
        result: ee_sim::world_field!(player.saved(), mGameResult),
        wonders: game.wonders(player.handle()),
        resources: [caps[2], caps[3], caps[4], caps[5], caps[6]],
        ..PlayerSample::default()
    };
    for unit in player.units() {
        if unit.complex_unit_handle() == 0 || unit.hit_points() <= 0 {
            continue;
        }
        let Some(class) = unit.class() else { continue };
        let fields = class.fields();
        let u = SampleUnit {
            category: class.category(),
            family: class.family(),
            wall: ee_sim::world_field!(fields, mIsWall),
            tower: ee_sim::world_field!(class.fields(), mIsTower),
            hit_points: unit.hit_points() as c_long,
            attack: unit.stats().map(|s| ee_sim::world_field!(s.fields(), mAttack)),
        };
        let value = if tree.is_some() {
            game.unit_value(player.handle(), class.technology() as c_long, class.unit_type() as c_ulong)
        } else {
            0
        };
        count_unit(&mut p, &u, value);
    }
    p
}
