//! The game screens' host calls that were the original's dispatcher chain under `ee_screenscpp_call`
//! (`UI Events/EEScreens.cpp`: `EEScreensHotKeyCall`, `_ChatCall`, `_MainGameCall`, `_CameraCall`, `_DiplomacyCall`,
//! `_PostGameCall`, `_SplashCall`, `_CivChoiceCall`, `_DebugSoundCall`, `_CampaignEditorCall`, and the root's
//! multi-statement cases) in Rust over one-statement C++ shims (UI lane round 9; the shims are `ee_screenscpp_sc_*`
//! in `EEScreens.cpp`). `game_calls_more.rs` asks [`call`] for the ops it does not have.

use core::ffi::c_ulong;

use crate::screens::camera::{
    EE_SCREENS_GAME_CAM_CHANGED, EE_SCREENS_GAME_CAM_COS, EE_SCREENS_GAME_CAM_FREEZE,
    EE_SCREENS_GAME_CAM_MOUSE_PRESENT, EE_SCREENS_GAME_CAM_MOUSE_VALUE, EE_SCREENS_GAME_CAM_RESET_ZOOM,
    EE_SCREENS_GAME_CAM_REVOLVE, EE_SCREENS_GAME_CAM_ROTATE, EE_SCREENS_GAME_CAM_SCROLL, EE_SCREENS_GAME_CAM_SET_ZOOM,
};
use crate::screens::campaign_editor::{
    EE_SCREENS_CE_IS_EDIT, EE_SCREENS_CE_IS_LABEL, EE_SCREENS_CE_IS_LIST, EE_SCREENS_OP_CE_IS,
};
use crate::screens::chat::{EE_SCREENS_GAME_CL_UNPAUSE, EE_SCREENS_OP_CH_MOUSE_INSIDE};
use crate::screens::civ_choice::{
    EE_SCREENS_GAME_PDCS_FILL, EE_SCREENS_GAME_PDCS_PAUSE, EE_SCREENS_OP_PDCS_CLEAR_STATS, EE_SCREENS_OP_PDCS_CONTROLS,
    EE_SCREENS_OP_PDCS_HIDE_LABEL, EE_SCREENS_OP_PDCS_RESET, EE_SCREENS_OP_PDCS_SELECTION,
    EE_SCREENS_OP_PDCS_SET_UPGRADES,
};
use crate::screens::debug_sound::{EE_SCREENS_GAME_DSU_SOUND, EE_SCREENS_GAME_DSU_VOLUME};
use crate::screens::diplomacy::{EE_SCREENS_OP_DS_BUTTON_NUMBER, EE_SCREENS_OP_DS_LABEL_NUMBER};
use crate::screens::game_cpp::{Cpp, EeScreensShimULongOut};
use crate::screens::hot_keys::{
    EE_SCREENS_GAME_HK_RESET_SAVED, EE_SCREENS_GAME_HK_SHIFT, EE_SCREENS_GAME_HK_UPDATE_SELECTION,
    EE_SCREENS_OP_HK_GROUP_BUTTON,
};
use crate::screens::lobby_form::{
    EE_SCREENS_GAME_LE_DISCONNECT, EE_SCREENS_GAME_LE_PLAYER_INFO, EE_SCREENS_GAME_LE_RULE,
    EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS, EE_SCREENS_GAME_LE_SEND_OPTION, EE_SCREENS_GAME_LE_SET_OPTION,
    EE_SCREENS_GAME_LE_START_LOADING, EE_SCREENS_GAME_LE_START_PING, EE_SCREENS_OP_LE_BITMAP, EE_SCREENS_OP_LE_PING,
};
use crate::screens::main_game::{
    EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS, EE_SCREENS_GAME_MG_ENTITY_GROUP, EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED,
    EE_SCREENS_GAME_MG_POINTER, EE_SCREENS_GAME_MG_SET_CAMERA_MODE, EE_SCREENS_GAME_MG_SET_HIDDEN_MODE,
    EE_SCREENS_GAME_MG_SET_UI_STATE, EE_SCREENS_GAME_MG_SHOW_DIALOG, EE_SCREENS_GAME_MG_ZOOM_STYLE,
};
use crate::screens::main_game_options::{
    EE_SCREENS_GAME_GO_BRIEFING, EE_SCREENS_GAME_GO_EXIT, EE_SCREENS_GAME_GO_REQUEST_PAUSE,
};
use crate::screens::menus::{
    EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN, EE_SCREENS_GAME_CREATE_EDITOR, EE_SCREENS_GAME_DELETE_EDITOR,
    EE_SCREENS_GAME_DELETE_WON, EE_SCREENS_GAME_EXTERNAL_LOBBY, EE_SCREENS_GAME_FORGET_MAIN_FORM,
    EE_SCREENS_GAME_LOBBY_END, EE_SCREENS_GAME_MULTI, EE_SCREENS_GAME_SHOW_SINGLE_PLAYER, EE_SCREENS_GAME_SINGLE,
    EE_SCREENS_GAME_START_EDITOR,
};
use crate::screens::post_game::{
    EE_SCREENS_GAME_PGS_ADD_SERIES, EE_SCREENS_GAME_PGS_NEW_MANAGER, EE_SCREENS_GAME_PGS_PROTOCOL,
    EE_SCREENS_GAME_PGS_SAVE, EE_SCREENS_GAME_PGS_SCI_DELETE, EE_SCREENS_GAME_PGS_SET_PAGE, EE_SCREENS_GAME_PGS_WINDOW,
    EE_SCREENS_OP_PGS_APPLY_R, EE_SCREENS_OP_PGS_IS_BITMAP, EE_SCREENS_OP_PGS_SET_R,
};
use crate::screens::post_game_calls;
use crate::screens::save_load::{
    EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM, EE_SCREENS_GAME_SAVE_POST, EE_SCREENS_GAME_SAVE_SEND,
};
use crate::screens::scenario_screens::{
    EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM, EE_SCREENS_GAME_SI_BLINK_OFF, EE_SCREENS_GAME_SI_GET_SCENARIO,
    EE_SCREENS_GAME_SI_OPEN_MAP, EE_SCREENS_GAME_SI_OPEN_SOUND, EE_SCREENS_GAME_SI_POST_LOAD,
};
use crate::screens::settings_form::{
    EE_SCREENS_GAME_AMBIENT, EE_SCREENS_GAME_APPLY_PORT, EE_SCREENS_GAME_LEFT_HANDED, EE_SCREENS_GAME_MIP_FILTER,
    EE_SCREENS_GAME_MOUSE_SENSITIVITY, EE_SCREENS_GAME_MUSIC_START, EE_SCREENS_GAME_MUSIC_STOP, EE_SCREENS_GAME_PAUSE,
    EE_SCREENS_GAME_SAVE, EE_SCREENS_GAME_SCREEN_COUNT, EE_SCREENS_GAME_TEXTURE_RESOLUTION,
    EE_SCREENS_GAME_UI_MUSIC_VOLUME, EE_SCREENS_OP_RAND, EE_SCREENS_OP_RESIZE_MENU, EE_SCREENS_OP_SRAND_TIME,
};

/// The case of `op` (its C++ arguments a..d), `None` for an op that is not here.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, form: usize, op: u32, a: u32, b: u32, cc: u32, _d: u32) -> Option<u32> {
    let float = f32::from_bits;
    Some(match op {
        // the hot keys' (hot_keys.rs): the current player's
        EE_SCREENS_GAME_HK_UPDATE_SELECTION => {
            c.sc_hk_update_selection();
            0
        }
        EE_SCREENS_GAME_HK_RESET_SAVED => {
            c.sc_hk_reset_saved();
            0
        }
        EE_SCREENS_OP_HK_GROUP_BUTTON => {
            c.sc_hk_group_button(form, a);
            0
        }
        EE_SCREENS_GAME_HK_SHIFT => c.key_shift_value() as u32,
        // the chat forms' (chat.rs)
        EE_SCREENS_OP_CH_MOUSE_INSIDE => c.sc_ch_mouse_inside(form) as u32,
        EE_SCREENS_GAME_CL_UNPAUSE => {
            c.sc_cl_unpause();
            0
        }
        // the main game form's (main_game.rs)
        EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS => {
            c.sc_mg_clear_gate_ghosts(form);
            0
        }
        EE_SCREENS_GAME_MG_SET_UI_STATE => {
            c.sc_mg_set_ui_state(form, a as i32);
            0
        }
        EE_SCREENS_GAME_MG_POINTER => {
            c.sc_mg_pointer(a as i32);
            0
        }
        EE_SCREENS_GAME_MG_SHOW_DIALOG => c.sc_mg_show_dialog(),
        EE_SCREENS_GAME_MG_ENTITY_GROUP => {
            c.sc_mg_entity_group(form, a);
            0
        }
        EE_SCREENS_GAME_MG_SET_HIDDEN_MODE => {
            c.sc_mg_set_hidden_mode(a);
            0
        }
        EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED => {
            // every unit of the world's list: a complex one that can hide marked moved
            let mut unit = c.sc_units_first();
            while unit != 0 {
                let complex = c.sc_unit_complex(unit);
                if complex != 0 && c.sc_complex_can_be_hidden(complex) {
                    c.sc_complex_mark_moved(complex);
                }
                unit = c.sc_unit_next(unit);
            }
            0
        }
        EE_SCREENS_GAME_MG_SET_CAMERA_MODE => {
            c.sc_mg_set_camera_mode(a);
            0
        }
        EE_SCREENS_GAME_MG_ZOOM_STYLE => {
            c.sc_mg_zoom_style();
            0
        }
        // the camera's (camera.rs): the mouse's axes, the world's camera
        EE_SCREENS_GAME_CAM_FREEZE => {
            c.sc_cam_freeze(a != 0);
            0
        }
        EE_SCREENS_GAME_CAM_MOUSE_PRESENT => {
            c.sc_mouse_entity_clear();
            c.sc_mouse_entity_load(a);
            c.sc_mouse_entity_present() as u32
        }
        EE_SCREENS_GAME_CAM_MOUSE_VALUE => {
            c.sc_mouse_entity_clear();
            c.sc_mouse_entity_load(a);
            (if b != 0 { c.sc_mouse_entity_normalized() } else { c.sc_mouse_entity_value() }) as u32
        }
        EE_SCREENS_GAME_CAM_REVOLVE => {
            c.sc_cam_revolve(float(a), float(b));
            0
        }
        EE_SCREENS_GAME_CAM_ROTATE => {
            c.sc_cam_rotate(float(a), float(b));
            0
        }
        EE_SCREENS_GAME_CAM_SCROLL => {
            c.sc_cam_scroll(float(a), float(b));
            0
        }
        EE_SCREENS_GAME_CAM_RESET_ZOOM => {
            c.sc_cam_reset_zoom();
            0
        }
        EE_SCREENS_GAME_CAM_CHANGED => {
            c.sc_cam_changed();
            0
        }
        EE_SCREENS_GAME_CAM_SET_ZOOM => {
            c.sc_cam_set_zoom(float(a));
            0
        }
        EE_SCREENS_GAME_CAM_COS => c.sc_cam_cos(float(a)).to_bits(),
        // the diplomacy form's (diplomacy.rs)
        EE_SCREENS_OP_DS_BUTTON_NUMBER => c.sc_ds_button_number(form, a),
        EE_SCREENS_OP_DS_LABEL_NUMBER => c.sc_ds_label_number(form, a),
        // the post-game summary's (post_game.rs)
        EE_SCREENS_GAME_PGS_SET_PAGE => {
            c.sc_pgs_set_page(a as i32);
            0
        }
        EE_SCREENS_OP_PGS_SET_R => {
            c.sc_pgs_set_r(a, float(b));
            0
        }
        EE_SCREENS_OP_PGS_APPLY_R => {
            c.sc_pgs_apply_r(form, a);
            0
        }
        EE_SCREENS_GAME_PGS_ADD_SERIES => {
            post_game_calls::add_series(c, form, a, b);
            0
        }
        EE_SCREENS_GAME_PGS_NEW_MANAGER => {
            c.sc_pgs_new_manager(form);
            0
        }
        EE_SCREENS_GAME_PGS_WINDOW => post_game_calls::window(c, form, a),
        EE_SCREENS_OP_PGS_IS_BITMAP => c.sc_pgs_is_bitmap(form, a) as u32,
        EE_SCREENS_GAME_PGS_PROTOCOL => {
            c.sc_pgs_protocol();
            0
        }
        EE_SCREENS_GAME_PGS_SCI_DELETE => {
            c.sc_pgs_sci_delete();
            0
        }
        EE_SCREENS_GAME_PGS_SAVE => {
            c.sc_preferences_save();
            0
        }
        // the civilization choice form's (civ_choice.rs): its controls 3 (the list), 4 (the statistics), 5 (the label)
        EE_SCREENS_GAME_PDCS_FILL => {
            c.sc_pdcs_fill(form, a);
            0
        }
        EE_SCREENS_GAME_PDCS_PAUSE => {
            c.sc_pause_game();
            0
        }
        EE_SCREENS_OP_PDCS_CLEAR_STATS => {
            c.sc_pdcs_clear_stats();
            0
        }
        EE_SCREENS_OP_PDCS_SELECTION => {
            let mut selection: c_ulong = 0;
            if c.pdcs_selection(EeScreensShimULongOut::of(&mut selection)) != 0 {
                0
            } else {
                (selection as u32).wrapping_add(1)
            }
        }
        EE_SCREENS_OP_PDCS_CONTROLS => {
            (c.sc_pdcs_control(3) != 0 && c.sc_pdcs_control(4) != 0 && c.sc_pdcs_control(5) != 0) as u32
        }
        EE_SCREENS_OP_PDCS_SET_UPGRADES => {
            c.sc_pdcs_set_upgrades();
            0
        }
        EE_SCREENS_OP_PDCS_HIDE_LABEL => {
            c.sc_pdcs_hide_label();
            0
        }
        EE_SCREENS_OP_PDCS_RESET => {
            c.sc_pdcs_reset();
            0
        }
        // the debug sound panel's (debug_sound.rs): the first selected unit's sound of kind a (1..10)
        EE_SCREENS_GAME_DSU_SOUND => {
            c.sc_dsu_unit_clear();
            c.sc_dsu_clear();
            if !c.sc_dsu_first_selected() || !c.sc_dsu_has_complex() {
                return Some(0);
            }
            match a {
                1 => c.sc_dsu_sound1(),
                2 => c.sc_dsu_sound2(),
                3 => c.sc_dsu_sound3(),
                4 => c.sc_dsu_sound4(),
                5 => c.sc_dsu_sound5(),
                6 => c.sc_dsu_sound6(),
                7 => c.sc_dsu_sound7(),
                8 => c.sc_dsu_sound8(),
                9 => c.sc_dsu_sound9(),
                10 => c.sc_dsu_sound10(),
                _ => {}
            }
            if c.sc_dsu_has_sound() {
                2
            } else {
                1
            }
        }
        EE_SCREENS_GAME_DSU_VOLUME => {
            c.sc_dsu_volume(float(a));
            0
        }
        // the campaign editor's (campaign_editor.rs): control a's kind
        EE_SCREENS_OP_CE_IS => {
            (match b {
                EE_SCREENS_CE_IS_EDIT => c.sc_ce_is_edit(form, a),
                EE_SCREENS_CE_IS_LABEL => c.sc_ce_is_label(form, a),
                EE_SCREENS_CE_IS_LIST => c.sc_ce_is_list(form, a),
                _ => c.sc_ce_is_combo(form, a),
            }) as u32
        }
        // the root's (menus.rs, lobby_form.rs)
        EE_SCREENS_GAME_SINGLE | EE_SCREENS_GAME_MULTI => {
            c.sc_game_state_multiplayer(op == EE_SCREENS_GAME_MULTI);
            0
        }
        EE_SCREENS_GAME_DELETE_WON => {
            // gWonNet cleared before its object is deleted
            let won = c.won_net();
            if won != 0 {
                c.sc_won_net_clear();
                c.sc_won_net_delete(won);
            }
            0
        }
        EE_SCREENS_GAME_LE_PLAYER_INFO => {
            c.sc_send_player_information(a, b, cc as i32);
            0
        }
        EE_SCREENS_GAME_LE_SEND_OPTION => {
            c.sc_send_game_option(a as i32, b as i32);
            0
        }
        EE_SCREENS_GAME_LE_START_LOADING => {
            c.sc_send_start_loading(a as u8, b as i32);
            0
        }
        _ => return root(c, form, op, a, b),
    })
}

/// The original `ee_screenscpp_call`'s own cases (the toolkit's and the game's calls on the event's form; a posted
/// event runs with no form: the game's manager), `None` for an op that is not here.
fn root<C: Cpp + ?Sized>(c: &mut C, form: usize, op: u32, a: u32, b: u32) -> Option<u32> {
    let float = f32::from_bits;
    Some(match op {
        EE_SCREENS_OP_RESIZE_MENU => {
            c.rc_resize_menu();
            0
        }
        EE_SCREENS_OP_SRAND_TIME => {
            c.rc_srand_time();
            0
        }
        EE_SCREENS_OP_RAND => c.rc_rand(),
        EE_SCREENS_GAME_UI_MUSIC_VOLUME => {
            c.rc_ui_music_volume(float(a));
            0
        }
        EE_SCREENS_GAME_MUSIC_START => {
            c.rc_music_start();
            0
        }
        EE_SCREENS_GAME_MUSIC_STOP => {
            c.rc_music_stop();
            0
        }
        EE_SCREENS_GAME_MOUSE_SENSITIVITY => {
            c.rc_mouse_sensitivity(float(a));
            0
        }
        EE_SCREENS_GAME_LEFT_HANDED => {
            c.rc_left_handed(a != 0);
            0
        }
        EE_SCREENS_GAME_PAUSE => {
            c.sc_pause_game();
            0
        }
        EE_SCREENS_GAME_APPLY_PORT => {
            c.rc_apply_port();
            0
        }
        EE_SCREENS_GAME_MIP_FILTER => {
            c.rc_mip_filter(a);
            0
        }
        EE_SCREENS_GAME_TEXTURE_RESOLUTION => {
            c.rc_texture_resolution(a as i32);
            0
        }
        EE_SCREENS_GAME_AMBIENT => {
            c.rc_ambient(a != 0);
            0
        }
        EE_SCREENS_GAME_SAVE => c.sc_preferences_save() as u32,
        // the custom campaign form's (custom_campaign.rs): a display device's screens (0 past them)
        EE_SCREENS_GAME_SCREEN_COUNT => {
            if a < c.rc_display_rasterizers() {
                c.rc_display_screens(a)
            } else {
                0
            }
        }
        EE_SCREENS_GAME_EXTERNAL_LOBBY => {
            c.rc_external_lobby();
            0
        }
        EE_SCREENS_GAME_DELETE_EDITOR | EE_SCREENS_GAME_CREATE_EDITOR | EE_SCREENS_GAME_START_EDITOR => {
            let manager = if form != 0 { c.form_manager(form) } else { c.tp_ui_manager() };
            match op {
                EE_SCREENS_GAME_DELETE_EDITOR => c.rc_delete_editor(manager),
                EE_SCREENS_GAME_CREATE_EDITOR => c.rc_create_editor(manager),
                _ => c.rc_start_editor(manager),
            }
            0
        }
        EE_SCREENS_GAME_FORGET_MAIN_FORM => {
            c.rc_forget_main_form();
            0
        }
        EE_SCREENS_GAME_LOBBY_END => {
            c.lobby_options_write_unlock();
            0
        }
        EE_SCREENS_GAME_SHOW_SINGLE_PLAYER => {
            c.rc_show_single_player();
            0
        }
        EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN => {
            c.rc_campaign_all_open(a != 0);
            0
        }
        // the load and save screens' (save_load.rs)
        EE_SCREENS_GAME_SAVE_SEND => {
            c.rc_save_send();
            0
        }
        EE_SCREENS_GAME_SAVE_POST => {
            c.rc_save_post();
            0
        }
        EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM => {
            c.rc_open_saved_main_form();
            0
        }
        // the campaign scenario select and scenario intro forms' (scenario_screens.rs)
        EE_SCREENS_GAME_SI_BLINK_OFF => {
            c.rc_si_blink_off();
            0
        }
        EE_SCREENS_GAME_SI_OPEN_MAP => c.rc_si_open_map(),
        EE_SCREENS_GAME_SI_OPEN_SOUND => c.rc_si_open_sound(),
        EE_SCREENS_GAME_SI_GET_SCENARIO => c.rc_si_get_scenario(),
        EE_SCREENS_GAME_SI_POST_LOAD => {
            c.rc_si_post_load();
            0
        }
        EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM => {
            c.rc_campaign_update_checksum();
            0
        }
        // the lobby's and its load screens' (lobby_form.rs)
        EE_SCREENS_GAME_LE_SET_OPTION => {
            c.lobby_set_option(a, b as i32 as i64);
            0
        }
        EE_SCREENS_OP_LE_BITMAP => {
            if b != 0 {
                c.rc_le_bitmap_on(form, a)
            } else {
                c.rc_le_bitmap_off(form, a)
            }
            0
        }
        EE_SCREENS_OP_LE_PING => {
            c.rc_le_ping(form, a, float(b));
            0
        }
        EE_SCREENS_GAME_LE_DISCONNECT => {
            c.rc_le_disconnect();
            0
        }
        EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS => {
            c.rc_le_save_last_options();
            0
        }
        EE_SCREENS_GAME_LE_START_PING => {
            c.rc_le_start_ping();
            0
        }
        EE_SCREENS_GAME_LE_RULE => c.rc_le_rule(a, b),
        // the in-game options menu's (main_game_options.rs)
        EE_SCREENS_GAME_GO_REQUEST_PAUSE => {
            c.rc_go_request_pause();
            0
        }
        EE_SCREENS_GAME_GO_BRIEFING => {
            c.rc_go_briefing();
            0
        }
        EE_SCREENS_GAME_GO_EXIT => {
            c.rc_go_exit();
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn run(f: &mut Fake, op: u32, a: u32, b: u32, cc: u32) -> Option<u32> {
        call(f, 0x10, op, a, b, cc, 0)
    }

    #[test]
    fn the_hidden_units_are_marked_along_the_world_list() {
        let mut f = Fake::default();
        f.answer("sc_units_first", &[1]).answer("sc_unit_next", &[2, 3, 0]);
        f.answer("sc_unit_complex", &[0x11, 0, 0x13]).answer("sc_complex_can_be_hidden", &[1, 0]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED, 0, 0, 0), Some(0));
        assert_eq!(
            f.calls(),
            "sc_units_first()\nsc_unit_complex(1)\nsc_complex_can_be_hidden(17)\nsc_complex_mark_moved(17)\n\
             sc_unit_next(1)\nsc_unit_complex(2)\nsc_unit_next(2)\nsc_unit_complex(3)\nsc_complex_can_be_hidden(19)\n\
             sc_unit_next(3)"
        );
    }

    #[test]
    fn the_mouse_axes_and_the_camera_take_their_floats_as_bits() {
        let mut f = Fake::default();
        f.answer("sc_mouse_entity_present", &[1]).answer("sc_mouse_entity_normalized", &[-1]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_CAM_MOUSE_PRESENT, 2, 0, 0), Some(1));
        assert_eq!(run(&mut f, EE_SCREENS_GAME_CAM_MOUSE_VALUE, 3, 1, 0), Some(u32::MAX));
        run(&mut f, EE_SCREENS_GAME_CAM_SCROLL, 1.5f32.to_bits(), (-2.0f32).to_bits(), 0);
        let calls = f.calls();
        assert!(calls.starts_with("sc_mouse_entity_clear()\nsc_mouse_entity_load(2)\nsc_mouse_entity_present()"));
        assert!(calls.contains("sc_mouse_entity_load(3)\nsc_mouse_entity_normalized()"));
        assert!(calls.ends_with("sc_cam_scroll(1.5, -2.0)"));
        let mut f = Fake::default();
        f.answer("sc_cam_cos", &[0]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_CAM_COS, 0, 0, 0), Some(0));
    }

    #[test]
    fn the_debug_sound_is_the_first_selected_units_of_its_kind() {
        let mut f = Fake::default();
        assert_eq!(run(&mut f, EE_SCREENS_GAME_DSU_SOUND, 4, 0, 0), Some(0));
        assert_eq!(f.calls(), "sc_dsu_unit_clear()\nsc_dsu_clear()\nsc_dsu_first_selected()");
        let mut f = Fake::default();
        f.answer("sc_dsu_first_selected", &[1]).answer("sc_dsu_has_complex", &[1]).answer("sc_dsu_has_sound", &[1]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_DSU_SOUND, 9, 0, 0), Some(2));
        assert!(f.calls().ends_with("sc_dsu_sound9()\nsc_dsu_has_sound()"));
        let mut f = Fake::default();
        f.answer("sc_dsu_first_selected", &[1]).answer("sc_dsu_has_complex", &[1]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_DSU_SOUND, 11, 0, 0), Some(1));
    }

    #[test]
    fn the_civilization_choice_needs_its_three_controls() {
        let mut f = Fake::default();
        f.answer("sc_pdcs_control", &[1, 0]);
        assert_eq!(run(&mut f, EE_SCREENS_OP_PDCS_CONTROLS, 0, 0, 0), Some(0));
        assert_eq!(f.calls(), "sc_pdcs_control(3)\nsc_pdcs_control(4)");
        let mut f = Fake::default();
        f.out("pdcs_selection", 4);
        assert_eq!(run(&mut f, EE_SCREENS_OP_PDCS_SELECTION, 0, 0, 0), Some(5));
        f.answer("pdcs_selection", &[1]);
        assert_eq!(run(&mut f, EE_SCREENS_OP_PDCS_SELECTION, 0, 0, 0), Some(0));
    }

    #[test]
    fn the_won_object_is_cleared_before_its_delete_and_the_sends_keep_their_signs() {
        let mut f = Fake::default();
        run(&mut f, EE_SCREENS_GAME_DELETE_WON, 0, 0, 0);
        assert_eq!(f.calls(), "won_net()");
        let mut f = Fake::default();
        f.answer("won_net", &[0x55]);
        run(&mut f, EE_SCREENS_GAME_DELETE_WON, 0, 0, 0);
        run(&mut f, EE_SCREENS_GAME_LE_PLAYER_INFO, 1, 2, u32::MAX);
        run(&mut f, EE_SCREENS_GAME_LE_START_LOADING, 0x101, 7, 0);
        run(&mut f, EE_SCREENS_GAME_MULTI, 0, 0, 0);
        assert_eq!(
            f.calls(),
            "won_net()\nsc_won_net_clear()\nsc_won_net_delete(85)\nsc_send_player_information(1, 2, -1)\n\
             sc_send_start_loading(1, 7)\nsc_game_state_multiplayer(true)"
        );
    }

    #[test]
    fn the_campaign_editors_kinds_and_the_rest_are_one_call_each() {
        let mut f = Fake::default();
        f.answer("sc_ce_is_combo", &[1]);
        assert_eq!(run(&mut f, EE_SCREENS_OP_CE_IS, 4, 99, 0), Some(1));
        assert_eq!(run(&mut f, EE_SCREENS_OP_CE_IS, 4, EE_SCREENS_CE_IS_LIST, 0), Some(0));
        for op in [
            EE_SCREENS_GAME_HK_UPDATE_SELECTION,
            EE_SCREENS_GAME_HK_RESET_SAVED,
            EE_SCREENS_OP_HK_GROUP_BUTTON,
            EE_SCREENS_GAME_HK_SHIFT,
            EE_SCREENS_OP_CH_MOUSE_INSIDE,
            EE_SCREENS_GAME_CL_UNPAUSE,
            EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS,
            EE_SCREENS_GAME_MG_SET_UI_STATE,
            EE_SCREENS_GAME_MG_POINTER,
            EE_SCREENS_GAME_MG_SHOW_DIALOG,
            EE_SCREENS_GAME_MG_ENTITY_GROUP,
            EE_SCREENS_GAME_MG_SET_HIDDEN_MODE,
            EE_SCREENS_GAME_MG_SET_CAMERA_MODE,
            EE_SCREENS_GAME_MG_ZOOM_STYLE,
            EE_SCREENS_GAME_CAM_FREEZE,
            EE_SCREENS_GAME_CAM_REVOLVE,
            EE_SCREENS_GAME_CAM_ROTATE,
            EE_SCREENS_GAME_CAM_RESET_ZOOM,
            EE_SCREENS_GAME_CAM_CHANGED,
            EE_SCREENS_GAME_CAM_SET_ZOOM,
            EE_SCREENS_OP_DS_BUTTON_NUMBER,
            EE_SCREENS_OP_DS_LABEL_NUMBER,
            EE_SCREENS_GAME_PGS_SET_PAGE,
            EE_SCREENS_OP_PGS_SET_R,
            EE_SCREENS_OP_PGS_APPLY_R,
            EE_SCREENS_GAME_PGS_NEW_MANAGER,
            EE_SCREENS_GAME_PGS_WINDOW,
            EE_SCREENS_OP_PGS_IS_BITMAP,
            EE_SCREENS_GAME_PGS_PROTOCOL,
            EE_SCREENS_GAME_PGS_SCI_DELETE,
            EE_SCREENS_GAME_PGS_SAVE,
            EE_SCREENS_GAME_PGS_ADD_SERIES,
            EE_SCREENS_GAME_PDCS_FILL,
            EE_SCREENS_GAME_PDCS_PAUSE,
            EE_SCREENS_OP_PDCS_CLEAR_STATS,
            EE_SCREENS_OP_PDCS_SET_UPGRADES,
            EE_SCREENS_OP_PDCS_HIDE_LABEL,
            EE_SCREENS_OP_PDCS_RESET,
            EE_SCREENS_GAME_DSU_VOLUME,
            EE_SCREENS_GAME_SINGLE,
            EE_SCREENS_GAME_LE_SEND_OPTION,
        ] {
            let mut f = Fake::default();
            assert!(run(&mut f, op, 1, 2, 3).is_some(), "{op}");
            assert!(!f.log.is_empty(), "{op}");
        }
        assert_eq!(run(&mut Fake::default(), 0xffff_fff0, 0, 0, 0), None);
    }

    #[test]
    fn the_roots_cases_are_one_call_each_and_the_editors_take_the_forms_manager() {
        let mut f = Fake::default();
        f.answer("form_manager", &[0x30]).answer("tp_ui_manager", &[0x40]);
        call(&mut f, 0x10, EE_SCREENS_GAME_CREATE_EDITOR, 0, 0, 0, 0);
        call(&mut f, 0, EE_SCREENS_GAME_START_EDITOR, 0, 0, 0, 0);
        assert_eq!(f.calls(), "form_manager(16)\nrc_create_editor(48)\ntp_ui_manager()\nrc_start_editor(64)");
        let mut f = Fake::default();
        f.answer("rc_display_rasterizers", &[2, 2]).answer("rc_display_screens", &[3]);
        assert_eq!(run(&mut f, EE_SCREENS_GAME_SCREEN_COUNT, 1, 0, 0), Some(3));
        assert_eq!(run(&mut f, EE_SCREENS_GAME_SCREEN_COUNT, 2, 0, 0), Some(0));
        let mut f = Fake::default();
        run(&mut f, EE_SCREENS_OP_LE_BITMAP, 4, 1, 0);
        run(&mut f, EE_SCREENS_OP_LE_BITMAP, 4, 0, 0);
        run(&mut f, EE_SCREENS_GAME_LE_SET_OPTION, 3, u32::MAX, 0);
        assert_eq!(f.calls(), "rc_le_bitmap_on(16, 4)\nrc_le_bitmap_off(16, 4)\nlobby_set_option(3, -1)");
        for op in [
            EE_SCREENS_OP_RESIZE_MENU,
            EE_SCREENS_OP_SRAND_TIME,
            EE_SCREENS_OP_RAND,
            EE_SCREENS_GAME_UI_MUSIC_VOLUME,
            EE_SCREENS_GAME_MUSIC_START,
            EE_SCREENS_GAME_MUSIC_STOP,
            EE_SCREENS_GAME_MOUSE_SENSITIVITY,
            EE_SCREENS_GAME_LEFT_HANDED,
            EE_SCREENS_GAME_PAUSE,
            EE_SCREENS_GAME_APPLY_PORT,
            EE_SCREENS_GAME_MIP_FILTER,
            EE_SCREENS_GAME_TEXTURE_RESOLUTION,
            EE_SCREENS_GAME_AMBIENT,
            EE_SCREENS_GAME_SAVE,
            EE_SCREENS_GAME_EXTERNAL_LOBBY,
            EE_SCREENS_GAME_FORGET_MAIN_FORM,
            EE_SCREENS_GAME_LOBBY_END,
            EE_SCREENS_GAME_SHOW_SINGLE_PLAYER,
            EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN,
            EE_SCREENS_GAME_SAVE_SEND,
            EE_SCREENS_GAME_SAVE_POST,
            EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM,
            EE_SCREENS_GAME_SI_BLINK_OFF,
            EE_SCREENS_GAME_SI_OPEN_MAP,
            EE_SCREENS_GAME_SI_OPEN_SOUND,
            EE_SCREENS_GAME_SI_GET_SCENARIO,
            EE_SCREENS_GAME_SI_POST_LOAD,
            EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM,
            EE_SCREENS_OP_LE_PING,
            EE_SCREENS_GAME_LE_DISCONNECT,
            EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS,
            EE_SCREENS_GAME_LE_START_PING,
            EE_SCREENS_GAME_LE_RULE,
            EE_SCREENS_GAME_GO_REQUEST_PAUSE,
            EE_SCREENS_GAME_GO_BRIEFING,
            EE_SCREENS_GAME_GO_EXIT,
            EE_SCREENS_GAME_DELETE_EDITOR,
        ] {
            let mut f = Fake::default();
            assert!(run(&mut f, op, 1, 2, 3).is_some(), "{op}");
            assert!(!f.log.is_empty(), "{op}");
        }
    }
}
