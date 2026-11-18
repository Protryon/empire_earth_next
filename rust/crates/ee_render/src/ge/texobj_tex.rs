//! `GETexture` as a whole Rust object (R4.3, eighth part): a texture's stage, frames, surface, screen, sizes and
//! flags with every method of GETexture.cpp (the constructors' fields, the destructor's releases, `Load`'s two frame
//! formats, `Recreate`, the stage setters over every frame, `SetFrame`, `SetSurface`, the texture pack's logical
//! shift) and the header's accessors.
//!
//! As the managers (`manobj.rs`, `texobj.rs`): the C++ `GETexture` stays the type the game, the renderer and the
//! managers use; its methods are one-line forwarders and its only member is this object. Every call the original
//! made on another object (its surfaces, its screen, the rasterizer, its manager, its frames' textures, the decoded
//! file) is one host operation (`EeGeTexOp`, `GETexture::Host`), in the original's order. A frame texture's fields
//! (format 0: one texture per frame) are read and written on its own object, which the host hands over
//! (`EeGeTexOp::TexObj`). No reference to an object lives across a host call.
//!
//! The stage is a `#[repr(C)]` mirror of `GETStage` and the frame corner of `GETextureCoordinate`
//! (GETexture.cpp asserts both layouts): `GetStage()` and `GetFrameTopLeftCorner()` return pointers into the object.

use std::ffi::{c_long, c_ulong, c_void};

use crate::ge::texobj::EeGeSurfaceDescription;
use crate::ge::texture;

/// `kErrorOK`, `kErrorAlreadyExists`, `kErrorOutOfMemory` (lle/LowLevelEngineCore.h).
const OK: c_long = 0;
const ALREADY_EXISTS: c_long = 2;
const OUT_OF_MEMORY: c_long = 8;
/// `kStageOpTexture`, `kAddressingClamp`, `kMinFilterPoint`, `kMagFilterPoint`, `kMipFilterPoint`
/// (lle/GERenderState.h).
const STAGE_OP_TEXTURE: c_ulong = 1;
const ADDRESSING_CLAMP: c_ulong = 0;
const MIN_FILTER_POINT: c_ulong = 0;
const MAG_FILTER_POINT: c_ulong = 0;
const MIP_FILTER_POINT: c_ulong = 1;
/// `kSurfaceTexture` (a file has one MIP level: frame format 1's surfaces are never `kSurfaceMipMappedTexture`).
const SURFACE_TEXTURE: c_ulong = 5;

/// `GETStage` (lle/GETexture.h), field for field.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGeTStage {
    /// `mTexture` (`GETexture*`).
    pub texture: *mut c_void,
    pub color_mode: c_ulong,
    pub alpha_mode: c_ulong,
    pub texture_addressing_mode: c_ulong,
    pub min_filter_mode: c_ulong,
    pub mag_filter_mode: c_ulong,
    pub mip_filter_mode: c_ulong,
    pub mip_map_bias: f32,
}

/// `GETextureCoordinate`: u, v.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGeTexCoord {
    pub u: f32,
    pub v: f32,
}

/// A rectangle as GERectangle's setters fill it: left, top, right, bottom.
pub type EeGeTexRect = [f32; 4];

/// A copy between two surfaces (`EeGeTexOp::SurfCopyTo` / `SurfCopyWithFilter`'s `b`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGeTexCopy {
    pub source: EeGeTexRect,
    pub destination: EeGeTexRect,
    /// `CopyTo`'s `inTransparent` (always false here).
    pub transparent: bool,
}

/// The calls a texture makes on other objects. `obj` is the object called; `a`..`c` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTexOp {
    /// `obj->Reference()` (a surface).
    SurfReference = 1,
    /// `obj->Dereference()` (a surface).
    SurfDereference,
    /// `obj->GetParentScreen()`.
    SurfParentScreen,
    /// `obj->GetAllocatedSize()`.
    SurfAllocatedSize,
    /// `obj->GetLogicalShift()`.
    SurfLogicalShift,
    /// `obj->SetLogicalShift(a)`.
    SurfSetLogicalShift,
    /// `obj->Lock()`.
    SurfLock,
    /// `obj->Unlock()`.
    SurfUnlock,
    /// `obj->GetMemoryAddress()`.
    SurfMemory,
    /// `obj->GetBytesPerRow()`.
    SurfBytesPerRow,
    /// `*(GESurfaceDescription*)a = obj->GetSurfaceDescription()`.
    SurfDescription,
    /// `obj->GetAttachedSurface(0)`.
    SurfAttached,
    /// `obj->GetAttachedSurfaceCount()`.
    SurfAttachedCount,
    /// `obj->GetWidth()` (logical).
    SurfWidth,
    /// `obj->GetHeight()` (logical).
    SurfHeight,
    /// `obj->CopyWithFilter(a, b's source, b's destination)` (`b` an `EeGeTexCopy`).
    SurfCopyWithFilter,
    /// `obj->CopyTo(a, b's source, b's destination, b's transparent)`.
    SurfCopyTo,
    /// `((GEScreen*)obj)->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b)`.
    ScreenCreateSurface,
    /// `ee_of_texture_info(obj, (EeOfTextureInfo*)a)`: the decoded file's width, height and frame count.
    FileInfo,
    /// `ee_of_texture_frame(a, b, &frame)`, then `GEStoreFrame(obj, frame)`: frame `b` of file `a` into surface `obj`.
    StoreFrame,
    /// `new GETexture((GETextureManager*)obj)`: a frame's texture.
    TexNew,
    /// The Rust object of texture `obj`.
    TexObj,
    /// `((GETexture*)obj)->Dereference()`.
    TexDereference,
    /// `((GETexture*)obj)->UReferenceCounter::Dereference()`.
    TexBaseDereference,
    /// `((GETexture*)obj)->mReferenceCount`.
    TexRefCount,
    /// `((GETextureManager*)obj)->RemoveTexture((GETexture*)a)`.
    ManagerRemove,
    /// `((GETextureManager*)obj)->PromoteTexture((GETexture*)a)`.
    ManagerPromote,
    /// `GETexture::sRasterizer->CreateTextureHandle((GETexture*)obj)`.
    RasterCreate,
    /// `GETexture::sRasterizer->DestroyTextureHandle((GETexture*)obj)`.
    RasterDestroy,
}

/// The host: `owner` is the C++ texture of the object calling.
pub type EeGeTexHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeTexOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize;

/// A field the header's accessors read or write (`ee_ge_tex_get` / `ee_ge_tex_set`). The `Frame...` ones are the
/// current frame's texture's (`mFrameTexture->m...`), the others this texture's own.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTexVal {
    /// `mFrameTexture->mData`.
    FrameData = 1,
    /// `mFrameTexture->mSurface`.
    FrameSurface,
    /// `mFrameTexture->mIsCompressed`.
    FrameIsCompressed,
    /// `mFrameTexture->mIsPromoted`.
    FrameIsPromoted,
    /// `mCreateMIPMaps`.
    CreateMipMaps,
    /// `mHasChanged` (set: true, or false for `ValidateChanges`).
    HasChanged,
    /// `mFrame`.
    Frame,
    /// `mFrameCount`.
    FrameCount,
    /// `mMipLevelCount` (signed).
    MipLevelCount,
    /// `mNumMipLevelsToDrop`.
    NumMipLevelsToDrop,
    /// `mScreen`.
    Screen,
    /// `mSurface`.
    Surface,
    /// `mFrameFormat`.
    FrameFormat,
    /// `mIsPromoted`.
    IsPromoted,
    /// `mIsManaged`.
    IsManaged,
    /// `mFrameTexture`.
    FrameTexture,
    /// `mFramesPerRow`.
    FramesPerRow,
    /// `mData`.
    Data,
    /// `mIsCompressed`.
    IsCompressed,
    /// `mIsFromTexturePack`.
    FromPack,
    /// `mTexturePackMipLevels`.
    PackMipLevels,
    /// `mManager`.
    Manager,
}

/// A stage field (`ee_ge_tex_set_stage`): each `SetStage...` setter.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTexStageField {
    AlphaMode = 1,
    ColorMode,
    MagFilterMode,
    MinFilterMode,
    MipFilterMode,
    TextureAddressingMode,
}

/// The texture.
pub struct EeGeTexture {
    host: EeGeTexHostFn,
    /// The C++ texture (`this`).
    owner: usize,
    stage: EeGeTStage,
    /// `mFrameTopLeftCorner`.
    corner: EeGeTexCoord,
    is_managed: bool,
    screen: usize,
    surface: usize,
    /// `mFrameTexture`: the current frame's texture (the texture itself when unframed).
    frame_texture: usize,
    /// `mFrames` (`new GETexture*[count]`, `None` for NULL); frame format 0 only.
    frames: Option<Vec<usize>>,
    manager: usize,
    data: usize,
    frame_uv_size: f32,
    frame_format: c_ulong,
    frame: c_ulong,
    frame_count: c_ulong,
    frames_per_row: c_ulong,
    mip_level_count: c_long,
    num_mip_levels_to_drop: c_ulong,
    is_compressed: bool,
    is_promoted: bool,
    has_changed: bool,
    /// Unset by the constructors in the original (set by `Load`); false here.
    create_mipmaps: bool,
    is_from_texture_pack: bool,
    texture_pack_mip_levels: c_long,
    /// Made by the C++ (`ee_ge_tex_new`): the other textures it reaches are `GETexture`s, read in place (LLE round 8:
    /// `TexObj`, `TexRefCount` and the non-virtual `Dereference` are answered here, not by the host).
    direct: bool,
}

/// Every access to an object is one short statement: a host call may come back in and use it.
///
/// # Safety
/// `p` is a live object from `ee_ge_tex_new`.
unsafe fn tx<'a>(p: *mut EeGeTexture) -> &'a mut EeGeTexture {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

impl EeGeTexture {
    /// A host call made by texture `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeTexOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeTexOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner, direct) = unsafe { (tx(p).host, tx(p).owner, tx(p).direct) };
        if direct {
            // SAFETY: a C++ texture's textures are GETextures (cppobj.rs's layout)
            match op {
                EeGeTexOp::TexObj => return unsafe { crate::ge::cppobj::counted::<Self>(obj) as isize },
                EeGeTexOp::TexRefCount => return unsafe { crate::ge::cppobj::reference_count(obj) as isize },
                EeGeTexOp::TexDereference => {
                    unsafe { ee_ge_tex_dereference(crate::ge::cppobj::counted::<Self>(obj)) };
                    return 0;
                }
                _ => {}
            }
        }
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b, 0) }
    }

    /// The Rust object of texture `tex` (`p`'s own for `p`'s owner).
    ///
    /// # Safety
    /// `p` is live; `tex` a live texture.
    unsafe fn obj(p: *mut Self, tex: usize) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe {
            if tex == tx(p).owner {
                return p;
            }
            Self::op(p, EeGeTexOp::TexObj, tex, 0, 0) as *mut Self
        }
    }

    /// # Safety
    /// `p` is live; `surface` a surface.
    unsafe fn surf(p: *mut Self, op: EeGeTexOp, surface: usize) -> isize {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, op, surface, 0, 0) }
    }

    /// `GetWidth()` and `GetHeight()` of a surface, as floats.
    ///
    /// # Safety
    /// `p` is live; `surface` a surface.
    unsafe fn surf_size(p: *mut Self, surface: usize) -> (f32, f32) {
        // SAFETY: the caller's contract
        unsafe {
            let w = Self::surf(p, EeGeTexOp::SurfWidth, surface) as c_ulong as f32;
            let h = Self::surf(p, EeGeTexOp::SurfHeight, surface) as c_ulong as f32;
            (w, h)
        }
    }

    /// `screen->CreateSurface(description, &surface)`.
    ///
    /// # Safety
    /// `p` is live; `screen` a screen.
    unsafe fn create_surface(p: *mut Self, screen: usize, d: &mut EeGeSurfaceDescription, out: &mut usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            Self::op(
                p,
                EeGeTexOp::ScreenCreateSurface,
                screen,
                d as *mut EeGeSurfaceDescription as isize,
                out as *mut usize as isize,
            ) as c_long
        }
    }

    /// `from->CopyTo` or `CopyWithFilter` (`op`) into `to`.
    ///
    /// # Safety
    /// `p` is live; `from` and `to` surfaces.
    unsafe fn copy(
        p: *mut Self,
        op: EeGeTexOp,
        from: usize,
        to: usize,
        source: EeGeTexRect,
        destination: EeGeTexRect,
    ) -> c_long {
        let copy = EeGeTexCopy { source, destination, transparent: false };
        // SAFETY: the caller's contract
        unsafe { Self::op(p, op, from, to as isize, &copy as *const EeGeTexCopy as isize) as c_long }
    }

    /// The constructor from a manager: an empty single-frame texture, point filtering, clamped addressing.
    pub fn create(host: EeGeTexHostFn, owner: usize, manager: usize) -> *mut Self {
        Box::into_raw(Box::new(Self {
            host,
            owner,
            stage: EeGeTStage {
                texture: owner as *mut c_void,
                color_mode: STAGE_OP_TEXTURE,
                alpha_mode: STAGE_OP_TEXTURE,
                texture_addressing_mode: ADDRESSING_CLAMP,
                min_filter_mode: MIN_FILTER_POINT,
                mag_filter_mode: MAG_FILTER_POINT,
                mip_filter_mode: MIP_FILTER_POINT,
                mip_map_bias: 0.0,
            },
            corner: EeGeTexCoord::default(),
            is_managed: false,
            screen: 0,
            surface: 0,
            frame_texture: owner,
            frames: None,
            manager,
            data: 0,
            frame_uv_size: 1.0,
            frame_format: 1,
            frame: 0,
            frame_count: 1,
            frames_per_row: 1,
            mip_level_count: 0,
            num_mip_levels_to_drop: 0,
            is_compressed: false,
            is_promoted: false,
            has_changed: true,
            create_mipmaps: false,
            is_from_texture_pack: false,
            texture_pack_mip_levels: 0,
            direct: false,
        }))
    }

    /// The constructor around a surface (after `create`): the surface referenced, its screen, the rasterizer's handle.
    ///
    /// # Safety
    /// `p` is live; `surface` a surface.
    pub unsafe fn construct_on_surface(p: *mut Self, surface: usize) {
        // SAFETY: the caller's contract
        unsafe {
            tx(p).surface = surface;
            Self::surf(p, EeGeTexOp::SurfReference, surface);
            let screen = Self::surf(p, EeGeTexOp::SurfParentScreen, surface) as usize;
            tx(p).screen = screen;
            let owner = tx(p).owner;
            Self::op(p, EeGeTexOp::RasterCreate, owner, 0, 0);
        }
    }

    /// The destructor: the extra frames released (format 0) and frame 0 shown, the rasterizer's handle destroyed, the
    /// surface released; the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            if tx(p).frame_format == 0 {
                let mut i: c_ulong = 1;
                while i < tx(p).frame_count {
                    let frame = Self::frame_at(p, i);
                    if frame != 0 {
                        Self::op(p, EeGeTexOp::TexBaseDereference, frame, 0, 0);
                    }
                    i += 1;
                }
                Self::set_frame(p, 0);
                tx(p).frames = None;
            }
            let owner = tx(p).owner;
            Self::op(p, EeGeTexOp::RasterDestroy, owner, 0, 0);
            let surface = tx(p).surface;
            if surface != 0 {
                Self::surf(p, EeGeTexOp::SurfDereference, surface);
                tx(p).surface = 0;
            }
            drop(Box::from_raw(p));
        }
    }

    /// `mFrames[i]` (NULL past the array or without one: the original read past it).
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn frame_at(p: *mut Self, i: c_ulong) -> usize {
        // SAFETY: the caller's contract
        unsafe { tx(p).frames.as_ref().and_then(|f| f.get(i as usize).copied()).unwrap_or(0) }
    }

    /// `Dereference`: one reference dropped while others remain; at the last outside reference a managed texture
    /// removes itself from its manager.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn dereference(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let (owner, manager, managed) = (tx(p).owner, tx(p).manager, tx(p).is_managed);
            let count = Self::op(p, EeGeTexOp::TexRefCount, owner, 0, 0) as c_ulong;
            let (dereference, remove) = texture::dereference_decision(count, managed);
            if dereference {
                Self::op(p, EeGeTexOp::TexBaseDereference, owner, 0, 0);
            }
            if remove {
                // the original read `mManager` here, after the reference it dropped (one is left)
                Self::op(p, EeGeTexOp::ManagerRemove, manager, owner as isize, 0);
            }
        }
    }

    /// `GetAllocatedSize`.
    ///
    /// # Safety
    /// `p` is live with a surface.
    pub unsafe fn allocated_size(p: *mut Self) -> c_ulong {
        // SAFETY: the caller's contract
        unsafe {
            let surface = tx(p).surface;
            let size = Self::surf(p, EeGeTexOp::SurfAllocatedSize, surface) as c_ulong;
            texture::allocated_size(size, tx(p).frame_format, tx(p).frame_count)
        }
    }

    /// `Load`: the texture (and its frames) from a decoded file. Frame format 0 makes one texture per frame, each
    /// with its own surface (MIP levels filtered down, levels dropped into a smaller surface); format 1 puts the
    /// frames into the cells of one grid surface. `description` is read and written as the original's reference.
    ///
    /// # Safety
    /// `p` is live; `file` a decoded file; `screen` a screen.
    pub unsafe fn load(
        p: *mut Self,
        file: usize,
        description: &mut EeGeSurfaceDescription,
        screen: usize,
        create_mipmaps: bool,
        num_mip_levels_to_drop: c_long,
        frame_format: c_ulong,
    ) -> c_long {
        let mut drop_levels = num_mip_levels_to_drop;
        let mut source: EeGeTexRect = [0.0; 4];
        let mut destination: EeGeTexRect = [0.0; 4];
        let mut result: c_long;
        // SAFETY: the caller's contract; every object below comes from the host
        unsafe {
            let surface = tx(p).surface;
            if surface != 0 {
                Self::surf(p, EeGeTexOp::SurfDereference, surface);
                tx(p).surface = 0;
            }
            if tx(p).frame_format == 0 {
                let mut i: c_ulong = 1;
                while i < tx(p).frame_count {
                    let frame = Self::frame_at(p, i);
                    if frame != 0 {
                        Self::op(p, EeGeTexOp::TexDereference, frame, 0, 0);
                    }
                    i += 1;
                }
                tx(p).frames = None;
            }
            tx(p).num_mip_levels_to_drop = drop_levels as c_ulong;
            tx(p).create_mipmaps = create_mipmaps;
            let mut info = [0u32; 3];
            Self::op(p, EeGeTexOp::FileInfo, file, info.as_mut_ptr() as isize, 0);
            let [width, height, count] = info;
            result = OK;
            if frame_format == 0 {
                let owner = tx(p).owner;
                tx(p).frame_format = 0;
                tx(p).frames = Some(vec![0; count as usize]);
                Self::set_frame_entry(p, 0, owner);
                tx(p).frame_texture = owner;
                for i in 1..count as usize {
                    let manager = tx(p).manager;
                    let frame = Self::op(p, EeGeTexOp::TexNew, manager, 0, 0) as usize;
                    Self::set_frame_entry(p, i, frame);
                }
                let mut desc = *description;
                desc.width = width as c_ulong;
                desc.height = height as c_ulong;
                desc.m14 = 0;
                let plan = texture::frame_plan(width, height, create_mipmaps, drop_levels);
                drop_levels = plan.drop;
                desc.kind = plan.surface_type;
                desc.mip_level_count = plan.mip_level_count;
                let mut drop_desc = EeGeSurfaceDescription::default();
                if plan.has_drop {
                    drop_desc = desc;
                    drop_desc.width = plan.drop_width;
                    drop_desc.height = plan.drop_height;
                    drop_desc.kind = plan.drop_type;
                    drop_desc.mip_level_count = plan.drop_mip_level_count;
                    *description = drop_desc;
                    tx(p).mip_level_count = drop_desc.mip_level_count;
                }
                tx(p).frame_count = count as c_ulong;
                for frame in 0..count {
                    let mut surface = 0usize;
                    if Self::create_surface(p, screen, &mut desc, &mut surface) == OK {
                        if Self::op(p, EeGeTexOp::StoreFrame, surface, file as isize, frame as isize) as c_long == OK {
                            let mut mip = surface;
                            let mut j: c_long = 1;
                            while j < desc.mip_level_count {
                                let next = Self::surf(p, EeGeTexOp::SurfAttached, mip) as usize;
                                (source[2], source[3]) = Self::surf_size(p, mip);
                                (destination[2], destination[3]) = Self::surf_size(p, next);
                                Self::copy(p, EeGeTexOp::SurfCopyWithFilter, mip, next, source, destination);
                                mip = next;
                                j += 1;
                            }
                        }
                    } else {
                        // no surface for this frame: reported (the original set a stale pointer)
                        result = OUT_OF_MEMORY;
                        continue;
                    }
                    if drop_levels > 0 {
                        let mut drop_surface = 0usize;
                        // the reduced surface replaces the frame's only when it was made
                        if Self::create_surface(p, screen, &mut drop_desc, &mut drop_surface) == OK {
                            let mut target = drop_surface;
                            let mut mip = surface;
                            for _ in 0..drop_levels {
                                mip = Self::surf(p, EeGeTexOp::SurfAttached, mip) as usize;
                            }
                            let mut j: c_long = 0;
                            while j < tx(p).mip_level_count {
                                (source[2], source[3]) = Self::surf_size(p, mip);
                                Self::copy(p, EeGeTexOp::SurfCopyTo, mip, target, source, source);
                                j += 1;
                                if j < tx(p).mip_level_count {
                                    mip = Self::surf(p, EeGeTexOp::SurfAttached, mip) as usize;
                                    target = Self::surf(p, EeGeTexOp::SurfAttached, target) as usize;
                                }
                            }
                            Self::surf(p, EeGeTexOp::SurfDereference, surface);
                            surface = drop_surface;
                        }
                    }
                    let texture = Self::frame_at(p, frame as c_ulong);
                    result = Self::set_surface(Self::obj(p, texture), surface);
                    if result == OK {
                        Self::surf(p, EeGeTexOp::SurfDereference, surface);
                    }
                }
            } else if frame_format == 1 {
                let grid = texture::grid_plan(count);
                tx(p).frames_per_row = grid.frames_per_row;
                tx(p).frame_uv_size = grid.uv_size;
                tx(p).frame_texture = tx(p).owner;
                tx(p).frame_format = 1;
                tx(p).frame_count = count as c_ulong;
                tx(p).corner = EeGeTexCoord { u: 0.0, v: 0.0 };
                description.width = width as c_ulong;
                description.height = height as c_ulong;
                description.m14 = 0;
                // the file has one MIP level
                description.kind = SURFACE_TEXTURE;
                description.mip_level_count = 1;
                let mut surface = 0usize;
                result = Self::create_surface(p, screen, description, &mut surface);
                if result == OK {
                    let per_row = tx(p).frames_per_row;
                    description.width = per_row.wrapping_mul(width as c_ulong);
                    description.height = per_row.wrapping_mul(height as c_ulong);
                    let mut grid_surface = 0usize;
                    result = Self::create_surface(p, screen, description, &mut grid_surface);
                    if result == OK {
                        // a failed store or copy ends the loops (the surfaces are released below)
                        let mut i: u32 = 0;
                        while i < count && result == OK {
                            let mut mip = surface;
                            let mut target = grid_surface;
                            result = Self::op(p, EeGeTexOp::StoreFrame, mip, file as isize, i as isize) as c_long;
                            if result == OK {
                                let (w, h) = Self::surf_size(p, mip);
                                source = [0.0, 0.0, w, h];
                                let mw = Self::surf(p, EeGeTexOp::SurfWidth, mip) as c_ulong;
                                let mh = Self::surf(p, EeGeTexOp::SurfHeight, mip) as c_ulong;
                                destination = texture::grid_cell(i as c_ulong, tx(p).frames_per_row, mw, mh);
                                result = Self::copy(p, EeGeTexOp::SurfCopyTo, mip, target, source, destination);
                                if result == OK {
                                    if Self::surf(p, EeGeTexOp::SurfAttachedCount, mip) as c_ulong > 0 {
                                        mip = Self::surf(p, EeGeTexOp::SurfAttached, mip) as usize;
                                    }
                                    if Self::surf(p, EeGeTexOp::SurfAttachedCount, target) as c_ulong > 0 {
                                        target = Self::surf(p, EeGeTexOp::SurfAttached, target) as usize;
                                    }
                                    let _ = (mip, target);
                                }
                            }
                            i += 1;
                        }
                        if result == OK {
                            result = Self::set_surface(p, grid_surface);
                            if result == OK {
                                Self::surf(p, EeGeTexOp::SurfDereference, grid_surface);
                            }
                        } else {
                            Self::surf(p, EeGeTexOp::SurfDereference, grid_surface);
                        }
                    }
                    // the single-frame staging surface (the original leaked it on every path)
                    Self::surf(p, EeGeTexOp::SurfDereference, surface);
                }
            }
        }
        result
    }

    /// `mFrames[i] = texture`.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn set_frame_entry(p: *mut Self, i: usize, texture: usize) {
        // SAFETY: the caller's contract
        if let Some(entry) = unsafe { tx(p).frames.as_mut() }.and_then(|f| f.get_mut(i)) {
            *entry = texture;
        }
    }

    /// `Lock`: the surface locked; its memory and row pitch.
    ///
    /// # Safety
    /// `p` is live with a surface.
    pub unsafe fn lock(p: *mut Self, memory: &mut usize, bytes_per_row: &mut c_ulong) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            let surface = tx(p).surface;
            let result = Self::surf(p, EeGeTexOp::SurfLock, surface) as c_long;
            if result == OK {
                *memory = Self::surf(p, EeGeTexOp::SurfMemory, tx(p).surface) as usize;
                *bytes_per_row = Self::surf(p, EeGeTexOp::SurfBytesPerRow, tx(p).surface) as c_ulong;
            }
            result
        }
    }

    /// `Unlock`.
    ///
    /// # Safety
    /// `p` is live with a surface.
    pub unsafe fn unlock(p: *mut Self) -> c_long {
        // SAFETY: the caller's contract
        unsafe { Self::surf(p, EeGeTexOp::SurfUnlock, tx(p).surface) as c_long }
    }

    /// `Recreate`: the surface made again on `screen` with its description (and its logical size).
    ///
    /// # Safety
    /// `p` is live with a surface; `screen` a screen.
    pub unsafe fn recreate(p: *mut Self, screen: usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            tx(p).screen = screen;
            let mut desc = EeGeSurfaceDescription::default();
            let surface = tx(p).surface;
            Self::op(p, EeGeTexOp::SurfDescription, surface, &mut desc as *mut EeGeSurfaceDescription as isize, 0);
            let shift = Self::surf(p, EeGeTexOp::SurfLogicalShift, surface);
            Self::surf(p, EeGeTexOp::SurfDereference, surface);
            tx(p).surface = 0;
            let mut created = 0usize;
            let screen = tx(p).screen;
            let result = Self::create_surface(p, screen, &mut desc, &mut created);
            tx(p).surface = created;
            if result == OK {
                Self::op(p, EeGeTexOp::SurfSetLogicalShift, created, shift, 0);
            }
            result
        }
    }

    /// The logical shift of every frame's surface (a texture pack's texture).
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_logical_shift(p: *mut Self, shift: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            let surface = tx(p).surface;
            if surface != 0 {
                Self::op(p, EeGeTexOp::SurfSetLogicalShift, surface, shift as isize, 0);
            }
            if tx(p).frame_format == 0 && tx(p).frames.is_some() {
                let mut i: c_ulong = 1;
                while i < tx(p).frame_count {
                    let frame = Self::frame_at(p, i);
                    if frame != 0 {
                        let frame_surface = tx(Self::obj(p, frame)).surface;
                        if frame_surface != 0 {
                            Self::op(p, EeGeTexOp::SurfSetLogicalShift, frame_surface, shift as isize, 0);
                        }
                    }
                    i += 1;
                }
            }
        }
    }

    /// A `SetStage...` setter: the field set on every frame's texture (format 0) or on the texture, each changed one
    /// marked changed.
    ///
    /// # Safety
    /// `p` is live; format 0's frames are live.
    unsafe fn set_stage_with<T: PartialEq + Copy>(p: *mut Self, value: T, field: fn(&mut EeGeTStage) -> &mut T) {
        // SAFETY: the caller's contract
        unsafe {
            if tx(p).frame_format == 0 {
                let mut i: c_ulong = 0;
                while i < tx(p).frame_count {
                    let texture = Self::frame_at(p, i);
                    if texture != 0 {
                        let frame = Self::obj(p, texture);
                        let slot = field(&mut tx(frame).stage);
                        if *slot != value {
                            *slot = value;
                            tx(frame).has_changed = true;
                        }
                    }
                    i += 1;
                }
            } else {
                let slot = field(&mut tx(p).stage);
                if *slot != value {
                    *slot = value;
                    tx(p).has_changed = true;
                }
            }
        }
    }

    /// `SetStageAlphaMode` .. `SetStageTextureAddressingMode` (the stage argument is unused).
    ///
    /// # Safety
    /// `p` is live; format 0's frames are live.
    pub unsafe fn set_stage(p: *mut Self, which: EeGeTexStageField, mut mode: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            match which {
                EeGeTexStageField::AlphaMode => Self::set_stage_with(p, mode, |s| &mut s.alpha_mode),
                EeGeTexStageField::ColorMode => Self::set_stage_with(p, mode, |s| &mut s.color_mode),
                EeGeTexStageField::MagFilterMode => Self::set_stage_with(p, mode, |s| &mut s.mag_filter_mode),
                EeGeTexStageField::MinFilterMode => Self::set_stage_with(p, mode, |s| &mut s.min_filter_mode),
                EeGeTexStageField::MipFilterMode => {
                    // a texture pack's texture is drawn smaller than its pixels: its MIP levels are used even where
                    // the game asks for none
                    let surface = tx(p).surface;
                    let shift =
                        if surface != 0 { Self::surf(p, EeGeTexOp::SurfLogicalShift, surface) as c_ulong } else { 0 };
                    mode = texture::mip_filter_mode(mode, surface != 0, shift, tx(p).create_mipmaps);
                    Self::set_stage_with(p, mode, |s| &mut s.mip_filter_mode)
                }
                EeGeTexStageField::TextureAddressingMode => {
                    Self::set_stage_with(p, mode, |s| &mut s.texture_addressing_mode)
                }
            }
        }
    }

    /// `SetFrame`: the frame's texture (format 0) or its cell's UV corner (format 1); marked changed.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_frame(p: *mut Self, frame: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            tx(p).frame = frame;
            if tx(p).frame_format == 0 {
                tx(p).frame_texture = Self::frame_at(p, frame);
            } else {
                let (u, v) = texture::frame_corner(frame, tx(p).frames_per_row);
                tx(p).corner = EeGeTexCoord { u, v };
            }
            tx(p).has_changed = true;
        }
    }

    /// `SetSurface`: a new surface (referenced; the old one's handle destroyed and the surface released, the new
    /// one's handle made, promotion cleared); `kErrorAlreadyExists` for the same surface.
    ///
    /// # Safety
    /// `p` is live; `surface` a surface.
    pub unsafe fn set_surface(p: *mut Self, surface: usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            if surface == tx(p).surface {
                return ALREADY_EXISTS;
            }
            let owner = tx(p).owner;
            if tx(p).surface != 0 {
                Self::op(p, EeGeTexOp::RasterDestroy, owner, 0, 0);
                let old = tx(p).surface;
                Self::surf(p, EeGeTexOp::SurfDereference, old);
            }
            tx(p).surface = surface;
            Self::surf(p, EeGeTexOp::SurfReference, surface);
            let screen = Self::surf(p, EeGeTexOp::SurfParentScreen, surface) as usize;
            tx(p).screen = screen;
            tx(p).is_promoted = false;
            Self::op(p, EeGeTexOp::RasterCreate, owner, 0, 0);
            tx(p).has_changed = true;
            OK
        }
    }

    /// `Promote`: the current frame's texture promoted by the manager unless it is.
    ///
    /// # Safety
    /// `p` is live; the current frame's texture is live.
    pub unsafe fn promote(p: *mut Self) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            let frame = tx(p).frame_texture;
            if tx(Self::obj(p, frame)).is_promoted {
                return OK;
            }
            let manager = tx(p).manager;
            Self::op(p, EeGeTexOp::ManagerPromote, manager, frame as isize, 0) as c_long
        }
    }

    /// The current frame's texture's object.
    ///
    /// # Safety
    /// `p` is live; the current frame's texture is live.
    unsafe fn current(p: *mut Self) -> *mut Self {
        // SAFETY: the caller's contract
        unsafe { Self::obj(p, tx(p).frame_texture) }
    }

    /// A field (`EeGeTexVal`) as an integer.
    ///
    /// # Safety
    /// `p` is live; for the `Frame...` fields the current frame's texture is live.
    pub unsafe fn get(p: *mut Self, field: EeGeTexVal) -> isize {
        // SAFETY: the caller's contract
        unsafe {
            match field {
                EeGeTexVal::FrameData => return tx(Self::current(p)).data as isize,
                EeGeTexVal::FrameSurface => return tx(Self::current(p)).surface as isize,
                EeGeTexVal::FrameIsCompressed => return tx(Self::current(p)).is_compressed as isize,
                EeGeTexVal::FrameIsPromoted => return tx(Self::current(p)).is_promoted as isize,
                _ => {}
            }
            let t = tx(p);
            match field {
                EeGeTexVal::FrameData
                | EeGeTexVal::FrameSurface
                | EeGeTexVal::FrameIsCompressed
                | EeGeTexVal::FrameIsPromoted => 0,
                EeGeTexVal::CreateMipMaps => t.create_mipmaps as isize,
                EeGeTexVal::HasChanged => t.has_changed as isize,
                EeGeTexVal::Frame => t.frame as isize,
                EeGeTexVal::FrameCount => t.frame_count as isize,
                EeGeTexVal::MipLevelCount => t.mip_level_count as isize,
                EeGeTexVal::NumMipLevelsToDrop => t.num_mip_levels_to_drop as isize,
                EeGeTexVal::Screen => t.screen as isize,
                EeGeTexVal::Surface => t.surface as isize,
                EeGeTexVal::FrameFormat => t.frame_format as isize,
                EeGeTexVal::IsPromoted => t.is_promoted as isize,
                EeGeTexVal::IsManaged => t.is_managed as isize,
                EeGeTexVal::FrameTexture => t.frame_texture as isize,
                EeGeTexVal::FramesPerRow => t.frames_per_row as isize,
                EeGeTexVal::Data => t.data as isize,
                EeGeTexVal::IsCompressed => t.is_compressed as isize,
                EeGeTexVal::FromPack => t.is_from_texture_pack as isize,
                EeGeTexVal::PackMipLevels => t.texture_pack_mip_levels as isize,
                EeGeTexVal::Manager => t.manager as isize,
            }
        }
    }

    /// A field (`EeGeTexVal`) set; fields without a setter are left alone.
    ///
    /// # Safety
    /// `p` is live; for `FrameData` the current frame's texture is live.
    pub unsafe fn set(p: *mut Self, field: EeGeTexVal, value: isize) {
        // SAFETY: the caller's contract
        unsafe {
            match field {
                EeGeTexVal::FrameData => tx(Self::current(p)).data = value as usize,
                EeGeTexVal::HasChanged => tx(p).has_changed = value != 0,
                EeGeTexVal::NumMipLevelsToDrop => tx(p).num_mip_levels_to_drop = value as c_ulong,
                EeGeTexVal::IsPromoted => tx(p).is_promoted = value != 0,
                EeGeTexVal::IsManaged => tx(p).is_managed = value != 0,
                EeGeTexVal::FromPack => tx(p).is_from_texture_pack = value != 0,
                EeGeTexVal::PackMipLevels => tx(p).texture_pack_mip_levels = value as c_long,
                _ => {}
            }
        }
    }
}

/// `GETexture::GETexture(manager)`: the object of the C++ texture `owner`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_tex_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_new(
    host: EeGeTexHostFn,
    owner: *mut c_void,
    manager: *mut c_void,
) -> *mut EeGeTexture {
    let p = EeGeTexture::create(host, owner as usize, manager as usize);
    // SAFETY: just made
    unsafe { (*p).direct = true };
    p
}

/// `GETexture::GETexture(surface, manager)`'s statements after the fields (`ee_ge_tex_new` first).
///
/// # Safety
/// `p` is live; `surface` a surface.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_construct_on_surface(p: *mut EeGeTexture, surface: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::construct_on_surface(p, surface as usize) }
}

/// `~GETexture`: the releases, then the object freed.
///
/// # Safety
/// `p` is live; it is freed.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_destroy(p: *mut EeGeTexture) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::destroy(p) }
}

/// `Dereference`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_dereference(p: *mut EeGeTexture) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::dereference(p) }
}

/// `GetAllocatedSize`.
///
/// # Safety
/// `p` is live with a surface.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_allocated_size(p: *mut EeGeTexture) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::allocated_size(p) }
}

/// `Load(file, description, screen, create_mipmaps, drop, frame_format)`.
///
/// # Safety
/// `p` is live; `description` valid; `file` a decoded file; `screen` a screen.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_load(
    p: *mut EeGeTexture,
    file: *const c_void,
    description: *mut EeGeSurfaceDescription,
    screen: *mut c_void,
    create_mipmaps: bool,
    num_mip_levels_to_drop: c_long,
    frame_format: c_ulong,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        EeGeTexture::load(
            p,
            file as usize,
            &mut *description,
            screen as usize,
            create_mipmaps,
            num_mip_levels_to_drop,
            frame_format,
        )
    }
}

/// `Lock(memory, bytes_per_row)`.
///
/// # Safety
/// `p` is live with a surface; the out pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_lock(
    p: *mut EeGeTexture,
    memory: *mut *mut u8,
    bytes_per_row: *mut c_ulong,
) -> c_long {
    let mut m = 0usize;
    let mut b: c_ulong = 0;
    // SAFETY: the caller's contract
    unsafe {
        let result = EeGeTexture::lock(p, &mut m, &mut b);
        if result == OK {
            *memory = m as *mut u8;
            *bytes_per_row = b;
        }
        result
    }
}

/// `Unlock`.
///
/// # Safety
/// `p` is live with a surface.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_unlock(p: *mut EeGeTexture) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::unlock(p) }
}

/// `Recreate(screen)`.
///
/// # Safety
/// `p` is live with a surface.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_recreate(p: *mut EeGeTexture, screen: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::recreate(p, screen as usize) }
}

/// `SetLogicalShift(shift)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_logical_shift(p: *mut EeGeTexture, shift: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_logical_shift(p, shift) }
}

/// `SetStageAlphaMode` .. `SetStageTextureAddressingMode`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_stage(p: *mut EeGeTexture, which: EeGeTexStageField, mode: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_stage(p, which, mode) }
}

/// `SetStageMipMapBias(bias)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_stage_bias(p: *mut EeGeTexture, bias: f32) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_stage_with(p, bias, |s| &mut s.mip_map_bias) }
}

/// `SetStageTexture(texture)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_stage_texture(p: *mut EeGeTexture, texture: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_stage_with(p, texture, |s| &mut s.texture) }
}

/// `SetFrame(frame)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_frame(p: *mut EeGeTexture, frame: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_frame(p, frame) }
}

/// `SetSurface(surface)`.
///
/// # Safety
/// `p` is live; `surface` a surface.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set_surface(p: *mut EeGeTexture, surface: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set_surface(p, surface as usize) }
}

/// `Promote`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_promote(p: *mut EeGeTexture) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::promote(p) }
}

/// A field (the header's accessors).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_get(p: *mut EeGeTexture, field: EeGeTexVal) -> isize {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::get(p, field) }
}

/// A field set (the header's setters).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_set(p: *mut EeGeTexture, field: EeGeTexVal, value: isize) {
    // SAFETY: the caller's contract
    unsafe { EeGeTexture::set(p, field, value) }
}

/// `GetStage()`: the stage inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_stage(p: *mut EeGeTexture) -> *const EeGeTStage {
    // SAFETY: the caller's contract
    unsafe { &tx(p).stage }
}

/// `GetFrameTopLeftCorner()`: the corner inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_corner(p: *mut EeGeTexture) -> *const EeGeTexCoord {
    // SAFETY: the caller's contract
    unsafe { &tx(p).corner }
}

/// `GetFrameUVSize()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_frame_uv_size(p: *mut EeGeTexture) -> f32 {
    // SAFETY: the caller's contract
    unsafe { tx(p).frame_uv_size }
}

/// `mFrames` (NULL without an array).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_frames(p: *mut EeGeTexture) -> *const *mut c_void {
    // SAFETY: the caller's contract
    unsafe { tx(p).frames.as_ref().map_or(std::ptr::null(), |f| f.as_ptr() as *const *mut c_void) }
}

/// `GetFrameTexture(frame)`: frame `frame`'s texture in format 0, else the array's first (NULL without an array or
/// past it: the original read past it).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_tex_frame_texture(p: *mut EeGeTexture, frame: c_ulong) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe {
        let i = if tx(p).frame_format == 0 { frame } else { 0 };
        EeGeTexture::frame_at(p, i) as *mut c_void
    }
}

#[cfg(test)]
#[path = "texobj_tex_tests.rs"]
mod tests;
