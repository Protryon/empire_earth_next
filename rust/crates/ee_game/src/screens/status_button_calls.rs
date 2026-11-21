//! The command buttons in Rust over one-statement C++ shims (UI lane round 8; the shims are `ee_screenscpp_sbt_*`
//! in `User Interface Thread/EEUIFMGStatusButton.cpp`): the main game form host's button ops (the original's
//! `EEStatusButtonOp`, `EE_SCREENS_MF_OP_SB + EE_SCREENS_SB_OP_*`, on the form's call record `rec`: the button, the
//! first selected unit, the held text) that `hud_direct.rs` does not answer from the records, and the button's
//! members (`button` an `EEUIFMGStatusButton*`): the constructor's body, the destructor, `ReleaseButton`,
//! `DrawPolygons`, `DrawTextA`, `HotKeyUp`, `SetCommandButton`, `SetTechButton`, `SetLocation`, and
//! `EEUIManager::Fn26FA55`/`Fn26FA6A` (the main group's refill).

use core::ffi::c_long;

use crate::screens::command_panel::{EE_SCREENS_BUTTON_ICON_NONE, EE_SCREENS_BUTTON_ICON_PRESSED};
use crate::screens::game_cpp::{Cpp, EeScreensShimText};
use crate::screens::status_button::*;

/// The tech tree node of the record's button (none for a command button).
fn sbt_node_of(c: &mut impl Cpp, rec: usize) -> usize {
    if c.sbt_is_command(rec) {
        0
    } else {
        c.sbt_node(rec)
    }
}

/// One button op (`op` without `EE_SCREENS_MF_OP_SB`; `cc` the io's first number); 0 for an op the host does not
/// know, as the C++'s.
pub(crate) fn call(c: &mut impl Cpp, rec: usize, op: u32, a: i64, b: i64, _cc: i64) -> i64 {
    match op {
        EE_SCREENS_SB_OP_MAIN_GROUP => main_group_refill(c, a == 0),
        EE_SCREENS_SB_OP_UNIT_POWER => {
            let unit = c.sbt_unit(a);
            return if b != 0 { c.sbt_unit_get_power(unit) } else { c.sbt_unit_power(unit) };
        }
        EE_SCREENS_SB_OP_UNLOAD_COST => return c.sbt_unload_cost(),
        EE_SCREENS_SB_OP_FIRST_CATEGORY => return c.sbt_first_category(rec),
        EE_SCREENS_SB_OP_FIRST_TYPE => return c.sbt_first_type(rec),
        EE_SCREENS_SB_OP_SEND => match a {
            EE_SCREENS_SB_SEND_UNLOAD => c.sbt_send_unload(rec),
            EE_SCREENS_SB_SEND_RESEARCH => c.sbt_send_research(rec, b != 0),
            _ => c.sbt_send_remove(rec, b != 0),
        },
        EE_SCREENS_SB_OP_START_PATROL => c.sbt_start_patrol(rec),
        EE_SCREENS_SB_OP_START_FLARE => c.sbt_start_flare(rec),
        EE_SCREENS_SB_OP_GAME_RESOURCES => return c.sbt_game_resources(),
        EE_SCREENS_SB_OP_GAME_CLOCK => return c.sbt_game_clock().to_bits() as i64,
        EE_SCREENS_SB_OP_WORLD_LONG => return c.sbt_world_long(a),
        EE_SCREENS_SB_OP_CHAT_STRING => {
            c.sbt_string_clear();
            c.sbt_string_load(a);
            c.sbt_chat_add_string();
        }
        EE_SCREENS_SB_OP_SHIFT => return c.sbt_shift() as i64,
        EE_SCREENS_SB_OP_NODE_KIND => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_kind(node);
        }
        EE_SCREENS_SB_OP_NODE_CLASS => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_class(node);
        }
        EE_SCREENS_SB_OP_CLASS_LAND_BUILDING => {
            let class = c.sbt_class(a);
            return (c.sbt_class_is_land_building(class) && !c.sbt_class_naval_building(class)) as i64;
        }
        EE_SCREENS_SB_OP_NODE_MAKES_UNIT => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_makes_unit(node);
        }
        EE_SCREENS_SB_OP_BUILD_UNIT => c.sbt_build_unit(rec),
        EE_SCREENS_SB_OP_START_WALL => c.sbt_start_wall(rec),
        EE_SCREENS_SB_OP_START_GATE => c.sbt_start_gate(rec),
        EE_SCREENS_SB_OP_CAN_PRODUCE => return c.sbt_can_produce(rec, a) as i64,
        EE_SCREENS_SB_OP_CALAMITY_COST => {
            let node = sbt_node_of(c, rec);
            let calamity = c.sbt_node_calamity_class(node);
            return if calamity != 0 { c.sbt_calamity_cost(calamity) } else { 0 };
        }
        EE_SCREENS_SB_OP_NODE_CONTAINS => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_contains(node, a) as i64;
        }
        EE_SCREENS_SB_OP_START_CALAMITY => c.sbt_start_calamity(rec),
        EE_SCREENS_SB_OP_BUTTON_UP => c.sbt_button_up(rec),
        EE_SCREENS_SB_OP_MOUSE_INSIDE => return c.sbt_mouse_inside(rec) as i64,
        EE_SCREENS_SB_OP_COUNT_TEXT => {
            let text = format!("{}", a as c_long);
            c.sbt_count_text(rec, EeScreensShimText::of(text.as_bytes()));
        }
        EE_SCREENS_SB_OP_NODE_CALAMITY => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_calamity(node);
        }
        EE_SCREENS_SB_OP_CALAMITY_CLASS_COST => {
            let calamity = c.sbt_calamity_class(a);
            return c.sbt_calamity_cost(calamity);
        }
        EE_SCREENS_SB_OP_IN_FANATIC_SET => return c.sbt_in_fanatic_set(a) as i64,
        EE_SCREENS_SB_OP_UNIT_CLOAKED => {
            let unit = c.sbt_unit(a);
            return c.sbt_unit_cloaked(unit) as i64;
        }
        EE_SCREENS_SB_OP_HELP_TEXT => return help_text(c, rec, a, b),
        EE_SCREENS_SB_OP_TRADE_PRICE => return c.sbt_trade_price(a, b != 0),
        EE_SCREENS_SB_OP_UNIT_PRIEST => {
            let unit = c.sbt_unit(a);
            return (unit != 0 && c.sbt_unit_is_priest(unit)) as i64;
        }
        EE_SCREENS_SB_OP_CONVERSION_COST => {
            let unit = c.sbt_unit(a);
            return c.sbt_conversion_cost(unit);
        }
        EE_SCREENS_SB_OP_NODE_PRESENT => return (sbt_node_of(c, rec) != 0) as i64,
        EE_SCREENS_SB_OP_NODE_NAME_ID => {
            let node = sbt_node_of(c, rec);
            return c.sbt_node_name_id(node);
        }
        EE_SCREENS_SB_OP_CLASS_UPGRADE_NAMED => {
            let class = c.sbt_class(a);
            return c.sbt_class_upgrade_named(class) as i64;
        }
        EE_SCREENS_SB_OP_CLASS_POPULATION => {
            let class = c.sbt_class(a);
            return c.sbt_class_population(class);
        }
        EE_SCREENS_SB_OP_HAS_OBJECT => return c.sbt_has_object(rec) as i64,
        EE_SCREENS_SB_OP_ICON => {
            if a == EE_SCREENS_BUTTON_ICON_NONE as i64 {
                c.sbt_set_draw_icon(rec, false);
            } else {
                if a == EE_SCREENS_BUTTON_ICON_PRESSED as i64 {
                    c.sbt_object_pressed_icon(rec);
                } else {
                    c.sbt_object_face_icon(rec);
                }
                c.sbt_set_draw_icon(rec, true);
            }
        }
        EE_SCREENS_SB_OP_CAN_PRODUCE_QUIET => return c.sbt_can_produce_quiet(rec) as i64,
        _ => {}
    }
    0
}

/// The help text's pieces into the record's text (`EE_SCREENS_SB_TEXT_*` a): its length, -1 without a hot key.
fn help_text(c: &mut impl Cpp, rec: usize, a: i64, b: i64) -> i64 {
    match a {
        EE_SCREENS_SB_TEXT_STRING => c.sbt_text_string(rec, b),
        EE_SCREENS_SB_TEXT_OBJECT_HOT_KEY => {
            let object = c.sbt_button_object_of(rec);
            if object == 0 {
                return -1;
            }
            let hot_key = c.sbt_object_hot_key(object);
            if hot_key == 0 {
                return -1;
            }
            c.sbt_hot_key_name(rec, hot_key);
        }
        EE_SCREENS_SB_TEXT_NODE_HOT_KEY => {
            let node = sbt_node_of(c, rec);
            let hot_key = c.sbt_node_hot_key(node);
            if hot_key == 0 {
                return -1;
            }
            c.sbt_hot_key_name(rec, hot_key);
        }
        EE_SCREENS_SB_TEXT_UPGRADE_NAME => c.sbt_text_upgrade_name(rec, b),
        EE_SCREENS_SB_TEXT_COST => {
            let node = sbt_node_of(c, rec);
            c.sbt_text_node_cost(rec, node);
        }
        _ => {}
    }
    c.sbt_text_length(rec)
}

/// `EEUIManager::Fn26FA6A` (`set`) / `Fn26FA55`: the main panel refills its unit, its flag set or cleared.
fn main_group_refill(c: &mut impl Cpp, set: bool) {
    c.sbt_main_group_l1c(set as i64);
    c.sbt_main_group_reset();
}

/// The constructor's body after the base and the record (`manager` a `UIManager*`, `rectangle` its
/// `GERectangle*`, `table` its `UIControlsTable*`): the bitmaps, the icon, the queue count text and the dither.
pub(crate) fn construct(c: &mut impl Cpp, button: usize, manager: usize, rectangle: usize, table: usize) {
    c.sbt_object_clear(button);
    let normal = c.sbt_normal_bitmap(button, manager, table);
    let pressed = c.sbt_pressed_bitmap(button, manager, table);
    c.sbt_set_pressed_bitmap(button, pressed);
    c.sbt_set_normal_bitmap(button, normal);
    c.um_bitmap_dereference(normal);
    c.um_bitmap_dereference(pressed);
    c.sbt_set_f13x(button, 40.0);
    c.sbt_icon_new(button);
    let (left, top) = (c.sbt_left(button), c.sbt_top(button));
    c.sbt_icon_at(button, left, top);
    c.sbt_queue_new(button, table);
    c.sbt_queue_verify(button);
    c.sbt_queue_at(button, left + 3.0, top + 1.0);
    c.sbt_queue_setup(button, table);
    let texture = c.sbt_load_dither(manager);
    if texture != 0 {
        c.texture_alpha_modulate(texture);
        c.texture_color_modulate(texture);
        c.texture_min_filter(texture, 1);
        c.texture_mag_filter(texture, 1);
        c.texture_mip_point(texture);
        c.texture_mip_bias(texture, -0.75);
        c.texture_clamp(texture);
        c.sbt_dither_new(button);
        c.sbt_dither_texture(button, texture, manager);
        let height = c.sbt_texture_height(texture);
        let width = c.sbt_texture_width(texture);
        c.sbt_dither_stretch(button, rectangle, width, height, manager);
    }
}

/// The destructor's body.
pub(crate) fn destroy(c: &mut impl Cpp, button: usize) {
    c.sbt_icon_dereference(button);
    c.sbt_queue_delete(button);
    c.sbt_dither_dereference(button);
    release(c, button);
}

/// `ReleaseButton`: the button object released.
pub(crate) fn release(c: &mut impl Cpp, button: usize) {
    if c.sbt_object(button) != 0 {
        c.sbt_object_release(button);
        c.sbt_object_clear(button);
    }
}

/// `DrawPolygons`: the icon, dithered over the last drawn rectangle when unavailable, under the button.
pub(crate) fn draw_polygons(c: &mut impl Cpp, button: usize) -> i64 {
    if c.sbt_draw_icon(button) {
        c.sbt_icon_white(button);
        c.sbt_draw_icon_bitmap(button);
        if !c.sbt_available(button) {
            c.sbt_dither_at_drawn(button);
            c.sbt_draw_dither(button);
        }
    }
    c.sbt_base_draw_polygons(button)
}

/// `DrawTextA`: the queued count over the button.
pub(crate) fn draw_text(c: &mut impl Cpp, button: usize) -> i64 {
    if c.sbt_show_count(button) {
        c.sbt_draw_count(button);
    }
    c.sbt_base_draw_text(button)
}

/// `HotKeyUp`: a pressed button's release.
pub(crate) fn hot_key_up(c: &mut impl Cpp, button: usize) {
    if c.sbt_pressed(button) {
        c.sbt_up(button);
    }
}

/// `SetCommandButton`: a command button showing the button object `index`.
pub(crate) fn set_command(c: &mut impl Cpp, button: usize, index: u32) {
    release(c, button);
    c.sbt_set_count(button, 0);
    c.sbt_set_command(button, true);
    c.sbt_set_button(button, index);
    c.sbt_set_object(button, index);
    c.sbt_object_acquire(button);
}

/// `SetTechButton`: a tech button for a unit's technology, showing the tech's button.
pub(crate) fn set_tech(c: &mut impl Cpp, button: usize, technology: i64, kind: u32, id: u32) {
    release(c, button);
    c.sbt_set_command(button, false);
    c.sbt_set_count(button, 0);
    c.sbt_set_technology(button, technology);
    c.sbt_set_unit_type(button, kind);
    c.sbt_set_unit_id(button, id);
    c.sbt_set_node_object(button, technology);
    c.sbt_object_acquire(button);
}

/// `SetLocation`: the button, its icon and its queue count text (3, 1 to the right and down) moved.
pub(crate) fn set_location(c: &mut impl Cpp, button: usize, x: f32, y: f32) {
    c.sbt_icon_at(button, x, y);
    c.sbt_queue_at(button, x + 3.0, y + 1.0);
    c.sbt_base_set_location(button, x, y);
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUIFMGStatusButton`'s constructor's body (`rectangle` the `GERectangle*`, `table` the
    /// `UIControlsTable*`).
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_construct(button: usize, manager: usize, rectangle: usize, table: usize) {
        super::construct(&mut Ffi, button, manager, rectangle, table)
    }
    /// `~EEUIFMGStatusButton`'s body.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_destroy(button: usize) {
        super::destroy(&mut Ffi, button)
    }
    /// `EEUIFMGStatusButton::ReleaseButton`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_release(button: usize) {
        super::release(&mut Ffi, button)
    }
    /// `EEUIFMGStatusButton::DrawPolygons`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_draw_polygons(button: usize) -> i64 {
        super::draw_polygons(&mut Ffi, button)
    }
    /// `EEUIFMGStatusButton::DrawTextA`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_draw_text(button: usize) -> i64 {
        super::draw_text(&mut Ffi, button)
    }
    /// `EEUIFMGStatusButton::HotKeyUp`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_hot_key_up(button: usize) {
        super::hot_key_up(&mut Ffi, button)
    }
    /// `EEUIFMGStatusButton::SetCommandButton`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_set_command(button: usize, index: u32) {
        super::set_command(&mut Ffi, button, index)
    }
    /// `EEUIFMGStatusButton::SetTechButton`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_set_tech(button: usize, technology: i64, kind: u32, id: u32) {
        super::set_tech(&mut Ffi, button, technology, kind, id)
    }
    /// `EEUIFMGStatusButton::SetLocation`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_set_location(button: usize, x: f32, y: f32) {
        super::set_location(&mut Ffi, button, x, y)
    }
    /// `EEUIManager::Fn26FA6A` (`set`) / `Fn26FA55`.
    #[no_mangle]
    pub extern "C" fn ee_screens_sbt_main_group_refill(set: bool) {
        super::main_group_refill(&mut Ffi, set)
    }
}

#[cfg(test)]
#[path = "status_button_calls_tests.rs"]
mod tests;
