//! The fixture's compound operations (game core round 9): what EETestFixture.cpp's host computed in a case of several
//! statements is done here over finer operations, one C++ statement each ([`EeFixtureOp`]'s second half). The callers
//! keep their operations: [`super::host::Call::run`] answers a compound one here, with the same result, outputs and
//! sink the C++ case gave.
//!
//! Only harness sessions (the tour, gates.sh's games) run the fixture.

use ee_base::plat_core::sim::{gPlatSimNoHumanDefeat, gPlatSimPortOffset, gPlatSimSeed, gPlatSimSeedSet};

use super::host::{Call, EeFixtureCall, EeFixtureOp as Op, EeFixtureSink};

use ee_base::plat_core::time::{PlatSimGetTimeFactor, PlatTimeMilliseconds};

/// A fine operation on handle `h`.
fn on(op: Op, h: u64) -> Call {
    Call::new(op).h(h)
}

/// A fine operation's result.
fn go(op: Op, h: u64, i: &[i64]) -> i64 {
    on(op, h).i(i).go()
}

/// A fine operation's float answer (its f0).
fn fl(op: Op, h: u64, i: &[i64]) -> f32 {
    on(op, h).i(i).run().f(0)
}

/// The control `k` of form `form` (0: none: no form or no such control).
fn control_of(form: u64, k: i64) -> u64 {
    if form == 0 || (k as u32 as i64) >= go(Op::FxFormControlCount, form, &[]) {
        return 0;
    }
    go(Op::FxFormControl, form, &[k]) as u64
}

/// A list's first 64 lines into the sink.
fn lines(list: u64, sink: &mut EeFixtureSink) {
    if list == 0 {
        return;
    }
    let count = go(Op::FxListLineCount, list, &[]);
    for k in 0..count.min(64) {
        sink.texts.extend(on(Op::FxListLine, list).i(&[k]).run().sink.texts);
    }
}

/// A combo's or list's current selection (32-bit; 0xffffffff none).
fn selection(op: Op, h: u64) -> i64 {
    on(op, h).i(&[0xffff_ffff]).run().i(0) as u32 as i32 as i64
}

/// The compound operations: Some(result) when `call` is one (its outputs written), None for a host operation.
pub(super) fn run(call: &mut EeFixtureCall, sink: &mut EeFixtureSink) -> Option<i64> {
    let i = call.i;
    let c = call;
    let result = match c.op {
        Op::SetSwitch => {
            if i[0] == 0 {
                gPlatSimSeed.set(i[1] as u32);
                gPlatSimSeedSet.set(1);
            } else if i[0] == 1 {
                gPlatSimNoHumanDefeat.set(i[1] as i32);
            } else {
                gPlatSimPortOffset.set(i[1] as i32);
            }
            0
        }
        Op::Install => {
            if i[0] == 0 {
                // the game's debug output (PlatDebugOutput: GX's log, EE-AI lines, ...) to stderr
                ee_base::plat_core::sim::PlatDebugSetHandler(Some(debug_to_stderr));
                // the fault report (sim_driver fault.rs, EE-SIM's own)
                // SAFETY: a NUL-ended text
                unsafe { crate::sim_driver::fault::ee_sim_driver_fault_install(c"EE-AOC".as_ptr()) };
            } else {
                go(Op::FxInstallLasso, 0, &[]);
                // each executed input's view (gEEMessageExecutedTestHook, gamecpp globals.rs)
                let hook: unsafe extern "C" fn(*mut std::ffi::c_void, i32) = message_hook;
                // SAFETY: the physics' hook pointer, set before a game runs
                unsafe {
                    *std::ptr::addr_of_mut!(crate::gamecpp::globals::gEEMessageExecutedTestHook) =
                        hook as *mut std::ffi::c_void
                };
            }
            0
        }
        Op::Clock => {
            let (mut num, mut den) = (1u32, 1u32);
            // SAFETY: two out values
            unsafe { PlatSimGetTimeFactor(&mut num, &mut den) };
            c.i[0] = num as i64;
            c.i[1] = den as i64;
            c.i[2] = PlatTimeMilliseconds() as i64;
            0
        }
        Op::UiLock => {
            go(if i[0] != 0 { Op::FxUiLockOn } else { Op::FxUiLockOff }, 0, &[]);
            0
        }
        Op::TopForm => {
            let form = go(Op::FxCurrentForm, 0, &[]) as u64;
            let over = if form != 0 { go(Op::FxFormMouseOver, form, &[]) as u64 } else { 0 };
            c.i[0] = if form != 0 { go(Op::FxFormId, form, &[]) } else { 0 };
            c.i[1] = if form != 0 { go(Op::FxFormControlCount, form, &[]) } else { 0 };
            c.i[2] = if over != 0 { go(Op::FxControlIndex, over, &[]) } else { -1 };
            let count = go(Op::FxFormCount, 0, &[]);
            for k in 0..count.min(64) {
                let stacked = go(Op::FxStackedForm, 0, &[k]) as u64;
                if stacked != 0 {
                    sink.ids.push(go(Op::FxFormId, stacked, &[]));
                }
            }
            form as i64
        }
        Op::Window => {
            // no screen while the rasterizer is re-activated (a settings change)
            let screen = go(Op::FxUiScreen, 0, &[]);
            let window =
                if screen < go(Op::FxScreenCount, 0, &[]) { go(Op::FxWindowAt, 0, &[screen]) as u64 } else { 0 };
            if window == 0 {
                0
            } else {
                c.i[0] = go(Op::FxWindowWidth, window, &[]);
                c.i[1] = go(Op::FxWindowHeight, window, &[]);
                1
            }
        }
        Op::Control => {
            let control = control_of(c.h, i[0]);
            if control == 0 {
                -1
            } else {
                c.i[1] = go(Op::FxHasTrigger, control, &[]);
                let combo = go(Op::FxAsCombo, control, &[]) as u64;
                let check = if combo == 0 { go(Op::FxAsCheck, control, &[]) as u64 } else { 0 };
                let list = if combo == 0 && check == 0 { go(Op::FxAsListString, control, &[]) as u64 } else { 0 };
                if combo != 0 {
                    c.i[2] = selection(Op::FxComboSelection, combo);
                    let box_ = go(Op::FxComboListBox, combo, &[]) as u64;
                    c.i[3] = if box_ != 0 { go(Op::FxListLineCount, box_, &[]) } else { 0 };
                    lines(box_, sink);
                    1
                } else if check != 0 {
                    c.i[2] = go(Op::FxCheckIsChecked, check, &[]);
                    2
                } else if list != 0 {
                    c.i[2] = selection(Op::FxListSelection, list);
                    c.i[3] = go(Op::FxListLineCount, list, &[]);
                    lines(list, sink);
                    3
                } else {
                    0
                }
            }
        }
        Op::Click => {
            let control = control_of(c.h, i[0]);
            if control == 0 || go(Op::FxHasTrigger, control, &[]) == 0 {
                0
            } else {
                let radio = go(Op::FxAsRadio, control, &[]) as u64;
                if radio != 0 {
                    go(Op::FxRadioCheck, radio, &[]);
                }
                go(Op::FxFireTrigger, control, &[]);
                1
            }
        }
        Op::ComboSelect => {
            let combo = go(Op::FxAsCombo, control_of(c.h, i[0]), &[]) as u64;
            let box_ = if combo != 0 { go(Op::FxComboListBox, combo, &[]) as u64 } else { 0 };
            if box_ == 0 || (i[1] as u32 as i64) >= go(Op::FxListLineCount, box_, &[]) {
                0
            } else {
                go(Op::FxComboSetSelection, combo, &[i[1]]);
                1
            }
        }
        Op::CheckSet => {
            let check = go(Op::FxAsCheck, control_of(c.h, i[0]), &[]) as u64;
            if check != 0 {
                go(Op::FxCheckSet, check, &[i[1]]);
            }
            (check != 0) as i64
        }
        Op::EditNumber => {
            // the editor exercise's campaign name
            let edit = go(Op::FxAsEdit, control_of(c.h, i[0]), &[]) as u64;
            if edit != 0 {
                go(Op::FxEditSetNumber, edit, &[i[1]]);
            }
            (edit != 0) as i64
        }
        Op::ListSelect => {
            let list = go(Op::FxAsListString, control_of(c.h, i[0]), &[]) as u64;
            if list == 0 || (i[1] as u32 as i64) >= go(Op::FxListLineCount, list, &[]) {
                0
            } else {
                go(Op::FxListChangeSelection, list, &[i[1]]);
                1
            }
        }
        Op::ClickEvent => {
            // the editor exercise: the main menu's Editors, the editors' Scenario Editor
            let count = go(Op::FxFormControlCount, c.h, &[]);
            let mut clicked = 0;
            for k in 0..count {
                let one = go(Op::FxFormControl, c.h, &[k]) as u64;
                if one != 0 && go(Op::FxScreenEventOf, one, &[]) as u32 == i[0] as u32 {
                    go(Op::FxFireTrigger, one, &[]);
                    clicked = 1;
                    break;
                }
            }
            clicked
        }
        Op::MouseSet | Op::MouseFix => {
            if go(Op::FxMousePresent, 0, &[]) == 0 {
                if c.op == Op::MouseSet {
                    0
                } else {
                    -1
                }
            } else {
                let (x, y) = (c.f[0], c.f[1]);
                let mut result = 1;
                go(Op::FxMouseLock, 0, &[]);
                if c.op == Op::MouseSet || fl(Op::FxMouseX, 0, &[]) != x || fl(Op::FxMouseY, 0, &[]) != y {
                    Call::new(Op::FxMouseSetPosition).f(&[x, y]).go();
                } else {
                    result = 0;
                }
                c.f[0] = fl(Op::FxMouseX, 0, &[]);
                c.f[1] = fl(Op::FxMouseY, 0, &[]);
                go(Op::FxMouseUnlock, 0, &[]);
                result
            }
        }
        Op::Modifiers => match go(Op::FxModifierState, 0, &[]) {
            0 => 0,
            s if s == go(Op::FxShiftModifier, 0, &[]) => 1,
            _ => 2,
        },
        Op::SelectState => {
            // as UpdateMouse writes it
            c.i[0] = go(Op::FxSelectCaptured, c.h, &[]);
            c.i[1] = go(Op::FxSelectDragging, c.h, &[]);
            c.f[0] = fl(Op::FxSelectDragX, c.h, &[]);
            c.f[1] = fl(Op::FxSelectDragY, c.h, &[]);
            0
        }
        Op::ViewRect => {
            for k in 0..4 {
                c.f[k] = fl(Op::FxViewSide, 0, &[k as i64]);
            }
            0
        }
        Op::Post => {
            let post = match i[0] {
                0 => Op::FxPostSelect,
                1 => Op::FxPostKill,
                2 => Op::FxPostPath,
                _ => Op::FxPostStop,
            };
            let ids = if c.ids.is_null() || c.n == 0 {
                Vec::new()
            } else {
                // SAFETY: the caller's ids, live through the call
                unsafe { std::slice::from_raw_parts(c.ids, c.n) }.to_vec()
            };
            Call::new(post).f(&[c.f[0], c.f[1]]).ids(&ids).go();
            0
        }
        Op::CenterCamera => {
            // posted outside the world's lock, as the mini map's click does
            let action = go(Op::FxNewCenterCamera, 0, &[]) as u64;
            go(Op::FxCenterCameraAt, action, &[i[0], i[1]]);
            go(Op::FxActionDereference, action, &[]);
            0
        }
        Op::StatusButton => {
            let mut found = 0;
            go(Op::FxUiLockOn, 0, &[]);
            let form = go(Op::FxCurrentForm, 0, &[]) as u64;
            let count = if form != 0 { go(Op::FxFormControlCount, form, &[]) } else { 0 };
            for k in 0..count {
                let button = go(Op::FxAsStatusButton, go(Op::FxFormControl, form, &[k]) as u64, &[]) as u64;
                if button == 0 || go(Op::FxStatusButtonShows, button, &[i[0]]) == 0 {
                    continue;
                }
                c.f[0] = fl(Op::FxStatusButtonCenter, button, &[0]);
                c.f[1] = fl(Op::FxStatusButtonCenter, button, &[1]);
                found = 1;
                break;
            }
            go(Op::FxUiLockOff, 0, &[]);
            found
        }
        Op::WorldLock => {
            if crate::sim_driver::sim_host::world_map_and_lock().1.is_null() {
                0
            } else {
                // 0 read, 1 read unlock, 2 write, 3 write unlock
                use crate::sim_driver::host::*;
                let ops =
                    [EE_SIM_DRIVER_READ_LOCK, EE_SIM_DRIVER_READ_UNLOCK, EE_SIM_DRIVER_WRITE_LOCK, EE_SIM_DRIVER_WRITE_UNLOCK];
                crate::sim_driver::sim_host::world_lock(ops[i[0].clamp(0, 3) as usize]);
                1
            }
        }
        Op::Physics => {
            if !crate::gamecpp::globals::have!(gPhysics) {
                0
            } else {
                for k in 0..3 {
                    c.i[k] = go(Op::FxPhysicsValue, 0, &[k as i64]);
                }
                1
            }
        }
        Op::Comms => {
            if go(Op::FxCommsPresent, 0, &[]) == 0 {
                0
            } else {
                for k in 0..4 {
                    c.i[k] = go(Op::FxCommsValue, 0, &[k as i64]);
                }
                1
            }
        }
        Op::Camera => {
            // plain reads: the scroll check compares polls a second apart
            if crate::sim_driver::sim_host::world_map_and_lock().0 == 0 {
                0
            } else {
                let focus = on(Op::FxCameraFocus, 0).run();
                let end = on(Op::FxCameraEndFocus, 0).run();
                c.f[0] = focus.f(0);
                c.f[1] = focus.f(1);
                c.f[2] = focus.f(2);
                c.f[3] = end.f(0);
                c.f[4] = end.f(1);
                c.f[5] = fl(Op::FxCameraZoom, 0, &[]);
                c.i[0] = go(Op::FxCameraMode, 0, &[]);
                c.i[1] = go(Op::FxMapSize, 0, &[]);
                c.i[2] = Call::new(Op::FxMeshValidPoint).f(&[c.f[0], c.f[1]]).go();
                c.i[3] = Call::new(Op::FxMeshValidPoint).f(&[c.f[3], c.f[4]]).go();
                1
            }
        }
        Op::ActiveGroup => {
            let group = go(Op::FxActiveGroup, 0, &[]) as u64;
            if group == 0 {
                0
            } else {
                for u in 0..go(Op::FxGroupUnitCount, group, &[]) {
                    sink.ids.push(go(Op::FxGroupUnit, group, &[u]));
                }
                1
            }
        }
        Op::Pick => {
            let unit = Call::new(Op::FxPickUnit).i(&[i[0]]).f(&[c.f[0], c.f[1]]).go() as u64;
            if unit != 0 {
                go(Op::FxUnitId, unit, &[])
            } else {
                -1
            }
        }
        Op::PointTile => {
            let a = Call::new(Op::FxPointUnitOrTile).f(&[c.f[0], c.f[1]]).run();
            if a.result == 0 || a.call.h2 == 0 || go(Op::FxTileSeenByLocal, a.call.h2, &[]) == 0 {
                0
            } else {
                c.f[0] = a.f(2);
                c.f[1] = a.f(3);
                c.f[2] = a.f(4);
                1
            }
        }
        Op::NearestRoute => {
            // EEMPatrol(-1): the first unit's nearest town's route (EEUnitHost town point)
            let town = go(Op::FxNearestTown, c.h, &[]) as u64;
            let route = if town != 0 { go(Op::FxTownPatrolGroup, town, &[]) as u64 } else { 0 };
            let point = if route != 0 {
                let (x, y) = (go(Op::FxUnitTileX, c.h, &[]), go(Op::FxUnitTileY, c.h, &[]));
                // SAFETY: a live patrol group (the world's lock is the caller's)
                let p = unsafe {
                    ee_sim::units::patrol_group::ee_units_pg_nearest(route as usize as *mut _, x as i32, y as i32)
                };
                p as u64
            } else {
                0
            };
            if point == 0 {
                -1
            } else {
                c.h2 = go(Op::FxUnitPatrolGroup, point, &[]) as u64;
                go(Op::FxUnitId, point, &[])
            }
        }
        Op::Goal => goal(c),
        Op::MessageValue => {
            if i[1] == 2 {
                c.f[0] = fl(Op::FxMessageFloat, c.h, &[i[0]]);
            }
            match i[1] {
                0 => go(Op::FxMessageInt, c.h, &[i[0]]),
                1 => go(Op::FxMessageBool, c.h, &[i[0]]),
                _ => 0,
            }
        }
        Op::CalamityClass => {
            let class = go(Op::FxCalamityClassAt, 0, &[i[0]]) as u64;
            if class == 0 {
                0
            } else {
                c.i[1] = go(Op::FxCalamityClassEnum, class, &[]);
                c.i[2] = go(Op::FxCalamityClassTarget, class, &[]);
                1
            }
        }
        Op::CalamityCast => {
            let kind = match i[3] {
                0 => Op::FxCastAtUnit,
                1 => Op::FxCastAtLocation,
                _ => Op::FxCastGlobal,
            };
            go(kind, 0, &[i[0], i[1], i[2], i[4], i[5]])
        }
        Op::CalamityAlive => {
            let mut alive = -1;
            for k in 0..go(Op::FxCalamityCount, 0, &[]) {
                if go(Op::FxCalamityEnumAt, 0, &[k]) as u32 == i[0] as u32 {
                    alive = go(Op::FxCalamityTimeAlive, 0, &[k]);
                }
            }
            alive
        }
        Op::BuildQueue => {
            for k in 0..go(Op::FxBuildQueueSize, c.h, &[]) {
                sink.ids.push(go(Op::FxBuildQueueAt, c.h, &[k]));
            }
            0
        }
        Op::LobbyPost => {
            // SAFETY: the call's text, NUL ended, alive through the call
            let command = if c.s.is_null() {
                String::new()
            } else {
                unsafe { std::ffi::CStr::from_ptr(c.s) }.to_string_lossy().into_owned()
            };
            super::lobby::post(&command, i[0] as u64)
        }
        Op::LobbySnapshot => super::lobby::snapshot(sink),
        _ => return None,
    };
    Some(result)
}

/// The goal kinds the fixture classes (`kGoalGather`, `kGoalLocation`, `kGoalAttackUnit`, `kGoalAttackGround`,
/// `kGoalPatrol`, `kGoalBuildRepair`, `kGoalExplore`; EETestFixture.cpp static_asserts them), in class order 0..6.
pub const FIXTURE_GOAL_KINDS: [i64; 7] = [6, 1, 2, 3, 9, 4, 10];

/// [`Op::Goal`]: goal h's kind; i0 its class, i1 its target's id, h2 its target or patrol group, (f0, f1) its point.
fn goal(c: &mut EeFixtureCall) -> i64 {
    let kind = go(Op::FxGoalKind, c.h, &[]);
    let class = FIXTURE_GOAL_KINDS.iter().position(|&k| k == kind).map_or(7, |p| p as i64);
    c.i[0] = class;
    let mut target = 0u64;
    match class {
        0 => target = go(Op::FxGoalGatherResource, c.h, &[]) as u64,
        1 | 3 => {
            let point = if class == 3 { Op::FxGoalGroundAt } else { Op::FxGoalLocationAt };
            c.f[0] = fl(point, c.h, &[0]);
            c.f[1] = fl(point, c.h, &[1]);
        }
        2 => target = go(Op::FxGoalAttackTarget, c.h, &[]) as u64,
        4 => {
            target = go(Op::FxGoalPatrolTarget, c.h, &[]) as u64;
            c.i[2] = go(Op::FxGoalPatrolGroup, c.h, &[]);
        }
        _ => {}
    }
    c.i[1] = if target != 0 { go(Op::FxUnitId, target, &[]) } else { -1 };
    c.h2 = target;
    kind
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_goal_classes_are_the_cases_order() {
        // gather 0, location 1, attack 2, ground 3, patrol 4, build 5, explore 6, other 7
        assert_eq!(FIXTURE_GOAL_KINDS.iter().position(|&k| k == 9), Some(4));
        let mut sorted = FIXTURE_GOAL_KINDS.to_vec();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sorted.len(), 7);
    }

    #[test]
    fn a_selection_is_thirty_two_bits() {
        assert_eq!(0xffff_ffffi64 as u32 as i32 as i64, -1);
    }
}

/// The debug line the debugger harness read (no debugger runs under the fixture driver): "debugstring <text>", with a
/// new line unless the text ends in one (was EETestFixture.cpp's DebugToStderr).
pub fn debug_line(text: &[u8]) -> Vec<u8> {
    let mut line = b"debugstring ".to_vec();
    line.extend_from_slice(text);
    if text.last() != Some(&10) {
        line.push(10); // a new line
    }
    line
}

/// PlatDebugSetHandler's handler: the line on the C library's stderr, flushed.
///
/// # Safety
/// `text` is NULL or NUL-ended.
unsafe extern "C" fn debug_to_stderr(text: *const std::ffi::c_char) {
    let text = if text.is_null() { &[][..] } else { unsafe { std::ffi::CStr::from_ptr(text) }.to_bytes() };
    crate::sim_driver::stdio::write(2, &debug_line(text), true);
}

/// gEEMessageExecutedTestHook (EEPhysics::DrainInput, the world's write lock held): an input message before (phase
/// 0) and after (1) it executes, and whether the local player sent it (was EETestFixture.cpp's MessageHook).
///
/// # Safety
/// `message` is the executing message.
unsafe extern "C" fn message_hook(message: *mut std::ffi::c_void, phase: i32) {
    let m = message as u64;
    let kind = go(Op::FxMessageType, m, &[]) as u64;
    let local = go(Op::FxMessageLocal, m, &[]) != 0;
    super::orders::message(m, kind, local, phase);
}

#[cfg(test)]
mod debug_line_tests {
    #[test]
    fn a_new_line_unless_the_text_ends_in_one() {
        assert_eq!(super::debug_line(b"GX up"), b"debugstring GX up\x0a");
        assert_eq!(super::debug_line(b"x\x0a"), b"debugstring x\x0a");
        assert_eq!(super::debug_line(b""), b"debugstring \x0a");
    }
}
