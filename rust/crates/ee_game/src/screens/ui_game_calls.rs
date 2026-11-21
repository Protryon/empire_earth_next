//! `ee_screenscpp_ui_game`'s cases whole in Rust over one-statement shims (UI lane round 11; the C++ host,
//! `User Interface Thread/EEUserInterface.cpp`, is gone): the game starts' world calls (the screen, the paths, the
//! sight, the units, the continents, the resizes, the physics' posts, the music, the scenario's save and load, the
//! players, the computer players, the state, the sounds, the options, the loaded file, the preloads, the record,
//! the server's loss, the media, the lobby, the editor, the civilization builder). `ui_game_ffi.rs`' host answers
//! the other ops before these.

use crate::screens::game_cpp::Cpp;
use crate::screens::ui_game::*;

/// The game host's op `op` on the UI object `ui` (an `EEUserInterface*`); 0 for an op that is none of these.
pub(crate) fn ui_game<C: Cpp + ?Sized>(c: &mut C, ui: usize, op: u32, a: i64, b: i64) -> i64 {
    let file = a as usize;
    match op {
        EE_SCREENS_UG_OP_SCREEN_ACTIVE => c.ug_screen_active(true),
        EE_SCREENS_UG_OP_PATH_RESET => c.ug_path_reset(),
        EE_SCREENS_UG_OP_SIGHT_ON => c.ug_sight_on(),
        EE_SCREENS_UG_OP_RESTART_UNITS => c.ug_restart_units(),
        EE_SCREENS_UG_OP_LAND_CONTINENTS => c.ug_land_continents(),
        EE_SCREENS_UG_OP_RESIZE_GAME => c.ug_resize_game(ui, a != 0, b != 0),
        EE_SCREENS_UG_OP_PHYSICS_POST => c.ug_physics_post(a as u64),
        EE_SCREENS_UG_OP_MUSIC => c.ug_music(),
        EE_SCREENS_UG_OP_PREPARE_SAVE => c.ug_prepare_save(),
        EE_SCREENS_UG_OP_SAVE_SCENARIO => return c.ug_save_scenario(),
        EE_SCREENS_UG_OP_SAVE_ERROR => c.ug_save_error(),
        EE_SCREENS_UG_OP_MAX_PLAYERS => c.ug_max_players(),
        EE_SCREENS_UG_OP_COLLECT_COMPUTERS => c.ug_collect_computers(),
        EE_SCREENS_UG_OP_TIMER_RUNNING => return c.ug_timer_running() as i64,
        EE_SCREENS_UG_OP_STATE_CLEAR => c.ug_state_clear(),
        EE_SCREENS_UG_OP_STOP_SOUNDS => c.ug_stop_sounds(),
        EE_SCREENS_UG_OP_OPTIONS_DEFAULTS => c.ug_options_defaults(),
        // a: the file; its name holds the scenario extension
        EE_SCREENS_UG_OP_FILE_IS_SCENARIO => {
            let length = c.ug_file_name_length(file);
            return (c.ug_file_scenario_at(file) < length) as i64;
        }
        EE_SCREENS_UG_OP_POST_LOAD => c.ug_post_load(a != 0, b != 0),
        EE_SCREENS_UG_OP_DELETE_FILE => c.ug_delete_file(file),
        EE_SCREENS_UG_OP_UPDATE_INTERFACE => c.ug_update_interface(ui),
        EE_SCREENS_UG_OP_FREE_CAMERA => return c.ug_free_camera() as i64,
        EE_SCREENS_UG_OP_SIM_RUN => return c.ug_sim_run() as i64,
        EE_SCREENS_UG_OP_WAIT_PRELOAD => c.ug_wait_preload(),
        EE_SCREENS_UG_OP_WORLD_CLEAR => c.ug_world_clear(),
        EE_SCREENS_UG_OP_SCREEN_INACTIVE => c.ug_screen_active(false),
        EE_SCREENS_UG_OP_RESIZE_MENU => c.ug_resize_menu(ui, a != 0, b != 0),
        EE_SCREENS_UG_OP_ONE_RECORD => return c.ug_one_record() as i64,
        EE_SCREENS_UG_OP_SERVER_LOST => {
            return (c.ug_has_communications() && c.ug_is_client() && c.ug_server_lost_after_loading()) as i64
        }
        EE_SCREENS_UG_OP_UNLOAD_MEDIA => c.ug_unload_media(),
        EE_SCREENS_UG_OP_LOBBY_OPEN => return c.ug_lobby_open() as i64,
        EE_SCREENS_UG_OP_QUIT_FLAG => c.set_quit_flag(),
        EE_SCREENS_UG_OP_CREATE_EDITOR => c.ug_create_editor(),
        EE_SCREENS_UG_OP_LOAD_SCENDEFAULT => return c.ug_load_scendefault(),
        EE_SCREENS_UG_OP_STOP_ONE_SHOTS => c.ug_stop_one_shots(),
        EE_SCREENS_UG_OP_GRAPHICS_PAUSED => return c.ug_graphics_paused() as i64,
        EE_SCREENS_UG_OP_CAN_BUY_CIV => return c.ug_can_buy_civ() as i64,
        EE_SCREENS_UG_OP_CUSTOM_CIVS => return c.ug_custom_civs() as i64,
        EE_SCREENS_UG_OP_CIV_EDIT => {
            c.ug_civ_edit();
        }
        EE_SCREENS_UG_OP_CIV_CREATE => {
            c.ug_civ_create(file, b != 0);
        }
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn a_file_is_a_scenario_when_its_name_holds_the_extension() {
        let mut fake = Fake::default();
        fake.answer("ug_file_name_length", &[10, 10]).answer("ug_file_scenario_at", &[6, -1]);
        assert_eq!(ui_game(&mut fake, 1, EE_SCREENS_UG_OP_FILE_IS_SCENARIO, 5, 0), 1);
        assert_eq!(ui_game(&mut fake, 1, EE_SCREENS_UG_OP_FILE_IS_SCENARIO, 5, 0), 0);
    }

    #[test]
    fn the_server_is_lost_only_for_a_client() {
        let mut fake = Fake::default();
        fake.answer("ug_has_communications", &[1]);
        assert_eq!(ui_game(&mut fake, 1, EE_SCREENS_UG_OP_SERVER_LOST, 0, 0), 0);
        assert_eq!(fake.calls(), "ug_has_communications()\nug_is_client()");
    }

    #[test]
    fn the_screen_and_the_resizes() {
        let mut fake = Fake::default();
        ui_game(&mut fake, 1, EE_SCREENS_UG_OP_SCREEN_INACTIVE, 0, 0);
        ui_game(&mut fake, 1, EE_SCREENS_UG_OP_RESIZE_MENU, 1, 0);
        ui_game(&mut fake, 1, EE_SCREENS_UG_OP_CIV_CREATE, 7, 1);
        assert_eq!(fake.calls(), "ug_screen_active(false)\nug_resize_menu(1, true, false)\nug_civ_create(7, true)");
        assert_eq!(ui_game(&mut fake, 1, 9999, 0, 0), 0);
    }
}
