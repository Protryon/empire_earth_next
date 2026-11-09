//! `DX7Rasterizer DrawTerrain.cpp`: the terrain and water material draws, the grid and highlight overlays, the terrain
//! lighting and the camera-space transform with its polygon sort. `GETerrainMesh` keeps its data; the fixed visible
//! vertex arrays and the polygon records cross as pinned records (`RenderVertex`, `TerrainPolygon`, checked field by
//! field in the C++), the per-point height field and colours through [`TerrainHost`]. Golden: `golden_draw.txt`
//! (sections `terrain*`, `water_draw`, `gridlines`, `highlight_*`, `light`, `tocamera_*`), printed by a scratch run of
//! the original C++ bodies (with the real `UMath`).
#![allow(clippy::unnecessary_cast)]
// the C++ comparisons and operand orders are kept (NaN falls through `!(x > 0.0)`)
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::assign_op_pattern)]

use core::ffi::{c_long, c_ulong};

use crate::draw::*;
use crate::draw_model::{Plane, ViewportInfo};
use crate::vertex::{float_to_long, rgba, Dx7StdColorVertex};

/// `GETM_VISIBLE_POINTS`: the fixed visible-area arrays are `[N][N]`, indexed `[column][row]`.
pub const VISIBLE_POINTS: usize = 257;

pub const CLIP_NEAR: c_ulong = 1;
pub const CLIP_FAR: c_ulong = 2;
pub const CLIP_LEFT: c_ulong = 4;
pub const CLIP_RIGHT: c_ulong = 8;
pub const CLIP_TOP: c_ulong = 0x10;
pub const CLIP_BOTTOM: c_ulong = 0x20;

/// `GETMRenderVertex`, pinned.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct RenderVertex {
    pub point: [f32; 3],
    pub color: [f32; 3],
    pub visibility: f32,
    pub fog: f32,
    pub clip_flags: c_ulong,
    pub linked_visibility: *mut f32,
}

/// `GETMPolygon`, pinned.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TerrainPolygon {
    pub vertices: [*mut RenderVertex; 3],
    pub alpha: [f32; 3],
    pub uv: [[f32; 2]; 3],
}

/// The parts of one `GETerrainMesh` the draws read directly (plain data, valid during the forwarded call).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct TerrainMeshView {
    pub mesh: Handle,
    pub terrain_vertices: *mut RenderVertex,
    pub water_vertices: *mut RenderVertex,
    pub terrain_visible: *const bool,
    pub water_visible: *const bool,
    pub visible_tile_rows: c_ulong,
    pub start_x: c_ulong,
    pub start_y: c_ulong,
    pub width: c_ulong,
    pub height: c_ulong,
    pub water_percentage: f32,
    pub is_space: bool,
    pub edge_world: *const [f32; 3],
    pub edge_vertices: *const *mut RenderVertex,
    pub edge_count: c_ulong,
    pub cliff_world: *const [f32; 3],
    pub cliff_vertices: *const *mut RenderVertex,
    pub cliff_count: c_ulong,
}

/// One `(texture, material index)` of the terrain material map, in map order.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MaterialMapEntry {
    pub texture: Handle,
    pub index: c_ulong,
}

/// One `GETMMaterial` (its vectors' memory).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct MaterialView {
    pub texture: Handle,
    pub polygons: *const *mut TerrainPolygon,
    pub polygon_count: c_ulong,
    pub indices: *mut *mut TerrainPolygon,
    pub indices_length: c_ulong,
    pub strips: *mut *mut TerrainPolygon,
    pub strips_length: c_ulong,
}

/// One device light as `Light` gathers it.
#[repr(C)]
#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct LightData {
    pub light_type: c_ulong,
    pub diffuse: [f32; 3],
    pub direction: [f32; 3],
}

const LIGHT_DIRECTIONAL: c_ulong = 3;

/// The per-point reads, the mode changes and the device light queries.
pub trait TerrainHost: DrawHost {
    fn mesh_z(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> f32;
    fn mesh_normal(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3];
    fn mesh_vertex_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3];
    fn mesh_water_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3];
    fn mesh_valid_tile(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> bool;
    /// `mViewportChanged = true`.
    fn mark_viewport_changed(&mut self);
    /// `gLightsInUse[index]`: `None` when not in use, else `GetLight(index)`.
    fn light(&mut self, index: u32) -> Option<LightData>;
    fn um_arctan(&mut self, value: f32) -> f32;
    fn um_tan(&mut self, angle: f32) -> f32;
    fn um_sin(&mut self, angle: f32) -> f32;
    fn um_cos(&mut self, angle: f32) -> f32;
}

/// A material's polygon-list count (`DX7ListCount`): its last slot holds the count as a pointer value.
#[inline]
unsafe fn list_count(list: *mut *mut TerrainPolygon, length: c_ulong) -> u32 {
    (unsafe { *list.add(length as usize - 1) }) as usize as u32
}

#[inline]
unsafe fn visible_at(array: *const bool, column: usize, row: usize) -> bool {
    unsafe { *array.add(column * VISIBLE_POINTS + row) }
}

#[inline]
unsafe fn vertex_at(array: *mut RenderVertex, column: usize, row: usize) -> *mut RenderVertex {
    unsafe { array.add(column * VISIBLE_POINTS + row) }
}

/// `DrawTerrainMaterial`: a `NULL`-terminated polygon list (count in its last element) in batches of at most 0x960
/// vertices, with fog-darkened vertex colours; a triangle whose vertices are all fully visible with no alpha is
/// skipped. Returns the last unlock's result.
pub(crate) fn draw_terrain_material(
    h: &mut impl DrawHost,
    list: *mut *mut TerrainPolygon,
    length: c_ulong,
) -> LongResult {
    let mut result: LongResult = 0;
    let mut polygon = list as *const *mut TerrainPolygon;
    let mut remaining = unsafe { list_count(list, length) } * 3;
    while remaining > 0 {
        let mut batch = remaining.min(0x960);
        remaining -= batch;
        let mut vertex_count = 0;
        let locked = h.lock_vb(batch);
        let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
        while !unsafe { *polygon }.is_null() && batch > 0 {
            let triangle = unsafe { *polygon };
            let mut vertices = [core::ptr::null_mut::<RenderVertex>(); 3];
            let mut visibilities = [0.0f32; 3];
            let mut alphas = [0.0f32; 3];
            let mut is_visible = false;
            for i in 0..3 {
                let vertex = unsafe { (*triangle).vertices[i] };
                vertices[i] = vertex;
                let visibility = unsafe { (*vertex).visibility };
                visibilities[i] = if visibility >= 0.0 { visibility } else { unsafe { *(*vertex).linked_visibility } };
                let alpha = unsafe { (*triangle).alpha[i] };
                alphas[i] = if alpha >= 0.0 { alpha } else { 1.0 - visibilities[i] };
                if visibilities[i] < 1.0 || alphas[i] > 0.0 {
                    is_visible = true;
                }
            }
            if is_visible {
                for i in 0..3 {
                    let vertex = unsafe { *vertices[i] };
                    let color = if unsafe { (*triangle).alpha[i] } >= 0.0 {
                        rgba(
                            float_to_long(vertex.color[0] * visibilities[i] * 255.0),
                            float_to_long(vertex.color[1] * visibilities[i] * 255.0),
                            float_to_long(vertex.color[2] * visibilities[i] * 255.0),
                            float_to_long(alphas[i] * 255.0),
                        )
                    } else {
                        rgba(
                            float_to_long(0.0),
                            float_to_long(0.0),
                            float_to_long(0.0),
                            float_to_long(alphas[i] * 255.0),
                        )
                    };
                    let uv = unsafe { (*triangle).uv[i] };
                    unsafe {
                        *buffer = Dx7StdColorVertex {
                            x: vertex.point[0],
                            y: vertex.point[1],
                            z: vertex.point[2],
                            nx: 0.0,
                            ny: 0.0,
                            nz: 0.0,
                            color,
                            tu: uv[0],
                            tv: uv[1],
                        };
                        buffer = buffer.add(1);
                    }
                }
                vertex_count += 3;
            }
            polygon = unsafe { polygon.add(1) };
            batch -= 3;
        }
        result = h.unlock_vb();
        if vertex_count > 0 {
            let start = h.vb_position();
            draw_primitive_retry(h, PT_TRIANGLELIST, start, vertex_count);
        }
        h.advance_vb(vertex_count);
    }
    result
}

fn draw_one_material(h: &mut impl DrawHost, texture: Handle, m: &MaterialView) -> LongResult {
    h.add_triangles(unsafe { list_count(m.indices, m.indices_length) });
    h.add_triangles(unsafe { list_count(m.strips, m.strips_length) });
    h.set_texture(texture);
    h.set_render_state(RS_CLIPPING, 0);
    draw_terrain_material(h, m.indices, m.indices_length);
    h.set_render_state(RS_CLIPPING, 1);
    draw_terrain_material(h, m.strips, m.strips_length)
}

/// `Draw(GETerrainMesh&)`: the terrain texture by texture, unclipped then clipped polygons (space maps draw materials
/// 1..8 first); with no visible tile row, two black quads fill the view instead.
pub fn draw_terrain(
    h: &mut impl TerrainHost,
    mesh: &TerrainMeshView,
    map: &[MaterialMapEntry],
    materials: &[MaterialView],
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    let mut result = set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
    if mesh.visible_tile_rows != 0 {
        if mesh.is_space {
            for entry in map {
                if entry.index < 9 && entry.index > 0 {
                    result = draw_one_material(h, entry.texture, &materials[entry.index as usize]);
                }
            }
        }
        for entry in map {
            if !mesh.is_space || entry.index >= 9 || entry.index == 0 {
                result = draw_one_material(h, entry.texture, &materials[entry.index as usize]);
            }
        }
        h.mark_viewport_changed();
        result
    } else {
        draw_terrain_black(h, viewport);
        0
    }
}

/// The no-visible-tile case of `Draw(GETerrainMesh&)`: two black quads in front of the camera.
fn draw_terrain_black(h: &mut impl DrawHost, viewport: &ViewportInfo) {
    const BLACK: u32 = rgba(0, 0, 0, 255);
    let camera = &viewport.camera;
    let inverse = &viewport.camera_inverse;
    let mut forward = apply_vector(camera, [0.0, 1.0, 0.0]);
    forward[2] = 0.0;
    forward = normalize(forward);
    let mut right = apply_vector(camera, [1.0, 0.0, 0.0]);
    right[2] = 0.0;
    right = normalize(right);
    let x = camera[0][3];
    let y = camera[1][3];
    let scale = |v: [f32; 3], s: f32| [s * v[0], s * v[1], s * v[2]];
    let offset = scale(right, -1000.0);
    let p0 = [offset[0] + x, offset[1] + y, offset[2]];
    let offset = scale(right, 1000.0);
    let p1 = [offset[0] + x, offset[1] + y, offset[2]];
    let offset = scale(forward, 7.071067);
    let (cx, cy, cz) = (offset[0] + x, offset[1] + y, offset[2]);
    let offset = scale(right, -1000.0);
    let p2 = [offset[0] + cx, offset[1] + cy, offset[2] + cz];
    let offset = scale(right, 1000.0);
    let p3 = [offset[0] + cx, offset[1] + cy, offset[2] + cz];
    let rear = viewport.rear_clip;
    let offset = scale(forward, (rear * rear + rear * rear).sqrt());
    let (cx, cy, cz) = (offset[0] + x, offset[1] + y, offset[2]);
    let offset = scale(right, -1000.0);
    let p4 = [offset[0] + cx, offset[1] + cy, offset[2] + cz];
    let offset = scale(right, 1000.0);
    let p5 = [offset[0] + cx, offset[1] + cy, offset[2] + cz];
    let world = [p0, p1, p2, p3, p4, p5];
    let camera_points: [[f32; 3]; 6] = core::array::from_fn(|i| apply_point(inverse, world[i]));
    h.add_triangles(4);
    h.set_texture(core::ptr::null_mut());
    h.set_render_state(RS_CLIPPING, 1);
    let locked = h.lock_vb(12);
    let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
    for n in [2usize, 3, 0, 3, 1, 0, 4, 5, 2, 5, 3, 2] {
        let p = camera_points[n];
        unsafe {
            *buffer = Dx7StdColorVertex {
                x: p[0],
                y: p[1],
                z: p[2],
                nx: 0.0,
                ny: 0.0,
                nz: 0.0,
                color: BLACK,
                tu: 0.0,
                tv: 0.0,
            };
            buffer = buffer.add(1);
        }
    }
    h.unlock_vb();
    let start = h.vb_position();
    draw_primitive_retry(h, PT_TRIANGLELIST, start, 12);
    h.advance_vb(12);
}

/// `DrawWater`: every water material, unclipped then clipped polygons, its triangles counted.
pub fn draw_water(
    h: &mut impl DrawHost,
    mesh: &TerrainMeshView,
    materials: &[MaterialView],
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    let mut result: LongResult = 0;
    if mesh.visible_tile_rows != 0 {
        result = set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
        for m in materials {
            h.add_triangles(unsafe { list_count(m.indices, m.indices_length) });
            h.add_triangles(unsafe { list_count(m.strips, m.strips_length) });
            h.set_texture(m.texture);
            h.set_render_state(RS_CLIPPING, 0);
            draw_terrain_material(h, m.indices, m.indices_length);
            h.set_render_state(RS_CLIPPING, 1);
            result = draw_terrain_material(h, m.strips, m.strips_length);
        }
    }
    result
}

/// Sets `mViewportChanged` after a water draw (`DrawWater`'s last statement; separate so the unit test of the draw needs
/// only [`DrawHost`]).
pub fn draw_water_and_mark(
    h: &mut impl TerrainHost,
    mesh: &TerrainMeshView,
    materials: &[MaterialView],
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    let result = draw_water(h, mesh, materials, viewport, orthographic);
    if mesh.visible_tile_rows != 0 {
        h.mark_viewport_changed();
    }
    result
}

/// `DrawGridLines`: lines between tile corners whose both ends are visible, as one red line list of at most 1200
/// lines. Each line in `lines` is `(x1, y1, x2, y2)`; the vertex-buffer position advances by the unfiltered count.
#[allow(clippy::too_many_arguments)]
pub fn draw_grid_lines(
    h: &mut impl TerrainHost,
    mesh: &TerrainMeshView,
    lines: &[[c_long; 4]],
    texture: Handle,
    width: f32,
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    let mut line_count = lines.len() as u32;
    let mut vertex_count = line_count.wrapping_mul(2);
    let end_v = width + 1.0;
    let mut drawn: u32 = 0;
    if mesh.visible_tile_rows == 0 {
        return 0;
    }
    set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
    h.set_texture(texture);
    h.set_material(&Material::WHITE);
    h.set_shading_color_source(0);
    let end_x = mesh.width.wrapping_add(mesh.start_x).wrapping_add(1);
    let end_y = mesh.height.wrapping_add(mesh.start_y).wrapping_add(1);
    if vertex_count > 0x960 {
        vertex_count = 0x960;
        line_count = 0x4b0;
    }
    let locked = h.lock_vb(vertex_count);
    let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
    let in_range = |v: c_ulong, start: c_ulong, end: c_ulong| v >= start && v < end;
    for line in &lines[..line_count as usize] {
        let (x1, y1, x2, y2) = (line[0] as c_ulong, line[1] as c_ulong, line[2] as c_ulong, line[3] as c_ulong);
        if !in_range(x1, mesh.start_x, end_x) || !in_range(x2, mesh.start_x, end_x) {
            continue;
        }
        if !in_range(y1, mesh.start_y, end_y) || !in_range(y2, mesh.start_y, end_y) {
            continue;
        }
        let (c1, r1) = ((x1 - mesh.start_x) as usize, (y1 - mesh.start_y) as usize);
        let (c2, r2) = ((x2 - mesh.start_x) as usize, (y2 - mesh.start_y) as usize);
        if !unsafe { visible_at(mesh.terrain_visible, c1, r1) } || !unsafe { visible_at(mesh.terrain_visible, c2, r2) }
        {
            continue;
        }
        let z1 = h.mesh_z(mesh.mesh, x1, y1);
        let p1 = apply_point(&viewport.camera_inverse, [x1 as f32, y1 as f32, if z1 >= 0.0 { z1 } else { 0.0 }]);
        let z2 = h.mesh_z(mesh.mesh, x2, y2);
        let p2 = apply_point(&viewport.camera_inverse, [x2 as f32, y2 as f32, if z2 >= 0.0 { z2 } else { 0.0 }]);
        unsafe {
            *buffer = red_vertex(p1, width);
            *buffer.add(1) = red_vertex(p2, end_v);
            buffer = buffer.add(2);
        }
        drawn += 1;
    }
    let result = h.unlock_vb();
    if drawn > 0 {
        let start = h.vb_position();
        draw_primitive_retry(h, PT_LINELIST, start, drawn * 2);
    }
    h.advance_vb(vertex_count);
    result
}

const RED: u32 = rgba(255, 0, 0, 255);

fn red_vertex(p: [f32; 3], tv: f32) -> Dx7StdColorVertex {
    Dx7StdColorVertex { x: p[0], y: p[1], z: p[2], nx: 0.0, ny: 0.0, nz: 0.0, color: RED, tu: 0.0, tv }
}

/// `DrawTerrainGrid`: two red edge lines for every visible tile, one draw call per tile.
pub fn draw_terrain_grid(
    h: &mut impl DrawHost,
    mesh: &TerrainMeshView,
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    if mesh.visible_tile_rows == 0 {
        return 0;
    }
    let mut result = set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
    let end_x = mesh.width.wrapping_add(mesh.start_x).wrapping_add(1);
    let end_y = mesh.height.wrapping_add(mesh.start_y).wrapping_add(1);
    let (vis, verts) = (mesh.terrain_visible, mesh.terrain_vertices);
    for y in mesh.start_y..end_y {
        for x in mesh.start_x..end_x {
            let (c, r) = ((x - mesh.start_x) as usize, (y - mesh.start_y) as usize);
            if unsafe { visible_at(vis, c, r) && visible_at(vis, c + 1, r) && visible_at(vis, c, r + 1) } {
                let locked = h.lock_vb(4);
                let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
                for (dx, dy) in [(0, 0), (0, 1), (0, 0), (1, 0)] {
                    let p = unsafe { (*vertex_at(verts, c + dx, r + dy)).point };
                    unsafe {
                        *buffer = red_vertex(p, 0.0);
                        buffer = buffer.add(1);
                    }
                }
                result = h.unlock_vb();
                let start = h.vb_position();
                draw_primitive_retry(h, PT_LINELIST, start, 4);
                h.advance_vb(4);
            }
        }
    }
    result
}

/// `DrawTerrainHighlight`: shades a tile rectangle with a translucent colour over every valid visible tile, in batches
/// of 400 quads. `rect` is `left, top, right, bottom` tile indices (inclusive).
#[allow(clippy::too_many_arguments)]
pub fn draw_terrain_highlight(
    h: &mut impl TerrainHost,
    mesh: &TerrainMeshView,
    rect: [c_long; 4],
    color: [f32; 3],
    opacity: f32,
    viewport: &ViewportInfo,
    orthographic: bool,
) -> LongResult {
    let (right, left, bottom, top) = (rect[2] as c_ulong, rect[0] as c_ulong, rect[3] as c_ulong, rect[1] as c_ulong);
    let mut quad_count: c_long = 0;
    let mut width = right.wrapping_sub(left) as c_long;
    if width < 0 {
        width = -width;
    }
    let mut height = bottom.wrapping_sub(top) as c_long;
    if height < 0 {
        height = -height;
    }
    let mut remaining: c_long = (height + 1).wrapping_mul(width + 1);
    if mesh.visible_tile_rows == 0 {
        return 0;
    }
    let packed = rgba(
        float_to_long(color[0] * 255.0),
        float_to_long(color[1] * 255.0),
        float_to_long(color[2] * 255.0),
        float_to_long(opacity * 255.0),
    );
    h.set_texture(core::ptr::null_mut());
    set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
    let end_x = mesh.width.wrapping_add(mesh.start_x).wrapping_add(1);
    let end_y = mesh.height.wrapping_add(mesh.start_y).wrapping_add(1);
    let lock_size = |remaining: c_long| 400.min(remaining) * 6;
    let locked = h.lock_vb(lock_size(remaining) as u32);
    let mut buffer = unsafe { locked.add(h.vb_position() as usize) };
    let (vis, verts) = (mesh.terrain_visible, mesh.terrain_vertices);
    let mut y = top;
    while y <= bottom {
        let row = y.wrapping_sub(mesh.start_y) as usize;
        let mut x = left;
        while x <= right {
            let column = x.wrapping_sub(mesh.start_x) as usize;
            remaining -= 1;
            if h.mesh_valid_tile(mesh.mesh, x, y)
                && x >= mesh.start_x
                && x <= end_x
                && y >= mesh.start_y
                && y <= end_y
                && unsafe {
                    visible_at(vis, column, row)
                        && visible_at(vis, column + 1, row)
                        && visible_at(vis, column, row + 1)
                        && visible_at(vis, column + 1, row + 1)
                }
            {
                for (dx, dy) in [(0usize, 0usize), (0, 1), (1, 0), (0, 1), (1, 1), (1, 0)] {
                    let p = unsafe { (*vertex_at(verts, column + dx, row + dy)).point };
                    unsafe {
                        *buffer = Dx7StdColorVertex {
                            x: p[0],
                            y: p[1],
                            z: p[2],
                            nx: 0.0,
                            ny: 0.0,
                            nz: 0.0,
                            color: packed,
                            tu: 0.0,
                            tv: 0.0,
                        };
                        buffer = buffer.add(1);
                    }
                }
                quad_count += 1;
                if quad_count == 400 && remaining > 0 {
                    h.unlock_vb();
                    let start = h.vb_position();
                    draw_primitive_retry(h, PT_TRIANGLELIST, start, 0x960);
                    quad_count = 0;
                    h.advance_vb(0x960);
                    let locked = h.lock_vb(lock_size(remaining) as u32);
                    buffer = unsafe { locked.add(h.vb_position() as usize) };
                }
            }
            x = x.wrapping_add(1);
        }
        y = y.wrapping_add(1);
    }
    let result = h.unlock_vb();
    if quad_count > 0 {
        let start = h.vb_position();
        draw_primitive_retry(h, PT_TRIANGLELIST, start, (quad_count * 6) as u32);
        h.advance_vb((quad_count * 6) as u32);
    }
    result
}

fn clamp_color(c: [f32; 3]) -> [f32; 3] {
    [clamp_unit(c[0]), clamp_unit(c[1]), clamp_unit(c[2])]
}

/// `Light`: ambient plus each active directional light, normalised, times the vertex colour, for the visible terrain
/// and water vertices. `lights` is what `DrawHost` gathered; `Light` is split so its math tests without a device.
pub fn light_terrain(h: &mut impl TerrainHost, mesh: &TerrainMeshView, ambient: [f32; 3]) {
    if mesh.visible_tile_rows == 0 {
        return;
    }
    let mut lights: Vec<([f32; 3], [f32; 3])> = Vec::with_capacity(8);
    for i in 0..8 {
        let Some(l) = h.light(i) else { continue };
        if l.light_type != LIGHT_DIRECTIONAL || (l.diffuse[0] == 0.0 && l.diffuse[1] == 0.0 && l.diffuse[2] == 0.0) {
            continue;
        }
        lights.push((l.diffuse, [-l.direction[0], -l.direction[1], -l.direction[2]]));
    }
    let mut water_light = ambient;
    for (color, direction) in &lights {
        let intensity = direction[2];
        if !(intensity > 0.0) {
            continue;
        }
        water_light[0] = color[0] * intensity + water_light[0];
        water_light[1] = color[1] * intensity + water_light[1];
        water_light[2] = color[2] * intensity + water_light[2];
    }
    let water_light = clamp_color(water_light);
    let end_x = mesh.width.wrapping_add(mesh.start_x).wrapping_add(2);
    let end_y = mesh.height.wrapping_add(mesh.start_y).wrapping_add(2);
    let mut y = mesh.start_y;
    while y < end_y {
        let row = (y - mesh.start_y) as usize;
        let mut x = mesh.start_x;
        while x < end_x {
            let column = (x - mesh.start_x) as usize;
            if !unsafe { visible_at(mesh.terrain_visible, column, row) } {
                x += 1;
                continue;
            }
            let normal = h.mesh_normal(mesh.mesh, x, y);
            let mut terrain_light = ambient;
            for (color, direction) in &lights {
                let intensity = normal[2] * direction[2] + normal[1] * direction[1] + normal[0] * direction[0];
                if !(intensity > 0.0) {
                    continue;
                }
                terrain_light[0] = color[0] * intensity + terrain_light[0];
                terrain_light[1] = color[1] * intensity + terrain_light[1];
                terrain_light[2] = color[2] * intensity + terrain_light[2];
            }
            let terrain_light = clamp_color(terrain_light);
            let color = h.mesh_vertex_color(mesh.mesh, x, y);
            unsafe {
                (*vertex_at(mesh.terrain_vertices, column, row)).color =
                    [color[0] * terrain_light[0], color[1] * terrain_light[1], color[2] * terrain_light[2]];
            }
            if unsafe { visible_at(mesh.water_visible, column, row) } {
                let water = h.mesh_water_color(mesh.mesh, x, y);
                unsafe {
                    (*vertex_at(mesh.water_vertices, column, row)).color =
                        [water[0] * water_light[0], water[1] * water_light[1], water[2] * water_light[2]];
                }
            }
            x += 1;
        }
        y += 1;
    }
}

/// The ripple tunables (`mWaterRipple0..2`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct WaterRipple {
    pub time_rate: f32,
    pub water_height: f32,
    pub terrain_height: f32,
}

struct ClipPlanes {
    near: f32,
    far: f32,
    left: Plane,
    right: Plane,
    top: Plane,
    bottom: Plane,
    ortho: bool,
    ortho_left: f32,
    ortho_right: f32,
    top_z: f32,
    bottom_z: f32,
}

impl ClipPlanes {
    fn flags(&self, p: [f32; 3]) -> c_ulong {
        let mut flags: c_ulong = 0;
        if p[1] <= self.near {
            flags |= CLIP_NEAR;
        }
        if p[1] >= self.far {
            flags |= CLIP_FAR;
        }
        if self.ortho {
            if p[0] <= self.ortho_left {
                flags |= CLIP_LEFT;
            }
            if p[0] >= self.ortho_right {
                flags |= CLIP_RIGHT;
            }
            if p[2] >= self.top_z {
                flags |= CLIP_TOP;
            }
            if p[2] <= self.bottom_z {
                flags |= CLIP_BOTTOM;
            }
        } else {
            if self.left.on_or_below(p) {
                flags |= CLIP_LEFT;
            }
            if self.right.on_or_below(p) {
                flags |= CLIP_RIGHT;
            }
            if self.top.on_or_below(p) {
                flags |= CLIP_TOP;
            }
            if self.bottom.on_or_below(p) {
                flags |= CLIP_BOTTOM;
            }
        }
        flags
    }
}

unsafe fn sort_material_polygons(material: &MaterialView) {
    let mut strip_count: u32 = 0;
    let mut index_count: u32 = 0;
    for j in 0..material.polygon_count as usize {
        let polygon = unsafe { *material.polygons.add(j) };
        let flags = unsafe {
            [
                (*(*polygon).vertices[0]).clip_flags,
                (*(*polygon).vertices[1]).clip_flags,
                (*(*polygon).vertices[2]).clip_flags,
            ]
        };
        let clipped = (flags[0] != 0) as u32 + (flags[1] != 0) as u32 + (flags[2] != 0) as u32;
        if clipped == 0 {
            unsafe { *material.indices.add(index_count as usize) = polygon };
            index_count += 1;
        } else if clipped != 3 || (flags[0] & flags[1] & flags[2]) == 0 || (flags[0] & flags[1] & flags[2]) == 0x3f {
            unsafe { *material.strips.add(strip_count as usize) = polygon };
            strip_count += 1;
        }
    }
    unsafe {
        *material.strips.add(strip_count as usize) = core::ptr::null_mut();
        *material.indices.add(index_count as usize) = core::ptr::null_mut();
        *material.strips.add(material.strips_length as usize - 1) = strip_count as usize as *mut TerrainPolygon;
        *material.indices.add(material.indices_length as usize - 1) = index_count as usize as *mut TerrainPolygon;
    }
}

/// `TransformToCamera`: moves the visible terrain and water vertices to camera space (animating the water waves), sets
/// their clip flags, then sorts each material's polygons into unclipped and clipped lists (water-only mode skips dry
/// vertices). `water_time` is the `sWaterTime` static the C++ keeps.
#[allow(clippy::too_many_arguments)]
pub fn transform_to_camera(
    h: &mut impl TerrainHost,
    mesh: &TerrainMeshView,
    terrain_materials: &[MaterialView],
    water_materials: &[MaterialView],
    viewport: &ViewportInfo,
    scale: f32,
    water_only: bool,
    orthographic: bool,
    ripple: WaterRipple,
    water_time: &mut f32,
) {
    if mesh.visible_tile_rows == 0 {
        return;
    }
    let inverse = &viewport.camera_inverse;
    let angle = {
        let t = h.um_tan(viewport.field_of_view * 0.5);
        h.um_arctan(viewport.half_height * t / (viewport.half_width - 1.0))
    };
    let mut normal = [0.0f32; 3];
    normal[1] = h.um_sin(angle);
    normal[2] = h.um_cos(angle);
    normal = normalize(normal);
    let bottom = Plane { point: [0.0; 3], normal };
    let top = Plane { point: [0.0; 3], normal: [normal[0], normal[1], -normal[2]] };
    if (ripple.time_rate == 0.0 || mesh.water_percentage == 0.0) && water_only {
        return;
    }
    *water_time += scale * ripple.time_rate;
    let time = *water_time;
    let planes = ClipPlanes {
        near: viewport.front_clip,
        far: viewport.rear_clip,
        left: viewport.left_plane,
        right: viewport.right_plane,
        top,
        bottom,
        ortho: orthographic,
        ortho_left: viewport.ortho_left,
        ortho_right: viewport.ortho_right,
        top_z: -(viewport.y_clip_plane * viewport.ortho_top),
        bottom_z: -(viewport.y_clip_plane * viewport.ortho_bottom),
    };
    let end_x = mesh.width.wrapping_add(mesh.start_x).wrapping_add(2);
    let end_y = mesh.height.wrapping_add(mesh.start_y).wrapping_add(2);
    let mut y = mesh.start_y;
    while y < end_y {
        let mut x = mesh.start_x;
        while x < end_x {
            let (column, row) = ((x - mesh.start_x) as usize, (y - mesh.start_y) as usize);
            let (fx, fy) = (x as f32, y as f32);
            if !unsafe { visible_at(mesh.terrain_visible, column, row) } {
                x += 1;
                continue;
            }
            let z = h.mesh_z(mesh.mesh, x, y);
            let water_visible = unsafe { visible_at(mesh.water_visible, column, row) };
            let wave = (fx * fy + time).sin();
            let terrain_vertex = unsafe { vertex_at(mesh.terrain_vertices, column, row) };
            if water_only {
                if !(z < 0.0) && !water_visible {
                    x += 1;
                    continue;
                }
                if z < -0.5 {
                    let camera = apply_point(inverse, [fx, fy, z - wave * ripple.terrain_height]);
                    unsafe {
                        (*terrain_vertex).point = camera;
                        (*terrain_vertex).clip_flags = planes.flags(camera);
                    }
                }
            } else {
                let terrain_z = if z >= -0.5 { z } else { z - wave * ripple.terrain_height };
                let camera = apply_point(inverse, [fx, fy, terrain_z]);
                unsafe {
                    (*terrain_vertex).point = camera;
                    (*terrain_vertex).clip_flags = planes.flags(camera);
                }
            }
            if water_visible {
                let water_z =
                    if z < 0.0 { wave * ripple.water_height } else { (wave - 1.0) * ripple.water_height * 0.5 };
                let camera = apply_point(inverse, [fx, fy, water_z]);
                let water_vertex = unsafe { vertex_at(mesh.water_vertices, column, row) };
                unsafe {
                    (*water_vertex).point = camera;
                    (*water_vertex).clip_flags = planes.flags(camera);
                }
            }
            x += 1;
        }
        y += 1;
    }
    if !water_only {
        for i in 0..mesh.edge_count as usize {
            let world = unsafe { *mesh.edge_world.add(i) };
            unsafe { (**mesh.edge_vertices.add(i)).point = apply_point(inverse, world) };
        }
        for i in 0..mesh.cliff_count as usize {
            let world = unsafe { *mesh.cliff_world.add(i) };
            unsafe { (**mesh.cliff_vertices.add(i)).point = apply_point(inverse, world) };
        }
    }
    for material in terrain_materials.iter().chain(water_materials) {
        unsafe { sort_material_polygons(material) };
    }
}

#[cfg(test)]
mod tests;
