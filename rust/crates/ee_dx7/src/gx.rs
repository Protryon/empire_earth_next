//! The GX contract: the `extern "C"` functions of `ee_gx` (`rust/crates/ee_gx/src/ffi.rs`), resolved in the staticlib.
//! Not compiled in `cargo test` (no wgpu in the test binary): the logic is tested through [`crate::state::Device`]
//! with a recorder. The handle types (`GXDevice*`, ...) are the same opaque pointers the C++ `GX.h` shims hold.
#![allow(non_snake_case)] // keep the C names
#![allow(clippy::useless_conversion, clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)] // c_ulong differs per target

use core::ffi::{c_long, c_ulong, c_void};

use crate::state::Device;

/// `GXMATERIAL` (`rust/crates/ee_gx/src/abi.rs GxMaterial`): colours r,g,b,a of diffuse/ambient/specular/emissive, power.
/// Only the white material of `SetShadingColorSource` is built here.
#[repr(C)]
struct GxMaterial {
    diffuse: [f32; 4],
    ambient: [f32; 4],
    specular: [f32; 4],
    emissive: [f32; 4],
    power: f32,
}

// cbindgen:ignore (the Rust side of ee_gx's own exports; this is not API)
extern "C" {
    fn gx_device_set_render_state(device: *mut c_void, state: u32, value: c_ulong) -> c_long;
    fn gx_device_set_stage_state(device: *mut c_void, stage: c_ulong, state: u32, value: c_ulong) -> c_long;
    fn gx_device_set_texture(device: *mut c_void, stage: c_ulong, texture: *mut c_void) -> c_long;
    fn gx_device_set_material(device: *mut c_void, material: *const GxMaterial) -> c_long;
}

/// `GXDevice*`.
#[derive(Clone, Copy)]
pub struct RawDevice(pub *mut c_void);

impl Device for RawDevice {
    fn set_render_state(&mut self, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_render_state(self.0, state, c_ulong::from(value)) as i32 }
    }

    fn set_stage_state(&mut self, stage: u32, state: u32, value: u32) -> i32 {
        unsafe { gx_device_set_stage_state(self.0, c_ulong::from(stage), state, c_ulong::from(value)) as i32 }
    }

    fn set_texture(&mut self, stage: u32, surface: *mut c_void) -> i32 {
        unsafe { gx_device_set_texture(self.0, c_ulong::from(stage), surface) as i32 }
    }

    fn set_material_white(&mut self) -> i32 {
        let material = GxMaterial {
            diffuse: [1.0, 1.0, 1.0, 1.0],
            ambient: [1.0, 1.0, 1.0, 1.0],
            specular: [0.0; 4],
            emissive: [0.0; 4],
            power: 0.0,
        };
        unsafe { gx_device_set_material(self.0, &material) as i32 }
    }
}
