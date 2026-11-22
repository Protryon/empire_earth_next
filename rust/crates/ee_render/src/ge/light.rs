//! `GELight` whole in Rust: the C++ class holds an [`EeGeLight`] by value (its only member, so a `GELight*` is an
//! `EeGeLight*`) and every member forwards here; the DX7 rasterizer reads the record directly
//! (`dx7::rastobj` `light_params`).

use std::ffi::c_ulong;

use crate::dx7::raster::LightParams;
use ee_base::math::types::GeVector;

/// `GELight`'s members in the C++'s order (sizeof 0x74).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGeLight {
    pub color: [f32; 3],
    pub computed_color: [f32; 3],
    pub location: [f32; 3],
    pub model_space_location: [f32; 3],
    pub direction: [f32; 3],
    pub model_space_direction: [f32; 3],
    pub attenuation0: f32,
    pub attenuation1: f32,
    pub attenuation2: f32,
    pub intensity: f32,
    pub range: f32,
    pub range_inverse: f32,
    pub range_squared: f32,
    pub model_space_range: f32,
    pub model_space_range_inverse: f32,
    pub model_space_range_squared: f32,
    pub style: c_ulong,
}

/// `GEColor::Normalize`: each channel clamped to 0..1 (a NaN stays).
fn clamp_channel(v: f32) -> f32 {
    v.clamp(0.0, 1.0)
}

impl EeGeLight {
    /// `GELight()`: white, at the origin, intensity 1, constant attenuation 1, style 0 (the members' own
    /// constructors: `GEColor` white, the points and vectors zero; the range fields as the C++ left them, zero).
    pub fn new() -> Self {
        EeGeLight {
            color: [1.0; 3],
            computed_color: [1.0; 3],
            location: [0.0; 3],
            model_space_location: [0.0; 3],
            direction: [0.0; 3],
            model_space_direction: [0.0; 3],
            attenuation0: 1.0,
            attenuation1: 0.0,
            attenuation2: 0.0,
            intensity: 1.0,
            range: 0.0,
            range_inverse: 0.0,
            range_squared: 0.0,
            model_space_range: 0.0,
            model_space_range_inverse: 0.0,
            model_space_range_squared: 0.0,
            style: 0,
        }
    }

    /// `operator=`: the colour, location, direction, intensity, attenuations, range and style (not the computed
    /// colour, the range's derived values or the model-space fields, as the original).
    pub fn assign(&mut self, other: &EeGeLight) {
        self.color = other.color;
        self.location = other.location;
        self.direction = other.direction;
        self.intensity = other.intensity;
        self.attenuation0 = other.attenuation0;
        self.attenuation1 = other.attenuation1;
        self.attenuation2 = other.attenuation2;
        self.range = other.range;
        self.style = other.style;
    }

    /// `SetColor`: the colour clamped; the computed colour from the colour as given times the intensity.
    pub fn set_color(&mut self, color: [f32; 3]) {
        self.color = color.map(clamp_channel);
        self.computed_color = color.map(|c| c * self.intensity);
    }

    /// `SetDirection`: the reversed direction, normalized.
    pub fn set_direction(&mut self, direction: [f32; 3]) {
        let mut v = GeVector { x: -direction[0], y: -direction[1], z: -direction[2] };
        v.normalize();
        self.direction = [v.x, v.y, v.z];
    }

    /// `SetIntensity`: the computed colour from the colour times the intensity.
    pub fn set_intensity(&mut self, intensity: f32) {
        self.intensity = intensity;
        self.computed_color = self.color.map(|c| c * intensity);
    }

    /// The fields the DX7 rasterizer's `AddLight`/`UpdateLight` read (`c_ulong` is u32 on Windows only).
    #[allow(clippy::unnecessary_cast)]
    pub fn params(&self) -> LightParams {
        LightParams {
            location: self.location,
            direction: self.direction,
            computed_color: self.computed_color,
            style: self.style as u32,
            attenuation: [self.attenuation0, self.attenuation1, self.attenuation2],
            range: self.range,
        }
    }
}

impl Default for EeGeLight {
    fn default() -> Self {
        Self::new()
    }
}

/// `GELight()`.
///
/// # Safety
/// `l` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_light_init(l: *mut EeGeLight) {
    // SAFETY: the caller's contract
    unsafe { l.write(EeGeLight::new()) };
}

/// `operator=(inLight)`.
///
/// # Safety
/// Both are live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_light_assign(l: *mut EeGeLight, other: *const EeGeLight) {
    // SAFETY: the caller's contract (a self-assignment copies the fields onto themselves)
    unsafe {
        let other = *other;
        (*l).assign(&other);
    }
}

/// `SetColor(inColor)`.
///
/// # Safety
/// `l` is live; `color` points at three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_light_set_color(l: *mut EeGeLight, color: *const [f32; 3]) {
    // SAFETY: the caller's contract
    unsafe { (*l).set_color(*color) };
}

/// `SetDirection(inDirection)`.
///
/// # Safety
/// `l` is live; `direction` points at three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_light_set_direction(l: *mut EeGeLight, direction: *const [f32; 3]) {
    // SAFETY: the caller's contract
    unsafe { (*l).set_direction(*direction) };
}

/// `SetIntensity(inIntensity)`.
///
/// # Safety
/// `l` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_light_set_intensity(l: *mut EeGeLight, intensity: f32) {
    // SAFETY: the caller's contract
    unsafe { (*l).set_intensity(intensity) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_new_light_is_white_with_unit_intensity() {
        let l = EeGeLight::new();
        assert_eq!(
            (l.color, l.computed_color, l.intensity, l.attenuation0, l.style),
            ([1.0; 3], [1.0; 3], 1.0, 1.0, 0)
        );
        assert_eq!(std::mem::size_of::<EeGeLight>(), 0x70 + std::mem::size_of::<c_ulong>());
    }

    #[test]
    fn the_colour_is_clamped_but_the_computed_colour_is_not() {
        let mut l = EeGeLight::new();
        l.set_intensity(0.5);
        l.set_color([2.0, -1.0, 0.25]);
        assert_eq!(l.color, [1.0, 0.0, 0.25]);
        assert_eq!(l.computed_color, [1.0, -0.5, 0.125]);
        l.set_intensity(2.0);
        assert_eq!(l.computed_color, [2.0, 0.0, 0.5]);
    }

    #[test]
    fn the_direction_is_reversed_and_normalized() {
        let mut l = EeGeLight::new();
        l.set_direction([0.0, 3.0, 4.0]);
        assert_eq!(l.direction, [-0.0, -0.6, -0.8]);
        l.set_direction([0.0, 0.00001, 0.0]);
        assert_eq!(l.direction, [0.0; 3]);
    }

    #[test]
    fn assignment_copies_the_set_fields_only() {
        let mut a = EeGeLight::new();
        a.set_intensity(0.5);
        a.range_squared = 9.0;
        a.style = 3;
        let mut b = EeGeLight::new();
        b.assign(&a);
        assert_eq!((b.intensity, b.style, b.computed_color, b.range_squared), (0.5, 3, [1.0; 3], 0.0));
        let p = a.params();
        assert_eq!((p.style, p.computed_color, p.attenuation), (3, [0.5; 3], [1.0, 0.0, 0.0]));
    }
}
