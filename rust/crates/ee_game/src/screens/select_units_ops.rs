//! `EEUICSelectUnits::RustOp` whole in Rust over one-statement shims (UI lane round 16): the picker, the lasso's
//! selections, the click action, the point under the mouse (the trigger tab's) and the base's left press.

use crate::screens::game_cpp::Cpp;
use crate::screens::select_units::*;

/// The select control's op `op` on `ctl` (its `EEUIRustControl*`) with the op's floats `f`; `None` for another host's.
pub(crate) fn op<C: Cpp + ?Sized>(c: &mut C, ctl: usize, op: u32, a: i64, b: i64, f: &mut [f32]) -> Option<i64> {
    Some(match op {
        EE_SCREENS_SU_PICK => c.su_pick() as i64,
        EE_SCREENS_SU_LASSO_TYPE => {
            c.su_lasso_type(ctl, a as usize, b);
            0
        }
        EE_SCREENS_SU_DEREF => {
            c.su_unit_release(a as usize);
            0
        }
        EE_SCREENS_SU_LASSO_BOX => {
            c.su_lasso_box(ctl, f[0], f[1], f[2], f[3], a);
            0
        }
        EE_SCREENS_SU_CLICK => {
            c.su_click(ctl, a as u32);
            0
        }
        // the trigger tab's: the unit or tile point under the mouse (x, y into f)
        EE_SCREENS_SU_POINT_AT => {
            c.uh_point_reset();
            c.su_point_at(ctl);
            f[0] = c.uh_point_x();
            f[1] = c.uh_point_y();
            0
        }
        EE_SCREENS_SU_LASSO_IN_RECT => {
            c.su_lasso_in_rect(ctl, f[0], f[1], f[2], f[3], a);
            0
        }
        EE_SCREENS_SU_CLICK_EXECUTE => {
            c.su_click_execute(ctl, f[0], f[1]);
            0
        }
        EE_SCREENS_SU_L_DOWN => c.su_left_down(ctl),
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
        fake.answer("su_pick", &[40]).answer("uh_point_x", &[3]).answer("uh_point_y", &[4]);
        let mut f = [1.0f32, 2.0, 3.0, 4.0];
        assert_eq!(op(&mut fake, 9, EE_SCREENS_SU_PICK, 0, 0, &mut f), Some(40));
        assert_eq!(op(&mut fake, 9, EE_SCREENS_SU_LASSO_BOX, 1, 0, &mut f), Some(0));
        assert_eq!(op(&mut fake, 9, EE_SCREENS_SU_POINT_AT, 0, 0, &mut f), Some(0));
        assert_eq!((f[0], f[1]), (3.0, 4.0));
        assert_eq!(op(&mut fake, 9, 999, 0, 0, &mut f), None);
        assert_eq!(
            fake.calls(),
            "su_pick()\nsu_lasso_box(9, 1.0, 2.0, 3.0, 4.0, 1)\nuh_point_reset()\nsu_point_at(9)\nuh_point_x()\nuh_point_y()"
        );
    }
}
