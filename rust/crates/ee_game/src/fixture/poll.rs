//! The fixture's switches, its thread and its poll (TEST ONLY): the ui-state file, the frame request, the commands.

use std::sync::atomic::{AtomicBool, AtomicI64, AtomicU64, Ordering};
use std::sync::{Mutex, OnceLock};
use std::time::Instant;

use ee_sim::world::{WorldRead, WorldView};

use super::host::{self, Call, EeFixtureOp as Op};
use super::text;

const EE_FIXTURE_LIST_KINDS: [&str; 4] = ["", "combo", "check", "list"];

// the renderer's TEST ONLY frame hook and read-back (ee_render gx ffi.rs)
extern "C" {
    fn GXTestSetPresentHook(hook: Option<extern "C" fn(*mut std::ffi::c_void, *mut std::ffi::c_void)>);
    fn GXTestReadFrame(
        frame: *mut std::ffi::c_void,
        out_pixels: *mut std::ffi::c_void,
        capacity: std::os::raw::c_ulong,
        out_width: *mut std::os::raw::c_ulong,
        out_height: *mut std::os::raw::c_ulong,
    ) -> std::os::raw::c_long;
}

static DIRECTORY: OnceLock<String> = OnceLock::new();
static FRAME_WANTED: AtomicBool = AtomicBool::new(false);
static FRAMES_WRITTEN: AtomicU64 = AtomicU64::new(0);

/// The test switches (EETestFixtureConfigure): the harness clock, the seed, the idle human, the port offset; the
/// debug output to stderr. A malformed one stops the game.
pub fn configure() {
    let Some(dir) = std::env::var_os("EE_TEST_UI") else { return };
    if dir.is_empty() {
        return;
    }
    Call::new(Op::Install).i(&[0]).go();
    let var = |name: &str| std::env::var(name).ok();
    if let Some(factor) = var("EE_TEST_TIME_FACTOR") {
        let parsed = text::time_factor(&factor);
        let (num, den) = parsed.unwrap_or((0, 1));
        if parsed.is_none() || Call::new(Op::SetTimeFactor).i(&[num as i64, den as i64]).go() == 0 {
            host::stderr(&format!("TEST UI EE_TEST_TIME_FACTOR={factor} is not a factor in 1/1000..4096\n"));
            Call::new(Op::Exit).i(&[3]).go();
        }
        host::stderr(&format!("TEST UI clock factor {num}/{den}\n"));
    }
    if let Some(seed) = var("EE_TEST_SEED") {
        let (value, whole) = text::strtoull0(&seed);
        if seed.is_empty() || !whole || value > 0xffff_ffff {
            host::stderr(&format!("TEST UI EE_TEST_SEED={seed} is not a 32-bit number\n"));
            Call::new(Op::Exit).i(&[3]).go();
        }
        Call::new(Op::SetSwitch).i(&[0, value as i64]).go();
        host::stderr(&format!("TEST UI seed {}\n", value as u32));
    }
    if let Some(flag) = var("EE_TEST_NO_HUMAN_DEFEAT") {
        let on = i64::from(crate::sim_driver::options::c_atol(&flag) != 0);
        Call::new(Op::SetSwitch).i(&[1, on]).go();
        host::stderr(&format!("TEST UI no human defeat {on}\n"));
    }
    if let Some(offset) = var("EE_TEST_PORT_OFFSET") {
        let (value, whole) = text::strtol10(&offset);
        if offset.is_empty() || !whole || !(0..=4095).contains(&value) {
            host::stderr(&format!("TEST UI EE_TEST_PORT_OFFSET={offset} is not a port offset in 0..4095\n"));
            Call::new(Op::Exit).i(&[3]).go();
        }
        Call::new(Op::SetSwitch).i(&[2, value]).go();
        host::stderr(&format!(
            "TEST UI port offset {value} (TCP {}/{}, LAN broadcast {}, lobby {})\n",
            33334 + value,
            33335 + value,
            8585 + value,
            8888 + value
        ));
    }
}

/// The fixture's start (EETestFixtureStart): the frame hook, the game's test hooks, the poll thread.
pub fn start() {
    let Some(dir) = std::env::var("EE_TEST_UI").ok().filter(|d| !d.is_empty()) else { return };
    let _ = DIRECTORY.set(dir.clone());
    // SAFETY: the renderer's TEST ONLY hook (ee_render gx ffi.rs)
    unsafe { GXTestSetPresentHook(Some(present_hook)) };
    Call::new(Op::Install).i(&[1]).go();
    host::stderr(&format!("TEST UI fixture on {dir}\n"));
    std::thread::spawn(move || {
        let mut last = 0u64;
        loop {
            poll(&dir, &mut last);
            host::sleep_ms(250);
        }
    });
}

/// Before every presented frame (the render thread): when a frame was asked for, writes it to <dir>/frame.bmp
/// (atomically: .tmp + rename), a top-down 32-bit BMP.
extern "C" fn present_hook(_window: *mut std::ffi::c_void, frame: *mut std::ffi::c_void) {
    if !FRAME_WANTED.swap(false, Ordering::SeqCst) {
        return;
    }
    let Some(dir) = DIRECTORY.get() else { return };
    let (mut width, mut height) = (0, 0);
    // SAFETY: the renderer's frame, inside its present
    unsafe { GXTestReadFrame(frame, std::ptr::null_mut(), 0, &mut width, &mut height) };
    if width == 0 || height == 0 || width > 16384 || height > 16384 {
        return;
    }
    let mut pixels = vec![0u8; width as usize * height as usize * 4];
    // SAFETY: as above; the buffer holds the frame
    let read =
        unsafe { GXTestReadFrame(frame, pixels.as_mut_ptr().cast(), pixels.len() as _, &mut width, &mut height) };
    if read != 0 {
        return;
    }
    let temporary = format!("{dir}/frame.tmp");
    let mut bytes = text::bmp_header(u64::from(width), u64::from(height)).to_vec();
    bytes.extend_from_slice(&pixels);
    if std::fs::write(&temporary, &bytes).is_ok() && text::replace_with(&temporary, &format!("{dir}/frame.bmp")) {
        FRAMES_WRITTEN.fetch_add(1, Ordering::SeqCst);
    }
}

// ---- the pointer hold: "pointer ... <ms>" keeps the game's pointer at the commanded position for ms milliseconds
// (the game moves its pointer on its own events too: a thread puts it back every 2 ms while the hold lasts); any
// later pointer, lclick, boxselect or order command ends it. Its end: "TEST UI pointer hold seq N done: K corrections".

static HOLD_UNTIL: AtomicI64 = AtomicI64::new(0);
static HOLD_SEQ: AtomicU64 = AtomicU64::new(0);
static HOLD_THREAD: AtomicBool = AtomicBool::new(false);
static HOLD_AT: Mutex<(f32, f32)> = Mutex::new((0.0, 0.0));

fn now_ms() -> i64 {
    static START: OnceLock<Instant> = OnceLock::new();
    START.get_or_init(Instant::now).elapsed().as_millis() as i64 + 1
}

fn hold_loop() {
    let (mut reported, mut fixes, mut seq) = (0u64, 0u64, 0u64);
    let report = |reported: &mut u64, fixes: u64, seq: u64| {
        if seq != 0 && *reported != seq {
            host::stderr(&format!("TEST UI pointer hold seq {seq} done: {fixes} corrections\n"));
        }
        *reported = seq;
    };
    loop {
        let until = HOLD_UNTIL.load(Ordering::SeqCst);
        let now_seq = HOLD_SEQ.load(Ordering::SeqCst);
        if now_seq != seq {
            report(&mut reported, fixes, seq);
            seq = now_seq;
            fixes = 0;
        }
        let at = *HOLD_AT.lock().unwrap_or_else(|e| e.into_inner());
        let fixed = if until == 0 || now_ms() >= until { -1 } else { Call::new(Op::MouseFix).f(&[at.0, at.1]).go() };
        if fixed < 0 {
            report(&mut reported, fixes, seq);
            host::sleep_ms(10);
            continue;
        }
        fixes += fixed as u64;
        host::sleep_ms(2);
    }
}

/// Ends a hold (a command that moves the pointer itself); the hold thread reports it.
pub fn hold_release() {
    HOLD_UNTIL.store(0, Ordering::SeqCst);
}

/// The pointer at (x, y) (clamped as the game clamps it), held for `ms` milliseconds when `ms` > 0.
fn hold_pointer(seq: u64, x: f32, y: f32, ms: u64) {
    hold_release();
    let Some(at) = host::mouse_set(x, y) else { return };
    if ms == 0 {
        return;
    }
    *HOLD_AT.lock().unwrap_or_else(|e| e.into_inner()) = at;
    HOLD_UNTIL.store(now_ms() + ms as i64, Ordering::SeqCst); // before the sequence: the thread reads it first
    HOLD_SEQ.store(seq, Ordering::SeqCst);
    if !HOLD_THREAD.swap(true, Ordering::SeqCst) {
        std::thread::spawn(hold_loop);
    }
}

// ---- the poll

/// The game's telemetry lines (plain reads: the scroll check compares polls a second apart).
fn telemetry(out: &mut String) {
    // SAFETY: the C++ read the world's members with no lock here too (the UI's copy lock is held)
    let t = unsafe { WorldRead::begin() };
    let world = WorldView::global(&t);
    let players = world.map_or(0, |w| w.num_players());
    let physics = Call::new(Op::Physics).run();
    let (ms, ticks, over) = if physics.result != 0 { (physics.i(0), physics.i(1), physics.i(2)) } else { (-1, 0, 0) };
    out.push_str(&format!(
        "game ms {ms} ticks {ticks} over {over} players {players} map {} local {}\n",
        i32::from(world.and_then(|w| w.map()).is_some()),
        world.map_or(0, |w| w.current_player_index())
    ));
    let comms = Call::new(Op::Comms).run();
    if comms.result != 0 {
        out.push_str(&format!(
            "comm ct {} last {} paused {} multiplayer {}\n",
            comms.i(0),
            comms.i(1),
            comms.i(2),
            comms.i(3)
        ));
    }
    for i in 0..players.min(17) as usize {
        let Some(p) = world.and_then(|w| w.player(i)) else { continue };
        let saved = p.saved();
        let s: [std::os::raw::c_long; 7] = ee_sim::world_field!(saved, mResourceCaps);
        out.push_str(&format!(
            "player {i} observer {} result {} population {} victory {} civ {} {} stock {}/{}/{}/{}/{}\n",
            i32::from(p.observer()),
            ee_sim::world_field!(saved, mGameResult) as std::os::raw::c_ulong,
            ee_sim::world_field!(saved, mPopulationCount) as std::os::raw::c_long,
            ee_sim::world_field!(saved, mEndOfGameCount) as std::os::raw::c_long,
            i32::from(ee_sim::world_field!(saved, mCivilizationLocked)),
            i32::from(ee_sim::world_field!(saved, mCivilizationChosen)),
            s[2],
            s[3],
            s[4],
            s[5],
            s[6]
        ));
    }
    let clock = Call::new(Op::Clock).run();
    out.push_str(&format!(
        "clock {}/{} ms {} frames {}\n",
        clock.i(0),
        clock.i(1),
        clock.i(2),
        FRAMES_WRITTEN.load(Ordering::SeqCst)
    ));
    let camera = Call::new(Op::Camera).run();
    if camera.result != 0 {
        out.push_str(&format!(
            "camera {:.3} {:.3} {:.3} target {:.3} {:.3} zoom {:.4} mode {} valid {} {} size {}\n",
            camera.f(0),
            camera.f(1),
            camera.f(2),
            camera.f(3),
            camera.f(4),
            camera.f(5),
            camera.i(0),
            camera.i(2),
            camera.i(3),
            camera.i(1)
        ));
    }
}

/// Every 250 ms: the ui-state file (under the UI's copy lock), the frame request, the newest command.
fn poll(dir: &str, last: &mut u64) {
    if host::state() & host::EE_FIXTURE_STATE_UI_MANAGER == 0 {
        return;
    }
    let mut selection = super::boxsel::snapshot(); // before the UI lock (the world's read lock inside)
    selection.push_str(&super::orders::snapshot());
    host::with_ui_lock(|| {
        let form = Call::new(Op::TopForm).run();
        let (form_handle, form_id, count) = (form.result as u64, form.i(0), form.i(1));
        let mut out = format!("form {form_id} controls {count}\ncommand {last}\npid {}\nstack", std::process::id());
        for id in &form.sink.ids {
            out.push_str(&format!(" {id}"));
        }
        out.push_str(&format!("\nmouseover {}\n", form.i(2)));
        let window = Call::new(Op::Window).run();
        if window.result != 0 {
            out.push_str(&format!("window {} {}\n", window.i(0), window.i(1)));
        }
        for i in 0..count.min(2000) {
            let c = Call::new(Op::Control).h(form_handle).i(&[i]).run();
            if c.result < 0 {
                continue;
            }
            if c.i(1) != 0 {
                out.push_str(&format!("control {i} event-rva 0\n"));
            }
            match c.result {
                1 | 3 => {
                    out.push_str(&format!(
                        "{} {i} sel {} lines {}",
                        EE_FIXTURE_LIST_KINDS[c.result as usize],
                        c.i(2),
                        c.i(3)
                    ));
                    out.push_str(&text::lines(&c.sink.texts));
                }
                2 => out.push_str(&format!("check {i} {}\n", c.i(2))),
                _ => {}
            }
        }
        telemetry(&mut out);
        out.push_str(&selection);
        let lobby = Call::new(Op::LobbySnapshot).run();
        if lobby.result != 0 {
            out.push_str("lobbyopen 1\n");
            for t in &lobby.sink.texts {
                out.push_str(&String::from_utf8_lossy(t));
            }
        }
        let temporary = format!("{dir}/ui-state.tmp");
        if std::fs::write(&temporary, text::text_mode(&out)).is_err() {
            return;
        }
        if std::fs::remove_file(format!("{dir}/frame.request")).is_ok() {
            FRAME_WANTED.store(true, Ordering::SeqCst); // the driver asks for the next presented frame
        }
        text::replace_with(&temporary, &format!("{dir}/ui-state.txt"));
        if form_handle != 0 {
            execute(dir, form_handle, form_id as u64, count as u64, last);
        }
    });
}

fn verdict(verdict: &str, op: &str, control: u64, value: u64, form: u64, seq: u64, top: u64) {
    host::stderr(&format!(
        "TEST UI {verdict} {op} control {control} value {value} on form {form} (seq {seq}, top form {top})\n"
    ));
}

/// The newest line of <dir>/test.command, once per sequence number, only while its form is on top (under the UI's
/// copy lock).
fn execute(dir: &str, form: u64, form_id: u64, count: u64, last: &mut u64) {
    let Ok(bytes) = std::fs::read(format!("{dir}/test.command")) else { return };
    let line = text::first_line(&bytes);
    if line.is_empty() {
        return;
    }
    let cmd = text::command(line);
    if cmd.fields < 4 || cmd.seq <= *last {
        return;
    }
    *last = cmd.seq;
    let (seq, op, a, b, c, d) = (cmd.seq, cmd.op.as_str(), cmd.a, cmd.b, cmd.c, cmd.d);
    let five = cmd.fields >= 5;
    if op.starts_with("won") {
        // a lobby command, run on the lobby's message loop (the verdict comes from there)
        let posted = Call::new(Op::LobbyPost).i(&[seq as i64]).s(&format!("{op} {a} {b}")).go() != 0;
        verdict(if posted { "lobby-posted" } else { "rejected" }, op, a, b, 0, seq, form_id);
        return;
    }
    let on_form = a == form_id;
    let mouse = host::state() & host::EE_FIXTURE_STATE_MOUSE != 0;
    let control =
        (on_form && b < count).then(|| Call::new(Op::Control).h(form).i(&[b as i64]).run()).filter(|c| c.result >= 0);
    let has_control = control.is_some();
    let has_trigger = control.as_ref().is_some_and(|c| c.i(1) != 0);
    let mut result = "rejected";
    let control_op = |op: Op, value: u64| Call::new(op).h(form).i(&[b as i64, value as i64]).go() != 0;
    if has_trigger && op == "click" {
        control_op(Op::Click, 0);
        result = "triggered";
    } else if has_control && op == "select" && five {
        if control_op(Op::ComboSelect, c) {
            result = "selected";
        }
    } else if has_control && op == "check" && five {
        if control_op(Op::CheckSet, c) {
            result = "checked";
        }
    } else if op == "zoom" && five && on_form {
        Call::new(Op::Zoom).f(&[c as f32 / 100.0]).go(); // the camera zoom (100 = the original's farthest)
        result = "zoomed";
    } else if op == "pointer" && five && on_form && mouse {
        hold_pointer(seq, b as f32, c as f32, if cmd.fields >= 6 && d <= 60000 { d } else { 0 });
        host::ui_update();
        result = "pointer-moved";
    } else if op == "lclick" && five && on_form && mouse {
        hold_release();
        host::mouse_set(b as f32, c as f32);
        host::ui_update();
        std::thread::spawn(|| {
            // not under the UI lock: the UI thread reads the button between the two
            host::sleep_ms(300);
            host::input(host::EE_FIXTURE_MOUSE_DOWN, 1);
            host::sleep_ms(200);
            host::input(host::EE_FIXTURE_MOUSE_UP, 1);
        });
        result = "clicked";
    } else if op == "key" && five && on_form && b > 0 && b < 0x100 && c <= 10000 {
        std::thread::spawn(move || {
            host::input(host::EE_FIXTURE_KEY_DOWN, b as i64);
            host::sleep_ms(c);
            host::input(host::EE_FIXTURE_KEY_UP, b as i64);
        });
        result = "key-pressed";
    } else if op == "wheel" && five && on_form && b <= 1 && c > 0 && c <= 20 {
        let notch = if b == 0 { 1.0f32 } else { -1.0 };
        std::thread::spawn(move || {
            for _ in 0..c {
                Call::new(Op::Input).i(&[host::EE_FIXTURE_MOUSE_WHEEL, 0]).f(&[notch]).go();
                host::sleep_ms(100);
            }
        });
        result = "wheeled";
    } else if op == "clickevent" && on_form {
        if Call::new(Op::ClickEvent).h(form).i(&[b as i64]).go() != 0 {
            result = "triggered";
        }
    } else if op == "screenevent" && on_form {
        Call::new(Op::ScreenEvent).h(form).i(&[b as i64]).go();
        result = "screen-event";
    } else if op == "chat" && on_form && host::state() & host::EE_FIXTURE_STATE_UI != 0 {
        Call::new(Op::Chat).go();
        result = "chat-added";
    } else if op == "boxselect" && on_form {
        hold_release();
        result = super::boxsel::start(seq, line, form);
    } else if op == "cast" && five && on_form && b > 0 && c < 1000 {
        std::thread::spawn(move || super::cast::run(b as i64, c as i64));
        result = "cast-started";
    } else if op == "order" && on_form {
        hold_release();
        result = super::orders::start(seq, b);
    } else if has_control && op == "lselect" && five && control_op(Op::ListSelect, c) {
        result = "list-selected";
    }
    verdict(result, op, b, c, a, seq, form_id);
}
