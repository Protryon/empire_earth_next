//! `ee_screenscpp_ui_message`'s cases whole in Rust over one-statement shims (UI lane round 11; the C++ host,
//! `User Interface Thread/EEUserInterface.cpp`, is gone): the graphics' pause and resume, the status bar, the
//! resizes, the screen capture's wait, the error box's callbacks, the chat entries, the information, the icon bar's
//! update, the rasterizer change, the posted events, the editor's tab, the saved groups, the server choice and the
//! message of the day. `ui_thread_ffi.rs`' host answers the other ops before these.

use crate::screens::game_cpp::Cpp;
use crate::screens::main_form::EE_SCREENS_MF_DO_UPDATE_MEDIA;
use crate::screens::main_form::{
    EE_SCREENS_MF_DO_HIDE_MESSAGES, EE_SCREENS_MF_DO_ICON_BAR, EE_SCREENS_MF_DO_INFO_PAUSE,
};
use crate::screens::ui_thread::*;

/// The original's line of the capture event's check (`ESVERIFY_AT`).
const CAPTURE_VERIFY_LINE: u32 = 1956;

/// The main game form (0 without the UI manager).
fn main_form<C: Cpp + ?Sized>(c: &mut C) -> usize {
    if c.uim_has_manager() {
        c.uim_main_form()
    } else {
        0
    }
}

/// The message host's op `op` on the UI object `ui` (an `EEUserInterface*`); 0 for an op that is none of these.
pub(crate) fn ui_message<C: Cpp + ?Sized>(c: &mut C, ui: usize, op: u32, a: i64, b: i64, cc: i64) -> i64 {
    let p = a as usize;
    match op {
        EE_SCREENS_UI_OP_PAUSE => c.uim_pause(ui),
        EE_SCREENS_UI_OP_RESUME => c.uim_resume(ui),
        EE_SCREENS_UI_OP_STATUS_VALUE => c.uim_status_value(p, b as u64, cc),
        EE_SCREENS_UI_OP_RESIZE_GAME => c.uim_resize_game(ui),
        EE_SCREENS_UI_OP_RESIZE_MENU => c.uim_resize_menu(ui),
        EE_SCREENS_UI_OP_SCREEN_ACTIVE => c.uim_screen_active(),
        EE_SCREENS_UI_OP_POST_LOAD_GAME => c.uim_post_load_game(),
        EE_SCREENS_UI_OP_BACKGROUND_MEDIA => {
            let main = main_form(c);
            c.uim_main_rust(main, EE_SCREENS_MF_DO_UPDATE_MEDIA, 1);
        }
        EE_SCREENS_UI_OP_GFX_POST_PAUSE => return c.uim_gfx_post_pause() as i64,
        EE_SCREENS_UI_OP_GFX_PAUSED => return c.uim_gfx_paused() as i64,
        EE_SCREENS_UI_OP_GFX_POST_RESUME => c.uim_gfx_post_resume(),
        EE_SCREENS_UI_OP_RESET_GFX_VALUES => c.uim_reset_gfx_values(),
        // a: the capture (its event made, the shot posted and waited for, its surface and itself freed)
        EE_SCREENS_UI_OP_SCREEN_CAPTURE => {
            if !c.uim_capture_event(p) {
                c.uim_verify(CAPTURE_VERIFY_LINE);
            }
            c.uim_capture_post(p);
            c.uim_capture_wait(p);
            c.uim_capture_event_free(p);
            if c.uim_capture_width(p) != -1 {
                c.uim_capture_surface_release(p);
            }
            c.uim_capture_delete(p);
        }
        EE_SCREENS_UI_OP_POST_GFX_ALL => c.uim_post_gfx_all(),
        EE_SCREENS_UI_OP_ERROR_HAS_CALLBACK => return c.uim_error_has_callback(p) as i64,
        EE_SCREENS_UI_OP_ERROR_CALLBACK => c.uim_error_callback(p),
        EE_SCREENS_UI_OP_DELETE_FORM => c.uim_delete_form(p),
        EE_SCREENS_UI_OP_DELETE_ERROR_DATA => c.uim_delete_error_data(p),
        EE_SCREENS_UI_OP_CHAT_SHOW => {
            let main = main_form(c);
            c.uim_chat_show(main, p);
        }
        EE_SCREENS_UI_OP_CHAT_TYPE => return c.uim_chat_type(p),
        EE_SCREENS_UI_OP_CHAT_LOG => c.uim_chat_log(p),
        EE_SCREENS_UI_OP_DELETE_CHAT => c.uim_delete_chat(p),
        EE_SCREENS_UI_OP_SHOW_INFORMATION => {
            let main = main_form(c);
            c.uim_show_information(main, p);
        }
        EE_SCREENS_UI_OP_DELETE_DIALOG => c.uim_delete_dialog(p),
        EE_SCREENS_UI_OP_UPDATE_ICON_BAR => {
            let main = main_form(c);
            c.uim_main_rust(main, EE_SCREENS_MF_DO_ICON_BAR, 0);
            c.uim_icon_bar_posted_clear(ui);
        }
        EE_SCREENS_UI_OP_CHANGE_RASTERIZER => c.uim_change_rasterizer(ui, a, b),
        EE_SCREENS_UI_OP_INFORMATION => {
            let main = main_form(c);
            c.uim_main_rust(main, EE_SCREENS_MF_DO_INFO_PAUSE, (a != 0) as i64);
        }
        EE_SCREENS_UI_OP_POST_EVENT => c.uim_post_event(p),
        EE_SCREENS_UI_OP_HIDE_MESSAGES => {
            let main = main_form(c);
            c.uim_main_rust(main, EE_SCREENS_MF_DO_HIDE_MESSAGES, 0);
        }
        EE_SCREENS_UI_OP_SELECT_TAB => c.uim_select_tab(a as u8, b),
        EE_SCREENS_UI_OP_SAVED_GROUP => {
            let main = main_form(c);
            c.uim_saved_group(main, a as u64);
        }
        EE_SCREENS_UI_OP_SELECT_SERVER => c.uim_select_server(ui),
        EE_SCREENS_UI_OP_MOTD => c.uim_motd(ui, p, b as usize),
        EE_SCREENS_UI_OP_DELETE_WSTRING => c.uim_delete_wstring(p),
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_capture_waits_then_frees_its_surface_unless_none() {
        let mut fake = Fake::default();
        fake.answer("uim_capture_event", &[1]).answer("uim_capture_width", &[-1]);
        ui_message(&mut fake, 1, EE_SCREENS_UI_OP_SCREEN_CAPTURE, 5, 0, 0);
        assert_eq!(
            fake.calls(),
            "uim_capture_event(5)\nuim_capture_post(5)\nuim_capture_wait(5)\nuim_capture_event_free(5)\n\
             uim_capture_width(5)\nuim_capture_delete(5)"
        );
        let mut fake = Fake::default();
        ui_message(&mut fake, 1, EE_SCREENS_UI_OP_SCREEN_CAPTURE, 5, 0, 0);
        assert!(fake.calls().starts_with("uim_capture_event(5)\nuim_verify(1956)"));
        assert!(fake.calls().contains("uim_capture_surface_release(5)"));
    }

    #[test]
    fn the_icon_bar_update_clears_its_post() {
        let mut fake = Fake::default();
        fake.answer("uim_has_manager", &[1]).answer("uim_main_form", &[8]);
        ui_message(&mut fake, 1, EE_SCREENS_UI_OP_UPDATE_ICON_BAR, 0, 0, 0);
        assert_eq!(
            fake.calls(),
            format!(
                "uim_has_manager()\nuim_main_form()\nuim_main_rust(8, {EE_SCREENS_MF_DO_ICON_BAR}, 0)\n\
                 uim_icon_bar_posted_clear(1)"
            )
        );
    }

    #[test]
    fn the_answers_and_an_unknown_op() {
        let mut fake = Fake::default();
        fake.answer("uim_chat_type", &[3]).answer("uim_gfx_paused", &[1]);
        assert_eq!(ui_message(&mut fake, 1, EE_SCREENS_UI_OP_CHAT_TYPE, 4, 0, 0), 3);
        assert_eq!(ui_message(&mut fake, 1, EE_SCREENS_UI_OP_GFX_PAUSED, 0, 0, 0), 1);
        assert_eq!(ui_message(&mut fake, 1, 9999, 0, 0, 0), 0);
    }
}
