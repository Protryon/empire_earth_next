//! `GETextureManager` as a whole Rust object (R4.3, seventh part): the cache of textures by name (the original's
//! `std::map<UString, GETexture*>`, in `strcmp` order), the texture search path (held), the lock, the screen and the
//! memory footprint, with every method of GETextureManager.cpp and its file helpers (the cache name's stem, the
//! texture pack's file first, the surface description a texture description asks for).
//!
//! As `manobj.rs`'s model manager: the C++ `GETextureManager` stays the type the game, `GEScreen` and `GETexture` use;
//! its methods are one-line forwarders and its only member is this object; every call the original made on another
//! object goes through the C++ host (`GETextureManagerHost`), in its order. A texture's fields the original read or
//! wrote directly (`mIsFromTexturePack`, `mTexturePackMipLevels`, `mNumMipLevelsToDrop`, `mIsManaged`, its stage) are
//! host operations too. The surface description is this module's `#[repr(C)]` mirror of `GESurfaceDescription`
//! (GETextureManager.cpp asserts the layout), so `RefreshAllTextures` keeps one description across its textures as the
//! original did (`GETexture::Load` writes into it).
//!
//! The texture pack's settings and the made textures' counter stay `managers.rs`'s process-wide state.
//!
//! LLE round 8: the host operations on a texture that only crossed into C++ to reach its Rust object (the fields,
//! the stage, the frames, the reference count, `Load`, `Recreate`, the logical shift, the allocated size, the
//! non-virtual `Dereference`) are answered here for a manager the C++ made (`ee_ge_texman_new`): the texture is a
//! `GETexture`, read in place (`cppobj::counted`), and each answer is the C++ forwarder's call with its casts. A
//! manager made over a recorded world (`create`, the replays) asks its host as before. What stays C++: making a
//! texture, `UReferenceCounter::Dereference` (the virtual destructor), the surface reads (`GESurface`'s virtuals),
//! the files, the lock, the path and the dump.

use std::collections::BTreeMap;
use std::ffi::{c_char, c_long, c_ulong, c_void};

use crate::ge::cppobj;
use crate::ge::managers::{self, with_pack, NOT_FOUND, OK};
use crate::ge::manobj::{c_bytes, c_string, next_after, EeGeManHostFn, EeGeManOp};
use crate::ge::texobj_tex::{self as tex, EeGeTexStageField, EeGeTexVal, EeGeTexture};

/// The original's buffer for a made texture's name (`char theName[64]`).
const MADE_NAME_CAPACITY: usize = 64;
/// The original's buffer for a texture pack name (`char theBuffer[1024]`).
const PACK_NAME_CAPACITY: usize = 1024;

/// `GESurfaceDescription` (lle/GESurface.h), field for field.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGeSurfaceDescription {
    /// `mType`.
    pub kind: c_ulong,
    pub pixel_format: c_ulong,
    pub surface_volatility: c_ulong,
    pub width: c_ulong,
    pub height: c_ulong,
    pub m14: c_ulong,
    pub mip_level_count: c_long,
    pub pixel_depth: c_ulong,
    pub is_3d_renderable: bool,
    pub is_transparency_enabled: bool,
    /// The constructor's one store (false).
    pub no_alpha: bool,
}

/// `GETextureDescription` (lle/GETextureManager.h), field for field.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGeTextureDescription {
    pub surface_volatility: c_ulong,
    pub frame_format: c_ulong,
    pub pixel_depth: c_ulong,
    pub num_mip_levels_to_drop: c_long,
    pub is_3d_renderable: bool,
    pub is_transparency_enabled: bool,
    pub has_alpha: bool,
    pub create_mipmaps: bool,
    pub use_texture_pack: bool,
}

/// `GETexture::Load`'s arguments but the texture and the screen (`EeGeManOp::TexLoad`'s `a`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeTexLoad {
    /// The decoded file (`const EeOfTexture*`).
    pub file: *mut c_void,
    /// The description `Load` reads and writes.
    pub description: *mut EeGeSurfaceDescription,
    pub create_mipmaps: bool,
    pub num_mip_levels_to_drop: c_long,
    pub frame_format: c_ulong,
}

/// A texture's field (`EeGeManOp::TexGet` / `TexSet`'s `a`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTexField {
    /// `mIsFromTexturePack` (0 or 1).
    FromPack = 1,
    /// `mTexturePackMipLevels`.
    PackMipLevels,
    /// `mNumMipLevelsToDrop` (`GetNumMipLevelsToDrop`).
    NumMipLevelsToDrop,
    /// `mIsManaged` (set only).
    IsManaged,
    /// `GetCreateMIPMaps()` (get only).
    CreateMipMaps,
    /// `GetFrameFormat()` (get only).
    FrameFormat,
    /// `GetFrameCount()` (get only).
    FrameCount,
    /// `SetIsPromoted(b != 0)` (set only).
    Promoted,
}

/// A stage setting (`EeGeManOp::TexStage`'s and `TexSetStage`'s `a`): the `GETStage` field and its `SetStage...`
/// setter (always for stage -1, as the manager calls them). The bias is its `f32` bits.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeTexStage {
    AlphaMode = 1,
    ColorMode,
    MagFilterMode,
    MinFilterMode,
    MipFilterMode,
    MipMapBias,
    TextureAddressingMode,
}

/// `kStageOpModulate`, `kMipFilterPoint`, `kAddressingClamp` (lle/GERenderState.h).
const STAGE_OP_MODULATE: isize = 2;
const MIP_FILTER_POINT: isize = 1;
const ADDRESSING_CLAMP: isize = 0;

/// The texture manager.
pub struct EeGeTextureManager {
    host: EeGeManHostFn,
    owner: *mut c_void,
    /// `mTextures`: name -> texture (the cache's reference).
    textures: BTreeMap<Vec<u8>, usize>,
    /// `mCriticalSection`.
    lock: usize,
    /// `mTexturePath` (referenced).
    path: usize,
    /// `mScreen`.
    screen: usize,
    /// `mMemoryFootprint`: the cached textures' `GetAllocatedSize`s.
    footprint: c_ulong,
    /// Made by the C++ (`ee_ge_texman_new`): its textures are `GETexture`s, whose Rust objects it reaches directly.
    direct: bool,
}

/// The `EeGeTexField` of a host operation's `a` (the manager passes only its own values).
fn tex_field(a: isize) -> Option<EeGeTexField> {
    use EeGeTexField as F;
    [
        F::FromPack,
        F::PackMipLevels,
        F::NumMipLevelsToDrop,
        F::IsManaged,
        F::CreateMipMaps,
        F::FrameFormat,
        F::FrameCount,
        F::Promoted,
    ]
    .into_iter()
    .find(|f| *f as isize == a)
}

/// The `EeGeTexStage` of a host operation's `a`.
fn tex_stage(a: isize) -> Option<EeGeTexStage> {
    use EeGeTexStage as S;
    [
        S::AlphaMode,
        S::ColorMode,
        S::MagFilterMode,
        S::MinFilterMode,
        S::MipFilterMode,
        S::MipMapBias,
        S::TextureAddressingMode,
    ]
    .into_iter()
    .find(|s| *s as isize == a)
}

/// A host operation on the texture `obj` answered on its Rust object as the C++ host's forwarder made it (the
/// GETexture.h accessor's or GETexture.cpp member's call, with its casts); `None` for one the C++ makes.
///
/// # Safety
/// For a texture operation `obj` is a live `GETexture*`; the arguments are as `EeGeManOp` says.
pub(crate) unsafe fn tex_direct(op: EeGeManOp, obj: usize, a: isize, b: isize) -> Option<isize> {
    use EeGeManOp as O;
    use EeGeTexField as F;
    use EeGeTexStage as S;
    // SAFETY (every use): the caller's contract
    let t = || unsafe { cppobj::counted::<EeGeTexture>(obj) };
    let get = |v: EeGeTexVal| unsafe { tex::ee_ge_tex_get(t(), v) };
    let set = |v: EeGeTexVal, value: isize| unsafe { tex::ee_ge_tex_set(t(), v, value) };
    Some(match op {
        // the host's TexGet: bools 0 or 1, long sign-extended, unsigned long zero-extended
        O::TexGet => match tex_field(a)? {
            F::FromPack => (get(EeGeTexVal::FromPack) != 0) as isize,
            F::PackMipLevels => get(EeGeTexVal::PackMipLevels) as c_long as isize,
            F::NumMipLevelsToDrop => get(EeGeTexVal::NumMipLevelsToDrop) as c_ulong as isize,
            F::CreateMipMaps => (get(EeGeTexVal::CreateMipMaps) != 0) as isize,
            F::FrameFormat => get(EeGeTexVal::FrameFormat) as c_ulong as isize,
            F::FrameCount => get(EeGeTexVal::FrameCount) as c_ulong as isize,
            F::IsManaged | F::Promoted => 0,
        },
        // the host's TexSet: SetIsFromTexturePack(b != 0), SetTexturePackMipLevels((long)b),
        // SetNumMipLevelsToDrop((unsigned long)b), SetIsManaged(b != 0), SetIsPromoted(b != 0)
        O::TexSet => {
            match tex_field(a)? {
                F::FromPack => set(EeGeTexVal::FromPack, (b != 0) as isize),
                F::PackMipLevels => set(EeGeTexVal::PackMipLevels, b as c_long as isize),
                F::NumMipLevelsToDrop => set(EeGeTexVal::NumMipLevelsToDrop, b as c_ulong as isize),
                F::IsManaged => set(EeGeTexVal::IsManaged, (b != 0) as isize),
                F::Promoted => set(EeGeTexVal::IsPromoted, (b != 0) as isize),
                F::CreateMipMaps | F::FrameFormat | F::FrameCount => {}
            }
            0
        }
        // GetStage()'s field (the bias as its bits)
        O::TexStage => {
            // SAFETY: the texture's own stage
            let s = unsafe { &*tex::ee_ge_tex_stage(t()) };
            match tex_stage(a)? {
                S::AlphaMode => s.alpha_mode as isize,
                S::ColorMode => s.color_mode as isize,
                S::MagFilterMode => s.mag_filter_mode as isize,
                S::MinFilterMode => s.min_filter_mode as isize,
                S::MipFilterMode => s.mip_filter_mode as isize,
                S::MipMapBias => s.mip_map_bias.to_bits() as isize,
                S::TextureAddressingMode => s.texture_addressing_mode as isize,
            }
        }
        // SetStage...((unsigned long)b, -1); the bias from its bits
        O::TexSetStage => {
            let field = match tex_stage(a)? {
                S::AlphaMode => EeGeTexStageField::AlphaMode,
                S::ColorMode => EeGeTexStageField::ColorMode,
                S::MagFilterMode => EeGeTexStageField::MagFilterMode,
                S::MinFilterMode => EeGeTexStageField::MinFilterMode,
                S::MipFilterMode => EeGeTexStageField::MipFilterMode,
                S::TextureAddressingMode => EeGeTexStageField::TextureAddressingMode,
                S::MipMapBias => {
                    // SAFETY: a live texture
                    unsafe { tex::ee_ge_tex_set_stage_bias(t(), f32::from_bits(b as u32)) };
                    return Some(0);
                }
            };
            // SAFETY: a live texture
            unsafe { tex::ee_ge_tex_set_stage(t(), field, b as c_ulong) };
            0
        }
        O::TexSetLogicalShift => {
            // SAFETY: a live texture
            unsafe { tex::ee_ge_tex_set_logical_shift(t(), a as c_ulong) };
            0
        }
        // SAFETY (the arms below): a live texture
        O::TexFrame => unsafe { tex::ee_ge_tex_frame_texture(t(), a as c_ulong) as isize },
        O::TexReference => {
            unsafe { cppobj::reference(obj) };
            0
        }
        O::TexDereference => {
            unsafe { tex::ee_ge_tex_dereference(t()) };
            0
        }
        O::RefCount => unsafe { cppobj::reference_count(obj) as isize },
        O::TexRecreate => unsafe { tex::ee_ge_tex_recreate(t(), a as *mut c_void) as isize },
        O::TexAllocatedSize => unsafe { tex::ee_ge_tex_allocated_size(t()) as isize },
        O::TexLoad => {
            // SAFETY: a is the manager's own EeGeTexLoad
            let l = unsafe { *(a as *const EeGeTexLoad) };
            unsafe {
                tex::ee_ge_tex_load(
                    t(),
                    l.file,
                    l.description,
                    b as *mut c_void,
                    l.create_mipmaps,
                    l.num_mip_levels_to_drop,
                    l.frame_format,
                ) as isize
            }
        }
        _ => return None,
    })
}

/// Every access to the object is one short statement: a host call may come back in and use it.
///
/// # Safety
/// `p` is a live object from `ee_ge_texman_new`.
unsafe fn tm<'a>(p: *mut EeGeTextureManager) -> &'a mut EeGeTextureManager {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

/// The surface description a texture description asks for (`GESurfaceDescriptionFor`): its depth, format and alpha;
/// the volatility and the two flags copied; the rest as `d` had them.
pub fn surface_description_for(t: &EeGeTextureDescription, d: &mut EeGeSurfaceDescription) {
    let (depth, format, no_alpha) = managers::surface_format(t.pixel_depth, t.has_alpha);
    d.pixel_depth = depth;
    d.pixel_format = format;
    d.no_alpha = no_alpha;
    d.surface_volatility = t.surface_volatility;
    d.is_3d_renderable = t.is_3d_renderable;
    d.is_transparency_enabled = t.is_transparency_enabled;
}

/// A name through the original's fixed buffer: cut to `capacity - 1` bytes, NUL-terminated.
fn buffered(mut name: Vec<u8>, capacity: usize) -> Vec<u8> {
    name.truncate(capacity - 1);
    c_string(&name)
}

impl EeGeTextureManager {
    /// A host call.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeManOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeManOp, obj: usize, a: isize, b: isize, c: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner, direct) = unsafe { ((*p).host, (*p).owner, (*p).direct) };
        if direct {
            // SAFETY: the C++ manager's textures are GETextures
            if let Some(r) = unsafe { tex_direct(op, obj, a, b) } {
                return r;
            }
        }
        // SAFETY: the host's contract
        unsafe { host(owner, op, obj as *mut c_void, a, b, c) }
    }

    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn get(p: *mut Self, tex: usize, field: EeGeTexField) -> isize {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::TexGet, tex, field as isize, 0, 0) }
    }

    /// `GetFrameCount()`.
    ///
    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn frame_count(p: *mut Self, tex: usize) -> usize {
        // SAFETY: the caller's contract
        unsafe { Self::get(p, tex, EeGeTexField::FrameCount) as usize }
    }

    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn set(p: *mut Self, tex: usize, field: EeGeTexField, value: isize) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::TexSet, tex, field as isize, value, 0) };
    }

    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn set_stage(p: *mut Self, tex: usize, stage: EeGeTexStage, value: isize) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::TexSetStage, tex, stage as isize, value, 0) };
    }

    /// # Safety
    /// `p` is live.
    unsafe fn enter(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::LockEnter, tm(p).lock, 0, 0, 0) };
    }

    /// # Safety
    /// `p` is live.
    unsafe fn leave(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, EeGeManOp::LockLeave, tm(p).lock, 0, 0, 0) };
    }

    /// `GEFindTextureFile(path, name, *file)`.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn find(p: *mut Self, path: usize, name: &[u8], file: &mut usize) -> c_long {
        let name = c_string(name);
        // SAFETY: the caller's contract
        unsafe {
            Self::op(p, EeGeManOp::TexFindFile, path, name.as_ptr() as isize, 0, file as *mut usize as isize) as c_long
        }
    }

    /// `GEFindCachedTextureFile`: a cached texture's files, the pack's first when it was loaded from there (the
    /// original's when the pack's are gone).
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn find_cached(p: *mut Self, path: usize, name: &[u8], from_pack: bool, file: &mut usize) -> c_long {
        if from_pack {
            if let Some(packed) = with_pack(|k| k.pack_name(name)) {
                let packed = buffered(packed, PACK_NAME_CAPACITY);
                // SAFETY: the caller's contract
                if unsafe { Self::find(p, path, &packed[..packed.len() - 1], file) } == OK {
                    return OK;
                }
            }
        }
        // SAFETY: the caller's contract
        unsafe { Self::find(p, path, name, file) }
    }

    /// `SetTexturePackSizes`: a texture just loaded from the pack with `mips` more levels dropped than the caller asked.
    ///
    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn set_pack_sizes(p: *mut Self, tex: usize, mips: c_long) {
        // SAFETY: the caller's contract
        unsafe {
            Self::set(p, tex, EeGeTexField::FromPack, 1);
            Self::set(p, tex, EeGeTexField::PackMipLevels, mips as isize);
            let drop = Self::get(p, tex, EeGeTexField::NumMipLevelsToDrop) as c_ulong;
            let (new_drop, shift) = with_pack(|k| k.pack_sizes(drop, mips));
            Self::set(p, tex, EeGeTexField::NumMipLevelsToDrop, new_drop as isize);
            Self::op(p, EeGeManOp::TexSetLogicalShift, tex, shift as isize, 0, 0);
        }
    }

    /// `GETexture::Load(file, *description, mScreen, ...)`.
    ///
    /// # Safety
    /// `p` is live; `tex` a texture.
    unsafe fn load_texture(
        p: *mut Self,
        tex: usize,
        file: usize,
        description: &mut EeGeSurfaceDescription,
        mipmaps: bool,
        drop: c_long,
        frame_format: c_ulong,
    ) -> c_long {
        let args = EeGeTexLoad {
            file: file as *mut c_void,
            description,
            create_mipmaps: mipmaps,
            num_mip_levels_to_drop: drop,
            frame_format,
        };
        // SAFETY: the caller's contract
        unsafe {
            let screen = tm(p).screen;
            Self::op(p, EeGeManOp::TexLoad, tex, &args as *const EeGeTexLoad as isize, screen as isize, 0) as c_long
        }
    }

    /// The constructor: no screen, no textures, the path referenced, then the lock.
    ///
    /// # Safety
    /// `host` is valid for `owner` from now until `destroy`.
    pub unsafe fn create(host: EeGeManHostFn, owner: *mut c_void, path: usize) -> *mut Self {
        let p = Box::into_raw(Box::new(Self {
            host,
            owner,
            textures: BTreeMap::new(),
            lock: 0,
            path: 0,
            screen: 0,
            footprint: 0,
            direct: false,
        }));
        // SAFETY: p is live
        unsafe {
            tm(p).path = path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathReference, path, 0, 0, 0);
            }
            let lock = Self::op(p, EeGeManOp::LockCreate, 0, 0, 0, 0) as usize;
            tm(p).lock = lock;
        }
        p
    }

    /// The destructor: every unreferenced texture removed, the path released, the lock destroyed, the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            Self::remove_all(p);
            let path = tm(p).path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathDereference, path, 0, 0, 0);
            }
            Self::op(p, EeGeManOp::LockDestroy, tm(p).lock, 0, 0, 0);
            drop(Box::from_raw(p));
        }
    }

    /// `DumpAllLoadedMedia`: the heading and each cached texture's line, in name order.
    ///
    /// # Safety
    /// `p` is live; `file` is the host's.
    pub unsafe fn dump(p: *mut Self, file: usize) {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            Self::op(p, EeGeManOp::DumpHeader, file, 0, 0, 0);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, tex)) = next_after(&tm(p).textures, at.as_deref()) {
                let name = c_string(&key);
                Self::op(p, EeGeManOp::DumpLine, tex, name.as_ptr() as isize, file as isize, 0);
                at = Some(key);
            }
            Self::leave(p);
        }
    }

    /// `LoadTexture`: the cached texture of the name's stem, else its files found (the pack's first when the
    /// description allows it) and loaded; 0 when there are none or they do not load.
    ///
    /// # Safety
    /// `p` is live; `name` is NUL-terminated; `t` is valid.
    pub unsafe fn load(p: *mut Self, name: *const c_char, t: &EeGeTextureDescription) -> usize {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let name = c_bytes(name);
            let stem = name[..managers::texture_stem(name)].to_vec();
            let mut tex = tm(p).textures.get(&stem).copied().unwrap_or(0);
            if tex == 0 {
                let mut file: usize = 0;
                let path = tm(p).path;
                let mut from_pack = false;
                if t.use_texture_pack {
                    if let Some(packed) = with_pack(|k| k.enabled_pack_name(&stem)) {
                        let packed = buffered(packed, PACK_NAME_CAPACITY);
                        from_pack = Self::find(p, path, &packed[..packed.len() - 1], &mut file) == OK;
                    }
                }
                let found = if from_pack { OK } else { Self::find(p, path, &stem, &mut file) };
                if found == OK {
                    let mut description = EeGeSurfaceDescription::default();
                    surface_description_for(t, &mut description);
                    tex = Self::op(p, EeGeManOp::TexNew, 0, 0, 0, 0) as usize;
                    let (pack_drop, drop, mipmaps) =
                        with_pack(|k| k.load_plan(t.create_mipmaps, t.num_mip_levels_to_drop, from_pack));
                    if Self::load_texture(p, tex, file, &mut description, mipmaps, drop, t.frame_format) == OK {
                        if from_pack {
                            Self::set_pack_sizes(p, tex, pack_drop);
                            let logged = c_string(&stem);
                            Self::op(p, EeGeManOp::TexPackLog, tex, logged.as_ptr() as isize, drop as isize, 0);
                        }
                        tm(p).textures.insert(stem, tex);
                        let size = Self::op(p, EeGeManOp::TexAllocatedSize, tex, 0, 0, 0) as c_ulong;
                        tm(p).footprint = tm(p).footprint.wrapping_add(size);
                        Self::set(p, tex, EeGeTexField::IsManaged, 1);
                    } else {
                        Self::op(p, EeGeManOp::BaseDereference, tex, 0, 0, 0);
                        tex = 0;
                    }
                    Self::op(p, EeGeManOp::TexFreeFile, file, 0, 0, 0);
                }
            }
            Self::leave(p);
            tex
        }
    }

    /// `MakeTexture`: `surface` in a new cached texture named by the made textures' counter, modulating, clamped,
    /// point MIP filtered.
    ///
    /// # Safety
    /// `p` is live; `surface` is the host's.
    pub unsafe fn make(p: *mut Self, surface: usize) -> usize {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let tex = Self::op(p, EeGeManOp::TexNewFromSurface, surface, 0, 0, 0) as usize;
            let name = buffered(managers::next_made_texture_name(), MADE_NAME_CAPACITY);
            tm(p).textures.insert(name[..name.len() - 1].to_vec(), tex);
            Self::set_stage(p, tex, EeGeTexStage::AlphaMode, STAGE_OP_MODULATE);
            Self::set_stage(p, tex, EeGeTexStage::ColorMode, STAGE_OP_MODULATE);
            Self::set_stage(p, tex, EeGeTexStage::MinFilterMode, 1);
            Self::set_stage(p, tex, EeGeTexStage::MagFilterMode, 1);
            Self::set_stage(p, tex, EeGeTexStage::MipFilterMode, MIP_FILTER_POINT);
            Self::set_stage(p, tex, EeGeTexStage::MipMapBias, 0.0f32.to_bits() as isize);
            Self::set_stage(p, tex, EeGeTexStage::TextureAddressingMode, ADDRESSING_CLAMP);
            Self::set(p, tex, EeGeTexField::IsManaged, 1);
            Self::leave(p);
            tex
        }
    }

    /// `PromoteTexture`: the texture marked promoted; always OK.
    ///
    /// # Safety
    /// `p` is live; `tex` a texture.
    pub unsafe fn promote(p: *mut Self, tex: usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe { Self::set(p, tex, EeGeTexField::Promoted, 1) };
        OK
    }

    /// `ReloadTexture`: the cached texture of exactly that name loaded again from its files (its pack's when it came
    /// from there); 3 when it is not cached.
    ///
    /// # Safety
    /// `p` is live; `name` is NUL-terminated; `t` is valid.
    pub unsafe fn reload(p: *mut Self, name: *const c_char, t: &EeGeTextureDescription) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let name = c_bytes(name).to_vec();
            let mut result;
            if let Some(&tex) = tm(p).textures.get(&name) {
                let mut file: usize = 0;
                let path = tm(p).path;
                let cached_from_pack = Self::get(p, tex, EeGeTexField::FromPack) != 0;
                result = Self::find_cached(p, path, &name, cached_from_pack, &mut file);
                if result == OK {
                    let mut description = EeGeSurfaceDescription::default();
                    surface_description_for(t, &mut description);
                    let size = Self::op(p, EeGeManOp::TexAllocatedSize, tex, 0, 0, 0) as c_ulong;
                    tm(p).footprint = tm(p).footprint.wrapping_sub(size);
                    let from_pack = Self::get(p, tex, EeGeTexField::FromPack) != 0;
                    let pack_drop = Self::get(p, tex, EeGeTexField::PackMipLevels) as c_long;
                    let mipmaps =
                        t.create_mipmaps || (from_pack && Self::get(p, tex, EeGeTexField::CreateMipMaps) != 0);
                    result = Self::load_texture(
                        p,
                        tex,
                        file,
                        &mut description,
                        mipmaps,
                        t.num_mip_levels_to_drop.wrapping_add(pack_drop),
                        t.frame_format,
                    );
                    if result == OK && from_pack {
                        Self::set_pack_sizes(p, tex, pack_drop);
                    }
                    if result == OK {
                        let size = Self::op(p, EeGeManOp::TexAllocatedSize, tex, 0, 0, 0) as c_ulong;
                        tm(p).footprint = tm(p).footprint.wrapping_add(size);
                    }
                    // the original's file Close result (always kErrorOK for the in-memory file it read)
                    result = OK;
                    Self::op(p, EeGeManOp::TexFreeFile, file, 0, 0, 0);
                }
            } else {
                result = NOT_FOUND;
            }
            Self::leave(p);
            result
        }
    }

    /// `RefreshAllTextures`: every cached texture loaded again from its files (or its surface recreated when they are
    /// gone), its frames' stage settings applied to it again; the last result. One surface description and one frame
    /// list serve every texture, as the original's locals did.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn refresh_all(p: *mut Self) -> c_long {
        let mut result = OK;
        let mut frames: Vec<usize> = Vec::new();
        let mut description = EeGeSurfaceDescription::default();
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, tex)) = next_after(&tm(p).textures, at.as_deref()) {
                let mut file: usize = 0;
                let path = tm(p).path;
                let cached_from_pack = Self::get(p, tex, EeGeTexField::FromPack) != 0;
                if Self::find_cached(p, path, &key, cached_from_pack, &mut file) == OK {
                    Self::op(p, EeGeManOp::TexSurfaceDescription, tex, &mut description as *mut _ as isize, 0, 0);
                    if Self::frame_count(p, tex) > 1 {
                        if Self::frame_count(p, tex) > frames.len() {
                            frames.resize(Self::frame_count(p, tex), 0);
                        }
                        let mut i = 1;
                        while i < Self::frame_count(p, tex) {
                            frames[i] = Self::op(p, EeGeManOp::TexFrame, tex, i as isize, 0, 0) as usize;
                            Self::op(p, EeGeManOp::TexReference, frames[i], 0, 0, 0);
                            i += 1;
                        }
                    }
                    let from_pack = Self::get(p, tex, EeGeTexField::FromPack) != 0;
                    let pack_drop = Self::get(p, tex, EeGeTexField::PackMipLevels) as c_long;
                    let mipmaps = Self::get(p, tex, EeGeTexField::CreateMipMaps) != 0;
                    let drop = (Self::get(p, tex, EeGeTexField::NumMipLevelsToDrop) as c_long).wrapping_add(pack_drop);
                    let frame_format = Self::get(p, tex, EeGeTexField::FrameFormat) as c_ulong;
                    if Self::load_texture(p, tex, file, &mut description, mipmaps, drop, frame_format) == OK
                        && from_pack
                    {
                        Self::set_pack_sizes(p, tex, pack_drop);
                    }
                    if Self::frame_count(p, tex) > 1 {
                        let mut i = 1;
                        while i < Self::frame_count(p, tex) {
                            // a frame past the list (a reload that added frames) was the original's stale read
                            let frame = frames.get(i).copied().unwrap_or(0);
                            for stage in [
                                EeGeTexStage::AlphaMode,
                                EeGeTexStage::ColorMode,
                                EeGeTexStage::MagFilterMode,
                                EeGeTexStage::MinFilterMode,
                                EeGeTexStage::MipFilterMode,
                                EeGeTexStage::MipMapBias,
                                EeGeTexStage::TextureAddressingMode,
                            ] {
                                let value = Self::op(p, EeGeManOp::TexStage, frame, stage as isize, 0, 0);
                                Self::set_stage(p, tex, stage, value);
                            }
                            Self::op(p, EeGeManOp::TexDereference, frame, 0, 0, 0);
                            i += 1;
                        }
                    }
                    // the original's file Close result (always kErrorOK for the in-memory file it read)
                    result = OK;
                    Self::op(p, EeGeManOp::TexFreeFile, file, 0, 0, 0);
                } else {
                    let screen = tm(p).screen;
                    result = Self::op(p, EeGeManOp::TexRecreate, tex, screen as isize, 0, 0) as c_long;
                }
                at = Some(key);
            }
            Self::leave(p);
        }
        result
    }

    /// The cache's removal of `tex` (named `key`) at its reference count: released and uncached when the cache's
    /// reference is the last; the decision's result.
    ///
    /// # Safety
    /// `p` is live.
    unsafe fn remove_entry(p: *mut Self, key: &[u8], tex: usize) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            let count = Self::op(p, EeGeManOp::RefCount, tex, 0, 0, 0) as c_ulong;
            let (remove, result) = managers::remove_decision(count);
            if remove {
                let size = Self::op(p, EeGeManOp::TexAllocatedSize, tex, 0, 0, 0) as c_ulong;
                tm(p).footprint = tm(p).footprint.wrapping_sub(size);
                Self::op(p, EeGeManOp::BaseDereference, tex, 0, 0, 0);
                tm(p).textures.remove(key);
            }
            result
        }
    }

    /// `RemoveAllTextures`: every texture nobody else references released; 5 when some are still in use.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn remove_all(p: *mut Self) -> c_long {
        let mut result = OK;
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, tex)) = next_after(&tm(p).textures, at.as_deref()) {
                let decision = Self::remove_entry(p, &key, tex);
                if decision != OK {
                    result = decision;
                }
                at = Some(key);
            }
            Self::leave(p);
        }
        result
    }

    /// `RemoveTexture`: `tex` released if nobody else references it; 3 when it is not cached, 5 when in use.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn remove(p: *mut Self, tex: usize) -> c_long {
        let mut result = NOT_FOUND;
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, cached)) = next_after(&tm(p).textures, at.as_deref()) {
                if cached == tex {
                    result = Self::remove_entry(p, &key, tex);
                    break;
                }
                at = Some(key);
            }
            Self::leave(p);
        }
        result
    }

    /// `SetTexturePath`: the old path released, the new one held.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn set_path(p: *mut Self, path: usize) {
        // SAFETY: the caller's contract
        unsafe {
            let old = tm(p).path;
            if old != 0 {
                Self::op(p, EeGeManOp::PathDereference, old, 0, 0, 0);
            }
            tm(p).path = path;
            if path != 0 {
                Self::op(p, EeGeManOp::PathReference, path, 0, 0, 0);
            }
        }
    }

    /// `UpdateMIPFilterMode`: the MIP filter of every cached texture that MIP filters.
    ///
    /// # Safety
    /// `p` is live.
    pub unsafe fn update_mip_filter(p: *mut Self, mode: c_ulong) {
        // SAFETY: the caller's contract
        unsafe {
            Self::enter(p);
            let mut at: Option<Vec<u8>> = None;
            while let Some((key, tex)) = next_after(&tm(p).textures, at.as_deref()) {
                if Self::op(p, EeGeManOp::TexStage, tex, EeGeTexStage::MipFilterMode as isize, 0, 0) != 0 {
                    Self::set_stage(p, tex, EeGeTexStage::MipFilterMode, mode as isize);
                }
                at = Some(key);
            }
            Self::leave(p);
        }
    }
}

/// `GETextureManager::GETextureManager(path)`: the object of the C++ manager `owner`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_texman_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_new(
    host: EeGeManHostFn,
    owner: *mut c_void,
    path: *mut c_void,
) -> *mut EeGeTextureManager {
    // SAFETY: the caller's contract
    unsafe {
        let p = EeGeTextureManager::create(host, owner, path as usize);
        (*p).direct = true;
        p
    }
}

/// `~GETextureManager`.
///
/// # Safety
/// `p` is from `ee_ge_texman_new` (freed).
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_free(p: *mut EeGeTextureManager) {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::destroy(p) }
}

/// `DumpAllLoadedMedia(file)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_dump(p: *mut EeGeTextureManager, file: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::dump(p, file as usize) }
}

/// `GetMemoryFootprint`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_footprint(p: *mut EeGeTextureManager) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { tm(p).footprint }
}

/// `GetScreen`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_screen(p: *mut EeGeTextureManager) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { tm(p).screen as *mut c_void }
}

/// `SetScreen(screen)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_set_screen(p: *mut EeGeTextureManager, screen: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { tm(p).screen = screen as usize }
}

/// `GetTexturePath`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_path(p: *mut EeGeTextureManager) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { tm(p).path as *mut c_void }
}

/// `GetTextureCount`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_count(p: *mut EeGeTextureManager) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { tm(p).textures.len() as c_ulong }
}

/// `GetTextureAt(index)`: the cache's texture at `index` in name order (NULL past the end).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_at(p: *mut EeGeTextureManager, index: c_ulong) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { tm(p).textures.values().nth(index as usize).copied().unwrap_or(0) as *mut c_void }
}

/// `LoadTexture(name, description)`.
///
/// # Safety
/// `p` is live; `name` is NUL-terminated; `description` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_load(
    p: *mut EeGeTextureManager,
    name: *const c_char,
    description: *const EeGeTextureDescription,
) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::load(p, name, &*description) as *mut c_void }
}

/// `MakeTexture(surface)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_make(p: *mut EeGeTextureManager, surface: *mut c_void) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::make(p, surface as usize) as *mut c_void }
}

/// `PromoteTexture(texture)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_promote(p: *mut EeGeTextureManager, texture: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::promote(p, texture as usize) }
}

/// `RefreshAllTextures`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_refresh_all(p: *mut EeGeTextureManager) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::refresh_all(p) }
}

/// `ReloadTexture(name, description)`.
///
/// # Safety
/// `p` is live; `name` is NUL-terminated; `description` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_reload(
    p: *mut EeGeTextureManager,
    name: *const c_char,
    description: *const EeGeTextureDescription,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::reload(p, name, &*description) }
}

/// `RemoveAllTextures`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_remove_all(p: *mut EeGeTextureManager) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::remove_all(p) }
}

/// `RemoveTexture(texture)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_remove(p: *mut EeGeTextureManager, texture: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::remove(p, texture as usize) }
}

/// `SetTexturePath(path)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_set_path(p: *mut EeGeTextureManager, path: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::set_path(p, path as usize) }
}

/// `UpdateMIPFilterMode(mode)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_texman_update_mip_filter(p: *mut EeGeTextureManager, mode: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { EeGeTextureManager::update_mip_filter(p, mode) }
}

#[cfg(test)]
#[path = "texobj_tests.rs"]
mod tests;
