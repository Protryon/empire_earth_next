//! The backgrounds' golden (`golden_bgobj.txt`): the first sequences the round-20 driver (`build/ui20`) printed over
//! the background units at the round's base (a world of the form's window, exclusive coordinates, current form,
//! rectangle and fade, bitmaps and textures by handle; the virtuals `LoadBackground`, `UnloadBackground`,
//! `DrawStrectchedBackground` and `DrawTiledBackground` calling the base back), replayed op for op against the Rust
//! record. The kinds: 1 a bitmap background, 2..5 tiled (tiling 0..3), 6 `UIBFull`, 7 `UIFullBackground`. The
//! driver's own call 99 is a digest of the fields; `FullLoad`'s description is an index into [`DESCS`].

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_bgobj.txt");

/// The driver's `this` (the C++ background's address in its world).
const THIS: usize = 0x1000;

const NAMES: [&[u8]; 4] = [b"data\\back.png\0", b"map\0", b"a.b\\cell\0", b"\0"];

const fn desc(v: [u32; 4], f: [bool; 5]) -> EeUiTextureDescription {
    EeUiTextureDescription {
        mSurfaceVolatility: v[0] as c_ulong,
        mFrameFormat: v[1] as c_ulong,
        mPixelDepth: v[2] as c_ulong,
        mNumMipLevelsToDrop: v[3] as c_long,
        mIs3DRenderable: f[0],
        mIsTransparencyEnabled: f[1],
        mHasAlpha: f[2],
        mCreateMIPMaps: f[3],
        mUseTexturePack: f[4],
    }
}

static DESCS: [EeUiTextureDescription; 3] = [
    desc([1, 0, 16, 0], [false, false, true, true, true]),
    desc([0, 2, 32, 1], [true, false, false, true, true]),
    desc([1, 1, 16, 2], [false, true, true, false, false]),
];

struct Fnv(u32);

impl Fnv {
    fn mix(&mut self, v: u64) {
        for i in 0..8 {
            self.0 = (self.0 ^ ((v >> (i * 8)) as u8) as u32).wrapping_mul(16777619);
        }
    }
    fn rect(&mut self, r: UiRect) {
        self.mix(r.bottom.to_bits() as u64 | (r.left.to_bits() as u64) << 32);
        self.mix(r.right.to_bits() as u64 | (r.top.to_bits() as u64) << 32);
    }
}

fn digest(r: &EeUiBackgroundFields) -> isize {
    let mut h = Fnv(2166136261);
    h.rect(r.mRectangle);
    h.rect(r.mTextureRectangle);
    h.mix(r.mIsVisible as u64);
    h.mix(r.mBitmap as u64);
    h.mix(r.mColumns as u64);
    h.mix(r.mRows as u64);
    for i in 0..r.mBitmapCount {
        // SAFETY: the record's array
        h.mix(unsafe { *r.mBitmaps.add(i) } as u64);
    }
    h.mix(r.mBackgroundID as u64);
    h.mix(r.mDrawPriority as u64);
    h.0 as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    let rec = target as *mut EeUiBackgroundFields;
    match call {
        // SAFETY: the test's live record; the digest makes no call out
        99 => digest(unsafe { &*rec }),
        // SAFETY: the call numbers are EeUiBgCall's (the golden's); FullLoad's a indexes the descriptions
        _ => unsafe {
            let a = if call == EeUiBgCall::FullLoad as u32 { &DESCS[a as usize] as *const _ as isize } else { a };
            ee_ui_bg_call(rec, core::mem::transmute::<u32, EeUiBgCall>(call), a, b, f)
        },
    }
}

fn hex(w: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(w, 16).unwrap())
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    // SAFETY: an all-zero record is a valid value (NULL pointers, no host, zeros)
    let rec: *mut EeUiBackgroundFields =
        Box::into_raw(Box::new(unsafe { core::mem::zeroed::<EeUiBackgroundFields>() }));
    r.target.set(rec as *const c_void);
    let mut kind = 0;
    r.run(|_, w| match w[0] {
        "new" => {
            let n = |i: usize| w[i].parse::<c_ulong>().unwrap();
            kind = n(1);
            let (id, priority, visible, desc, name) = (n(2), n(3), n(4) != 0, n(5) as usize, n(6) as usize);
            let (columns, rows, dw, dh) = (n(7), n(8), n(9), n(10));
            let layout = Layout {
                left_offset: hex(w[11]),
                right_offset: hex(w[12]),
                top_offset: hex(w[13]),
                bottom_offset: hex(w[14]),
                left_type: n(15),
                right_type: n(16),
                top_type: n(17),
                bottom_type: n(18),
            };
            let (across, down) = (hex(w[19]), hex(w[20]));
            let texture = [hex(w[21]), hex(w[22]), hex(w[23]), hex(w[24])];
            let name = NAMES[name].as_ptr().cast();
            let host: EeUiHostFn = Some(host_current);
            // SAFETY: the test's record; the pointers live for the calls
            unsafe {
                if kind == 7 {
                    ee_ui_bg_full_init(
                        rec,
                        THIS as *mut c_void,
                        host,
                        name,
                        columns,
                        rows,
                        core::ptr::null_mut(),
                        dw,
                        dh,
                    );
                    return true;
                }
                ee_ui_bg_init(rec, THIS as *mut c_void, host, id, &layout, core::ptr::null_mut(), priority, visible);
                ee_ui_bg_texture(rec, texture.as_ptr(), &DESCS[desc], name);
                if kind <= 5 {
                    ee_ui_bg_call(rec, EeUiBgCall::BitmapInit, 0, 0, core::ptr::null_mut());
                    (*rec).mTiling = kind.wrapping_sub(2);
                    if kind == 5 {
                        (*rec).mVerticalCount = down;
                        (*rec).mHorizontalCount = across;
                    }
                } else {
                    ee_ui_bg_call(rec, EeUiBgCall::BFullInit, columns as isize, rows as isize, core::ptr::null_mut());
                }
            }
            true
        }
        "free" => {
            let call = match kind {
                1..=5 => EeUiBgCall::BitmapRelease,
                6 => EeUiBgCall::BFullRelease,
                _ => EeUiBgCall::FullRelease,
            };
            // SAFETY: the test's record, made by "new"
            unsafe {
                ee_ui_bg_call(rec, call, 0, 0, core::ptr::null_mut());
                ee_ui_bg_free(rec);
            }
            true
        }
        _ => false,
    });
    // SAFETY: made above
    drop(unsafe { Box::from_raw(rec) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 2_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_draw_fails() {
    // the first bitmap draw's answer changed: the call's result no longer matches its ret line
    let at = GOLDEN.find("\nop 560 ").expect("a bitmap draw");
    let eq = at + 1 + GOLDEN[at + 1..].find("\n= ").unwrap() + 1;
    let rest = &GOLDEN[eq + 2..];
    let g = format!("{}= 77{}", &GOLDEN[..eq], &rest[rest.find(' ').unwrap()..]);
    replay(&g);
}

#[test]
#[should_panic(expected = "golden line")]
fn changed_texture_sizes_fail() {
    // every texture 16 pixels wider: the grid tiles' texture rectangle (a stretch's third rectangle) changes
    let g: Vec<String> = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 555 ") {
            Some(rest) => {
                let w: Vec<&str> = rest.split(' ').collect();
                let width = w[1].parse::<u32>().unwrap() + 16;
                format!("set 555 {} {} {}", w[0], width, w[2..].join(" "))
            }
            None => l.to_string(),
        })
        .collect();
    replay(&g.join("\n"));
}

#[test]
fn the_tile_names_are_round_5s() {
    // UIBFull's and UIFullBackground's tile names are full_tile_name's: the suffix before the last dot
    assert_eq!(background::full_tile_name(b"a.b\\cell\0", 0, 2), b"a_1_3.b\\cell\0".to_vec());
}
