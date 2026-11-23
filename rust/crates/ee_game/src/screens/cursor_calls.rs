//! The cursor's host's cases of more than one call (`ee_screenscpp_cursor`, `EEUIFMainGame.cpp`) in Rust over
//! one-statement shims (UI lane round 10): the mouse over the game, the pick, the target's disabled state, the aim
//! (EEGetAttackRangeKind's: the model's default point scaled and placed, else the tile's centre), the gather and
//! convert questions. `ctx` is the host's read state (`EEUIFMGCursorRead*`); `cursor_ffi.rs` asks here first.

use crate::screens::cursor::{
    EE_SCREENS_CURSOR_OP_AIM, EE_SCREENS_CURSOR_OP_CAN_GATHER, EE_SCREENS_CURSOR_OP_CAN_HEAL,
    EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK, EE_SCREENS_CURSOR_OP_FLAGS, EE_SCREENS_CURSOR_OP_IN_VIEW,
    EE_SCREENS_CURSOR_OP_MOUSE_OK, EE_SCREENS_CURSOR_OP_PICK, EE_SCREENS_CURSOR_OP_RELEASE,
    EE_SCREENS_CURSOR_OP_SEEN, EE_SCREENS_CURSOR_OP_SEL_AIR_BASE, EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK,
    EE_SCREENS_CURSOR_OP_SEL_CONVERT, EE_SCREENS_CURSOR_OP_SEL_CRUSADER, EE_SCREENS_CURSOR_OP_SEL_PRIEST,
    EE_SCREENS_CURSOR_OP_T_DISABLED, EE_SCREENS_CURSOR_OP_T_TARGETABLE,
};
use crate::screens::game_cpp::{Cpp, EeScreensShimF32Out as F32Out};

/// The host's moved op; `None` for the C++ host's.
pub(crate) fn call<C: Cpp + ?Sized>(c: &mut C, ctx: usize, op: u32, a: u32, b: u32, f: &mut [f32; 3]) -> Option<i32> {
    Some(match op {
        EE_SCREENS_CURSOR_OP_MOUSE_OK => {
            let over = c.cur_mouse_over(ctx);
            (over == 0 || over == c.cur_command_units(ctx)) as i32
        }
        EE_SCREENS_CURSOR_OP_PICK => {
            c.cur_pick(ctx, a);
            c.cur_has_target(ctx) as i32
        }
        EE_SCREENS_CURSOR_OP_T_DISABLED => {
            (c.cur_t_on_tile(ctx) && c.cur_t_disabled(ctx) && !c.cur_t_protected(ctx)) as i32
        }
        EE_SCREENS_CURSOR_OP_AIM => {
            let mut aim = [0.0f32; 3];
            if c.cur_t_has_complex(ctx) && c.cur_t_has_model(ctx) {
                let scale = c.cur_t_scale(ctx);
                let mut point = [0.0f32; 3];
                c.cur_t_default_point(ctx, F32Out(point.as_mut_ptr()));
                c.cur_t_place(ctx, point[0] * scale, point[1] * scale, point[2] * scale, F32Out(aim.as_mut_ptr()));
            } else {
                aim[0] = c.cur_t_tile_x(ctx) as f32 + 0.5;
                aim[1] = c.cur_t_tile_y(ctx) as f32 + 0.5;
                aim[2] = c.cur_t_tile_height(ctx);
            }
            *f = aim;
            0
        }
        EE_SCREENS_CURSOR_OP_CAN_GATHER => c.cur_can_gather(ctx, a) as i32,
        EE_SCREENS_CURSOR_OP_SEL_CONVERT => {
            c.cur_set_shielded(b != 0);
            let converts = c.cur_convert(ctx, a);
            converts as i32 | if c.cur_shielded() { 2 } else { 0 }
        }
        // the one-call ops (the C++ host's switch until UI lane round 16)
        EE_SCREENS_CURSOR_OP_FLAGS => c.cur_flags(ctx),
        EE_SCREENS_CURSOR_OP_IN_VIEW => c.cur_in_view() as i32,
        EE_SCREENS_CURSOR_OP_RELEASE => {
            c.cur_release(ctx);
            0
        }
        EE_SCREENS_CURSOR_OP_SEEN => c.cur_seen(ctx) as i32,
        EE_SCREENS_CURSOR_OP_T_TARGETABLE => c.cur_t_targetable(ctx) as i32,
        EE_SCREENS_CURSOR_OP_CAN_HEAL => c.cur_can_heal(ctx) as i32,
        EE_SCREENS_CURSOR_OP_SEL_AIR_BASE => c.cur_sel_air_base(ctx, a) as i32,
        EE_SCREENS_CURSOR_OP_SEL_CRUSADER => c.cur_sel_crusader(ctx, a) as i32,
        EE_SCREENS_CURSOR_OP_SEL_PRIEST => c.cur_sel_priest(ctx, a) as i32,
        EE_SCREENS_CURSOR_OP_SEL_CAN_ATTACK => c.cur_sel_can_attack(ctx, a) as i32,
        EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK => c.cur_cargo_can_attack(ctx, a, b) as i32,
        _ => return None,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn mouse_pick_and_disabled() {
        let mut fake = Fake::default();
        fake.answer("cur_mouse_over", &[0, 5, 5]).answer("cur_command_units", &[5, 6]).answer("cur_has_target", &[1]);
        fake.answer("cur_t_on_tile", &[1]).answer("cur_t_disabled", &[1]);
        let mut f = [0.0; 3];
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_MOUSE_OK, 0, 0, &mut f), Some(1));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_MOUSE_OK, 0, 0, &mut f), Some(1));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_MOUSE_OK, 0, 0, &mut f), Some(0));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_PICK, 3, 0, &mut f), Some(1));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_T_DISABLED, 0, 0, &mut f), Some(1));
        assert_eq!(
            fake.calls(),
            "cur_mouse_over(1)\ncur_mouse_over(1)\ncur_command_units(1)\ncur_mouse_over(1)\ncur_command_units(1)\n\
             cur_pick(1, 3)\ncur_has_target(1)\ncur_t_on_tile(1)\ncur_t_disabled(1)\ncur_t_protected(1)"
        );
    }

    #[test]
    fn aim_is_the_tile_centre_without_a_model() {
        let mut fake = Fake::default();
        fake.answer("cur_t_tile_x", &[4]).answer("cur_t_tile_y", &[6]).answer("cur_t_tile_height", &[2]);
        let mut f = [9.0; 3];
        call(&mut fake, 1, EE_SCREENS_CURSOR_OP_AIM, 0, 0, &mut f);
        assert_eq!(f, [4.5, 6.5, 2.0]);
        let mut fake = Fake::default();
        fake.answer("cur_t_has_complex", &[1]).answer("cur_t_has_model", &[1]).answer("cur_t_scale", &[2]);
        call(&mut fake, 1, EE_SCREENS_CURSOR_OP_AIM, 0, 0, &mut f);
        assert_eq!(
            fake.calls(),
            "cur_t_has_complex(1)\ncur_t_has_model(1)\ncur_t_scale(1)\ncur_t_default_point(1, out)\n\
             cur_t_place(1, 0.0, 0.0, 0.0, out)"
        );
    }

    #[test]
    fn gather_and_convert() {
        let mut fake = Fake::default();
        fake.answer("cur_convert", &[1]).answer("cur_shielded", &[1]);
        let mut f = [0.0; 3];
        call(&mut fake, 1, EE_SCREENS_CURSOR_OP_CAN_GATHER, 8, 0, &mut f);
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_SEL_CONVERT, 2, 1, &mut f), Some(3));
        assert_eq!(fake.calls(), "cur_can_gather(1, 8)\ncur_set_shielded(true)\ncur_convert(1, 2)\ncur_shielded()");
        assert_eq!(call(&mut fake, 1, 999, 0, 0, &mut f), None);
    }

    #[test]
    fn the_one_call_ops() {
        let mut fake = Fake::default();
        fake.answer("cur_flags", &[6]).answer("cur_sel_priest", &[1]);
        let mut f = [0.0; 3];
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_FLAGS, 0, 0, &mut f), Some(6));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_RELEASE, 0, 0, &mut f), Some(0));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_SEL_PRIEST, 4, 0, &mut f), Some(1));
        assert_eq!(call(&mut fake, 1, EE_SCREENS_CURSOR_OP_CARGO_CAN_ATTACK, 4, 2, &mut f), Some(0));
        assert_eq!(
            fake.calls(),
            "cur_flags(1)
cur_release(1)
cur_sel_priest(1, 4)
cur_cargo_can_attack(1, 4, 2)"
        );
    }
}
