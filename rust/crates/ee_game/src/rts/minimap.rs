//! `RTSMiniMap`'s bodies (LLE round 8, from `src/RTS Engine/Map/RTSMiniMap.cpp`): the mini map's textures, unit
//! layer, locks and battle-flag grid, their release, and `Ping`.
//!
//! The class stays C++ (`ee/EEMiniMap.h`: EEMiniMap derives from it); its members are reached through one-statement
//! shims of `RTSMiniMap.cpp` ([`MiniMapCpp`]); the locks are Rust objects (ee_base ts).

use core::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::Ge2DPoint;

ee_base::cpp_shims! {
    trait MiniMapCpp;
    fn set_viewing_player(map: usize, player: usize) = ee_rtsmmcpp_set_viewing_player;
    fn terrain_slot(map: usize) -> *mut *mut c_void = ee_rtsmmcpp_terrain_slot;
    fn world_slot(map: usize) -> *mut *mut c_void = ee_rtsmmcpp_world_slot;
    fn unit_slot(map: usize) -> *mut *mut c_void = ee_rtsmmcpp_unit_slot;
    fn terrain_lock_slot(map: usize) -> *mut *mut c_void = ee_rtsmmcpp_terrain_lock_slot;
    fn unit_lock_slot(map: usize) -> *mut *mut c_void = ee_rtsmmcpp_unit_lock_slot;
    fn flags_slot(map: usize) -> *mut *mut u8 = ee_rtsmmcpp_flags_slot;
    fn flags_width_slot(map: usize) -> *mut c_long = ee_rtsmmcpp_flags_width_slot;
    fn flags_height_slot(map: usize) -> *mut c_long = ee_rtsmmcpp_flags_height_slot;
    fn flags_new() -> *mut u8 = ee_rtsmmcpp_flags_new;
    fn flags_delete(flags: *mut u8) = ee_rtsmmcpp_flags_delete;
    fn map_to_cell(map: usize, point: *const Ge2DPoint, out: *mut Ge2DPoint) = ee_rtsmmcpp_map_to_cell;
    fn cells_push(map: usize, x: u8, y: u8) = ee_rtsmmcpp_cells_push;
    fn screen_create_surface(screen: usize, description: *const c_void, slot: *mut *mut c_void) -> c_long = ee_rtsmcpp_screen_create_surface;
    fn make_texture(screen: usize, surface: usize) -> usize = ee_rtsmmcpp_make_texture;
    fn texture_reference(texture: usize) = ee_rtsmmcpp_texture_reference;
    fn texture_dereference(texture: usize) = ee_rtsmmcpp_texture_dereference;
    fn texture_min_filter(texture: usize, mode: c_ulong) = ee_rtsmmcpp_texture_min_filter;
    fn texture_mag_filter(texture: usize, mode: c_ulong) = ee_rtsmmcpp_texture_mag_filter;
    fn texture_mip_filter(texture: usize, mode: c_ulong) = ee_rtsmmcpp_texture_mip_filter;
    fn texture_addressing(texture: usize, mode: c_ulong) = ee_rtsmmcpp_texture_addressing;
    fn surf_clear_black(surface: usize) = ee_rtsmmcpp_surf_clear_black;
    fn surf_dereference(surface: usize) = ee_rtsgcpp_surf_dereference;
}

const OK: c_long = 0;
/// `kSurfaceTexture`, `kSurfaceSystemMemory`.
const SURFACE_TEXTURE: c_ulong = 5;
const SURFACE_SYSTEM_MEMORY: c_ulong = 4;
/// `kPixelFormatR5G6B5`, `kPixelFormatA4R4G4B4`.
const FORMAT_R5G6B5: c_ulong = 7;
const FORMAT_A4R4G4B4: c_ulong = 2;
/// `kMipFilterNone`, `kAddressingClamp`.
const MIP_NONE: c_ulong = 0;
const ADDRESSING_CLAMP: c_ulong = 0;

fn description(kind: c_ulong, format: c_ulong, depth: c_ulong, volatility: c_ulong, side: c_ulong) -> ee_render::ge::texobj::EeGeSurfaceDescription {
    ee_render::ge::texobj::EeGeSurfaceDescription {
        kind,
        pixel_format: format,
        surface_volatility: volatility,
        width: side,
        height: side,
        m14: 0,
        mip_level_count: 0,
        pixel_depth: depth,
        is_3d_renderable: false,
        is_transparency_enabled: false,
        no_alpha: false,
    }
}

/// The texture made of a new surface, stored in its slot.
fn texture_of<C: MiniMapCpp>(c: &mut C, screen: usize, surface: usize, slot: *mut *mut c_void) -> usize {
    let t = c.make_texture(screen, surface);
    // SAFETY: the C++ object's texture slot
    unsafe { *slot = t as *mut c_void };
    t
}

/// `RTSMiniMap::RTSMiniMap`: a 128x128 terrain texture (R5G6B5), a 64x64 world texture (A4R4G4B4) and a 64x64 unit
/// layer in system memory, both cleared to black, their locks and an empty 64x64 battle-flag grid.
///
/// # Safety
/// `map` is the C++ object under construction, `screen` a live screen.
pub unsafe fn construct<C: MiniMapCpp>(
    c: &mut C,
    map: usize,
    player: usize,
    screen: usize,
    verify: &mut dyn FnMut(bool, c_ulong, &'static [u8]),
    new_section: &mut dyn FnMut() -> *mut c_void,
) {
    c.set_viewing_player(map, player);
    unsafe {
        *c.terrain_slot(map) = core::ptr::null_mut();
        *c.world_slot(map) = core::ptr::null_mut();
        *c.unit_slot(map) = core::ptr::null_mut();
    }
    // the terrain texture
    let d = description(SURFACE_TEXTURE, FORMAT_R5G6B5, 0x10, 1, 0x80);
    let mut surface: *mut c_void = core::ptr::null_mut();
    let r = c.screen_create_surface(screen, &d as *const _ as *const c_void, &mut surface);
    verify(r == OK, 61, b"theResult == keseOK");
    let t = {
        let slot = c.terrain_slot(map);
        texture_of(c, screen, surface as usize, slot)
    };
    verify(t != 0, 65, b"this->mTerrainTexture != NULL");
    setup_texture(c, t, 1);
    c.surf_dereference(surface as usize);
    // the world texture (its depth: the previous description's)
    let d = description(SURFACE_TEXTURE, FORMAT_A4R4G4B4, 0x10, 0, 0x40);
    let r = c.screen_create_surface(screen, &d as *const _ as *const c_void, &mut surface);
    verify(r == OK, 91, b"theResult == keseOK");
    c.surf_clear_black(surface as usize);
    let t = {
        let slot = c.world_slot(map);
        texture_of(c, screen, surface as usize, slot)
    };
    verify(t != 0, 98, b"this->mWorldTexture != NULL");
    setup_texture(c, t, 0);
    c.surf_dereference(surface as usize);
    // the unit layer
    let d = description(SURFACE_SYSTEM_MEMORY, FORMAT_A4R4G4B4, 0x10, 0, 0x40);
    let slot = c.unit_slot(map);
    let r = c.screen_create_surface(screen, &d as *const _ as *const c_void, slot);
    verify(r == OK, 124, b"theResult == keseOK");
    c.surf_clear_black(unsafe { *slot } as usize);
    unsafe {
        *c.terrain_lock_slot(map) = new_section();
        verify(true, 131, b"TSScheduler::CreateCriticalSection(this->mTerrainCriticalSection) == keseOK");
        *c.unit_lock_slot(map) = new_section();
        verify(true, 132, b"TSScheduler::CreateCriticalSection(this->mUnitCriticalSection) == keseOK");
        *c.flags_width_slot(map) = 0x40;
        *c.flags_height_slot(map) = 0x40;
        let flags = c.flags_new();
        *c.flags_slot(map) = flags;
        verify(!flags.is_null(), 138, b"this->mBattleFlags != NULL");
        core::ptr::write_bytes(flags, 0, 0x1000);
    }
}

fn setup_texture<C: MiniMapCpp>(c: &mut C, t: usize, filter: c_ulong) {
    c.texture_reference(t);
    c.texture_min_filter(t, filter);
    c.texture_mag_filter(t, filter);
    c.texture_mip_filter(t, MIP_NONE);
    c.texture_addressing(t, ADDRESSING_CLAMP);
}

/// `RTSMiniMap::~RTSMiniMap`: the locks, the textures, the unit layer, the battle-flag grid.
///
/// # Safety
/// `map` is the C++ object being destroyed.
pub unsafe fn destruct<C: MiniMapCpp>(c: &mut C, map: usize, free_section: &mut dyn FnMut(*mut c_void)) {
    unsafe {
        free_section(*c.terrain_lock_slot(map));
        free_section(*c.unit_lock_slot(map));
        let t = *c.terrain_slot(map) as usize;
        c.texture_dereference(t);
        let t = *c.world_slot(map) as usize;
        c.texture_dereference(t);
        let s = *c.unit_slot(map) as usize;
        c.surf_dereference(s);
        let flags = *c.flags_slot(map);
        if !flags.is_null() {
            c.flags_delete(flags);
        }
    }
}

/// `RTSMiniMap::Ping`: the cell under a map point (its centre through the map-to-cell transformation, truncated to
/// bytes) flagged as a fresh battle (8), queued when it was not flagged.
///
/// # Safety
/// `map` is the live object.
pub unsafe fn ping<C: MiniMapCpp>(c: &mut C, map: usize, point: Ge2DPoint) {
    let center = Ge2DPoint { x: point.x + 0.5, y: point.y + 0.5 };
    let mut cell = Ge2DPoint { x: 0.0, y: 0.0 };
    c.map_to_cell(map, &center, &mut cell);
    // (unsigned char)float: the conversion through int the C++ compiles to
    let x = cell.x as i32 as u8;
    let y = cell.y as i32 as u8;
    unsafe {
        let width = *c.flags_width_slot(map);
        let at = (*c.flags_slot(map)).wrapping_offset((y as c_long * width + x as c_long) as isize);
        if *at == 0 {
            c.cells_push(map, x, y);
        }
        *at = 8;
    }
}

#[cfg(not(test))]
mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    fn verify(ok: bool, line: c_ulong, expression: &'static [u8]) {
        if !ok {
            let mut e = expression.to_vec();
            e.push(0);
            // SAFETY: C strings
            unsafe { ee_base::error::ee_error_verify(c"C:\\EEX\\RTS Engine\\Map\\RTSMiniMap.cpp".as_ptr(), line, e.as_ptr().cast()) };
        }
    }

    /// `RTSMiniMap::RTSMiniMap`.
    ///
    /// # Safety
    /// `map` is the C++ object under construction, `screen` a live screen.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_minimap_construct(map: *mut c_void, player: *mut c_void, screen: *mut c_void) {
        let mut section = || ee_base::ts::ee_ts_section_new().cast::<c_void>();
        unsafe { construct(&mut Ffi, map as usize, player as usize, screen as usize, &mut verify, &mut section) }
    }

    /// `RTSMiniMap::~RTSMiniMap`.
    ///
    /// # Safety
    /// `map` is the C++ object being destroyed.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_minimap_destruct(map: *mut c_void) {
        let mut free = |s: *mut c_void| unsafe { ee_base::ts::ee_ts_section_free(s.cast()) };
        unsafe { destruct(&mut Ffi, map as usize, &mut free) }
    }

    /// `RTSMiniMap::Ping`.
    ///
    /// # Safety
    /// `map` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_minimap_ping(map: *mut c_void, point: *const Ge2DPoint) {
        unsafe { ping(&mut Ffi, map as usize, *point) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    #[test]
    fn ping_flags_a_cell_once_and_queues_it_once() {
        let mut flags = vec![0u8; 0x1000];
        let mut fp = flags.as_mut_ptr();
        let mut width: c_long = 0x40;
        let mut c = ShimFake::default();
        c.answer("flags_width_slot", &[&mut width as *mut _ as i64; 4]);
        c.answer("flags_slot", &[&mut fp as *mut _ as i64; 4]);
        unsafe {
            ping(&mut c, 1, Ge2DPoint { x: 0.0, y: 0.0 });
            ping(&mut c, 1, Ge2DPoint { x: 0.0, y: 0.0 });
        }
        // the fake's map_to_cell leaves (0, 0)
        assert_eq!(flags[0], 8);
        assert_eq!(c.count("cells_push"), 1);
    }

    #[test]
    fn construction_makes_three_surfaces_two_textures_and_the_grid() {
        let mut slots = [core::ptr::null_mut::<c_void>(); 5];
        let mut flags_slot: *mut u8 = core::ptr::null_mut();
        let (mut w, mut h): (c_long, c_long) = (0, 0);
        let mut grid = vec![1u8; 0x1000];
        let mut c = ShimFake::default();
        for (i, n) in ["terrain_slot", "world_slot", "unit_slot", "terrain_lock_slot", "unit_lock_slot"].into_iter().enumerate() {
            c.answer(n, &[&mut slots[i] as *mut _ as i64; 4]);
        }
        c.answer("flags_slot", &[&mut flags_slot as *mut _ as i64]);
        c.answer("flags_width_slot", &[&mut w as *mut _ as i64]);
        c.answer("flags_height_slot", &[&mut h as *mut _ as i64]);
        c.answer("flags_new", &[grid.as_mut_ptr() as i64]);
        c.answer("make_texture", &[0x50, 0x51]);
        let mut lines = Vec::new();
        let mut section = || 0x99 as *mut c_void;
        unsafe { construct(&mut c, 1, 2, 3, &mut |ok, l, _| lines.push((ok, l)), &mut section) };
        assert_eq!(lines.iter().map(|l| l.1).collect::<Vec<_>>(), [61, 65, 91, 98, 124, 131, 132, 138]);
        assert!(lines.iter().all(|l| l.0));
        assert_eq!(c.count("screen_create_surface"), 3);
        assert_eq!(c.count("surf_clear_black"), 2);
        assert!(c.calls().contains("texture_min_filter(80, 1)") && c.calls().contains("texture_min_filter(81, 0)"));
        assert_eq!((w, h), (0x40, 0x40));
        assert!(grid.iter().all(|&b| b == 0));
        assert_eq!(slots[0] as usize, 0x50);
    }
}
