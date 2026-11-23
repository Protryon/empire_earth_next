//! The unit hack's host's cases of more than one call (`ee_screenscpp_unit_hack`, `EEUIFMainGame.cpp`) in Rust over
//! one-statement shims (UI lane round 10): the class's flags and name, the object list's append, first selection
//! (without its change event) and selection, the lists' selections, the keys, the place message, the point under
//! the mouse; and from UI lane round 16 the host's one-statement cases too (the C++ host switch is gone).

use core::ffi::c_ulong;

use crate::screens::game_cpp::{
    Cpp, EeScreensShimOut as Out, EeScreensShimText as Text, EeScreensShimULongOut as ULongOut,
};
use crate::screens::unit_hack::{
    EE_SCREENS_UH_OP_CLASS, EE_SCREENS_UH_OP_CLASS_CATEGORY, EE_SCREENS_UH_OP_CLASS_COUNT, EE_SCREENS_UH_OP_CLASS_NAME,
    EE_SCREENS_UH_OP_CLASS_TYPE, EE_SCREENS_UH_OP_COMBO_APPEND, EE_SCREENS_UH_OP_COMBO_CLEAR,
    EE_SCREENS_UH_OP_COMBO_SELECTION, EE_SCREENS_UH_OP_COMBO_SELECT_FIRST, EE_SCREENS_UH_OP_IN_VIEW,
    EE_SCREENS_UH_OP_KEY, EE_SCREENS_UH_OP_LEFT_BUTTON, EE_SCREENS_UH_OP_LIST_SELECTION, EE_SCREENS_UH_OP_LOCAL_PLAYER,
    EE_SCREENS_UH_OP_PLAYER_SKIPPED, EE_SCREENS_UH_OP_POINT, EE_SCREENS_UH_OP_SEND_LOAD, EE_SCREENS_UH_OP_SEND_PLACE,
    EE_SCREENS_UH_OP_TILE,
};

/// The host's moved op `op` on the form; `None` for the C++ host's.
#[allow(clippy::too_many_arguments)]
pub(crate) fn call<C: Cpp + ?Sized>(
    c: &mut C,
    form: usize,
    op: u32,
    a: i32,
    b: i32,
    f: &mut [f32; 2],
    out: &mut u32,
    text: &mut [u8],
) -> Option<i32> {
    Some(match op {
        EE_SCREENS_UH_OP_CLASS => {
            if !c.uh_class_exists(a) {
                return Some(0);
            }
            1 | if c.uh_class_enabled(a) { 2 } else { 0 } | if c.uh_class_placeable(a) { 4 } else { 0 }
        }
        EE_SCREENS_UH_OP_CLASS_NAME => {
            let mut name = Vec::new();
            c.uh_class_name(a, Out::of(&mut name));
            if name.len() <= text.len() {
                text[..name.len()].copy_from_slice(&name);
            }
            name.len() as i32
        }
        EE_SCREENS_UH_OP_COMBO_APPEND => {
            c.uh_combo_append(form, Text::of(text));
            0
        }
        EE_SCREENS_UH_OP_COMBO_SELECT_FIRST => {
            // port: the first line is selected without the box's change event (round 7's note in the C++ of old: the
            // original's event raced the game's start)
            let event = c.uh_combo_trigger(form);
            if event != 0 {
                c.event_reference(event);
                c.uh_combo_set_trigger(form, 0);
            }
            c.uh_combo_select_first(form);
            if event != 0 {
                c.uh_combo_set_trigger(form, event);
                c.event_dereference(event);
            }
            0
        }
        EE_SCREENS_UH_OP_COMBO_SELECTION => {
            let mut selection: c_ulong = 0;
            c.uh_combo_selection(form, ULongOut::of(&mut selection));
            selection as i32
        }
        EE_SCREENS_UH_OP_LIST_SELECTION => {
            let mut selection = *out as c_ulong;
            let result = if a == 1 {
                c.uh_object_list_selection(form, ULongOut::of(&mut selection))
            } else {
                c.uh_player_list_selection(form, ULongOut::of(&mut selection))
            };
            *out = selection as u32;
            result
        }
        EE_SCREENS_UH_OP_KEY => {
            if a == 0 {
                c.key_shift_value()
            } else {
                c.uh_control_value()
            }
        }
        EE_SCREENS_UH_OP_SEND_PLACE => {
            c.uh_send_place(a, f[0], f[1], b);
            0
        }
        EE_SCREENS_UH_OP_POINT => {
            c.uh_point_reset();
            let found = c.uh_point(f[0], f[1]);
            f[0] = c.uh_point_x();
            f[1] = c.uh_point_y();
            found as i32
        }
        EE_SCREENS_UH_OP_CLASS_COUNT => c.uh_class_count(),
        EE_SCREENS_UH_OP_CLASS_CATEGORY => c.uh_class_category(a),
        EE_SCREENS_UH_OP_CLASS_TYPE => c.uh_class_type(a),
        EE_SCREENS_UH_OP_COMBO_CLEAR => {
            c.uh_combo_clear(form);
            0
        }
        EE_SCREENS_UH_OP_TILE => c.uh_tile(f[0], f[1]) as i32,
        EE_SCREENS_UH_OP_PLAYER_SKIPPED => c.uh_player_skipped(a) as i32,
        EE_SCREENS_UH_OP_LOCAL_PLAYER => c.uh_local_player(),
        EE_SCREENS_UH_OP_SEND_LOAD => {
            c.uh_send_load(a);
            0
        }
        EE_SCREENS_UH_OP_IN_VIEW => c.uh_in_view(f[0], f[1]) as i32,
        EE_SCREENS_UH_OP_LEFT_BUTTON => {
            (if a == 0 { c.mouse_left_just_pressed() } else { c.mouse_left_just_unpressed() }) as i32
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn run(fake: &mut Fake, op: u32, a: i32, b: i32, f: &mut [f32; 2], out: &mut u32, text: &mut [u8]) -> Option<i32> {
        call(fake, 0x100, op, a, b, f, out, text)
    }

    #[test]
    fn class_flags_and_name() {
        let mut fake = Fake::default();
        fake.answer("uh_class_exists", &[0, 1]).answer("uh_class_placeable", &[1]);
        fake.text("uh_class_name", b"Archer").text("uh_class_name", b"Archer");
        let (mut f, mut out, mut text) = ([0.0; 2], 0, [0u8; 4]);
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_CLASS, 3, 0, &mut f, &mut out, &mut text), Some(0));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_CLASS, 3, 0, &mut f, &mut out, &mut text), Some(5));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_CLASS_NAME, 3, 0, &mut f, &mut out, &mut text), Some(6));
        assert_eq!(text, [0; 4]);
        let mut long = [0u8; 8];
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_CLASS_NAME, 3, 0, &mut f, &mut out, &mut long), Some(6));
        assert_eq!(&long[..6], b"Archer");
    }

    #[test]
    fn first_selection_goes_without_the_change_event() {
        let mut fake = Fake::default();
        fake.answer("uh_combo_trigger", &[0x40]);
        let (mut f, mut out, mut text) = ([0.0; 2], 0, [0u8; 0]);
        run(&mut fake, EE_SCREENS_UH_OP_COMBO_SELECT_FIRST, 0, 0, &mut f, &mut out, &mut text);
        assert_eq!(
            fake.calls(),
            "uh_combo_trigger(256)\nevent_reference(64)\nuh_combo_set_trigger(256, 0)\nuh_combo_select_first(256)\n\
             uh_combo_set_trigger(256, 64)\nevent_dereference(64)"
        );
        let mut fake = Fake::default();
        run(&mut fake, EE_SCREENS_UH_OP_COMBO_SELECT_FIRST, 0, 0, &mut f, &mut out, &mut text);
        assert_eq!(fake.calls(), "uh_combo_trigger(256)\nuh_combo_select_first(256)");
    }

    #[test]
    fn selections_keys_place_and_point() {
        let mut fake = Fake::default();
        fake.out("uh_combo_selection", 4).out("uh_player_list_selection", 2).answer("uh_point", &[1]);
        fake.answer("uh_point_x", &[7]).answer("uh_point_y", &[8]);
        let (mut f, mut out, mut text) = ([1.5, 2.5], 9, *b"ab");
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_COMBO_SELECTION, 0, 0, &mut f, &mut out, &mut text), Some(4));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_LIST_SELECTION, 2, 0, &mut f, &mut out, &mut text), Some(0));
        assert_eq!(out, 2);
        run(&mut fake, EE_SCREENS_UH_OP_LIST_SELECTION, 1, 0, &mut f, &mut out, &mut text);
        run(&mut fake, EE_SCREENS_UH_OP_KEY, 0, 0, &mut f, &mut out, &mut text);
        run(&mut fake, EE_SCREENS_UH_OP_KEY, 1, 0, &mut f, &mut out, &mut text);
        run(&mut fake, EE_SCREENS_UH_OP_COMBO_APPEND, 0, 0, &mut f, &mut out, &mut text);
        run(&mut fake, EE_SCREENS_UH_OP_SEND_PLACE, 12, 3, &mut f, &mut out, &mut text);
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_POINT, 0, 0, &mut f, &mut out, &mut text), Some(1));
        assert_eq!(f, [7.0, 8.0]);
        assert_eq!(
            fake.calls(),
            "uh_combo_selection(256, out)\nuh_player_list_selection(256, out)\nuh_object_list_selection(256, out)\n\
             key_shift_value()\nuh_control_value()\nuh_combo_append(256, \"ab\")\nuh_send_place(12, 1.5, 2.5, 3)\n\
             uh_point_reset()\nuh_point(1.5, 2.5)\nuh_point_x()\nuh_point_y()"
        );
        assert_eq!(run(&mut fake, 999, 0, 0, &mut f, &mut out, &mut text), None);
    }

    #[test]
    fn the_one_call_ops() {
        let mut fake = Fake::default();
        fake.answer("uh_class_count", &[40]).answer("uh_tile", &[1]).answer("mouse_left_just_unpressed", &[1]);
        let (mut f, mut out, mut text) = ([3.5, 4.5], 0, [0u8; 0]);
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_CLASS_COUNT, 0, 0, &mut f, &mut out, &mut text), Some(40));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_TILE, 0, 0, &mut f, &mut out, &mut text), Some(1));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_SEND_LOAD, 7, 0, &mut f, &mut out, &mut text), Some(0));
        assert_eq!(run(&mut fake, EE_SCREENS_UH_OP_LEFT_BUTTON, 1, 0, &mut f, &mut out, &mut text), Some(1));
        assert_eq!(
            fake.calls(),
            "uh_class_count()
uh_tile(3.5, 4.5)
uh_send_load(7)
mouse_left_just_unpressed()"
        );
    }
}
