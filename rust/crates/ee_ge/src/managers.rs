//! The management logic of `GETextureManager` and `GEModelManager` (`GETextureManager.cpp`, `GEModelManager.cpp`):
//! a texture's cache name, the texture pack (its settings, a texture's name in it, the sizes of a texture loaded from
//! it), the surface description a texture description asks for, a load's MIP decisions, the reference-count decision
//! of a removal, and the names of made textures and created models. The maps, the lock, the textures and models and
//! their lifetimes stay C++.
//!
//! Not simulation code: the graphics thread and the UI load media.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong differ per target

use std::ffi::{c_long, c_ulong};
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Mutex;

/// `kErrorOK`.
pub const OK: c_long = 0;
/// The managers' "not managed here" result.
pub const NOT_FOUND: c_long = 3;
/// The managers' "still in use" result.
pub const IN_USE: c_long = 5;

/// `GESurface`'s A8R8G8B8, A4R4G4B4 and R5G6B5 format numbers.
pub const FORMAT_A8R8G8B8: c_ulong = 0;
pub const FORMAT_A4R4G4B4: c_ulong = 2;
pub const FORMAT_R5G6B5: c_ulong = 7;

/// `GETextureStem`: the length of a texture's cache name within its name. The name as given, or without its last
/// extension when it holds ".sst" (the original cached "<name>.tga"/"<name>.sst" and loaded those files; the stems are
/// the files its open-format lookup read in their place), so "textures\x" and "textures\x.sst" share one texture.
pub fn texture_stem(name: &[u8]) -> usize {
    if !name.windows(4).any(|w| w == b".sst") {
        return name.len();
    }
    let file = name.iter().rposition(|&c| c == b'\\' || c == b'/').map_or(0, |p| p + 1);
    match name[file..].iter().rposition(|&c| c == b'.') {
        Some(dot) => file + dot,
        None => name.len(),
    }
}

/// The texture pack (`GETextureManager::SetTexturePack`): a directory that stands in for "textures\" and holds files
/// `1 << scale_shift` times the originals' size, of which `mip_levels_to_drop` more levels are dropped. The directory
/// stays set when the pack is turned off, so a texture loaded from it is reloaded from it.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct TexturePack {
    /// With its trailing separator; empty before the first `set`.
    pub directory: Vec<u8>,
    pub enabled: bool,
    pub scale_shift: c_long,
    pub mip_levels_to_drop: c_long,
}

/// `sTexturePackDirectory`'s size: the directory, its separator and the NUL must fit.
const DIRECTORY_CAPACITY: usize = 64;
const TEXTURES: &[u8] = b"textures\\";
/// The pictures the game reads as data (their pixels are values, not art): never from the pack.
const DATA_PICTURES: [&[u8]; 3] = [b"colorpalette", b"waterpalette", b"volcano"];

impl TexturePack {
    /// `SetTexturePack`: NULL or "" turns the pack off; a directory too long for the original's buffer too.
    pub fn set(&mut self, directory: &[u8], scale_shift: c_long, mip_levels_to_drop: c_long) {
        self.enabled = !directory.is_empty() && directory.len() + 2 <= DIRECTORY_CAPACITY;
        if self.enabled {
            self.directory = directory.to_vec();
            self.directory.push(b'\\');
        }
        self.scale_shift = if scale_shift > 0 { scale_shift } else { 0 };
        self.mip_levels_to_drop = if mip_levels_to_drop > 0 { mip_levels_to_drop } else { 0 };
        if self.mip_levels_to_drop > self.scale_shift {
            self.mip_levels_to_drop = self.scale_shift;
        }
    }

    /// `GETexturePackName`: the pack's name of "textures\x" ("<directory>x"), whether the pack is on or off; none for a
    /// name outside "textures\" (compared without case), with no pack directory, or for a data picture.
    pub fn pack_name(&self, name: &[u8]) -> Option<Vec<u8>> {
        if self.directory.is_empty()
            || name.len() <= TEXTURES.len()
            || !name[..TEXTURES.len()].eq_ignore_ascii_case(TEXTURES)
        {
            return None;
        }
        let file = &name[TEXTURES.len()..];
        for picture in DATA_PICTURES {
            if file.len() >= picture.len()
                && file[..picture.len()].eq_ignore_ascii_case(picture)
                && (file.len() == picture.len() || file[picture.len()] == b'.')
            {
                return None;
            }
        }
        let mut out = self.directory.clone();
        out.extend_from_slice(file);
        Some(out)
    }

    /// `GetTexturePackName`: the pack's name only while the pack is on.
    pub fn enabled_pack_name(&self, name: &[u8]) -> Option<Vec<u8>> {
        if self.enabled {
            self.pack_name(name)
        } else {
            None
        }
    }

    /// `GetTexturePackLogicalShift`: a pack file's logical shift after its extra drop.
    pub fn logical_shift(&self) -> c_long {
        self.scale_shift - self.mip_levels_to_drop
    }

    /// `LoadTexture`'s MIP decisions for a texture description asking for `create_mipmaps` and `drop` levels dropped:
    /// (the pack's extra drop, the levels to drop, whether to build MIP levels). A pack file is drawn smaller by
    /// dropping more levels on top of the caller's, and gets MIP levels when it is bigger than its logical size.
    pub fn load_plan(&self, create_mipmaps: bool, drop: c_long, from_pack: bool) -> (c_long, c_long, bool) {
        let pack_drop = if from_pack { self.mip_levels_to_drop } else { 0 };
        (pack_drop, drop.wrapping_add(pack_drop), create_mipmaps || (from_pack && self.scale_shift > pack_drop))
    }

    /// `SetTexturePackSizes` for a texture whose load dropped `mip_levels` more than the caller asked and whose
    /// `mNumMipLevelsToDrop` is `drop`: the caller's drop as its drop (the 2D code computes the original's size as
    /// `GetWidth() << GetNumMipLevelsToDrop()`) and its logical shift.
    pub fn pack_sizes(&self, drop: c_ulong, mip_levels: c_long) -> (c_ulong, c_ulong) {
        let new_drop = if drop as c_long > mip_levels { drop.wrapping_sub(mip_levels as c_ulong) } else { 0 };
        let shift = self.scale_shift - mip_levels;
        (new_drop, if shift > 0 { shift as c_ulong } else { 0 })
    }
}

static PACK: Mutex<TexturePack> =
    Mutex::new(TexturePack { directory: Vec::new(), enabled: false, scale_shift: 0, mip_levels_to_drop: 0 });

/// The game's one texture pack (`GETextureManager`'s statics).
pub fn with_pack<R>(f: impl FnOnce(&mut TexturePack) -> R) -> R {
    let mut pack = PACK.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut pack)
}

/// The surface description a texture description's depth and alpha ask for (`LoadTexture`, `ReloadTexture`):
/// (pixel depth, pixel format, no alpha). 16 bits: A4R4G4B4 with alpha, else R5G6B5; otherwise 32-bit A8R8G8B8 whose
/// `mNoAlpha` is the description's lack of alpha.
pub fn surface_format(pixel_depth: c_ulong, has_alpha: bool) -> (c_ulong, c_ulong, bool) {
    if pixel_depth == 16 {
        (16, if has_alpha { FORMAT_A4R4G4B4 } else { FORMAT_R5G6B5 }, false)
    } else {
        (32, FORMAT_A8R8G8B8, !has_alpha)
    }
}

/// A manager's removal of a cached object at reference count `count` (`RemoveTexture`, `RemoveModel`,
/// `RemoveAll*`): removed and released when the manager's reference is the last, else left with "in use".
pub fn remove_decision(count: c_ulong) -> (bool, c_long) {
    if count == 1 {
        (true, OK)
    } else {
        (false, IN_USE)
    }
}

static TEXTURE_NUMBER: AtomicU32 = AtomicU32::new(1);
static MODEL_NUMBER: AtomicU32 = AtomicU32::new(1);

/// `MakeTexture`'s name of the next made texture ("GETextureManager::MakeTexture(N)", `sTextureNumber` from 1).
pub fn next_made_texture_name() -> Vec<u8> {
    let n = TEXTURE_NUMBER.fetch_add(1, Ordering::Relaxed);
    format!("GETextureManager::MakeTexture({n})").into_bytes()
}

/// `CreateModel`'s name of the next created model ("GEModelManager::CreateModel(<name>_N)", `sModelNumber` from 1).
pub fn next_created_model_name(name: &[u8]) -> Vec<u8> {
    let n = MODEL_NUMBER.fetch_add(1, Ordering::Relaxed);
    let mut out = b"GEModelManager::CreateModel(".to_vec();
    out.extend_from_slice(name);
    out.extend_from_slice(format!("_{n})").as_bytes());
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_stem_drops_the_last_extension_only_of_sst_names() {
        assert_eq!(texture_stem(b"textures\\x"), 10);
        assert_eq!(texture_stem(b"textures\\x.sst"), 10);
        assert_eq!(texture_stem(b"textures\\x.tga"), 14);
        assert_eq!(texture_stem(b"textures\\a.b\\x.sst"), 14);
        assert_eq!(texture_stem(b"textures/x.sst.png"), 14); // the last extension goes, whatever it is
        assert_eq!(texture_stem(b"dir.sst\\name"), 12); // no dot in the file part: the name as given
        assert_eq!(texture_stem(b""), 0);
    }

    #[test]
    fn set_clamps_and_turns_off() {
        let mut p = TexturePack::default();
        p.set(b"textures-4x", 2, 5);
        assert_eq!(p.directory, b"textures-4x\\");
        assert!(p.enabled);
        assert_eq!((p.scale_shift, p.mip_levels_to_drop), (2, 2));
        p.set(b"textures-4x", -1, -3);
        assert_eq!((p.scale_shift, p.mip_levels_to_drop), (0, 0));
        p.set(b"", 1, 0);
        assert!(!p.enabled);
        assert_eq!(p.directory, b"textures-4x\\"); // the directory stays
        p.set(&[b'd'; 63], 1, 0);
        assert!(!p.enabled); // too long for the original's buffer
        p.set(&[b'd'; 62], 1, 0);
        assert!(p.enabled);
    }

    #[test]
    fn pack_names_replace_the_textures_directory() {
        let mut p = TexturePack::default();
        assert_eq!(p.pack_name(b"textures\\x"), None); // no directory yet
        p.set(b"textures-4x", 1, 1);
        assert_eq!(p.pack_name(b"textures\\x").unwrap(), b"textures-4x\\x");
        assert_eq!(p.pack_name(b"TEXTURES\\Ab.sst").unwrap(), b"textures-4x\\Ab.sst");
        assert_eq!(p.pack_name(b"textures\\"), None);
        assert_eq!(p.pack_name(b"models\\x"), None);
        assert_eq!(p.pack_name(b"textures/x"), None); // the separator is part of the prefix
        assert_eq!(p.pack_name(b"textures\\colorpalette"), None);
        assert_eq!(p.pack_name(b"textures\\WaterPalette.png"), None);
        assert_eq!(p.pack_name(b"textures\\volcano"), None);
        assert!(p.pack_name(b"textures\\volcanoes").is_some());
        assert!(p.pack_name(b"textures\\colorpalettes").is_some());
        p.set(b"", 0, 0);
        assert!(p.pack_name(b"textures\\x").is_some()); // off, but the directory is kept
        assert_eq!(p.enabled_pack_name(b"textures\\x"), None);
    }

    #[test]
    fn the_load_plan_adds_the_packs_drop_and_mip_levels() {
        let mut p = TexturePack::default();
        p.set(b"textures-4x", 2, 1);
        assert_eq!(p.load_plan(false, 0, false), (0, 0, false));
        assert_eq!(p.load_plan(false, 1, false), (0, 1, false));
        assert_eq!(p.load_plan(false, 1, true), (1, 2, true)); // bigger than its logical size: MIP levels
        assert_eq!(p.load_plan(true, 0, true), (1, 1, true));
        p.set(b"textures-4x", 2, 2);
        assert_eq!(p.load_plan(false, 0, true), (2, 2, false)); // drawn at its pixel size: none
        assert_eq!(p.logical_shift(), 0);
        p.set(b"textures-4x", 2, 1);
        assert_eq!(p.logical_shift(), 1);
    }

    #[test]
    fn pack_sizes_give_the_callers_drop_back_and_the_logical_shift() {
        let mut p = TexturePack::default();
        p.set(b"textures-4x", 2, 1);
        assert_eq!(p.pack_sizes(1, 1), (0, 1));
        assert_eq!(p.pack_sizes(3, 1), (2, 1));
        assert_eq!(p.pack_sizes(0, 1), (0, 1));
        assert_eq!(p.pack_sizes(2, 2), (0, 0));
        assert_eq!(p.pack_sizes(2, 3), (0, 0)); // a shift below zero is zero
    }

    #[test]
    fn surface_formats_follow_the_depth_and_alpha() {
        assert_eq!(surface_format(16, true), (16, FORMAT_A4R4G4B4, false));
        assert_eq!(surface_format(16, false), (16, FORMAT_R5G6B5, false));
        assert_eq!(surface_format(32, true), (32, FORMAT_A8R8G8B8, false));
        assert_eq!(surface_format(32, false), (32, FORMAT_A8R8G8B8, true));
        assert_eq!(surface_format(24, false), (32, FORMAT_A8R8G8B8, true)); // anything but 16 is 32
    }

    #[test]
    fn removal_needs_the_managers_reference_to_be_the_last() {
        assert_eq!(remove_decision(1), (true, OK));
        assert_eq!(remove_decision(2), (false, IN_USE));
        assert_eq!(remove_decision(0), (false, IN_USE));
    }

    #[test]
    fn the_names_count_up() {
        let a = next_made_texture_name();
        let b = next_made_texture_name();
        assert!(a.starts_with(b"GETextureManager::MakeTexture(") && a.ends_with(b")"));
        assert_ne!(a, b);
        let m = next_created_model_name(b"tree");
        assert!(m.starts_with(b"GEModelManager::CreateModel(tree_") && m.ends_with(b")"));
    }

    #[test]
    fn the_global_pack_is_shared() {
        with_pack(|p| p.set(b"packdir", 3, 1));
        assert_eq!(with_pack(|p| p.logical_shift()), 2);
        with_pack(|p| p.set(b"", 0, 0));
    }
}
