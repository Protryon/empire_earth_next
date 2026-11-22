//! `EEUIShellControl::RustOp` whole in Rust over one-statement shims (UI lane round 11): the icon bar kind's own
//! ops (a unit's class flags and goals, the selection message, the bar's sounds, the keys, the mouse, the buttons'
//! icons and the frame). `icon_bar_ffi.rs`' host asks [`op`] before the C++ host.

use crate::screens::game_cpp::Cpp;
use crate::screens::icon_bar::*;

/// The icon bar's op `op` (`EE_SCREENS_IB_OP_*`, without `EE_SCREENS_HC_CLASS`) on the bar `p` (its
/// `EEUIRustControl*`); `a` a unit or button handle, or the ids' address; `f` the op's floats. `None` for another
/// host's.
pub(crate) fn op<C: Cpp + ?Sized>(c: &mut C, p: usize, op: u32, a: i64, b: i64, f: &mut [f32]) -> Option<i64> {
    let handle = a as usize;
    Some(match op {
        EE_SCREENS_IB_OP_CLASS => {
            let transport = if c.ib_class_transport(handle) { EE_SCREENS_IB_CLASS_TRANSPORT } else { 0 };
            let air_base = if c.ib_class_air_base(handle) { EE_SCREENS_IB_CLASS_AIR_BASE } else { 0 };
            let bomber = if c.ib_class_paratrooper_bomber(handle) { EE_SCREENS_IB_CLASS_PARATROOPER_BOMBER } else { 0 };
            transport | air_base | bomber | c.ib_class_family(handle) << 8
        }
        EE_SCREENS_IB_OP_QUEUED_GOAL => c.ib_queued_goal(handle, b as u32 as u64) as i64,
        EE_SCREENS_IB_OP_NO_GOALS => (c.ib_goals_empty(handle) || c.ib_goal_front_none(handle)) as i64,
        // a: the ids (int32s), b: their count
        EE_SCREENS_IB_OP_SELECT_THESE => {
            let message = c.ib_select_these(handle, b as u64);
            c.ib_send(message);
            0
        }
        EE_SCREENS_IB_OP_SOUND => {
            c.ib_sound(p, if a == 0 { 2 } else { 3 });
            0
        }
        EE_SCREENS_IB_OP_CTRL => c.ib_ctrl() as i64,
        EE_SCREENS_IB_OP_SHIFT => c.ib_shift() as i64,
        EE_SCREENS_IB_OP_MOUSE => {
            f[0] = c.ib_mouse_x(p);
            0
        }
        EE_SCREENS_IB_OP_CONTAINS => c.ib_contains(p) as i64,
        EE_SCREENS_IB_OP_ACQUIRE => {
            c.ib_button_acquire(handle);
            0
        }
        EE_SCREENS_IB_OP_RELEASE => {
            c.ib_button_release(handle);
            0
        }
        // f: the icon's location, then the face's rectangle
        EE_SCREENS_IB_OP_ICON => {
            c.ib_icon_texture(p, handle);
            c.ib_icon_face(p, handle, f[2], f[3], f[4], f[5]);
            c.ib_slot_location(p, 0, f[0], f[1]);
            c.ib_slot_draw(p, 0);
            0
        }
        EE_SCREENS_IB_OP_FRAME => {
            c.ib_slot_location(p, 1, f[0], f[1]);
            c.ib_slot_draw(p, 1)
        }
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_class_flags_and_family() {
        let mut fake = Fake::default();
        fake.answer("ib_class_air_base", &[1]).answer("ib_class_family", &[3]);
        let r = op(&mut fake, 1, EE_SCREENS_IB_OP_CLASS, 5, 0, &mut []);
        assert_eq!(r, Some(EE_SCREENS_IB_CLASS_AIR_BASE | 3 << 8));
    }

    #[test]
    fn no_goals_stops_at_an_empty_queue() {
        let mut fake = Fake::default();
        fake.answer("ib_goals_empty", &[1]);
        assert_eq!(op(&mut fake, 1, EE_SCREENS_IB_OP_NO_GOALS, 5, 0, &mut []), Some(1));
        assert_eq!(fake.calls(), "ib_goals_empty(5)");
    }

    #[test]
    fn the_icon_and_the_frame() {
        let mut fake = Fake::default();
        fake.answer("ib_slot_draw", &[4, 6]);
        let mut f = [1.0, 2.0, 3.0, 4.0, 5.0, 6.0];
        assert_eq!(op(&mut fake, 1, EE_SCREENS_IB_OP_ICON, 9, 0, &mut f), Some(0));
        assert_eq!(op(&mut fake, 1, EE_SCREENS_IB_OP_FRAME, 0, 0, &mut f), Some(6));
        assert_eq!(
            fake.calls(),
            "ib_icon_texture(1, 9)\nib_icon_face(1, 9, 3.0, 4.0, 5.0, 6.0)\nib_slot_location(1, 0, 1.0, 2.0)\n\
             ib_slot_draw(1, 0)\nib_slot_location(1, 1, 1.0, 2.0)\nib_slot_draw(1, 1)"
        );
        assert_eq!(op(&mut fake, 1, 999, 0, 0, &mut f), None);
    }
}
