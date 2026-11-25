//! imgobj.rs over a stand-in host: handles, surfaces, display contexts and Graphics as counted objects; the
//! Graphics' calls logged.

use super::*;
use crate::guidraw::EE_WON_DRAW_IMAGE;
use crate::surface::{ee_won_surface_create, ee_won_surface_free, ee_won_surface_new};
use std::cell::RefCell;

enum What {
    Img(*mut EeWonImage),
    Surface(*mut EeWonSurface),
    Ctx,
    Gfx { ctx: isize, tx: i32, ty: i32 },
}

#[repr(C)]
struct Fake {
    _head: [usize; 2],
    /// at EE_WON_IMAGE_RUST_AT and EE_WON_CONTEXT_AT: a handle's image, a context's members
    rec: EeWonDisplayContextRec,
    rc: Cell<i32>,
    what: RefCell<What>,
}

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static LIVE: Cell<i32> = const { Cell::new(0) };
    static DECODES: Cell<i32> = const { Cell::new(0) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}
fn take_log() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}
fn live() -> i32 {
    LIVE.with(Cell::get)
}

fn fake(w: What, rc: i32) -> isize {
    fake_rec(w, rc, EeWonDisplayContextRec { surface: std::ptr::null_mut(), graphics: 0, pixel_scale: 0.0 })
}
fn fake_rec(w: What, rc: i32, rec: EeWonDisplayContextRec) -> isize {
    LIVE.with(|l| l.set(l.get() + 1));
    Box::into_raw(Box::new(Fake { _head: [0; 2], rec, rc: Cell::new(rc), what: RefCell::new(w) })) as isize
}
fn f<'a>(p: isize) -> &'a Fake {
    // SAFETY: a live fake
    unsafe { &*(p as *const Fake) }
}

extern "C" fn host(op: EeWonImageHost, a: isize, b: isize, c: isize, d: isize) -> isize {
    use EeWonImageHost as H;
    match op {
        H::Ref => {
            f(a).rc.set(f(a).rc.get() + 1);
            0
        }
        H::Unref => {
            let o = f(a);
            o.rc.set(o.rc.get() - 1);
            if o.rc.get() <= 0 {
                // SAFETY: the last reference
                let b = unsafe { Box::from_raw(a as *mut Fake) };
                LIVE.with(|l| l.set(l.get() - 1));
                match b.what.into_inner() {
                    // SAFETY: the handle owned it
                    What::Img(i) => unsafe { ee_won_image_free(i) },
                    // SAFETY: owned
                    What::Surface(s) => unsafe { ee_won_surface_free(s) },
                    _ => {}
                }
            }
            0
        }
        H::Wrap => {
            let rec = EeWonDisplayContextRec { surface: a as *mut EeWonSurface, graphics: 0, pixel_scale: 0.0 };
            let h = fake_rec(What::Img(a as *mut EeWonImage), 0, rec);
            // SAFETY: a live image
            unsafe { ee_won_image_bind(a as *const EeWonImage, h) };
            h
        }
        H::GfxTrans => {
            if let What::Gfx { tx, ty, .. } = &*f(a).what.borrow() {
                // SAFETY: two ints
                unsafe { *(b as *mut [i32; 2]) = [*tx, *ty] };
            }
            0
        }
        H::GfxContext => match &*f(a).what.borrow() {
            What::Gfx { ctx, .. } => *ctx,
            _ => 0,
        },
        H::GfxRun => {
            // SAFETY: c operations, four slots
            let (ops, slots) = unsafe {
                (std::slice::from_raw_parts(b as *const EeWonDrawOp, c as usize), &*(d as *const [isize; 4]))
            };
            for o in ops {
                if o.kind == EE_WON_DRAW_TRANSLATE {
                    if let What::Gfx { tx, ty, .. } = &mut *f(a).what.borrow_mut() {
                        *tx += o.a;
                        *ty += o.b;
                    }
                }
                let slot = if o.kind == EE_WON_DRAW_IMAGE { slots[o.a as usize] } else { 0 };
                log(format!("op {} {} {} {} {} slot {}", o.kind, o.a, o.b, o.c, o.d, slot != 0));
            }
            0
        }
        H::GfxDrawImage => {
            log("DrawImage".into());
            0
        }
        H::NewContext => {
            // SAFETY: the caller's float
            let rec = EeWonDisplayContextRec {
                surface: a as *mut EeWonSurface,
                graphics: 0,
                pixel_scale: unsafe { *(b as *const f32) },
            };
            fake_rec(What::Ctx, 1, rec)
        }
        H::NewGraphics => fake(What::Gfx { ctx: a, tx: 0, ty: 0 }, 1),
        H::FreeGraphics => host(H::Unref, a, 0, 0, 0),
        H::CreateImage => {
            let h = host(H::Wrap, ee_won_image_new_native(host, b as i32, c as i32, 1.0) as isize, 0, 0, 0);
            host(H::Ref, h, 0, 0, 0);
            h
        }
        H::Decode => {
            DECODES.with(|d| d.set(d.get() + 1));
            0
        }
        H::PackShift => 0,
    }
}

/// A new image's handle with one reference.
fn own(o: *mut EeWonImage) -> isize {
    let h = host(EeWonImageHost::Wrap, o as isize, 0, 0, 0);
    host(EeWonImageHost::Ref, h, 0, 0, 0);
    h
}
fn rust(h: isize) -> *const EeWonImage {
    f(h).rec.surface as *const EeWonImage
}
fn release(h: isize) {
    if h != 0 {
        host(EeWonImageHost::Unref, h, 0, 0, 0);
    }
}
fn get(h: isize, field: EeWonImageField) -> isize {
    // SAFETY: a live handle
    unsafe { ee_won_image_get(rust(h), field) }
}
fn call(h: isize, c: EeWonImageCall, a: isize, b: isize, cc: isize, d: isize) -> isize {
    // SAFETY: a live handle
    unsafe { ee_won_image_call(rust(h), c, a, b, cc, d) }
}
fn pixels32<'a>(h: isize) -> &'a mut [u32] {
    let n = (get(h, EeWonImageField::Width) * get(h, EeWonImageField::Height)) as usize;
    // SAFETY: a RawImage32's pixels
    unsafe { std::slice::from_raw_parts_mut(get(h, EeWonImageField::Pixels) as *mut u32, n) }
}

#[test]
fn raw32_duplicate_copies_pixels_and_transparency() {
    let a = own(ee_won_image_new_raw32(host, 3, 2));
    for (i, p) in pixels32(a).iter_mut().enumerate() {
        *p = 0x0100_0000 * i as u32 + 7;
    }
    call(a, EeWonImageCall::SetTransparency, 1, 0, 0, 0);
    let b = call(a, EeWonImageCall::Duplicate, 0, 0, 0, 0);
    assert_eq!(get(b, EeWonImageField::Kind), EeWonImageKind::Raw32 as isize);
    assert_eq!(get(b, EeWonImageField::RawType), 2);
    assert_eq!(get(b, EeWonImageField::HasTransparency), 1);
    assert_eq!(pixels32(a), pixels32(b));
    // SAFETY: a live image
    assert_eq!(unsafe { (*rust(b)).pixel(2, 1) }, 0x0500_0007);
    release(a);
    release(b);
    assert_eq!(live(), 0);
}

#[test]
fn raw8_filters_keep_the_palette_rules() {
    let pal = Palette::new(3);
    pal.colors[0].set(0x00ff_0000);
    pal.colors[1].set(0x0000_ff00);
    pal.colors[2].set(0x0000_00ff);
    let a = own(Box::into_raw(EeWonImage::new_raw8(host, 5, 3, Some(pal.clone()))));
    // the rows are padded to 8 bytes
    // SAFETY: a live image
    if let Kind::Raw8 { bytes_per_row, px, .. } = &unsafe { &*rust(a) }.kind {
        assert_eq!(*bytes_per_row, 8);
        assert_eq!(px.len(), 24);
        px[8 + 1].set(2);
    }
    // the transparent colour's index
    let t = call(a, EeWonImageCall::Transparent, 0x0000_ff00, 0, 0, 0);
    assert_eq!(t, a);
    release(t);
    assert_eq!(get(a, EeWonImageField::HasTransparency), 1);
    // a crop keeps the palette and transparent index
    let c = call(a, EeWonImageCall::Crop, 1, 1, 2, 2);
    // SAFETY: live images
    unsafe {
        assert_eq!((*rust(c)).pixel(0, 0), 2);
        assert!((*rust(c)).has_transparency());
    }
    // the tint works on a copy of the palette
    let t = call(a, EeWonImageCall::Tint, 50, 50, 50, 0);
    release(t);
    assert_eq!(pal.colors[0].get(), 0x00ff_0000);
    // SAFETY: a live image
    if let Kind::Raw8 { palette, .. } = &unsafe { &*rust(a) }.kind {
        assert!(!Rc::ptr_eq(palette.borrow().as_ref().unwrap(), &pal));
    }
    // a program image has no crop
    let one = own(ee_won_image_new_ms(host, 0, 0, 1, 1));
    assert_eq!(call(one, EeWonImageCall::Crop, 0, 0, 1, 1), 0);
    // the scale to the same size is the image itself
    let s = call(a, EeWonImageCall::ScaleTo, 5, 3, 0, 0);
    assert_eq!(s, a);
    let s2 = call(a, EeWonImageCall::ScalePercent, 200, 100, 0, 0);
    assert_eq!(get(s2, EeWonImageField::Width), 10);
    assert_eq!(get(s2, EeWonImageField::Height), 3);
    for h in [s, s2, c, one, a] {
        release(h);
    }
    assert_eq!(live(), 0);
}

#[test]
fn native_images_scale_mask_and_copy() {
    let n = own(ee_won_image_new_native(host, 3, 2, 2.0));
    assert_eq!(get(n, EeWonImageField::PixelWidth), 6);
    assert_eq!(get(n, EeWonImageField::PixelHeight), 4);
    assert_eq!(f32::from_bits(get(n, EeWonImageField::Scale) as u32), 2.0);
    // a scale under 1 is 1
    let m = own(ee_won_image_new_native(host, 3, 2, 0.5));
    assert_eq!(get(m, EeWonImageField::PixelWidth), 3);
    // a transparent raw image's native image has a mask; its raw copy is transparent
    let r = own(ee_won_image_new_raw32(host, 2, 2));
    pixels32(r).copy_from_slice(&[0xff00_0001, 0x0000_0002, 0xff00_0003, 0xff00_0004]);
    call(r, EeWonImageCall::SetTransparency, 1, 0, 0, 0);
    let nat = call(r, EeWonImageCall::Native, 0, 0, 0, 0);
    // SAFETY: a live image
    assert!(unsafe { (*rust(nat)).native_mask() }.is_some());
    // the native image's native image is itself
    let same = call(nat, EeWonImageCall::Native, 0, 0, 0, 0);
    assert_eq!(same, nat);
    release(same);
    let raw = call(nat, EeWonImageCall::Raw, 0, 0, 0, 0);
    assert_eq!(get(raw, EeWonImageField::HasTransparency), 1);
    assert_eq!(pixels32(raw)[0] & 0x00ff_ffff, 1);
    // the duplicate shares the mask
    let d = call(nat, EeWonImageCall::Duplicate, 0, 0, 0, 0);
    // SAFETY: live images
    unsafe { assert!(Rc::ptr_eq(&(*rust(d)).native_mask().unwrap(), &(*rust(nat)).native_mask().unwrap())) };
    // a Graphics made once, freed with the image
    let g = call(d, EeWonImageCall::Graphics, 0, 0, 0, 0);
    assert_eq!(call(d, EeWonImageCall::Graphics, 0, 0, 0, 0), g);
    // the native draws onto another's context
    let ctx = get(n, EeWonImageField::Context);
    // SAFETY: a live image
    unsafe { ee_won_image_draw(rust(nat), ctx, 1, 0, 0, 0, 2, 2) };
    for h in [n, m, r, nat, raw, d] {
        release(h);
    }
    assert_eq!(live(), 0);
}

#[test]
fn program_images_draw_through_the_graphics() {
    let s = ee_won_surface_new(0);
    // SAFETY: a new surface
    unsafe { ee_won_surface_create(s, 4, 4) };
    let surface = fake(What::Surface(s), 1);
    let ctx = host(EeWonImageHost::NewContext, s as isize, &1.0f32 as *const f32 as isize, 0, 0);
    let g = host(EeWonImageHost::NewGraphics, ctx, 0, 0, 0);
    // SAFETY: the live context fake
    unsafe { (*(ctx as *mut Fake)).rec.graphics = g };
    let ms = own(ee_won_image_new_ms(host, 7, 0, 10, 8));
    take_log();
    // SAFETY: a live image
    unsafe { ee_won_image_draw(rust(ms), ctx, 3, 4, 0, 0, 10, 8) };
    let l = take_log();
    assert_eq!(l.first().unwrap(), "op 11 3 4 0 0 slot false");
    assert_eq!(l.last().unwrap(), "op 11 -3 -4 0 0 slot false");
    // the push button's face between them
    assert!(l.len() > 2, "{l:?}");
    // a part of it: clipped
    // SAFETY: a live image
    unsafe { ee_won_image_draw(rust(ms), ctx, 0, 0, 1, 1, 2, 2) };
    let l = take_log();
    assert_eq!(l[1], "op 9 1 1 2 2 slot false");
    assert_eq!(l[l.len() - 2], "op 10 0 0 0 0 slot false");
    // the duplicate's size and type
    let d = call(ms, EeWonImageCall::Duplicate, 0, 0, 0, 0);
    assert_eq!((get(d, EeWonImageField::Width), get(d, EeWonImageField::Height)), (10, 8));
    // a stretch image builds its cache at its first paint, a size change marks it again
    let raw = own(ee_won_image_new_raw32(host, 4, 4));
    let st = own(ee_won_image_new_stretch(host, raw, 1, 1, 1, 1));
    call(st, EeWonImageCall::SetSize, 6, 5, 0, 0);
    // SAFETY: a live image
    unsafe { ee_won_image_draw(rust(st), ctx, 0, 0, 0, 0, 6, 5) };
    let l = take_log();
    assert!(l.iter().any(|s| s.starts_with("op 7 0 ") && s.ends_with("slot true")), "{l:?}");
    // SAFETY: a live image
    let cache = if let Kind::Stretch { cache, s, .. } = &unsafe { &*rust(st) }.kind {
        assert!(!s.get().need_recalc);
        cache.get()
    } else {
        0
    };
    assert_eq!((get(cache, EeWonImageField::Width), get(cache, EeWonImageField::Height)), (6, 5));
    // the copy's size is 0 x 0
    let sd = call(st, EeWonImageCall::Duplicate, 0, 0, 0, 0);
    assert_eq!(get(sd, EeWonImageField::Width), 0);
    assert_eq!(get(sd, EeWonImageField::Left), 1);
    // CopyAttributes: the settings and the size
    call(sd, EeWonImageCall::StretchCopy, st, 0, 0, 0);
    assert_eq!(get(sd, EeWonImageField::Width), 6);
    // a stretch-centre image sizes its stretch image
    let sc = own(ee_won_image_new_stretch_center(host, sd, ms));
    call(sc, EeWonImageCall::SetSize, 9, 9, 0, 0);
    assert_eq!(get(sd, EeWonImageField::Width), 9);
    // its native image: a new one with it drawn into it
    let nat = call(sc, EeWonImageCall::Native, ctx, 0, 0, 0);
    assert_eq!(take_log(), vec!["DrawImage".to_string()]);
    for h in [ms, d, raw, st, sd, sc, nat, g, ctx, surface] {
        release(h);
    }
    assert_eq!(live(), 0);
}

#[test]
fn delay_images_decode_once() {
    DECODES.with(|d| d.set(0));
    let p = b"a.png";
    // SAFETY: the path's bytes
    let i = own(unsafe { ee_won_image_new_delay(host, p.as_ptr(), p.len(), 4, 5) });
    assert_eq!(get(i, EeWonImageField::Width), 4);
    assert_eq!(call(i, EeWonImageCall::Native, 0, 0, 0, 0), 0);
    // SAFETY: a live image
    unsafe { ee_won_image_draw(rust(i), 0, 0, 0, 0, 0, 4, 5) };
    assert_eq!(DECODES.with(Cell::get), 1);
    assert_eq!(call(i, EeWonImageCall::Duplicate, 0, 0, 0, 0), 0);
    release(i);
    assert_eq!(live(), 0);
}

#[test]
fn decoders_by_extension_without_case() {
    let mut d = EeWonImgDecoder::new(host);
    d.add(EeWonImgKind::Jpg, b"jpg");
    d.add(EeWonImgKind::Jpg, b"JPE");
    d.add(EeWonImgKind::Png, b"png");
    d.add(EeWonImgKind::Jpg, b"PNG");
    assert_eq!(d.map.len(), 3);
    // the first spelling of a key stays, its decoder replaced
    assert!(d.map.iter().any(|(k, v)| k == b"png" && *v == EeWonImgKind::Jpg));
    d.set_file_name(b"dir\\a.b\\Pic.Jpe");
    assert_eq!(d.cur, b"Jpe");
    assert_eq!(d.find(), Some(EeWonImgKind::Jpg));
    d.set_file_name(b"x.gif");
    assert_eq!(d.find(), None);
    // a missing file: no image, no attributes
    assert!(matches!(d.decode_raw(c"no such file.png"), Ok(None)));
    assert_eq!(d.attributes(c"no such file.png"), None);
}

#[test]
fn decoded_png_is_a_raw32() {
    // a 1 x 1 RGBA PNG in a stored zlib block
    fn crc(b: &[u8]) -> u32 {
        let mut c = !0u32;
        for &v in b {
            c ^= v as u32;
            for _ in 0..8 {
                c = if c & 1 != 0 { 0xedb8_8320 ^ (c >> 1) } else { c >> 1 };
            }
        }
        !c
    }
    fn chunk(o: &mut Vec<u8>, kind: &[u8], d: &[u8]) {
        o.extend_from_slice(&(d.len() as u32).to_be_bytes());
        let mut k = kind.to_vec();
        k.extend_from_slice(d);
        o.extend_from_slice(&k);
        o.extend_from_slice(&crc(&k).to_be_bytes());
    }
    let mut png = vec![0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a];
    chunk(&mut png, b"IHDR", &[0, 0, 0, 1, 0, 0, 0, 1, 8, 6, 0, 0, 0]);
    let raw = [0u8, 0x10, 0x20, 0x30, 0x80];
    let mut z = vec![0x78, 0x01, 1, 5, 0, 0xfa, 0xff];
    z.extend_from_slice(&raw);
    let (mut a, mut b) = (1u32, 0u32);
    for &v in &raw {
        a = (a + v as u32) % 65521;
        b = (b + a) % 65521;
    }
    z.extend_from_slice(&((b << 16) | a).to_be_bytes());
    chunk(&mut png, b"IDAT", &z);
    chunk(&mut png, b"IEND", &[]);
    let dir = std::env::temp_dir().join(format!("ee_imgobj_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("one.PNG");
    std::fs::write(&path, &png).unwrap();
    let cpath = std::ffi::CString::new(path.to_str().unwrap()).unwrap();
    let mut d = EeWonImgDecoder::new(host);
    d.add(EeWonImgKind::Png, b"png");
    assert_eq!(d.attributes(&cpath), Some((1, 1)));
    let h = d.decode_raw(&cpath).unwrap().unwrap().into_raw();
    assert_eq!(get(h, EeWonImageField::Kind), EeWonImageKind::Raw32 as isize);
    assert_eq!(get(h, EeWonImageField::HasTransparency), 1);
    assert_eq!(pixels32(h)[0], 0x8010_2030);
    // the decode: its native image (no pack without a shift)
    let n = d.decode(0, &cpath).unwrap().unwrap().into_raw();
    assert_eq!(get(n, EeWonImageField::Kind), EeWonImageKind::Native as isize);
    release(h);
    release(n);
    std::fs::remove_dir_all(&dir).ok();
    assert_eq!(live(), 0);
}
