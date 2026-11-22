//! `EEUICPortrait.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 11): the class's own ops (its
//! `RustOp`, gone: the unit's handle and button, the bar state, the nodes' and improvements' reads, the level text,
//! the tooltip's texts, the click's sound), its constructor's and destructor's bodies, `DrawTextA` and `MouseMove`.
//! `portrait_ffi.rs`' host asks [`op`] before the C++ host.

use crate::screens::game_cpp::{Cpp, EeScreensShimF32Out, EeScreensShimText};
use crate::screens::portrait::*;

/// The class op `op` (`EE_SCREENS_PT_OP_*`, without `EE_SCREENS_HC_CLASS`) on the portrait `p`; `None` for one of
/// another host. `a` is a unit or node handle (an address) for the ops on one; `f` the op's floats.
pub(crate) fn op<C: Cpp + ?Sized>(c: &mut C, p: usize, op: u32, a: i64, b: i64, f: &[f32]) -> Option<i64> {
    let unit = a as usize;
    let node = a as usize;
    Some(match op {
        EE_SCREENS_PT_OP_HAS_UNIT => c.pt_has_unit(p) as i64,
        EE_SCREENS_PT_OP_RELEASE => {
            c.pt_button_release(p);
            c.pt_unit_release(p);
            0
        }
        EE_SCREENS_PT_OP_SET_UNIT => {
            c.pt_set_unit(p, unit);
            0
        }
        EE_SCREENS_PT_OP_ACQUIRE => {
            c.pt_unit_acquire(p);
            c.pt_button_take(p);
            c.pt_button_acquire(p);
            0
        }
        EE_SCREENS_PT_OP_FACE => {
            c.pt_face_texture(p);
            c.pt_face_bitmap(p);
            0
        }
        EE_SCREENS_PT_OP_STATE => {
            let level = c.pt_level(p) as i64;
            let first = c.pt_first_bar(p) as i64;
            let second = c.pt_second_bar(p) as i64;
            level | first << 32 | second << 33
        }
        EE_SCREENS_PT_OP_SET_STATE => {
            c.pt_set_level(p, (a & 0xffff_ffff) as u64);
            c.pt_set_first_bar(p, (a >> 32 & 1) != 0);
            c.pt_set_second_bar(p, (a >> 33 & 1) != 0);
            0
        }
        EE_SCREENS_PT_OP_COMPLEX => c.pt_unit_complex(p) as i64,
        EE_SCREENS_PT_OP_RESEARCHED => c.pt_researched(unit),
        EE_SCREENS_PT_OP_NODE => c.pt_node(a) as i64,
        EE_SCREENS_PT_OP_SCALE => {
            if !c.pt_node_makes_unit(node) {
                return Some(-1);
            }
            let upgrade = c.pt_node_upgrade(p, node);
            if upgrade == 0 {
                return Some(-1);
            }
            c.pt_upgrade_build_scale(upgrade).to_bits() as i64
        }
        EE_SCREENS_PT_OP_BUILD_TIME => c.pt_node_build_time(node),
        EE_SCREENS_PT_OP_IMPROVEMENT_LEVEL => c.pt_improvement_level(unit, b),
        EE_SCREENS_PT_OP_IMPROVEMENT_TIME => {
            if a == 1 {
                c.pt_improvement_time(b)
            } else {
                c.pt_improvement_time2(b)
            }
        }
        EE_SCREENS_PT_OP_PROGRESS => c.pt_progress(unit).to_bits() as i64,
        EE_SCREENS_PT_OP_TOTAL_COST => c.pt_total_cost(unit),
        EE_SCREENS_PT_OP_POINTS_MAX => c.pt_points_max(),
        // a: the text's address, b: its length
        EE_SCREENS_PT_OP_TEXT => {
            c.pt_text(p, EeScreensShimText { ptr: a as usize as *const u8, len: b as usize });
            0
        }
        EE_SCREENS_PT_OP_UPDATE_HELP => {
            c.pt_update_help(p);
            0
        }
        EE_SCREENS_PT_OP_CONTAINS => c.pt_contains(p, f[0], f[1], f[2], f[3]) as i64,
        EE_SCREENS_PT_OP_BUILDING => c.pt_unit_building(p) as i64,
        EE_SCREENS_PT_OP_HELP_RESEARCH => {
            c.pt_strings_clear();
            c.pt_name_clear();
            c.pt_format_research();
            if c.pt_node_makes_unit(node) {
                c.pt_name_unit_class(node);
            } else {
                c.pt_name_node(node);
            }
            c.pt_help_print();
            0
        }
        EE_SCREENS_PT_OP_HELP_IMPROVEMENT => {
            c.pt_strings_clear();
            c.pt_name_clear();
            c.pt_format_improvement();
            c.pt_name_improvement(p);
            c.pt_help_print();
            0
        }
        EE_SCREENS_PT_OP_ALIVE => (c.pt_has_unit(p) && c.pt_unit_hp_positive(p) && c.pt_unit_on_tile(p)) as i64,
        EE_SCREENS_PT_OP_COMPLEX_FIELD => c.pt_unit_complex_field(p) as i64,
        EE_SCREENS_PT_OP_SOUND => {
            c.pt_sound();
            0
        }
        _ => return None,
    })
}

/// The constructor's body: the portrait bitmap at the top left, the bars (`hud_draw.rs`), no bars shown, no unit,
/// level 0, the level text right aligned at the top right in a font half the rectangle's height.
pub(crate) fn init<C: Cpp + ?Sized>(c: &mut C, p: usize, rect: [f32; 4], show_level: bool) {
    let [left, top, right, bottom] = rect;
    c.pt_new_bitmap(p);
    c.pt_bitmap_location(p, left, top);
    let mut floats = rect;
    c.pt_draw_init(p, EeScreensShimF32Out(floats.as_mut_ptr()));
    c.pt_set_first_bar(p, false);
    c.pt_set_second_bar(p, false);
    c.pt_set_unit(p, 0);
    c.pt_set_level(p, 0);
    c.pt_set_show_level(p, show_level);
    c.pt_new_text(p, (bottom - top) * 0.5);
    c.pt_text_align_right(p);
    c.pt_text_transparent(p);
    c.pt_text_color(p);
    c.pt_text_location(p, right, top);
}

/// The destructor's body: the level text deleted, the unit and its button released, the bitmap released.
pub(crate) fn free<C: Cpp + ?Sized>(c: &mut C, p: usize) {
    c.pt_delete_text(p);
    if c.pt_has_unit(p) {
        c.pt_button_release(p);
        c.pt_unit_release(p);
    }
    c.pt_bitmap_release(p);
}

/// `DrawTextA`: the base's draw count, then the improvement level when above 0.
pub(crate) fn draw_text<C: Cpp + ?Sized>(c: &mut C, p: usize) -> i64 {
    c.pt_draw_text_base(p);
    if c.pt_level(p) > 0 {
        c.pt_draw_level(p) as i64
    } else {
        0
    }
}

/// `MouseMove`: the help text updated over a portrait showing both bars.
pub(crate) fn mouse_move<C: Cpp + ?Sized>(c: &mut C, p: usize) -> i64 {
    if c.pt_first_bar(p) && c.pt_second_bar(p) {
        c.pt_update_help(p);
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_state_packs_the_level_and_the_bars() {
        let mut fake = Fake::default();
        fake.answer("pt_level", &[3]).answer("pt_second_bar", &[1]);
        assert_eq!(op(&mut fake, 1, EE_SCREENS_PT_OP_STATE, 0, 0, &[]), Some(3 | 1 << 33));
        op(&mut fake, 1, EE_SCREENS_PT_OP_SET_STATE, 5 | 1 << 32, 0, &[]);
        assert!(fake.calls().ends_with("pt_set_level(1, 5)\npt_set_first_bar(1, true)\npt_set_second_bar(1, false)"));
    }

    #[test]
    fn the_scale_is_minus_1_without_a_unit_upgrade() {
        let mut fake = Fake::default();
        fake.answer("pt_node_makes_unit", &[1]);
        assert_eq!(op(&mut fake, 1, EE_SCREENS_PT_OP_SCALE, 9, 0, &[]), Some(-1));
        fake.answer("pt_node_makes_unit", &[1]).answer("pt_node_upgrade", &[4]).answer("pt_upgrade_build_scale", &[2]);
        assert_eq!(op(&mut fake, 1, EE_SCREENS_PT_OP_SCALE, 9, 0, &[]), Some(2.0f32.to_bits() as i64));
        let mut fake = Fake::default();
        assert_eq!(op(&mut fake, 1, EE_SCREENS_PT_OP_SCALE, 9, 0, &[]), Some(-1));
        assert_eq!(fake.calls(), "pt_node_makes_unit(9)");
    }

    #[test]
    fn the_research_help_names_the_unit_class_or_the_node() {
        let mut fake = Fake::default();
        op(&mut fake, 1, EE_SCREENS_PT_OP_HELP_RESEARCH, 9, 0, &[]);
        assert_eq!(
            fake.calls(),
            "pt_strings_clear()\npt_name_clear()\npt_format_research()\npt_node_makes_unit(9)\npt_name_node(9)\n\
             pt_help_print()"
        );
    }

    #[test]
    fn alive_stops_at_the_first_no() {
        let mut fake = Fake::default();
        fake.answer("pt_has_unit", &[1]);
        assert_eq!(op(&mut fake, 1, EE_SCREENS_PT_OP_ALIVE, 0, 0, &[]), Some(0));
        assert_eq!(fake.calls(), "pt_has_unit(1)\npt_unit_hp_positive(1)");
        assert_eq!(op(&mut fake, 1, 999, 0, 0, &[]), None);
    }

    #[test]
    fn the_constructor_and_destructor_bodies() {
        let mut fake = Fake::default();
        init(&mut fake, 1, [10.0, 20.0, 50.0, 60.0], true);
        assert!(fake.calls().contains("pt_bitmap_location(1, 10.0, 20.0)"));
        assert!(fake.calls().ends_with(
            "pt_new_text(1, 20.0)\npt_text_align_right(1)\npt_text_transparent(1)\n\
             pt_text_color(1)\npt_text_location(1, 50.0, 20.0)"
        ));
        let mut fake = Fake::default();
        free(&mut fake, 1);
        assert_eq!(fake.calls(), "pt_delete_text(1)\npt_has_unit(1)\npt_bitmap_release(1)");
    }

    #[test]
    fn the_level_draws_above_0() {
        let mut fake = Fake::default();
        assert_eq!(draw_text(&mut fake, 1), 0);
        fake.answer("pt_level", &[2]).answer("pt_draw_level", &[7]);
        assert_eq!(draw_text(&mut fake, 1), 7);
    }

    #[test]
    fn the_help_updates_over_both_bars() {
        let mut fake = Fake::default();
        fake.answer("pt_first_bar", &[1, 1]).answer("pt_second_bar", &[0, 1]);
        mouse_move(&mut fake, 1);
        mouse_move(&mut fake, 1);
        assert!(fake.calls().ends_with("pt_second_bar(1)\npt_update_help(1)"));
        assert_eq!(fake.log.len(), 5);
    }
}
