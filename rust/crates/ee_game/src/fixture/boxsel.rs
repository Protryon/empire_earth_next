//! The box selection (TEST ONLY). "<seq> boxselect <form> <mode> <x0> <y0> <x1> <y1>" drags a box in the game view
//! through the real input path, on a thread of its own (not under the UI lock): the pointer to (x0, y0), a left
//! button press as the platform delivers it, the pointer in 12 steps to (x1, y1), the release. Modes: 0 select (the
//! selected group becomes the box's units), 1 add (Shift held from before the press to after the release), 2 dead
//! (as 0, but one unit of the current selection is selected alone and deleted through the game's own messages,
//! posted by the box's own message as it is built: the box selection then holds a dead unit's id).
//!
//! Each stage waits for the state the UI thread must have reached, never for a time (PORT_LEARNINGS "Box selection:
//! the intermittent failure"): no Shift or Control seen (modes 0 and 2) or Shift seen (mode 1), the press taken (the
//! selection control holds the form's mouse), the drag at its end (the control dragging, at the end point clamped as
//! MouseMove clamps it), the release taken, and in mode 1 Shift's release seen. A stage not reached within 20 s
//! gives "box-stalled" ("BOXSELECT seq <s> stalled at <stage>"); "BOXSELECT seq <s> waits (ms): <stage> <ms> ..."
//! says how long each took. The verdicts: "box-started" (or "box-busy", "box-no-candidate"), then "box-released".
//!
//! Every poll then writes, read under the world's read lock: "selection <n> <ids>" (the local player's active
//! group), "view <l> <t> <r> <b>" (the game view's rectangle) and for the last box command "boxselect seq <s> mode
//! <m> phase <p> lasso <n> <ids> before <n> <ids> expected <n> <ids> match <0|1> dead <id|-1> dead_in_lasso <0|1>
//! dead_state <none|gone|invalid|dying|alive>" (phase 1 dragging, 2 released, 3 the message seen). expected is the
//! original's rule applied to the objects: of the box's ids, the local player's own units that are their own complex
//! unit, built, not a path point and valid, in mode 1 with the group before the box (those still valid).

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;
use std::time::{Duration, Instant};

use ee_base::math::types::{Ge2DPoint, GeRectangle};
use ee_sim::world::{UnitView, WorldRead, WorldView};

use super::host::{self, Call, EeFixtureOp as Op};
use super::text;

/// `kUnitTypePathPoint` (ee/EEUnitClassIDs.h)
pub const EE_FIXTURE_TYPE_PATH_POINT: u64 = 0xdf;

#[derive(Clone, Debug, Default)]
struct BoxState {
    seq: u64,
    mode: i64,
    phase: i32,
    /// the next box selection message is this command's
    armed: bool,
    dead: i64,
    lasso: Vec<i64>,
    before: Vec<i64>,
}

struct Shared {
    boxed: BoxState,
    /// the last poll's selection
    group: Vec<i64>,
    /// the last poll's dead candidate
    candidate: i64,
}

static SHARED: Mutex<Shared> = Mutex::new(Shared {
    boxed: BoxState { seq: 0, mode: 0, phase: 0, armed: false, dead: -1, lasso: Vec::new(), before: Vec::new() },
    group: Vec::new(),
    candidate: -1,
});
static DRAGGING: AtomicBool = AtomicBool::new(false);

fn shared() -> std::sync::MutexGuard<'static, Shared> {
    SHARED.lock().unwrap_or_else(|e| e.into_inner())
}

/// The box selection message's ids (EEMSelectUnits' ctor, the UI thread, before the lasso posts its action); in
/// mode 2 the dead unit's selection and deletion are posted here, after the pick.
pub fn lasso(ids: &[i64]) {
    let dead = {
        let mut s = shared();
        if !s.boxed.armed {
            return;
        }
        s.boxed.armed = false;
        s.boxed.lasso = ids.to_vec();
        s.boxed.phase = 3;
        if s.boxed.mode == 2 {
            s.boxed.dead
        } else {
            -1
        }
    };
    if dead >= 0 {
        // (the lock is released: these messages' ctors reach this hook too, disarmed now)
        Call::new(Op::Post).i(&[0]).ids(&[dead]).go();
        Call::new(Op::Post).i(&[1]).go(); // EEMKillUnit(0): the first selected unit
    }
}

/// The unit `id` of `player`'s list when it is its own complex unit.
pub fn own_complex<'t>(t: &'t WorldRead, player: u64, id: i64) -> Option<UnitView<'t>> {
    let handle = Call::new(Op::PlayerUnit).h(player).i(&[id]).go() as u64;
    // SAFETY: the player's list names live units (the world's read lock is held)
    let u = unsafe { UnitView::of(t, handle) }?;
    (u.complex_unit_handle() == handle).then_some(u)
}

/// The handles of the local player's active group (None: no player or group).
pub fn active_group() -> Option<Vec<u64>> {
    let a = Call::new(Op::ActiveGroup).run();
    (a.result != 0).then(|| a.sink.ids.iter().map(|&h| h as u64).collect())
}

/// The view's rectangle (left, top, right, bottom).
pub fn view_rect() -> GeRectangle {
    let a = Call::new(Op::ViewRect).run();
    GeRectangle { left: a.f(0), top: a.f(1), right: a.f(2), bottom: a.f(3) }
}

/// The selection lines (under the world's read lock, never under the UI lock: the UI thread takes the two the other
/// way round) -> the text for the ui-state file.
pub fn snapshot() -> String {
    let need = host::EE_FIXTURE_STATE_PHYSICS | host::EE_FIXTURE_STATE_MAP | host::EE_FIXTURE_STATE_LOCK;
    if host::state() & need != need {
        return String::new();
    }
    let text = host::with_read_lock(|| {
        // SAFETY: the world's read lock is held
        let t = unsafe { WorldRead::begin() };
        let player = WorldView::global(&t)?.current_player()?;
        let handles = active_group()?;
        let mut group = Vec::new();
        let (mut candidate, mut fallback) = (-1i64, -1i64); // a living unit it may delete; else any living one
        for &h in &handles {
            // SAFETY: the group's units are live (the read lock)
            let Some(u) = (unsafe { UnitView::of(&t, h) }) else { continue };
            let id = i64::from(u.id());
            group.push(id);
            let living = u.valid() && u.hit_points() > 0;
            if candidate < 0 && living && Call::new(Op::CanDelete).h(h).go() != 0 {
                candidate = id;
            }
            if fallback < 0 && living {
                fallback = id;
            }
        }
        if candidate < 0 {
            candidate = fallback;
        }
        let boxed = {
            let mut s = shared();
            s.group = group.clone();
            s.candidate = candidate;
            s.boxed.clone()
        };
        let mut out = format!("selection {}\n", text::ids(group.clone()));
        if boxed.seq != 0 {
            let mut expected = Vec::new();
            if boxed.mode == 1 {
                for &id in &boxed.before {
                    if own_complex(&t, player.handle(), id).is_some_and(|u| u.valid()) {
                        expected.push(id);
                    }
                }
            }
            if boxed.phase == 3 {
                for &id in &boxed.lasso {
                    let Some(u) = own_complex(&t, player.handle(), id) else { continue };
                    if u.is_built()
                        && u.class().map_or(0, |c| c.unit_type()) != EE_FIXTURE_TYPE_PATH_POINT
                        && u.valid()
                        && !expected.contains(&id)
                    {
                        expected.push(id);
                    }
                }
            }
            let mut sorted_group = group.clone();
            let mut sorted_expected = expected.clone();
            sorted_group.sort_unstable();
            sorted_expected.sort_unstable();
            let matched = boxed.phase == 3 && sorted_group == sorted_expected;
            let mut dead_state = "none";
            if boxed.dead >= 0 {
                let h = Call::new(Op::PlayerUnit).h(player.handle()).i(&[boxed.dead]).go() as u64;
                // SAFETY: as above
                dead_state = match unsafe { UnitView::of(&t, h) } {
                    None => "gone",
                    Some(u) if !u.valid() => "invalid",
                    Some(u) if u.hit_points() <= 0 => "dying",
                    Some(_) => "alive",
                };
            }
            let dead_in_lasso = boxed.dead >= 0 && boxed.lasso.contains(&boxed.dead);
            out.push_str(&format!(
                "boxselect seq {} mode {} phase {} lasso {} before {} expected {} match {} dead {} dead_in_lasso {} dead_state {}\n",
                boxed.seq,
                boxed.mode,
                boxed.phase,
                text::ids(boxed.lasso.clone()),
                text::ids(boxed.before.clone()),
                text::ids(expected),
                i32::from(matched),
                boxed.dead,
                i32::from(dead_in_lasso),
                dead_state
            ));
        }
        Some(out)
    });
    let Some(Some(mut out)) = text else { return String::new() };
    let v = view_rect();
    out.push_str(&format!("view {} {} {} {}\n", v.left as i64, v.top as i64, v.right as i64, v.bottom as i64));
    out
}

/// The selection control's state as the UI thread last left it: the form's mouse captured, dragging, the drag's end.
fn control_state(control: u64) -> (bool, bool, Ge2DPoint) {
    let a = host::with_ui_lock(|| Call::new(Op::SelectState).h(control).run());
    (a.i(0) != 0, a.i(1) != 0, Ge2DPoint { x: a.f(0), y: a.f(1) })
}

/// Polls `reached` until it holds -> true; false (and a note on stderr) when it does not within 20 s. `waits` gets
/// " <stage> <ms>".
fn wait(seq: u64, stage: &str, mut reached: impl FnMut() -> bool, waits: &mut String) -> bool {
    let start = Instant::now();
    let mut ok;
    loop {
        ok = reached();
        if ok || start.elapsed() >= Duration::from_secs(20) {
            break;
        }
        host::sleep_ms(5);
    }
    waits.push_str(&format!(" {stage} {}", start.elapsed().as_millis()));
    if !ok {
        host::stderr(&format!("BOXSELECT seq {seq} stalled at {stage}\n"));
    }
    ok
}

fn modifiers() -> i64 {
    Call::new(Op::Modifiers).go()
}

#[allow(clippy::too_many_arguments)]
fn drag(seq: u64, mode: i64, x0: f32, y0: f32, x1: f32, y1: f32, dead: i64, control: u64) {
    const LEFT_SHIFT: i64 = 0x2a; // DIK_LSHIFT
    let mut waits = String::new();
    let mut reached = if mode == 1 {
        host::input(host::EE_FIXTURE_KEY_DOWN, LEFT_SHIFT);
        wait(seq, "shift-held", || modifiers() == 1, &mut waits)
    } else {
        wait(seq, "no-modifier", || modifiers() == 0, &mut waits)
    };
    if reached {
        host::mouse_set(x0, y0);
        host::ui_update();
        host::input(host::EE_FIXTURE_MOUSE_DOWN, 1);
        reached = wait(seq, "press", || control_state(control).0, &mut waits);
    }
    if reached {
        for k in 1..=12 {
            // as a player drags (the UI may see any number of these points)
            host::mouse_set(x0 + (x1 - x0) * k as f32 / 12.0, y0 + (y1 - y0) * k as f32 / 12.0);
            host::sleep_ms(50);
        }
        let end = ee_render::gfx::select::drag_end(Ge2DPoint { x: x1, y: y1 }, &view_rect());
        reached = wait(
            seq,
            "drag-end",
            || {
                let (_, dragging, at) = control_state(control);
                dragging && at == end
            },
            &mut waits,
        );
    }
    {
        let mut s = shared();
        s.boxed.armed = reached;
        s.boxed.phase = 2;
    }
    host::input(host::EE_FIXTURE_MOUSE_UP, 1);
    if reached {
        reached = wait(seq, "release", || !control_state(control).0, &mut waits);
    }
    if mode == 1 {
        host::input(host::EE_FIXTURE_KEY_UP, LEFT_SHIFT);
        let released = wait(seq, "shift-released", || modifiers() == 0, &mut waits);
        reached = reached && released;
    }
    shared().boxed.armed = false;
    DRAGGING.store(false, Ordering::SeqCst);
    host::stderr(&format!("BOXSELECT seq {seq} waits (ms):{waits}\n"));
    host::stderr(&format!(
        "TEST UI {} boxselect control {mode} value {dead} on form 0 (seq {seq}, top form 0)\n",
        if reached { "box-released" } else { "box-stalled" }
    ));
}

/// The boxselect command (under the UI lock, from the poll) -> its verdict.
pub fn start(seq: u64, line: &[u8], form: u64) -> &'static str {
    let mut s = text::Scan::new(line);
    let (_, _, _) = (s.ulong(), s.word(31), s.ulong());
    let (mode, x0, y0, x1, y1) = (s.long(), s.long(), s.long(), s.long(), s.long());
    let need = host::EE_FIXTURE_STATE_MOUSE | host::EE_FIXTURE_STATE_KEYBOARD;
    let (Some(mode), Some(x0), Some(y0), Some(x1), Some(y1)) = (mode, x0, y0, x1, y1) else { return "rejected" };
    let mode = mode as i64;
    if s.fields != 8 || !(0..=2).contains(&mode) || host::state() & need != need {
        return "rejected";
    }
    let control = Call::new(Op::SelectControl).h(form).go() as u64;
    if control == 0 {
        return "rejected";
    }
    if DRAGGING.swap(true, Ordering::SeqCst) {
        return "box-busy";
    }
    let dead = {
        let mut s = shared();
        if mode == 2 && s.candidate < 0 {
            DRAGGING.store(false, Ordering::SeqCst);
            return "box-no-candidate";
        }
        let dead = if mode == 2 { s.candidate } else { -1 };
        let before = s.group.clone();
        s.boxed = BoxState { seq, mode, phase: 1, armed: false, dead, lasso: Vec::new(), before };
        dead
    };
    let (x0, y0, x1, y1) = (x0 as f32, y0 as f32, x1 as f32, y1 as f32);
    std::thread::spawn(move || drag(seq, mode, x0, y0, x1, y1, dead, control));
    "box-started"
}
