//! The UI backgrounds whole in Rust (R6 ui round 20): `UIBackground`, `UITextureBackground`, `UIBitmapBackground`,
//! `UITiledBackground` (with the left-right, top-bottom and stretched tilings that were the classes
//! `UILRTiledBackground`, `UITBTiledBackground` and `UIStretchedTiledBackground`), `UIBFull` and `UIFullBackground`.
//!
//! [`EeUiBackgroundFields`] is the C++ classes' base (`UIBackground` and the root class `UIFullBackground` derive
//! from it), the field names the C++ members' (`controlobj.rs`'s pattern): the C++ inline getters and the game's
//! subclasses (`EEUIBGenericBackground`, `EEUIBMapBackground`) read them in place. Every method is a call here
//! ([`EeUiBgCall`]), each C++ class's member passing its own call (a qualified `UIBFull::DrawBackground` stays
//! that class's); the tiling and drawing plans are `background.rs`'s (round 5).
//!
//! Every call out is an [`EeUiOp`] (540..562) on the record's host (`UIBackground.cpp`'s `UIBackgroundHost::Op`):
//! the form's and manager's questions (pure), the form's locks and redraws, the virtuals a subclass may override
//! (`LoadBackground`, `UnloadBackground`, `DrawStrectchedBackground`, `DrawTiledBackground`), and the bitmaps and
//! textures by address. No reference into the record is held across a call out (the virtuals re-enter): each step
//! reads or writes the fields it needs afresh.

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::background::{self, FullHost, MoveHost, TileHost, Tiling};
use crate::layout::Layout;
use crate::obj::{buf_rect, read_buf, rect_buf, EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::UiRect;

/// `GETextureDescription` (`lle/GETextureManager.h`), copied with the background.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUiTextureDescription {
    pub mSurfaceVolatility: c_ulong,
    pub mFrameFormat: c_ulong,
    pub mPixelDepth: c_ulong,
    pub mNumMipLevelsToDrop: c_long,
    pub mIs3DRenderable: bool,
    pub mIsTransparencyEnabled: bool,
    pub mHasAlpha: bool,
    pub mCreateMIPMaps: bool,
    pub mUseTexturePack: bool,
}

/// The goldens' trace of a description: FNV-1a over its fields (each 32 bits, then the five flags).
#[allow(clippy::unnecessary_cast)] // `c_ulong` is 64 bits on LP64
pub fn description_digest(d: &EeUiTextureDescription) -> isize {
    let mut h: u32 = 2166136261;
    let mut mix = |b: u8| h = (h ^ b as u32).wrapping_mul(16777619);
    for v in [d.mSurfaceVolatility as u32, d.mFrameFormat as u32, d.mPixelDepth as u32, d.mNumMipLevelsToDrop as u32] {
        for b in v.to_le_bytes() {
            mix(b);
        }
    }
    for f in [d.mIs3DRenderable, d.mIsTransparencyEnabled, d.mHasAlpha, d.mCreateMIPMaps, d.mUseTexturePack] {
        mix(f as u8);
    }
    h as isize
}

/// The backgrounds' members (`include/lle/UIBackground.h` and its subclasses'), the C++ classes' base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiBackgroundFields {
    /// The C++ background (the host's object).
    pub mCpp: *mut c_void,
    /// The host of the calls out.
    pub mHost: EeUiHostFn,
    pub mLayout: Layout,
    pub mRectangle: UiRect,
    /// `UITextureBackground`'s: the texture's rectangle.
    pub mTextureRectangle: UiRect,
    /// `UITextureBackground`'s.
    pub mTextureDescription: EeUiTextureDescription,
    pub mBackgroundID: c_ulong,
    pub mDrawPriority: c_ulong,
    /// The form (`UIForm*`).
    pub mForm: *mut c_void,
    /// The texture file's name (`UITextureBackground::mTextureFileName`, `UIFullBackground::mFileName`): NUL-terminated
    /// bytes, owned (`mNameSize` with the NUL), NULL before [`ee_ui_bg_texture`].
    pub mName: *mut u8,
    pub mNameSize: usize,
    /// `UIBitmapBackground`'s bitmap (`GEBitmap*`).
    pub mBitmap: *mut c_void,
    /// `UIBFull`'s and `UIFullBackground`'s tile bitmaps, row major (owned, `mBitmapCount` of them; NULL for none).
    pub mBitmaps: *mut *mut c_void,
    pub mBitmapCount: usize,
    pub mColumns: c_ulong,
    pub mRows: c_ulong,
    /// `UIFullBackground`'s: the screen size the grid was designed for.
    pub mDesignedWidth: c_ulong,
    pub mDesignedHeight: c_ulong,
    /// `UITiledBackground`'s tiling: 0 both ways, 1 left to right, 2 top to bottom, 3 stretched
    /// (`mHorizontalCount` x `mVerticalCount` tiles).
    pub mTiling: c_ulong,
    pub mHorizontalCount: f32,
    pub mVerticalCount: f32,
    pub mIsVisible: bool,
}

/// The inward calls (each C++ class's methods forward here with their own call).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiBgCall {
    /// `UIBackground::Close()`.
    Close = 1,
    /// `UIBackground::Open()`.
    Open = 2,
    /// `UIBackground::RelayoutBackground()`.
    Relayout = 3,
    /// `SetRectangle(f)`.
    SetRectangle = 4,
    /// `SetVisibility(a != 0)`.
    SetVisibility = 5,
    /// `UIBitmapBackground`'s constructor: its bitmap.
    BitmapInit = 6,
    /// `UIBitmapBackground`'s destructor.
    BitmapRelease = 7,
    /// `UIBitmapBackground::DrawBackground(f)`.
    BitmapDraw = 8,
    /// `UIBitmapBackground::DrawStrectchedBackground(f[0..4], f[4..8])` (twelve floats).
    BitmapStretched = 9,
    /// `UIBitmapBackground::LoadBackground()`.
    BitmapLoad = 10,
    /// `UIBitmapBackground::SetBitmapTexture(texture a)`.
    BitmapSetTexture = 11,
    /// `UIBitmapBackground::UnloadBackground()`.
    BitmapUnload = 12,
    /// `UITiledBackground::DrawBackground(f)` (the tiling's).
    TiledDraw = 13,
    /// `UITiledBackground::DrawTiledBackground(f[0..4], a, b, f[4] bits, f[5] bits, f[6], f[7])` (twelve floats).
    DrawTiled = 14,
    /// `UIBFull`'s constructor: a columns by b rows.
    BFullInit = 15,
    /// `UIBFull`'s destructor.
    BFullRelease = 16,
    /// `UIBFull::DrawBackground(f)`.
    BFullDraw = 17,
    /// `UIBFull::LoadBackground()`.
    BFullLoad = 18,
    /// `UIBFull::SetBitmapTexture(texture a, column b, row f[0] bits)`.
    BFullSetBitmapTexture = 19,
    /// `UIBFull::SetNumberColumnsAndRows(a, b)`.
    BFullSetGrid = 20,
    /// `UIBFull::UnloadBackground()`.
    BFullUnload = 21,
    /// `UIFullBackground`'s destructor.
    FullRelease = 22,
    /// `UIFullBackground::DrawBackground(f)`.
    FullDraw = 23,
    /// `UIFullBackground::LoadBackground(*(GETextureDescription*)a)`.
    FullLoad = 24,
    /// `UIFullBackground::UnloadBackground()`.
    FullUnload = 25,
}

/// A background's record, reached by address: every access reads or writes the C++ object's base afresh.
#[derive(Clone, Copy)]
pub struct Bg(*mut EeUiBackgroundFields);

fn texture_rect(w: c_ulong, h: c_ulong) -> UiRect {
    UiRect::new(0.0, 0.0, w as f32, h as f32)
}

impl Bg {
    /// # Safety
    /// `p` is a live record (a C++ background's base) for as long as the value is used.
    pub unsafe fn new(p: *mut EeUiBackgroundFields) -> Self {
        Bg(p)
    }

    fn get<T>(&self, f: impl FnOnce(&EeUiBackgroundFields) -> T) -> T {
        // SAFETY: live (the constructor's contract); the borrow ends before any call out
        f(unsafe { &*self.0 })
    }

    fn set(&self, f: impl FnOnce(&mut EeUiBackgroundFields)) {
        // SAFETY: as get
        f(unsafe { &mut *self.0 })
    }

    fn host(&self) -> Host {
        self.get(|r| Host { obj: r.mCpp, f: r.mHost })
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host().op(op, a, b, 0, 0)
    }

    fn rect(&self) -> UiRect {
        self.get(|r| r.mRectangle)
    }

    /// Tile `i`'s bitmap (`mBitmaps[i]`).
    fn tile(&self, i: c_ulong) -> isize {
        self.get(|r| {
            assert!((i as usize) < r.mBitmapCount, "a background tile out of its grid");
            // SAFETY: inside the array (checked)
            unsafe { *r.mBitmaps.add(i as usize) as isize }
        })
    }

    fn set_tile(&self, i: c_ulong, b: isize) {
        self.set(|r| {
            assert!((i as usize) < r.mBitmapCount, "a background tile out of its grid");
            // SAFETY: inside the array (checked)
            unsafe { *r.mBitmaps.add(i as usize) = b as *mut c_void };
        })
    }

    fn texture_of(&self, bitmap: isize) -> isize {
        self.op(Op::BgBitmapTexture, bitmap, 0)
    }

    fn texture_size(&self, texture: isize) -> (c_ulong, c_ulong) {
        let mut f = [0.0f32; 4];
        let w = self.host().op_f(Op::BgTextureSize, texture, 0, 0, 0, &mut f);
        (w as c_ulong, f[0] as c_ulong)
    }

    fn window(&self) -> (c_ulong, c_ulong) {
        (self.op(Op::BgWindow, 0, 0) as c_ulong, self.op(Op::BgWindow, 1, 0) as c_ulong)
    }

    fn exclusive(&self) -> bool {
        self.op(Op::BgExclusive, 0, 0) != 0
    }

    fn lock(&self, on: bool) {
        self.op(Op::BgLock, on as isize, 0);
    }

    /// The texture file's name with its NUL (empty: just the NUL).
    fn name(&self) -> Vec<u8> {
        self.get(|r| {
            if r.mName.is_null() {
                vec![0]
            } else {
                // SAFETY: owned, mNameSize bytes
                unsafe { core::slice::from_raw_parts(r.mName, r.mNameSize) }.to_vec()
            }
        })
    }

    fn load_texture(&self, name: &[u8], description: &EeUiTextureDescription) -> isize {
        self.op(Op::BgTextureLoad, name.as_ptr() as isize, description as *const _ as isize)
    }

    /// The bitmap's stretch from `source` onto `destination` over the texture rectangle, placed at the destination's
    /// top left (then its opacity, for `UIBFull`'s), drawn.
    fn stretch_draw(
        &self,
        bitmap: isize,
        source: UiRect,
        destination: UiRect,
        texture: UiRect,
        opacity: bool,
    ) -> c_long {
        let h = self.host();
        let mut f = [0.0f32; 12];
        f[..4].copy_from_slice(&rect_buf(source));
        f[4..8].copy_from_slice(&rect_buf(destination));
        f[8..].copy_from_slice(&rect_buf(texture));
        h.op_wide(Op::BgBitmapStretch, bitmap, 0, 0, 0, &mut f);
        h.op_f(Op::BgBitmapSetLocation, bitmap, 0, 0, 0, &mut [destination.left, destination.top, 0.0, 0.0]);
        if opacity {
            let mut fade = [0.0f32; 4];
            h.op_f(Op::BgFormFade, 0, 0, 0, 0, &mut fade);
            h.op_f(Op::BgBitmapSetOpacity, bitmap, 0, 0, 0, &mut [fade[0], 0.0, 0.0, 0.0]);
        }
        h.op(Op::BgBitmapDraw, bitmap, 0, 0, 0) as c_long
    }

    /// A grid tile's draw: the first tile's texture's size the texture rectangle.
    fn grid_draw(&self, index: c_ulong, tile: UiRect, clipped: UiRect, opacity: bool) -> c_long {
        let bitmap = self.tile(index);
        let (w, h) = self.texture_size(self.texture_of(self.tile(0)));
        self.stretch_draw(bitmap, tile, clipped, texture_rect(w, h), opacity)
    }

    /// A new tile array of `n` NULLs (the old one freed by the caller).
    fn alloc_tiles(&self, n: usize) {
        let p = Box::into_raw(vec![core::ptr::null_mut::<c_void>(); n].into_boxed_slice()) as *mut *mut c_void;
        self.set(|r| {
            r.mBitmaps = p;
            r.mBitmapCount = n;
        });
    }

    fn free_tiles(&self) {
        let (p, n) = self.get(|r| (r.mBitmaps, r.mBitmapCount));
        if !p.is_null() {
            // SAFETY: made by alloc_tiles with n entries
            drop(unsafe { Box::from_raw(core::ptr::slice_from_raw_parts_mut(p, n)) });
        }
        self.set(|r| {
            r.mBitmaps = core::ptr::null_mut();
            r.mBitmapCount = 0;
        });
    }

    fn grid(&self) -> (c_ulong, c_ulong) {
        self.get(|r| (r.mColumns, r.mRows))
    }

    /// Each tile (row major) of the grid.
    fn each_tile(&self, mut f: impl FnMut(c_ulong)) {
        let mut row = 0;
        while row < self.grid().1 {
            let mut column = 0;
            while column < self.grid().0 {
                f(self.grid().0.wrapping_mul(row).wrapping_add(column));
                column += 1;
            }
            row += 1;
        }
    }

    /// `UIBackground`'s constructor: the layout, priority, form and visibility; added to the form.
    #[allow(clippy::too_many_arguments)]
    pub fn init(
        &self,
        cpp: *mut c_void,
        host: EeUiHostFn,
        id: c_ulong,
        layout: Layout,
        form: *mut c_void,
        priority: c_ulong,
        visible: bool,
    ) {
        self.set(|r| {
            *r = EeUiBackgroundFields {
                mCpp: cpp,
                mHost: host,
                mLayout: layout,
                mRectangle: UiRect::default(),
                mTextureRectangle: UiRect::default(),
                mTextureDescription: EeUiTextureDescription::default(),
                mBackgroundID: id,
                mDrawPriority: priority,
                mForm: form,
                mName: core::ptr::null_mut(),
                mNameSize: 0,
                mBitmap: core::ptr::null_mut(),
                mBitmaps: core::ptr::null_mut(),
                mBitmapCount: 0,
                mColumns: 0,
                mRows: 0,
                mDesignedWidth: 0,
                mDesignedHeight: 0,
                mTiling: 0,
                mHorizontalCount: 0.0,
                mVerticalCount: 0.0,
                mIsVisible: visible,
            }
        });
        self.op(Op::BgAdd, 0, 0);
    }

    /// `UITextureBackground`'s constructor: the texture's rectangle, description and file name.
    pub fn texture(&self, rect: UiRect, description: EeUiTextureDescription, name: &[u8]) {
        self.set_name(name);
        self.set(|r| {
            r.mTextureRectangle = rect;
            r.mTextureDescription = description;
        });
    }

    fn set_name(&self, name: &[u8]) {
        let mut v = name[..name.iter().position(|&b| b == 0).unwrap_or(name.len())].to_vec();
        v.push(0);
        let n = v.len();
        let p = Box::into_raw(v.into_boxed_slice()) as *mut u8;
        self.set(|r| {
            r.mName = p;
            r.mNameSize = n;
        });
    }

    /// `UIFullBackground`'s constructor: a grid of empty tiles, not added to the form.
    #[allow(clippy::too_many_arguments)]
    pub fn full_init(
        &self,
        cpp: *mut c_void,
        host: EeUiHostFn,
        name: &[u8],
        columns: c_ulong,
        rows: c_ulong,
        form: *mut c_void,
        designed: (c_ulong, c_ulong),
    ) {
        self.set(|r| {
            *r = EeUiBackgroundFields {
                mCpp: cpp,
                mHost: host,
                mLayout: Layout::default(),
                mRectangle: UiRect::default(),
                mTextureRectangle: UiRect::default(),
                mTextureDescription: EeUiTextureDescription::default(),
                mBackgroundID: 0,
                mDrawPriority: 0,
                mForm: form,
                mName: core::ptr::null_mut(),
                mNameSize: 0,
                mBitmap: core::ptr::null_mut(),
                mBitmaps: core::ptr::null_mut(),
                mBitmapCount: 0,
                mColumns: columns,
                mRows: rows,
                mDesignedWidth: designed.0,
                mDesignedHeight: designed.1,
                mTiling: 0,
                mHorizontalCount: 0.0,
                mVerticalCount: 0.0,
                mIsVisible: false,
            }
        });
        self.set_name(name);
        self.alloc_tiles(columns.wrapping_mul(rows) as usize);
    }

    /// The record's own memory (the name, the tile array), the C++ base's destructor's.
    pub fn free(&self) {
        let (p, n) = self.get(|r| (r.mName, r.mNameSize));
        if !p.is_null() {
            // SAFETY: made by set_name with n bytes
            drop(unsafe { Box::from_raw(core::ptr::slice_from_raw_parts_mut(p, n)) });
        }
        self.set(|r| {
            r.mName = core::ptr::null_mut();
            r.mNameSize = 0;
        });
        self.free_tiles();
    }

    /// One method ([`EeUiBgCall`]); `f` four floats, twelve for the wide calls.
    ///
    /// # Safety
    /// `f` is as many live floats (or NULL for the calls that take none).
    pub unsafe fn call(&self, call: EeUiBgCall, a: isize, b: isize, f: *mut f32) -> isize {
        use EeUiBgCall as C;
        // SAFETY: the contract
        let rect_in = || buf_rect(&unsafe { read_buf(f) });
        match call {
            C::Close => {
                self.op(Op::BgVirtUnload, 0, 0);
                0
            }
            C::Open => {
                let (w, h) = self.window();
                let screen = background::screen_rect(w, h, self.exclusive());
                let r = self.get(|r| r.mLayout).calculate(&screen, self.exclusive());
                self.set(|x| x.mRectangle = r);
                self.op(Op::BgVirtLoad, 0, 0);
                0
            }
            C::Relayout => {
                let (w, h) = self.window();
                let screen = background::screen_rect(w, h, false);
                let r = self.get(|r| r.mLayout).calculate(&screen, self.exclusive());
                self.set(|x| x.mRectangle = r);
                0
            }
            C::SetRectangle => {
                background::set_rectangle(&mut Mover(*self), rect_in());
                0
            }
            C::SetVisibility => {
                background::set_visibility(&mut Mover(*self), a != 0);
                0
            }
            C::BitmapInit => {
                let bitmap = self.op(Op::BgBitmapNew, 0, 0);
                self.set(|r| r.mBitmap = bitmap as *mut c_void);
                0
            }
            C::BitmapRelease => {
                self.op(Op::BgBitmapRelease, self.get(|r| r.mBitmap as isize), 0);
                0
            }
            C::BitmapDraw => background::bitmap_draw(&mut Virtual(*self), self.rect(), rect_in()) as isize,
            C::BitmapStretched => {
                // SAFETY: twelve floats
                let w = unsafe { core::slice::from_raw_parts(f, 12) };
                let source = buf_rect(&[w[0], w[1], w[2], w[3]]);
                let destination = buf_rect(&[w[4], w[5], w[6], w[7]]);
                let (bitmap, texture) = self.get(|r| (r.mBitmap as isize, r.mTextureRectangle));
                self.stretch_draw(bitmap, source, destination, texture, false) as isize
            }
            C::BitmapLoad => {
                let description = self.get(|r| r.mTextureDescription);
                let name = self.name();
                if name.len() > 1 {
                    let texture = self.load_texture(&name, &description);
                    self.op(Op::BgBitmapSetTexture, self.get(|r| r.mBitmap as isize), texture);
                }
                0
            }
            C::BitmapSetTexture => {
                self.lock(true);
                self.op(Op::BgBitmapSetTexture, self.get(|r| r.mBitmap as isize), a);
                self.lock(false);
                0
            }
            C::BitmapUnload => {
                let bitmap = self.get(|r| r.mBitmap as isize);
                if self.texture_of(bitmap) != 0 {
                    self.op(Op::BgBitmapSetTexture, bitmap, 0);
                }
                0
            }
            C::TiledDraw => {
                let kind = self.get(|r| match r.mTiling {
                    0 => Tiling::Both,
                    1 => Tiling::LeftRight,
                    2 => Tiling::TopBottom,
                    _ => Tiling::Stretched { across: r.mHorizontalCount, down: r.mVerticalCount },
                });
                let (rect, texture) = self.get(|r| (r.mRectangle, r.mTextureRectangle));
                match background::tiled_plan(kind, rect, texture, rect_in()) {
                    None => 0,
                    Some(p) => {
                        let mut w = [0.0f32; 12];
                        w[..4].copy_from_slice(&rect_buf(p.clip));
                        w[4] = p.w;
                        w[5] = p.h;
                        self.host().op_wide(
                            Op::BgVirtTiled,
                            p.first_row as isize,
                            p.last_row as isize,
                            p.first_column as isize,
                            p.last_column as isize,
                            &mut w,
                        )
                    }
                }
            }
            C::DrawTiled => {
                // SAFETY: twelve floats
                let w = unsafe { core::slice::from_raw_parts(f, 12) };
                let clip = buf_rect(&[w[0], w[1], w[2], w[3]]);
                let (c0, c1) = (w[4].to_bits() as c_ulong, w[5].to_bits() as c_ulong);
                background::draw_tiled(
                    &mut Virtual(*self),
                    self.rect(),
                    clip,
                    a as c_ulong,
                    b as c_ulong,
                    c0,
                    c1,
                    w[6],
                    w[7],
                ) as isize
            }
            C::BFullInit => {
                self.set(|r| {
                    r.mBitmaps = core::ptr::null_mut();
                    r.mBitmapCount = 0;
                });
                self.set_grid(a as c_ulong, b as c_ulong);
                0
            }
            C::BFullRelease => {
                self.each_tile(|i| {
                    self.op(Op::BgBitmapRelease, self.tile(i), 0);
                });
                self.free_tiles();
                0
            }
            C::BFullDraw => {
                let (w, h) = self.texture_size(self.texture_of(self.tile(0)));
                let mut form = [0.0f32; 4];
                self.host().op_f(Op::BgFormRect, 0, 0, 0, 0, &mut form);
                let (texture, grid) = self.get(|r| (r.mTextureRectangle, (r.mColumns, r.mRows)));
                background::bfull_draw(&mut Grid(*self, true), buf_rect(&form), rect_in(), (w, h), texture, grid)
                    as isize
            }
            C::BFullLoad => {
                let description = self.get(|r| r.mTextureDescription);
                let name = self.name();
                let mut row = 0;
                while row < self.grid().1 {
                    let mut column = 0;
                    while column < self.grid().0 {
                        let i = self.grid().0.wrapping_mul(row).wrapping_add(column);
                        let tile = background::full_tile_name(&name, row, column);
                        let texture = self.load_texture(&tile, &description);
                        self.op(Op::BgBitmapSetTexture, self.tile(i), texture);
                        column += 1;
                    }
                    row += 1;
                }
                0
            }
            C::BFullSetBitmapTexture => {
                // SAFETY: four floats
                let row = unsafe { *f }.to_bits() as c_ulong;
                self.lock(true);
                let i = self.grid().0.wrapping_mul(row).wrapping_add(b as c_ulong);
                self.op(Op::BgBitmapSetTexture, self.tile(i), a);
                self.lock(false);
                0
            }
            C::BFullSetGrid => {
                self.set_grid(a as c_ulong, b as c_ulong);
                0
            }
            C::BFullUnload => {
                self.each_tile(|i| {
                    let bitmap = self.tile(i);
                    if bitmap != 0 && self.texture_of(bitmap) != 0 {
                        self.op(Op::BgBitmapSetTexture, bitmap, 0);
                    }
                });
                0
            }
            C::FullRelease => {
                self.full_unload();
                self.free_tiles();
                0
            }
            C::FullDraw => {
                let (tw, th) = self.texture_size(self.texture_of(self.tile(0)));
                let window = self.window();
                let (designed, grid) = self.get(|r| ((r.mDesignedWidth, r.mDesignedHeight), (r.mColumns, r.mRows)));
                background::full_draw(&mut Grid(*self, false), rect_in(), (tw, th), window, designed, grid) as isize
            }
            C::FullLoad => {
                let name = self.name();
                let mut row = 0;
                while row < self.grid().1 {
                    let mut column = 0;
                    while column < self.grid().0 {
                        let i = self.grid().0.wrapping_mul(row).wrapping_add(column);
                        let tile = background::full_tile_name(&name, row, column);
                        let texture = self.op(Op::BgTextureLoad, tile.as_ptr() as isize, a);
                        let bitmap = self.op(Op::BgBitmapNew, 0, 0);
                        self.set_tile(i, bitmap);
                        self.op(Op::BgBitmapSetTexture, bitmap, texture);
                        column += 1;
                    }
                    row += 1;
                }
                0
            }
            C::FullUnload => {
                self.full_unload();
                0
            }
        }
    }

    /// `UIBFull::SetNumberColumnsAndRows`: the old tiles released, a new grid of new bitmaps.
    fn set_grid(&self, columns: c_ulong, rows: c_ulong) {
        if !self.get(|r| r.mBitmaps.is_null()) {
            self.each_tile(|i| {
                self.op(Op::BgBitmapRelease, self.tile(i), 0);
            });
            self.free_tiles();
        }
        self.set(|r| {
            r.mColumns = columns;
            r.mRows = rows;
        });
        self.alloc_tiles(columns.wrapping_mul(rows) as usize);
        self.each_tile(|i| {
            let bitmap = self.op(Op::BgBitmapNew, 0, 0);
            self.set_tile(i, bitmap);
        });
    }

    /// `UIFullBackground::UnloadBackground`: every tile released.
    fn full_unload(&self) {
        self.each_tile(|i| {
            let bitmap = self.tile(i);
            if bitmap != 0 {
                self.op(Op::BgBitmapRelease, bitmap, 0);
                self.set_tile(i, 0);
            }
        });
    }
}

/// The moves' view of the record.
struct Mover(Bg);

impl MoveHost for Mover {
    fn visible(&self) -> bool {
        self.0.get(|r| r.mIsVisible)
    }
    fn set_visible(&mut self, v: bool) {
        self.0.set(|r| r.mIsVisible = v);
    }
    fn rect(&self) -> UiRect {
        self.0.rect()
    }
    fn set_rect(&mut self, r: UiRect) {
        self.0.set(|x| x.mRectangle = r);
    }
    fn form_is_current(&self) -> bool {
        self.0.op(Op::BgFormCurrent, 0, 0) != 0
    }
    fn lock(&mut self) {
        self.0.lock(true);
    }
    fn unlock(&mut self) {
        self.0.lock(false);
    }
    fn redraw(&mut self, r: UiRect) {
        self.0.host().op_rect(Op::BgRedraw, 0, 0, r);
    }
    fn dirty(&mut self, r: UiRect) {
        self.0.host().op_rect(Op::BgDirty, 0, 0, r);
    }
}

/// The tiles drawn through the virtual `DrawStrectchedBackground`.
struct Virtual(Bg);

impl TileHost for Virtual {
    fn draw(&mut self, source: UiRect, destination: UiRect) -> c_long {
        let mut f = [0.0f32; 12];
        f[..4].copy_from_slice(&rect_buf(source));
        f[4..8].copy_from_slice(&rect_buf(destination));
        self.0.host().op_wide(Op::BgVirtStretched, 0, 0, 0, 0, &mut f) as c_long
    }
}

/// A grid's tiles (`UIBFull`'s with the form's fade, `UIFullBackground`'s without).
struct Grid(Bg, bool);

impl FullHost for Grid {
    fn draw(&mut self, index: c_ulong, tile: UiRect, clipped: UiRect) -> c_long {
        self.0.grid_draw(index, tile, clipped, self.1)
    }
}

/// `UIBackground`'s constructor over its record (`layout` an `EeUiLayout`).
///
/// # Safety
/// `rec` is the C++ background's base, `layout` a live layout.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_bg_init(
    rec: *mut EeUiBackgroundFields,
    cpp: *mut c_void,
    host: EeUiHostFn,
    id: c_ulong,
    layout: *const Layout,
    form: *mut c_void,
    priority: c_ulong,
    visible: bool,
) {
    // SAFETY: the contract
    unsafe { Bg::new(rec).init(cpp, host, id, *layout, form, priority, visible) }
    crate::direct::put(crate::direct::Kind::Background, cpp, rec as *const c_void);
}

/// `UITextureBackground`'s constructor: the texture's rectangle (four floats), description and NUL-terminated name.
///
/// # Safety
/// `rec` is a record [`ee_ui_bg_init`] set up; the pointers live.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bg_texture(
    rec: *mut EeUiBackgroundFields,
    rect: *const f32,
    description: *const EeUiTextureDescription,
    name: *const c_char,
) {
    // SAFETY: the contract
    unsafe {
        let name = core::ffi::CStr::from_ptr(name).to_bytes();
        Bg::new(rec).texture(buf_rect(&read_buf(rect)), *description, name)
    }
}

/// `UIFullBackground`'s constructor over its record.
///
/// # Safety
/// `rec` is the C++ background's base; `name` NUL-terminated.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_ui_bg_full_init(
    rec: *mut EeUiBackgroundFields,
    cpp: *mut c_void,
    host: EeUiHostFn,
    name: *const c_char,
    columns: c_ulong,
    rows: c_ulong,
    form: *mut c_void,
    designed_width: c_ulong,
    designed_height: c_ulong,
) {
    // SAFETY: the contract
    unsafe {
        let name = core::ffi::CStr::from_ptr(name).to_bytes();
        Bg::new(rec).full_init(cpp, host, name, columns, rows, form, (designed_width, designed_height))
    }
}

/// The record's own memory, freed (the C++ base's destructor, after the classes' own releases).
///
/// # Safety
/// `rec` is a record [`ee_ui_bg_init`] or [`ee_ui_bg_full_init`] set up.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bg_free(rec: *mut EeUiBackgroundFields) {
    crate::direct::forget(crate::direct::Kind::Background, rec as *const c_void);
    // SAFETY: the contract
    unsafe { Bg::new(rec).free() }
}

/// One of the backgrounds' methods ([`EeUiBgCall`]).
///
/// # Safety
/// `rec` is a live record set up by an init; `f` four live floats (twelve for the wide calls) or NULL for the calls
/// that read none.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bg_call(
    rec: *mut EeUiBackgroundFields,
    call: EeUiBgCall,
    a: isize,
    b: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { Bg::new(rec).call(call, a, b, f) }
}

#[cfg(test)]
#[path = "bgobj_tests.rs"]
mod tests;
