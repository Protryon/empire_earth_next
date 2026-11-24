//! The improvement button's host ops of one C++ statement each (`EEUIBUpgrade::HostOp`'s switch until UI lane round
//! 16; the shims `ee_screenscpp_upx_*`, `User Interface Thread/EEUIBUpgrade.cpp`): `call` is the host's call record
//! (`EEUIBUpgradeCall*`), a the unit (an `EEUnit*`) where the op takes one. `upgrade_ffi.rs` asks here after
//! `upgrade_calls.rs`.

use crate::screens::game_cpp::Cpp;
#[allow(clippy::wildcard_imports)]
use crate::screens::upgrade::*;

/// The op `op` (its arguments a, b), `None` for one that is not here.
pub(crate) fn op<C: Cpp + ?Sized>(c: &mut C, call: usize, op: u32, a: i64, b: i64) -> Option<i64> {
    Some(match op {
        EE_SCREENS_UP_OP_ALLOWED => c.upx_allowed(call, a, b),
        EE_SCREENS_UP_OP_CAN_BUY => c.upx_can_buy(call, a, b),
        EE_SCREENS_UP_OP_EDITOR_UNIT => c.upx_editor_unit(call, a, b),
        EE_SCREENS_UP_OP_CAN_RAISE => c.upx_can_raise(call, a, b),
        EE_SCREENS_UP_OP_MAXED => c.upx_maxed(call, a, b),
        EE_SCREENS_UP_OP_LEVEL => c.upx_level(call, a, b),
        EE_SCREENS_UP_OP_TEXT_NAME => {
            c.upx_text_name(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_TEXT_GAIN => {
            c.upx_text_gain(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_TEXT_STRING => {
            c.upx_text_string(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_OUT_APPEND => {
            c.upx_out_append(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_OUT_SET => {
            c.upx_out_set(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_OUT_BYTES => {
            c.upx_out_bytes(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_BASE_HELP => {
            c.upx_base_help(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_UPGRADE => {
            c.upx_upgrade(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_OUT_LENGTH => c.upx_out_length(call, a, b),
        EE_SCREENS_UP_OP_REFUSAL_INTERVAL => c.upx_refusal_interval(call, a, b),
        EE_SCREENS_UP_OP_TIME => c.upx_time(call, a, b),
        EE_SCREENS_UP_OP_CHAT => {
            c.upx_chat(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_UPDATE_HELP => {
            c.upx_update_help(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_PRESSED => c.upx_pressed(call, a, b),
        EE_SCREENS_UP_OP_BASE_BUTTON_UP => {
            c.upx_base_button_up(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_BASE_MOVE => c.upx_base_move(call, a, b),
        EE_SCREENS_UP_OP_HELP_TEXT => {
            c.upx_help_text(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_CONTAINS => c.upx_contains(call, a, b),
        EE_SCREENS_UP_OP_RELEASE_MOUSE => {
            c.upx_release_mouse(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_COST_DIFFERS => c.upx_cost_differs(call, a, b),
        EE_SCREENS_UP_OP_COST_SET => {
            c.upx_cost_set(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_DIRTY => {
            c.upx_dirty(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_BORDER_THICKNESS => {
            c.upx_border_thickness(call, a, b);
            0
        }
        EE_SCREENS_UP_OP_BAR_SET_PRESSED => {
            c.upx_bar_set_pressed(call, a, b);
            0
        }
        // the status bar's base presses: a 0 the left press, 1 the left release, else the button's right press
        EE_SCREENS_UP_OP_BAR_BASE => match a {
            0 => c.upx_bar_base_l_down(call),
            1 => c.upx_bar_base_l_up(call),
            _ => c.upx_bar_base_r_down(call),
        },
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_ops() {
        let mut fake = Fake::default();
        fake.answer("upx_allowed", &[1]).answer("upx_level", &[3]);
        assert_eq!(op(&mut fake, 8, EE_SCREENS_UP_OP_ALLOWED, 0, 0), Some(1));
        assert_eq!(op(&mut fake, 8, EE_SCREENS_UP_OP_LEVEL, 5, 0), Some(3));
        assert_eq!(op(&mut fake, 8, EE_SCREENS_UP_OP_DIRTY, 0, 0), Some(0));
        assert_eq!(op(&mut fake, 8, 0xffff, 0, 0), None);
        assert_eq!(fake.calls(), "upx_allowed(8, 0, 0)\nupx_level(8, 5, 0)\nupx_dirty(8, 0, 0)");
    }
}
