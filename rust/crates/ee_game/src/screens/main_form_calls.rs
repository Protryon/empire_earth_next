//! The main game form's host cases (`EEUIFMainGame::HostOp` and the mouse modes' `EEMouseModeOp`,
//! `User Interface Thread/EEUIFMainGame.cpp`) in Rust (UI lane round 7): each case that was more than one C++
//! statement, over the one-statement shims of [`Cpp`] (`game_cpp.rs`) on the host's call record (`rec`, an
//! `EEUIFMGFormCall*`). The host's adapter (`main_form_ffi.rs`) asks [`call`] before the C++ host.

use core::ffi::{c_long, c_ulong};

use crate::screens::game_cpp::{Cpp, EeScreensShimLongOut as OutL, EeScreensShimULongOut as OutUL};
use crate::screens::main_form::*;
use crate::screens::main_game::{EE_SCREENS_MG_CANCEL_UI_MODE, EE_SCREENS_MG_PAUSE};
use crate::screens::mouse_modes::*;
use crate::screens::shells_icon_bar::EE_SCREENS_SKOP_IB_UPDATE;
use crate::screens::shells_saved::EE_SCREENS_SKOP_SG_REFRESH;

/// `kMainGameUnitIconBar`.
const ICON_BAR: u32 = 222;
/// `kDisplayModeShown`.
const MODE_SHOWN: u32 = 1;

/// The case `op` (its arguments a, b and the io record), `None` for one that is not here.
pub(crate) fn call<C: Cpp + ?Sized>(
    c: &mut C,
    rec: usize,
    op: u32,
    a: i64,
    b: i64,
    io: &mut EeScreensMainFormIo,
) -> Option<i64> {
    let (x, y) = (io.f[0], io.f[1]);
    Some(match op {
        // ---- the mouse modes' (mouse_modes.rs)
        EE_SCREENS_MF_OP_MD_UI_STATE => {
            if b == 0 {
                c.mf_set_ui_state(rec, a as i32);
            } else if c.mf_has_wall(rec) {
                c.mf_wall_cancel(rec);
            } else {
                c.mf_gate_cancel(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_MD_BUTTONS => {
            let bits = [
                c.mouse_left_just_pressed(),
                c.mouse_left_down(),
                c.mouse_left_just_unpressed(),
                c.mouse_right_just_pressed(),
                c.mouse_right_down(),
                c.mouse_right_just_unpressed(),
            ];
            bits.iter().enumerate().map(|(i, on)| (*on as i64) << i).sum()
        }
        EE_SCREENS_MF_OP_MD_SHIFT => c.key_shift_value() as i64,
        EE_SCREENS_MF_OP_MD_VIEW => {
            if a != 0 {
                c.rasterizer_view_contains(x, y) as i64
            } else {
                c.view_contains(x, y) as i64
            }
        }
        EE_SCREENS_MF_OP_MD_UNIT if a == EE_SCREENS_MF_UNIT_HOSTILE_SEEN => {
            (!c.picked_owned_by_current(rec) && c.picked_owner_hostile(rec) && c.picked_seen_by_current(rec)) as i64
        }
        EE_SCREENS_MF_OP_MD_FIRST => {
            if a != 0 {
                return Some(c.mf_first_from_group(rec) as i64);
            }
            c.mf_first_clear(rec);
            let found = c.mf_first_selected(rec);
            (found as i64) | ((c.mf_has_first(rec) as i64) << 1)
        }
        EE_SCREENS_MF_OP_MD_FIRST_FIELD if a == EE_SCREENS_MF_FIRST_IN_RANGE => {
            let d = c.first_distance_squared(rec, io.f[0], io.f[1], io.f[2]);
            let r = c.first_min_range(rec);
            (d >= r * r) as i64
        }
        EE_SCREENS_MF_OP_MD_EVENT => {
            c.player_add_event(io.n[0], io.n[1], a as i32);
            0
        }
        EE_SCREENS_MF_OP_MD_GHOST => {
            if a < 0 {
                return Some(c.ghost_count() as i64);
            }
            let g = a as u32;
            match b {
                EE_SCREENS_MF_GHOST_CAN_PLACE => c.ghost_can_place(g) as i64,
                EE_SCREENS_MF_GHOST_TYPE => c.ghost_type(g) as i64,
                EE_SCREENS_MF_GHOST_TILE_X => c.ghost_tile_x(g),
                _ => c.ghost_tile_y(g),
            }
        }
        EE_SCREENS_MF_OP_MD_BUILD => {
            c.build_set_queued(rec, a != 0);
            c.build_at(rec, x, y);
            0
        }
        EE_SCREENS_MF_OP_MD_CAMERA => {
            if a == 0 {
                c.camera_clear_unit_a();
                c.camera_clear_unit_b();
            } else {
                c.camera_follow(0, false);
            }
            0
        }
        EE_SCREENS_MF_OP_MD_HACK => {
            match a {
                2 => c.hack_update(rec, x, y),
                0 => c.hack_point(rec, x, y),
                _ => c.hack_place(rec, x, y),
            }
            0
        }
        EE_SCREENS_MF_OP_MD_MOUSE_AT => {
            io.f[0] = c.mf_mouse_x(rec);
            io.f[1] = c.mf_mouse_y(rec);
            c.mf_mouse_mode(rec)
        }
        EE_SCREENS_MF_OP_MD_KEY => {
            if !c.key_exists(a as u32) {
                return Some(0);
            }
            1 | (c.key_value(a as u32) as i64) << 1 | (c.key_just_pressed(a as u32) as i64) << 2
        }
        EE_SCREENS_MF_OP_MD_CINEMATIC => {
            if a == EE_SCREENS_MF_CINE_ENDING {
                return Some(c.cinematic_ending() as i64);
            }
            if a == EE_SCREENS_MF_CINE_SCREENSHOT {
                c.post_cinematic_screen_shot();
                return Some(0);
            }
            let event = if a == EE_SCREENS_MF_CINE_END {
                c.end_cinematic_event_new()
            } else {
                c.screen_event_new(EE_SCREENS_MG_PAUSE)
            };
            c.post_event(event);
            c.event_dereference(event);
            0
        }
        EE_SCREENS_MF_OP_MD_START => {
            if a == 0 {
                c.mf_start_selection(rec);
            } else {
                c.mf_start_command(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_MD_BUILD_WALL => {
            if a == 0 {
                c.mf_wall_build(rec);
            } else {
                c.mf_gate_build(rec, x, y, b as i32);
            }
            0
        }
        EE_SCREENS_MF_OP_MD_GATE_PREVIEW => {
            let mut direction = io.n[0] as c_long;
            c.mf_gate_create_ghost(rec);
            c.gate_preview(x, y, OutL::of(&mut direction));
            io.n[0] = direction as i64;
            0
        }
        EE_SCREENS_MF_OP_MD_PLACEMENT => {
            if a == 0 {
                c.mf_wall_update(rec);
            } else {
                c.mf_gate_update(rec);
            }
            0
        }
        // ---- the form's (main_form.rs)
        EE_SCREENS_MF_OP_ERROR_LOCK => {
            if a != 0 {
                c.mf_error_lock_enter(rec);
            } else {
                c.mf_error_lock_leave(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_GROUP => {
            match a {
                EE_SCREENS_MF_GROUP_RESET => c.mf_group_reset(rec),
                EE_SCREENS_MF_GROUP_REFRESH => c.mf_group_refresh(rec),
                EE_SCREENS_MF_GROUP_SELECT => c.mf_group_select(rec),
                _ => return Some(c.mf_group_wants_refresh(rec) as i64),
            }
            0
        }
        EE_SCREENS_MF_OP_PICK => {
            if c.mf_pick(rec) == 0 {
                0
            } else if c.mf_picked_is_wall(rec) {
                2
            } else {
                1
            }
        }
        EE_SCREENS_MF_OP_PICKED_TILE => {
            io.n[0] = c.mf_picked_tile_x(rec);
            io.n[1] = c.mf_picked_tile_y(rec);
            0
        }
        EE_SCREENS_MF_OP_MOUSE_TILE => {
            let (mut tx, mut ty): (c_long, c_long) = (0, 0);
            c.mouse_tile(OutL::of(&mut tx), OutL::of(&mut ty));
            io.n[0] = tx as i64;
            io.n[1] = ty as i64;
            0
        }
        EE_SCREENS_MF_OP_RELEASE => {
            c.mf_picked_dereference(rec);
            c.mf_picked_clear(rec);
            0
        }
        EE_SCREENS_MF_OP_PREVIEW_GATE => {
            let mut direction = io.n[0] as c_long;
            c.mf_form_gate_create_ghost(rec);
            c.gate_preview_at_mouse(OutL::of(&mut direction));
            io.n[0] = direction as i64;
            0
        }
        EE_SCREENS_MF_OP_GHOST_LOCK => {
            if a != 0 {
                c.ghosts_lock_enter();
            } else {
                c.ghosts_lock_leave();
            }
            0
        }
        EE_SCREENS_MF_OP_WALL_START => {
            io.f[0] = c.mf_wall_start_x(rec);
            io.f[1] = c.mf_wall_start_y(rec);
            0
        }
        EE_SCREENS_MF_OP_COUNTDOWN => {
            let mut value: c_ulong = 0;
            let winner = c.team_wonder_countdown(a as u32, OutUL::of(&mut value));
            io.n[0] = value as i64;
            winner as i64
        }
        EE_SCREENS_MF_OP_INFO_SET_TEXT => {
            c.mf_info_set_text(rec);
            let text = c.mf_control(rec, EE_SCREENS_MF_CTL_DIALOG_MESSAGE as u32);
            c.control_set_display_mode(text, MODE_SHOWN);
            0
        }
        EE_SCREENS_MF_OP_INFO_HAS_BUTTON => {
            // (a class past the table is no portrait)
            let kind = c.mf_info_unit_type(rec);
            (kind < c.unit_class_count() && c.unit_class_exists(kind) && c.player_class_has_button(kind)) as i64
        }
        EE_SCREENS_MF_OP_INFO_PORTRAIT => {
            let portrait = c.mf_control(rec, EE_SCREENS_MF_CTL_DIALOG_PORTRAIT as u32);
            let bitmap = c.bitmap_new();
            c.mf_info_set_face(rec, bitmap, portrait);
            c.ui_bitmap_set_bitmap(portrait, bitmap);
            c.control_set_display_mode(portrait, MODE_SHOWN);
            c.control_refresh_opacity(portrait);
            c.bitmap_dereference(bitmap);
            0
        }
        EE_SCREENS_MF_OP_INFO_SOUND => {
            let (mut sample, mut length): (c_ulong, c_ulong) = (0, 0);
            c.mf_info_load_sound(rec, OutUL::of(&mut sample), OutUL::of(&mut length));
            io.n[0] = sample as i64;
            io.n[1] = length as i64;
            0
        }
        EE_SCREENS_MF_OP_NEW_POLL => {
            c.mf_poll_new(rec, a as u32);
            c.mf_poll_start(rec);
            0
        }
        EE_SCREENS_MF_OP_WINDOW_SIZE => {
            if a == 0 {
                io.n[0] = c.game_size_width() as i64;
                io.n[1] = c.game_size_height() as i64;
            } else {
                io.n[0] = c.game_window_width() as i64;
                io.n[1] = c.game_window_height() as i64;
            }
            0
        }
        EE_SCREENS_MF_OP_NEW_SCROLL => {
            c.mf_scroll_new(rec);
            c.mf_scroll_set_edges(rec, io.f[0], io.f[1], io.f[2], io.f[3]);
            0
        }
        EE_SCREENS_MF_OP_SCROLL_EDGES => {
            c.mf_scroll_set_edges(rec, io.f[0], io.f[1], io.f[2], io.f[3]);
            0
        }
        EE_SCREENS_MF_OP_NEW_ACTION => {
            match a {
                0 => c.mf_mouse_click_new(rec),
                1 => c.mf_lasso_new(rec),
                2 => c.mf_build_new(rec),
                _ => c.mf_calamity_new(rec),
            }
            0
        }
        EE_SCREENS_MF_OP_WHEEL_ZOOM => {
            c.mf_wheel_zoom_new(rec, a as u32);
            c.mf_set_wheel_event(rec);
            0
        }
        EE_SCREENS_MF_OP_GRAB_SCROLL => {
            c.mf_grab_scroll_new(rec, a as u32);
            c.mf_set_middle_pressed_event(rec);
            c.mf_set_middle_unpressed_event(rec);
            0
        }
        EE_SCREENS_MF_OP_ADD_HOT_KEY => {
            let mut events = [0usize; 4];
            for (i, e) in events.iter_mut().enumerate() {
                let code = ((io.n[i / 2] as u64) >> (i % 2 * 32)) as u32;
                *e = if code == EE_SCREENS_MF_NO_EVENT { 0 } else { c.screen_event_new(code) };
            }
            let keys = [a & 0xffff, a >> 16 & 0xffff, a >> 32 & 0xffff, a >> 48 & 0xffff].map(|k| k as u32);
            c.mf_add_hot_key(
                rec,
                keys[0],
                keys[1],
                keys[2],
                keys[3],
                events[0],
                events[1],
                events[2],
                events[3],
                b as u32,
                (b >> 32) != 0,
            );
            0
        }
        EE_SCREENS_MF_OP_UNIT => {
            c.mf_unit_load(rec, a as u32);
            let unit = c.mf_unit(rec);
            if unit == 0 {
                0
            } else if !c.unit_has_complex(unit) {
                1
            } else {
                2
            }
        }
        EE_SCREENS_MF_OP_GHOST => {
            if a == 0 {
                c.mf_ghost_of_node(rec);
            } else {
                c.mf_ghost_of_calamity(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_GATE_BEGIN => {
            c.mf_gate_set_technology(rec, a as i32);
            c.mf_gate_wall_type_from_node(rec);
            c.mf_gate_class_from_node(rec);
            c.mf_gate_initialize(rec);
            0
        }
        EE_SCREENS_MF_OP_WALL_BEGIN => {
            c.mf_wall_set_technology(rec, a as i32);
            c.mf_wall_initialize(rec);
            0
        }
        EE_SCREENS_MF_OP_CASTER => {
            if c.selected_count() != 0 {
                c.mf_caster_first(rec);
            } else {
                c.mf_caster_clear(rec);
            }
            c.mf_has_caster(rec) as i64
        }
        EE_SCREENS_MF_OP_CALAMITY_SET => {
            c.mf_calamity_set(rec, a as i32);
            c.mf_calamity_set_caster(rec);
            c.mf_calamity_target_kind(rec)
        }
        EE_SCREENS_MF_OP_MOUSE_RESET => {
            c.mouse_shift_off();
            c.mouse_set_pointer(0);
            c.mouse_set_bounds(io.f[0], io.f[1], io.f[2], io.f[3]);
            0
        }
        EE_SCREENS_MF_OP_UNPAUSE => {
            let server = c.server_dynamic();
            c.server_unpause(server);
            c.post_pause_information(false);
            0
        }
        EE_SCREENS_MF_OP_GRAPHICS_UI => {
            if c.graphics() != 0 {
                c.gfx_clear_render_objects();
                c.gfx_push_ui_render_object();
            }
            0
        }
        EE_SCREENS_MF_OP_CANCEL_EVENT => {
            let event = c.screen_event_new(EE_SCREENS_MG_CANCEL_UI_MODE);
            c.mf_cancel_button_up(rec, event);
            c.mf_cancel_right_button_up(rec, event);
            c.event_dereference(event);
            0
        }
        EE_SCREENS_MF_OP_BLINKING => {
            if a == 0 {
                c.civ_builder_button_blinking(b != 0);
            } else {
                c.intro_set_blinking(b != 0);
            }
            0
        }
        EE_SCREENS_MF_OP_MAIN_GROUP => {
            if a == 0 {
                c.mf_main_group_open(rec);
            } else {
                c.mf_main_group_clear_l20(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_POST_GRAPHICS => {
            c.post_graphics_all();
            c.gfx_fade_in(0);
            0
        }
        EE_SCREENS_MF_OP_MEDIA_LOAD => {
            if a != 0 {
                c.mf_media_load(rec);
            } else {
                c.mf_old_media_unload(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_SAVED_GROUP_REFRESH | EE_SCREENS_MF_OP_ICON_BAR_UPDATE => {
            let icon_bar = op == EE_SCREENS_MF_OP_ICON_BAR_UPDATE;
            let control = c.mf_control(rec, if icon_bar { ICON_BAR } else { a as u32 });
            if control == 0 {
                return Some(0);
            }
            c.shell_op(control, if icon_bar { EE_SCREENS_SKOP_IB_UPDATE } else { EE_SCREENS_SKOP_SG_REFRESH });
            1
        }
        EE_SCREENS_MF_OP_DRAW_BACKGROUND => {
            let background = c.mf_background(rec, a as u32);
            c.background_draw(background);
            0
        }
        EE_SCREENS_MF_OP_FPS => {
            io.f[0] = c.mf_fps(rec);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "main_form_calls_tests.rs"]
mod tests;
