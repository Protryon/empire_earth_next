//! `GETerrainMesh` as a whole Rust object (R4.3, eleventh part): the diamond-shaped height field (the point and tile
//! rows), the registered texture and colour sets, the colour table, the map-edge planes, the water state, and the
//! visible frame (the render vertex and polygon arrays, the materials, the skirt and cliff vertices, the visible
//! rows and point flags), with every method of GETerrainMesh.cpp and every accessor of its header.
//!
//! As `fbobj.rs`: the C++ `GETerrainMesh` stays the type the map, the simulation and the renderer use; its methods
//! are one-line forwarders and its only member is this object. Every call the original made on another object (the
//! clock, the colour fractal, the quantized normals, the texture sets and the textures' references) is one host
//! operation (`EeGeTmOp`, `GETerrainMesh::Host`), in the original's order. The queries, the codec, the flat layout,
//! the shorelines, the fog of war, the water grid and the visible plan are `terrain.rs`'s, `terrain_mesh.rs`'s,
//! `terrain_volume.rs`'s and `terrain_visible.rs`'s, over a `GeTerrainView` of this object's rows.
//!
//! Reads of a point or a tile go through the row table unchecked, as the header's inlines did (an index off its
//! row is the caller's error, as it was).

// `c_ulong` is `u32` here but `u64` on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;
use std::ffi::{c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicU64, Ordering};

use crate::ge::io::{CppIo, EeGeFieldIo};
use crate::ge::terrain::{edge_planes, GeTerrainView, GeTmPoint, GeTmRow};
use crate::ge::terrain_mesh::{self, GeTerrainFog, GeTerrainLoad, GeTmTile, WATER_GRID};
use crate::ge::terrain_visible::{self, GeTextureSetView, GeVisibleInput, GeVisiblePolygon};
use crate::ge::terrain_volume;
use crate::ge::viewport::GeViewport;
use ee_base::math::types::{Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use ee_base::math::um::lrintf;

/// `GETM_VISIBLE_POINTS`: the side of the visible point arrays.
pub const EE_GE_TM_SIDE: usize = 257;
const SIDE: usize = EE_GE_TM_SIDE;
/// The polygon arrays' capacities (`mTerrainPolygons[12000]`, `mWaterPolygons[5000]`).
const TERRAIN_POLYGONS: usize = 12000;
const WATER_POLYGONS: usize = 5000;
/// `kWaterInfoWater`, `kWaterInfoWaterMask`, `kWaterInfoSpace`.
const WATER: u8 = 1;
const WATER_MASK: u8 = 0x7f;
const SPACE: u8 = 0x80;
/// Every clip flag (a skirt or cliff vertex starts clipped everywhere).
const ALL_CLIPPED: c_ulong = 0x3f;
const WHITE: [f32; 3] = [1.0, 1.0, 1.0];

/// The calls a terrain mesh makes on other objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTmOp {
    /// `TMGetTime()`.
    Time = 1,
    /// `new U2DFractal(64, 0.0f, 10.5f, a)` -> the fractal.
    FractalNew,
    /// `delete (U2DFractal*)obj`.
    FractalDelete,
    /// `obj->GetValue(b >> 16, b & 0xffff)` into `*(float*)a`.
    FractalValue,
    /// `obj->ComputeFractal(0.0f, 10.5f, a)`.
    FractalCompute,
    /// `((const GEVector*)a)->GetQuantizedVersion()` -> the quantized vector.
    Quantize,
    /// `delete (GETextureSet*)obj`.
    SetDelete,
    /// `obj->GetNumTexturesInWidth()` (a texture set).
    SetWidth,
    /// `obj->GetNumTexturesInHeight()`.
    SetHeight,
    /// `obj->GetTextureTileCoverage()`.
    SetCoverage,
    /// `obj->GetTexture(a, b)`.
    SetTexture,
    /// `obj->GetTextureAt(a, b)`.
    SetTextureAt,
    /// `obj->Reference()` (a texture).
    TexReference,
    /// `obj->Dereference()` (a texture).
    TexDereference,
}

/// The host: `owner` is the C++ terrain mesh of the object calling.
pub type EeGeTmHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeTmOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// `GETerrainMesh::GETMRenderVertex` (sizeof 0x28 on the original's 32 bits): the rasterizer fills the point and
/// colour; the mesh writes the fog of war's visibility and, for a cliff vertex, the terrain visibility it copies.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeTmRenderVertex {
    pub point: Ge3DPoint,
    pub color: [f32; 3],
    pub visibility: f32,
    pub fog: f32,
    pub clip_flags: c_ulong,
    pub linked_visibility: *mut f32,
}

/// `GETerrainMesh::GETMPolygon`: a visible triangle.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeTmPolygon {
    pub vertices: [*mut EeGeTmRenderVertex; 3],
    pub alpha: [f32; 3],
    pub uv: [[f32; 2]; 3],
}

/// `GETerrainMesh::GETMMaterialView`: a material of the frame as the renderer reads it.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeTmMaterialView {
    pub texture: *mut c_void,
    pub polygons: *mut *mut EeGeTmPolygon,
    pub polygon_count: usize,
    pub indices: *mut *mut EeGeTmPolygon,
    pub indices_length: usize,
    pub strips: *mut *mut EeGeTmPolygon,
    pub strips_length: usize,
}

/// The mesh's scalars and lists read through `ee_ge_tm_get` (`unsigned long`) and `ee_ge_tm_get_f` (`float`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTmVal {
    GlobalWaterSetId = 1,
    NumVisibleTiles,
    VisibleTileRows,
    VisiblePolyCount,
    TerrainMaterialCount,
    WaterMaterialCount,
    SpaceCliffTextureCount,
    EdgeVertexCount,
    CliffVertexCount,
    BottomLeftX,
    BottomLeftY,
    VisibleTileWidth,
    VisibleTileHeight,
    TileRowCount,
    SpaceTileCount,
    /// floats
    MinimumZ,
    MaximumZ,
    VisibleWaterPercentage,
    VisibleSpacePercentage,
}

/// The arrays of the visible frame and the fixed tables, allocated once.
#[repr(C)]
struct Arrays {
    color_table: [[f32; 3]; 256],
    water_color: [f32; 3],
    /// front, back, left, right
    planes: [Ge3DPlane; 4],
    terrain_polygons: [EeGeTmPolygon; TERRAIN_POLYGONS],
    terrain_vertices: [[EeGeTmRenderVertex; SIDE]; SIDE],
    water_polygons: [EeGeTmPolygon; WATER_POLYGONS],
    water_vertices: [[EeGeTmRenderVertex; SIDE]; SIDE],
    water_texture_indices: [[c_ulong; WATER_GRID]; WATER_GRID],
    terrain_visible: [[u8; SIDE]; SIDE],
    water_visible: [[u8; SIDE]; SIDE],
}

/// A material of the frame (`GETMMaterial`): its texture and polygon lists.
#[derive(Clone, Debug, Default)]
struct Material {
    texture: usize,
    polygons: Vec<*mut EeGeTmPolygon>,
    indices: Vec<*mut EeGeTmPolygon>,
    strips: Vec<*mut EeGeTmPolygon>,
}

impl Material {
    fn of(texture: usize) -> Self {
        Material { texture, ..Default::default() }
    }
}

/// The terrain mesh.
pub struct EeGeTerrainMesh {
    host: EeGeTmHostFn,
    /// The C++ terrain mesh (`this`).
    owner: usize,
    a: Box<Arrays>,
    /// `mPointRows` and the points they point at.
    point_rows: Vec<GeTmRow>,
    points: Vec<Vec<GeTmPoint>>,
    /// `mTileRows` and their tiles.
    tile_rows: Vec<GeTmRow>,
    tiles: Vec<Vec<GeTmTile>>,
    /// `mTerrainMaterialMap` (texture -> first list index; `std::map` order is the address order) and the list.
    terrain_map: BTreeMap<usize, c_ulong>,
    terrain_materials: Vec<Material>,
    /// `mWaterAnimationMaterialList`'s textures (one per water frame) and `mWaterMaterialList`. The original's
    /// `mWaterMaterialMap` was never filled (only cleared and walked): it is gone.
    water_animation: Vec<usize>,
    water_materials: Vec<Material>,
    /// `mVisibleTileRows`: (y, first x, last x).
    visible_rows: Vec<[c_long; 3]>,
    texture_sets: Vec<usize>,
    color_sets: Vec<Vec<c_ulong>>,
    /// The skirt (`mEdge...`) and cliff vertices of the frame, each a heap vertex as the original's `new`.
    edge_vertices: Vec<*mut EeGeTmRenderVertex>,
    edge_points: Vec<Ge3DPoint>,
    cliff_vertices: Vec<*mut EeGeTmRenderVertex>,
    cliff_points: Vec<Ge3DPoint>,
    space_cliff_textures: Vec<usize>,
    fractal: usize,
    space_tile_count: c_ulong,
    water_texture_scale: f32,
    visible_water_percentage: f32,
    visible_space_percentage: f32,
    minimum_z: f32,
    maximum_z: f32,
    playable_width: c_long,
    playable_height: c_long,
    /// left, top, right, bottom: x + y, y - x, x + y, x - y on each edge
    edges: [c_long; 4],
    visible_tile_width: c_ulong,
    visible_tile_height: c_ulong,
    visible_bottom_left_x: c_ulong,
    visible_bottom_left_y: c_ulong,
    water_animation_frame: c_ulong,
    global_water_set_id: c_ulong,
    water_color_set_id: c_ulong,
    water_animation_period: c_ulong,
    visible_poly_count: c_ulong,
    num_visible_tiles: c_ulong,
    fractal_seed: c_ulong,
}

/// `sWaterLastTime`: the last water step's time, shared by every mesh as the original's file static.
static WATER_LAST_TIME: AtomicU64 = AtomicU64::new(0);

/// A fresh render vertex as `GETMRenderVertex()` leaves it (the point at the origin, the colour white; the rest
/// zero where the original left it unset).
const FRESH_VERTEX: EeGeTmRenderVertex = EeGeTmRenderVertex {
    point: Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 },
    color: WHITE,
    visibility: 0.0,
    fog: 0.0,
    clip_flags: 0,
    linked_visibility: std::ptr::null_mut(),
};

/// # Safety
/// `p` is a live object from `ee_ge_tm_new`.
unsafe fn tm<'a>(p: *mut EeGeTerrainMesh) -> &'a mut EeGeTerrainMesh {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

/// The arrays, zeroed then set as the original's member constructors left them.
fn new_arrays() -> Box<Arrays> {
    let layout = std::alloc::Layout::new::<Arrays>();
    // SAFETY: the layout is not zero-sized; every field of `Arrays` is valid when zeroed (floats, integers, null
    // pointers), and the box takes the allocation with the same layout
    let mut a = unsafe {
        let raw = std::alloc::alloc_zeroed(layout) as *mut Arrays;
        if raw.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        Box::from_raw(raw)
    };
    a.color_table = [WHITE; 256];
    a.water_color = WHITE;
    for plane in a.planes.iter_mut() {
        plane.normal.z = 1.0;
    }
    for row in a.terrain_vertices.iter_mut().chain(a.water_vertices.iter_mut()) {
        for v in row.iter_mut() {
            v.color = WHITE;
        }
    }
    a
}

impl EeGeTerrainMesh {
    /// A host call made by mesh `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeTmOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeTmOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (tm(p).host, tm(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b) }
    }

    /// The view the queries read: the rows and the four planes.
    fn view(&self) -> GeTerrainView {
        GeTerrainView {
            point_rows: self.point_rows.as_ptr(),
            tile_rows: self.tile_rows.as_ptr(),
            tile_row_count: self.tile_rows.len() as c_ulong,
            planes: self.a.planes.as_ptr(),
        }
    }

    /// The point at map (x, y), through its row as the header's inlines read it.
    ///
    /// # Safety
    /// Row `y` exists and `x` is in it.
    unsafe fn point(&self, x: c_ulong, y: c_ulong) -> *mut GeTmPoint {
        // SAFETY: the caller's contract
        unsafe {
            let row = *self.point_rows.as_ptr().add(y as usize);
            (row.items as *mut GeTmPoint).add(x.wrapping_sub(row.start) as usize)
        }
    }

    /// The tile at map (x, y).
    ///
    /// # Safety
    /// As `point`, for the tile rows.
    unsafe fn tile(&self, x: c_ulong, y: c_ulong) -> *mut GeTmTile {
        // SAFETY: the caller's contract
        unsafe {
            let row = *self.tile_rows.as_ptr().add(y as usize);
            (row.items as *mut GeTmTile).add(x.wrapping_sub(row.start) as usize)
        }
    }

    fn is_valid_point(&self, x: c_ulong, y: c_ulong) -> bool {
        match self.point_rows.get(y as usize) {
            Some(row) => x.wrapping_sub(row.start) < row.count,
            None => false,
        }
    }

    fn is_valid_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        match self.tile_rows.get(y as usize) {
            Some(row) => x.wrapping_sub(row.start) < row.count,
            None => false,
        }
    }

    /// `GETerrainMesh()`: an empty mesh with the default water animation (period 80) and a 64 x 64 colour fractal
    /// seeded from the clock.
    ///
    /// # Safety
    /// `host` serves `owner` from now on.
    pub unsafe fn create(host: EeGeTmHostFn, owner: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(Self {
            host,
            owner,
            a: new_arrays(),
            point_rows: Vec::new(),
            points: Vec::new(),
            tile_rows: Vec::new(),
            tiles: Vec::new(),
            terrain_map: BTreeMap::new(),
            terrain_materials: Vec::new(),
            water_animation: Vec::new(),
            water_materials: Vec::new(),
            visible_rows: Vec::new(),
            texture_sets: Vec::new(),
            color_sets: Vec::new(),
            edge_vertices: Vec::new(),
            edge_points: Vec::new(),
            cliff_vertices: Vec::new(),
            cliff_points: Vec::new(),
            space_cliff_textures: Vec::new(),
            fractal: 0,
            space_tile_count: 0,
            water_texture_scale: -0.71428573,
            visible_water_percentage: 0.0,
            visible_space_percentage: 0.0,
            minimum_z: f32::MAX,
            maximum_z: -f32::MAX,
            playable_width: 0,
            playable_height: 0,
            edges: [0; 4],
            visible_tile_width: 0,
            visible_tile_height: 0,
            visible_bottom_left_x: 0,
            visible_bottom_left_y: 0,
            water_animation_frame: 0,
            global_water_set_id: 0,
            water_color_set_id: 0,
            water_animation_period: 80,
            visible_poly_count: 0,
            num_visible_tiles: 0,
            fractal_seed: 0,
        }));
        // SAFETY: `p` is live; each host call is a statement of its own
        unsafe {
            // TEST ONLY: a fixed colour fractal under the simulation capture (EE_SIM_CAPTURE: the app lane's MAPC and MMPX)
            let seed = if std::env::var_os("EE_SIM_CAPTURE").is_some() {
                1
            } else {
                Self::op(p, EeGeTmOp::Time, 0, 0, 0) as c_ulong
            };
            tm(p).fractal_seed = seed;
            let fractal = Self::op(p, EeGeTmOp::FractalNew, 0, seed as isize, 0) as usize;
            tm(p).fractal = fractal;
        }
        p
    }

    /// `~GETerrainMesh`: the texture sets deleted, the skirt and cliff vertices, the rows and the fractal freed,
    /// then every texture of the terrain materials released; the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: `p` is live until the end
        unsafe {
            let mut i = 0;
            while i < tm(p).texture_sets.len() {
                let set = tm(p).texture_sets[i];
                Self::op(p, EeGeTmOp::SetDelete, set, 0, 0);
                i += 1;
            }
            Self::free_vertices(&mut tm(p).edge_vertices);
            Self::free_vertices(&mut tm(p).cliff_vertices);
            let fractal = tm(p).fractal;
            Self::op(p, EeGeTmOp::FractalDelete, fractal, 0, 0);
            Self::release_terrain_textures(p);
            drop(Box::from_raw(p));
        }
    }

    /// Every texture of the terrain material map released, in the map's order (the water map was always empty).
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn release_terrain_textures(p: *mut Self) {
        // SAFETY: `p` is live; the walk goes by key, so a callee cannot invalidate it
        unsafe {
            let mut next = tm(p).terrain_map.keys().next().copied();
            while let Some(texture) = next {
                if texture != 0 {
                    Self::op(p, EeGeTmOp::TexDereference, texture, 0, 0);
                }
                next = tm(p).terrain_map.range(texture + 1..).next().map(|(k, _)| *k);
                if texture == usize::MAX {
                    break;
                }
            }
        }
    }

    fn free_vertices(list: &mut Vec<*mut EeGeTmRenderVertex>) {
        for &v in list.iter() {
            // SAFETY: each came from `Box::into_raw` and is freed once
            drop(unsafe { Box::from_raw(v) });
        }
        list.clear();
    }

    /// `ComputeShorelines(x, y)`: the tile's shoreline bits from its eight neighbours.
    ///
    /// # Safety
    /// `p` is live and the tile exists.
    pub unsafe fn compute_shorelines(&mut self, x: c_ulong, y: c_ulong) {
        let view = self.view();
        // SAFETY: the caller's contract
        if let Some(bits) = unsafe { view.shoreline(self.space_tile_count > 0, x, y) } {
            // SAFETY: as above
            unsafe { self.set_water_info(x, y, bits) };
        }
    }

    /// `SetWaterInfo(x, y, info)`: the space bit kept.
    ///
    /// # Safety
    /// The tile exists.
    pub unsafe fn set_water_info(&mut self, x: c_ulong, y: c_ulong, info: u8) {
        // SAFETY: the caller's contract
        unsafe {
            let t = self.tile(x, y);
            (*t).water_info = ((*t).water_info & SPACE) | info;
        }
    }

    /// `ComputeVertexNormal(x, y)`: the point's normal from its neighbours' heights, quantized by the host.
    ///
    /// # Safety
    /// `p` is live and the point exists.
    pub unsafe fn compute_vertex_normal(p: *mut Self, x: c_ulong, y: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let rows = tm(p).point_rows.as_ptr();
            let count = tm(p).point_rows.len() as c_ulong;
            let normal = terrain_mesh::vertex_normal(rows, count, x, y);
            let q = Self::op(p, EeGeTmOp::Quantize, 0, &normal as *const GeVector as isize, 0) as *const GeVector;
            (*tm(p).point(x, y)).normal = q;
        }
    }

    /// `CreateFlatMesh(width, height)`: a flat diamond of tiles, every height 0 and every normal straight up.
    ///
    /// # Safety
    /// `p` is live; the size makes at least one tile row.
    pub unsafe fn create_flat_mesh(p: *mut Self, width: c_long, height: c_long) {
        let up = GeVector { x: 0.0, y: 0.0, z: 1.0 };
        // SAFETY: the caller's contract
        let normal =
            unsafe { Self::op(p, EeGeTmOp::Quantize, 0, &up as *const GeVector as isize, 0) } as *const GeVector;
        // SAFETY: as above; no host call below
        let m = unsafe { tm(p) };
        m.space_tile_count = 0;
        m.playable_width = width;
        m.playable_height = height;
        let layout = terrain_mesh::flat_mesh(width, height);
        m.edges = [layout.left_edge, layout.top_edge, layout.right_edge, layout.bottom_edge];
        m.tiles = layout
            .tile_rows
            .iter()
            .map(|&(start, end)| vec![GeTmTile::default(); end.wrapping_sub(start).wrapping_add(1) as usize])
            .collect();
        m.tile_rows = layout
            .tile_rows
            .iter()
            .zip(m.tiles.iter())
            .map(|(&(start, _), t)| GeTmRow { start, count: t.len() as c_ulong, items: t.as_ptr() as *const u8 })
            .collect();
        let fresh = GeTmPoint { z: 0.0, normal, color_table_index: 0 };
        m.points = layout
            .point_rows
            .iter()
            .map(|&(start, end)| vec![fresh; end.wrapping_sub(start).wrapping_add(1) as usize])
            .collect();
        m.point_rows = layout
            .point_rows
            .iter()
            .zip(m.points.iter())
            .map(|(&(start, _), t)| GeTmRow { start, count: t.len() as c_ulong, items: t.as_ptr() as *const u8 })
            .collect();
        // SAFETY: the rows were just built
        m.a.planes = unsafe { edge_planes(m.tile_rows.as_ptr(), m.tile_rows.len() as c_ulong) };
        m.minimum_z = 0.0;
        m.maximum_z = 0.0;
    }

    /// `SetZ(x, y, z)`: the height, widening the height range (`min`/`max` as the macros compare).
    ///
    /// # Safety
    /// The point exists.
    pub unsafe fn set_z(&mut self, x: c_ulong, y: c_ulong, z: f32) {
        // SAFETY: the caller's contract
        unsafe { (*self.point(x, y)).z = z };
        self.minimum_z = if self.minimum_z < z { self.minimum_z } else { z };
        self.maximum_z = if self.maximum_z > z { self.maximum_z } else { z };
    }

    /// `SetIsWater(x, y, water)`: on a map without space, the water bits cleared to water or land.
    ///
    /// # Safety
    /// The tile exists.
    pub unsafe fn set_is_water(&mut self, x: c_ulong, y: c_ulong, water: bool) {
        if self.space_tile_count == 0 {
            // SAFETY: the caller's contract
            unsafe {
                let t = self.tile(x, y);
                (*t).water_info &= SPACE;
                if water {
                    (*t).water_info |= WATER;
                }
            }
        }
    }

    /// `SetIsSpace(x, y, space)`: the space bit and count, then the shorelines of the tile and its eight neighbours.
    ///
    /// # Safety
    /// The tile exists.
    pub unsafe fn set_is_space(&mut self, x: c_ulong, y: c_ulong, space: bool) {
        // SAFETY: the caller's contract
        let t = unsafe { self.tile(x, y) };
        // SAFETY: as above
        let info = unsafe { (*t).water_info };
        if space {
            if info & SPACE == 0 {
                // SAFETY: as above
                unsafe { (*t).water_info |= SPACE };
                self.space_tile_count = self.space_tile_count.wrapping_add(1);
                // SAFETY: as above
                unsafe { self.shorelines_around(x, y) };
            }
        } else if info & SPACE != 0 {
            // SAFETY: as above
            unsafe {
                (*t).water_info &= WATER_MASK;
                self.shorelines_around(x, y);
            }
            self.space_tile_count = self.space_tile_count.wrapping_sub(1);
        }
    }

    /// The shorelines of the valid tiles from (x - 1, y - 1) to (x + 1, y + 1), as the original's unsigned loops
    /// (which run no row at all for y = 0, and no column for x = 0).
    ///
    /// # Safety
    /// The rows are live.
    unsafe fn shorelines_around(&mut self, x: c_ulong, y: c_ulong) {
        let mut ty = y.wrapping_sub(1);
        while ty <= y.wrapping_add(1) {
            let mut tx = x.wrapping_sub(1);
            while tx <= x.wrapping_add(1) {
                if self.is_valid_tile(tx, ty) {
                    // SAFETY: the tile is valid
                    unsafe { self.compute_shorelines(tx, ty) };
                }
                tx = tx.wrapping_add(1);
                if tx == 0 {
                    break;
                }
            }
            ty = ty.wrapping_add(1);
            if ty == 0 {
                break;
            }
        }
    }

    /// `SetColorSet(x, y, id)`: the point's colour from colour set `id`, picked by the fractal at the point.
    ///
    /// # Safety
    /// `p` is live; the point and the set exist.
    pub unsafe fn set_color_set(p: *mut Self, x: c_ulong, y: c_ulong, id: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let count = tm(p).color_sets[id as usize].len() as c_ulong;
            let fractal = tm(p).fractal;
            let mut value = 0.0f32;
            let xy = (((x & 63) << 16) | (y & 63)) as isize;
            Self::op(p, EeGeTmOp::FractalValue, fractal, &mut value as *mut f32 as isize, xy);
            let index = lrintf(value * (count as f32 - 1.0));
            let m = tm(p);
            let color = m.color_sets[id as usize].get(index as usize).copied().unwrap_or(0);
            (*m.point(x, y)).color_table_index = color as u8;
        }
    }

    /// `SetWaterColorSet(x, y)`: the point's colour from the water colour set by its scaled depth.
    ///
    /// # Safety
    /// The point and the water colour set exist.
    pub unsafe fn set_water_color_set(&mut self, x: c_ulong, y: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let pt = self.point(x, y);
            let set = &self.color_sets[self.water_color_set_id as usize];
            let index = terrain_mesh::water_color_index(self.water_texture_scale, (*pt).z, set.len() as c_ulong);
            (*pt).color_table_index = set.get(index as usize).copied().unwrap_or(0) as u8;
        }
    }

    /// `SetColorTable(colors)`: up to 256 colours, the rest white.
    pub fn set_color_table(&mut self, colors: &[[f32; 3]]) {
        for (i, slot) in self.a.color_table.iter_mut().enumerate() {
            *slot = colors.get(i).copied().unwrap_or(WHITE);
        }
    }

    /// `SetGlobalWaterState(set, colors, period, scale)`: the water's texture set (its textures the animation
    /// frames), colour set, period and scale; the 33 x 33 texture grid; every underwater point recoloured.
    ///
    /// # Safety
    /// `p` is live; the texture set and colour set exist.
    pub unsafe fn set_global_water_state(
        p: *mut Self,
        set_id: c_ulong,
        color_id: c_ulong,
        period: c_ulong,
        scale: f32,
    ) {
        // SAFETY: the caller's contract; each host call is a statement of its own
        unsafe {
            let m = tm(p);
            m.global_water_set_id = set_id;
            m.water_color_set_id = color_id;
            m.water_texture_scale = 1.0 / scale;
            m.water_animation_period = period;
            let set = m.texture_sets[set_id as usize];
            let width = Self::op(p, EeGeTmOp::SetWidth, set, 0, 0) as c_ulong;
            let height = Self::op(p, EeGeTmOp::SetHeight, set, 0, 0) as c_ulong;
            tm(p).water_animation.clear();
            for i in 0..width {
                for j in 0..height {
                    let texture = Self::op(p, EeGeTmOp::SetTexture, set, i as isize, j as isize) as usize;
                    tm(p).water_animation.push(texture);
                }
            }
            let m = tm(p);
            let grid = terrain_mesh::water_texture_indices(height.wrapping_mul(width) as c_long);
            for (dst, src) in m.a.water_texture_indices.iter_mut().zip(grid.iter()) {
                dst.copy_from_slice(src);
            }
            for j in 0..m.point_rows.len() {
                let row = m.point_rows[j];
                for i in 0..row.count {
                    if m.points[j][i as usize].z < 0.0 {
                        m.set_water_color_set(row.start.wrapping_add(i), j as c_ulong);
                    }
                }
            }
        }
    }

    /// `UpdateFogOfWar(all)`: each visible point's visibility and the visible water and space shares.
    pub fn update_fog_of_war(&mut self, all_visible: bool) {
        let view = self.view();
        let stride = std::mem::size_of::<EeGeTmRenderVertex>();
        let fog = GeTerrainFog {
            visible_row_count: self.visible_rows.len(),
            bottom_left_x: self.visible_bottom_left_x,
            bottom_left_y: self.visible_bottom_left_y,
            tile_width: self.visible_tile_width,
            tile_height: self.visible_tile_height,
            num_visible_tiles: self.num_visible_tiles,
            side: SIDE,
            terrain_point_visible: self.a.terrain_visible.as_ptr() as *const u8,
            water_point_visible: self.a.water_visible.as_ptr() as *const u8,
            terrain_visibility: &mut self.a.terrain_vertices[0][0].visibility,
            water_visibility: &mut self.a.water_vertices[0][0].visibility,
            vertex_stride: stride,
            all_visible,
        };
        // SAFETY: the view and the fog describe this mesh's live rows and arrays
        if let Some((water, space)) = unsafe { terrain_mesh::fog_of_war(&view, &fog) } {
            self.visible_water_percentage = water;
            self.visible_space_percentage = space;
        }
    }

    /// `UpdateWaterAnimation()`: the whole periods elapsed since the last step advance the water frame, and every
    /// water material shows its new frame.
    ///
    /// # Safety
    /// `p` is live; with water frames, the water texture set exists.
    pub unsafe fn update_water_animation(p: *mut Self) {
        // SAFETY: the caller's contract; each host call is a statement of its own
        unsafe {
            let frames = tm(p).water_animation.len() as c_ulong;
            let set = tm(p).texture_sets.get(tm(p).global_water_set_id as usize).copied().unwrap_or(0);
            let now = Self::op(p, EeGeTmOp::Time, 0, 0, 0) as c_ulong;
            let last = WATER_LAST_TIME.load(Ordering::Relaxed) as c_ulong;
            let m = tm(p);
            let Some((frame, new_last)) = terrain_mesh::water_animation_step(
                now,
                last,
                m.water_animation_period,
                m.water_animation_frame,
                frames,
            ) else {
                return;
            };
            m.water_animation_frame = frame;
            WATER_LAST_TIME.store(new_last as u64, Ordering::Relaxed);
            for i in 0..frames {
                let index = (frame.wrapping_add(i) % frames) as isize;
                let texture = Self::op(p, EeGeTmOp::SetTexture, set, index, 1) as usize;
                let m = tm(p);
                if let Some(material) = m.water_materials.get_mut(i as usize) {
                    material.texture = texture;
                }
                m.water_animation[i as usize] = texture;
            }
        }
    }

    /// `UpdateVisibleTerrain(camera, viewport, ...)`: the frame planned for the viewport (`terrain_visible.rs`), the
    /// last frame's materials released, the material lists rebuilt from the plan's textures (each referenced), the
    /// skirt and cliff vertices made and the polygon arrays filled from the plan's vertex references.
    ///
    /// # Safety
    /// `p` is live and has rows; `viewport` is valid.
    pub unsafe fn update_visible_terrain(p: *mut Self, viewport: *const GeViewport, orthographic: bool) {
        // SAFETY: the caller's contract; each host call is a statement of its own
        unsafe {
            let set_count = tm(p).texture_sets.len();
            let mut sets =
                vec![GeTextureSetView { textures: std::ptr::null(), width: 0, height: 0, coverage: 0 }; set_count];
            let mut set_textures: Vec<Vec<usize>> = vec![Vec::new(); set_count];
            for i in 0..set_count {
                // an unregistered id's slot is NULL: an empty view
                let set = tm(p).texture_sets[i];
                if set == 0 {
                    continue;
                }
                let width = Self::op(p, EeGeTmOp::SetWidth, set, 0, 0) as c_ulong;
                let height = Self::op(p, EeGeTmOp::SetHeight, set, 0, 0) as c_ulong;
                let n = width.wrapping_mul(height);
                set_textures[i] = vec![0; n as usize];
                for j in 0..n {
                    let (a, b) = ((j / height) as isize, (j % height) as isize);
                    set_textures[i][j as usize] = Self::op(p, EeGeTmOp::SetTextureAt, set, a, b) as usize;
                }
                sets[i].textures = set_textures[i].as_ptr();
                sets[i].width = width;
                sets[i].height = height;
                sets[i].coverage = Self::op(p, EeGeTmOp::SetCoverage, set, 0, 0) as c_ulong;
            }
            let m = tm(p);
            let input = GeVisibleInput {
                view: m.view(),
                point_row_count: m.point_rows.len() as c_ulong,
                left_edge: m.edges[0],
                top_edge: m.edges[1],
                right_edge: m.edges[2],
                bottom_edge: m.edges[3],
                space_tile_count: m.space_tile_count,
                texture_sets: sets.as_ptr(),
                texture_set_count: sets.len(),
                space_cliff_textures: m.space_cliff_textures.as_ptr(),
                space_cliff_texture_count: m.space_cliff_textures.len(),
                water_texture_indices: m.a.water_texture_indices.as_ptr() as *const c_ulong,
                side: SIDE as c_ulong,
            };
            let plan = terrain_visible::update_visible_terrain(&input, &*viewport, orthographic);
            m.a.terrain_visible.as_flattened_mut().copy_from_slice(&plan.terrain_visible[..SIDE * SIDE]);
            m.a.water_visible.as_flattened_mut().copy_from_slice(&plan.water_visible[..SIDE * SIDE]);
            m.visible_poly_count = plan.visible_poly_count;
            m.num_visible_tiles = plan.num_visible_tiles;
            m.visible_rows = plan.rows.as_chunks::<3>().0.to_vec();
            m.visible_bottom_left_x = plan.bottom_left_x;
            m.visible_bottom_left_y = plan.bottom_left_y;
            m.visible_tile_width = plan.tile_width;
            m.visible_tile_height = plan.tile_height;
            // the last frame's materials released, the lists rebuilt (the water list from the animation frames)
            Self::release_terrain_textures(p);
            let m = tm(p);
            m.terrain_map.clear();
            m.terrain_materials.clear();
            m.water_materials = m.water_animation.iter().map(|&t| Material::of(t)).collect();
            m.edge_points.clear();
            Self::free_vertices(&mut m.edge_vertices);
            m.cliff_points.clear();
            Self::free_vertices(&mut m.cliff_vertices);
            // the terrain materials: the plan's textures in order (the map keeps a repeated texture's first index),
            // each referenced once per entry
            for (k, &texture) in plan.materials.iter().enumerate() {
                let m = tm(p);
                m.terrain_map.entry(texture).or_insert(k as c_ulong);
                m.terrain_materials.push(Material::of(texture));
                if texture != 0 {
                    Self::op(p, EeGeTmOp::TexReference, texture, 0, 0);
                }
            }
            // the skirt vertices (black, fully visible) and the cliff vertices (following a terrain vertex's
            // visibility)
            let m = tm(p);
            for point in plan.edge_points.iter() {
                m.edge_points.push(*point);
                let v =
                    EeGeTmRenderVertex { color: [0.0; 3], visibility: 1.0, clip_flags: ALL_CLIPPED, ..FRESH_VERTEX };
                m.edge_vertices.push(Box::into_raw(Box::new(v)));
            }
            for (k, point) in plan.cliff_points.iter().enumerate() {
                m.cliff_points.push(*point);
                let (a, b) = (plan.cliff_links[2 * k] as usize, plan.cliff_links[2 * k + 1] as usize);
                let v = EeGeTmRenderVertex {
                    visibility: -1.0,
                    linked_visibility: &mut m.a.terrain_vertices[a][b].visibility,
                    clip_flags: ALL_CLIPPED,
                    ..FRESH_VERTEX
                };
                m.cliff_vertices.push(Box::into_raw(Box::new(v)));
            }
            // the polygons, into the arrays and their materials' lists (a frame past an array's capacity stops
            // there)
            for (k, planned) in plan.terrain_polygons.iter().take(TERRAIN_POLYGONS).enumerate() {
                let polygon: *mut EeGeTmPolygon = &mut m.a.terrain_polygons[k];
                m.fill_polygon(planned, polygon);
                if let Some(material) = m.terrain_materials.get_mut(planned.material as usize) {
                    material.polygons.push(polygon);
                }
            }
            for (k, planned) in plan.water_polygons.iter().take(WATER_POLYGONS).enumerate() {
                let polygon: *mut EeGeTmPolygon = &mut m.a.water_polygons[k];
                m.fill_polygon(planned, polygon);
                if let Some(material) = m.water_materials.get_mut(planned.material as usize) {
                    material.polygons.push(polygon);
                }
            }
            for material in m.terrain_materials.iter_mut().chain(m.water_materials.iter_mut()) {
                let n = material.polygons.len() + 2;
                material.strips.resize(n, std::ptr::null_mut());
                material.indices.resize(n, std::ptr::null_mut());
            }
        }
    }

    /// A planned polygon into a polygon of the arrays: its corners as pointers, its alphas, and its texture
    /// coordinates when the plan set them (a skirt polygon keeps its slot's, as the original did).
    ///
    /// # Safety
    /// `out` is one of the arrays' polygons; the plan's references are in range.
    unsafe fn fill_polygon(&mut self, planned: &GeVisiblePolygon, out: *mut EeGeTmPolygon) {
        for v in 0..3 {
            let r = planned.vertices[v];
            let vertex: *mut EeGeTmRenderVertex = match r.kind {
                0 => &mut self.a.terrain_vertices[r.a as usize][r.b as usize],
                1 => &mut self.a.water_vertices[r.a as usize][r.b as usize],
                2 => self.edge_vertices[r.a as usize],
                _ => self.cliff_vertices[r.a as usize],
            };
            // SAFETY: the caller's contract
            unsafe {
                (*out).vertices[v] = vertex;
                (*out).alpha[v] = planned.alpha[v];
                if planned.uv_set != 0 {
                    (*out).uv[v] = planned.uv[v];
                }
            }
        }
    }

    /// `Load(major, minor, file)`: the map size (then a flat mesh of it), each tile's texture set (an unregistered
    /// id becomes 0) and water info, each point's height and colour, the fractal seed (the fractal recomputed from
    /// it), then every normal; returns the first read error.
    ///
    /// # Safety
    /// `p` is live; `io` is a live field stream.
    pub unsafe fn load(p: *mut Self, io: &EeGeFieldIo) -> c_long {
        // SAFETY: the caller's contract; each host call is a statement of its own
        unsafe {
            let m = tm(p);
            let mut io = CppIo(*io);
            let result = terrain_mesh::load_size(&mut io, &mut m.playable_width, &mut m.playable_height);
            if result != 0 {
                return result;
            }
            let (w, h) = (m.playable_width, m.playable_height);
            Self::create_flat_mesh(p, w, h);
            let m = tm(p);
            let mut valid = [0u8; 256];
            for (i, v) in valid.iter_mut().enumerate() {
                *v = u8::from(m.texture_sets.get(i).is_some_and(|&s| s != 0));
            }
            let mut state = GeTerrainLoad {
                space_tile_count: m.space_tile_count,
                minimum_z: m.minimum_z,
                maximum_z: m.maximum_z,
                fractal_seed: m.fractal_seed,
                compute_fractal: false,
            };
            let view = m.view();
            let result = terrain_mesh::load_body(&mut io, &view, m.point_rows.len() as c_ulong, &valid, &mut state);
            m.space_tile_count = state.space_tile_count;
            m.minimum_z = state.minimum_z;
            m.maximum_z = state.maximum_z;
            m.fractal_seed = state.fractal_seed;
            if state.compute_fractal {
                let (fractal, seed) = (m.fractal, m.fractal_seed);
                Self::op(p, EeGeTmOp::FractalCompute, fractal, seed as isize, 0);
            }
            let mut j = 0;
            while j < tm(p).point_rows.len() {
                let row = tm(p).point_rows[j];
                for i in 0..row.count {
                    Self::compute_vertex_normal(p, row.start.wrapping_add(i), j as c_ulong);
                }
                j += 1;
            }
            result
        }
    }

    /// `Save(file)`: the map size, the tiles, the points and the fractal seed; returns the first write error.
    ///
    /// # Safety
    /// `io` is a live field stream.
    pub unsafe fn save(&self, io: &EeGeFieldIo) -> c_long {
        let view = self.view();
        // SAFETY: the caller's contract; the view is this mesh's
        unsafe {
            terrain_mesh::save(
                &mut CppIo(*io),
                &view,
                self.point_rows.len() as c_ulong,
                self.playable_width,
                self.playable_height,
                self.fractal_seed,
            )
        }
    }

    fn materials(&mut self, water: bool) -> &mut Vec<Material> {
        if water {
            &mut self.water_materials
        } else {
            &mut self.terrain_materials
        }
    }
}

// ---- the C++ class's forwarders ----

/// `GETerrainMesh()`.
///
/// # Safety
/// `host` serves `owner` (the C++ mesh) for the object's life.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_new(host: EeGeTmHostFn, owner: *mut c_void) -> *mut EeGeTerrainMesh {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::create(host, owner as usize) }
}

/// `~GETerrainMesh`.
///
/// # Safety
/// `p` came from `ee_ge_tm_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_destroy(p: *mut EeGeTerrainMesh) {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::destroy(p) }
}

/// A scalar of the mesh (`unsigned long` values; 0 for the float ones).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_get(p: *const EeGeTerrainMesh, what: EeGeTmVal) -> c_ulong {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    match what {
        EeGeTmVal::GlobalWaterSetId => m.global_water_set_id,
        EeGeTmVal::NumVisibleTiles => m.num_visible_tiles,
        EeGeTmVal::VisibleTileRows => m.visible_rows.len() as c_ulong,
        EeGeTmVal::VisiblePolyCount => m.visible_poly_count,
        EeGeTmVal::TerrainMaterialCount => m.terrain_materials.len() as c_ulong,
        EeGeTmVal::WaterMaterialCount => m.water_materials.len() as c_ulong,
        EeGeTmVal::SpaceCliffTextureCount => m.space_cliff_textures.len() as c_ulong,
        EeGeTmVal::EdgeVertexCount => m.edge_points.len() as c_ulong,
        EeGeTmVal::CliffVertexCount => m.cliff_points.len() as c_ulong,
        EeGeTmVal::BottomLeftX => m.visible_bottom_left_x,
        EeGeTmVal::BottomLeftY => m.visible_bottom_left_y,
        EeGeTmVal::VisibleTileWidth => m.visible_tile_width,
        EeGeTmVal::VisibleTileHeight => m.visible_tile_height,
        EeGeTmVal::TileRowCount => m.tile_rows.len() as c_ulong,
        EeGeTmVal::SpaceTileCount => m.space_tile_count,
        _ => 0,
    }
}

/// A float of the mesh (0 for the `unsigned long` ones).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_get_f(p: *const EeGeTerrainMesh, what: EeGeTmVal) -> f32 {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    match what {
        EeGeTmVal::MinimumZ => m.minimum_z,
        EeGeTmVal::MaximumZ => m.maximum_z,
        EeGeTmVal::VisibleWaterPercentage => m.visible_water_percentage,
        EeGeTmVal::VisibleSpacePercentage => m.visible_space_percentage,
        _ => 0.0,
    }
}

/// `SetVisibleWaterTilePercentage`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_set_water_percentage(p: *mut EeGeTerrainMesh, percentage: f32) {
    // SAFETY: the function's contract
    unsafe { (*p).visible_water_percentage = percentage };
}

/// Map-edge plane `which` (front, back, left, right).
///
/// # Safety
/// `p` is live; `which` < 4.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_plane(p: *const EeGeTerrainMesh, which: u32) -> *const Ge3DPlane {
    // SAFETY: the function's contract
    unsafe { &(*p).a.planes[which as usize] }
}

/// `GetIsEdgeTile` (`which` 0), `GetIsLeftEdgeTile` (1), `GetIsTopEdgeTile` (2), `GetIsRightEdgeTile` (3),
/// `GetIsBottomEdgeTile` (4).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_is_edge_tile(p: *const EeGeTerrainMesh, x: c_long, y: c_long, which: u32) -> bool {
    // SAFETY: the function's contract
    let e = unsafe { &(*p).edges };
    let left = x.wrapping_add(y) == e[0];
    let top = y.wrapping_sub(x) == e[1];
    let right = x.wrapping_add(y) == e[2];
    let bottom = x.wrapping_sub(y) == e[3];
    match which {
        0 => left || top || right || bottom,
        1 => left,
        2 => top,
        3 => right,
        _ => bottom,
    }
}

/// `GetIsValidPoint` (`tile` false) or `GetIsValidTile` (true).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_is_valid(p: *const EeGeTerrainMesh, x: c_ulong, y: c_ulong, tile: bool) -> bool {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    if tile {
        m.is_valid_tile(x, y)
    } else {
        m.is_valid_point(x, y)
    }
}

/// The point at map (x, y) (`GetZ`, `GetColorTableIndex`, `GetVertexNormal` read it).
///
/// # Safety
/// `p` is live and the point exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_point(p: *const EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> *const GeTmPoint {
    // SAFETY: the function's contract
    unsafe { (*p).point(x, y) }
}

/// `GetVertexColor(x, y)`: the colour table's entry of the point's index.
///
/// # Safety
/// `p` is live and the point exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_vertex_color(p: *const EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> *const f32 {
    // SAFETY: the function's contract
    unsafe {
        let index = (*(*p).point(x, y)).color_table_index;
        (*p).a.color_table[index as usize].as_ptr()
    }
}

/// `GetWaterColor`: the water colour (white: nothing sets it).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_water_color(p: *const EeGeTerrainMesh) -> *const f32 {
    // SAFETY: the function's contract
    unsafe { (*p).a.water_color.as_ptr() }
}

/// The tile at map (x, y) (`GetTextureSetID`, `GetWaterInfo`, `GetIsWaterTile` read it).
///
/// # Safety
/// `p` is live and the tile exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_tile(p: *const EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> *const GeTmTile {
    // SAFETY: the function's contract
    unsafe { (*p).tile(x, y) }
}

/// The tile edits of the header: `ReferenceTile` (`what` 0), `DereferenceTile` (1), `SetTileReferenceCount` (2,
/// `value`), `SetTextureSet` (3, `value`), `SetWaterInfo` (4, `value`), `SetIsWater` (5, `value` != 0),
/// `SetIsSpace` (6, `value` != 0), `ComputeShorelines` (7).
///
/// # Safety
/// `p` is live and the tile exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_tile_edit(
    p: *mut EeGeTerrainMesh,
    x: c_ulong,
    y: c_ulong,
    what: u32,
    value: c_ulong,
) {
    // SAFETY: the function's contract
    unsafe {
        let m = &mut *p;
        let t = m.tile(x, y);
        match what {
            0 => {
                (*t).reference_count = if (*t).reference_count > 0 { (*t).reference_count.wrapping_add(1) } else { 2 };
            }
            1 => (*t).reference_count = (*t).reference_count.wrapping_sub(1),
            2 => (*t).reference_count = value as u16,
            3 => (*t).texture_set_id = value as u8,
            4 => m.set_water_info(x, y, value as u8),
            5 => m.set_is_water(x, y, value != 0),
            6 => m.set_is_space(x, y, value != 0),
            _ => m.compute_shorelines(x, y),
        }
    }
}

/// `GetIsSpaceTile(x, y)`: false off the map.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_is_space_tile(p: *const EeGeTerrainMesh, x: c_ulong, y: c_ulong) -> bool {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    // SAFETY: the tile is valid
    m.is_valid_tile(x, y) && unsafe { (*m.tile(x, y)).water_info } & SPACE != 0
}

/// The point edits: `SetZ` (`what` 0, `z`), `SetColorFromColorTableIndex` (1, `value`), `SetColorSet` (2, `value`
/// the set), `SetWaterColorSet` (3), `ComputeVertexNormal` (4).
///
/// # Safety
/// `p` is live and the point exists (and for 2 and 3 the colour set).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_point_edit(
    p: *mut EeGeTerrainMesh,
    x: c_ulong,
    y: c_ulong,
    what: u32,
    value: c_ulong,
    z: f32,
) {
    // SAFETY: the function's contract
    unsafe {
        match what {
            0 => (*p).set_z(x, y, z),
            1 => (*(*p).point(x, y)).color_table_index = value as u8,
            2 => EeGeTerrainMesh::set_color_set(p, x, y, value),
            3 => (*p).set_water_color_set(x, y),
            _ => EeGeTerrainMesh::compute_vertex_normal(p, x, y),
        }
    }
}

/// `CreateFlatMesh(width, height)`.
///
/// # Safety
/// `p` is live; the size makes at least one tile row.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_create_flat_mesh(p: *mut EeGeTerrainMesh, width: c_long, height: c_long) {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::create_flat_mesh(p, width, height) }
}

/// `GetMeshZ(x, y)`.
///
/// # Safety
/// `p` is live; the tile under the position exists.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_mesh_z(p: *const EeGeTerrainMesh, x: f32, y: f32) -> f32 {
    // SAFETY: the function's contract
    unsafe { (*p).view().mesh_z(x, y) }
}

/// `GetMeshNormal(x, y, out)`.
///
/// # Safety
/// As `ee_ge_tm_mesh_z`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_mesh_normal(p: *const EeGeTerrainMesh, x: f32, y: f32, out: *mut GeVector) {
    // SAFETY: the function's contract
    unsafe { *out = (*p).view().mesh_normal(x, y) };
}

/// `GetClosestTile(line, x, y)`.
///
/// # Safety
/// `p` is live; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_closest_tile_line(
    p: *const EeGeTerrainMesh,
    line: *const Ge3DLine,
    out_x: *mut c_long,
    out_y: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let line = *line;
        let (x, y) = (*p).view().closest_tile_to_line(&line);
        *out_x = x;
        *out_y = y;
    }
}

/// `GetClosestTile(point, x, y)`.
///
/// # Safety
/// `p` is live; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_closest_tile_point(
    p: *const EeGeTerrainMesh,
    point: *const Ge3DPoint,
    out_x: *mut c_long,
    out_y: *mut c_long,
) {
    // SAFETY: the function's contract
    unsafe {
        let (x, y) = (*p).view().closest_tile_to_point(*point);
        *out_x = x;
        *out_y = y;
    }
}

/// `Intersects(line, distance)`: `distance` is written whenever the original wrote it.
///
/// # Safety
/// `p` is live; the pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_intersects(
    p: *const EeGeTerrainMesh,
    line: *const Ge3DLine,
    distance: *mut f32,
) -> bool {
    // SAFETY: the function's contract
    unsafe {
        let line = *line;
        let (hit, d) = (*p).view().intersects(&line);
        if let Some(d) = d {
            *distance = d;
        }
        hit
    }
}

/// `GetTileRowsInVolume`: the tile rows under the convex hull of `volume` (`count` points) as (y, first x, last x)
/// triples into `out_rows` (room for `3 * TileRowCount`), the bounding box into `bounds`; returns the row count.
///
/// # Safety
/// `p` is live; the pointers are valid for those sizes.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_rows_in_volume(
    p: *const EeGeTerrainMesh,
    volume: *const Ge3DPoint,
    count: usize,
    out_rows: *mut c_long,
    bounds: *mut c_long,
) -> usize {
    // SAFETY: the function's contract
    unsafe {
        let m = &*p;
        let points = if volume.is_null() || count == 0 { &[][..] } else { std::slice::from_raw_parts(volume, count) };
        let (rows, min, max) =
            terrain_volume::tile_rows_in_volume_raw(points, m.tile_rows.as_ptr(), m.tile_rows.len() as c_ulong);
        for (k, (y, first, last)) in rows.iter().enumerate() {
            *out_rows.add(3 * k) = *y;
            *out_rows.add(3 * k + 1) = *first;
            *out_rows.add(3 * k + 2) = *last;
        }
        *bounds = min.0;
        *bounds.add(1) = min.1;
        *bounds.add(2) = max.0;
        *bounds.add(3) = max.1;
        rows.len()
    }
}

/// `RegisterColorSet(set, id)`: returns the new id.
///
/// # Safety
/// `p` is live; `colors` holds `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_register_color_set(
    p: *mut EeGeTerrainMesh,
    colors: *const c_ulong,
    count: usize,
) -> c_ulong {
    // SAFETY: the function's contract
    unsafe {
        let m = &mut *p;
        let id = m.color_sets.len() as c_ulong;
        let set = if count == 0 { Vec::new() } else { std::slice::from_raw_parts(colors, count).to_vec() };
        m.color_sets.push(set);
        id
    }
}

/// `RegisterTextureSet(set, id)`: returns the new id (the mesh deletes the set).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_register_texture_set(p: *mut EeGeTerrainMesh, set: *mut c_void) -> c_ulong {
    // SAFETY: the function's contract
    let m = unsafe { &mut *p };
    m.texture_sets.push(set as usize);
    (m.texture_sets.len() - 1) as c_ulong
}

/// `GetRegisteredTextureSet(id)` (NULL past the registered ones).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_texture_set(p: *const EeGeTerrainMesh, id: c_ulong) -> *mut c_void {
    // SAFETY: the function's contract
    let sets = unsafe { &(*p).texture_sets };
    sets.get(id as usize).copied().unwrap_or(0) as *mut c_void
}

/// `SetColorTable(colors)`: `count` colours of three floats.
///
/// # Safety
/// `p` is live; `colors` holds `count` colours.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_set_color_table(p: *mut EeGeTerrainMesh, colors: *const [f32; 3], count: usize) {
    // SAFETY: the function's contract
    unsafe {
        let colors = if count == 0 { &[][..] } else { std::slice::from_raw_parts(colors, count) };
        (*p).set_color_table(colors);
    }
}

/// `SetGlobalWaterState(set, colors, period, scale)`.
///
/// # Safety
/// `p` is live; the texture set and colour set exist.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_set_global_water_state(
    p: *mut EeGeTerrainMesh,
    set: c_ulong,
    colors: c_ulong,
    period: c_ulong,
    scale: f32,
) {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::set_global_water_state(p, set, colors, period, scale) }
}

/// `SetSpaceCliffTextures(textures)`.
///
/// # Safety
/// `p` is live; `textures` holds `count` pointers.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_set_space_cliff_textures(
    p: *mut EeGeTerrainMesh,
    textures: *const *mut c_void,
    count: usize,
) {
    // SAFETY: the function's contract
    unsafe {
        (*p).space_cliff_textures = if count == 0 {
            Vec::new()
        } else {
            std::slice::from_raw_parts(textures, count).iter().map(|&t| t as usize).collect()
        };
    }
}

/// `UpdateFogOfWar(all)`.
///
/// # Safety
/// `p` is live and has rows.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_update_fog_of_war(p: *mut EeGeTerrainMesh, all_visible: bool) {
    // SAFETY: the function's contract
    unsafe { (*p).update_fog_of_war(all_visible) }
}

/// `UpdateWaterAnimation()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_update_water_animation(p: *mut EeGeTerrainMesh) {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::update_water_animation(p) }
}

/// `UpdateVisibleTerrain(camera, viewport, width, height, screen, orthographic)` (the original read only the
/// viewport and the projection).
///
/// # Safety
/// `p` is live and has rows; `viewport` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_update_visible_terrain(
    p: *mut EeGeTerrainMesh,
    viewport: *const GeViewport,
    orthographic: bool,
) {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::update_visible_terrain(p, viewport, orthographic) }
}

/// `Load(major, minor, file)`.
///
/// # Safety
/// `p` is live; `io` is a live field stream.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_load(p: *mut EeGeTerrainMesh, io: *const EeGeFieldIo) -> c_long {
    // SAFETY: the function's contract
    unsafe { EeGeTerrainMesh::load(p, &*io) }
}

/// `Save(file)`.
///
/// # Safety
/// `p` is live; `io` is a live field stream.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_save(p: *const EeGeTerrainMesh, io: *const EeGeFieldIo) -> c_long {
    // SAFETY: the function's contract
    unsafe { (*p).save(&*io) }
}

/// The visible tile row `index`: (y, first x, last x) into `out`.
///
/// # Safety
/// `p` is live; the row exists; `out` holds three values.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_visible_row(p: *const EeGeTerrainMesh, index: c_ulong, out: *mut c_long) {
    // SAFETY: the function's contract
    unsafe {
        let rows = &(*p).visible_rows;
        let r = rows[index as usize];
        std::ptr::copy_nonoverlapping(r.as_ptr(), out, 3);
    }
}

/// The render vertex array (`GetTerrainRenderVertexArray`, or with `water` `GetWaterRenderVertexArray`):
/// `[GETM_VISIBLE_POINTS][GETM_VISIBLE_POINTS]`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_render_vertices(p: *mut EeGeTerrainMesh, water: bool) -> *mut EeGeTmRenderVertex {
    // SAFETY: the function's contract
    let a = unsafe { &mut (*p).a };
    if water {
        a.water_vertices.as_mut_ptr() as *mut EeGeTmRenderVertex
    } else {
        a.terrain_vertices.as_mut_ptr() as *mut EeGeTmRenderVertex
    }
}

/// The visible point flags (`GetTerrainPointVisibilityArray`, or with `water` the water's), as bytes.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_point_visibility(p: *mut EeGeTerrainMesh, water: bool) -> *mut u8 {
    // SAFETY: the function's contract
    let a = unsafe { &mut (*p).a };
    if water {
        a.water_visible.as_mut_ptr() as *mut u8
    } else {
        a.terrain_visible.as_mut_ptr() as *mut u8
    }
}

/// `GetMaterialView(water, index, out)`.
///
/// # Safety
/// `p` is live; the material exists; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_material(
    p: *mut EeGeTerrainMesh,
    water: bool,
    index: c_ulong,
    out: *mut EeGeTmMaterialView,
) {
    // SAFETY: the function's contract
    unsafe {
        let m = &mut (*p).materials(water)[index as usize];
        *out = EeGeTmMaterialView {
            texture: m.texture as *mut c_void,
            polygons: m.polygons.as_mut_ptr(),
            polygon_count: m.polygons.len(),
            indices: m.indices.as_mut_ptr(),
            indices_length: m.indices.len(),
            strips: m.strips.as_mut_ptr(),
            strips_length: m.strips.len(),
        };
    }
}

/// `GetTerrainMaterialMap`: up to `capacity` (texture, index) entries in texture order; returns the entry count.
///
/// # Safety
/// `p` is live; the out arrays hold `capacity` values.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_material_map(
    p: *const EeGeTerrainMesh,
    textures: *mut *mut c_void,
    indices: *mut c_ulong,
    capacity: usize,
) -> usize {
    // SAFETY: the function's contract
    let map = unsafe { &(*p).terrain_map };
    for (k, (&texture, &index)) in map.iter().take(capacity).enumerate() {
        // SAFETY: k < capacity
        unsafe {
            *textures.add(k) = texture as *mut c_void;
            *indices.add(k) = index;
        }
    }
    map.len()
}

/// The skirt vertices (`cliff` false: `GetEdgeRenderVertices`) or the cliff vertices, as pointers.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_edge_vertices(
    p: *const EeGeTerrainMesh,
    cliff: bool,
) -> *const *mut EeGeTmRenderVertex {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    if cliff {
        m.cliff_vertices.as_ptr()
    } else {
        m.edge_vertices.as_ptr()
    }
}

/// The skirt (or cliff) vertices' world points.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tm_edge_points(p: *const EeGeTerrainMesh, cliff: bool) -> *const Ge3DPoint {
    // SAFETY: the function's contract
    let m = unsafe { &*p };
    if cliff {
        m.cliff_points.as_ptr()
    } else {
        m.edge_points.as_ptr()
    }
}

#[cfg(test)]
#[path = "tmobj_tests.rs"]
mod tests;

#[cfg(test)]
#[path = "tmobj_golden.rs"]
mod golden;
