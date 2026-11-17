//! `GEMaterial`, `GEFont`, `GEText` and `GEWindow` as whole Rust objects (R4.3, sixteenth part): every member of
//! GEMaterial.cpp, GEFont.cpp, GEText.cpp and GEWindow.cpp and of their headers' inline members.
//!
//! As `prim.rs`: the C++ classes stay the types the models, the UI and the renderer use; their only member is the
//! object and their methods are one-line forwarders. A material and a font keep their `UReferenceCounter` base (the
//! count stays C++). The calls the originals made on other objects are one host's operations (`EeGeObjOp`,
//! `GEObjectHost` in GEBitmap.cpp), in the original's order, where they cross into C++: making and deleting the
//! `UString`s and `UWideString`s they held by value, releasing the textures, fonts and font bitmaps they referenced
//! (a release may delete), the platform's fonts and windows. The strings' reads, assignments and comparisons, the
//! references taken and a font's object are read in place (`cppobj.rs`, R4.3 part 17). The text extent is
//! `font_ffi.rs`'s.

use std::ffi::{c_long, c_ulong, c_void};

use crate::ge::cppobj;
use crate::ge::font_ffi::{ee_ge_text_extent, GeTextOps};
use ee_base::math::types::{Ge2DPoint, GeRectangle};

/// The calls the four classes make on other objects.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeObjOp {
    /// `new UString` -> the string.
    StrNew = 1,
    /// `new UString(*a)` -> the string.
    StrCopy,
    /// `delete obj` (a `UString`).
    StrDelete,
    /// `new UWideString(a)` (a capacity in characters) -> the string.
    WStrNew,
    /// `delete obj` (a `UWideString`).
    WStrDelete,
    /// `obj->Dereference()` (a `GETexture`: its own).
    TexDereference,
    /// `obj->Dereference()` (a `UReferenceCounter`).
    RefDereference,
    /// `PlatFontCreate(obj, a, b & 1, (b >> 1) & 1, (b >> 2) & 1, (b >> 3) & 1)` (face, height, bold, italic,
    /// underline, antialiased) -> the font or NULL.
    PlatFontCreate,
    /// `PlatFontDestroy(obj)`.
    PlatFontDestroy,
    /// `PlatFontGetMetrics(obj, ...)`'s `mHeight`.
    PlatFontHeight,
    /// `GERustTextOps()` -> the platform's text functions (`GeTextOps`).
    TextOps,
    /// `PlatWindowDestroy(obj)`.
    PlatWindowDestroy,
    /// `PlatWindowSetSize(obj, a, b)`.
    PlatWindowSetSize,
    /// `PlatWindowGetPosition(obj, a, b)` (`int32_t*` outputs).
    PlatWindowPosition,
}

/// The C++ host: `owner` is the C++ object making the call.
pub type EeGeObjHostFn =
    unsafe extern "C" fn(owner: *mut c_void, op: EeGeObjOp, obj: *mut c_void, a: isize, b: isize) -> isize;

/// A host and the C++ object it serves.
#[derive(Clone, Copy)]
struct Host {
    host: EeGeObjHostFn,
    owner: usize,
}

impl Host {
    /// # Safety
    /// The operation's arguments are as `EeGeObjOp` describes.
    unsafe fn op(self, op: EeGeObjOp, obj: usize, a: isize, b: isize) -> isize {
        // SAFETY: the host's contract
        unsafe { (self.host)(self.owner as *mut c_void, op, obj as *mut c_void, a, b) }
    }
}

// ---- GEMaterial

/// A material's unsigned fields (`ee_ge_mat_get` / `ee_ge_mat_set`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeMatField {
    /// `GetLODCount` (read only).
    LodCount = 1,
    /// `Get/SetPolygonStartingIndex(lod)`.
    PolygonStart,
    /// `Get/SetPolygonRangeLength(lod)`.
    PolygonLength,
    /// `Get/SetTextureIndex`.
    TextureIndex,
    /// `Get/SetVertexStartingIndex`.
    VertexStart,
    /// `Get/SetVertexRangeLength`.
    VertexLength,
    /// The screens (`mScreenCount`, read only: the store gate's digest).
    ScreenCount,
}

/// The material.
pub struct EeGeMat {
    host: Host,
    /// `mName` (a C++ `UString`).
    name: usize,
    /// `mColor` (red, green, blue: `GEColor`'s layout).
    color: [f32; 3],
    /// `mTextures`, one per screen (referenced).
    textures: Vec<usize>,
    /// `mPolygonStartingIndices`, one per level of detail.
    polygon_starts: Vec<c_ulong>,
    /// `mPolygonRangeLengths`.
    polygon_lengths: Vec<c_ulong>,
    vertex_start: c_ulong,
    vertex_length: c_ulong,
    texture_index: c_ulong,
    /// `mOpacity`.
    opacity: f32,
}

/// `GEMaterial(screens, lods)`: the name, a white colour, empty texture slots, opacity 1, texture index 0. The
/// original left the polygon ranges and the vertex range uninitialized (`new unsigned long[]`): here they are 0.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_mat_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_new(
    host: EeGeObjHostFn,
    owner: *mut c_void,
    screens: c_ulong,
    lods: c_ulong,
) -> *mut EeGeMat {
    let host = Host { host, owner: owner as usize };
    // SAFETY: the host's contract
    let name = unsafe { host.op(EeGeObjOp::StrNew, 0, 0, 0) } as usize;
    Box::into_raw(Box::new(EeGeMat {
        host,
        name,
        color: [1.0; 3],
        textures: vec![0; screens as usize],
        polygon_starts: vec![0; lods as usize],
        polygon_lengths: vec![0; lods as usize],
        vertex_start: 0,
        vertex_length: 0,
        texture_index: 0,
        opacity: 1.0,
    }))
}

/// `~GEMaterial`: every screen's texture released, then the name.
///
/// # Safety
/// `p` is from `ee_ge_mat_new`; it is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_destroy(p: *mut EeGeMat) {
    // SAFETY: the caller's contract
    let m = unsafe { Box::from_raw(p) };
    for &texture in &m.textures {
        if texture != 0 {
            // SAFETY: a referenced texture
            unsafe { m.host.op(EeGeObjOp::TexDereference, texture, 0, 0) };
        }
    }
    // SAFETY: the material's own string
    unsafe { m.host.op(EeGeObjOp::StrDelete, m.name, 0, 0) };
}

/// `SetTexture(texture, screen)`: the old texture released, the new one referenced.
///
/// # Safety
/// `p` is live; `texture` is a texture or NULL; `screen` is one of its screens.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_set_texture(p: *mut EeGeMat, texture: *mut c_void, screen: c_ulong) {
    // SAFETY: the caller's contract (no reference is held across a host call)
    unsafe {
        let host = (*p).host;
        let textures: &Vec<usize> = &(*p).textures;
        let Some(&old) = textures.get(screen as usize) else { return };
        if old != 0 {
            host.op(EeGeObjOp::TexDereference, old, 0, 0);
        }
        let textures: &mut Vec<usize> = &mut (*p).textures;
        textures[screen as usize] = texture as usize;
        if !texture.is_null() {
            cppobj::reference(texture as usize);
        }
    }
}

/// `GetTexture(screen)` (NULL past the screens: the original read past its array).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_texture(p: *const EeGeMat, screen: c_ulong) -> *mut c_void {
    // SAFETY: the caller's contract
    let textures: &Vec<usize> = unsafe { &(*p).textures };
    textures.get(screen as usize).copied().unwrap_or(0) as *mut c_void
}

/// `GetName()`: the C++ `UString`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_name(p: *const EeGeMat) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).name as *mut c_void }
}

/// `SetName(name)`.
///
/// # Safety
/// `p` is live; `name` is a `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_set_name(p: *mut EeGeMat, name: *const c_void) {
    // SAFETY: the caller's contract
    unsafe { cppobj::str_assign((*p).name, name as usize) }
}

/// `GetColor()`: the colour inside the object (`GEColor`'s layout).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_color(p: *mut EeGeMat) -> *mut f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).color.as_mut_ptr() }
}

/// `GetOpacity()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_opacity(p: *const EeGeMat) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*p).opacity }
}

/// `SetOpacity(opacity)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_set_opacity(p: *mut EeGeMat, opacity: f32) {
    // SAFETY: the caller's contract
    unsafe { (*p).opacity = opacity }
}

/// One unsigned field (`lod` for the polygon ranges; 0 past the levels: the original read past its arrays).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_get(p: *const EeGeMat, field: EeGeMatField, lod: c_ulong) -> c_ulong {
    // SAFETY: the caller's contract
    let m = unsafe { &*p };
    match field {
        EeGeMatField::LodCount => m.polygon_starts.len() as c_ulong,
        EeGeMatField::PolygonStart => m.polygon_starts.get(lod as usize).copied().unwrap_or(0),
        EeGeMatField::PolygonLength => m.polygon_lengths.get(lod as usize).copied().unwrap_or(0),
        EeGeMatField::TextureIndex => m.texture_index,
        EeGeMatField::VertexStart => m.vertex_start,
        EeGeMatField::VertexLength => m.vertex_length,
        EeGeMatField::ScreenCount => m.textures.len() as c_ulong,
    }
}

/// Sets one unsigned field (a level past the arrays is ignored: the original wrote past them).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_set(p: *mut EeGeMat, field: EeGeMatField, lod: c_ulong, value: c_ulong) {
    // SAFETY: the caller's contract
    let m = unsafe { &mut *p };
    let slot = match field {
        EeGeMatField::LodCount | EeGeMatField::ScreenCount => return,
        EeGeMatField::PolygonStart => m.polygon_starts.get_mut(lod as usize),
        EeGeMatField::PolygonLength => m.polygon_lengths.get_mut(lod as usize),
        EeGeMatField::TextureIndex => Some(&mut m.texture_index),
        EeGeMatField::VertexStart => Some(&mut m.vertex_start),
        EeGeMatField::VertexLength => Some(&mut m.vertex_length),
    };
    if let Some(slot) = slot {
        *slot = value;
    }
}

/// `SizeOf()`: the original object (`object`, the C++'s size of its old layout), its name, texture slots and
/// per-level polygon ranges (`texture.rs`'s `material_size_of`).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_mat_size_of(
    p: *const EeGeMat,
    object: usize,
    string: usize,
    pointer: usize,
    ulong: usize,
) -> c_ulong {
    // SAFETY: the caller's contract
    let (name, screens, lods) = unsafe {
        let m = &*p;
        (m.name, m.textures.len(), m.polygon_starts.len())
    };
    // SAFETY: the material's own string
    let name_size = unsafe { cppobj::str_size_of(name) };
    crate::ge::texture_ffi::ee_ge_material_size_of(
        object,
        string,
        name_size,
        screens as c_ulong,
        pointer,
        lods as c_ulong,
        ulong,
    )
}

// ---- GEFont

/// A font's unsigned fields (`ee_ge_font_value`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGeFontValue {
    /// `GetFontQuality`.
    Quality = 1,
    /// `GetFontSize`.
    Size,
    /// `GetFontStyle`.
    Style,
    /// `GetHeight` (the platform font's cell, 0 without one).
    Height,
}

/// The font.
pub struct EeGeFont {
    host: Host,
    /// `mName` (a C++ `UString`).
    name: usize,
    /// `mFont` (a `PlatFont*`, NULL when none was made).
    font: usize,
    quality: c_ulong,
    style: c_ulong,
    /// `mFontBitmap` (referenced).
    bitmap: usize,
    size: c_ulong,
    height: c_long,
}

/// `GEFont(name, size, style, quality)`: the name copied, the platform font made (height `-size`; bold, italic and
/// underline from the style's bits 0-2; antialiased only for quality 2) and its cell height read.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_font_destroy`; `name` is a `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_new(
    host: EeGeObjHostFn,
    owner: *mut c_void,
    name: *const c_void,
    size: c_ulong,
    style: c_ulong,
    quality: c_ulong,
) -> *mut EeGeFont {
    let host = Host { host, owner: owner as usize };
    // SAFETY: the host's contract; each call is a statement of its own
    unsafe {
        let name = host.op(EeGeObjOp::StrCopy, 0, name as isize, 0) as usize;
        let face = cppobj::str_text(name) as usize;
        let flags =
            (style & 1) | (((style >> 1) & 1) << 1) | (((style >> 2) & 1) << 2) | ((quality == 2) as c_ulong) << 3;
        let height = (size as i32).wrapping_neg();
        let font = host.op(EeGeObjOp::PlatFontCreate, face, height as isize, flags as isize) as usize;
        let mut cell: c_long = 0;
        if font != 0 {
            cell = host.op(EeGeObjOp::PlatFontHeight, font, 0, 0) as c_long;
        }
        Box::into_raw(Box::new(EeGeFont { host, name, font, quality, style, bitmap: 0, size, height: cell }))
    }
}

/// `~GEFont`: the platform font destroyed, the font bitmap released, then the name.
///
/// # Safety
/// `p` is from `ee_ge_font_new`; it is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_destroy(p: *mut EeGeFont) {
    // SAFETY: the caller's contract
    let f = unsafe { Box::from_raw(p) };
    // SAFETY: the font's own objects
    unsafe {
        f.host.op(EeGeObjOp::PlatFontDestroy, f.font, 0, 0);
        if f.bitmap != 0 {
            f.host.op(EeGeObjOp::RefDereference, f.bitmap, 0, 0);
        }
        f.host.op(EeGeObjOp::StrDelete, f.name, 0, 0);
    }
}

/// `CompareFont(name, style, quality, size)`: the quality, style and size, then the name.
///
/// # Safety
/// `p` is live; `name` is a `UString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_compare(
    p: *const EeGeFont,
    name: *const c_void,
    style: c_ulong,
    quality: c_ulong,
    size: c_ulong,
) -> bool {
    // SAFETY: the caller's contract
    let (own, q, st, si) = unsafe {
        let f = &*p;
        (f.name, f.quality, f.style, f.size)
    };
    // SAFETY: the font's own string
    q == quality && st == style && si == size && unsafe { cppobj::str_equal(own, name as usize) }
}

/// `SetFontBitmap(bitmap)`: stored and referenced (the old one is kept referenced, as the original did).
///
/// # Safety
/// `p` is live; `bitmap` is a font bitmap.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_set_bitmap(p: *mut EeGeFont, bitmap: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        (*p).bitmap = bitmap as usize;
        cppobj::reference(bitmap as usize);
    }
}

/// `GetFontBitmap()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_bitmap(p: *const EeGeFont) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).bitmap as *mut c_void }
}

/// `GetFontName()`: the C++ `UString`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_name(p: *const EeGeFont) -> *const c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).name as *const c_void }
}

/// `GetOSFont()`: the `PlatFont*`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_os(p: *const EeGeFont) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).font as *mut c_void }
}

/// One unsigned field.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_font_value(p: *const EeGeFont, value: EeGeFontValue) -> c_long {
    // SAFETY: the caller's contract
    let f = unsafe { &*p };
    match value {
        EeGeFontValue::Quality => f.quality as c_long,
        EeGeFontValue::Size => f.size as c_long,
        EeGeFontValue::Style => f.style as c_long,
        EeGeFontValue::Height => f.height,
    }
}

// ---- GEText

/// The text.
pub struct EeGeText {
    host: Host,
    location: Ge2DPoint,
    background: [f32; 3],
    foreground: [f32; 3],
    extent: GeRectangle,
    /// `mText` (a C++ `UWideString`).
    text: usize,
    /// `mFont` (a referenced `GEFont`).
    font: usize,
    alignment: c_ulong,
    transparent: bool,
}

/// `GEText(font)`: at the origin, white on white, an empty extent, a 256-character string, the font referenced,
/// opaque, left-aligned.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_text_destroy`; `font` is a `GEFont`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_new(host: EeGeObjHostFn, owner: *mut c_void, font: *mut c_void) -> *mut EeGeText {
    let host = Host { host, owner: owner as usize };
    // SAFETY: the host's contract
    unsafe {
        let text = host.op(EeGeObjOp::WStrNew, 0, 0x100, 0) as usize;
        cppobj::reference(font as usize);
        Box::into_raw(Box::new(EeGeText {
            host,
            location: Ge2DPoint::default(),
            background: [1.0; 3],
            foreground: [1.0; 3],
            extent: GeRectangle::default(),
            text,
            font: font as usize,
            alignment: 0,
            transparent: false,
        }))
    }
}

/// `~GEText`: the font released, then the string.
///
/// # Safety
/// `p` is from `ee_ge_text_new`; it is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_destroy(p: *mut EeGeText) {
    // SAFETY: the caller's contract
    let t = unsafe { Box::from_raw(p) };
    // SAFETY: the text's own objects
    unsafe {
        t.host.op(EeGeObjOp::RefDereference, t.font, 0, 0);
        t.host.op(EeGeObjOp::WStrDelete, t.text, 0, 0);
    }
}

/// `ComputeTextExtent()`: nothing without a platform font; else the font's cell height and `ee_ge_text_extent` over
/// the text's bytes and character count.
///
/// # Safety
/// `p` is live.
unsafe fn compute_extent(p: *mut EeGeText) {
    // SAFETY: the caller's contract; no reference is held across a host call
    unsafe {
        let (host, font, text) = ((*p).host, (*p).font, (*p).text);
        let os = (*cppobj::counted::<EeGeFont>(font)).font;
        if os == 0 {
            return;
        }
        let height = host.op(EeGeObjOp::PlatFontHeight, os, 0, 0) as i32;
        let ops = host.op(EeGeObjOp::TextOps, 0, 0, 0) as *const GeTextOps;
        let bytes = cppobj::wstr_text(text);
        let characters = cppobj::wstr_length(text);
        let mut extent = GeRectangle::default();
        ee_ge_text_extent(ops, os as *const c_void, bytes, characters, height, &mut extent);
        (*p).extent = extent;
    }
}

/// `operator=(from)`: the font released and `from`'s referenced, then the flags, the string, the extent, the colours
/// and the location.
///
/// # Safety
/// `p` and `from` are live and distinct.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_assign(p: *mut EeGeText, from: *const EeGeText) {
    // SAFETY: the caller's contract
    unsafe {
        let host = (*p).host;
        host.op(EeGeObjOp::RefDereference, (*p).font, 0, 0);
        (*p).font = (*from).font;
        cppobj::reference((*p).font);
        (*p).transparent = (*from).transparent;
        (*p).alignment = (*from).alignment;
        cppobj::wstr_assign((*p).text, (*from).text);
        (*p).extent = (*from).extent;
        (*p).foreground = (*from).foreground;
        (*p).background = (*from).background;
        (*p).location = (*from).location;
    }
}

/// `SetFont(font)`: the old font released, the new one referenced, the extent measured again.
///
/// # Safety
/// `p` is live; `font` is a `GEFont`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_set_font(p: *mut EeGeText, font: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        let host = (*p).host;
        host.op(EeGeObjOp::RefDereference, (*p).font, 0, 0);
        (*p).font = font as usize;
        cppobj::reference(font as usize);
        compute_extent(p);
    }
}

/// `SetText(text)`: the string assigned, the extent measured again.
///
/// # Safety
/// `p` is live; `text` is a `UWideString`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_set_text(p: *mut EeGeText, text: *const c_void) {
    // SAFETY: the caller's contract
    unsafe {
        cppobj::wstr_assign((*p).text, text as usize);
        compute_extent(p);
    }
}

/// `GetText()`: the C++ `UWideString`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_string(p: *const EeGeText) -> *const c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).text as *const c_void }
}

/// `GetFont()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_font(p: *const EeGeText) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).font as *mut c_void }
}

/// `GetFgColor()` (`foreground`) or `GetBgColor()`: the colour inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_color(p: *mut EeGeText, foreground: bool) -> *mut f32 {
    // SAFETY: the caller's contract
    unsafe {
        if foreground {
            (*p).foreground.as_mut_ptr()
        } else {
            (*p).background.as_mut_ptr()
        }
    }
}

/// `GetLocation()`: the point inside the object (`SetLocation` writes it).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_location(p: *mut EeGeText) -> *mut Ge2DPoint {
    // SAFETY: the caller's contract
    unsafe { &mut (*p).location }
}

/// `GetTextExtent()`: the rectangle inside the object.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_extent_of(p: *const EeGeText) -> *const GeRectangle {
    // SAFETY: the caller's contract
    unsafe { &(*p).extent }
}

/// `GetAlignment()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_alignment(p: *const EeGeText) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*p).alignment }
}

/// `SetAlignment(alignment)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_set_alignment(p: *mut EeGeText, alignment: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { (*p).alignment = alignment }
}

/// `GetIsTransparent()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_transparent(p: *const EeGeText) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*p).transparent }
}

/// `SetIsTransparent(transparent)`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_text_set_transparent(p: *mut EeGeText, transparent: bool) {
    // SAFETY: the caller's contract
    unsafe { (*p).transparent = transparent }
}

// ---- GEWindow

/// The window.
pub struct EeGeWin {
    host: Host,
    /// `mWindow` (a `PlatWindow*`).
    window: usize,
    /// `mClientOrigin`, written by `GetClientOrigin`.
    origin: Ge2DPoint,
    width: c_ulong,
    height: c_ulong,
    /// `mDepth` (the creation style; nothing reads it).
    depth: c_ulong,
    /// `mIsFullScreen`: the destructor destroys the platform window.
    full_screen: bool,
}

/// `GEWindow(window, width, height, depth, full screen)`.
///
/// # Safety
/// `host` serves `owner` until `ee_ge_win_destroy`.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_new(
    host: EeGeObjHostFn,
    owner: *mut c_void,
    window: *mut c_void,
    width: c_ulong,
    height: c_ulong,
    depth: c_ulong,
    full_screen: bool,
) -> *mut EeGeWin {
    let host = Host { host, owner: owner as usize };
    Box::into_raw(Box::new(EeGeWin {
        host,
        window: window as usize,
        origin: Ge2DPoint::default(),
        width,
        height,
        depth,
        full_screen,
    }))
}

/// `~GEWindow`: the platform window destroyed only in full-screen mode.
///
/// # Safety
/// `p` is from `ee_ge_win_new`; it is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_destroy(p: *mut EeGeWin) {
    // SAFETY: the caller's contract
    let w = unsafe { Box::from_raw(p) };
    let _ = w.depth;
    if w.full_screen {
        // SAFETY: the window's platform window
        unsafe { w.host.op(EeGeObjOp::PlatWindowDestroy, w.window, 0, 0) };
    }
}

/// `GetClientOrigin()`: the platform window's position, stored in the object and returned.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_origin(p: *mut EeGeWin) -> *const Ge2DPoint {
    // SAFETY: the caller's contract
    unsafe {
        let (host, window) = ((*p).host, (*p).window);
        let (mut x, mut y) = (0i32, 0i32);
        host.op(EeGeObjOp::PlatWindowPosition, window, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize);
        (*p).origin = Ge2DPoint { x: x as f32, y: y as f32 };
        &(*p).origin
    }
}

/// `GetWidth()` (`height` false) or `GetHeight()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_size(p: *const EeGeWin, height: bool) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe {
        if height {
            (*p).height
        } else {
            (*p).width
        }
    }
}

/// `GetOSRepresentation()`: the `PlatWindow*`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_os(p: *const EeGeWin) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*p).window as *mut c_void }
}

/// `Resize(width, height)`: the platform window's client area, then the stored size.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_win_resize(p: *mut EeGeWin, width: c_ulong, height: c_ulong) {
    // SAFETY: the caller's contract
    unsafe {
        let (host, window) = ((*p).host, (*p).window);
        host.op(EeGeObjOp::PlatWindowSetSize, window, width as i32 as isize, height as i32 as isize);
        (*p).width = width;
        (*p).height = height;
    }
}

#[cfg(test)]
#[path = "smallobj_tests.rs"]
mod tests;
