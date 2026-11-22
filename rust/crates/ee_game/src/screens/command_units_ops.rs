//! `EEUICCommandUnits::RustOp` whole in Rust over one-statement shims (UI lane round 11): the facing arrow's draw
//! (the rasterizer's state saved, set for the arrow and put back), the ground point under the mouse at a height,
//! the unit or tile under a point, the selected group's centre, and the click action's `Execute`. The editor's
//! `EESEUICCommandUnits` keeps its own ops (the rotation, the pick, the deletion) in its C++ `RustOp`.

use crate::screens::command_units::*;
use crate::screens::game_cpp::Cpp;

/// The command control's op `op` on `ctl` (its `EEUIRustControl*`) with the op's floats `f`; `None` for another
/// host's.
pub(crate) fn op<C: Cpp + ?Sized>(c: &mut C, ctl: usize, op: u32, a: i64, b: i64, f: &mut [f32]) -> Option<i64> {
    match op {
        // f: the yaw, then the position
        EE_SCREENS_CU_DRAW_ARROW => {
            c.cu_save_ambient_color();
            c.cu_transform_reset();
            c.cu_save_viewport();
            let screen = c.cu_screen();
            let intensity = c.cu_ambient_intensity();
            let lighting = c.cu_lighting(screen);
            let zbuffer = c.cu_zbuffer(screen);
            let zcompare = c.cu_zcompare(screen);
            c.cu_depth_range(0.0, 0.0);
            c.cu_set_viewport_view(screen);
            c.cu_set_zbuffer_test_only(screen);
            c.cu_set_zcompare_less(screen);
            c.cu_set_ambient_white(screen);
            c.cu_set_ambient_intensity(1.0, screen);
            c.cu_set_lighting(0, screen);
            c.cu_transform_yaw(f[0]);
            c.cu_transform_position(f[1], f[2], f[3]);
            c.cu_model_frame(ctl);
            let zoom = c.cu_camera_zoom();
            c.cu_model_scale(ctl, zoom + 0.5);
            c.cu_model_transform(ctl);
            c.cu_model_prepare(ctl);
            c.cu_model_draw(ctl, screen);
            c.cu_depth_range(0.0, 1.0);
            c.cu_restore_viewport(screen);
            c.cu_set_zbuffer(zbuffer, screen);
            c.cu_set_zcompare(zcompare, screen);
            c.cu_restore_ambient_color(screen);
            c.cu_set_ambient_intensity(intensity, screen);
            c.cu_set_lighting(lighting, screen);
        }
        // the mouse's line from the front to the rear clipping plane, solved at the press's height (f: x, y, the
        // height; the point back in f[0..3])
        EE_SCREENS_CU_GROUND => {
            let scale = 1.0f32 / c.cu_half_width();
            let x = (f[0] - c.cu_x_center()) * scale;
            let y = -((f[1] - c.cu_y_center()) * scale);
            c.cu_near(x, y, 0.0);
            c.cu_far(x, y, 1.0);
            if !c.cu_ortho() {
                c.cu_denorm_persp_near();
                c.cu_denorm_persp_far();
            } else {
                c.cu_denorm_ortho_near();
                c.cu_denorm_ortho_far();
            }
            c.cu_line_define();
            c.cu_line_solve(f[2]);
            f[0] = c.cu_target_x();
            f[1] = c.cu_target_y();
            f[2] = c.cu_target_z();
        }
        // f: the screen point, then the point in and out (f[2..5])
        EE_SCREENS_CU_POINT_AT => {
            c.cu_point_set(f[2], f[3], f[4]);
            let hit = c.cu_point_at(f[0], f[1]);
            f[2] = c.cu_target_x();
            f[3] = c.cu_target_y();
            f[4] = c.cu_target_z();
            return Some(hit as i64);
        }
        EE_SCREENS_CU_GROUP_CENTER => {
            c.cu_bounds_reset();
            c.cu_group_bounds();
            c.cu_bounds_center();
            f[0] = c.cu_center_x();
            f[1] = c.cu_center_y();
        }
        EE_SCREENS_CU_EXECUTE => c.cu_execute(
            ctl,
            f[0],
            f[1],
            b & EE_SCREENS_CU_EXECUTE_HIT != 0,
            a as u32 as u64,
            b & EE_SCREENS_CU_EXECUTE_FACING != 0,
            f[2],
            b & EE_SCREENS_CU_EXECUTE_DRAGGED != 0,
        ),
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_arrow_draw_puts_the_rasterizer_back() {
        let mut fake = Fake::default();
        fake.answer("cu_screen", &[1]).answer("cu_lighting", &[3]).answer("cu_camera_zoom", &[2]);
        let mut f = [0.5, 1.0, 2.0, 3.0];
        assert_eq!(op(&mut fake, 9, EE_SCREENS_CU_DRAW_ARROW, 0, 0, &mut f), Some(0));
        let calls = fake.calls();
        assert!(calls.contains("cu_transform_yaw(0.5)\ncu_transform_position(1.0, 2.0, 3.0)"));
        assert!(calls.contains("cu_model_scale(9, 2.5)"));
        assert!(calls.ends_with("cu_set_ambient_intensity(0.0, 1)\ncu_set_lighting(3, 1)"));
    }

    #[test]
    fn the_ground_point_from_the_view_centre() {
        let mut fake = Fake::default();
        fake.answer("cu_half_width", &[2]).answer("cu_x_center", &[10]).answer("cu_y_center", &[20]);
        fake.answer("cu_ortho", &[1]).answer("cu_target_x", &[7]);
        let mut f = [14.0, 16.0, 5.0];
        op(&mut fake, 9, EE_SCREENS_CU_GROUND, 0, 0, &mut f);
        assert!(fake
            .calls()
            .contains("cu_near(2.0, 2.0, 0.0)\ncu_far(2.0, 2.0, 1.0)\ncu_ortho()\ncu_denorm_ortho_near()"));
        assert!(fake.calls().contains("cu_line_solve(5.0)"));
        assert_eq!(f, [7.0, 0.0, 0.0]);
    }

    #[test]
    fn the_click_unpacks_its_flags() {
        let mut fake = Fake::default();
        let mut f = [1.0, 2.0, 3.0];
        let flags = EE_SCREENS_CU_EXECUTE_HIT | EE_SCREENS_CU_EXECUTE_DRAGGED;
        op(&mut fake, 9, EE_SCREENS_CU_EXECUTE, 4, flags, &mut f);
        assert_eq!(fake.calls(), "cu_execute(9, 1.0, 2.0, true, 4, false, 3.0, true)");
        assert_eq!(op(&mut fake, 9, EE_SCREENS_CU_ROTATE, 0, 0, &mut f), None);
    }
}
