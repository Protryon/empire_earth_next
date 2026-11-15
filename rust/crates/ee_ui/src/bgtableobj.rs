//! `UIBackgroundTable` whole in Rust and `UIManager::CreateBackgrounds` (R6 ui round 20): [`EeUiBgTableFields`] is
//! the C++ class's private base (the field names the members'; the inline getters read them in place, the texture
//! file stays a C++ `UString` the getter answers); the static array is a Rust list of the entries' records; `Create`
//! is round 7's loop (`background_table.rs`, pinned by its golden) over this host, `DeleteAll` and
//! `CreateBackgrounds` run here.
//!
//! Every call out is an [`EeUiOp`] (590..595) on the host `UIBackgroundTable.cpp` passes (`UIBackgroundTableHost::Op`,
//! its object the manager for `CreateBackgrounds`): an entry's `new`, its file name and `delete`, the manager's form
//! by id and texture depth (pure), and the background's construction by type.

use core::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

use crate::background_table::{self, BackgroundTablesHost, DbBackground};
use crate::bgobj::EeUiTextureDescription;
use crate::button::UiColor;
use crate::layout::Layout;
use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};
use crate::rect::UiRect;

/// `UIBackgroundTable`'s members (`include/lle/UIBackgroundTable.h`), the C++ class's base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug)]
pub struct EeUiBgTableFields {
    pub mLayout: Layout,
    pub mTextureRect: UiRect,
    pub mColor: UiColor,
    pub mTileWidthPercent: f32,
    pub mTileHeightPercent: f32,
    pub mOpacity: f32,
    pub mID: c_long,
    pub mBackgroundID: c_ulong,
    pub mFormID: c_ulong,
    pub mBackgroundType: c_ulong,
    pub mNumCols: c_long,
    pub mNumRows: c_long,
    pub mDrawPriority: c_ulong,
    pub mVisible: bool,
    pub mHasAlpha: bool,
    pub mAttribute1: bool,
    pub mAttribute2: bool,
    pub mAttribute3: bool,
}

/// `kBackgroundBitmap`: the type an empty entry has.
const BITMAP: c_ulong = 0;

/// The constructor's entry: ids -1, a hidden opaque bitmap background of priority 1 (layout, rectangles and colour
/// are the C++ members' own constructors': the layout filling its parent, a zero rectangle, white).
pub fn empty() -> EeUiBgTableFields {
    EeUiBgTableFields {
        mLayout: Layout::default(),
        mTextureRect: UiRect::default(),
        mColor: UiColor { red: 1.0, green: 1.0, blue: 1.0 },
        mTileWidthPercent: 1.0,
        mTileHeightPercent: 1.0,
        mOpacity: 1.0,
        mID: -1,
        mBackgroundID: c_ulong::MAX,
        mFormID: c_ulong::MAX,
        mBackgroundType: BITMAP,
        mNumCols: 0,
        mNumRows: 0,
        mDrawPriority: 1,
        mVisible: false,
        mHasAlpha: false,
        mAttribute1: false,
        mAttribute2: false,
        mAttribute3: false,
    }
}

/// The static array (`sUIBackgroundArray`): each entry's record, 0 for an empty slot.
static TABLE: Mutex<Vec<usize>> = Mutex::new(Vec::new());

fn table() -> std::sync::MutexGuard<'static, Vec<usize>> {
    TABLE.lock().unwrap_or_else(|e| e.into_inner())
}

/// `Create`'s file and this list: the host's object is the file (`FSFileSpec*`).
struct Tables(Host);

impl BackgroundTablesHost for Tables {
    fn open(&mut self) {
        self.0.op(Op::BgtFileOpen, 0, 0, 0, 0);
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        let mut f = [0.0f32; 4];
        let r = self.0.op_f(Op::BgtFileCount, 0, 0, 0, 0, &mut f);
        *count = f[0].to_bits() as c_ulong;
        r as c_long
    }
    fn resize(&mut self, count: c_ulong) {
        table().resize(count as usize, 0);
    }
    fn read_record(&mut self, record: &mut DbBackground) -> c_long {
        let n = core::mem::size_of::<DbBackground>() as isize;
        self.0.op(Op::BgtFileRead, 0, (record as *mut DbBackground) as isize, n, 0) as c_long
    }
    fn set(&mut self, index: c_ulong, record: Option<&DbBackground>) {
        let e = record.map_or(0, |r| make(self.0, r));
        table()[index as usize] = e;
    }
    fn close(&mut self) {
        self.0.op(Op::BgtFileClose, 0, 0, 0, 0);
    }
}

/// A new entry (`new UIBackgroundTable`, `BgtNew`) made from a record: its members, its texture file name.
pub fn make(host: Host, r: &DbBackground) -> usize {
    let t = background_table::from_record(r);
    let f = t.fields;
    let e = EeUiBgTableFields {
        mLayout: t.layout,
        mTextureRect: t.texture_rect,
        mColor: t.color,
        mTileWidthPercent: f.tile_width_percent,
        mTileHeightPercent: f.tile_height_percent,
        mOpacity: f.opacity,
        mID: f.id,
        mBackgroundID: f.background_id,
        mFormID: f.form_id,
        mBackgroundType: f.background_type,
        mNumCols: f.num_cols,
        mNumRows: f.num_rows,
        mDrawPriority: f.draw_priority,
        mVisible: f.visible,
        mHasAlpha: f.has_alpha,
        mAttribute1: f.attribute1,
        mAttribute2: f.attribute2,
        mAttribute3: f.attribute3,
    };
    let name = &r.texture_file;
    let n = name.iter().position(|&b| b == 0).unwrap_or(name.len());
    let mut bytes = name[..n].to_vec();
    bytes.push(0);
    host.op(Op::BgtNew, &e as *const _ as isize, bytes.as_ptr() as isize, 0, 0) as usize
}

/// The goldens' trace of an entry's record: FNV-1a over its fields, each widened to 64 bits.
pub fn entry_digest(r: &EeUiBgTableFields) -> isize {
    let mut h: u32 = 2166136261;
    let mut mix = |v: u64| {
        for i in 0..8 {
            h = (h ^ ((v >> (i * 8)) as u8) as u32).wrapping_mul(16777619);
        }
    };
    let l = r.mLayout;
    for v in [l.left_offset, l.right_offset, l.top_offset, l.bottom_offset] {
        mix(v.to_bits() as u64);
    }
    for v in [l.left_type, l.right_type, l.top_type, l.bottom_type] {
        mix(v as u64);
    }
    let t = r.mTextureRect;
    for v in [t.bottom, t.left, t.right, t.top, r.mColor.red, r.mColor.green, r.mColor.blue] {
        mix(v.to_bits() as u64);
    }
    for v in [r.mTileWidthPercent, r.mTileHeightPercent, r.mOpacity] {
        mix(v.to_bits() as u64);
    }
    for v in [r.mID, r.mNumCols, r.mNumRows] {
        mix(v as i64 as u64);
    }
    for v in [r.mBackgroundID, r.mFormID, r.mBackgroundType, r.mDrawPriority] {
        mix(v as u64);
    }
    for v in [r.mVisible, r.mHasAlpha, r.mAttribute1, r.mAttribute2, r.mAttribute3] {
        mix(v as u64);
    }
    h as isize
}

/// `UIManager::CreateBackgrounds`: each entry's background made on its form (by `BgtMake`, the type's class or the
/// manager's user-defined one), entries without a form skipped; the textures 16 bits deep until the manager's depth,
/// with alpha as the entry says.
pub fn create_backgrounds(host: Host) {
    let mut description = EeUiTextureDescription {
        mSurfaceVolatility: 1,
        mFrameFormat: 0,
        mPixelDepth: 16,
        mNumMipLevelsToDrop: 0,
        mIs3DRenderable: false,
        mIsTransparencyEnabled: false,
        mHasAlpha: true,
        mCreateMIPMaps: true,
        mUseTexturePack: true,
    };
    let count = table().len();
    description.mPixelDepth = host.op(Op::BgtTextureDepth, 0, 0, 0, 0) as c_ulong;
    for i in 0..count {
        let entry = table()[i] as isize;
        if entry == 0 {
            continue;
        }
        description.mHasAlpha = host.op(Op::BgtEntryAlpha, entry, 0, 0, 0) != 0;
        let form = host.op(Op::BgtForm, host.op(Op::BgtEntryFormId, entry, 0, 0, 0), 0, 0, 0);
        if form == 0 {
            continue; // no such form (GetFormByID used to return a garbage pointer here)
        }
        host.op(Op::BgtMake, entry, form, &description as *const _ as isize, 0);
    }
}

/// `UIBackgroundTable::Create(*file)`; `host` reads the file (its object) and makes the entries.
///
/// # Safety
/// `file` is a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bgtable_create(file: *mut c_void, host: EeUiHostFn) -> c_long {
    background_table::create(&mut Tables(Host { obj: file, f: host }))
}

/// `UIBackgroundTable`'s constructor over its record.
///
/// # Safety
/// `rec` is the C++ entry's base.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bgtable_init(rec: *mut EeUiBgTableFields) {
    // SAFETY: the contract
    unsafe { rec.write(empty()) }
}

/// `UIBackgroundTable::DeleteAll()`: each entry deleted (`BgtDelete`) in order, the array emptied.
#[no_mangle]
pub extern "C" fn ee_ui_bgtable_delete_all(host: EeUiHostFn) {
    let all = core::mem::take(&mut *table());
    let h = Host { obj: core::ptr::null_mut(), f: host };
    for e in all {
        if e != 0 {
            h.op(Op::BgtDelete, e as isize, 0, 0, 0);
        }
    }
}

/// `UIBackgroundTable::GetArraySize()`.
#[no_mangle]
pub extern "C" fn ee_ui_bgtable_count() -> c_ulong {
    table().len() as c_ulong
}

/// `UIBackgroundTable::GetUIBackgroundTable(i)`: its record (NULL for an empty slot).
#[no_mangle]
pub extern "C" fn ee_ui_bgtable_at(i: c_ulong) -> *mut EeUiBgTableFields {
    table()[i as usize] as *mut EeUiBgTableFields
}

/// `UIManager::CreateBackgrounds()` (`manager` the host's object).
#[no_mangle]
pub extern "C" fn ee_ui_bgtable_create_backgrounds(manager: *mut c_void, host: EeUiHostFn) {
    create_backgrounds(Host { obj: manager, f: host })
}

#[cfg(test)]
#[path = "bgtableobj_tests.rs"]
mod tests;
