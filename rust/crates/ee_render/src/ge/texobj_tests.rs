//! The texture manager's golden replay (`golden_texman.txt`): sequences of calls recorded by a C++ driver over the
//! original GETextureManager.cpp with traced fakes of the objects it calls (the lock, the paths, the textures and
//! their surfaces and frames, the file lookup), replayed here on the Rust object through a mock host that must make
//! the trace's calls in its order. The line kinds are `manobj_tests.rs`'s; a texture's fields the original read or
//! wrote directly are a world the lines keep (`~tex` after a load, `~surf` for a surface), checked against the
//! driver's `state` lines after every call.

use super::*;
use crate::ge::manobj::tests::{cat, cstr, id, num, sequences, split_named, tok};
use std::collections::BTreeMap as Map;

#[derive(Clone, Default)]
struct Tex {
    refs: i64,
    managed: bool,
    pack: bool,
    pack_mips: i64,
    drop: u64,
    shift: u64,
    promoted: bool,
    /// alpha, color, mag, min, mip, bias bits, addressing (`EeGeTexStage` order less one)
    stage: [u64; 7],
    frame_format: u64,
    count: u64,
    mips: bool,
    alloc: u64,
    surface: usize,
    frames: Vec<usize>,
}

#[derive(Default)]
struct World {
    tex: Map<usize, Tex>,
    /// format, depth, volatility, 3D, transparency
    surf: Map<usize, [u64; 5]>,
    depth: Map<usize, i64>,
    paths: Map<usize, i64>,
    locks: i64,
    files: i64,
    dumps: Map<usize, Vec<u8>>,
}

struct Replay {
    lines: Vec<Vec<u8>>,
    at: usize,
    world: World,
    mgr: *mut EeGeTextureManager,
    last_dump: usize,
    /// A mock host's failure (a panic cannot unwind through the Rust object's `extern "C"` host call).
    failed: Option<String>,
}

fn words(line: &[u8]) -> Vec<&[u8]> {
    line.split(|&c| c == b' ').collect()
}

fn unum(t: &[u8]) -> u64 {
    std::str::from_utf8(t).unwrap().parse().unwrap()
}

fn desc_text(d: &EeGeSurfaceDescription) -> Vec<u8> {
    format!(
        "{},{},{},{},{},{}",
        d.pixel_format,
        d.surface_volatility,
        d.pixel_depth,
        d.is_3d_renderable as u8,
        d.is_transparency_enabled as u8,
        d.no_alpha as u8
    )
    .into_bytes()
}

fn new_tex(frames: usize) -> Tex {
    Tex { refs: 1, count: 1, frames: vec![frames], ..Tex::default() }
}

impl Replay {
    fn peek(&self) -> &[u8] {
        self.lines.get(self.at).map_or(b"", |l| l.as_slice())
    }

    fn fail(&self, what: &str) -> ! {
        let around: Vec<String> = self.lines[self.at.saturating_sub(8)..(self.at + 3).min(self.lines.len())]
            .iter()
            .map(|l| String::from_utf8_lossy(l).into_owned())
            .collect();
        panic!("{what} at line {} of the sequence:\n{}", self.at, around.join("\n"));
    }

    fn t(&mut self, x: usize) -> &mut Tex {
        self.world.tex.get_mut(&x).unwrap_or_else(|| panic!("no texture {}", String::from_utf8_lossy(&tok(x))))
    }

    /// The world's part of a line.
    fn apply(&mut self, line: &[u8]) {
        let line = line.strip_prefix(b"~").unwrap_or(line);
        let t = words(line);
        match t[0] {
            b"TexNew" => {
                self.world.tex.insert(id(t[1]), new_tex(id(t[1])));
            }
            b"TexNewFromSurface" => {
                let mut x = new_tex(id(t[1]));
                x.surface = id(t[2]);
                self.world.tex.insert(id(t[1]), x);
            }
            b"tex" => {
                let x = id(t[1]);
                let e = self.world.tex.entry(x).or_insert_with(|| new_tex(x));
                e.frame_format = unum(t[3]);
                e.count = unum(t[5]);
                e.drop = unum(t[7]);
                e.mips = t[9] == b"1";
                e.alloc = unum(t[11]);
                e.surface = id(t[13]);
                let st: Vec<&[u8]> = t[15].split(|&c| c == b',').collect();
                for (i, v) in st.iter().enumerate() {
                    e.stage[i] = if i == 5 {
                        u64::from_str_radix(std::str::from_utf8(v).unwrap(), 16).unwrap()
                    } else {
                        unum(v)
                    };
                }
                e.frames = t[17..].iter().map(|f| id(f)).collect();
            }
            b"surf" => {
                let v = [unum(t[2]), unum(t[3]), unum(t[4]), unum(t[5]), unum(t[6])];
                self.world.surf.insert(id(t[1]), v);
            }
            b"Reference" => self.t(id(t[1])).refs += 1,
            b"BaseDereference" => self.t(id(t[1])).refs -= 1,
            b"destroy" => {
                self.world.tex.remove(&id(t[1]));
            }
            b"TexSetStage" => {
                let i = unum(t[2]) as usize - 1;
                let v = unum(t[3]);
                self.t(id(t[1])).stage[i] = v;
            }
            b"TexSetPromoted" => self.t(id(t[1])).promoted = t[2] == b"1",
            b"TexSetLogicalShift" => self.t(id(t[1])).shift = unum(t[2]),
            b"ext" => match t[1] {
                b"ref" => self.t(id(t[2])).refs += 1,
                b"deref" => self.t(id(t[2])).refs -= 1,
                b"made" => managers::set_numbers_for_test(Some(unum(t[2]) as u32), None),
                b"pack" => {
                    let (_, dir, after) = split_named(line);
                    let dir = if dir == b"(null)" { Vec::new() } else { dir };
                    let (s, m) = (num(after[0]) as c_long, num(after[1]) as c_long);
                    with_pack(|k| k.set(&dir, s, m));
                }
                _ => {}
            },
            b"LockEnter" => *self.world.depth.entry(id(t[1])).or_default() += 1,
            b"LockLeave" => *self.world.depth.entry(id(t[1])).or_default() -= 1,
            b"LockCreate" => self.world.locks += 1,
            b"LockDestroy" => self.world.locks -= 1,
            b"PathReference" => *self.world.paths.entry(id(t[1])).or_insert(1) += 1,
            b"PathDereference" => *self.world.paths.entry(id(t[1])).or_insert(1) -= 1,
            b"TexFindFile" if line.ends_with(b"= 2") => {}
            b"TexFindFile" => self.world.files += 1,
            b"TexFreeFile" => self.world.files -= 1,
            _ => {}
        }
    }

    fn take(&mut self) {
        let line = self.peek().to_vec();
        self.apply(&line);
        self.at += 1;
        self.sides();
    }

    /// The fakes' own lines (`~`).
    fn sides(&mut self) {
        while self.peek().starts_with(b"~") {
            let side = self.peek().to_vec();
            self.apply(&side);
            self.at += 1;
        }
    }

    fn expect(&mut self, want: &[u8]) {
        if self.peek() != want {
            self.fail(&format!("expected `{}`", String::from_utf8_lossy(want)));
        }
        self.take();
    }

    fn expect_answer(&mut self, want: &[u8]) -> Vec<Vec<u8>> {
        let line = self.peek().to_vec();
        let prefix = cat(&[want, b" = "]);
        if !line.starts_with(&prefix) {
            self.fail(&format!("expected `{} = ...`", String::from_utf8_lossy(want)));
        }
        self.take();
        line[prefix.len()..].split(|&c| c == b' ').map(|t| t.to_vec()).collect()
    }

    /// A line naming a new object (`TexNew X5 G`): the object.
    fn expect_new(&mut self, kind: &[u8], args: &[&[u8]]) -> usize {
        let line = self.peek().to_vec();
        let t = words(&line);
        if t.len() != args.len() + 2 || t[0] != kind || t[2..] != *args {
            self.fail(&format!("expected {}", String::from_utf8_lossy(kind)));
        }
        let x = id(t[1]);
        self.take();
        x
    }

    fn nested(&mut self) {
        while self.peek().starts_with(b"call ") {
            self.call();
        }
    }

    fn op(&mut self, op: EeGeManOp, obj: usize, a: isize, b: isize, c: isize) -> isize {
        use EeGeManOp as O;
        use EeGeTexField as F;
        let o = tok(obj);
        match op {
            O::LockCreate => id(&self.expect_answer(b"LockCreate")[0]) as isize,
            O::LockDestroy => {
                let d = self.world.depth.get(&obj).copied().unwrap_or(0).to_string();
                self.expect(&cat(&[b"LockDestroy ", &o, b" depth ", d.as_bytes()]));
                0
            }
            O::LockEnter => {
                self.expect(&cat(&[b"LockEnter ", &o]));
                0
            }
            O::LockLeave => {
                self.expect(&cat(&[b"LockLeave ", &o]));
                0
            }
            O::PathReference => {
                self.expect(&cat(&[b"PathReference ", &o]));
                0
            }
            O::PathDereference => {
                self.expect(&cat(&[b"PathDereference ", &o]));
                0
            }
            O::RefCount => self.t(obj).refs as isize,
            O::BaseDereference => {
                self.expect(&cat(&[b"BaseDereference ", &o]));
                0
            }
            O::DumpHeader => {
                let d = self.world.dumps.entry(obj).or_default();
                d.extend_from_slice(b"\n\n*********Texture Manager - Current Loaded Files In Memory*********\n\n");
                0
            }
            O::DumpLine => {
                let count = self.t(obj).refs.to_string();
                let line = cat(&[
                    b"The ",
                    &cstr(a),
                    b" texture file is loaded in memory with a reference count of ",
                    count.as_bytes(),
                    b".\n",
                ]);
                self.world.dumps.entry(b as usize).or_default().extend_from_slice(&line);
                0
            }
            O::TexFindFile => {
                let r = self.expect_answer(&cat(&[b"TexFindFile ", &o, b" [", &cstr(a), b"]"]));
                let result = num(&r[0]);
                if result == 0 {
                    // SAFETY: the out pointer the Rust object passes
                    unsafe { *(c as *mut usize) = id(&r[1]) };
                }
                result as isize
            }
            O::TexFreeFile => {
                self.expect(&cat(&[b"TexFreeFile ", &o]));
                0
            }
            O::TexNew => self.expect_new(b"TexNew", &[b"G"]) as isize,
            O::TexNewFromSurface => self.expect_new(b"TexNewFromSurface", &[&o, b"G"]) as isize,
            O::TexLoad => {
                // SAFETY: the Rust object's arguments
                let l = unsafe { &*(a as *const EeGeTexLoad) };
                // SAFETY: the description the arguments point at
                let d = unsafe { &mut *l.description };
                let want = cat(&[
                    b"TexLoad ",
                    &o,
                    b" ",
                    &tok(l.file as usize),
                    b" ",
                    &tok(b as usize),
                    format!(
                        " mips {} drop {} ff {} desc ",
                        l.create_mipmaps as u8, l.num_mip_levels_to_drop, l.frame_format
                    )
                    .as_bytes(),
                    &desc_text(d),
                ]);
                self.expect(&want);
                self.nested();
                self.sides();
                let line = self.peek().to_vec();
                let t = words(&line);
                if t.len() != 4 || t[0] != b"=>" || t[2] != b"desc" {
                    self.fail("expected the load's result");
                }
                let v: Vec<u64> = t[3].split(|&c| c == b',').map(unum).collect();
                d.pixel_format = v[0] as c_ulong;
                d.surface_volatility = v[1] as c_ulong;
                d.pixel_depth = v[2] as c_ulong;
                d.is_3d_renderable = v[3] != 0;
                d.is_transparency_enabled = v[4] != 0;
                d.no_alpha = v[5] != 0;
                self.take();
                num(t[1]) as isize
            }
            O::TexGet => {
                let x = self.t(obj).clone();
                match a {
                    a if a == F::FromPack as isize => x.pack as isize,
                    a if a == F::PackMipLevels as isize => x.pack_mips as isize,
                    a if a == F::NumMipLevelsToDrop as isize => x.drop as isize,
                    a if a == F::CreateMipMaps as isize => x.mips as isize,
                    a if a == F::FrameFormat as isize => x.frame_format as isize,
                    a if a == F::FrameCount as isize => x.count as isize,
                    _ => self.fail("an unknown field read"),
                }
            }
            O::TexSet => {
                match a {
                    a if a == F::FromPack as isize => self.t(obj).pack = b != 0,
                    a if a == F::PackMipLevels as isize => self.t(obj).pack_mips = b as i64,
                    a if a == F::NumMipLevelsToDrop as isize => self.t(obj).drop = b as c_ulong as u64,
                    a if a == F::IsManaged as isize => self.t(obj).managed = b != 0,
                    a if a == F::Promoted as isize => {
                        self.expect(&cat(&[b"TexSetPromoted ", &o, if b != 0 { b" 1" } else { b" 0" }]))
                    }
                    _ => self.fail("an unknown field write"),
                }
                0
            }
            O::TexSetLogicalShift => {
                self.expect(&cat(&[b"TexSetLogicalShift ", &o, b" ", (a as c_ulong).to_string().as_bytes()]));
                0
            }
            O::TexStage => self.t(obj).stage[a as usize - 1] as isize,
            O::TexSetStage => {
                let v = if a == EeGeTexStage::MipMapBias as isize {
                    (b as u32).to_string()
                } else {
                    (b as c_ulong).to_string()
                };
                self.expect(&cat(&[b"TexSetStage ", &o, b" ", a.to_string().as_bytes(), b" ", v.as_bytes(), b" -1"]));
                0
            }
            O::TexFrame => {
                let x = self.t(obj).clone();
                (if x.frame_format == 0 { x.frames[a as usize] } else { x.frames[0] }) as isize
            }
            O::TexReference => {
                self.expect(&cat(&[b"Reference ", &o]));
                0
            }
            O::TexDereference => {
                self.expect(&cat(&[b"TexDereference ", &o]));
                self.nested();
                0
            }
            O::TexSurfaceDescription => {
                let surface = self.t(obj).surface;
                let s = self.world.surf[&surface];
                // SAFETY: the description the Rust object passes
                let d = unsafe { &mut *(a as *mut EeGeSurfaceDescription) };
                d.pixel_format = s[0] as c_ulong;
                d.pixel_depth = s[1] as c_ulong;
                d.surface_volatility = s[2] as c_ulong;
                d.is_3d_renderable = s[3] != 0;
                d.is_transparency_enabled = s[4] != 0;
                0
            }
            O::TexRecreate => {
                let r = self.expect_answer(&cat(&[b"TexRecreate ", &o, b" ", &tok(a as usize)]));
                num(&r[0]) as isize
            }
            O::TexAllocatedSize => self.t(obj).alloc as isize,
            O::TexPackLog => 0,
            _ => self.fail(&format!("a model operation {op:?}")),
        }
    }

    fn description(t: &[&[u8]]) -> EeGeTextureDescription {
        EeGeTextureDescription {
            surface_volatility: unum(t[0]) as c_ulong,
            frame_format: unum(t[1]) as c_ulong,
            pixel_depth: unum(t[2]) as c_ulong,
            num_mip_levels_to_drop: num(t[3]) as c_long,
            is_3d_renderable: t[4] == b"1",
            is_transparency_enabled: t[5] == b"1",
            has_alpha: t[6] == b"1",
            create_mipmaps: t[7] == b"1",
            use_texture_pack: t[8] == b"1",
        }
    }

    fn call(&mut self) {
        let line = self.peek().to_vec();
        self.take();
        let t = words(&line);
        let p = self.mgr;
        let me = self as *mut Replay as *mut c_void;
        // SAFETY: the object is the replay's; the mock host serves it
        let ret: Vec<u8> = unsafe {
            match t[1] {
                b"new" => {
                    self.mgr = EeGeTextureManager::create(host, me, id(t[2]));
                    Vec::new()
                }
                b"free" => {
                    EeGeTextureManager::destroy(p);
                    Vec::new()
                }
                b"load" | b"reload" => {
                    let (_, name, after) = split_named(&line);
                    let d = Self::description(&after);
                    let cname = crate::ge::manobj::c_string(&name);
                    if t[1] == b"load" {
                        tok(EeGeTextureManager::load(p, cname.as_ptr().cast(), &d))
                    } else {
                        EeGeTextureManager::reload(p, cname.as_ptr().cast(), &d).to_string().into_bytes()
                    }
                }
                b"make" => tok(EeGeTextureManager::make(p, id(t[2]))),
                b"promote" => EeGeTextureManager::promote(p, id(t[2])).to_string().into_bytes(),
                b"refresh_all" => EeGeTextureManager::refresh_all(p).to_string().into_bytes(),
                b"remove" => EeGeTextureManager::remove(p, id(t[2])).to_string().into_bytes(),
                b"remove_all" => EeGeTextureManager::remove_all(p).to_string().into_bytes(),
                b"set_path" => {
                    EeGeTextureManager::set_path(p, id(t[2]));
                    Vec::new()
                }
                b"update_mip" => {
                    EeGeTextureManager::update_mip_filter(p, unum(t[2]) as c_ulong);
                    Vec::new()
                }
                b"dump" => {
                    self.last_dump = id(t[2]);
                    EeGeTextureManager::dump(p, id(t[2]));
                    Vec::new()
                }
                b"footprint" => ee_ge_texman_footprint(p).to_string().into_bytes(),
                b"count" => ee_ge_texman_count(p).to_string().into_bytes(),
                b"screen" => tok(ee_ge_texman_screen(p) as usize),
                b"path" => tok(ee_ge_texman_path(p) as usize),
                b"at" => tok(ee_ge_texman_at(p, unum(t[2]) as c_ulong) as usize),
                b"set_screen" => {
                    ee_ge_texman_set_screen(p, id(t[2]) as *mut c_void);
                    Vec::new()
                }
                _ => self.fail("an unknown call"),
            }
        };
        if let Some(what) = self.failed.take() {
            panic!("{what}");
        }
        let want = if ret.is_empty() { b"ret".to_vec() } else { cat(&[b"ret ", &ret]) };
        self.expect(&want);
    }

    fn state_line(&self, x: usize, e: &Tex) -> Vec<u8> {
        let s = e.stage;
        cat(&[
            b"state ",
            &tok(x),
            format!(
                " r {} m {} p {} pm {} d {} sh {} pr {} st {},{},{},{},{},{:08x},{}",
                e.refs,
                e.managed as u8,
                e.pack as u8,
                e.pack_mips,
                e.drop,
                e.shift,
                e.promoted as u8,
                s[0],
                s[1],
                s[2],
                s[3],
                s[4],
                s[5],
                s[6]
            )
            .as_bytes(),
        ])
    }

    fn sequence(&mut self) {
        loop {
            let line = self.peek().to_vec();
            if line.starts_with(b"call ") {
                self.call();
            } else if line.starts_with(b"state ") {
                // the driver's states, every live texture in id order
                let states: Vec<Vec<u8>> = self.world.tex.iter().map(|(x, e)| self.state_line(*x, e)).collect();
                for s in states {
                    if self.peek() != s.as_slice() {
                        self.fail(&format!("expected `{}`", String::from_utf8_lossy(&s)));
                    }
                    self.at += 1;
                }
                // the next block (a call that traced nothing) starts again at the first texture
                let first = states_first(&self.world);
                if self.peek().starts_with(b"state ") && Some(words(self.peek())[1].to_vec()) != first {
                    self.fail("a texture the world does not have");
                }
            } else if let Some(text) = line.strip_prefix(b"file ") {
                let got = self.world.dumps.remove(&self.last_dump).unwrap_or_default();
                let got: Vec<u8> = got.iter().map(|&c| if c == b'\n' { b'|' } else { c }).collect();
                if got != text {
                    self.fail("a different dump");
                }
                self.at += 1;
            } else if line.starts_with(b"end ") {
                let w = &self.world;
                let paths: Vec<String> = (1..=3)
                    .map(|i| w.paths.get(&id(format!("P{i}").as_bytes())).copied().unwrap_or(1).to_string())
                    .collect();
                let want = format!(
                    "end textures {} paths {} locks {} files {}",
                    w.tex.len(),
                    paths.join(" "),
                    w.locks,
                    w.files
                );
                if line != want.as_bytes() {
                    self.fail(&format!("expected `{want}`"));
                }
                return;
            } else {
                // the driver's own actions and what the fakes did for them (a texture released by its holder)
                self.take();
            }
        }
    }
}

/// The first live texture's token (a state block's first line).
fn states_first(w: &World) -> Option<Vec<u8>> {
    w.tex.keys().next().map(|&x| tok(x))
}

unsafe extern "C" fn host(owner: *mut c_void, op: EeGeManOp, obj: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    // SAFETY: the owner is the replay running the sequence
    let r = unsafe { &mut *(owner as *mut Replay) };
    if r.failed.is_some() {
        return 0;
    }
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| r.op(op, obj as usize, a, b, c))) {
        Ok(v) => v,
        Err(e) => {
            let what = e.downcast_ref::<String>().cloned().or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()));
            r.failed = Some(what.unwrap_or_default());
            0
        }
    }
}

#[test]
fn the_texture_managers_golden_sequences_replay() {
    let golden = include_bytes!("golden_texman.txt");
    let seqs = sequences(golden);
    assert!(seqs.len() >= 40);
    let _globals = managers::test_globals();
    let mut lines_replayed = 0;
    for (n, lines) in seqs {
        let count = lines.len();
        let mut r =
            Replay { lines, at: 0, world: World::default(), mgr: std::ptr::null_mut(), last_dump: 0, failed: None };
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| r.sequence()));
        if let Err(e) = result {
            let what = e.downcast_ref::<String>().cloned().or_else(|| e.downcast_ref::<&str>().map(|s| s.to_string()));
            panic!("sequence {n}: {}", what.unwrap_or_default());
        }
        lines_replayed += count;
    }
    with_pack(|k| k.set(b"", 0, 0));
    assert!(lines_replayed > 10000, "{lines_replayed}");
}

#[test]
fn the_surface_description_follows_the_texture_description() {
    let t = EeGeTextureDescription {
        surface_volatility: 2,
        frame_format: 1,
        pixel_depth: 32,
        num_mip_levels_to_drop: 1,
        is_3d_renderable: true,
        is_transparency_enabled: false,
        has_alpha: false,
        create_mipmaps: true,
        use_texture_pack: true,
    };
    let mut d = EeGeSurfaceDescription { kind: 6, width: 9, ..Default::default() };
    surface_description_for(&t, &mut d);
    assert_eq!((d.pixel_depth, d.pixel_format, d.no_alpha), (32, managers::FORMAT_A8R8G8B8, true));
    assert_eq!((d.surface_volatility, d.is_3d_renderable, d.is_transparency_enabled), (2, true, false));
    assert_eq!((d.kind, d.width), (6, 9), "the rest as it was");
    assert_eq!(buffered(vec![b'a'; 100], 64).len(), 64);
}

/// A texture's host that answers nothing (the direct path's test makes no call out but the stage setters' marks).
unsafe extern "C" fn quiet_tex_host(
    _owner: *mut c_void,
    _op: crate::ge::texobj_tex::EeGeTexOp,
    _obj: *mut c_void,
    _a: isize,
    _b: isize,
    _c: isize,
) -> isize {
    0
}

/// LLE round 8: the texture operations a C++-made manager answers on the texture's Rust object (`tex_direct`) give
/// what the C++ host's forwarder gave: the getters' casts, the setters' values, the stage and its bias bits, the
/// reference count read in place; the operations the C++ keeps are not answered.
#[test]
fn a_cpp_managers_texture_operations_are_the_forwarders_calls() {
    use crate::ge::cppobj::CppCounted;
    use crate::ge::texobj_tex::{self as tex, EeGeTexVal};
    let mut cpp = CppCounted { vtable: std::ptr::null(), count: 3, rust: std::ptr::null_mut() };
    let obj = &mut cpp as *mut CppCounted as usize;
    let t = EeGeTexture::create(quiet_tex_host, obj, 0);
    cpp.rust = t.cast();
    unsafe {
        let d = |op, a: isize, b: isize| tex_direct(op, obj, a, b);
        assert_eq!(d(EeGeManOp::RefCount, 0, 0), Some(3));
        assert_eq!(d(EeGeManOp::TexReference, 0, 0), Some(0));
        assert_eq!(cpp.count, 4, "Reference() is the count's increment");
        // the setters and their getters
        assert_eq!(d(EeGeManOp::TexSet, EeGeTexField::FromPack as isize, 5), Some(0));
        assert_eq!(tex::ee_ge_tex_get(t, EeGeTexVal::FromPack), 1, "SetIsFromTexturePack(b != 0)");
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::FromPack as isize, 0), Some(1));
        d(EeGeManOp::TexSet, EeGeTexField::PackMipLevels as isize, -2);
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::PackMipLevels as isize, 0), Some(-2), "long, sign-extended");
        d(EeGeManOp::TexSet, EeGeTexField::NumMipLevelsToDrop as isize, 3);
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::NumMipLevelsToDrop as isize, 0), Some(3));
        d(EeGeManOp::TexSet, EeGeTexField::IsManaged as isize, 9);
        assert_eq!(tex::ee_ge_tex_get(t, EeGeTexVal::IsManaged), 1);
        d(EeGeManOp::TexSet, EeGeTexField::Promoted as isize, 1);
        assert_eq!(tex::ee_ge_tex_get(t, EeGeTexVal::IsPromoted), 1, "SetIsPromoted");
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::FrameCount as isize, 0), Some(1));
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::FrameFormat as isize, 0), Some(1));
        assert_eq!(d(EeGeManOp::TexGet, EeGeTexField::CreateMipMaps as isize, 0), Some(0));
        // the stage: the constructor's, then a setter and the bias as its bits
        assert_eq!(d(EeGeManOp::TexStage, EeGeTexStage::TextureAddressingMode as isize, 0), Some(ADDRESSING_CLAMP));
        assert_eq!(d(EeGeManOp::TexStage, EeGeTexStage::MipFilterMode as isize, 0), Some(MIP_FILTER_POINT));
        d(EeGeManOp::TexSetStage, EeGeTexStage::ColorMode as isize, STAGE_OP_MODULATE);
        assert_eq!(d(EeGeManOp::TexStage, EeGeTexStage::ColorMode as isize, 0), Some(STAGE_OP_MODULATE));
        assert_eq!((*tex::ee_ge_tex_stage(t)).color_mode, STAGE_OP_MODULATE as c_ulong);
        d(EeGeManOp::TexSetStage, EeGeTexStage::MipMapBias as isize, 1.5f32.to_bits() as isize);
        assert_eq!((*tex::ee_ge_tex_stage(t)).mip_map_bias, 1.5);
        assert_eq!(d(EeGeManOp::TexStage, EeGeTexStage::MipMapBias as isize, 0), Some(1.5f32.to_bits() as isize));
        assert_eq!(d(EeGeManOp::TexFrame, 0, 0), Some(tex::ee_ge_tex_frame_texture(t, 0) as isize), "GetFrameTexture");
        // what stays the C++'s
        for op in
            [EeGeManOp::TexNew, EeGeManOp::BaseDereference, EeGeManOp::TexSurfaceDescription, EeGeManOp::LockEnter]
        {
            assert_eq!(d(op, 0, 0), None, "{op:?}");
        }
        assert_eq!(d(EeGeManOp::TexGet, 99, 0), None, "an unknown field");
        drop(Box::from_raw(t));
    }
}
