//! TEST ONLY: the capture's map, mini map and cheat list records whose data the C++ hands over whole (were
//! EEAppCaptureWorld.cpp's `EEAppCaptureChecksumImpl`, `EEAppCaptureSlopeImpl`, `EEAppCaptureMiniMapPixelsImpl`,
//! `EEAppCaptureMiniMapSnapshotImpl` and `EEAppCaptureCheatListImpl`; tools/sim_capture.py app profile). The game's
//! units call these directly: they are no C++ unit, so the TEST ONLY code adds no edge to the C++ graph.
//!
//! - MAPK: the map checksum's cursor before, its result, the cursor after.
//! - MAPP: a slope's kind (0 pitch, 1 roll), the location's transformation, the result.
//! - MMPX: a redrawn rectangle of the mini map's pixels, row by row.
//! - MMSN: every mini map cell's count and pixel, and the dots of a cell that has any.
//! - CHLS: a cheat line's bytes.
#![allow(clippy::unnecessary_cast)] // c_long is 32 bits on Windows, 64 on LP64

use std::os::raw::{c_char, c_long, c_ulong};

use ee_base::math::types::GeTransformation;
use ee_sim::app::capture;

use crate::app::map_capture::{tag, Payload};
use crate::app::minimap::{EeAppMiniMap, EeAppMiniMapDot, EeAppMiniMapUnitDot};
use crate::app::minimap_ffi::ee_app_minimap_cell;

/// MAPK: `EEMap::GetChecksum`'s cursor before, its result and the cursor after.
#[no_mangle]
pub extern "C" fn ee_app_capture_map_checksum(before: c_ulong, result: c_long, after: c_ulong) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut r = Payload::default();
    r.u32(before as u32).i32(result as i32).u32(after as u32);
    capture::commit(tag(b"MAPK"), &r.0);
}

/// MAPP: a slope's kind, the location (its 64 bytes) and the result.
///
/// # Safety
/// `location` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_slope(kind: u32, location: *const GeTransformation, result: f32) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut r = Payload::default();
    r.u32(kind);
    // SAFETY: the caller's live transformation, read as its bytes
    let bytes = unsafe {
        std::slice::from_raw_parts(location.cast::<u8>(), std::mem::size_of::<GeTransformation>())
    };
    r.0.extend_from_slice(bytes);
    r.f32(result);
    capture::commit(tag(b"MAPP"), &r.0);
}

/// MMPX: the rectangle and its pixels, rows `bottom` to `top` of `left` to `right` (`pitch` pixels a row).
///
/// # Safety
/// `bits` holds the rows the rectangle reads.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_minimap_pixels(
    left: c_long,
    right: c_long,
    bottom: c_long,
    top: c_long,
    bits: *const u16,
    pitch: c_ulong,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    let mut r = Payload::default();
    r.i32(left as i32).i32(right as i32).i32(bottom as i32).i32(top as i32);
    let mut y = bottom;
    while y < top {
        if right > left {
            let start = pitch as usize * y as usize + left as usize;
            // SAFETY: the caller's rows
            let row = unsafe { std::slice::from_raw_parts(bits.add(start).cast::<u8>(), (right - left) as usize * 2) };
            r.0.extend_from_slice(row);
        }
        y += 1;
    }
    capture::commit(tag(b"MMPX"), &r.0);
}

/// One cell of the snapshot: its count with its pixel, then (when it has dots) its index's marker and the dots.
fn snapshot_cell(r: &mut Payload, i: u32, count: u16, pixel: u16, dots: &[EeAppMiniMapDot], units: &[EeAppMiniMapUnitDot]) {
    r.u32(count as u32 | (pixel as u32) << 16);
    if dots.is_empty() && units.is_empty() {
        return;
    }
    r.u32(0x8000_0000 | i).u32(dots.len() as u32);
    for d in dots {
        r.u32(d.player as u32 | (d.color as u32) << 16).i32(d.flag as i32);
    }
    r.u32(units.len() as u32);
    for d in units {
        r.u32(d.player as u32 | (d.color as u32) << 16).i32(d.flag as i32).i32(d.x as i32).i32(d.y as i32);
    }
}

/// MMSN: the mini map `map`'s 4096 cells (no record without a mini map); `pixels` (null: 0) holds the unit surface's
/// pixels, `pitch` a row (the cell at index i is the pixel of column i >> 6, row i & 0x3f).
///
/// # Safety
/// `map` is a live mini map or null; `pixels` covers 64 rows of `pitch` or is null.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_minimap_snapshot(map: *const EeAppMiniMap, pixels: *const u16, pitch: c_long) {
    if !capture::ee_app_capture_active() || map.is_null() {
        return;
    }
    let mut r = Payload::default();
    for i in 0..0x1000u32 {
        let mut count = 0u16;
        let (mut dots, mut units) = (std::ptr::null(), std::ptr::null());
        let (mut dot_count, mut unit_count): (c_long, c_long) = (0, 0);
        // SAFETY: a live mini map, an index below 4096, live outputs
        unsafe { ee_app_minimap_cell(map, i as c_long, &mut count, &mut dots, &mut dot_count, &mut units, &mut unit_count) };
        let pixel = if pixels.is_null() {
            0
        } else {
            // SAFETY: the caller's surface
            unsafe { *pixels.add(pitch as usize * (i & 0x3f) as usize + (i >> 6) as usize) }
        };
        // SAFETY: the cell's lists (live until the mini map changes, not during this call)
        let (dots, units) = unsafe {
            (
                std::slice::from_raw_parts(dots, dot_count as usize),
                std::slice::from_raw_parts(units, unit_count as usize),
            )
        };
        snapshot_cell(&mut r, i, count, pixel, dots, units);
    }
    capture::commit(tag(b"MMSN"), &r.0);
}

/// CHLS: one line of the cheat list (its bytes, a C string).
///
/// # Safety
/// `line` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_cheat_list(line: *const c_char) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's C string
    let bytes = unsafe { std::ffi::CStr::from_ptr(line) }.to_bytes();
    capture::commit(tag(b"CHLS"), bytes);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_cell_with_and_without_dots() {
        let mut r = Payload::default();
        snapshot_cell(&mut r, 3, 2, 0x1234, &[], &[]);
        assert_eq!(r.0, (2u32 | 0x1234 << 16).to_le_bytes());
        let mut r = Payload::default();
        let dot = EeAppMiniMapDot { player: 1, color: 7, flag: 1 };
        let unit = EeAppMiniMapUnitDot { player: 2, color: 9, flag: 0, x: 4, y: 5 };
        snapshot_cell(&mut r, 0x41, 1, 0, &[dot], &[unit]);
        let words: Vec<u32> = r.0.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect();
        assert_eq!(words, vec![1, 0x8000_0041, 1, 1 | 7 << 16, 1, 1, 2 | 9 << 16, 0, 4, 5]);
    }

    #[test]
    fn nothing_without_a_capture() {
        // tests never set EE_SIM_CAPTURE: each returns before reading its arguments
        ee_app_capture_map_checksum(1, 2, 3);
        unsafe {
            ee_app_capture_slope(0, std::ptr::null(), 0.0);
            ee_app_capture_minimap_pixels(0, 4, 0, 4, std::ptr::null(), 4);
            ee_app_capture_minimap_snapshot(std::ptr::null(), std::ptr::null(), 0);
            ee_app_capture_cheat_list(std::ptr::null());
        }
    }
}
