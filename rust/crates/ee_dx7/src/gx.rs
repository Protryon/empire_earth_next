//! The GX contract: the `extern "C"` functions of `ee_gx` (`rust/crates/ee_gx/src/ffi.rs`), resolved in the staticlib.
//! Not compiled in `cargo test` (no wgpu in the test binary): the logic is tested through [`crate::state::Device`]
//! with a recorder. The handle types (`GXDevice*`, ...) are the same opaque pointers the C++ `GX.h` shims hold.
#![allow(non_snake_case)] // keep the C names
#![allow(clippy::useless_conversion, clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)] // c_ulong differs per target

use core::ffi::{c_long, c_ulong, c_void};

use crate::state::Device;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_long, c_ulong, c_void};

    /// `GXMATERIAL` (`rust/crates/ee_gx/src/abi.rs GxMaterial`): colours of diffuse/ambient/specular/emissive, power.
    #[repr(C)]
    pub struct GxMaterial {
        pub diffuse: [f32; 4],
        pub ambient: [f32; 4],
        pub specular: [f32; 4],
        pub emissive: [f32; 4],
        pub power: f32,
    }

    extern "C" {
        pub fn gx_device_set_render_state(device: *mut c_void, state: u32, value: c_ulong) -> c_long;
        pub fn gx_device_set_stage_state(device: *mut c_void, stage: c_ulong, state: u32, value: c_ulong) -> c_long;
        pub fn gx_device_set_texture(device: *mut c_void, stage: c_ulong, texture: *mut c_void) -> c_long;
        pub fn gx_device_set_material(device: *mut c_void, material: *const GxMaterial) -> c_long;
        pub fn gx_device_set_transform(device: *mut c_void, state: u32, matrix: *const f32) -> c_long;
        pub fn gx_device_draw_primitive_vb(
            device: *mut c_void,
            primitive: u32,
            buffer: *mut c_void,
            start: c_ulong,
            count: c_ulong,
        ) -> c_long;
        pub fn gx_device_draw_indexed_primitive_vb(
            device: *mut c_void,
            primitive: u32,
            buffer: *mut c_void,
            start: c_ulong,
            count: c_ulong,
            indices: *const u16,
            index_count: c_ulong,
        ) -> c_long;
        pub fn PlatSleep(milliseconds: u32);
    }
}

use sys::*;

/// `GXDevice*`.
#[derive(Clone, Copy)]
pub struct RawDevice(pub *mut c_void);

impl RawDevice {
    pub fn set_transform(self, kind: u32, matrix: &[f32; 16]) -> c_long {
        unsafe { gx_device_set_transform(self.0, kind, matrix.as_ptr()) }
    }

    pub fn set_render_state_raw(self, state: u32, value: u32) -> c_long {
        unsafe { gx_device_set_render_state(self.0, state, c_ulong::from(value)) }
    }

    pub fn set_material(self, material: *const c_void) {
        unsafe { gx_device_set_material(self.0, material as *const GxMaterial) };
    }

    pub fn draw_primitive(self, primitive: u32, buffer: *mut c_void, start: u32, count: u32) -> i32 {
        unsafe {
            gx_device_draw_primitive_vb(self.0, primitive, buffer, c_ulong::from(start), c_ulong::from(count)) as i32
        }
    }

    pub fn draw_indexed(
        self,
        primitive: u32,
        buffer: *mut c_void,
        start: u32,
        count: u32,
        ix: *const u16,
        n: u32,
    ) -> i32 {
        unsafe {
            gx_device_draw_indexed_primitive_vb(
                self.0,
                primitive,
                buffer,
                c_ulong::from(start),
                c_ulong::from(count),
                ix,
                c_ulong::from(n),
            ) as i32
        }
    }
}

/// `PlatSleep(0)`.
pub fn yield_now() {
    unsafe { PlatSleep(0) };
}

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
