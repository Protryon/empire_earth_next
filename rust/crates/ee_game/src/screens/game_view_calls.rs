//! The screens' read views' C++ fills in Rust over one-statement C++ shims (UI lane round 8; the shims are
//! `ee_screenscpp_gv_*` and `_gt_*` in `UI Events/EEScreens.cpp`): the original's `ee_screenscpp_game_view` (a
//! group's values into an [`EeScreensGameView`]) and `ee_screenscpp_game_texts` (a text group's texts, each ended
//! by a 0 byte).

use crate::screens::game_cpp::{Cpp, EeScreensShimOut};
use crate::screens::game_view::*;
use crate::screens::scenario_screens::{
    EE_SCREENS_SI_FLAG_CAMPAIGN, EE_SCREENS_SI_FLAG_IN_GAME, EE_SCREENS_SI_FLAG_LOADING, EE_SCREENS_SI_FLAG_SCROLL,
};

/// A group's values (`EE_SCREENS_VIEW_*`; the world's and a player's members are the world views', filled after).
pub(crate) fn view(c: &mut impl Cpp, group: u32, arg: u32) -> EeScreensGameView {
    let mut v = EeScreensGameView::default();
    match group {
        EE_SCREENS_VIEW_SOUND => {
            v.sound_manager = c.gv_sound_manager();
            v.conductor = if c.gv_has_conductor() { c.gv_conductor() } else { 0 };
        }
        EE_SCREENS_VIEW_SETTINGS => {
            for i in 0..c.gv_rasterizer_count() {
                if c.gv_rasterizer_is_current(i) {
                    v.current_rasterizer = i;
                    break;
                }
            }
            v.rasterizer_count = c.gv_display_rasterizers();
            v.not_korean = c.gv_not_korean();
            v.pgs_page = c.gv_pgs_page();
            v.pgs_screenshots = c.gv_pgs_screenshots();
            v.grab_scroll_speed = c.gv_grab_scroll_speed();
            v.settings_zoom_enabled = c.gv_settings_zoom();
            v.zoom_enabled = c.gv_zoom_enabled();
            v.difficulty = c.gv_difficulty();
            v.free_camera = c.gv_free_camera();
            v.screen_active = c.gv_screen_active();
            v.saved_main_form = c.gv_saved_main_form();
            v.max_zoom = c.gv_max_zoom();
            v.hidden_mode = c.gv_hidden_mode();
            v.header_players = if c.gv_has_header() { c.gv_header_players() } else { 0 };
        }
        EE_SCREENS_VIEW_SESSION => {
            if c.gv_comms() {
                v.is_server = c.gv_is_server();
                v.is_client = c.gv_is_client();
                v.multiplayer_session = c.gv_multiplayer_session();
                v.cts_processed = c.gv_cts_processed();
                v.cannot_exit = if v.is_server { c.gv_server_migrating() } else { c.gv_client_playback() };
                v.humans = c.gv_humans();
            }
        }
        EE_SCREENS_VIEW_WORLD => {
            v.camera_mode = c.gv_camera_mode();
            v.lock_speed = c.gv_lock_speed();
            v.zoom = c.gv_zoom();
            v.scenario_game = c.gv_scenario_game();
            v.speed_step = c.gv_speed_step();
            v.speed_low = c.gv_speed_low();
            v.speed_high = c.gv_speed_high();
            v.hot_key_mode = if c.gv_main_form() { c.gv_hot_key_mode() } else { 0 };
        }
        EE_SCREENS_VIEW_COLOUR => v.colour = [c.gv_red(arg), c.gv_green(arg), c.gv_blue(arg)],
        EE_SCREENS_VIEW_CAMPAIGN => {
            v.campaign_checksum = c.gv_campaign_checksum();
            v.all_scenarios_open = c.gv_all_scenarios_open();
        }
        EE_SCREENS_VIEW_INTRO => {
            let flag = |on: bool, f: u32| if on { f } else { 0 };
            v.si_flags = flag(c.gv_intro_in_game(), EE_SCREENS_SI_FLAG_IN_GAME)
                | flag(c.gv_intro_loading(), EE_SCREENS_SI_FLAG_LOADING)
                | flag(c.gv_intro_campaign(), EE_SCREENS_SI_FLAG_CAMPAIGN)
                | flag(c.gv_intro_scroll(), EE_SCREENS_SI_FLAG_SCROLL);
        }
        _ => {}
    }
    v
}

/// A text group's texts (`EE_SCREENS_VIEW_TEXTS_*`), each ended by a 0 byte.
pub(crate) fn texts(c: &mut impl Cpp, group: u32, arg: u32) -> Vec<u8> {
    let mut out = Vec::new();
    {
        let mut add = |f: &mut dyn FnMut(EeScreensShimOut)| {
            f(EeScreensShimOut::of(&mut out));
            out.push(0);
        };
        match group {
            EE_SCREENS_VIEW_TEXTS_SETTINGS => {
                // (game_view.rs T_*: in this order)
                add(&mut |o| c.gt_rasterizer_name(o));
                add(&mut |o| c.gt_default_rasterizer_name(o));
                add(&mut |o| c.gt_audio_device(o));
                add(&mut |o| c.gt_default_audio_device(o));
                add(&mut |o| c.gt_game_version(o));
                add(&mut |o| c.gt_player_name(o));
                add(&mut |o| c.gt_campaign_roman(o));
                add(&mut |o| c.gt_campaign_pacific(o));
                add(&mut |o| c.gt_campaign_asian(o));
                add(&mut |o| c.gt_campaign_learning(o));
                add(&mut |o| c.gt_saved_game_extension(o));
                add(&mut |o| c.gt_scenario_extension(o));
                add(&mut |o| c.gt_campaign_extension(o));
                add(&mut |o| c.gt_u530bc8(o));
                add(&mut |o| c.gt_campaign_error(o));
                add(&mut |o| c.gt_load_saved_game_name(o));
                add(&mut |o| c.gt_load_scenario_name(o));
                add(&mut |o| c.gt_lobby_missing(o));
                add(&mut |o| c.gt_campaign_editor_translated(o));
            }
            EE_SCREENS_VIEW_TEXTS_CAMPAIGN => {
                add(&mut |o| c.gt_campaign_description(o));
                add(&mut |o| c.gt_campaign_civilization(o));
                add(&mut |o| c.gt_campaign_name(o));
                add(&mut |o| c.gt_campaign_form_description(o));
            }
            EE_SCREENS_VIEW_TEXTS_INTRO => {
                add(&mut |o| c.gt_intro_briefing(o));
                add(&mut |o| c.gt_intro_hints(o));
                add(&mut |o| c.gt_intro_history(o));
            }
            EE_SCREENS_VIEW_TEXTS_PLAYER => {
                let player = c.gt_player(arg);
                if player != 0 {
                    add(&mut |o| c.gt_player_name_of(player, o));
                    add(&mut |o| c.gt_player_civilization(player, o));
                }
            }
            EE_SCREENS_VIEW_TEXTS_WORLD => add(&mut |o| c.gt_world_campaign_name(o)),
            EE_SCREENS_VIEW_TEXTS_RECORD => {
                if arg < c.gt_record_count() {
                    add(&mut |o| c.gt_record_name(arg, o));
                }
            }
            EE_SCREENS_VIEW_TEXTS_MAP_TYPE => add(&mut |o| c.gt_map_type(arg, o)),
            _ => {}
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_settings_find_the_current_rasterizer() {
        let mut f = Fake::default();
        f.answer("gv_rasterizer_count", &[3]).answer("gv_rasterizer_is_current", &[0, 1]);
        f.answer("gv_has_header", &[1]).answer("gv_header_players", &[4]);
        let v = view(&mut f, EE_SCREENS_VIEW_SETTINGS, 0);
        assert_eq!((v.current_rasterizer, v.header_players), (1, 4));
        assert_eq!(f.log.iter().filter(|l| l.starts_with("gv_rasterizer_is_current")).count(), 2);
    }

    #[test]
    fn the_session_and_the_intro() {
        let mut f = Fake::default();
        assert_eq!(view(&mut f, EE_SCREENS_VIEW_SESSION, 0), EeScreensGameView::default());
        assert_eq!(f.calls(), "gv_comms()");
        let mut f = Fake::default();
        f.answer("gv_comms", &[1]).answer("gv_is_server", &[1]).answer("gv_server_migrating", &[1]);
        assert!(view(&mut f, EE_SCREENS_VIEW_SESSION, 0).cannot_exit);
        assert!(!f.calls().contains("gv_client_playback"));
        let mut f = Fake::default();
        f.answer("gv_intro_loading", &[1]).answer("gv_intro_scroll", &[1]);
        let v = view(&mut f, EE_SCREENS_VIEW_INTRO, 0);
        assert_eq!(v.si_flags, EE_SCREENS_SI_FLAG_LOADING | EE_SCREENS_SI_FLAG_SCROLL);
    }

    #[test]
    fn the_texts_end_each_with_a_zero() {
        let mut f = Fake::default();
        f.text("gt_intro_briefing", b"brief").text("gt_intro_history", b"h");
        assert_eq!(texts(&mut f, EE_SCREENS_VIEW_TEXTS_INTRO, 0), b"brief\0\0h\0");
        let mut f = Fake::default();
        assert!(texts(&mut f, EE_SCREENS_VIEW_TEXTS_PLAYER, 3).is_empty());
        assert_eq!(f.calls(), "gt_player(3)");
        let mut f = Fake::default();
        f.answer("gt_record_count", &[2]).text("gt_record_name", b"r");
        assert_eq!(texts(&mut f, EE_SCREENS_VIEW_TEXTS_RECORD, 1), b"r\0");
        assert_eq!(texts(&mut f, EE_SCREENS_VIEW_TEXTS_SETTINGS, 0).len(), 19);
    }
}
