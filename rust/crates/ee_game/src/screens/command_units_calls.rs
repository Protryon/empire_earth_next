//! The right-drag command control's constructor and destructor bodies in Rust over one-statement C++ shims (UI
//! lane round 8; the shims are `ee_screenscpp_cu_*` in `User Interface Thread/EEUICCommandUnits.cpp`): the facing
//! arrow's textured quad model and its "Unit Facing Arrow" texture. `cu` is the `EEUICCommandUnits*`.

use crate::screens::game_cpp::{Cpp, EeScreensShimText};

/// The arrow's corners and their texture coordinates.
const CU_POINTS: [[f32; 3]; 4] = [[-0.25, 0.5, 0.0], [0.25, 0.5, 0.0], [0.25, 0.0, 0.0], [-0.25, 0.0, 0.0]];
const CU_COORDINATES: [[f32; 2]; 4] = [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]];

/// The constructor's body after the record and the display mode (`listener` the `EEAMouseClick*`).
pub(crate) fn construct(c: &mut impl Cpp, cu: usize, listener: usize) {
    c.cu_set_listener(cu, listener);
    c.cu_listener_reference(cu);
    c.cu_always_dirty(cu);
    c.cu_model_new(cu);
    c.cu_model_reference(cu);
    for (i, p) in CU_POINTS.iter().enumerate() {
        c.cu_point(cu, i as u32, p[0], p[1], p[2]);
    }
    for i in 0..4 {
        c.cu_normal(cu, i);
    }
    for (i, t) in CU_COORDINATES.iter().enumerate() {
        c.cu_coordinate(cu, i as u32, t[0], t[1]);
    }
    for i in 0..2 {
        c.cu_vertex1(cu, i, 0);
        c.cu_vertex2(cu, i, i + 1);
        c.cu_vertex3(cu, i, i + 2);
    }
    c.cu_polygon_start(cu);
    c.cu_polygon_length(cu);
    c.cu_vertex_start(cu);
    c.cu_vertex_length(cu);
    c.cu_bounds(cu);
    c.cu_set_description();
    let description = c.cu_description();
    let texture = c.texture_load(EeScreensShimText::of(b"textures\\Unit Facing Arrow"), description);
    c.cu_set_texture(cu, texture);
    if texture != 0 {
        c.texture_reference(texture);
        c.texture_alpha_texture(texture);
        c.texture_color_texture(texture);
        c.texture_min_filter(texture, 1);
        c.texture_mag_filter(texture, 1);
        c.texture_mip_point(texture);
        c.texture_mip_bias(texture, -1.0);
        c.texture_clamp(texture);
    }
    c.cu_material_texture(cu);
}

/// The destructor's body.
pub(crate) fn destroy(c: &mut impl Cpp, cu: usize) {
    c.cu_listener_dereference(cu);
    c.cu_model_dereference(cu);
    c.cu_texture_dereference(cu);
}

#[cfg(not(test))]
mod ffi {
    use crate::screens::game_cpp::Ffi;

    /// `EEUICCommandUnits`' constructor's body after the record and the display mode.
    #[no_mangle]
    pub extern "C" fn ee_screens_cu_construct(cu: usize, listener: usize) {
        super::construct(&mut Ffi, cu, listener)
    }
    /// `~EEUICCommandUnits`' body.
    #[no_mangle]
    pub extern "C" fn ee_screens_cu_destroy(cu: usize) {
        super::destroy(&mut Ffi, cu)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_arrow_model_and_texture() {
        let mut f = Fake::default();
        f.answer("texture_load", &[9]);
        construct(&mut f, 0x10, 0x20);
        let calls = f.calls();
        assert!(calls
            .starts_with("cu_set_listener(16, 32)\ncu_listener_reference(16)\ncu_always_dirty(16)\ncu_model_new(16)"));
        assert!(calls.contains("cu_point(16, 3, -0.25, 0.0, 0.0)\ncu_normal(16, 0)"));
        assert!(
            calls.contains("cu_vertex1(16, 1, 0)\ncu_vertex2(16, 1, 2)\ncu_vertex3(16, 1, 3)\ncu_polygon_start(16)")
        );
        assert!(calls.contains("cu_set_texture(16, 9)\ntexture_reference(9)\ntexture_alpha_texture(9)"));
        assert!(calls.ends_with("texture_mip_bias(9, -1.0)\ntexture_clamp(9)\ncu_material_texture(16)"));
        let mut f = Fake::default();
        construct(&mut f, 0x10, 0x20);
        assert!(f.calls().ends_with("cu_set_texture(16, 0)\ncu_material_texture(16)"));
        destroy(&mut f, 0x10);
        assert!(f
            .calls()
            .ends_with("cu_listener_dereference(16)\ncu_model_dereference(16)\ncu_texture_dereference(16)"));
    }
}
