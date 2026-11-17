//! `texobj_tex.rs`'s tests: the golden replay (`golden_gtex.txt`: the first sequences of the scratch driver over
//! the original GETexture.cpp and traced fakes) and a few direct checks of the object.
//!
//! The replay keeps a world of the driver's objects (textures and surfaces by construction serial, two managers,
//! two screens), makes each "do" line's call on the Rust objects, and writes every line the fakes and the driver
//! would write; each must equal the golden's (pixel hashes, `#...`, aside). The fakes' random answers (a failed
//! surface creation, lock, copy or promotion) and a frame that does not fit its surface are read from the golden.

use super::*;
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet};

const GOLDEN: &str = include_str!("golden_gtex.txt");
const FILE: usize = 0x7f11e;
const MANAGER_BASE: usize = 0x1000;

#[derive(Clone, Copy)]
struct Surf {
    desc: EeGeSurfaceDescription,
    screen: usize,
    next: usize,
    shift: c_ulong,
}

struct Obj {
    surf: Option<Surf>,
    tex: *mut EeGeTexture,
    refs: u32,
    alive: bool,
}

#[derive(Default)]
struct World {
    objects: BTreeMap<usize, Obj>,
    serial: usize,
    caches: [BTreeSet<usize>; 2],
    lines: Vec<String>,
    at: usize,
    side: i32,
    file: [u32; 3],
    errors: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(&mut w.borrow_mut()))
}

/// The golden's next line without its hash.
fn peek() -> String {
    with(|w| w.lines.get(w.at).map(|l| l.split(" #").next().unwrap_or("").to_string()).unwrap_or_default())
}

/// A line the replay writes: it must be the golden's next.
fn emit(line: String) {
    with(|w| {
        let line = if w.side > 0 { format!("~{line}") } else { line };
        let want = w.lines.get(w.at).map(|l| l.split(" #").next().unwrap_or("").to_string()).unwrap_or_default();
        if want != line && w.errors.len() < 5 {
            w.errors.push(format!("line {}: want [{want}] got [{line}]", w.at));
        }
        w.at += 1;
    })
}

/// The number after `= ` in the golden's next line (the fakes' answer), or 0.
fn answer() -> isize {
    let l = peek();
    l.split(" = ").nth(1).and_then(|r| r.split(' ').next()).and_then(|r| r.parse().ok()).unwrap_or(0)
}

fn name(o: usize) -> String {
    if o == 0 {
        return "0".into();
    }
    with(|w| match w.objects.get(&o) {
        None => "?".into(),
        Some(x) => format!("{}{o}{}", if x.surf.is_some() { 'S' } else { 'T' }, if x.alive { "" } else { "+" }),
    })
}

fn bits(f: f32) -> String {
    format!("{:08x}", f.to_bits())
}

fn desc_text(d: &EeGeSurfaceDescription) -> String {
    format!(
        "{{{} {} {} {}x{} {} ml{} d{} {}{}{}}}",
        d.kind,
        d.pixel_format,
        d.surface_volatility,
        d.width,
        d.height,
        d.m14,
        d.mip_level_count,
        d.pixel_depth,
        d.is_3d_renderable as u8,
        d.is_transparency_enabled as u8,
        d.no_alpha as u8
    )
}

fn parse_desc(t: &str) -> EeGeSurfaceDescription {
    let t = t.trim_matches(|c| c == '{' || c == '}');
    let f: Vec<&str> = t.split(' ').collect();
    let (w, h) = f[3].split_once('x').unwrap();
    let flags = f[7].as_bytes();
    EeGeSurfaceDescription {
        kind: f[0].parse().unwrap(),
        pixel_format: f[1].parse().unwrap(),
        surface_volatility: f[2].parse().unwrap(),
        width: w.parse().unwrap(),
        height: h.parse().unwrap(),
        m14: f[4].parse().unwrap(),
        mip_level_count: f[5][2..].parse().unwrap(),
        pixel_depth: f[6][1..].parse().unwrap(),
        is_3d_renderable: flags[0] == b'1',
        is_transparency_enabled: flags[1] == b'1',
        no_alpha: flags[2] == b'1',
    }
}

/// The fakes' surface: a chain of halving levels, each holding the next (serials in construction order).
fn new_surface(desc: EeGeSurfaceDescription, screen: usize, levels: c_long) -> usize {
    let s = with(|w| {
        w.serial += 1;
        let s = w.serial;
        w.objects.insert(
            s,
            Obj {
                surf: Some(Surf { desc, screen, next: 0, shift: 0 }),
                tex: std::ptr::null_mut(),
                refs: 1,
                alive: true,
            },
        );
        s
    });
    if levels > 1 {
        let mut d = desc;
        d.width = if desc.width > 1 { desc.width / 2 } else { 1 };
        d.height = if desc.height > 1 { desc.height / 2 } else { 1 };
        d.mip_level_count = levels - 1;
        let next = new_surface(d, screen, levels - 1);
        with(|w| w.objects.get_mut(&s).unwrap().surf.as_mut().unwrap().next = next);
    }
    s
}

fn surf(s: usize) -> Surf {
    with(|w| w.objects[&s].surf.unwrap())
}

fn bytes_per_row(d: &EeGeSurfaceDescription) -> c_ulong {
    (d.width + 1) * if d.pixel_depth == 32 { 4 } else { 2 }
}

fn surf_dereference(s: usize) {
    let gone = with(|w| {
        let o = w.objects.get_mut(&s).unwrap();
        o.refs -= 1;
        o.refs == 0
    });
    if gone {
        emit(format!("destroy {}", name(s)));
        with(|w| w.side += 1);
        let next = surf(s).next;
        with(|w| w.objects.get_mut(&s).unwrap().alive = false);
        if next != 0 {
            surf_dereference(next);
        }
        with(|w| w.side -= 1);
    }
}

fn new_texture(manager: usize) -> usize {
    let t = with(|w| {
        w.serial += 1;
        let t = w.serial;
        w.objects.insert(t, Obj { surf: None, tex: std::ptr::null_mut(), refs: 1, alive: true });
        t
    });
    let p = EeGeTexture::create(host, t, manager);
    with(|w| w.objects.get_mut(&t).unwrap().tex = p);
    t
}

fn tex(t: usize) -> *mut EeGeTexture {
    with(|w| w.objects[&t].tex)
}

fn base_dereference(t: usize) {
    let gone = with(|w| {
        let o = w.objects.get_mut(&t).unwrap();
        o.refs -= 1;
        o.refs == 0
    });
    if gone {
        emit(format!("destroy {}", name(t)));
        with(|w| w.side += 1);
        // SAFETY: a live texture of the world
        unsafe { EeGeTexture::destroy(tex(t)) };
        with(|w| {
            let o = w.objects.get_mut(&t).unwrap();
            o.alive = false;
            o.tex = std::ptr::null_mut();
        });
        with(|w| w.side -= 1);
    }
}

fn get(t: usize, f: EeGeTexVal) -> isize {
    // SAFETY: a live texture of the world
    unsafe { EeGeTexture::get(tex(t), f) }
}

fn remove_texture(manager: usize, t: usize) -> isize {
    let m = manager - MANAGER_BASE;
    emit(format!("RemoveTexture M{m} {}", name(t)));
    if with(|w| w.caches[m - 1].remove(&t)) {
        with(|w| w.side += 1);
        base_dereference(t);
        with(|w| w.side -= 1);
        0
    } else {
        3
    }
}

/// The fakes' `GEStoreFrame`: a frame that fits is locked in, stored and unlocked.
fn store_frame(s: usize, frame: isize) -> isize {
    emit(format!("FileFrame {frame}"));
    if !peek().trim_start_matches('~').starts_with("Lock ") {
        return 9;
    }
    let r = answer();
    emit(format!("Lock {} = {r}", name(s)));
    if r != 0 {
        return r;
    }
    let r = answer();
    emit(format!("Unlock {} = {r}", name(s)));
    r
}

fn rect(r: &EeGeTexRect) -> String {
    format!("({} {} {} {})", bits(r[0]), bits(r[1]), bits(r[2]), bits(r[3]))
}

unsafe extern "C" fn host(owner: *mut c_void, op: EeGeTexOp, obj: *mut c_void, a: isize, b: isize, _c: isize) -> isize {
    let o = obj as usize;
    let _ = owner;
    match op {
        EeGeTexOp::SurfReference => {
            with(|w| w.objects.get_mut(&o).unwrap().refs += 1);
            0
        }
        EeGeTexOp::SurfDereference => {
            surf_dereference(o);
            0
        }
        EeGeTexOp::SurfParentScreen => surf(o).screen as isize,
        EeGeTexOp::SurfAllocatedSize => {
            let s = surf(o);
            let bytes = if s.desc.pixel_depth == 32 { 4 } else { 2 };
            ((s.desc.width + 1) * bytes * s.desc.height + 16 + if s.next != 0 { 7 } else { 0 }) as isize
        }
        EeGeTexOp::SurfLogicalShift => surf(o).shift as isize,
        EeGeTexOp::SurfSetLogicalShift => {
            emit(format!("SetLogicalShift {} {a}", name(o)));
            with(|w| w.objects.get_mut(&o).unwrap().surf.as_mut().unwrap().shift = a as c_ulong);
            0
        }
        EeGeTexOp::SurfLock => {
            let r = answer();
            emit(format!("Lock {} = {r}", name(o)));
            r
        }
        EeGeTexOp::SurfUnlock => {
            let r = answer();
            emit(format!("Unlock {} = {r}", name(o)));
            r
        }
        EeGeTexOp::SurfMemory => (o << 16) as isize,
        EeGeTexOp::SurfBytesPerRow => bytes_per_row(&surf(o).desc) as isize,
        EeGeTexOp::SurfDescription => {
            // SAFETY: the object's out pointer
            unsafe { *(a as *mut EeGeSurfaceDescription) = surf(o).desc };
            0
        }
        EeGeTexOp::SurfAttached => surf(o).next as isize,
        EeGeTexOp::SurfAttachedCount => (surf(o).next != 0) as isize,
        EeGeTexOp::SurfWidth => (surf(o).desc.width >> surf(o).shift) as isize,
        EeGeTexOp::SurfHeight => (surf(o).desc.height >> surf(o).shift) as isize,
        EeGeTexOp::SurfCopyWithFilter | EeGeTexOp::SurfCopyTo => {
            // SAFETY: the object's copy
            let copy = unsafe { *(b as *const EeGeTexCopy) };
            let (from, to) = (name(o), name(a as usize));
            if op == EeGeTexOp::SurfCopyTo {
                let r = answer();
                emit(format!(
                    "CopyTo {from} {to} {} {} {} = {r}",
                    rect(&copy.source),
                    rect(&copy.destination),
                    copy.transparent as u8
                ));
                r
            } else {
                emit(format!("CopyWithFilter {from} {to} {} {}", rect(&copy.source), rect(&copy.destination)));
                0
            }
        }
        EeGeTexOp::ScreenCreateSurface => {
            // SAFETY: the object's description and out pointer
            let d = unsafe { *(a as *const EeGeSurfaceDescription) };
            if peek().ends_with("= 8") {
                emit(format!("CreateSurface W{o} {} = 8", desc_text(&d)));
                return 8;
            }
            let levels = if d.kind == 6 { d.mip_level_count.min(12) } else { 1 };
            let s = new_surface(d, o, levels);
            // SAFETY: the out pointer
            unsafe { *(b as *mut usize) = s };
            emit(format!("CreateSurface W{o} {} = 0 {}", desc_text(&d), name(s)));
            0
        }
        EeGeTexOp::FileInfo => {
            let f = with(|w| w.file);
            // SAFETY: the out array of three
            unsafe { std::ptr::copy_nonoverlapping(f.as_ptr(), a as *mut u32, 3) };
            0
        }
        EeGeTexOp::StoreFrame => store_frame(o, b),
        EeGeTexOp::TexNew => new_texture(o) as isize,
        EeGeTexOp::TexObj => tex(o) as isize,
        EeGeTexOp::TexDereference => {
            // SAFETY: a live texture
            unsafe { EeGeTexture::dereference(tex(o)) };
            0
        }
        EeGeTexOp::TexBaseDereference => {
            base_dereference(o);
            0
        }
        EeGeTexOp::TexRefCount => with(|w| w.objects[&o].refs) as isize,
        EeGeTexOp::ManagerRemove => remove_texture(o, a as usize),
        EeGeTexOp::ManagerPromote => {
            let r = answer();
            emit(format!("PromoteTexture M{} {} = {r}", o - MANAGER_BASE, name(a as usize)));
            if r == 0 {
                // SAFETY: a live texture
                unsafe { EeGeTexture::set(tex(a as usize), EeGeTexVal::IsPromoted, 1) };
            }
            r
        }
        EeGeTexOp::RasterCreate | EeGeTexOp::RasterDestroy => {
            let what = if op == EeGeTexOp::RasterCreate { "CreateHandle" } else { "DestroyHandle" };
            let s = get(o, EeGeTexVal::Surface) as usize;
            emit(format!("{what} {} {}", name(o), name(s)));
            0
        }
    }
}

/// The driver's state line of a live texture (`GETextureManager::TraceTexture` in the fakes).
fn state_line(t: usize) -> String {
    let p = tex(t);
    // SAFETY: a live texture of the world
    let (stage, corner, uv, frames) = unsafe {
        let s = *ee_ge_tex_stage(p);
        let c = *ee_ge_tex_corner(p);
        let f = tx(p).frames.clone();
        (s, c, tx(p).frame_uv_size, f)
    };
    let refs = with(|w| w.objects[&t].refs);
    let surface = get(t, EeGeTexVal::Surface) as usize;
    let screen = get(t, EeGeTexVal::Screen);
    let mut l = format!(
        "tex {} r{refs} m{} st[{} {} {} {} {} {} {} {}] c({} {}) ft{} d{:x} uv{} ff{} f{} fc{} fpr{} ml{} drop{} {}{}{}{} pack{}/{} sur{} scr{} fr{}",
        name(t),
        get(t, EeGeTexVal::IsManaged),
        name(stage.texture as usize),
        stage.color_mode,
        stage.alpha_mode,
        stage.texture_addressing_mode,
        stage.min_filter_mode,
        stage.mag_filter_mode,
        stage.mip_filter_mode,
        bits(stage.mip_map_bias),
        bits(corner.u),
        bits(corner.v),
        name(get(t, EeGeTexVal::FrameTexture) as usize),
        get(t, EeGeTexVal::Data),
        bits(uv),
        get(t, EeGeTexVal::FrameFormat),
        get(t, EeGeTexVal::Frame),
        get(t, EeGeTexVal::FrameCount),
        get(t, EeGeTexVal::FramesPerRow),
        get(t, EeGeTexVal::MipLevelCount),
        get(t, EeGeTexVal::NumMipLevelsToDrop),
        get(t, EeGeTexVal::IsCompressed),
        get(t, EeGeTexVal::IsPromoted),
        get(t, EeGeTexVal::HasChanged),
        get(t, EeGeTexVal::CreateMipMaps),
        get(t, EeGeTexVal::FromPack),
        get(t, EeGeTexVal::PackMipLevels),
        name(surface),
        screen,
        if frames.is_some() { "" } else { "0" }
    );
    if get(t, EeGeTexVal::FrameFormat) == 0 {
        if let Some(f) = &frames {
            for &x in f.iter().take(get(t, EeGeTexVal::FrameCount) as usize) {
                l += &format!(" {}", name(x));
            }
        }
    }
    let frame = get(t, EeGeTexVal::FrameTexture) as usize;
    if frame != 0 && with(|w| w.objects.get(&frame).is_some_and(|o| o.alive)) {
        let fs = get(t, EeGeTexVal::FrameSurface) as usize;
        l += &format!(
            " | d{:x} {}{} fs{}",
            get(t, EeGeTexVal::FrameData),
            get(t, EeGeTexVal::FrameIsCompressed),
            get(t, EeGeTexVal::FrameIsPromoted),
            name(fs)
        );
        if fs != 0 {
            let s = surf(fs);
            let (w, h) = ((s.desc.width >> s.shift) as f32, (s.desc.height >> s.shift) as f32);
            l += &format!(" fw{} fh{} pf{}", bits(w * uv), bits(h * uv), s.desc.pixel_format);
        }
    }
    if surface != 0 {
        let s = surf(surface);
        l += &format!(" w{} h{}", s.desc.width >> s.shift, s.desc.height >> s.shift);
    }
    if frames.is_some() {
        // SAFETY: a live texture
        let first = unsafe { ee_ge_tex_frame_texture(p, 0) } as usize;
        l += &format!(" gft{} st{}", name(first), name(stage.texture as usize));
    }
    l
}

fn live_textures() -> Vec<usize> {
    with(|w| w.objects.iter().filter(|(_, o)| o.alive && o.surf.is_none()).map(|(&k, _)| k).collect())
}

fn object(token: &str) -> usize {
    token.trim_end_matches('+')[1..].parse().unwrap()
}

/// One "do" line's call.
fn act(line: &str) {
    let f: Vec<&str> = line.split(' ').collect();
    let t = if f.len() > 2 && f[2].starts_with('T') { object(f[2]) } else { 0 };
    match f[1] {
        "New" => {
            let m: usize = f[4][1..].parse().unwrap();
            let t = new_texture(MANAGER_BASE + m);
            emit(format!("do New = {} M{m}", name(t)));
        }
        "NewFromSurface" => {
            let d = parse_desc(&f[3..11].join(" "));
            let screen: usize = f[11][1..].parse().unwrap();
            let m: usize = f[12][1..].parse().unwrap();
            let s = new_surface(d, screen, 1);
            emit(format!("do NewFromSurface {} {} W{screen} M{m}", name(s), desc_text(&d)));
            let t = new_texture(MANAGER_BASE + m);
            // SAFETY: a live texture
            unsafe { EeGeTexture::construct_on_surface(tex(t), s) };
            emit(format!("= {}", name(t)));
            surf_dereference(s);
        }
        "Load" => {
            emit(line.to_string());
            let mut d = parse_desc(&f[3..11].join(" "));
            let screen: usize = f[11][1..].parse().unwrap();
            let mips = f[13] == "1";
            let drop: c_long = f[15].parse().unwrap();
            let format: c_ulong = f[17].parse().unwrap();
            // SAFETY: a live texture
            let r = unsafe { EeGeTexture::load(tex(t), FILE, &mut d, screen, mips, drop, format) };
            emit(format!("= {r} {}", desc_text(&d)));
        }
        "Dereference" => {
            emit(line.to_string());
            // SAFETY: a live texture
            unsafe { EeGeTexture::dereference(tex(t)) };
        }
        "Reference" => {
            emit(line.to_string());
            with(|w| w.objects.get_mut(&t).unwrap().refs += 1);
        }
        "Manage" => {
            emit(line.to_string());
            let m: usize = f[3][1..].parse().unwrap();
            with(|w| {
                w.objects.get_mut(&t).unwrap().refs += 1;
                w.caches[m - 1].insert(t);
            });
            // SAFETY: a live texture
            unsafe { EeGeTexture::set(tex(t), EeGeTexVal::IsManaged, 1) };
        }
        "SetStage" => {
            emit(line.to_string());
            let which: u32 = f[3].parse().unwrap();
            let mode: c_ulong = f[4].parse().unwrap();
            let p = tex(t);
            let field = match which {
                0 => EeGeTexStageField::AlphaMode,
                1 => EeGeTexStageField::ColorMode,
                2 => EeGeTexStageField::MagFilterMode,
                3 => EeGeTexStageField::MinFilterMode,
                4 => EeGeTexStageField::MipFilterMode,
                5 => {
                    // the driver's "bias" line follows its "do" line
                    let bias = f32::from_bits(u32::from_str_radix(peek().trim_start_matches("bias "), 16).unwrap());
                    with(|w| w.at += 1);
                    // SAFETY: a live texture
                    unsafe { ee_ge_tex_set_stage_bias(p, bias) };
                    return;
                }
                _ => EeGeTexStageField::TextureAddressingMode,
            };
            // SAFETY: a live texture
            unsafe { EeGeTexture::set_stage(p, field, mode) };
        }
        "SetStageTexture" => {
            emit(line.to_string());
            let other = if f[3] == "0" { 0 } else { object(f[3]) };
            // SAFETY: a live texture
            unsafe { ee_ge_tex_set_stage_texture(tex(t), other as *mut c_void) };
        }
        "SetFrame" => {
            emit(line.to_string());
            // SAFETY: a live texture
            unsafe { EeGeTexture::set_frame(tex(t), f[3].parse().unwrap()) };
        }
        "SetSurface" => {
            if f[3] == "same" {
                emit(line.to_string());
                let s = object(f[4]);
                // SAFETY: a live texture
                let r = unsafe { EeGeTexture::set_surface(tex(t), s) };
                emit(format!("= {r}"));
            } else {
                let d = parse_desc(&f[4..12].join(" "));
                let screen: usize = f[12][1..].parse().unwrap();
                let levels: c_long = f[14].parse().unwrap();
                let s = new_surface(d, screen, levels);
                emit(format!("do SetSurface {} {} {} W{screen} levels {levels}", name(t), name(s), desc_text(&d)));
                // SAFETY: a live texture
                let r = unsafe { EeGeTexture::set_surface(tex(t), s) };
                emit(format!("= {r}"));
                surf_dereference(s);
            }
        }
        "Lock" => {
            emit(line.to_string());
            let (mut m, mut bpr) = (0usize, 0 as c_ulong);
            // SAFETY: a live texture
            let r = unsafe { EeGeTexture::lock(tex(t), &mut m, &mut bpr) };
            let surface = get(t, EeGeTexVal::Surface) as usize;
            emit(format!("= {r} {} {bpr}", (m == surface << 16) as u8));
        }
        "Unlock" => {
            emit(line.to_string());
            // SAFETY: a live texture
            let r = unsafe { EeGeTexture::unlock(tex(t)) };
            emit(format!("= {r}"));
        }
        "Recreate" => {
            emit(line.to_string());
            // SAFETY: a live texture
            let r = unsafe { EeGeTexture::recreate(tex(t), f[3][1..].parse().unwrap()) };
            emit(format!("= {r}"));
        }
        "SetLogicalShift" => {
            emit(line.to_string());
            // SAFETY: a live texture
            unsafe { EeGeTexture::set_logical_shift(tex(t), f[3].parse().unwrap()) };
        }
        "GetAllocatedSize" => {
            // SAFETY: a live texture
            let n = unsafe { EeGeTexture::allocated_size(tex(t)) };
            emit(format!("do GetAllocatedSize {} = {n}", name(t)));
        }
        "Promote" => {
            emit(line.to_string());
            // SAFETY: a live texture
            let r = unsafe { EeGeTexture::promote(tex(t)) };
            emit(format!("= {r}"));
        }
        "Demote" => emit(line.to_string()),
        "SetData" => {
            emit(line.to_string());
            let v = usize::from_str_radix(f[3].trim_start_matches("0x").trim_start_matches("0X"), 16).unwrap();
            // SAFETY: a live texture
            unsafe { EeGeTexture::set(tex(t), EeGeTexVal::FrameData, v as isize) };
        }
        "SetHasChanged" | "ValidateChanges" => {
            emit(line.to_string());
            // SAFETY: a live texture
            unsafe { EeGeTexture::set(tex(t), EeGeTexVal::HasChanged, (f[1] == "SetHasChanged") as isize) };
        }
        "SetPack" => {
            emit(line.to_string());
            let p = tex(t);
            // SAFETY: a live texture
            unsafe {
                EeGeTexture::set(p, EeGeTexVal::FromPack, f[3].parse().unwrap());
                EeGeTexture::set(p, EeGeTexVal::PackMipLevels, f[4].parse().unwrap());
                EeGeTexture::set(p, EeGeTexVal::NumMipLevelsToDrop, f[5].parse().unwrap());
            }
        }
        "HoldFrame" => {
            emit(line.to_string());
            with(|w| w.objects.get_mut(&object(f[3])).unwrap().refs += 1);
        }
        "ManagerRemove" => {
            emit(line.to_string());
            let m: usize = f[2][1..].parse().unwrap();
            remove_texture(MANAGER_BASE + m, object(f[3]));
        }
        other => panic!("unknown action {other}"),
    }
}

/// Replays one sequence; its errors.
fn replay(lines: Vec<String>) -> Vec<String> {
    with(|w| *w = World { lines, ..World::default() });
    loop {
        let (at, n) = with(|w| (w.at, w.lines.len()));
        if at >= n || with(|w| !w.errors.is_empty()) {
            break;
        }
        let l = peek();
        if let Some(rest) = l.strip_prefix("file ") {
            let f: Vec<&str> = rest.split(' ').collect();
            let (w, h) = f[0].split_once('x').unwrap();
            let file = [w.parse().unwrap(), h.parse().unwrap(), f[2].parse().unwrap()];
            with(|x| {
                x.file = file;
                x.at += 1;
            });
        } else if l == "end" {
            with(|x| x.at += 1);
        } else if l.starts_with("tex ") {
            for t in live_textures() {
                emit(state_line(t));
            }
        } else if l.starts_with("left ") {
            let live = with(|w| w.objects.values().filter(|o| o.alive).count());
            emit(format!("left {live}"));
        } else if l.starts_with("do ") {
            act(&l);
        } else {
            let at = with(|w| w.at);
            with(|w| w.errors.push(format!("line {at}: unexpected [{l}]")));
        }
    }
    with(|w| std::mem::take(&mut w.errors))
}

fn sequences() -> Vec<Vec<String>> {
    let mut out: BTreeMap<u32, Vec<String>> = BTreeMap::new();
    for l in GOLDEN.lines() {
        let (s, rest) = l.split_once("| ").unwrap();
        out.entry(s.parse().unwrap()).or_default().push(rest.to_string());
    }
    out.into_values().collect()
}

#[test]
fn the_golden_sequences_replay_line_for_line() {
    let seqs = sequences();
    assert!(seqs.len() >= 20);
    let mut lines = 0;
    for (i, s) in seqs.into_iter().enumerate() {
        lines += s.len();
        let errors = replay(s);
        assert!(errors.is_empty(), "sequence {i}: {errors:?}");
    }
    assert!(lines > 1000);
}

#[test]
fn the_replay_fails_on_a_changed_line() {
    let mut s = sequences().swap_remove(0);
    let k = s.iter().position(|l| l.starts_with("tex ")).unwrap();
    s[k] = s[k].replace(" r1 ", " r9 ");
    assert!(!replay(s).is_empty());
}

#[test]
fn a_new_texture_is_one_frame_with_point_filtering_and_clamped_addressing() {
    with(|w| *w = World::default());
    let t = new_texture(MANAGER_BASE + 1);
    let p = tex(t);
    // SAFETY: a live texture
    let stage = unsafe { *ee_ge_tex_stage(p) };
    assert_eq!(stage.texture as usize, t);
    assert_eq!((stage.color_mode, stage.alpha_mode), (STAGE_OP_TEXTURE, STAGE_OP_TEXTURE));
    assert_eq!((stage.min_filter_mode, stage.mag_filter_mode, stage.mip_filter_mode), (0, 0, 1));
    assert_eq!(get(t, EeGeTexVal::FrameTexture) as usize, t);
    assert_eq!((get(t, EeGeTexVal::FrameFormat), get(t, EeGeTexVal::FrameCount)), (1, 1));
    assert_eq!(get(t, EeGeTexVal::HasChanged), 1);
    assert_eq!(get(t, EeGeTexVal::Manager) as usize, MANAGER_BASE + 1);
    // SAFETY: a live texture
    unsafe {
        assert!(ee_ge_tex_frames(p).is_null());
        assert!(ee_ge_tex_frame_texture(p, 3).is_null());
        assert_eq!(ee_ge_tex_frame_uv_size(p), 1.0);
    }
}

#[test]
fn a_grid_frame_moves_the_corner_and_a_stage_setter_marks_only_a_change() {
    with(|w| *w = World { lines: vec![String::new(); 64], ..World::default() });
    let t = new_texture(MANAGER_BASE + 1);
    let p = tex(t);
    // SAFETY: a live texture
    unsafe {
        tx(p).frames_per_row = 2;
        EeGeTexture::set_frame(p, 3);
        assert_eq!(*ee_ge_tex_corner(p), EeGeTexCoord { u: 0.5, v: 0.5 });
        EeGeTexture::set(p, EeGeTexVal::HasChanged, 0);
        EeGeTexture::set_stage(p, EeGeTexStageField::AlphaMode, STAGE_OP_TEXTURE);
        assert_eq!(get(t, EeGeTexVal::HasChanged), 0);
        EeGeTexture::set_stage(p, EeGeTexStageField::AlphaMode, 4);
        assert_eq!(get(t, EeGeTexVal::HasChanged), 1);
        assert_eq!((*ee_ge_tex_stage(p)).alpha_mode, 4);
    }
}

/// A world without a golden: the lines written are not checked.
fn quiet_world() {
    with(|w| *w = World { lines: vec![String::new(); 4096], ..World::default() });
}

/// A frame-format-0 texture of two frames (the texture and one frame texture).
fn two_frames() -> (usize, usize) {
    let t = new_texture(MANAGER_BASE + 1);
    let f = new_texture(MANAGER_BASE + 1);
    // SAFETY: live textures
    unsafe {
        tx(tex(t)).frame_format = 0;
        tx(tex(t)).frame_count = 2;
        tx(tex(t)).frames = Some(vec![t, f]);
    }
    (t, f)
}

#[test]
fn the_destructor_releases_the_frames_through_the_base_reference_count() {
    quiet_world();
    let (t, f) = two_frames();
    // GETexture::Dereference would keep a frame at one reference; the destructor's base release frees it
    base_dereference(t);
    assert!(!with(|w| w.objects[&f].alive));
}

#[test]
fn a_stage_setter_marks_only_the_frames_it_changes() {
    quiet_world();
    let (t, f) = two_frames();
    // SAFETY: live textures
    unsafe {
        tx(tex(f)).stage.alpha_mode = 4;
        tx(tex(t)).has_changed = false;
        tx(tex(f)).has_changed = false;
        EeGeTexture::set_stage(tex(t), EeGeTexStageField::AlphaMode, 4);
    }
    assert_eq!((get(t, EeGeTexVal::HasChanged), get(f, EeGeTexVal::HasChanged)), (1, 0));
}

#[test]
fn a_texture_pack_texture_keeps_mip_filtering_only_with_mip_levels() {
    quiet_world();
    let d = EeGeSurfaceDescription { kind: 5, width: 8, height: 8, pixel_depth: 32, ..Default::default() };
    let s = new_surface(d, 1, 1);
    with(|w| w.objects.get_mut(&s).unwrap().surf.as_mut().unwrap().shift = 1);
    let t = new_texture(MANAGER_BASE + 1);
    // SAFETY: a live texture
    unsafe {
        EeGeTexture::construct_on_surface(tex(t), s);
        EeGeTexture::set_stage(tex(t), EeGeTexStageField::MipFilterMode, texture::MIP_FILTER_NONE);
        assert_eq!((*ee_ge_tex_stage(tex(t))).mip_filter_mode, texture::MIP_FILTER_NONE);
        tx(tex(t)).create_mipmaps = true;
        EeGeTexture::set_stage(tex(t), EeGeTexStageField::MipFilterMode, texture::MIP_FILTER_NONE);
        assert_eq!((*ee_ge_tex_stage(tex(t))).mip_filter_mode, texture::MIP_FILTER_POINT);
    }
}
