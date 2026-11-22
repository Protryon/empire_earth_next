//! The main panel group in Rust over one-statement C++ shims (UI lane round 8; the shims are `ee_screenscpp_mg_*`
//! in `User Interface Thread/EEUIFMGMainGroup.cpp`): its host (`ee_screenscpp_main_group`'s ops that the records
//! and the views do not answer), `ShowCommands` (the panel's buttons for the selected unit; the layout itself is
//! `command_panel.rs`'), the hot keys of a status button (`EEMainGroupHotKey`) and the hot key event's
//! `ProcessEvent`. `group` is an `EEUIFMGMainGroup*`, units are `EEUnit*`, buttons `EEUIFMGStatusButton*`.

// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts to the shims' `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_ulong;

use crate::screens::ui_ids::EEMainGameControl;
use crate::screens::command_panel::*;
use crate::screens::game_cpp::{Cpp, EeScreensShimText, EeScreensShimULongOut};
use crate::screens::main_group::*;
use crate::screens::status_button_calls;

/// `kMainGameActionButton1`: the stop button's control.
pub(crate) const EE_SCREENS_MGC_ACTION_BUTTON1: u32 = EEMainGameControl::kMainGameActionButton1 as u32;
/// The timed rally buttons (`kMainGameTimedFighterRally`, `kMainGameTimedBomberRally`,
/// `kMainGameTimedAtomicBomberRally`).
pub(crate) const EE_SCREENS_MGC_TIMED_RALLIES: [u32; 3] = [
    EEMainGameControl::kMainGameTimedFighterRally as u32,
    EEMainGameControl::kMainGameTimedBomberRally as u32,
    EEMainGameControl::kMainGameTimedAtomicBomberRally as u32,
];
/// `kMainGameUnitIconBar`.
pub(crate) const EE_SCREENS_MGC_UNIT_ICON_BAR: u32 = EEMainGameControl::kMainGameUnitIconBar as u32;

/// One host op of the group (0 for an op the host does not know, as the C++'s).
pub(crate) fn call(c: &mut impl Cpp, group: usize, op: u32, a: i64, b: i64, cc: i64) -> i64 {
    match op {
        EE_SCREENS_MGR_OP_FORMS => return if a == 0 { c.mg_owner(group) } else { c.mg_main_form() } as i64,
        EE_SCREENS_MGR_OP_WORLD_UNIT => return c.mg_world_unit(a as u32) as i64,
        EE_SCREENS_MGR_OP_PLAYER_UNIT => return c.mg_player_unit(a as u32) as i64,
        EE_SCREENS_MGR_OP_CLOAK => {
            if a == 0 {
                c.mg_remove_cloak(b as usize)
            } else {
                c.mg_add_cloak(b as usize)
            }
        }
        EE_SCREENS_MGR_OP_BOX => match a {
            EE_SCREENS_MGR_BOX_FILL => c.mg_box_fill(group, b as usize),
            EE_SCREENS_MGR_BOX_REFRESH => c.mg_box_refresh(group, b as usize),
            EE_SCREENS_MGR_BOX_HIDE => c.mg_box_hide(group),
            _ => c.mg_box_queued(group, b as usize),
        },
        EE_SCREENS_MGR_OP_BOX_BIND => c.mg_box_bind(group, a as u32, b as u32),
        EE_SCREENS_MGR_OP_BOX_PAIR => c.mg_box_pair(group, a as u32),
        EE_SCREENS_MGR_OP_ICON_BAR => {
            let bar = c.mg_control(group, EE_SCREENS_MGC_UNIT_ICON_BAR);
            if bar != 0 {
                c.mg_icon_bar_update(bar);
            }
        }
        EE_SCREENS_MGR_OP_SHOW_COMMANDS => {
            show_commands(c, group, a as usize, |which, unit, b| member(group, which, unit, b))
        }
        EE_SCREENS_MGR_OP_BUTTON_OBJECT => return (c.mg_button_object(a as u32) != 0) as i64,
        EE_SCREENS_MGR_OP_SET_COMMAND => {
            let button = c.mg_button(group, a as u32);
            status_button_calls::set_command(c, button, b as u32);
        }
        EE_SCREENS_MGR_OP_BEHAVIOR => {
            for k in [0, 1, 2, 5] {
                if c.mg_unit_behavior_is(a as usize, k) {
                    return k;
                }
            }
            return -1;
        }
        EE_SCREENS_MGR_OP_FORMATION => return c.mg_formation_type(),
        EE_SCREENS_MGR_OP_VERIFY => c.mg_verify(a),
        EE_SCREENS_MGR_OP_HAS_HOT_KEY => return c.mg_object_has_hot_key(a as u32) as i64,
        EE_SCREENS_MGR_OP_HOT_KEY => {
            let key = c.mg_object_hot_key(a as u32);
            let button = c.mg_button(group, cc as u32);
            hot_key(c, group, key, button, b);
        }
        EE_SCREENS_MGR_OP_TIME_LEFT => return c.mg_time_left(a as usize, b),
        EE_SCREENS_MGR_OP_TIMER_TEXT => {
            let (mut minutes, mut seconds): (c_ulong, c_ulong) = (0, 0);
            c.mg_minutes_seconds(
                b as u32,
                EeScreensShimULongOut::of(&mut minutes),
                EeScreensShimULongOut::of(&mut seconds),
            );
            let text = format!("{minutes}:{seconds:02}");
            c.mg_show(group, a as u32);
            c.mg_timer_text(group, a as u32, EeScreensShimText::of(text.as_bytes()));
        }
        EE_SCREENS_MGR_OP_QUEUE_SIZE => return c.mg_queue_size(a as usize),
        EE_SCREENS_MGR_OP_QUEUE_KEY => return c.mg_queue_key(a as usize, cc as u32),
        _ => {}
    }
    0
}

/// `EEMainGroupHotKey`: the hot key `key` pressing the status button `button` (`how` 0: always firing, its
/// events), added to the form's group 2 (1), or its copy with Shift held, pressing the button, added (2).
pub(crate) fn hot_key(c: &mut impl Cpp, group: usize, key: usize, button: usize, how: i64) {
    match how {
        0 => {
            c.mg_hot_key_always_fire(key);
            hot_key_events(c, key, button);
        }
        1 => c.mg_add_hot_key(group, key),
        _ => {
            let copy = c.mg_hot_key_copy(group, key);
            c.mg_hot_key_shift(copy);
            hot_key_events(c, copy, button);
            c.mg_add_hot_key(group, copy);
            c.mg_hot_key_delete(copy);
        }
    }
}

/// A hot key's pressed and released events (`EEUIEMGHotKeyStatusButton`) on the button.
fn hot_key_events(c: &mut impl Cpp, key: usize, button: usize) {
    let event = c.mg_hot_key_event_new(button, true);
    c.mg_hot_key_pressed_event(key, event);
    c.event_dereference(event);
    let event = c.mg_hot_key_event_new(button, false);
    c.mg_hot_key_unpressed_event(key, event);
    c.event_dereference(event);
}

/// `EEUIEMGHotKeyStatusButton::ProcessEvent`: the button pressed (the saved groups' state reset) or released.
pub(crate) fn hot_key_event(c: &mut impl Cpp, event: usize) {
    let button = c.mg_event_button(event);
    if button == 0 {
        return;
    }
    if c.mg_event_pressed(event) {
        c.mg_button_hot_key_down(button);
        c.mg_saved_groups_reset();
    } else {
        status_button_calls::hot_key_up(c, button);
    }
}

/// `EEUIFMGMainGroup::ShowCommands`: the command and research buttons of the player's selected unit (civ power
/// buttons, the class's commands, its available techs with hot keys, the airport's timers); all hidden for others.
/// `member` is the group's own member call (`ee_screens_main_group_member`: which, unit, b).
pub(crate) fn show_commands(c: &mut impl Cpp, group: usize, unit: usize, mut member: impl FnMut(u32, usize, i64)) {
    let complex = if unit != 0 { c.mg_unit_complex(unit) } else { 0 };
    c.mg_remove_hot_keys(group);
    for control in (0x6e..0xce).chain(0x4e..0x57) {
        c.mg_hide(group, control);
        let button = c.mg_button(group, control);
        status_button_calls::release(c, button);
    }
    for control in EE_SCREENS_MGC_TIMED_RALLIES {
        c.mg_hide(group, control);
    }
    let local = c.mg_current_player();
    let player = if c.mg_player_observer(local) { 0 } else { local };
    let mut view = EeScreensPanelView { unit: unit != 0, complex: complex != 0, ..Default::default() };
    if unit != 0 && complex != 0 {
        view.built = c.mg_complex_built(complex);
        view.alive = c.mg_unit_alive(unit);
        view.commandable = c.mg_complex_commandable(complex);
        view.owned = c.mg_unit_player(unit) == player;
    }
    view.cancel_hidden = c.mg_cancel_hidden(group);
    view.player = player != 0;
    if !panel_gate(&view) {
        return;
    }
    view.human = c.mg_player_human(player);
    if !panel_shown(&view) {
        return;
    }
    let mode = c.mg_mode(group);
    view.mode = mode as _;
    if mode == 1 || mode == 3 {
        view.fanatic = c.mg_player_fanaticism(player) && c.mg_unit_in_fanatic_set(unit);
        view.jit = c.mg_player_jit(player) && c.mg_complex_rally(complex);
        view.jit_on = c.mg_player_jit_on(player);
        let objects = [c.mg_button_object(0x28), c.mg_button_object(0x29), c.mg_button_object(0x2a)];
        let mut position = |o: usize| if o != 0 { c.mg_object_position(o) as _ } else { 0 };
        view.fanatic_position = position(objects[0]);
        view.jit_positions = [position(objects[1]), position(objects[2])];
    } else if mode == 4 {
        for j in 0..8 {
            let object = c.mg_button_object(0x5dc + j as u32);
            view.trade_positions[j] = c.mg_object_position(object) as _;
        }
    }
    for k in 0..9 {
        view.commands[k] = c.mg_class_command(unit, k as u32) as _;
    }
    let mut research = Vec::new();
    let mut nodes = Vec::new();
    if mode == 2 || mode == 3 {
        let count = c.mg_research_count(unit);
        for m in 0..count {
            let technology = c.mg_research_tech(unit, m);
            let node = c.mg_player_node(player, technology);
            let object = c.mg_node_object(node);
            let position = c.mg_object_position(object);
            let command = c.mg_object_id(object);
            let kind = c.sbt_node_kind(node);
            let eligible = (0..10000).contains(&position)
                && c.mg_node_available(node)
                && (c.mg_node_allowed(node) || c.mg_research_queued(complex, node));
            research.push(EeScreensPanelResearch {
                position: position as _,
                command: command as _,
                kind: kind as _,
                eligible,
            });
            nodes.push(node);
        }
    }
    let capacity = 8 + 2 + 9 + research.len() + 1;
    for entry in panel_buttons(&view, &research).into_iter().take(capacity) {
        if entry.kind == EE_SCREENS_PANEL_COMMAND {
            let b = entry.button as i64 | (entry.control as i64) << 32;
            member(EE_SCREENS_MGR_DO_COMMAND_BUTTON, unit, b);
        } else if entry.kind == EE_SCREENS_PANEL_STOP {
            let button = c.mg_button(group, EE_SCREENS_MGC_ACTION_BUTTON1);
            status_button_calls::set_command(c, button, 3);
            c.mg_show(group, EE_SCREENS_MGC_ACTION_BUTTON1);
        } else {
            let m = entry.research as usize;
            let node = nodes[m];
            let control = entry.control as u32;
            let button = c.mg_button(group, control);
            let technology = c.mg_research_tech(unit, m as i64);
            let (kind, id) = (c.mg_unit_class_type(unit), c.mg_unit_id(unit));
            status_button_calls::set_tech(c, button, technology, kind, id);
            c.mg_show(group, control);
            if c.sbt_node_hot_key(node) != 0 {
                for how in [0, 2, 1] {
                    let key = c.sbt_node_hot_key(node);
                    hot_key(c, group, key, button, how);
                }
            }
        }
    }
    if c.mg_complex_airport(complex) {
        member(EE_SCREENS_MGR_DO_SPECIAL_BUTTONS, unit, 0);
    }
}

/// The group's member `which` (`ee_screens_main_group_member`; none in a test binary).
fn member(group: usize, which: u32, unit: usize, b: i64) {
    #[cfg(not(test))]
    crate::screens::main_group_ffi::ee_screens_main_group_member(group as *mut _, which, unit as *mut _, b);
    #[cfg(test)]
    let _ = (group, which, unit, b);
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUIFMGMainGroup::ShowCommands(unit)`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mg_show_commands(group: usize, unit: usize) {
        super::show_commands(&mut Ffi, group, unit, |which, unit, b| super::member(group, which, unit, b))
    }
    /// `EEUIEMGHotKeyStatusButton::ProcessEvent`.
    #[no_mangle]
    pub extern "C" fn ee_screens_mg_hot_key_event(event: usize) {
        super::hot_key_event(&mut Ffi, event)
    }
}

#[cfg(test)]
#[path = "main_group_calls_tests.rs"]
mod tests;
