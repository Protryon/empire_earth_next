//! Two small Graphics Thread bodies in Rust (game core lane, round 11): EEOrbital::Init (the orbiting part's model and
//! its speed from the model's name) and EEROEffects::SceneStarting (the game view's z buffer cleared before the
//! effects draw). Each C++ call or member is a case of EERenderObject.cpp's `EECore9OpA`.

use std::ffi::{c_char, c_void};

use ee_base::math::types::GeRectangle;
use ee_render::gfx::effect_orbit::ee_gfx_orbital_speed;

use super::{h, hf, hp, pa, ptr, O, P};

/// EEOrbital::Init: attached to `model`, its speed from the model's name (degrees per second; 0 first, as the C++).
#[no_mangle]
pub extern "C" fn ee_gamecpp_ov_orbital_init(orbital: *mut c_void, model: *mut c_void) {
    let (o, m) = (orbital as P, model as P);
    hf(O::OvSetSpeed, o, &[], &[0.0]);
    h(O::OvSetModel, o, &[pa(m)]);
    // SAFETY: the model's name (NUL-terminated)
    let speed = unsafe { ee_gfx_orbital_speed(hp(O::OvModelName, m, &[]) as *const c_char) };
    hf(O::OvSetSpeed, o, &[], &[speed]);
}

/// EEROEffects::SceneStarting: the main viewport's view rectangle's z buffer cleared to 1.
#[no_mangle]
pub extern "C" fn ee_gamecpp_ov_effects_scene_starting() {
    let mut rect = GeRectangle::default();
    h(O::RuMainViewRectangle, 0, &[ptr(&mut rect)]);
    hf(O::GuClearZ, 0, &[ptr(&mut rect)], &[1.0]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_name_without_the_prefix_does_not_orbit() {
        // SAFETY: a NUL-terminated name
        assert_eq!(unsafe { ee_gfx_orbital_speed(c"turret_01".as_ptr()) }, 0.0);
    }
}
