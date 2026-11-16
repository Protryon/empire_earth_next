//! `find.rs`'s tests: the lookups over a recording host (a path of in-memory files, faults per file) in the original's
//! order: what is looked up, opened, read, closed and deleted, and what stops the game.

use super::*;
use std::cell::RefCell;
use std::collections::BTreeMap;

#[derive(Default)]
struct World {
    files: BTreeMap<String, Vec<u8>>,
    /// a file's fault: 1 the open fails, 2 the length fails, 3 a short read
    faults: BTreeMap<String, u8>,
    open: Vec<String>,
    log: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(&mut w.borrow_mut()))
}

const PATH: usize = 0x10;

unsafe extern "C" fn host(op: EeOfFileOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    // SAFETY: the tests' pointers: a C string for a name, the files' own boxed names
    unsafe {
        let name = |o: *mut c_void| (*(o as *const String)).clone();
        match op {
            EeOfFileOp::PathFind => {
                assert_eq!(obj as usize, PATH);
                let wanted = CStr::from_ptr(a as *const c_char).to_str().unwrap().to_string();
                let found = with(|w| w.files.contains_key(&wanted));
                with(|w| w.log.push(format!("find {wanted} {found}")));
                if found {
                    Box::into_raw(Box::new(wanted)) as isize
                } else {
                    0
                }
            }
            EeOfFileOp::FileOpen => {
                assert_eq!(a, K_FILE_ACCESS_READ);
                let n = name(obj);
                with(|w| w.log.push(format!("open {n}")));
                if with(|w| w.faults.get(&n) == Some(&1)) {
                    return 5;
                }
                with(|w| w.open.push(n));
                0
            }
            EeOfFileOp::FileLength => {
                let n = name(obj);
                if with(|w| w.faults.get(&n) == Some(&2)) {
                    return 4;
                }
                *(a as *mut c_ulong) = with(|w| w.files[&n].len()) as c_ulong;
                0
            }
            EeOfFileOp::FileRead => {
                let n = name(obj);
                let data = with(|w| w.files[&n].clone());
                let count = if with(|w| w.faults.get(&n) == Some(&3)) { data.len() / 2 } else { data.len() };
                std::ptr::copy_nonoverlapping(data.as_ptr(), a as *mut u8, (b as usize).min(count));
                count as isize
            }
            EeOfFileOp::FileClose => {
                let n = name(obj);
                with(|w| {
                    w.open.retain(|o| *o != n);
                    w.log.push(format!("close {n}"));
                });
                0
            }
            EeOfFileOp::FileDelete => {
                drop(Box::from_raw(obj as *mut String));
                0
            }
            EeOfFileOp::Verify => {
                let message = CStr::from_ptr(a as *const c_char).to_str().unwrap().to_string();
                with(|w| w.log.push(format!("verify {message}")));
                0
            }
        }
    }
}

fn reset(files: &[(&str, &[u8])]) {
    with(|w| {
        *w = World::default();
        for (n, d) in files {
            w.files.insert(n.to_string(), d.to_vec());
        }
    });
}

fn take_log() -> Vec<String> {
    with(|w| std::mem::take(&mut w.log))
}

fn texture(name: &str) -> (c_long, *mut PngTexture) {
    let mut out = std::ptr::null_mut();
    let c = std::ffi::CString::new(name).unwrap();
    // SAFETY: the test host serves the path
    let r = unsafe { ee_of_find_texture(host, PATH as *mut c_void, c.as_ptr(), &mut out) };
    (r, out)
}

fn model(name: &str, shared: &[EeGeSharedFrame]) -> (c_long, *mut CemModel) {
    let mut out = std::ptr::null_mut();
    let c = std::ffi::CString::new(name).unwrap();
    // SAFETY: as above
    let r = unsafe { ee_of_find_model(host, PATH as *mut c_void, c.as_ptr(), shared.as_ptr(), shared.len(), &mut out) };
    (r, out)
}

/// A 1x1 RGB PNG (stored zlib block; the CRCs are not checked).
fn png(rgb: [u8; 3]) -> Vec<u8> {
    let mut p = vec![0x89, b'P', b'N', b'G', 0x0d, 0x0a, 0x1a, 0x0a];
    let chunk = |out: &mut Vec<u8>, kind: &[u8], data: &[u8]| {
        out.extend_from_slice(&(data.len() as u32).to_be_bytes());
        out.extend_from_slice(kind);
        out.extend_from_slice(data);
        out.extend_from_slice(&[0, 0, 0, 0]);
    };
    chunk(&mut p, b"IHDR", &[0, 0, 0, 1, 0, 0, 0, 1, 8, 2, 0, 0, 0]);
    let raw = [0u8, rgb[0], rgb[1], rgb[2]];
    let mut z = vec![0x78, 0x01, 1, 4, 0, 0xfb, 0xff];
    z.extend_from_slice(&raw);
    let (mut a, mut b) = (1u32, 0u32);
    for &v in &raw {
        a = (a + v as u32) % 65521;
        b = (b + a) % 65521;
    }
    z.extend_from_slice(&((b << 16) | a).to_be_bytes());
    chunk(&mut p, b"IDAT", &z);
    chunk(&mut p, b"IEND", &[]);
    p
}

#[test]
fn a_texture_is_its_png_and_the_further_frames_up_to_the_first_missing_one() {
    let (a, b) = (png([1, 2, 3]), png([4, 5, 6]));
    reset(&[("t\\x.png", &a), ("t\\x@1.png", &b), ("t\\x@3.png", &a)]);
    let (r, t) = texture("t\\x");
    assert_eq!(r, K_ERROR_OK);
    assert_eq!(
        take_log(),
        [
            "find t\\x.png true",
            "open t\\x.png",
            "close t\\x.png",
            "find t\\x@1.png true",
            "open t\\x@1.png",
            "close t\\x@1.png",
            "find t\\x@2.png false"
        ]
    );
    // SAFETY: a texture from the lookup
    unsafe {
        assert_eq!((*t).frames.len(), 2);
        crate::ffi::ee_of_texture_free(t);
    }
    assert!(with(|w| w.open.is_empty()));

    reset(&[]);
    assert_eq!(texture("t\\missing"), (K_ERROR_NOT_FOUND, std::ptr::null_mut()));
}

#[test]
fn a_frame_that_does_not_decode_stops_the_game_with_the_first_files_name() {
    let a = png([1, 2, 3]);
    reset(&[("t\\x.png", &a), ("t\\x@1.png", b"not a png")]);
    let (r, t) = texture("t\\x");
    assert_eq!((r, t), (K_ERROR_FAILED, std::ptr::null_mut()));
    assert_eq!(take_log().last().unwrap(), "verify t\\x.png: not a PNG file");
}

#[test]
fn a_file_that_cannot_be_read_counts_as_missing_and_is_closed() {
    let a = png([1, 2, 3]);
    for fault in 1..=3u8 {
        reset(&[("t\\x.png", &a)]);
        with(|w| w.faults.insert("t\\x.png".into(), fault));
        assert_eq!(texture("t\\x").0, K_ERROR_NOT_FOUND);
        let log = take_log();
        // a failed open is not closed; a failed length or short read is
        assert_eq!(log.contains(&"close t\\x.png".to_string()), fault != 1);
        assert!(with(|w| w.open.is_empty()));
    }
    // an empty file is read without a read call and refused by the decoder
    reset(&[("t\\e.png", b"")]);
    assert_eq!(texture("t\\e").0, K_ERROR_FAILED);
}

#[test]
fn a_model_is_glb_first_then_gltf_with_its_buffers_beside_it() {
    let doc = br#"{"asset":{"version":"2.0"},"buffers":[{"byteLength":3,"uri":"sub/a.bin"}]}"#;
    reset(&[("models\\m.gltf", doc), ("models\\sub\\a.bin", &[1, 2, 3])]);
    let shared = [EeGeSharedFrame { frame: 1, offset: -1 }];
    let (r, m) = model("models\\m", &shared);
    // no node with a mesh: the lookup's order and the buffer's name are under test
    assert_eq!((r, m), (K_ERROR_FAILED, std::ptr::null_mut()));
    let log = take_log();
    assert_eq!(log[0], "find models\\m.glb false");
    assert_eq!(log[1], "find models\\m.gltf true");
    assert!(log.contains(&"find models\\sub\\a.bin true".to_string()));
    assert_eq!(log.last().unwrap(), "verify models\\m.gltf: no node with a mesh");

    reset(&[("models\\m.glb", b"glTF bad")]);
    let (r, _) = model("models\\m", &[]);
    assert_eq!(r, K_ERROR_FAILED);
    assert!(!take_log().iter().any(|l| l.contains(".gltf")));

    reset(&[]);
    assert_eq!(model("models\\none", &[]), (K_ERROR_NOT_FOUND, std::ptr::null_mut()));
    assert_eq!(take_log(), ["find models\\none.glb false", "find models\\none.gltf false"]);
}

#[test]
fn the_directory_is_the_names_part_up_to_its_last_separator() {
    assert_eq!(directory_of(b"models\\a\\b"), b"models\\a\\");
    assert_eq!(directory_of(b"models/b"), b"models/");
    assert_eq!(directory_of(b"b"), b"");
}
