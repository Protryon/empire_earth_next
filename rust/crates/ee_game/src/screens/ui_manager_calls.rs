//! The game's UI manager's host cases and members over one-statement C++ shims (UI lane round 8; the shims are
//! `ee_screenscpp_um_*`, `ct_*`, `bt_*` in `User Interface Thread/EEUIManager.cpp`): the host ops of
//! `ui_manager.rs` that were more than one call (`EEUIManager::HostOp`), the event factories (`EECreateGameEvent`,
//! `EECreateFormEvent`, `EECreateWonNetServerSelectEvent`) and the members `CreateScenarioEditor`,
//! `DeleteScenarioEditor`, `CreateForm`, `CreateUserDefinedBackground` and `CreateUserDefinedControl`.

use crate::screens::ui_ids::EEFormID;
use crate::screens::factories::{EE_SCREENS_CPP_CANCEL_LOAD, EE_SCREENS_CPP_LOAD_UNIT, EE_SCREENS_CPP_WNS_OPEN};
use crate::screens::game_cpp::Cpp;
use crate::screens::save_load::EE_SCREENS_SL_ACTIVATE;
use crate::screens::ui_manager::*;

/// `kFormMainGame` (the form `CreateForm` makes as the game's class).
pub(crate) const EE_SCREENS_UMC_FORM_MAIN_GAME: u32 = EEFormID::kFormMainGame as u32;
/// `kControlEventWNSCancel` (WON's server select form's cancel).
pub const EE_SCREENS_UMC_WNS_CANCEL: u32 = 106;
/// `kBackgroundGeneric` (the background table's type the game makes).
pub const EE_SCREENS_UMC_BACKGROUND_GENERIC: u32 = 101;
/// The game's control types (`kDbControl*`, `CreateUserDefinedControl`'s cases).
pub const EE_SCREENS_UMC_STATUS_BUTTON: u32 = 0x3e8;
pub const EE_SCREENS_UMC_MINI_MAP: u32 = 0x3e9;
pub const EE_SCREENS_UMC_ERROR_LOG: u32 = 0x3eb;
pub const EE_SCREENS_UMC_ATTRIBUTE: u32 = 0x3ed;
pub const EE_SCREENS_UMC_MAIN_GAME_STATUS_BAR: u32 = 0x3ee;
pub const EE_SCREENS_UMC_SELECT_UNITS: u32 = 0x3ef;
pub const EE_SCREENS_UMC_COMMAND_UNITS: u32 = 0x3f0;
pub const EE_SCREENS_UMC_PORTRAIT: u32 = 0x3f1;
pub const EE_SCREENS_UMC_CHAT_LINE: u32 = 0x3f2;
pub const EE_SCREENS_UMC_CB_LIST_BOX: u32 = 0x3f3;
pub const EE_SCREENS_UMC_WON_STAT_BOX: u32 = 0x3f5;
pub const EE_SCREENS_UMC_UNIT_ICON_BAR: u32 = 0x3f6;
pub const EE_SCREENS_UMC_SAVED_GROUP: u32 = 0x3f7;
pub const EE_SCREENS_UMC_PGS_LINE_GRAPH: u32 = 0x3f8;
pub const EE_SCREENS_UMC_PING: u32 = 0x3f9;
pub const EE_SCREENS_UMC_WON_ROOMS_BOX: u32 = 0x3fa;
pub const EE_SCREENS_UMC_WON_GAMES_BOX: u32 = 0x3fb;

/// `EEUIManager::HostOp`'s cases of more than one call on the call record `call`; `None` for the others.
pub(crate) fn call(c: &mut impl Cpp, call: usize, op: u32, a: i64, cc: i64) -> Option<i64> {
    match op {
        EE_SCREENS_UM_OP_REGISTER => {
            let m = c.um_call_manager(call);
            c.um_register_game_event(m);
            c.um_register_wns_event(m);
            c.um_register_form_event(m);
        }
        EE_SCREENS_UM_OP_LOAD_DATABASE => {
            let m = c.um_call_manager(call);
            c.um_load_database(m);
        }
        EE_SCREENS_UM_OP_ADD_LIKE => {
            let m = c.um_call_manager(call);
            return Some(c.um_add_like(m, cc as usize));
        }
        EE_SCREENS_UM_OP_MOVE => return Some(c.um_move(cc as usize)),
        EE_SCREENS_UM_OP_SAVE_EVENT => {
            let m = c.um_call_manager(call);
            c.um_clear_u138(m);
            let event = c.screen_event_new(EE_SCREENS_SL_ACTIVATE);
            c.um_save_form_set_activate(m, event);
            c.event_dereference(event);
        }
        EE_SCREENS_UM_OP_LOBBY_LOCK => {
            if a != 0 {
                c.lobby_options_write_lock()
            } else {
                c.lobby_options_write_unlock()
            }
        }
        EE_SCREENS_UM_OP_EDITOR_ON => {
            let m = c.um_call_manager(call);
            c.um_set_in_editor(m);
            c.um_clear_multiplayer();
        }
        EE_SCREENS_UM_OP_NEW_SERVER => {
            if c.comms_present() {
                c.comms_delete();
            }
            c.um_new_server();
            c.server_start_threads();
            c.server_clear_load_responses();
        }
        EE_SCREENS_UM_OP_LOBBY_DEFAULTS => {
            c.lobby_set_map_size(1);
            c.lobby_set_load_type(3);
            c.lobby_set_epoch_prehistoric();
            c.lobby_set_variant(0);
        }
        EE_SCREENS_UM_OP_NAME => {
            if a == 0 {
                c.um_name_human(call)
            } else {
                c.um_name_computer(call)
            }
        }
        EE_SCREENS_UM_OP_SET_PLAYER => {
            // SAFETY: the record ui_manager.rs hands with the op (a live local)
            let p = unsafe { *(cc as *const EeScreensManagerPlayer) };
            c.lobby_player_reset(p.slot);
            c.lobby_player_set_type(p.slot, p.kind);
            c.um_lobby_player_set_name(call, p.slot, p.name as u32);
            c.lobby_player_set_slot(p.slot, p.slot);
            c.lobby_player_set_color(p.slot, p.color);
            c.lobby_player_set_citizens(p.slot, p.starting_citizens);
            c.lobby_player_set_local(p.slot, p.is_local != 0);
            c.lobby_player_set_ready(p.slot, p.is_ready != 0);
        }
        EE_SCREENS_UM_OP_SERVER_LOAD => {
            c.server_stop_ping();
            c.server_set_load_state(true);
            c.comms_start_loading();
        }
        EE_SCREENS_UM_OP_KEY_PRESSED => {
            c.um_key_load(a as u32);
            return Some(c.um_key_just_pressed() as i64);
        }
        EE_SCREENS_UM_OP_KEYS_FREE => {
            let m = c.um_call_manager(call);
            let form = c.um_current_form(m);
            return Some((form != 0 && !c.form_keyboard_captured(form) && !c.loading_saved_game()) as i64);
        }
        _ => return None,
    }
    Some(0)
}

/// `EECreateGameEvent`: a game form's event for a control id (the factories' code; the save/load screens' Cancel
/// opens the form the load was made for), 0 for an id no factory knows.
pub(crate) fn game_event(c: &mut impl Cpp, form: usize, id: u32) -> usize {
    let m = c.form_manager(form);
    let active = c.game_screen_active();
    let code = crate::screens::factories::control_event_for(id as _, active, c.um_in_editor(m));
    if code == EE_SCREENS_CPP_CANCEL_LOAD {
        let back = if !c.game_screen_active() { c.um_editor_form(m) } else { c.um_main_game_form(m) };
        return c.um_new_cancel(back);
    }
    if code == EE_SCREENS_CPP_LOAD_UNIT {
        return c.um_new_load_unit();
    }
    c.screen_event_new(code)
}

/// `EECreateFormEvent`: the event a form fires when it opens, closes or activates.
pub(crate) fn form_event(c: &mut impl Cpp, id: u32) -> usize {
    let code = crate::screens::factories::form_event_for(id as _);
    if code == EE_SCREENS_CPP_WNS_OPEN {
        return c.um_new_wns_open();
    }
    c.screen_event_new(code)
}

/// `EECreateWonNetServerSelectEvent`: the server select form's cancel event, 0 for other ids.
pub(crate) fn wns_event(c: &mut impl Cpp, id: u32) -> usize {
    if id == EE_SCREENS_UMC_WNS_CANCEL {
        return c.um_new_wns_cancel();
    }
    0
}

/// `EEUIManager::CreateScenarioEditor`: the editor form on first use.
pub(crate) fn create_editor(c: &mut impl Cpp, m: usize) {
    if c.um_editor_form(m) == 0 {
        c.um_new_editor(m);
    }
}

/// `EEUIManager::DeleteScenarioEditor`.
pub(crate) fn delete_editor(c: &mut impl Cpp, m: usize) {
    c.um_delete_editor(m);
    c.um_clear_editor(m);
}

/// `EEUIManager::CreateForm`: the main game form as the game's class, others as the base manager makes them.
pub(crate) fn create_form(c: &mut impl Cpp, m: usize, data: usize) -> usize {
    if c.um_form_data_id(data) != EE_SCREENS_UMC_FORM_MAIN_GAME {
        return c.um_base_create_form(m, data);
    }
    c.um_new_main_game(m, data)
}

/// `EEUIManager::CreateUserDefinedBackground`: the framed generic background of a table entry (flags from its
/// attributes), 0 for another type or a form that is missing.
pub(crate) fn create_background(c: &mut impl Cpp, m: usize, table: usize) -> usize {
    if c.bt_type(table) != EE_SCREENS_UMC_BACKGROUND_GENERIC {
        return 0;
    }
    let id = c.bt_form_id(table);
    let form = c.um_form_by_id(m, id);
    if form == 0 {
        return 0;
    }
    let flags =
        c.bt_attribute1(table) as u32 | (c.bt_attribute2(table) as u32) << 1 | (c.bt_attribute3(table) as u32) << 2;
    c.um_new_generic_background(m, table, form, flags)
}

/// `EEUIManager::CreateUserDefinedControl`: the game's own control of a controls table entry (its rectangle laid
/// out in the window, its strings set up), 0 for a type the game does not make.
pub(crate) fn create_control(c: &mut impl Cpp, m: usize, t: usize) -> usize {
    let id = c.ct_form_id(t);
    let form = c.um_form_by_id(m, id);
    let (width, height) = (c.um_window_width(m), c.um_window_height(m));
    let exclusive = if c.form_exclusive_coords(form) { 0.0f32 } else { 1.0f32 };
    c.um_parent_set(0.0, 0.0, width as f32 - exclusive, height as f32 - exclusive);
    c.ct_layout_rectangle(t, form);
    c.ct_strings(m, t);
    let control = match c.ct_type(t) {
        EE_SCREENS_UMC_MINI_MAP => c.ct_new_mini_map(t, form),
        EE_SCREENS_UMC_STATUS_BUTTON => c.ct_new_status_button(m, t),
        EE_SCREENS_UMC_ERROR_LOG => c.ct_new_error_log(t, form),
        EE_SCREENS_UMC_ATTRIBUTE => c.ct_new_attribute(t, form),
        EE_SCREENS_UMC_MAIN_GAME_STATUS_BAR => c.ct_new_status_bar(t, form),
        EE_SCREENS_UMC_SELECT_UNITS => c.ct_new_select_units(m, t, form),
        EE_SCREENS_UMC_COMMAND_UNITS => c.ct_new_command_units(m, t, form),
        EE_SCREENS_UMC_PORTRAIT => c.ct_new_portrait(t, form),
        EE_SCREENS_UMC_CHAT_LINE => crate::screens::shell_factories::chat_line(c, t, form),
        EE_SCREENS_UMC_CB_LIST_BOX => {
            c.ct_setup_font(m, t);
            c.ct_new_cb_list_box(t, form)
        }
        EE_SCREENS_UMC_WON_ROOMS_BOX => c.ct_new_rooms_box(t, form),
        EE_SCREENS_UMC_WON_GAMES_BOX => c.ct_new_games_box(t, form),
        EE_SCREENS_UMC_WON_STAT_BOX => c.ct_new_stat_box(t, form),
        EE_SCREENS_UMC_SAVED_GROUP => {
            let button = c.ct_new_saved_group(t, form);
            let bitmap = c.ct_normal_bitmap(t, form);
            c.um_button_set_normal(button, bitmap);
            c.um_bitmap_dereference(bitmap);
            let bitmap = c.ct_pressed_bitmap(t, form);
            c.um_button_set_pressed(button, bitmap);
            c.um_bitmap_dereference(bitmap);
            c.um_button_control(button)
        }
        EE_SCREENS_UMC_UNIT_ICON_BAR => crate::screens::shell_factories::icon_bar(c, t, form),
        EE_SCREENS_UMC_PGS_LINE_GRAPH => {
            let graph = c.ct_new_pgs_graph(t, form);
            c.ct_graph_font(graph, t);
            c.ct_graph_font_color(graph, t);
            graph
        }
        EE_SCREENS_UMC_PING => c.ct_new_ping(t, form),
        _ => 0,
    };
    c.um_strings_clear();
    control
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EECreateGameEvent(form, id)`: the event (a `UIEvent*`) or 0.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_game_event(form: usize, id: u32) -> usize {
        super::game_event(&mut Ffi, form, id)
    }
    /// `EECreateFormEvent(manager, id)`.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_form_event(id: u32) -> usize {
        super::form_event(&mut Ffi, id)
    }
    /// `EECreateWonNetServerSelectEvent(form, id)`.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_wns_event(id: u32) -> usize {
        super::wns_event(&mut Ffi, id)
    }
    /// `EEUIManager::CreateScenarioEditor` (`manager` the `EEUIManager*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_um_create_editor(manager: usize) {
        super::create_editor(&mut Ffi, manager)
    }
    /// `EEUIManager::DeleteScenarioEditor`.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_delete_editor(manager: usize) {
        super::delete_editor(&mut Ffi, manager)
    }
    /// `EEUIManager::CreateForm` (`data` the `UIDbDataForms*`): the form (a `UIForm*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_um_create_form(manager: usize, data: usize) -> usize {
        super::create_form(&mut Ffi, manager, data)
    }
    /// `EEUIManager::CreateUserDefinedBackground` (`table` the `UIBackgroundTable*`): a `UIBackground*` or 0.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_create_background(manager: usize, table: usize) -> usize {
        super::create_background(&mut Ffi, manager, table)
    }
    /// `EEUIManager::CreateUserDefinedControl` (`table` the `UIControlsTable*`): a `UIControl*` or 0.
    #[no_mangle]
    pub extern "C" fn ee_screens_um_create_control(manager: usize, table: usize) -> usize {
        super::create_control(&mut Ffi, manager, table)
    }
}

#[cfg(test)]
#[path = "ui_manager_calls_tests.rs"]
mod tests;
