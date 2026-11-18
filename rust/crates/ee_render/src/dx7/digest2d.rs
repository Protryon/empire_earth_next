//! The 2D-draw digest (`EE_DRAW2D_DIGEST=<file>`, the HUD 2D lane's check): every 2D draw the rasterizer makes
//! (bitmaps, lines, points, rhombi, texts), with what it sends: the vertices after the draw (the bitmap's UVs as the
//! draw remapped them), the texture's frame, the text's string, place, colours, alignment and clip. Off unless the
//! variable is set: one relaxed load per draw.
//!
//! The file: `C <hash> <command>` the first time a command is seen, then one `F <frame> <count> <hash>` line per
//! shown frame (the hash of the frame's commands in order). Two runs drew the same 2D frames when the sets of `F`
//! hashes agree (menus), and the same commands when the `C` lines do. Hashes are FNV-1a 64 (no HashMap order).

use std::collections::BTreeSet;
use std::fmt::Write as _;
use std::io::Write as _;
use std::sync::atomic::{AtomicU8, Ordering};
use std::sync::Mutex;

use crate::ge::vertarr::Dx7StdColorVertex;

/// 0 unknown, 1 off, 2 on.
static STATE: AtomicU8 = AtomicU8::new(0);

struct Digest {
    file: Option<std::fs::File>,
    seen: BTreeSet<u64>,
    frame: u64,
    count: u32,
    hash: u64,
}

static DIGEST: Mutex<Digest> =
    Mutex::new(Digest { file: None, seen: BTreeSet::new(), frame: 0, count: 0, hash: FNV_OFFSET });

const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

/// FNV-1a 64 of `bytes` continued from `hash`.
pub fn fnv(hash: u64, bytes: &[u8]) -> u64 {
    bytes.iter().fold(hash, |h, &b| (h ^ b as u64).wrapping_mul(FNV_PRIME))
}

/// Whether the digest is on (the variable read once).
pub fn enabled() -> bool {
    match STATE.load(Ordering::Relaxed) {
        2 => true,
        1 => false,
        _ => {
            let path = std::env::var_os("EE_DRAW2D_DIGEST");
            let on = match path {
                Some(p) if !p.is_empty() => {
                    let file = std::fs::File::create(p).ok();
                    let on = file.is_some();
                    if let Ok(mut d) = DIGEST.lock() {
                        d.file = file;
                    }
                    on
                }
                _ => false,
            };
            STATE.store(if on { 2 } else { 1 }, Ordering::Relaxed);
            on
        }
    }
}

/// One command of the current frame.
pub fn command(text: &str) {
    let Ok(mut d) = DIGEST.lock() else { return };
    let h = fnv(FNV_OFFSET, text.as_bytes());
    if d.seen.insert(h) {
        if let Some(f) = d.file.as_mut() {
            let _ = writeln!(f, "C {h:016x} {text}");
        }
    }
    d.count += 1;
    d.hash = fnv(d.hash, &h.to_le_bytes());
}

/// The frame shown: its line, and the next frame starts.
pub fn frame() {
    let Ok(mut d) = DIGEST.lock() else { return };
    let (frame, count, hash) = (d.frame, d.count, d.hash);
    if count > 0 {
        if let Some(f) = d.file.as_mut() {
            let _ = writeln!(f, "F {frame} {count} {hash:016x}");
            let _ = f.flush();
        }
    }
    d.frame += 1;
    d.count = 0;
    d.hash = FNV_OFFSET;
}

/// A float as its bits (exact).
fn fb(out: &mut String, v: f32) {
    let _ = write!(out, " {:08x}", v.to_bits());
}

/// `count` vertices at `v` (x, y, z, colour, u, v).
///
/// # Safety
/// `v` is `count` live vertices.
pub unsafe fn vertices(out: &mut String, v: *const Dx7StdColorVertex, count: usize) {
    for i in 0..count {
        // SAFETY: the caller's contract
        let x = unsafe { *v.add(i) };
        out.push_str(" |");
        for f in [x.x, x.y, x.z] {
            fb(out, f);
        }
        let _ = write!(out, " {:08x}", x.color);
        for f in [x.tu, x.tv] {
            fb(out, f);
        }
    }
}

/// A texture's frame (its UV size and corner, its frame and frame count), or `-` for none.
pub fn texture(out: &mut String, frame: Option<(f32, [f32; 2], isize, isize)>) {
    match frame {
        None => out.push_str(" tex -"),
        Some((size, corner, f, n)) => {
            out.push_str(" tex");
            fb(out, size);
            fb(out, corner[0]);
            fb(out, corner[1]);
            let _ = write!(out, " {f} {n}");
        }
    }
}

/// Floats as bits.
pub fn floats(out: &mut String, values: &[f32]) {
    for &v in values {
        fb(out, v);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fnv_is_fnv1a_64() {
        assert_eq!(fnv(FNV_OFFSET, b""), 0xcbf2_9ce4_8422_2325);
        assert_eq!(fnv(FNV_OFFSET, b"a"), 0xaf63_dc4c_8601_ec8c);
    }

    #[test]
    fn vertices_and_texture_text() {
        let v = [Dx7StdColorVertex {
            x: 1.0,
            y: 2.0,
            z: 0.0,
            nx: 0.0,
            ny: 0.0,
            nz: 0.0,
            color: 0xff00ff00,
            tu: 0.5,
            tv: 1.0,
        }];
        let mut s = String::from("bmp");
        unsafe { vertices(&mut s, v.as_ptr(), 1) };
        texture(&mut s, None);
        assert_eq!(s, "bmp | 3f800000 40000000 00000000 ff00ff00 3f000000 3f800000 tex -");
        let mut t = String::new();
        texture(&mut t, Some((1.0, [0.0, 0.5], 2, 4)));
        assert_eq!(t, " tex 3f800000 00000000 3f000000 2 4");
    }
}
