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
                crate::screens::sci_calls::get(c);
                crate::screens::sci_calls::set_blinking(c, b != 0);
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
        // ---- the rest of the original host (UI lane round 9): with these EEUIFMainGame::HostOp and the mouse
        // modes' EEMouseModeOp are gone
        EE_SCREENS_MF_OP_FIELD | EE_SCREENS_MF_OP_SET_FIELD => {
            // (the fields of EeScreensMainFormFields are main_form_fields.rs')
            let set = op == EE_SCREENS_MF_OP_SET_FIELD;
            match a {
                EE_SCREENS_MF_FIELD_GATE_DIRECTION => {
                    if set {
                        c.mf_set_gate_direction(rec, b);
                    }
                    c.mf_gate_direction(rec)
                }
                EE_SCREENS_MF_FIELD_WALL_STATE => {
                    if set {
                        c.mf_set_wall_state(rec, b);
                    }
                    c.mf_wall_state(rec)
                }
                EE_SCREENS_MF_FIELD_TOOLTIP_SHADOW => {
                    if set {
                        c.mf_set_tooltip_shadow(rec, b != 0);
                    }
                    c.mf_tooltip_shadow(rec) as i64
                }
                EE_SCREENS_MF_FIELD_BUILD_TECH => {
                    if set {
                        c.mf_set_build_tech(rec, b);
                    }
                    c.mf_build_tech(rec)
                }
                _ => 0,
            }
        }
        EE_SCREENS_MF_OP_LOG_LINE => {
            c.mf_log_line(rec, a);
            0
        }
        EE_SCREENS_MF_OP_POLL => c.mf_poll(rec),
        EE_SCREENS_MF_OP_SCROLL => {
            c.mf_scroll(rec);
            0
        }
        EE_SCREENS_MF_OP_FIND_GATE_WALL => {
            // the wall at tile (a, b) and its direction (io.n[0] in and out)
            c.mf_tile_x(a);
            c.mf_tile_y(b);
            let mut direction = io.n[0] as c_long;
            let found = c.mf_find_gate_wall(OutL::of(&mut direction));
            io.n[0] = direction as i64;
            found as i64
        }
        EE_SCREENS_MF_OP_CLEAR_GHOSTS => {
            c.mf_clear_ghosts();
            0
        }
        EE_SCREENS_MF_OP_PREVIEW_WALL => {
            // the a tiles at b (x, y pairs) previewed with the running placement's (or the form's) wall
            // SAFETY: the tiles main_form.rs hands with the op (a live slice of a pairs)
            let tiles = unsafe { core::slice::from_raw_parts(b as usize as *const [i32; 2], a.max(0) as usize) };
            c.mf_tiles_clear();
            for t in tiles {
                c.mf_tile_x(t[0] as i64);
                c.mf_tile_y(t[1] as i64);
                c.mf_tiles_add();
            }
            if c.mf_has_wall(rec) {
                c.mf_preview_wall_call(rec);
            } else {
                c.mf_preview_wall_form(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_PREVIEW_PLACEMENT => {
            c.mf_preview_placement_mouse();
            0
        }
        EE_SCREENS_MF_OP_POPULATION => c.mf_population(),
        EE_SCREENS_MF_OP_POPULATION_LIMIT => c.mf_population_limit(),
        EE_SCREENS_MF_OP_NUM_TEAMS => c.mf_num_teams(),
        EE_SCREENS_MF_OP_LABEL_PLAYER_COLOR => {
            c.mf_label_player_color(rec, a, b);
            0
        }
        EE_SCREENS_MF_OP_TEXT_WONDER => {
            c.mf_wonder_format();
            c.mf_wonder_print(rec, a, b);
            0
        }
        EE_SCREENS_MF_OP_TEXT_TIME => {
            c.mf_text_time(rec);
            0
        }
        EE_SCREENS_MF_OP_HAS_COMMS => c.gv_comms() as i64,
        EE_SCREENS_MF_OP_COMMS_PAUSED => c.mf_comms_paused() as i64,
        EE_SCREENS_MF_OP_HAS_TEAMS => c.mf_has_teams() as i64,
        EE_SCREENS_MF_OP_PAUSE_TEXT => {
            c.mf_pause_text(rec, a);
            0
        }
        EE_SCREENS_MF_OP_LAG_STATUS => c.mf_lag_status(),
        EE_SCREENS_MF_OP_LAG_COLOR => {
            c.mf_lag_color(rec, a, b);
            0
        }
        EE_SCREENS_MF_OP_HAS_INSTRUCTIONS => c.mf_has_instructions() as i64,
        EE_SCREENS_MF_OP_UPDATE_CURSOR => {
            c.mf_update_cursor(rec);
            0
        }
        EE_SCREENS_MF_OP_INFO_DURATION => c.mf_info_duration(rec),
        EE_SCREENS_MF_OP_CHAT_TOP => {
            c.mf_chat_top(rec, a);
            0
        }
        EE_SCREENS_MF_OP_HIDE_MESSAGES => {
            c.mf_hide_messages(rec);
            0
        }
        EE_SCREENS_MF_OP_CREATE_ERROR_LOCK => {
            c.mf_create_error_lock(rec);
            0
        }
        EE_SCREENS_MF_OP_CREATE_GROUP => {
            c.mf_create_group(rec, a);
            0
        }
        EE_SCREENS_MF_OP_NEW_UNIT_HACK => {
            c.mf_new_unit_hack(rec);
            0
        }
        EE_SCREENS_MF_OP_BUILD_TERMINATE => {
            if a == 0 {
                c.mf_wall_terminate(rec);
            } else {
                c.mf_gate_terminate(rec);
            }
            0
        }
        EE_SCREENS_MF_OP_NODE => {
            c.mf_node_load(rec, a);
            c.mf_has_node(rec) as i64
        }
        EE_SCREENS_MF_OP_NODE_CLASS => (c.mf_has_node(rec) && c.mf_node_class_exists(rec)) as i64,
        EE_SCREENS_MF_OP_CAN_PRODUCE => c.mf_can_produce(rec, a),
        EE_SCREENS_MF_OP_PLAY_SOUND => {
            c.mf_play_sound(a);
            0
        }
        EE_SCREENS_MF_OP_CHAT_LOG_ERROR => {
            c.mf_chat_log_error(rec);
            0
        }
        EE_SCREENS_MF_OP_SELECTED_COUNT => c.mf_selected_count(),
        EE_SCREENS_MF_OP_NODE_CALAMITY => c.mf_node_calamity(rec),
        EE_SCREENS_MF_OP_CALAMITY_CLASS => {
            c.mf_calamity_load(rec, a);
            c.mf_has_calamity(rec) as i64
        }
        EE_SCREENS_MF_OP_CALAMITY_CAST => {
            c.mf_calamity_cast(rec);
            0
        }
        EE_SCREENS_MF_OP_MOUSE_FREEZE => {
            c.sc_cam_freeze(a != 0);
            0
        }
        EE_SCREENS_MF_OP_USER_PAUSED => c.mf_user_paused() as i64,
        EE_SCREENS_MF_OP_IS_SERVER => c.gv_is_server() as i64,
        EE_SCREENS_MF_OP_MASTER_VOLUME => {
            c.mf_master_volume();
            0
        }
        EE_SCREENS_MF_OP_MOUSE_POSITION => {
            c.mf_mouse_position(x, y);
            0
        }
        EE_SCREENS_MF_OP_FILL_HACK => {
            c.mf_fill_hack(rec);
            0
        }
        EE_SCREENS_MF_OP_HAS_TECHS => c.mf_has_techs() as i64,
        EE_SCREENS_MF_OP_TEXT_EPOCH_TITLE => {
            c.mf_text_epoch_title(rec);
            0
        }
        EE_SCREENS_MF_OP_UNIT_INFO_BACKGROUND => {
            c.mf_unit_info_background(rec);
            0
        }
        EE_SCREENS_MF_OP_MEDIA_OLD => {
            c.mf_media_old_load(rec);
            c.mf_has_media_old(rec) as i64
        }
        EE_SCREENS_MF_OP_PLAYER_MEDIA_ID => c.mf_player_media_id(),
        EE_SCREENS_MF_OP_USING_CARBON => c.mf_using_carbon() as i64,
        EE_SCREENS_MF_OP_MEDIA => {
            c.mf_media_get(rec, a);
            c.mf_has_media(rec) as i64
        }
        EE_SCREENS_MF_OP_BG_TEXTURE => {
            c.mf_bg_texture(rec, a, b);
            0
        }
        EE_SCREENS_MF_OP_POST_SHOW_DIALOG => {
            c.sc_mg_show_dialog();
            0
        }
        // the mouse modes' (mouse_modes.rs): the picked unit, the first selected unit and the pointed tile held in
        // the call; io.f[0..2] the mouse location where an op takes one
        EE_SCREENS_MF_OP_MD_CAPTURED => c.md_captured(rec) as i64,
        EE_SCREENS_MF_OP_MD_POINTER => {
            c.sc_mg_pointer(a as i32);
            0
        }
        EE_SCREENS_MF_OP_MD_POINT => {
            if a == 0 {
                c.md_point_reset();
                let found = c.md_point(rec, x, y);
                io.f[0] = c.md_point_x();
                io.f[1] = c.md_point_y();
                io.f[2] = c.md_point_z();
                found as i64
            } else {
                let (mut tx, mut ty): (c_long, c_long) = (-10000, -10000);
                let found = c.md_point_xy(rec, x, y, OutL::of(&mut tx), OutL::of(&mut ty));
                io.n[0] = tx as i64;
                io.n[1] = ty as i64;
                found as i64
            }
        }
        EE_SCREENS_MF_OP_MD_TILE_SEEN => c.md_tile_seen(rec) as i64,
        EE_SCREENS_MF_OP_MD_PICK => c.md_pick(rec, x, y) as i64,
        // (the unit's plain fields, and the first unit's, are hud_views.rs mouse_mode's)
        EE_SCREENS_MF_OP_MD_UNIT => match a {
            EE_SCREENS_MF_UNIT_CAN_BE_ATTACKED => c.md_can_be_attacked(rec) as i64,
            EE_SCREENS_MF_UNIT_IN_WONDER_SET => c.md_in_wonder_set(rec) as i64,
            _ => 0,
        },
        EE_SCREENS_MF_OP_MD_DAMAGE => {
            io.f[0] = c.md_damage(a, b);
            0
        }
        EE_SCREENS_MF_OP_MD_FIRST_FIELD => {
            if a == EE_SCREENS_MF_FIRST_IS_CASTER {
                c.md_is_caster(rec) as i64
            } else {
                0
            }
        }
        EE_SCREENS_MF_OP_MD_CAN_CONVERT => {
            c.md_in_range_set(b != 0);
            let found = c.md_can_convert(rec, a);
            (found as i64) | ((c.md_in_range() as i64) << 1)
        }
        EE_SCREENS_MF_OP_MD_SEND => {
            // (the airport's, the attack ground's and the flare's are hud_sends.rs')
            match a {
                EE_SCREENS_MF_SEND_LOAD_GARRISON => c.md_send_load_garrison(rec),
                EE_SCREENS_MF_SEND_UNLOAD => c.md_send_unload(rec, io.n[0], io.n[1]),
                EE_SCREENS_MF_SEND_HEAL => c.md_send_heal(rec),
                EE_SCREENS_MF_SEND_CONVERT => c.md_send_convert(rec, b != 0),
                _ => c.md_send_rally(rec, x, y, io.n[0], io.n[1]),
            }
            0
        }
        EE_SCREENS_MF_OP_MD_ORDER_AT => {
            // no unit, the picked unit or the picked unit's complex one
            match a {
                0 => c.md_order_at_none(x, y),
                1 => c.md_order_at_picked(rec, x, y),
                _ if c.md_has_picked(rec) => c.md_order_at_complex(rec, x, y),
                _ => c.md_order_at_none(x, y),
            }
            0
        }
        EE_SCREENS_MF_OP_MD_CALAMITY_EFFECT => {
            c.md_calamity_effect(rec, x, y, io.f[2]);
            0
        }
        EE_SCREENS_MF_OP_MD_PREVIEW => {
            c.md_preview(rec);
            0
        }
        EE_SCREENS_MF_OP_MD_CALAMITY_CHECK => c.md_calamity_check(rec, x, y),
        EE_SCREENS_MF_OP_MD_IN_GAME_VIEW => c.md_in_game_view(rec, x, y) as i64,
        EE_SCREENS_MF_OP_MD_ENTITY_ENABLED => c.md_entity_enabled(rec, a) as i64,
        EE_SCREENS_MF_OP_MD_SET_ENTITY_GROUP => {
            c.md_set_entity_group(rec, a);
            0
        }
        EE_SCREENS_MF_OP_MD_GATE_CLEAR => {
            c.md_gate_clear(rec);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "main_form_calls_tests.rs"]
mod tests;
