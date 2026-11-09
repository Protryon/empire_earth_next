//! `extern "C"` for `DX7Movie` (`Common/DX7Movie.cpp`): the ring and the C++ trampolines the playback thread drives.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_ulong, c_void};

use crate::api::SurfaceDescription;
use crate::movie::{self, MovieHost, RenderHost, SoundRing};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_ulong, c_void};
    extern "C" {
        pub fn ee_dx7cpp_movie_thread_begin(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_open(movie: *mut c_void, path: *const u8, length: usize) -> bool;
        pub fn ee_dx7cpp_movie_video_streams(movie: *mut c_void) -> i32;
        pub fn ee_dx7cpp_movie_width(movie: *mut c_void) -> i32;
        pub fn ee_dx7cpp_movie_height(movie: *mut c_void) -> i32;
        pub fn ee_dx7cpp_movie_create_frame_buffer(movie: *mut c_void, width: i32, height: i32) -> i32;
        pub fn ee_dx7cpp_movie_set_finished(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_set_loop(movie: *mut c_void, looping: bool);
        pub fn ee_dx7cpp_movie_install_video_callback(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_mixer_rate(movie: *mut c_void) -> u32;
        pub fn ee_dx7cpp_movie_audio_streams(movie: *mut c_void) -> i32;
        pub fn ee_dx7cpp_movie_sample_rate(movie: *mut c_void) -> i32;
        pub fn ee_dx7cpp_movie_set_audio_enabled(movie: *mut c_void, enabled: bool);
        pub fn ee_dx7cpp_movie_install_audio_callback(movie: *mut c_void, lead_time: f64);
        pub fn ee_dx7cpp_movie_set_sound_step(movie: *mut c_void, step: f64);
        pub fn ee_dx7cpp_movie_set_mixer_source(movie: *mut c_void, attach: bool);
        pub fn ee_dx7cpp_movie_set_playing_started(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_should_stop(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_fp_precision() -> u32;
        pub fn ee_dx7cpp_movie_set_fp_precision(precision: u32);
        pub fn ee_dx7cpp_movie_fp_rounding() -> u32;
        pub fn ee_dx7cpp_movie_set_fp_rounding(mode: u32);
        pub fn ee_dx7cpp_movie_decode(movie: *mut c_void, elapsed: f64);
        pub fn ee_dx7cpp_movie_has_ended(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_frame_enter(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_frame_leave(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_clear_playing(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_frame_ready_set(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_release_frame_buffer(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_clear_just_started(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_destroy(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_dereference(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_thread_end(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_is_playing(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_just_started(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_wait_frame_ready(movie: *mut c_void);
        pub fn ee_dx7cpp_movie_frame_buffer_present(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_has_rectangle(movie: *mut c_void) -> bool;
        pub fn ee_dx7cpp_movie_destination_rectangle(movie: *mut c_void, out: *mut f32);
        pub fn ee_dx7cpp_movie_target_size(movie: *mut c_void, width: *mut u32, height: *mut u32);
        pub fn ee_dx7cpp_movie_copy_frame(movie: *mut c_void, destination: *const f32) -> i32;

        pub fn PlatSleep(milliseconds: u32);
        pub fn PlatCounterFrequency() -> i64;
        pub fn PlatCounterTicks() -> i64;
        pub fn ee_dx7cpp_pack_bgrx_row(source: *const u8, destination: *mut u8, width: c_ulong, format: c_ulong);
    }
}

use sys::*;

/// The Rust side of `DX7Movie`'s thread hooks: only a raw pointer, which C++ keeps alive for the thread's life.
struct CppHost(*mut c_void);

impl MovieHost for CppHost {
    fn thread_begin(&mut self) {
        unsafe { ee_dx7cpp_movie_thread_begin(self.0) }
    }
    fn open(&mut self, path: &[u8]) -> bool {
        unsafe { ee_dx7cpp_movie_open(self.0, path.as_ptr(), path.len()) }
    }
    fn video_stream_count(&mut self) -> i32 {
        unsafe { ee_dx7cpp_movie_video_streams(self.0) }
    }
    fn width(&mut self) -> i32 {
        unsafe { ee_dx7cpp_movie_width(self.0) }
    }
    fn height(&mut self) -> i32 {
        unsafe { ee_dx7cpp_movie_height(self.0) }
    }
    fn create_frame_buffer(&mut self, width: i32, height: i32) -> i32 {
        unsafe { ee_dx7cpp_movie_create_frame_buffer(self.0, width, height) }
    }
    fn set_finished(&mut self) {
        unsafe { ee_dx7cpp_movie_set_finished(self.0) }
    }
    fn set_loop(&mut self, looping: bool) {
        unsafe { ee_dx7cpp_movie_set_loop(self.0, looping) }
    }
    fn install_video_callback(&mut self) {
        unsafe { ee_dx7cpp_movie_install_video_callback(self.0) }
    }
    fn mixer_rate(&mut self) -> u32 {
        unsafe { ee_dx7cpp_movie_mixer_rate(self.0) }
    }
    fn audio_stream_count(&mut self) -> i32 {
        unsafe { ee_dx7cpp_movie_audio_streams(self.0) }
    }
    fn sample_rate(&mut self) -> i32 {
        unsafe { ee_dx7cpp_movie_sample_rate(self.0) }
    }
    fn set_audio_enabled(&mut self, enabled: bool) {
        unsafe { ee_dx7cpp_movie_set_audio_enabled(self.0, enabled) }
    }
    fn install_audio_callback(&mut self, lead_time: f64) {
        unsafe { ee_dx7cpp_movie_install_audio_callback(self.0, lead_time) }
    }
    fn set_sound_step(&mut self, step: f64) {
        unsafe { ee_dx7cpp_movie_set_sound_step(self.0, step) }
    }
    fn set_mixer_source(&mut self, attach: bool) {
        unsafe { ee_dx7cpp_movie_set_mixer_source(self.0, attach) }
    }
    fn set_playing_started(&mut self) {
        unsafe { ee_dx7cpp_movie_set_playing_started(self.0) }
    }
    fn counter_frequency(&mut self) -> i64 {
        unsafe { PlatCounterFrequency() }
    }
    fn counter_ticks(&mut self) -> i64 {
        unsafe { PlatCounterTicks() }
    }
    fn should_stop(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_should_stop(self.0) }
    }
    fn fp_precision(&mut self) -> u32 {
        unsafe { ee_dx7cpp_movie_fp_precision() }
    }
    fn set_fp_precision(&mut self, precision: u32) {
        unsafe { ee_dx7cpp_movie_set_fp_precision(precision) }
    }
    fn fp_rounding(&mut self) -> u32 {
        unsafe { ee_dx7cpp_movie_fp_rounding() }
    }
    fn set_fp_rounding(&mut self, mode: u32) {
        unsafe { ee_dx7cpp_movie_set_fp_rounding(mode) }
    }
    fn decode(&mut self, elapsed: f64) {
        unsafe { ee_dx7cpp_movie_decode(self.0, elapsed) }
    }
    fn has_ended(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_has_ended(self.0) }
    }
    fn sleep(&mut self, milliseconds: u32) {
        unsafe { PlatSleep(milliseconds) }
    }
    fn frame_enter(&mut self) {
        unsafe { ee_dx7cpp_movie_frame_enter(self.0) }
    }
    fn frame_leave(&mut self) {
        unsafe { ee_dx7cpp_movie_frame_leave(self.0) }
    }
    fn clear_playing(&mut self) {
        unsafe { ee_dx7cpp_movie_clear_playing(self.0) }
    }
    fn frame_ready_set(&mut self) {
        unsafe { ee_dx7cpp_movie_frame_ready_set(self.0) }
    }
    fn release_frame_buffer(&mut self) {
        unsafe { ee_dx7cpp_movie_release_frame_buffer(self.0) }
    }
    fn clear_just_started(&mut self) {
        unsafe { ee_dx7cpp_movie_clear_just_started(self.0) }
    }
    fn destroy(&mut self) {
        unsafe { ee_dx7cpp_movie_destroy(self.0) }
    }
    fn dereference(&mut self) {
        unsafe { ee_dx7cpp_movie_dereference(self.0) }
    }
    fn thread_end(&mut self) {
        unsafe { ee_dx7cpp_movie_thread_end(self.0) }
    }
}

impl RenderHost for CppHost {
    fn just_started(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_just_started(self.0) }
    }
    fn is_playing(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_is_playing(self.0) }
    }
    fn wait_frame_ready(&mut self) {
        unsafe { ee_dx7cpp_movie_wait_frame_ready(self.0) }
    }
    fn frame_enter(&mut self) {
        unsafe { ee_dx7cpp_movie_frame_enter(self.0) }
    }
    fn frame_leave(&mut self) {
        unsafe { ee_dx7cpp_movie_frame_leave(self.0) }
    }
    fn frame_buffer_present(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_frame_buffer_present(self.0) }
    }
    fn has_rectangle(&mut self) -> bool {
        unsafe { ee_dx7cpp_movie_has_rectangle(self.0) }
    }
    fn destination_rectangle(&mut self) -> [f32; 4] {
        let mut out = [0.0f32; 4];
        unsafe { ee_dx7cpp_movie_destination_rectangle(self.0, out.as_mut_ptr()) };
        out
    }
    fn target_size(&mut self) -> (u32, u32) {
        let (mut width, mut height) = (0u32, 0u32);
        unsafe { ee_dx7cpp_movie_target_size(self.0, &mut width, &mut height) };
        (width, height)
    }
    fn copy_frame(&mut self, destination: [f32; 4]) -> i32 {
        unsafe { ee_dx7cpp_movie_copy_frame(self.0, destination.as_ptr()) }
    }
}

// ---- the sound ring

#[no_mangle]
pub extern "C" fn ee_dx7m_ring_new() -> *mut SoundRing {
    Box::into_raw(Box::new(SoundRing::new()))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_ring_destroy(ring: *mut SoundRing) {
    drop(unsafe { Box::from_raw(ring) });
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_ring_set_step(ring: *mut SoundRing, step: f64) {
    unsafe { (*ring).set_step(step) };
}

/// `DecodedSamples`: `2 * inCount` interleaved floats.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_ring_push(ring: *mut SoundRing, samples: *const f32, count: u32) {
    let samples = unsafe { core::slice::from_raw_parts(samples, 2 * count as usize) };
    unsafe { (*ring).push(samples) };
}

/// `MixSound`: adds to `2 * count` floats.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_ring_mix(ring: *mut SoundRing, out: *mut f32, count: u32) {
    let out = unsafe { core::slice::from_raw_parts_mut(out, 2 * count as usize) };
    unsafe { (*ring).mix(out, count as usize) };
}

// ---- the thread and the render

/// `DX7Movie::PlayMovie`: `movie` is the `DX7Movie*`, `filename` its file name (not NUL terminated in Rust).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_play_movie(movie: *mut c_void, filename: *const u8, length: usize, looping: bool) {
    let name = unsafe { core::slice::from_raw_parts(filename, length) }.to_vec(); // no reference across the C++ calls
    movie::play_movie(&mut CppHost(movie), &name, looping);
}

/// `DX7Movie::RenderFrame`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_render_frame(movie: *mut c_void) -> i32 {
    movie::render_frame(&mut CppHost(movie))
}

/// `DecodedFrame`'s rows, through `GEPackBGRXRow`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_convert_frame(
    bgra: *const u8,
    stride: u32,
    destination: *mut u8,
    pitch: u32,
    width: u32,
    height: u32,
    format: c_ulong,
) {
    unsafe {
        movie::convert_frame(bgra, stride, destination, pitch, height, |src, dst| {
            ee_dx7cpp_pack_bgrx_row(src, dst, c_ulong::from(width), format);
        });
    }
}

/// `CreateFrameBuffer`'s description.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7m_frame_buffer_description(
    out: *mut SurfaceDescription,
    pixel_format: u32,
    pixel_depth: u32,
    width: u32,
    height: u32,
) {
    unsafe { *out = movie::frame_buffer_description(pixel_format, pixel_depth, width, height) };
}
