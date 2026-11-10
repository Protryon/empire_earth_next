//! `GETerrainMesh::UpdateVisibleTerrain` (`GETerrainMesh.cpp`) as a plan: which tile rows the camera sees, the
//! terrain and water polygons with their vertices, alphas, texture coordinates and material, the materials as texture
//! handles in the order the C++ built its lists, the map-edge skirts and the space-map cliffs. The C++ keeps the
//! mesh, its render-vertex arrays, the material lists keyed by `GETexture*`, the reference counts and the polygon
//! arrays: it maps the plan's material indices to its lists and its vertex references to pointers (`GETerrainMesh.cpp`,
//! the fill after `ee_ge_terrain_visible_plan`).
//!
//! The graphics thread calls this once a frame; it is not simulation code, but the C++'s `f32` order, its `long` and
//! `unsigned long` arithmetic (`c_long`, `c_ulong`) and its quirks are kept: a tile layer's texture is chosen by the
//! lowest handle (the original compared `GETexture*` addresses), a skirt polygon leaves its slot's texture
//! coordinates as they were, and a material map entry is the first list entry with that texture.

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;
use std::ffi::{c_long, c_ulong};

use crate::terrain::{floor, GeTerrainView, GeTmRow};
use crate::terrain_mesh::GeTmTile;
use crate::terrain_volume::tile_rows_in_volume_raw;
use crate::texture::texture_set_slot;
use crate::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use crate::viewport::GeViewport;

/// A registered texture set as the plan reads it: its textures as handles (`GETexture*` values, 0 for none) in
/// column-major order (`mTextures[x][y]` at `x * height + y`), its grid and its tile coverage.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeTextureSetView {
    pub textures: *const usize,
    pub width: c_ulong,
    pub height: c_ulong,
    pub coverage: c_ulong,
}

/// What `UpdateVisibleTerrain` reads of a `GETerrainMesh`.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct GeVisibleInput {
    pub view: GeTerrainView,
    pub point_row_count: c_ulong,
    pub left_edge: c_long,
    pub top_edge: c_long,
    pub right_edge: c_long,
    pub bottom_edge: c_long,
    pub space_tile_count: c_ulong,
    pub texture_sets: *const GeTextureSetView,
    pub texture_set_count: usize,
    pub space_cliff_textures: *const usize,
    pub space_cliff_texture_count: usize,
    /// `mWaterTextureIndices[33][33]`, row-major.
    pub water_texture_indices: *const c_ulong,
    /// `GETM_VISIBLE_POINTS`: the side of the visible point arrays.
    pub side: c_ulong,
}

/// A polygon corner: a render vertex of the terrain array (`kind` 0, `[a][b]`), of the water array (1), a skirt
/// vertex (2, index `a`) or a cliff vertex (3, index `a`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GeVisibleVertex {
    pub kind: u32,
    pub a: u32,
    pub b: u32,
}

/// A planned polygon: its corners, alphas, texture coordinates (left as the polygon slot had them when `uv_set` is
/// false: the skirts) and material index.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct GeVisiblePolygon {
    pub vertices: [GeVisibleVertex; 3],
    pub alpha: [f32; 3],
    pub uv: [[f32; 2]; 3],
    pub uv_set: u32,
    pub material: u32,
}

/// The plan of one frame.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct VisiblePlan {
    /// The visible tile rows (y, first x, last x), clipped to the point arrays.
    pub rows: Vec<c_long>,
    pub bottom_left_x: c_ulong,
    pub bottom_left_y: c_ulong,
    pub tile_width: c_ulong,
    pub tile_height: c_ulong,
    pub num_visible_tiles: c_ulong,
    pub visible_poly_count: c_ulong,
    /// `side * side` flags each, `[a][b]` at `a * side + b`.
    pub terrain_visible: Vec<u8>,
    pub water_visible: Vec<u8>,
    /// The terrain materials' textures (0 for the untextured first one); a material's index is its position.
    pub materials: Vec<usize>,
    pub terrain_polygons: Vec<GeVisiblePolygon>,
    /// Water polygons; their material is an index into the mesh's water material list.
    pub water_polygons: Vec<GeVisiblePolygon>,
    /// The skirt vertices' world points, in creation order.
    pub edge_points: Vec<Ge3DPoint>,
    /// The cliff vertices' world points, in creation order.
    pub cliff_points: Vec<Ge3DPoint>,
    /// Per cliff vertex, the terrain render vertex (`[a][b]`) whose visibility it follows.
    pub cliff_links: Vec<u32>,
}

/// `GETerrainMesh::mTerrainPolygons`' and `mWaterPolygons`' sizes.
pub const TERRAIN_POLYGON_CAPACITY: usize = 12000;
pub const WATER_POLYGON_CAPACITY: usize = 5000;

const SHORELINE_MASK: u8 = 0x1e;
const WATER_MASK: u8 = 0x7f;
const WATER_GRID: c_long = 33;
const NONE: c_ulong = !0;

/// Per-cell height jitter of the cliff skirts' lower edge (`sCliffNoise`).
#[rustfmt::skip]
const CLIFF_NOISE: [[f32; 16]; 16] = [
    [-0.997, 0.127, -0.613, 0.617, 0.17, -0.04, -0.299, 0.792, 0.646, 0.493, -0.652, 0.718, 0.421, 0.027, -0.392, -0.97],
    [-0.817, -0.271, -0.705, -0.668, 0.977, -0.109, -0.762, -0.991, -0.982, -0.244, 0.063, 0.142, 0.204, 0.214, -0.668, 0.326],
    [-0.098, -0.296, -0.886, 0.215, 0.567, 0.605, 0.04, -0.396, 0.752, 0.453, 0.912, 0.851, 0.079, -0.715, -0.076, -0.529],
    [0.724, -0.581, 0.559, 0.687, 0.994, 0.999, 0.223, -0.215, -0.468, -0.405, 0.68, -0.953, -0.248, -0.815, 0.354, -0.888],
    [-0.982, 0.838, -0.448, -0.454, 0.176, 0.382, 0.675, 0.453, -0.03, -0.589, 0.487, -0.063, -0.084, 0.898, 0.489, -0.783],
    [0.198, -0.23, 0.47, 0.218, 0.145, -0.277, -0.697, -0.55, -0.15, 0.606, 0.034, 0.98, 0.503, -0.309, -0.662, 0.315],
    [-0.016, -0.873, 0.4, 0.01, -0.705, 0.899, -0.717, 0.81, 0.386, -0.394, -0.147, -0.859, 0.933, 0.366, -0.694, 0.755],
    [0.643, 0.164, -0.617, -0.644, 0.634, -0.049, -0.689, 0.008, 0.464, -0.189, -0.441, 0.137, 0.364, 0.512, 0.444, -0.049],
    [-0.754, -0.264, 0.669, -0.93, 0.034, 0.326, -0.148, -0.791, 0.899, 0.843, 0.099, -0.308, -0.057, -0.25, 0.694, -0.366],
    [-0.088, -0.456, 0.966, -0.404, 0.478, 0.135, -0.608, 0.523, 0.679, -0.205, 0.002, 0.78, -0.945, 0.989, 0.145, -0.899],
    [0.063, -0.612, 0.686, 0.254, 0.315, -0.604, 0.684, -0.753, -0.78, 0.486, -0.372, 0.882, -0.428, -0.327, -0.719, 0.466],
    [0.669, 0.416, 0.2, 0.494, -0.495, -0.711, -0.997, -0.878, 0.612, 0.705, -0.579, -0.769, 0.106, -0.971, -0.772, -0.091],
    [0.504, 0.372, 0.087, -0.852, -0.127, -0.596, 0.392, -0.419, -0.127, -0.535, 0.156, 0.065, 0.257, -0.68, 0.008, 0.926],
    [0.392, 0.85, -0.62, -0.328, -0.643, 0.99, -0.085, 0.996, -0.805, 0.25, -0.811, -0.125, 0.863, -0.903, 0.789, -0.42],
    [-0.545, 0.538, -0.179, -0.596, 0.256, 0.208, -0.097, -0.067, 0.196, 0.269, 0.71, 0.658, 0.25, 0.442, 0.132, -0.25],
    [-0.631, 0.476, 0.11, 0.81, -0.514, -0.622, 0.209, 0.397, 0.169, -0.297, -0.011, -0.839, 0.481, 0.224, 0.241, 0.382],
];

/// A space tile's shoreline drawing, per shoreline code (`sShorelines`): the cliff material offset, the shore
/// texture's corner UVs, the cliff orientation and the cliff end points.
struct Shoreline {
    material_offset: c_long,
    uv: [[f32; 2]; 4],
    cliff_type: c_long,
    cliff_offsets: [f32; 4],
}

#[rustfmt::skip]
const SHORELINES: [Shoreline; 16] = [
    Shoreline { material_offset: 0, uv: [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
    Shoreline { material_offset: 0, uv: [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]], cliff_type: 5, cliff_offsets: [0.4, 0.0, 0.0, 0.4] },
    Shoreline { material_offset: 0, uv: [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], cliff_type: 3, cliff_offsets: [0.0, 0.6, 0.4, 1.0] },
    Shoreline { material_offset: 1, uv: [[1.0, 0.0], [1.0, 1.0], [0.0, 1.0], [0.0, 0.0]], cliff_type: 2, cliff_offsets: [0.4, 0.0, 0.4, 1.0] },
    Shoreline { material_offset: 0, uv: [[1.0, 1.0], [0.0, 1.0], [0.0, 0.0], [1.0, 0.0]], cliff_type: 4, cliff_offsets: [0.6, 1.0, 1.0, 0.6] },
    Shoreline { material_offset: 3, uv: [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], cliff_type: 2, cliff_offsets: [0.0, 0.4, 0.4, 0.0] },
    Shoreline { material_offset: 1, uv: [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], cliff_type: 1, cliff_offsets: [0.0, 0.5, 1.0, 0.6] },
    Shoreline { material_offset: 2, uv: [[0.0, 1.0], [1.0, 1.0], [1.0, 0.0], [0.0, 0.0]], cliff_type: 4, cliff_offsets: [0.4, 0.0, 1.0, 0.6] },
    Shoreline { material_offset: 0, uv: [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
    Shoreline { material_offset: 1, uv: [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
    Shoreline { material_offset: 3, uv: [[1.0, 1.0], [0.0, 1.0], [0.0, 0.0], [1.0, 0.0]], cliff_type: 1, cliff_offsets: [0.0, 0.6, 0.4, 1.0] },
    Shoreline { material_offset: 2, uv: [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]], cliff_type: 6, cliff_offsets: [1.0, 0.4, 0.4, 1.0] },
    Shoreline { material_offset: 1, uv: [[1.0, 1.0], [1.0, 0.0], [0.0, 0.0], [0.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
    Shoreline { material_offset: 2, uv: [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
    Shoreline { material_offset: 2, uv: [[1.0, 1.0], [0.0, 1.0], [0.0, 0.0], [1.0, 0.0]], cliff_type: 3, cliff_offsets: [0.0, 0.6, 0.6, 0.0] },
    Shoreline { material_offset: 4, uv: [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]], cliff_type: 0, cliff_offsets: [0.0, 0.0, 0.0, 0.0] },
];

/// The four tiles around a map point (`sCornerTiles`) and, per tile, the three tiles beyond it
/// (`sCornerNeighbours`).
const CORNER_TILES: [(c_long, c_long); 4] = [(0, 0), (-1, 0), (-1, -1), (0, -1)];
const CORNER_NEIGHBOURS: [[(c_long, c_long); 3]; 4] =
    [[(0, 1), (1, 1), (1, 0)], [(0, 1), (-1, 1), (-1, 0)], [(0, -1), (-1, -1), (-1, 0)], [(0, -1), (1, -1), (1, 0)]];

/// A tile's four corners in the order the `Add*Polys` index them (`GETMQuad`).
#[derive(Clone, Copy, Default)]
struct Quad {
    vertices: [GeVisibleVertex; 4],
    alpha: [f32; 4],
    uv: [[f32; 2]; 4],
}

/// One texture layer of a tile (`GETMTileLayer`).
#[derive(Clone, Copy, Default)]
struct Layer {
    is_used: bool,
    texture_set_id: c_ulong,
    texture: usize,
}

/// The map-edge strips found visible this frame ([0] first, [1] last tile) and whether each was seen
/// (`sFrontEdge`.., `sHasFrontEdge`..).
#[derive(Clone, Copy, Default)]
struct EdgeTrack {
    front: [(c_long, c_long); 2],
    back: [(c_long, c_long); 2],
    left: [(c_long, c_long); 2],
    right: [(c_long, c_long); 2],
    has_front: bool,
    has_back: bool,
    has_left: bool,
    has_right: bool,
}

/// The mesh as the plan reads it.
struct Mesh<'a> {
    input: &'a GeVisibleInput,
    sets: &'a [GeTextureSetView],
    water_indices: &'a [c_ulong],
}

impl Mesh<'_> {
    /// `GetIsValidTile(long, long)`: the coordinates as `unsigned long`.
    unsafe fn is_valid_tile(&self, x: c_long, y: c_long) -> bool {
        // SAFETY: the view describes a live mesh
        unsafe { self.input.view.is_valid_tile(x as c_ulong, y as c_ulong) }
    }

    /// `GetIsValidPoint(long, long)`.
    unsafe fn is_valid_point(&self, x: c_long, y: c_long) -> bool {
        let (x, y) = (x as c_ulong, y as c_ulong);
        if y >= self.input.point_row_count {
            return false;
        }
        // SAFETY: the row exists
        let row: GeTmRow = unsafe { *self.input.view.point_rows.add(y as usize) };
        x.wrapping_sub(row.start) < row.count
    }

    /// `GetZ` (unchecked).
    unsafe fn z(&self, x: c_long, y: c_long) -> f32 {
        // SAFETY: the caller's contract (the point exists)
        unsafe { self.input.view.point(x as c_ulong, y as c_ulong).z }
    }

    /// `GetTile` (unchecked).
    unsafe fn tile(&self, x: c_long, y: c_long) -> GeTmTile {
        // SAFETY: the caller's contract (the tile exists)
        unsafe { *self.input.view.tile(x as c_ulong, y as c_ulong) }
    }

    /// `GetIsSpaceTile`: false off the map.
    unsafe fn is_space_tile(&self, x: c_long, y: c_long) -> bool {
        // SAFETY: the view describes a live mesh
        unsafe { self.input.view.is_space_tile(x as c_ulong, y as c_ulong) }
    }

    fn is_right_edge_tile(&self, x: c_long, y: c_long) -> bool {
        x.wrapping_add(y) == self.input.right_edge
    }

    fn is_left_edge_tile(&self, x: c_long, y: c_long) -> bool {
        x.wrapping_add(y) == self.input.left_edge
    }

    fn is_top_edge_tile(&self, x: c_long, y: c_long) -> bool {
        y.wrapping_sub(x) == self.input.top_edge
    }

    fn is_bottom_edge_tile(&self, x: c_long, y: c_long) -> bool {
        x.wrapping_sub(y) == self.input.bottom_edge
    }

    /// `GETextureSet::GetTexture(x, y)` of a registered set (0 when the set is not registered, where the C++ read
    /// past its vector, or its slot is empty: no textures, no width, height or coverage).
    unsafe fn set_texture(&self, id: c_ulong, x: c_long, y: c_long) -> usize {
        let Some(set) = self.sets.get(id as usize) else {
            return 0;
        };
        if set.textures.is_null() || set.width == 0 || set.height == 0 || set.coverage == 0 {
            return 0;
        }
        let (tx, ty) = texture_set_slot(x as c_ulong, y as c_ulong, set.coverage, set.width, set.height);
        let index = (tx as usize) * (set.height as usize) + ty as usize;
        if set.textures.is_null() || index >= (set.width as usize) * (set.height as usize) {
            return 0;
        }
        // SAFETY: the set's textures are width * height handles
        unsafe { *set.textures.add(index) }
    }

    /// The plane `index` (front 0, back 1, left 2, right 3).
    unsafe fn plane(&self, index: usize) -> Ge3DPlane {
        // SAFETY: the view's planes are four
        unsafe { *self.input.view.planes.add(index) }
    }
}

/// `IsSegmentInFrustum`: false only when both camera-space end points lie outside the same frustum plane.
fn is_segment_in_frustum(v: &GeViewport, start: Ge3DPoint, end: Ge3DPoint) -> bool {
    let both_below = |p: &Ge3DPlane| !p.is_point_on_or_above(start) && !p.is_point_on_or_above(end);
    !((v.front_clipping_distance > start.y && v.front_clipping_distance > end.y)
        || (v.rear_clipping_distance < start.y && v.rear_clipping_distance < end.y)
        || both_below(&v.left_plane)
        || both_below(&v.right_plane)
        || both_below(&v.top_plane)
        || both_below(&v.bottom_plane))
}

/// The builder of one frame's plan: the C++'s locals and the mesh members it appends to.
struct Builder<'a> {
    mesh: Mesh<'a>,
    viewport: &'a GeViewport,
    plan: VisiblePlan,
    material_map: BTreeMap<usize, u32>,
    side: usize,
    first_cliff_material: u32,
    vertex_base: c_ulong,
}

impl Builder<'_> {
    /// `mTerrainPointVisibility[a][b] = true` (`a` and `b` as the C++'s `unsigned long` differences).
    fn see_terrain(&mut self, a: c_ulong, b: c_ulong) {
        let index = (a as usize) * self.side + b as usize;
        self.plan.terrain_visible[index] = 1;
    }

    fn see_water(&mut self, a: c_ulong, b: c_ulong) {
        let index = (a as usize) * self.side + b as usize;
        self.plan.water_visible[index] = 1;
    }

    fn terrain_vertex(&self, a: c_ulong, b: c_ulong) -> GeVisibleVertex {
        GeVisibleVertex { kind: 0, a: a as u32, b: b as u32 }
    }

    fn water_vertex(&self, a: c_ulong, b: c_ulong) -> GeVisibleVertex {
        GeVisibleVertex { kind: 1, a: a as u32, b: b as u32 }
    }

    /// The material of a texture: the first list entry with it, else a new one (`mTerrainMaterialMap.find`, then
    /// the insert and `Reference()` the C++ does for a new entry).
    fn material_of(&mut self, texture: usize) -> u32 {
        if let Some(&index) = self.material_map.get(&texture) {
            return index;
        }
        let index = self.plan.materials.len() as u32;
        self.material_map.insert(texture, index);
        self.plan.materials.push(texture);
        index
    }

    /// A material list entry without the map's first-wins lookup (the cliff materials: `insert` keeps an earlier
    /// entry for a repeated texture, the list gets both).
    fn push_material(&mut self, texture: usize) {
        let index = self.plan.materials.len() as u32;
        self.material_map.entry(texture).or_insert(index);
        self.plan.materials.push(texture);
    }

    fn polygon(&self, q: &Quad, i0: usize, i1: usize, i2: usize, material: u32) -> GeVisiblePolygon {
        GeVisiblePolygon {
            vertices: [q.vertices[i0], q.vertices[i1], q.vertices[i2]],
            alpha: [q.alpha[i0], q.alpha[i1], q.alpha[i2]],
            uv: [q.uv[i0], q.uv[i1], q.uv[i2]],
            uv_set: 1,
            material,
        }
    }

    /// `AddTilePolys` and `AddWaterPolys`: one triangle on the left or right map edge, else two, minus the one cut
    /// by the top or bottom map edge.
    fn add_quad_polys(&mut self, x: c_long, y: c_long, q: &Quad, material: u32, water: bool) {
        let mut polys = Vec::with_capacity(2);
        if self.mesh.is_right_edge_tile(x, y) {
            polys.push(self.polygon(q, 2, 1, 3, material));
        } else if self.mesh.is_left_edge_tile(x, y) {
            polys.push(self.polygon(q, 1, 0, 3, material));
        } else {
            if !self.mesh.is_top_edge_tile(x, y) {
                polys.push(self.polygon(q, 2, 1, 0, material));
            }
            if !self.mesh.is_bottom_edge_tile(x, y) {
                polys.push(self.polygon(q, 2, 0, 3, material));
            }
        }
        self.plan.visible_poly_count = self.plan.visible_poly_count.wrapping_add(polys.len() as c_ulong);
        if water {
            self.plan.water_polygons.extend(polys);
        } else {
            self.plan.terrain_polygons.extend(polys);
        }
    }

    /// `AddCliffPolys`: a two-triangle cliff wall 8 units deep (noisy lower edge) along a space tile's shoreline,
    /// between the end points in `offsets`, with visibility linked to those points; nothing on the left or right
    /// map edge.
    unsafe fn add_cliff_polys(&mut self, x: c_long, y: c_long, offsets: &[f32; 4], material: u32) {
        if x.wrapping_add(y) == self.mesh.input.left_edge || x.wrapping_add(y) == self.mesh.input.right_edge {
            return;
        }
        self.plan.visible_poly_count = self.plan.visible_poly_count.wrapping_add(2);
        let fx = x as f32;
        let ax = fx + offsets[0];
        let fy = y as f32;
        let ay = fy + offsets[1];
        let bx = fx + offsets[2];
        let by = fy + offsets[3];
        let apx = (ax + 0.5) as c_long;
        let apy = (ay + 0.5) as c_long;
        let bpx = (bx + 0.5) as c_long;
        let bpy = (by + 0.5) as c_long;
        let (blx, bly) = (self.plan.bottom_left_x, self.plan.bottom_left_y);
        let a_link = [(apx as c_ulong).wrapping_sub(blx) as u32, (apy as c_ulong).wrapping_sub(bly) as u32];
        let b_link = [(bpx as c_ulong).wrapping_sub(blx) as u32, (bpy as c_ulong).wrapping_sub(bly) as u32];
        // SAFETY: the shoreline's end points are corners of a map tile
        let mut z = unsafe { self.mesh.z(apx, apy) } - 0.02;
        let base = self.plan.cliff_points.len() as u32;
        let mut point = Ge3DPoint::new(ax, ay, z);
        self.plan.cliff_points.push(point);
        self.plan.cliff_links.extend_from_slice(&a_link);
        point.z = z - 8.0 + CLIFF_NOISE[(apx % 16) as usize][(apy % 16) as usize] * 0.5;
        self.plan.cliff_points.push(point);
        self.plan.cliff_links.extend_from_slice(&a_link);
        // SAFETY: the point is read only when it is valid
        if unsafe { self.mesh.is_valid_point(bpx, bpy) } {
            z = unsafe { self.mesh.z(bpx, bpy) } - 0.05;
        }
        point.x = bx;
        point.y = by;
        point.z = z;
        self.plan.cliff_points.push(point);
        self.plan.cliff_links.extend_from_slice(&b_link);
        point.z = z - 8.0 + CLIFF_NOISE[(bpx % 16) as usize][(bpy % 16) as usize] * 0.5;
        self.plan.cliff_points.push(point);
        self.plan.cliff_links.extend_from_slice(&b_link);
        let cliff = |k: u32| GeVisibleVertex { kind: 3, a: base + k, b: 0 };
        let (a_top, a_bottom, b_top, b_bottom) = (cliff(0), cliff(1), cliff(2), cliff(3));
        self.plan.terrain_polygons.push(GeVisiblePolygon {
            vertices: [a_top, b_top, a_bottom],
            alpha: [1.0; 3],
            uv: [[0.0, 0.0], [1.0, 0.0], [0.0, 1.0]],
            uv_set: 1,
            material,
        });
        self.plan.terrain_polygons.push(GeVisiblePolygon {
            vertices: [a_bottom, b_top, b_bottom],
            alpha: [1.0; 3],
            uv: [[0.0, 1.0], [1.0, 0.0], [1.0, 1.0]],
            uv_set: 1,
            material,
        });
    }

    /// `CreateNewEdgeVertex`: a skirt vertex at (x, y, z) (the C++ gives it a black, fully visible render vertex).
    fn new_edge_vertex(&mut self, x: c_long, y: c_long, z: f32) {
        self.plan.edge_points.push(Ge3DPoint::new(x as f32, y as f32, z));
    }

    /// `DefineNewEdgePolygon`: three skirt vertices (indices relative to the base) into the untextured material;
    /// the slot's texture coordinates stay.
    fn define_edge_polygon(&mut self, v0: c_ulong, v1: c_ulong, v2: c_ulong) {
        let base = self.vertex_base;
        let edge = |v: c_ulong| GeVisibleVertex { kind: 2, a: v.wrapping_add(base) as u32, b: 0 };
        self.plan.terrain_polygons.push(GeVisiblePolygon {
            vertices: [edge(v0), edge(v1), edge(v2)],
            alpha: [1.0; 3],
            uv: [[0.0; 2]; 3],
            uv_set: 0,
            material: 0,
        });
        self.plan.visible_poly_count = self.plan.visible_poly_count.wrapping_add(1);
    }

    /// The skirt depth below a map edge plane: how far the camera's rear clip reaches past the plane, at least 1
    /// (the C++'s `theLimit` and `UMFloor`).
    unsafe fn skirt_depth(&self, plane: usize) -> c_long {
        let c = &self.viewport.camera;
        let start = Ge3DPoint::new(c.x(), c.y(), c.z());
        // SAFETY: the view's planes are four
        let plane = unsafe { self.mesh.plane(plane) };
        let n = plane.normal;
        let line = Ge3DLine { start, direction: GeVector::new(-n.x, -n.y, -n.z) };
        let limit = match line.intersects_plane(&plane) {
            Some(distance) if distance > 0.0 => self.viewport.rear_clipping_distance - distance + 1.0,
            _ => self.viewport.rear_clipping_distance,
        };
        floor(limit)
    }

    /// `Is*EdgeVisible`: whether the skirt from map point (x, y), `depth` tiles along (dx, dy), is in view; false
    /// off the map.
    unsafe fn is_edge_visible(&self, x: c_long, y: c_long, depth: c_long, dx: c_long, dy: c_long) -> bool {
        // SAFETY: the view describes a live mesh
        if !unsafe { self.mesh.is_valid_point(x, y) } {
            return false;
        }
        let start = Ge3DPoint::new(x as f32, y as f32, 0.0);
        let end = Ge3DPoint::new(
            x.wrapping_add(depth.wrapping_mul(dx)) as f32,
            y.wrapping_add(depth.wrapping_mul(dy)) as f32,
            0.0,
        );
        let inverse = &self.viewport.camera_inverse;
        is_segment_in_frustum(self.viewport, inverse.apply_point(start), inverse.apply_point(end))
    }

    /// `Generate{Back,Front,Left,Right}EdgePolygons`: extends the visible edge strip while its skirt stays in view,
    /// then builds the skirt out to the camera's rear clip depth (three vertices per edge point, four triangles per
    /// step). `plane` is the map edge's plane, (`sx`, `sy`) the skirt's direction, (`ex`, `ey`) the strip's step
    /// from its first to its last tile, `order` the four triangles' vertex indices.
    #[allow(clippy::too_many_arguments)]
    unsafe fn generate_edge(
        &mut self,
        edge: &mut [(c_long, c_long); 2],
        plane: usize,
        (sx, sy): (c_long, c_long),
        (ex, ey): (c_long, c_long),
        order: [[c_ulong; 3]; 4],
    ) {
        // SAFETY: the view describes a live mesh
        unsafe {
            let depth = self.skirt_depth(plane);
            while self.is_edge_visible(edge[0].0 - ex, edge[0].1 - ey, depth, sx, sy) {
                edge[0] = (edge[0].0 - ex, edge[0].1 - ey);
            }
            if self.mesh.is_valid_tile(edge[0].0 - ex, edge[0].1 - ey) {
                edge[0] = (edge[0].0 - ex, edge[0].1 - ey);
            }
            while self.is_edge_visible(edge[1].0 + ex, edge[1].1 + ey, depth, sx, sy) {
                edge[1] = (edge[1].0 + ex, edge[1].1 + ey);
            }
            if self.mesh.is_valid_tile(edge[1].0 + ex, edge[1].1 + ey) {
                edge[1] = (edge[1].0 + ex, edge[1].1 + ey);
            }
            let (mut x, mut y) = edge[0];
            while x <= edge[1].0 {
                let z = self.mesh.z(x, y);
                self.new_edge_vertex(x, y, z);
                self.new_edge_vertex(x + 5 * sx, y + 5 * sy, 0.0);
                self.new_edge_vertex(x + depth * sx, y + depth * sy, 0.0);
                if x != edge[0].0 {
                    for [a, b, c] in order {
                        self.define_edge_polygon(a, b, c);
                    }
                    self.vertex_base = self.vertex_base.wrapping_add(3);
                }
                x += ex;
                y += ey;
            }
            self.vertex_base = self.vertex_base.wrapping_add(3);
        }
    }

    /// The lone-strip half of a corner: when the strip does not reach the map's corner tile the skirt is extended
    /// 1000 tiles past its end point (x, y) along (`fx`, `fy`), with the skirt direction (`sx`, `sy`) and depth.
    #[allow(clippy::too_many_arguments)]
    unsafe fn corner_extension(
        &mut self,
        (x, y): (c_long, c_long),
        (fx, fy): (c_long, c_long),
        (sx, sy): (c_long, c_long),
        depth: c_long,
    ) {
        // SAFETY: the strip's end point is a map point
        let z = unsafe { self.mesh.z(x, y) };
        self.new_edge_vertex(x, y, z);
        self.new_edge_vertex(x + 5 * sx, y + 5 * sy, 0.0);
        self.new_edge_vertex(x + depth * sx, y + depth * sy, 0.0);
        let (x, y) = (x + 1000 * fx, y + 1000 * fy);
        self.new_edge_vertex(x, y, z);
        self.new_edge_vertex(x + 5 * sx, y + 5 * sy, 0.0);
        self.new_edge_vertex(x + depth * sx, y + depth * sy, 0.0);
        self.define_edge_polygon(0, 4, 1);
        self.define_edge_polygon(0, 3, 4);
        self.define_edge_polygon(1, 4, 5);
        self.define_edge_polygon(1, 5, 2);
        self.vertex_base = self.vertex_base.wrapping_add(6);
    }

    /// The seven-vertex fan joining two strips at a map corner point (x, y): the second skirt direction (`ax`, `ay`)
    /// with its depth moves the far pair, the first (`bx`, `by`) with its depth gives the near vertices, the middle
    /// vertex is at `mid` (the back corners take the left or right strip first, the front corners the front strip).
    #[allow(clippy::too_many_arguments)]
    unsafe fn corner_fan(
        &mut self,
        (x, y): (c_long, c_long),
        (ax, ay): (c_long, c_long),
        (bx, by): (c_long, c_long),
        mid: (c_long, c_long),
        depth_a: c_long,
        depth_b: c_long,
    ) {
        // SAFETY: the corner is a map point
        let z = unsafe { self.mesh.z(x, y) };
        self.new_edge_vertex(x, y, z);
        self.new_edge_vertex(x + 5 * bx, y + 5 * by, 0.0);
        self.new_edge_vertex(x + mid.0, y + mid.1, 0.0);
        self.new_edge_vertex(x + 5 * ax, y + 5 * ay, 0.0);
        self.new_edge_vertex(x + depth_b * bx, y + depth_b * by, 0.0);
        let (x, y) = (x + depth_a * ax, y + depth_a * ay);
        self.new_edge_vertex(x + depth_b * bx, y + depth_b * by, 0.0);
        self.new_edge_vertex(x, y, 0.0);
        self.define_edge_polygon(0, 2, 1);
        self.define_edge_polygon(0, 3, 2);
        self.define_edge_polygon(1, 2, 4);
        self.define_edge_polygon(2, 3, 6);
        self.define_edge_polygon(2, 6, 4);
        self.define_edge_polygon(4, 6, 5);
        self.vertex_base = self.vertex_base.wrapping_add(7);
    }

    /// `GenerateCornerBackLeftPolygons`.
    unsafe fn corner_back_left(&mut self, t: &EdgeTrack) {
        // SAFETY: as the callers'
        unsafe {
            let depth_a = self.skirt_depth(1);
            let depth_b = self.skirt_depth(2);
            if t.has_back && !t.has_left {
                if self.mesh.is_valid_tile(t.back[0].0 - 1, t.back[0].1 - 1) {
                    return;
                }
                self.corner_extension(t.back[0], (-1, -1), (1, -1), depth_a);
            } else if !t.has_back && t.has_left {
                if self.mesh.is_valid_tile(t.left[1].0 + 1, t.left[1].1 - 1) {
                    return;
                }
                self.corner_extension(t.left[1], (1, -1), (-1, -1), depth_b);
            } else if t.has_back && t.has_left {
                self.corner_fan(t.back[0], (1, -1), (-1, -1), (0, -7), depth_a, depth_b);
            }
        }
    }

    /// `GenerateCornerBackRightPolygons`.
    unsafe fn corner_back_right(&mut self, t: &EdgeTrack) {
        // SAFETY: as the callers'
        unsafe {
            let depth_a = self.skirt_depth(1);
            let depth_b = self.skirt_depth(3);
            if t.has_back && !t.has_right {
                if self.mesh.is_valid_tile(t.back[1].0 + 1, t.back[1].1 + 1) {
                    return;
                }
                self.corner_extension(t.back[1], (1, 1), (1, -1), depth_a);
            } else if !t.has_back && t.has_right {
                if self.mesh.is_valid_tile(t.right[1].0 + 1, t.right[1].1 - 1) {
                    return;
                }
                self.corner_extension(t.right[1], (1, -1), (1, 1), depth_b);
            } else if t.has_back && t.has_right {
                self.corner_fan(t.back[1], (1, -1), (1, 1), (7, 0), depth_a, depth_b);
            }
        }
    }

    /// `GenerateCornerFrontLeftPolygons`.
    unsafe fn corner_front_left(&mut self, t: &EdgeTrack) {
        // SAFETY: as the callers'
        unsafe {
            let depth_a = self.skirt_depth(0);
            let depth_b = self.skirt_depth(2);
            if t.has_front && !t.has_left {
                if self.mesh.is_valid_tile(t.front[0].0 - 1, t.front[0].1 - 1) {
                    return;
                }
                self.corner_extension(t.front[0], (-1, -1), (-1, 1), depth_a);
            } else if !t.has_front && t.has_left {
                if self.mesh.is_valid_tile(t.left[0].0 - 1, t.left[0].1 + 1) {
                    return;
                }
                self.corner_extension(t.left[0], (-1, 1), (-1, -1), depth_b);
            } else if t.has_front && t.has_left {
                self.corner_fan(t.front[0], (-1, -1), (-1, 1), (-7, 0), depth_b, depth_a);
            }
        }
    }

    /// `GenerateCornerFrontRightPolygons`.
    unsafe fn corner_front_right(&mut self, t: &EdgeTrack) {
        // SAFETY: as the callers'
        unsafe {
            let depth_a = self.skirt_depth(0);
            let depth_b = self.skirt_depth(3);
            if t.has_front && !t.has_right {
                if self.mesh.is_valid_tile(t.front[1].0 + 1, t.front[1].1 + 1) {
                    return;
                }
                self.corner_extension(t.front[1], (1, 1), (-1, 1), depth_a);
            } else if !t.has_front && t.has_right {
                if self.mesh.is_valid_tile(t.right[0].0 - 1, t.right[0].1 + 1) {
                    return;
                }
                self.corner_extension(t.right[0], (-1, 1), (1, 1), depth_b);
            } else if t.has_front && t.has_right {
                self.corner_fan(t.front[1], (1, 1), (-1, 1), (0, 7), depth_b, depth_a);
            }
        }
    }
}

/// `TrackVisibleEdges`: grows the visible map-edge strip the tile lies on (right, left, else front and/or back).
fn track_edges(t: &mut EdgeTrack, m: &Mesh, x: c_long, y: c_long) {
    fn grow(strip: &mut [(c_long, c_long); 2], has: &mut bool, x: c_long, y: c_long, diagonal: bool) {
        // the right and left strips run from (x, y + 1) to (x + 1, y); the front and back from (x, y) to
        // (x + 1, y + 1)
        let first = if diagonal { (x, y + 1) } else { (x, y) };
        let last = if diagonal { (x + 1, y) } else { (x + 1, y + 1) };
        if *has {
            if x < strip[0].0 {
                strip[0] = first;
            }
            if x + 1 > strip[1].0 {
                strip[1] = last;
            }
        } else {
            strip[0] = first;
            strip[1] = last;
            *has = true;
        }
    }
    if m.is_right_edge_tile(x, y) {
        grow(&mut t.right, &mut t.has_right, x, y, true);
    } else if m.is_left_edge_tile(x, y) {
        grow(&mut t.left, &mut t.has_left, x, y, true);
    } else {
        if m.is_top_edge_tile(x, y) {
            grow(&mut t.front, &mut t.has_front, x, y, false);
        }
        if m.is_bottom_edge_tile(x, y) {
            grow(&mut t.back, &mut t.has_back, x, y, false);
        }
    }
}

/// `TrackVisibleSpaceEdges`: only the back edge strip (space maps).
fn track_space_edges(t: &mut EdgeTrack, m: &Mesh, x: c_long, y: c_long) {
    if !m.is_bottom_edge_tile(x, y) {
        return;
    }
    if t.has_back {
        if x < t.back[0].0 {
            t.back[0] = (x, y);
        }
        if x + 1 > t.back[1].0 {
            t.back[1] = (x + 1, y + 1);
        }
    } else {
        t.back = [(x, y), (x + 1, y + 1)];
        t.has_back = true;
    }
}

/// `UpdateVisibleTerrain(camera, viewport, ...)`: the plan of the visible terrain for the viewport.
///
/// # Safety
/// `input` describes a live mesh (its rows, planes, texture sets and cliff textures).
pub unsafe fn update_visible_terrain(input: &GeVisibleInput, viewport: &GeViewport, orthographic: bool) -> VisiblePlan {
    let side = input.side as usize;
    // SAFETY: the caller's contract
    let (sets, cliffs, water_indices) = unsafe {
        (
            slice_or_empty(input.texture_sets, input.texture_set_count),
            slice_or_empty(input.space_cliff_textures, input.space_cliff_texture_count),
            slice_or_empty(input.water_texture_indices, (WATER_GRID * WATER_GRID) as usize),
        )
    };
    let mesh = Mesh { input, sets, water_indices };
    let mut plan = VisiblePlan {
        terrain_visible: vec![0; side * side],
        water_visible: vec![0; side * side],
        ..VisiblePlan::default()
    };
    let frustum = viewport.frustum_world_points(&viewport.view_rectangle, orthographic);
    // SAFETY: the caller's contract
    let (mut rows, min, mut max) =
        unsafe { tile_rows_in_volume_raw(&frustum, input.view.tile_rows, input.view.tile_row_count) };
    // port: the visible tiles stay inside the point arrays (a tile uses its far corner's point too)
    let span = (side as c_long) - 3;
    // (the empty bounds are 0x7fffffff and 0x80000000: the differences wrap as the C++'s do)
    if max.0.wrapping_sub(min.0) > span || max.1.wrapping_sub(min.1) > span {
        rows.retain_mut(|row| {
            if row.0.wrapping_sub(min.1) > span || row.1.wrapping_sub(min.0) > span {
                return false;
            }
            if row.2.wrapping_sub(min.0) > span {
                row.2 = min.0.wrapping_add(span);
            }
            true
        });
        if max.0.wrapping_sub(min.0) > span {
            max.0 = min.0.wrapping_add(span);
        }
        if max.1.wrapping_sub(min.1) > span {
            max.1 = min.1.wrapping_add(span);
        }
    }
    plan.bottom_left_x = min.0 as c_ulong;
    plan.bottom_left_y = min.1 as c_ulong;
    plan.tile_width = max.0.wrapping_sub(min.0) as c_ulong;
    plan.tile_height = max.1.wrapping_sub(min.1) as c_ulong;
    for (y, first, last) in &rows {
        plan.rows.extend_from_slice(&[*y, *first, *last]);
    }
    let mut b =
        Builder { mesh, viewport, plan, material_map: BTreeMap::new(), side, first_cliff_material: 0, vertex_base: 0 };
    // the untextured material first, then (space maps) one per cliff texture
    b.push_material(0);
    b.first_cliff_material = b.plan.materials.len() as u32;
    if input.space_tile_count != 0 {
        for &texture in cliffs {
            b.push_material(texture);
        }
    }
    let mut track = EdgeTrack::default();
    let mut quad = Quad::default();
    let (blx, bly) = (b.plan.bottom_left_x, b.plan.bottom_left_y);
    for &(y, first, last) in &rows {
        b.plan.num_visible_tiles =
            b.plan.num_visible_tiles.wrapping_add(last.wrapping_sub(first).wrapping_add(1) as c_ulong);
        let mut x = first;
        while x <= last {
            let ax = (x as c_ulong).wrapping_sub(blx);
            let ay = (y as c_ulong).wrapping_sub(bly);
            b.see_terrain(ax, ay);
            b.see_terrain(ax.wrapping_add(1), ay);
            b.see_terrain(ax, ay.wrapping_add(1));
            b.see_terrain(ax.wrapping_add(1), ay.wrapping_add(1));
            quad.vertices = [
                b.terrain_vertex(ax.wrapping_add(1), ay.wrapping_add(1)),
                b.terrain_vertex(ax, ay.wrapping_add(1)),
                b.terrain_vertex(ax, ay),
                b.terrain_vertex(ax.wrapping_add(1), ay),
            ];
            quad.uv = [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]];
            let mut layer_count = 4;
            // SAFETY: the tile is a visible tile of the mesh
            if input.space_tile_count != 0 && unsafe { b.mesh.is_space_tile(x, y) } {
                layer_count = 0;
                quad.alpha = [-1.0; 4];
                b.add_quad_polys(x, y, &quad, 0, false);
                // SAFETY: the tile exists
                let water_info = unsafe { b.mesh.tile(x, y) }.water_info;
                if water_info & SHORELINE_MASK != 0 {
                    let code = ((water_info >> 1) & 0xf) as usize;
                    let shore = &SHORELINES[code];
                    for k in 0..4 {
                        quad.uv[k] = shore.uv[k];
                        quad.alpha[k] = 1.0;
                    }
                    let material = b.first_cliff_material as c_long + 8 + shore.material_offset;
                    b.add_quad_polys(x, y, &quad, material as u32, false);
                    let cliff = match shore.cliff_type {
                        1 => x,
                        2 => y,
                        3 => x * 2,
                        4 => x * 2 + 1,
                        5 => y * 2,
                        6 => y * 2 + 1,
                        _ => -1,
                    };
                    if cliff % 8 >= 0 {
                        let material = (cliff % 8 + b.first_cliff_material as c_long) as u32;
                        // SAFETY: the shoreline's end points are the tile's corners
                        unsafe { b.add_cliff_polys(x, y, &shore.cliff_offsets, material) };
                    }
                }
            }
            let mut layers = [Layer::default(); 4];
            let mut best: c_long = -1;
            for layer in 0..layer_count {
                let (cx, cy) = (x.wrapping_add(CORNER_TILES[layer].0), y.wrapping_add(CORNER_TILES[layer].1));
                // SAFETY: the tile is read only when it is valid
                if unsafe { b.mesh.is_valid_tile(cx, cy) } {
                    // SAFETY: the tile is valid
                    layers[layer].texture_set_id = unsafe { b.mesh.tile(cx, cy) }.texture_set_id as c_ulong;
                    if input.space_tile_count != 0 {
                        for (nx, ny) in CORNER_NEIGHBOURS[layer] {
                            if layers[layer].texture_set_id as u32 == u32::MAX {
                                break;
                            }
                            let (nx, ny) = (x.wrapping_add(nx), y.wrapping_add(ny));
                            // SAFETY: the tile is read only when it is valid
                            if unsafe { b.mesh.is_valid_tile(nx, ny) }
                                && unsafe { b.mesh.tile(nx, ny) }.water_info & SHORELINE_MASK != 0
                            {
                                layers[layer].texture_set_id = NONE;
                                layers[layer].texture = cliffs.get(12).copied().unwrap_or(0);
                            }
                        }
                    }
                    if layers[layer].texture_set_id == layers[0].texture_set_id && layer != 0 {
                        layers[layer].is_used = false;
                        layers[layer].texture = layers[0].texture;
                    } else {
                        layers[layer].is_used = true;
                        if layers[layer].texture_set_id as u32 != u32::MAX {
                            // SAFETY: the registered sets are live
                            layers[layer].texture = unsafe { b.mesh.set_texture(layers[layer].texture_set_id, x, y) };
                        }
                        if best == -1 || layers[layer].texture < layers[best as usize].texture {
                            best = layer as c_long;
                        }
                    }
                } else {
                    layers[layer].is_used = false;
                }
            }
            for layer in 0..layer_count {
                if !layers[layer].is_used {
                    continue;
                }
                let material = b.material_of(layers[layer].texture);
                if layer as c_long == best {
                    quad.alpha = [1.0; 4];
                    for k in 0..4 {
                        if layer < k && layers[k].texture == layers[layer].texture {
                            layers[k].is_used = false;
                        }
                    }
                } else {
                    for k in 0..layer {
                        quad.alpha[k] = 0.0;
                    }
                    quad.alpha[layer] = 1.0;
                    for k in layer + 1..4 {
                        if layers[k].texture == layers[layer].texture {
                            layers[k].is_used = false;
                            quad.alpha[k] = 1.0;
                        } else {
                            quad.alpha[k] = 0.0;
                        }
                    }
                }
                b.add_quad_polys(x, y, &quad, material, false);
            }
            if input.space_tile_count == 0 {
                track_edges(&mut track, &b.mesh, x, y);
            } else {
                track_space_edges(&mut track, &b.mesh, x, y);
            }
            // SAFETY: the tile exists
            if input.space_tile_count != 0 || unsafe { b.mesh.tile(x, y) }.water_info & WATER_MASK == 0 {
                x += 1;
                continue;
            }
            quad.vertices = [
                b.water_vertex(ax.wrapping_add(1), ay.wrapping_add(1)),
                b.water_vertex(ax, ay.wrapping_add(1)),
                b.water_vertex(ax, ay),
                b.water_vertex(ax.wrapping_add(1), ay),
            ];
            b.see_water(ax, ay);
            b.see_water(ax.wrapping_add(1), ay);
            b.see_water(ax, ay.wrapping_add(1));
            b.see_water(ax.wrapping_add(1), ay.wrapping_add(1));
            quad.uv = [[1.0, 0.0], [0.0, 0.0], [0.0, 1.0], [1.0, 1.0]];
            // SAFETY: the tile exists
            let water_info = unsafe { b.mesh.tile(x, y) }.water_info;
            quad.alpha[2] = if water_info & 2 != 0 { 0.0 } else { 0.5 };
            quad.alpha[1] = if water_info & 4 != 0 { 0.0 } else { 0.5 };
            quad.alpha[0] = if water_info & 8 != 0 { 0.0 } else { 0.5 };
            quad.alpha[3] = if water_info & 16 != 0 { 0.0 } else { 0.5 };
            let index = ((x % WATER_GRID) * WATER_GRID + y % WATER_GRID) as usize;
            let material = b.mesh.water_indices.get(index).copied().unwrap_or(0) as u32;
            b.add_quad_polys(x, y, &quad, material, true);
            x += 1;
        }
    }
    // SAFETY: the caller's contract (the strips' points are map points)
    unsafe {
        if input.space_tile_count == 0 {
            if track.has_front {
                b.generate_edge(&mut track.front, 0, (-1, 1), (1, 1), [[0, 1, 4], [0, 4, 3], [1, 2, 5], [1, 5, 4]]);
            }
            if track.has_back {
                b.generate_edge(&mut track.back, 1, (1, -1), (1, 1), [[0, 4, 1], [0, 3, 4], [1, 5, 2], [1, 4, 5]]);
            }
            if track.has_left {
                b.generate_edge(&mut track.left, 2, (-1, -1), (1, -1), [[0, 4, 1], [0, 3, 4], [1, 5, 2], [1, 4, 5]]);
            }
            if track.has_right {
                b.generate_edge(&mut track.right, 3, (1, 1), (1, -1), [[0, 1, 4], [0, 4, 3], [1, 2, 5], [1, 5, 4]]);
            }
            b.corner_front_left(&track);
            b.corner_front_right(&track);
            b.corner_back_left(&track);
            b.corner_back_right(&track);
        } else {
            if track.has_back {
                b.generate_edge(&mut track.back, 1, (1, -1), (1, 1), [[0, 4, 1], [0, 3, 4], [1, 5, 2], [1, 4, 5]]);
            }
            b.corner_back_left(&track);
            b.corner_back_right(&track);
        }
    }
    b.plan
}

/// A slice over `count` elements at `data`, empty for a null pointer.
///
/// # Safety
/// `data` is null or valid for `count` elements.
unsafe fn slice_or_empty<'a, T>(data: *const T, count: usize) -> &'a [T] {
    if data.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, count) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_shoreline_table_has_the_cpp_rows() {
        assert_eq!(SHORELINES[1].cliff_type, 5);
        assert_eq!(SHORELINES[15].material_offset, 4);
        assert_eq!(SHORELINES[11].cliff_offsets, [1.0, 0.4, 0.4, 1.0]);
        assert_eq!(CLIFF_NOISE[15][15], 0.382);
    }

    #[test]
    fn a_segment_is_out_of_the_frustum_only_when_both_ends_fail_one_plane() {
        let v = GeViewport::new(
            GeRectangle { bottom: 768.0, left: 0.0, right: 1024.0, top: 0.0 },
            GeRectangle { bottom: 300.0, left: -400.0, right: 400.0, top: -300.0 },
            crate::types::GeTransformation::IDENTITY,
            1.0,
            100.0,
            1.0,
        );
        // both before the front plane
        assert!(!is_segment_in_frustum(&v, Ge3DPoint::new(0.0, 0.5, 0.0), Ge3DPoint::new(0.0, 0.2, 0.0)));
        // one end inside
        assert!(is_segment_in_frustum(&v, Ge3DPoint::new(0.0, 0.5, 0.0), Ge3DPoint::new(0.0, 5.0, 0.0)));
        // both beyond the rear
        assert!(!is_segment_in_frustum(&v, Ge3DPoint::new(0.0, 150.0, 0.0), Ge3DPoint::new(0.0, 200.0, 0.0)));
        // both far to the left
        assert!(!is_segment_in_frustum(&v, Ge3DPoint::new(-100.0, 5.0, 0.0), Ge3DPoint::new(-200.0, 5.0, 0.0)));
    }

    #[test]
    fn edge_tracking_grows_each_strip_from_its_tiles() {
        let input = GeVisibleInput {
            view: GeTerrainView {
                point_rows: std::ptr::null(),
                tile_rows: std::ptr::null(),
                tile_row_count: 0,
                planes: std::ptr::null(),
            },
            point_row_count: 0,
            left_edge: 1,
            top_edge: 2,
            right_edge: 7,
            bottom_edge: 2,
            space_tile_count: 0,
            texture_sets: std::ptr::null(),
            texture_set_count: 0,
            space_cliff_textures: std::ptr::null(),
            space_cliff_texture_count: 0,
            water_texture_indices: std::ptr::null(),
            side: 257,
        };
        let m = Mesh { input: &input, sets: &[], water_indices: &[] };
        let mut t = EdgeTrack::default();
        // (0, 1) is on the left edge (x + y == 1), (1, 3) on the top edge (y - x == 2)
        track_edges(&mut t, &m, 0, 1);
        assert!(t.has_left && !t.has_front);
        assert_eq!(t.left, [(0, 2), (1, 1)]);
        track_edges(&mut t, &m, 1, 3);
        assert_eq!(t.front, [(1, 3), (2, 4)]);
        track_edges(&mut t, &m, 3, 5);
        assert_eq!(t.front, [(1, 3), (4, 6)]);
        // the space variant only tracks the back edge (x - y == 2)
        let mut s = EdgeTrack::default();
        track_space_edges(&mut s, &m, 1, 3);
        assert!(!s.has_back);
        track_space_edges(&mut s, &m, 4, 2);
        assert_eq!(s.back, [(4, 2), (5, 3)]);
    }

    #[test]
    fn an_unregistered_texture_set_slot_gives_no_texture() {
        let input = GeVisibleInput {
            view: GeTerrainView {
                point_rows: std::ptr::null(),
                tile_rows: std::ptr::null(),
                tile_row_count: 0,
                planes: std::ptr::null(),
            },
            point_row_count: 0,
            left_edge: 0,
            top_edge: 0,
            right_edge: 0,
            bottom_edge: 0,
            space_tile_count: 0,
            texture_sets: std::ptr::null(),
            texture_set_count: 0,
            space_cliff_textures: std::ptr::null(),
            space_cliff_texture_count: 0,
            water_texture_indices: std::ptr::null(),
            side: 257,
        };
        let textures: [usize; 6] = [11, 12, 13, 21, 22, 23];
        // slot 0 is NULL in the C++ vector (an empty view), slot 1 a 2 x 3 set covering 4 tiles a texture
        let sets = [
            GeTextureSetView { textures: std::ptr::null(), width: 0, height: 0, coverage: 0 },
            GeTextureSetView { textures: textures.as_ptr(), width: 2, height: 3, coverage: 4 },
        ];
        let m = Mesh { input: &input, sets: &sets, water_indices: &[] };
        unsafe {
            assert_eq!(m.set_texture(0, 5, 13), 0);
            assert_eq!(m.set_texture(1, 5, 13), 21);
            assert_eq!(m.set_texture(2, 5, 13), 0);
        }
    }

    use crate::types::GeRectangle;
}
