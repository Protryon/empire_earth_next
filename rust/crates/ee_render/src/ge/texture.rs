//! `GETexture`'s logic (`GETexture.cpp`): how a texture's frames become surfaces (`Load`'s MIP description and the
//! dropped-level description for one texture per frame, the frame grid for one surface of cells), the current
//! frame's UV corner (`SetFrame`), the reference-count decision of `Dereference`, `GetAllocatedSize` and the texture
//! pack's MIP filter override; `GETextureSet`'s grid arithmetic and `GEMaterial::SizeOf`. The textures, surfaces,
//! frames and reference counts stay C++.
//!
//! Not simulation code (the graphics thread and the UI load and draw textures), but the C++'s `f32` order and
//! `unsigned long` arithmetic are kept.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::ffi::{c_long, c_ulong};

/// `GESurfaceDescription::mType` of a plain texture and of one with a MIP chain.
pub const SURFACE_TEXTURE: c_ulong = 5;
pub const SURFACE_MIP_MAPPED_TEXTURE: c_ulong = 6;

/// `kMipFilterNone` and `kMipFilterPoint`.
pub const MIP_FILTER_NONE: c_ulong = 0;
pub const MIP_FILTER_POINT: c_ulong = 1;

/// The surface description `Load` builds for one-texture-per-frame textures (frame format 0): the MIP level count of
/// the file's width, the drop clamped to it, the type and level count of the frames' surfaces, and (with a drop) the
/// reduced surface they are copied into.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct FramePlan {
    /// `theMipLevels`: how often the width halves before it reaches 0.
    pub mip_levels: c_long,
    /// The levels to drop, clamped to `mip_levels - 1`.
    pub drop: c_long,
    pub surface_type: c_ulong,
    pub mip_level_count: c_long,
    /// `drop > 0`: the reduced surface's description replaces the caller's, and its level count is the texture's.
    pub has_drop: bool,
    pub drop_type: c_ulong,
    pub drop_mip_level_count: c_long,
    pub drop_width: c_ulong,
    pub drop_height: c_ulong,
}

/// `Load`'s description planning for frame format 0 over a `width` x `height` file.
pub fn frame_plan(width: u32, height: u32, create_mipmaps: bool, drop: c_long) -> FramePlan {
    let mut plan = FramePlan::default();
    let mut size = width as c_long;
    loop {
        size >>= 1;
        plan.mip_levels += 1;
        if size <= 0 {
            break;
        }
    }
    plan.drop = if plan.mip_levels - 1 < drop { plan.mip_levels - 1 } else { drop };
    if create_mipmaps {
        plan.surface_type = SURFACE_MIP_MAPPED_TEXTURE;
        plan.mip_level_count = plan.mip_levels;
    } else if plan.drop > 0 {
        plan.surface_type = SURFACE_MIP_MAPPED_TEXTURE;
        plan.mip_level_count = plan.drop + 1;
    } else {
        plan.surface_type = SURFACE_TEXTURE;
        plan.mip_level_count = 1;
    }
    if plan.drop > 0 {
        plan.has_drop = true;
        if plan.drop == plan.mip_levels - 1 {
            plan.drop_width = 1;
            plan.drop_height = 1;
            plan.drop_type = SURFACE_TEXTURE;
            plan.drop_mip_level_count = 1;
        } else if create_mipmaps {
            plan.drop_type = SURFACE_MIP_MAPPED_TEXTURE;
            plan.drop_width = (width >> plan.drop) as c_ulong;
            plan.drop_height = (height >> plan.drop) as c_ulong;
            plan.drop_mip_level_count = plan.mip_levels - plan.drop;
        } else {
            plan.drop_type = SURFACE_TEXTURE;
            plan.drop_mip_level_count = 1;
            plan.drop_width = (width >> plan.drop) as c_ulong;
            plan.drop_height = (height >> plan.drop) as c_ulong;
        }
    }
    plan
}

/// The frame grid of a one-surface texture (frame format 1): the smallest power-of-four cell count holding the
/// frames, its side (`mFramesPerRow`) and a cell's UV size (`mFrameUVSize`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct GridPlan {
    pub frames_per_row: c_ulong,
    pub uv_size: f32,
}

pub fn grid_plan(frame_count: u32) -> GridPlan {
    let mut grid_size: c_ulong = 1;
    while frame_count as c_ulong > grid_size {
        grid_size = grid_size.wrapping_mul(4);
    }
    let frames_per_row = (grid_size as f64).sqrt() as c_ulong;
    GridPlan { frames_per_row, uv_size: 1.0f32 / frames_per_row as f32 }
}

/// A grid cell: left, top, right, bottom in the grid surface's pixels, as `Load` computes it for frame `frame` of a
/// `mip_width` x `mip_height` level (`f32`, in the C++'s order).
pub fn grid_cell(frame: c_ulong, frames_per_row: c_ulong, mip_width: c_ulong, mip_height: c_ulong) -> [f32; 4] {
    let x = frame as f32 / frames_per_row as f32;
    let row = x as c_ulong;
    let left = (x - row as f32) * mip_width as f32 * frames_per_row as f32;
    let top = mip_height.wrapping_mul(row) as f32;
    let right = mip_width as f32 + left;
    let bottom = mip_height as f32 + top;
    [left, top, right, bottom]
}

/// `SetFrame`'s UV corner of frame `frame` in a grid of `frames_per_row`: (u, v).
pub fn frame_corner(frame: c_ulong, frames_per_row: c_ulong) -> (f32, f32) {
    let per_row = frames_per_row as f32;
    let x = frame as f32 / per_row;
    let row = (x as c_ulong) as f32;
    (x - row, row / per_row)
}

/// `GETexture::Dereference` at reference count `count`: whether to drop one reference, and whether a managed texture
/// then removes itself from its manager (at the last outside reference).
pub fn dereference_decision(count: c_ulong, managed: bool) -> (bool, bool) {
    if count > 2 {
        (true, false)
    } else {
        (count > 1, managed)
    }
}

/// `GetAllocatedSize`: the surface's size, times the frame count when each frame has its own texture.
pub fn allocated_size(surface_size: c_ulong, frame_format: c_ulong, frame_count: c_ulong) -> c_ulong {
    if frame_format == 0 {
        surface_size.wrapping_mul(frame_count)
    } else {
        surface_size
    }
}

/// `SetStageMipFilterMode`'s mode: a texture drawn smaller than its pixels (a texture pack's, logical shift above 0)
/// with MIP levels keeps point MIP filtering where none is asked for.
pub fn mip_filter_mode(mode: c_ulong, has_surface: bool, logical_shift: c_ulong, create_mipmaps: bool) -> c_ulong {
    if mode == MIP_FILTER_NONE && has_surface && logical_shift > 0 && create_mipmaps {
        MIP_FILTER_POINT
    } else {
        mode
    }
}

/// `GETextureSet::GetTexture`'s and `GetTextureIndices`' slot of tile (x, y): the texture column and row.
pub fn texture_set_slot(
    x: c_ulong,
    y: c_ulong,
    coverage: c_ulong,
    width: c_ulong,
    height: c_ulong,
) -> (c_ulong, c_ulong) {
    ((x / coverage) % width, (y / coverage) % height)
}

/// `GETextureSet::GetTextureCoordinate`: the tile's (u, v) within its texture.
pub fn texture_set_coordinate(x: c_ulong, y: c_ulong, coverage: c_ulong) -> (f32, f32) {
    ((x % coverage) as f32 / coverage as f32, (y % coverage) as f32 / coverage as f32)
}

/// `GEMaterial::SizeOf`: the object without its name and two words, plus the name's size, a texture slot per screen
/// and two polygon ranges per detail level.
pub fn material_size_of(
    object_size: usize,
    string_size: usize,
    name_size: c_ulong,
    screen_count: c_ulong,
    pointer_size: usize,
    lod_count: c_ulong,
    ulong_size: usize,
) -> c_ulong {
    (object_size
        .wrapping_sub(string_size)
        .wrapping_sub(8)
        .wrapping_add(name_size as usize)
        .wrapping_add((screen_count as usize).wrapping_mul(pointer_size))
        .wrapping_add((lod_count as usize).wrapping_mul(2).wrapping_mul(ulong_size))) as c_ulong
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_mip_level_count_is_the_halvings_to_zero() {
        assert_eq!(frame_plan(1, 1, false, 0).mip_levels, 1);
        assert_eq!(frame_plan(2, 2, false, 0).mip_levels, 2);
        assert_eq!(frame_plan(256, 256, false, 0).mip_levels, 9);
        assert_eq!(frame_plan(255, 1, false, 0).mip_levels, 8);
        assert_eq!(frame_plan(0, 0, false, 0).mip_levels, 1);
    }

    #[test]
    fn no_mipmaps_and_no_drop_is_a_plain_texture() {
        let p = frame_plan(64, 32, false, 0);
        assert_eq!(p.surface_type, SURFACE_TEXTURE);
        assert_eq!(p.mip_level_count, 1);
        assert!(!p.has_drop);
        assert_eq!(p.drop, 0);
    }

    #[test]
    fn mipmaps_give_the_whole_chain() {
        let p = frame_plan(64, 32, true, 0);
        assert_eq!(p.surface_type, SURFACE_MIP_MAPPED_TEXTURE);
        assert_eq!(p.mip_level_count, 7);
        assert!(!p.has_drop);
    }

    #[test]
    fn a_drop_without_mipmaps_builds_just_enough_levels_and_keeps_one() {
        let p = frame_plan(64, 32, false, 2);
        assert_eq!(p.surface_type, SURFACE_MIP_MAPPED_TEXTURE);
        assert_eq!(p.mip_level_count, 3);
        assert!(p.has_drop);
        assert_eq!(p.drop_type, SURFACE_TEXTURE);
        assert_eq!(p.drop_mip_level_count, 1);
        assert_eq!((p.drop_width, p.drop_height), (16, 8));
    }

    #[test]
    fn a_drop_with_mipmaps_keeps_the_rest_of_the_chain() {
        let p = frame_plan(64, 32, true, 2);
        assert_eq!(p.mip_level_count, 7);
        assert!(p.has_drop);
        assert_eq!(p.drop_type, SURFACE_MIP_MAPPED_TEXTURE);
        assert_eq!(p.drop_mip_level_count, 5);
        assert_eq!((p.drop_width, p.drop_height), (16, 8));
    }

    #[test]
    fn the_drop_is_clamped_and_the_last_level_is_one_pixel() {
        let p = frame_plan(16, 16, false, 10);
        assert_eq!(p.mip_levels, 5);
        assert_eq!(p.drop, 4);
        assert_eq!(p.mip_level_count, 5);
        assert!(p.has_drop);
        assert_eq!((p.drop_width, p.drop_height), (1, 1));
        assert_eq!(p.drop_type, SURFACE_TEXTURE);
        assert_eq!(p.drop_mip_level_count, 1);
        // the same with MIP maps asked for: still one plain pixel
        let q = frame_plan(16, 16, true, 4);
        assert_eq!(q.drop_type, SURFACE_TEXTURE);
        assert_eq!(q.drop_mip_level_count, 1);
        // a negative drop is no drop
        assert!(!frame_plan(16, 16, false, -1).has_drop);
    }

    #[test]
    fn the_grid_is_the_smallest_power_of_four() {
        assert_eq!(grid_plan(0).frames_per_row, 1);
        assert_eq!(grid_plan(1).frames_per_row, 1);
        assert_eq!(grid_plan(2).frames_per_row, 2);
        assert_eq!(grid_plan(4).frames_per_row, 2);
        assert_eq!(grid_plan(5).frames_per_row, 4);
        assert_eq!(grid_plan(16).frames_per_row, 4);
        assert_eq!(grid_plan(17).frames_per_row, 8);
        assert_eq!(grid_plan(4).uv_size, 0.5);
        assert_eq!(grid_plan(17).uv_size, 0.125);
    }

    #[test]
    fn grid_cells_walk_the_rows() {
        assert_eq!(grid_cell(0, 4, 32, 16), [0.0, 0.0, 32.0, 16.0]);
        assert_eq!(grid_cell(1, 4, 32, 16), [32.0, 0.0, 64.0, 16.0]);
        assert_eq!(grid_cell(4, 4, 32, 16), [0.0, 16.0, 32.0, 32.0]);
        assert_eq!(grid_cell(7, 4, 32, 16), [96.0, 16.0, 128.0, 32.0]);
        assert_eq!(grid_cell(15, 4, 1, 1), [3.0, 3.0, 4.0, 4.0]);
    }

    #[test]
    fn the_frame_corner_is_the_cell_in_uv() {
        assert_eq!(frame_corner(0, 2), (0.0, 0.0));
        assert_eq!(frame_corner(1, 2), (0.5, 0.0));
        assert_eq!(frame_corner(2, 2), (0.0, 0.5));
        assert_eq!(frame_corner(3, 2), (0.5, 0.5));
        assert_eq!(frame_corner(5, 4), (0.25, 0.25));
        assert_eq!(frame_corner(0, 1), (0.0, 0.0));
    }

    #[test]
    fn dereference_drops_until_the_managers_reference_is_the_last() {
        assert_eq!(dereference_decision(5, true), (true, false));
        assert_eq!(dereference_decision(3, false), (true, false));
        assert_eq!(dereference_decision(2, true), (true, true));
        assert_eq!(dereference_decision(2, false), (true, false));
        assert_eq!(dereference_decision(1, true), (false, true));
        assert_eq!(dereference_decision(1, false), (false, false));
    }

    #[test]
    fn allocated_size_counts_separate_frame_textures() {
        assert_eq!(allocated_size(1000, 0, 3), 3000);
        assert_eq!(allocated_size(1000, 1, 3), 1000);
    }

    #[test]
    fn the_pack_keeps_point_mip_filtering() {
        assert_eq!(mip_filter_mode(MIP_FILTER_NONE, true, 2, true), MIP_FILTER_POINT);
        assert_eq!(mip_filter_mode(MIP_FILTER_NONE, true, 0, true), MIP_FILTER_NONE);
        assert_eq!(mip_filter_mode(MIP_FILTER_NONE, true, 2, false), MIP_FILTER_NONE);
        assert_eq!(mip_filter_mode(MIP_FILTER_NONE, false, 2, true), MIP_FILTER_NONE);
        assert_eq!(mip_filter_mode(3, true, 2, true), 3);
    }

    #[test]
    fn texture_set_slots_tile_the_grid() {
        assert_eq!(texture_set_slot(0, 0, 4, 2, 3), (0, 0));
        assert_eq!(texture_set_slot(5, 13, 4, 2, 3), (1, 0));
        assert_eq!(texture_set_slot(9, 9, 4, 2, 3), (0, 2));
        assert_eq!(texture_set_coordinate(5, 7, 4), (0.25, 0.75));
        assert_eq!(texture_set_coordinate(8, 0, 4), (0.0, 0.0));
    }

    #[test]
    fn material_size_adds_the_name_and_the_arrays() {
        // the original's 32-bit layout: sizeof 0x48, UString 0x10, 4-byte pointers and longs
        assert_eq!(material_size_of(0x48, 0x10, 20, 1, 4, 1, 4), 0x48 - 0x10 - 8 + 20 + 4 + 8);
        assert_eq!(material_size_of(0x60, 0x18, 0, 2, 8, 3, 4), 0x60 - 0x18 - 8 + 16 + 24);
    }
}
