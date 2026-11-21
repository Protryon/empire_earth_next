//! The improvement button's host cases (`EEUIBUpgrade::HostOp`, `User Interface Thread/EEUIBUpgrade.cpp`) that were
//! more than one C++ statement, in Rust over one-statement shims (UI lane round 9; the shims are
//! `ee_screenscpp_up_*`). `call` is the host's call record (`EEUIBUpgradeCall*`); `upgrade_ffi.rs` asks [`call`]
//! before the C++ host. (The base class calls, `UIButton::` and `UIControl::`, stay the member's.)

use crate::screens::game_cpp::Cpp;
use crate::screens::upgrade::{
    EE_SCREENS_UP_FIELD_ATTRIBUTE, EE_SCREENS_UP_FIELD_DISABLED, EE_SCREENS_UP_FIELD_EDITOR,
    EE_SCREENS_UP_FIELD_LAST_MESSAGE, EE_SCREENS_UP_FIELD_LEVEL, EE_SCREENS_UP_OP_BAR_FIELD, EE_SCREENS_UP_OP_BAR_ICON,
    EE_SCREENS_UP_OP_BAR_INSIDE, EE_SCREENS_UP_OP_BG_OPACITY, EE_SCREENS_UP_OP_BORDER_OPACITY,
    EE_SCREENS_UP_OP_COST_FACTOR, EE_SCREENS_UP_OP_EDITOR_COMPLEX, EE_SCREENS_UP_OP_FIELD, EE_SCREENS_UP_OP_HELP_LABEL,
    EE_SCREENS_UP_OP_NODE, EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_OP_SOUND, EE_SCREENS_UP_OP_STEP,
    EE_SCREENS_UP_OP_TEXT_COST,
};

/// The technology of a unit type without a class (`EE_SCREENS_UP_OP_NODE`'s: no node).
const UP_NO_TECHNOLOGY: i64 = 100000;

/// The case `op` (its arguments a..c), `None` for one that is not here.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, call: usize, op: u32, a: i64, b: i64, cc: i64) -> Option<i64> {
    let float = |v: i64| f32::from_bits(v as u32);
    Some(match op {
        EE_SCREENS_UP_OP_FIELD => match a {
            EE_SCREENS_UP_FIELD_EDITOR => c.up_editor(call) as i64,
            EE_SCREENS_UP_FIELD_DISABLED => c.up_disabled(call) as i64,
            EE_SCREENS_UP_FIELD_ATTRIBUTE => c.up_attribute(call),
            EE_SCREENS_UP_FIELD_LEVEL => c.up_level(call),
            EE_SCREENS_UP_FIELD_LAST_MESSAGE => c.up_last_message(call),
            _ => return None,
        },
        EE_SCREENS_UP_OP_SET_FIELD => {
            match a {
                EE_SCREENS_UP_FIELD_DISABLED => c.up_set_disabled(call, b != 0),
                EE_SCREENS_UP_FIELD_ATTRIBUTE => c.up_set_attribute(call, b),
                EE_SCREENS_UP_FIELD_LEVEL => c.up_set_level(call, b),
                EE_SCREENS_UP_FIELD_LAST_MESSAGE => c.up_set_last_message(call, b),
                _ => return None,
            }
            0
        }
        EE_SCREENS_UP_OP_EDITOR_COMPLEX => c.up_editor_complex(call) as i64,
        EE_SCREENS_UP_OP_STEP => c.up_step(call, a as usize).to_bits() as i64,
        EE_SCREENS_UP_OP_COST_FACTOR => c.up_cost_factor(call, a as usize).to_bits() as i64,
        EE_SCREENS_UP_OP_NODE => {
            // the current player's node of the unit's class's technology
            let unit_type = c.up_unit_type(a as usize);
            let technology =
                if c.up_class_exists(unit_type) { c.up_class_technology(unit_type) } else { UP_NO_TECHNOLOGY };
            c.up_node(technology) as i64
        }
        EE_SCREENS_UP_OP_TEXT_COST => {
            c.up_text_cost(call, a as usize, b as usize, float(cc));
            0
        }
        EE_SCREENS_UP_OP_SOUND => {
            // the first button sound for a == 1, else the second
            c.up_sound((a == 1) as u32);
            0
        }
        EE_SCREENS_UP_OP_HELP_LABEL => {
            let label = c.up_help_label(call);
            if label != 0 {
                c.up_help_label_set(label, call);
            }
            0
        }
        EE_SCREENS_UP_OP_BG_OPACITY => {
            c.up_bg_opacity(call, float(a));
            0
        }
        EE_SCREENS_UP_OP_BORDER_OPACITY => {
            c.up_border_opacity(call, float(a));
            0
        }
        EE_SCREENS_UP_OP_BAR_FIELD => match a {
            0 => c.up_bar_enabled(call) as i64,
            1 => c.up_bar_pressed(call) as i64,
            _ => c.up_has_capture(call) as i64,
        },
        EE_SCREENS_UP_OP_BAR_INSIDE => {
            (if a == 0 { c.up_inside_form_mouse(call) } else { c.up_inside_location(call) }) as i64
        }
        EE_SCREENS_UP_OP_BAR_ICON => {
            // the icon moved by a pixels right and down (the press's offset)
            let (x, y) = (c.up_icon_left(call), c.up_icon_top(call));
            c.up_icon_set_location(call, x + a as f32, y + a as f32);
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    const CALL: usize = 0x20;

    #[test]
    fn the_fields_read_and_set() {
        let mut f = Fake::default();
        f.answer("up_last_message", &[-1]).answer("up_editor", &[1]);
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_FIELD, EE_SCREENS_UP_FIELD_LAST_MESSAGE, 0, 0), Some(-1));
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_FIELD, EE_SCREENS_UP_FIELD_EDITOR, 0, 0), Some(1));
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_FIELD_DISABLED, 2, 0), Some(0));
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_FIELD_LEVEL, 3, 0), Some(0));
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_FIELD, 9, 0, 0), None);
        assert_eq!(f.calls(), "up_last_message(32)\nup_editor(32)\nup_set_disabled(32, true)\nup_set_level(32, 3)");
    }

    #[test]
    fn the_node_falls_back_to_no_technology() {
        let mut f = Fake::default();
        f.answer("up_unit_type", &[7]).answer("up_node", &[0x40]);
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_NODE, 0x10, 0, 0), Some(0x40));
        assert_eq!(f.calls(), "up_unit_type(16)\nup_class_exists(7)\nup_node(100000)");
        let mut f = Fake::default();
        f.answer("up_class_exists", &[1]).answer("up_class_technology", &[55]);
        call(&mut f, CALL, EE_SCREENS_UP_OP_NODE, 0x10, 0, 0);
        assert!(f.calls().ends_with("up_class_technology(0)\nup_node(55)"));
    }

    #[test]
    fn the_floats_cross_as_bits_and_the_icon_moves_by_the_offset() {
        let mut f = Fake::default();
        f.answer("up_step", &[2]).answer("up_icon_left", &[10]).answer("up_icon_top", &[20]);
        assert_eq!(call(&mut f, CALL, EE_SCREENS_UP_OP_STEP, 0x10, 0, 0), Some(2.0f32.to_bits() as i64));
        call(&mut f, CALL, EE_SCREENS_UP_OP_BG_OPACITY, 0.5f32.to_bits() as i64, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_BAR_ICON, 1, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_TEXT_COST, 3, 4, 1.5f32.to_bits() as i64);
        assert!(f.calls().ends_with(
            "up_bg_opacity(32, 0.5)\nup_icon_left(32)\nup_icon_top(32)\nup_icon_set_location(32, 11.0, 21.0)\n\
             up_text_cost(32, 3, 4, 1.5)"
        ));
    }

    #[test]
    fn the_rest_are_one_call_or_a_choice() {
        let mut f = Fake::default();
        f.answer("up_help_label", &[0, 0x50]);
        call(&mut f, CALL, EE_SCREENS_UP_OP_HELP_LABEL, 0, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_HELP_LABEL, 0, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_SOUND, 1, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_BAR_FIELD, 2, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_BAR_INSIDE, 1, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_EDITOR_COMPLEX, 0, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_COST_FACTOR, 5, 0, 0);
        call(&mut f, CALL, EE_SCREENS_UP_OP_BORDER_OPACITY, 0, 0, 0);
        assert_eq!(
            f.calls(),
            "up_help_label(32)\nup_help_label(32)\nup_help_label_set(80, 32)\nup_sound(1)\nup_has_capture(32)\n\
             up_inside_location(32)\nup_editor_complex(32)\nup_cost_factor(32, 5)\nup_border_opacity(32, 0.0)"
        );
        assert_eq!(call(&mut f, CALL, 0xffff, 0, 0, 0), None);
    }
}
