//! `GEFontBitmap` as a whole Rust object (R4.3, tenth part): a font's glyph cells (one per character of its range,
//! each a rectangle in one of the 256 x 256 glyph textures, its texture coordinates, scales and texture) and its
//! drawing bitmap, with every method of GEFontBitmap.cpp (the constructor's glyph rendering, the destructor, the
//! glyph textures, the alpha pass, the two draws).
//!
//! As `bmpobj.rs`: the C++ `GEFontBitmap` stays the type the fonts, the rasterizers and the UI use; its methods are
//! one-line forwarders and its only member is this object. Every call the original made on another object (the
//! rasterizer, the font, the working text, the drawing bitmap, the textures, surfaces and screens) is one host
//! operation (`EeGeFbOp`, `GEFontBitmap::Host`), in the original's order. The cell layout, the alpha pass and the
//! draw plan are `font.rs`'s.

use std::ffi::{c_char, c_long, c_ulong, c_void};

use crate::ge::font::{self, GeFontPacker, GeGlyphDraw, GE_GLYPH_FULL, GE_GLYPH_SKIPPED};
use crate::ge::font_ffi::{GeFbCharacter, GeTextOps};
use crate::ge::texobj::{EeGeSurfaceDescription, EeGeTexStage};
use ee_base::math::types::{Ge2DPoint, GeRectangle};

/// The calls a font bitmap makes on other objects. `obj` is the object called; `a`, `b` as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeFbOp {
    /// `new GEText(obj)` (the constructor's working text: the font referenced) -> the text.
    TextNew = 1,
    /// `delete (GEText*)obj` (the font released).
    TextDelete,
    /// `obj->SetFgColor(GEColor())`, then `obj->SetIsTransparent(true)`.
    TextPrepare,
    /// The text set to Windows-1252 byte `a` as UTF-8 (`PlatCp1252ToUtf8`, then `obj->SetText`).
    TextSetByte,
    /// `obj->GetTextExtent()`'s width and height into `*(float(*)[2])a`.
    TextExtent,
    /// `obj->SetLocation(*(const GE2DPoint*)a)`.
    TextSetLocation,
    /// `(const char*)obj->GetText()`.
    TextString,
    /// `&obj->GetLocation()`.
    TextLocation,
    /// `&obj->GetFgColor()`.
    TextColor,
    /// The platform's text functions (`GERustTextOps()`).
    TextOps,
    /// `PlatFontGetMetrics(obj->GetOSFont())`'s first and last characters into `*(unsigned long(*)[2])a`.
    FontRange,
    /// `obj->SetFontBitmap(owner)`.
    FontSetBitmap,
    /// `obj->GetScreen(a)` (a rasterizer).
    RasterScreen,
    /// `obj->GetBlending((*(unsigned long(*)[2])a)[0], [1], b)`.
    RasterGetBlending,
    /// `obj->SetBlending((*(const unsigned long(*)[2])a)[0], [1], b)`.
    RasterSetBlending,
    /// `obj->Draw((GEText*)a, (GESurface*)b)`.
    RasterDrawText,
    /// `obj->Draw((GEBitmap*)a, b)` -> its result.
    RasterDrawBitmap,
    /// `new GEBitmap`.
    BmpNew,
    /// `obj->Dereference()` (a bitmap).
    BmpDereference,
    /// `obj->SetTexture((GETexture*)a, b)`.
    BmpSetTexture,
    /// `obj->ComputeStretchedCoordsAndScale(r[0], r[0], r[1], b)` with `r = *(const GERectangle(*)[2])a`.
    BmpStretch,
    /// `obj->GetTextureCoordinatesPerVertex(c[0], t, t, c[1])` with `c = *(GETextureCoordinate(*)[2])a`.
    BmpUvsPerVertex,
    /// `obj->GetXScale()`, `obj->GetYScale()` into `*(float(*)[2])a`.
    BmpScales,
    /// `obj->SetColor(*(const GEColor*)a)`.
    BmpSetColor,
    /// `obj->SetTextureCoordinates(c[0], c[1], c[2], c[3])` with `c = *(const GETextureCoordinate(*)[4])a`.
    BmpSetUvs,
    /// `obj->SetXYScale(a's bits, b's bits)`.
    BmpSetXyScale,
    /// `obj->SetLocation(*(const GE2DPoint*)a)`.
    BmpSetLocation,
    /// `obj->GetSurface()` (a texture).
    TexSurface,
    /// `obj->Reference()` (a texture).
    TexReference,
    /// The texture's stage setter `a` (`EeGeTexStage`) with `b`, for stage -1.
    TexStage,
    /// `obj->TextUnlock()` (a surface).
    SurfTextUnlock,
    /// `obj->Lock()`.
    SurfLock,
    /// `obj->Unlock()`.
    SurfUnlock,
    /// `obj->GetMemoryAddress()`, with `GetWidth()`, `GetHeight()` and `GetBytesPerRow()` into
    /// `*(unsigned long(*)[3])a`.
    SurfPixels,
    /// `obj->Clear(GEColor(0, 0, 0), 1.0f)`.
    SurfClear,
    /// `obj->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b)` (a screen) -> its result.
    ScreenCreateSurface,
    /// `obj->GetTextureManager()->MakeTexture((GESurface*)a)` (a screen) -> the texture.
    ScreenMakeTexture,
    /// `ESVerify` of the constructor's check `a`: 0 the surface's creation (line 228), 1 the texture (235).
    Verify,
}

/// The host: `owner` is the C++ font bitmap of the object calling.
pub type EeGeFbHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeFbOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// `kBlendSrcAlpha`, `kBlendInvSrcAlpha` (lle/GERenderState.h).
const BLEND_SRC_ALPHA: c_ulong = 4;
const BLEND_INV_SRC_ALPHA: c_ulong = 5;
/// `kSurfaceTexture`, `kPixelFormatA4R4G4B4`.
const SURFACE_TEXTURE: c_ulong = 5;
const PIXEL_FORMAT_A4R4G4B4: c_ulong = 2;
/// The glyph textures' stage: unfiltered, no MIP filter, clamped, alpha from the texture, colour modulated.
const GLYPH_STAGE: [(EeGeTexStage, isize); 6] = [
    (EeGeTexStage::MinFilterMode, 0),
    (EeGeTexStage::MagFilterMode, 0),
    (EeGeTexStage::MipFilterMode, 0),
    (EeGeTexStage::TextureAddressingMode, 0),
    (EeGeTexStage::AlphaMode, 1),
    (EeGeTexStage::ColorMode, 2),
];

/// The font bitmap.
pub struct EeGeFontBitmap {
    host: EeGeFbHostFn,
    /// The C++ font bitmap (`this`).
    owner: usize,
    /// `mCharacters`, indexed by character - `first_character`.
    characters: Vec<GeFbCharacter>,
    /// `mBitmap` (referenced).
    bitmap: usize,
    /// `mFirstCharacter`.
    first_character: c_ulong,
}

/// A cell before its glyph is rendered.
const EMPTY_CELL: GeFbCharacter = GeFbCharacter {
    top_left: [0.0; 2],
    bottom_right: [0.0; 2],
    rect: GeRectangle { bottom: 0.0, left: 0.0, right: 0.0, top: 0.0 },
    texture: std::ptr::null_mut(),
    x_scale: 0.0,
    y_scale: 0.0,
};

/// # Safety
/// `p` is a live object from `ee_ge_fb_new`.
unsafe fn fb<'a>(p: *mut EeGeFontBitmap) -> &'a mut EeGeFontBitmap {
    // SAFETY: the caller's contract; the reference is not held across a host call
    unsafe { &mut *p }
}

impl EeGeFontBitmap {
    /// A host call made by font bitmap `p`.
    ///
    /// # Safety
    /// `p` is live; the operation's arguments are as `EeGeFbOp` describes.
    unsafe fn op(p: *mut Self, op: EeGeFbOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the caller's contract (host and owner are copied out before the call)
        let (host, owner) = unsafe { (fb(p).host, fb(p).owner) };
        // SAFETY: the host's contract
        unsafe { host(owner as *mut c_void, op, obj as *mut c_void, a, b) }
    }

    /// A host call with a pointer argument.
    ///
    /// # Safety
    /// As `op`; `arg` points at what `op` reads or writes.
    unsafe fn op_ptr<T>(p: *mut Self, op: EeGeFbOp, obj: usize, arg: *const T, b: isize) -> isize {
        // SAFETY: the caller's contract
        unsafe { Self::op(p, op, obj, arg as isize, b) }
    }

    /// `GEFontBitmap(rasterizer, font, screen)`: every character of the font's range drawn by the rasterizer into
    /// the glyph textures (a new texture when the cells reach the bottom edge, each finished one turned into alpha),
    /// with each cell's texture coordinates and scales measured through the drawing bitmap; the font then holds the
    /// object. The rasterizer's blending is source alpha for the time.
    ///
    /// # Safety
    /// `host` serves `owner` from now on; the pointers are live C++ objects.
    pub unsafe fn create(host: EeGeFbHostFn, owner: usize, raster: usize, font: usize, screen: c_ulong) -> *mut Self {
        let p = Box::into_raw(Box::new(Self { host, owner, characters: Vec::new(), bitmap: 0, first_character: 0 }));
        // SAFETY: `p` is live; every host call is a statement of its own
        unsafe {
            let text = Self::op(p, EeGeFbOp::TextNew, font, 0, 0) as usize;
            let mut packer = GeFontPacker::default();
            let screen_obj = Self::op(p, EeGeFbOp::RasterScreen, raster, screen as isize, 0) as usize;
            let mut texture = 0usize;
            let mut saved: [c_ulong; 2] = [0; 2];
            Self::op_ptr(p, EeGeFbOp::RasterGetBlending, raster, saved.as_mut_ptr(), screen as isize);
            let blend: [c_ulong; 2] = [BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA];
            Self::op_ptr(p, EeGeFbOp::RasterSetBlending, raster, blend.as_ptr(), screen as isize);
            let bitmap = Self::op(p, EeGeFbOp::BmpNew, 0, 0, 0) as usize;
            fb(p).bitmap = bitmap;
            Self::op(p, EeGeFbOp::TextPrepare, text, 0, 0);
            let mut range: [c_ulong; 2] = [0; 2];
            Self::op_ptr(p, EeGeFbOp::FontRange, font, range.as_mut_ptr(), 0);
            let (first, last) = (range[0], range[1]);
            fb(p).first_character = first;
            let count = last.wrapping_sub(first).wrapping_add(1) as usize;
            fb(p).characters = vec![EMPTY_CELL; count];
            let mut character = first;
            while character <= last {
                let index = character.wrapping_sub(first) as usize;
                Self::op(p, EeGeFbOp::TextSetByte, text, (character as u8) as isize, 0);
                let mut extent = [0.0f32; 2];
                Self::op_ptr(p, EeGeFbOp::TextExtent, text, extent.as_mut_ptr(), 0);
                let (place, new_texture) = packer.place(extent[0], extent[1]);
                if new_texture {
                    if texture != 0 {
                        let surface = Self::op(p, EeGeFbOp::TexSurface, texture, 0, 0) as usize;
                        Self::op(p, EeGeFbOp::SurfTextUnlock, surface, 0, 0);
                        Self::surface_alpha(p, surface);
                    }
                    texture = Self::new_font_texture(p, screen_obj);
                }
                fb(p).characters[index].rect = place;
                let top_left = Ge2DPoint { x: place.left, y: place.top };
                Self::op_ptr(p, EeGeFbOp::TextSetLocation, text, &top_left, 0);
                let surface = Self::op(p, EeGeFbOp::TexSurface, texture, 0, 0);
                Self::op(p, EeGeFbOp::RasterDrawText, raster, text as isize, surface);
                fb(p).characters[index].texture = texture as *mut c_void;
                Self::op(p, EeGeFbOp::TexReference, texture, 0, 0);
                Self::op(p, EeGeFbOp::BmpSetTexture, bitmap, texture as isize, screen as isize);
                let inner = GeRectangle {
                    bottom: place.bottom - 1.0,
                    left: place.left,
                    right: place.right - 1.0,
                    top: place.top,
                };
                let rects = [inner, place];
                Self::op_ptr(p, EeGeFbOp::BmpStretch, bitmap, rects.as_ptr(), screen as isize);
                let mut uvs = [[0.0f32; 2]; 2];
                Self::op_ptr(p, EeGeFbOp::BmpUvsPerVertex, bitmap, uvs.as_mut_ptr(), 0);
                let mut scales = [0.0f32; 2];
                Self::op_ptr(p, EeGeFbOp::BmpScales, bitmap, scales.as_mut_ptr(), 0);
                let cell = &mut fb(p).characters[index];
                cell.top_left = uvs[0];
                cell.bottom_right = uvs[1];
                cell.x_scale = scales[0];
                cell.y_scale = scales[1];
                if character == c_ulong::MAX {
                    break;
                }
                character += 1;
            }
            Self::op(p, EeGeFbOp::FontSetBitmap, font, 0, 0);
            let surface = Self::op(p, EeGeFbOp::TexSurface, texture, 0, 0) as usize;
            Self::op(p, EeGeFbOp::SurfTextUnlock, surface, 0, 0);
            Self::surface_alpha(p, surface);
            Self::op_ptr(p, EeGeFbOp::RasterSetBlending, raster, saved.as_ptr(), screen as isize);
            Self::op(p, EeGeFbOp::TextDelete, text, 0, 0);
        }
        p
    }

    /// The destructor: the drawing bitmap released (the glyph textures keep the cells' references, as the
    /// original's); the object freed.
    ///
    /// # Safety
    /// `p` is live; it is freed.
    pub unsafe fn destroy(p: *mut Self) {
        // SAFETY: the caller's contract
        unsafe {
            let bitmap = fb(p).bitmap;
            if bitmap != 0 {
                Self::op(p, EeGeFbOp::BmpDereference, bitmap, 0, 0);
            }
            drop(Box::from_raw(p));
        }
    }

    /// `CreateNewFontTexture`: a cleared 256 x 256 A4R4G4B4 texture surface of the screen, made a texture of its
    /// manager, unfiltered and clamped, alpha from the texture.
    ///
    /// # Safety
    /// `p` is live; `screen` a live C++ screen.
    unsafe fn new_font_texture(p: *mut Self, screen: usize) -> usize {
        let description = EeGeSurfaceDescription {
            kind: SURFACE_TEXTURE,
            pixel_format: PIXEL_FORMAT_A4R4G4B4,
            surface_volatility: 1,
            width: font::GE_FONT_ATLAS_SIZE as c_ulong,
            height: font::GE_FONT_ATLAS_SIZE as c_ulong,
            m14: 0,
            mip_level_count: 0,
            pixel_depth: 16,
            is_3d_renderable: false,
            is_transparency_enabled: false,
            no_alpha: false,
        };
        // SAFETY: the caller's contract
        unsafe {
            let mut surface = 0usize;
            let result = Self::op_ptr(
                p,
                EeGeFbOp::ScreenCreateSurface,
                screen,
                &description,
                &mut surface as *mut usize as isize,
            ) as c_long;
            if result != 0 {
                Self::op(p, EeGeFbOp::Verify, 0, 0, 0);
            }
            Self::op(p, EeGeFbOp::SurfClear, surface, 0, 0);
            let texture = Self::op(p, EeGeFbOp::ScreenMakeTexture, screen, surface as isize, 0) as usize;
            if texture == 0 {
                Self::op(p, EeGeFbOp::Verify, 0, 1, 0);
            }
            for (stage, value) in GLYPH_STAGE {
                Self::op(p, EeGeFbOp::TexStage, texture, stage as isize, value);
            }
            texture
        }
    }

    /// `CreateSurfaceAlpha`: the surface locked, its glyphs turned white with their alpha from green, unlocked.
    ///
    /// # Safety
    /// `p` is live; `surface` a live C++ surface.
    unsafe fn surface_alpha(p: *mut Self, surface: usize) {
        // SAFETY: the caller's contract; the host answers the locked surface's memory and its sizes
        unsafe {
            Self::op(p, EeGeFbOp::SurfLock, surface, 0, 0);
            let mut sizes: [c_ulong; 3] = [0; 3];
            let pixels = Self::op_ptr(p, EeGeFbOp::SurfPixels, surface, sizes.as_mut_ptr(), 0) as *mut u16;
            let n = font::surface_alpha_len(sizes[0], sizes[1], sizes[2]);
            if n != 0 && !pixels.is_null() {
                font::surface_alpha(std::slice::from_raw_parts_mut(pixels, n), sizes[0], sizes[1], sizes[2]);
            }
            Self::op(p, EeGeFbOp::SurfUnlock, surface, 0, 0);
        }
    }

    /// `Draw(text, rasterizer, screen)` (`clip` `None`) and `Draw(text, rectangle, rasterizer, screen)`: the text's
    /// glyphs (`font.rs`'s plan) drawn through the bitmap in the text's colour -> the last draw's result.
    ///
    /// # Safety
    /// `p` is live; `text` and `raster` live C++ objects.
    pub unsafe fn draw(
        p: *mut Self,
        text: usize,
        clip: Option<&GeRectangle>,
        raster: usize,
        screen: c_ulong,
    ) -> c_long {
        // SAFETY: the caller's contract; the plan is made before any draw (it reads the text and its location)
        unsafe {
            let string = Self::op(p, EeGeFbOp::TextString, text, 0, 0) as *const c_char;
            let location = *(Self::op(p, EeGeFbOp::TextLocation, text, 0, 0) as *const Ge2DPoint);
            let ops = &*(Self::op(p, EeGeFbOp::TextOps, 0, 0, 0) as *const GeTextOps);
            let rects: Vec<GeRectangle> = fb(p).characters.iter().map(|c| c.rect).collect();
            let mut plan: Vec<GeGlyphDraw> = Vec::new();
            let bytes = std::ffi::CStr::from_ptr(string).to_bytes_with_nul();
            font::draw_plan(bytes, fb(p).first_character, &rects, location, clip, ops, &mut plan);
            let bitmap = fb(p).bitmap;
            let color = Self::op(p, EeGeFbOp::TextColor, text, 0, 0);
            Self::op(p, EeGeFbOp::BmpSetColor, bitmap, color, 0);
            let mut result: c_long = 0;
            for glyph in &plan {
                let cell = &fb(p).characters[glyph.cell as usize];
                let (texture, top_left, bottom_right, scales) =
                    (cell.texture, cell.top_left, cell.bottom_right, [cell.x_scale, cell.y_scale]);
                Self::op(p, EeGeFbOp::BmpSetTexture, bitmap, texture as isize, screen as isize);
                if glyph.kind == GE_GLYPH_SKIPPED {
                    continue;
                }
                if glyph.kind == GE_GLYPH_FULL {
                    let uvs = [top_left, [bottom_right[0], top_left[1]], [top_left[0], bottom_right[1]], bottom_right];
                    Self::op_ptr(p, EeGeFbOp::BmpSetUvs, bitmap, uvs.as_ptr(), 0);
                    Self::op(
                        p,
                        EeGeFbOp::BmpSetXyScale,
                        bitmap,
                        scales[0].to_bits() as isize,
                        scales[1].to_bits() as isize,
                    );
                } else {
                    let rects = [glyph.destination, glyph.source];
                    Self::op_ptr(p, EeGeFbOp::BmpStretch, bitmap, rects.as_ptr(), screen as isize);
                }
                Self::op_ptr(p, EeGeFbOp::BmpSetLocation, bitmap, &glyph.location, 0);
                result = Self::op(p, EeGeFbOp::RasterDrawBitmap, raster, bitmap as isize, screen as isize) as c_long;
            }
            result
        }
    }
}

/// `new GEFontBitmap(rasterizer, font, screen)`'s object (the glyphs rendered; the font holds `owner`).
///
/// # Safety
/// `host` serves `owner` (a constructed `UReferenceCounter`) from now on; the pointers are live C++ objects.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_fb_new(
    host: EeGeFbHostFn,
    owner: *mut c_void,
    raster: *mut c_void,
    font: *mut c_void,
    screen: c_ulong,
) -> *mut EeGeFontBitmap {
    // SAFETY: the function's contract
    unsafe { EeGeFontBitmap::create(host, owner as usize, raster as usize, font as usize, screen) }
}

/// `~GEFontBitmap`.
///
/// # Safety
/// `p` is live or NULL; it is freed.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_fb_destroy(p: *mut EeGeFontBitmap) {
    if !p.is_null() {
        // SAFETY: the function's contract
        unsafe { EeGeFontBitmap::destroy(p) }
    }
}

/// `GEFontBitmap::Draw` (`clip` NULL: the unclipped draw) -> the last draw's result.
///
/// # Safety
/// `p` is live; `text`, `raster` live C++ objects; `clip` NULL or valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_fb_draw(
    p: *mut EeGeFontBitmap,
    text: *mut c_void,
    clip: *const GeRectangle,
    raster: *mut c_void,
    screen: c_ulong,
) -> c_long {
    if p.is_null() {
        return 0;
    }
    // SAFETY: the function's contract
    unsafe { EeGeFontBitmap::draw(p, text as usize, clip.as_ref(), raster as usize, screen) }
}

#[cfg(test)]
#[path = "fbobj_tests.rs"]
mod tests;
