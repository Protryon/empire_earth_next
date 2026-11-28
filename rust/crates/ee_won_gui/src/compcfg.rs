//! WONGUI's component configs' readers whole (R6 WONLobby round 25): `ComponentConfig`'s value readers, resource
//! look-ups, config map and paths and its instructions (ComponentConfigRead.cpp, ComponentConfigResource.cpp,
//! ComponentConfigValueReaders.cpp, ComponentConfigLoad.cpp's InitComponents), `ResourceConfig` (the resource
//! groups' maps, handlers, look-ups and the colour, font, sound, image and background reads: ResourceConfig*.cpp) and
//! `ResourceConfigTable` (the groups, the glue functions, the class-static control ids and allocators:
//! ResourceTableQueries.cpp, ResourceConfigTableMethods.cpp). Those units are deleted.
//!
//! The C++ classes keep their vtables (the subclasses override the hooks), the members the subclasses and the table
//! programs read (the component, the three flags, the error text, the parent and names of `ConfigObject`) and the
//! C++ objects a read makes or names (fonts, images, sounds, backgrounds, cursors, configs): those are host ops
//! (`EeWonCcOp`), the C++ end in include/ee/wonapi/WONCompCfg.inl. The Rust objects hold what the classes held:
//!
//! - a config ([`EeWonCompCfg`]): its map of named child configs (a reference each, ordered ignoring case as the
//!   original's `std::map<std::string, ..., StringLessNoCase>`), the current resource group (a reference) and the
//!   config parser and value parser the instruction being handled reads;
//! - a resource group ([`EeWonResCfg`]): the overwrite flag, the parse count and the seven maps of named strings,
//!   images, fonts, colours, backgrounds, ints and sounds, each value with the parse that made it;
//! - a table ([`EeWonResTable`]): its groups and glue functions; the control ids and allocators are one global each
//!   (the original's class statics).
//!
//! Every entry is `ee_won_cc_call` with a per-call frame of the C++ (`frame`): a read that fails with the original's
//! message hands it to the frame (`SetMessage`) and returns 1, the C++ throwing `ConfigObjectException` with it; a
//! host op that threw (a virtual, a nested read's exception, a decoder) leaves its exception in the frame and the
//! entry returns 2, the C++ rethrowing it. No Rust reference to an object lives across a host op (they re-enter).

use std::cell::{Cell, RefCell};
use std::ffi::{c_char, c_void};
use std::sync::Mutex;

use ee_won_net::cfgobject::EeWonCfgStringParser;
use ee_won_net::ffi_parse::{
    ee_won_parser_check_next_char, ee_won_parser_get_char, ee_won_parser_get_wchar, ee_won_parser_read_bool,
    ee_won_parser_read_int, ee_won_parser_read_string, ee_won_parser_read_wstring, ee_won_parser_skip_whitespace,
};
use ee_won_net::strcase::compare_no_case;

use crate::cfgkind::{self, EeWonCfgKind};
use crate::guicfg::{self, ExprSource};
use crate::guiconf::*;
use crate::guistring;

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = u32;
const WCHAR_SIZE: u32 = std::mem::size_of::<WChar>() as u32;

/// The host's ops, each one C++ statement (R6 WON round 34: the compound cases' decisions are the Rust's). `c` is a
/// C++ object (a `ConfigObject*` for a config or resource group, an `Image*`, ...); texts are a pointer and a length;
/// `ok` is cleared by an op that threw (its exception kept in the frame).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCcOp {
    /// `c->AddRef()` of a config or resource group.
    CfgAddRef = 0,
    /// `c->Release()` of a config or resource group.
    CfgRelease,
    /// `((Image*)c)->AddRef()`.
    ImageAddRef,
    /// `((Image*)c)->Release()`.
    ImageRelease,
    /// `((Font*)c)->AddRef()`.
    FontAddRef,
    /// `((Font*)c)->Release()`.
    FontRelease,
    /// `((Sound*)c)->AddRef()`.
    SoundAddRef,
    /// `((Sound*)c)->Release()`.
    SoundRelease,
    /// `((Component*)c)->AddRef()`.
    CompAddRef,
    /// `((Component*)c)->Release()`.
    CompRelease,
    /// The frame's message: `a` bytes, `b` long (the C++ throws `ConfigObjectException` with it).
    SetMessage,
    /// The Rust object of `ComponentConfig::mResourceConfigTable`, NULL for none.
    CurTable,
    /// `c->mParent` (a `ConfigObject*`, NULL for none).
    Parent,
    /// `c->mParent = a`.
    SetParent,
    /// `c->mName` appended to the `Vec<u8>` at `a` (`ee_won_cc_append`).
    Name,
    /// `c->mName` = `a`, `b` bytes.
    SetName,
    /// `c->mBaseName`, as `Name`.
    BaseName,
    /// Config `c`'s kind (`mKind`, an `EeWonCfgKind`).
    Kind,
    /// Config `c`'s flag `a` (0 `mHasSetWidth`, 1 `mHasSetHeight`, 2 `mHaveInitedComponents`).
    Flag,
    /// Flag `a` of config `c` = `b`.
    SetFlag,
    /// `c->mComponent->Left()`.
    CompLeft,
    /// `c->mComponent->Right()`.
    CompRight,
    /// `c->mComponent->Top()`.
    CompTop,
    /// `c->mComponent->Bottom()`.
    CompBottom,
    /// `c->mComponent->Width()`.
    CompWidth,
    /// `c->mComponent->Height()`.
    CompHeight,
    /// `c->mComponent->GetDesiredSize(w, h)` into the two ints at `a`.
    CompDesired,
    /// `c->mComponent->SetSize(a, b)`.
    CompSetSize,
    /// `c->mComponent->SetWantGrabBG(true)`.
    CompWantGrab,
    /// `c->mComponent->InitComponent(ComponentConfigInit(c))`.
    CompInit,
    /// `c->mComponent.get()`.
    CompOf,
    /// The part `a` (`EeWonCfgPart`) of component `c` (borrowed).
    CompPart,
    /// `WONShell::Op(c, GetFrameBorder, a)` of component `c`.
    CompFrameBorder,
    /// `CfgNewOfKind(a)`: a new config of kind `a`, as its `ComponentConfig*` (no reference).
    NewOfKind,
    /// The `ConfigObject*` of the `ComponentConfig*` `c`.
    AsObject,
    /// The game's skin made when there is none (`ee_won_skin_ensure`).
    SkinEnsure,
    /// `c->GetNamedValue(key, *d)`: `a`, `b` the key; true when it answered.
    VNamed,
    /// `c->CallNamedFunction(key, params, *out)` over the `EeWonCcCallArgs` at `a`.
    VCall,
    /// `c->GetPredefinedComponent(name, config, component)`: `a`, `b` the name, the two pointers at `d` (the
    /// config a `ComponentConfig*`).
    VPredefined,
    /// `c->SetComponent(a)`.
    VSetComponent,
    /// `c->Virtual24(key)`: `a`, `b` the key.
    V24,
    /// `c->ConfigObject::HandleInstruction(a, type)`: `b`, `d` the type.
    CoHandleInstruction,
    /// `c->ConfigObject::HandleValueInstruction(a, key, *e)`: `b`, `d` the key, `e` the `StringParser*`.
    CoHandleValue,
    /// A new empty `std::string` (the host's local).
    StrNew,
    /// `std::string` `c` appended to the `Vec<u8>` at `a`.
    StrTake,
    /// `std::string` `c` deleted.
    StrFree,
    /// `c->ReadGroupStart(*a, name, inherited)` into the `std::string`s `b` and `d`.
    ReadGroupStart,
    /// `c->ParseGroup(*a, ...)` with the four flags of `b` (bit 0 the first).
    ParseGroup,
    /// `c->ParseChild(*a, b, flag 0, flag 1)` with the flags of `d`.
    ParseChild,
    /// `c->IncludeGroup(*a, name)`: `b`, `d` the name.
    IncludeGroup,
    /// `((ConfigParser*)c)->AddWarning(text)`: `a`, `b` the text.
    AddWarning,
    /// Whether `c` is a FontConfig (`dynamic_cast`).
    IsFontCfg,
    /// FontConfig `c`'s `GetFont()` (borrowed, may be NULL).
    FontOf,
    /// Whether `c` is an ImageConfig (`dynamic_cast`).
    IsImageCfg,
    /// ImageConfig `c`'s `GetImage()` with a reference for the caller (NULL for none).
    ImageOf,
    /// ImageConfig `c`'s `GetRawImage()` with a reference for the caller (NULL for none).
    RawImageOf,
    /// The window's `DecodeImageRaw` of file `a`, `b` long, with a reference for the caller, NULL when it failed.
    DecodeRaw,
    /// The window manager's `DecodeDelayImage` of file `a`, `b` long, as `DecodeRaw`.
    DecodeDelay,
    /// `Window::GetDefaultWindow()->GetFont(FontDescriptor(face, d, e))`, the face `a`, `b` long: a reference.
    FontGet,
    /// A new `SoundDescriptor` (the host's local).
    SoundDescNew,
    /// `SoundDescriptor` `c`'s `SetSoundFlags(a, true)`.
    SoundDescFlags,
    /// `SoundDescriptor` `c`'s path = `a`, `b` bytes.
    SoundDescPath,
    /// The window manager's `DecodeSound(*c)`: a reference, NULL when it failed.
    SoundDecode,
    /// `SoundDescriptor` `c` deleted.
    SoundDescFree,
    /// `*(Background*)c` = `Background()`.
    BgReset,
    /// `((Background*)c)->mImage = (Image*)a`.
    BgImage,
    /// `((Background*)c)->mColor = a`.
    BgColor,
    /// `((Background*)c)->mBorderColor = a`.
    BgBorder,
    /// `((Background*)c)->mUseOffsets = a != 0`.
    BgOffsets,
    /// `((Background*)c)->mStretchImage = a != 0`.
    BgStretch,
    /// `((Background*)c)->mWantGrabBG = a != 0`.
    BgGrab,
    /// The config factory `a` called: its config with a reference (`ConfigObject*`, NULL for none).
    AllocConfig,
    /// The component factory `a` called: its component with a reference (NULL for none).
    AllocComp,
    /// The glue function `a` called with table `c`.
    Glue,
    /// A new `ResourceConfig` with a reference.
    NewRes,
    /// `*(GUIString*)c` = the `a` `wchar_t` units, `b` long.
    SetText,
    /// `*(SelectionColor*)c` = `SelectionColor(a, b)` (a foreground colour).
    SelFore,
    /// `*(SelectionColor*)c` = `SelectionColor(a, ForegroundSelectionMode_InvertOriginalColor)`.
    SelInvert,
    /// `*(SelectionColor*)c` = `SelectionColor(a, ForegroundSelectionMode_UseOriginalColor)`.
    SelOriginal,
    /// `WONShellNew(a, b, d, e)`: a new shell of kind `a` (no reference).
    NewShell,
}

/// What a `Ref` op counts.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCcKind {
    Config = 0,
    Res = 1,
    Image = 2,
    Font = 3,
    Sound = 4,
    Component = 5,
}

/// `CallNamedFunction`'s arguments.
#[repr(C)]
pub struct EeWonCcCallArgs {
    pub key: *const u8,
    pub key_len: usize,
    pub params: *const i32,
    pub count: usize,
    pub out: *mut i32,
}

/// A background as the readers make it (`Background`'s members; the image a held C++ `Image*`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeWonCcBackground {
    pub image: *mut c_void,
    pub color: i32,
    pub border_color: i32,
    pub use_offsets: bool,
    pub stretch_image: bool,
    pub want_grab_bg: bool,
}

/// A selection colour as `ReadSelectionColor` makes it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonCcSelColor {
    pub background: u32,
    pub foreground: u32,
    pub mode: i32,
}

/// The host: `frame` the C++ call's frame (NULL where no op can throw).
pub type EeWonCcHost = Option<
    unsafe extern "C" fn(
        frame: *mut c_void,
        op: EeWonCcOp,
        c: *mut c_void,
        a: isize,
        b: isize,
        d: isize,
        e: isize,
        ok: *mut bool,
    ) -> isize,
>;

/// Why a call failed.
#[derive(Debug, PartialEq, Eq)]
pub enum CcErr {
    /// `ConfigObjectException(text)`.
    Msg(Vec<u8>),
    /// A host op threw; the frame holds the exception.
    Host,
}
type R<T> = Result<T, CcErr>;

fn msg<T>(parts: &[&[u8]]) -> R<T> {
    Err(CcErr::Msg(parts.concat()))
}

/// A host and the frame of the C++ call being answered.
#[derive(Clone, Copy)]
pub struct Cx {
    host: EeWonCcHost,
    frame: *mut c_void,
}

impl Cx {
    fn op(&self, op: EeWonCcOp, c: *mut c_void, a: isize, b: isize, d: isize, e: isize) -> R<isize> {
        let mut ok = true;
        let f = self.host.expect("compcfg: no host");
        // SAFETY: the host's contract (its objects are the C++ ones handed to us)
        let r = unsafe { f(self.frame, op, c, a, b, d, e, &mut ok) };
        if ok {
            Ok(r)
        } else {
            Err(CcErr::Host)
        }
    }
    /// An op that cannot throw.
    fn call(&self, op: EeWonCcOp, c: *mut c_void, a: isize, b: isize, d: isize, e: isize) -> isize {
        self.op(op, c, a, b, d, e).unwrap_or(0)
    }
    fn hold(&self, kind: EeWonCcKind, p: *mut c_void) {
        if !p.is_null() {
            self.call(ref_op(kind, true), p, 0, 0, 0, 0);
        }
    }
    fn release(&self, kind: EeWonCcKind, p: *mut c_void) {
        if !p.is_null() {
            self.call(ref_op(kind, false), p, 0, 0, 0, 0);
        }
    }
    fn bytes(&self, op: EeWonCcOp, c: *mut c_void) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::new();
        self.call(op, c, &mut v as *mut Vec<u8> as isize, 0, 0, 0);
        v
    }
    /// The Rust object of config `c` (the original's `dynamic_cast<ComponentConfig*>`: every ComponentConfig makes
    /// its object, registered under its `ConfigObject*`), None for another object.
    fn cfg(&self, c: *mut c_void) -> Option<&EeWonCompCfg> {
        let p = registered(&CONFIGS, c) as *const EeWonCompCfg;
        // SAFETY: the config's own object, alive while the config is (the caller holds the config)
        unsafe { p.as_ref() }
    }
    /// The Rust object of resource group `c`.
    fn res(&self, c: *mut c_void) -> Option<&EeWonResCfg> {
        let p = registered(&GROUPS, c) as *const EeWonResCfg;
        // SAFETY: as `cfg`
        unsafe { p.as_ref() }
    }
    /// The component edge `k` (0 left, 1 right, 2 top, 3 bottom, 4 width, 5 height) of config `c`.
    fn edge(&self, c: *mut c_void, k: isize) -> i32 {
        const EDGES: [EeWonCcOp; 6] = [
            EeWonCcOp::CompLeft,
            EeWonCcOp::CompRight,
            EeWonCcOp::CompTop,
            EeWonCcOp::CompBottom,
            EeWonCcOp::CompWidth,
            EeWonCcOp::CompHeight,
        ];
        self.call(EDGES[k.clamp(0, 5) as usize], c, 0, 0, 0, 0) as i32
    }
    /// A host `std::string` local's bytes, the local deleted.
    fn take_str(&self, s: *mut c_void) -> Vec<u8> {
        let mut v: Vec<u8> = Vec::new();
        self.call(EeWonCcOp::StrTake, s, &mut v as *mut Vec<u8> as isize, 0, 0, 0);
        self.call(EeWonCcOp::StrFree, s, 0, 0, 0, 0);
        v
    }
    /// `c->ReadGroupStart(*parser, name, inherited)`: whether a group starts, its name and inherited group (the
    /// host's two string locals given back however the read ends).
    fn read_group_start(&self, c: *mut c_void, parser: isize) -> R<(bool, Vec<u8>, Vec<u8>)> {
        let n = self.call(EeWonCcOp::StrNew, std::ptr::null_mut(), 0, 0, 0, 0);
        let i = self.call(EeWonCcOp::StrNew, std::ptr::null_mut(), 0, 0, 0, 0);
        let r = self.op(EeWonCcOp::ReadGroupStart, c, parser, n, i, 0);
        let (name, inherited) = (self.take_str(n as *mut c_void), self.take_str(i as *mut c_void));
        Ok((r? != 0, name, inherited))
    }
    fn table(&self) -> Option<&EeWonResTable> {
        let p = self.call(EeWonCcOp::CurTable, std::ptr::null_mut(), 0, 0, 0, 0) as *const EeWonResTable;
        // SAFETY: the static SmartPtr's table, alive during the parse
        unsafe { p.as_ref() }
    }
}

/// The ops that count a reference of an object of `kind` (a resource group is a config object).
fn ref_op(kind: EeWonCcKind, add: bool) -> EeWonCcOp {
    use EeWonCcOp as O;
    match (kind, add) {
        (EeWonCcKind::Config | EeWonCcKind::Res, true) => O::CfgAddRef,
        (EeWonCcKind::Config | EeWonCcKind::Res, false) => O::CfgRelease,
        (EeWonCcKind::Image, true) => O::ImageAddRef,
        (EeWonCcKind::Image, false) => O::ImageRelease,
        (EeWonCcKind::Font, true) => O::FontAddRef,
        (EeWonCcKind::Font, false) => O::FontRelease,
        (EeWonCcKind::Sound, true) => O::SoundAddRef,
        (EeWonCcKind::Sound, false) => O::SoundRelease,
        (EeWonCcKind::Component, true) => O::CompAddRef,
        (EeWonCcKind::Component, false) => O::CompRelease,
    }
}

/// The configs' and the resource groups' Rust objects by their C++ `ConfigObject*` (each made by the object's
/// constructor and forgotten by its destructor).
static CONFIGS: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());
static GROUPS: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());

fn registry(m: &Mutex<Vec<(usize, usize)>>) -> std::sync::MutexGuard<'_, Vec<(usize, usize)>> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}
fn register(m: &Mutex<Vec<(usize, usize)>>, this_: *mut c_void, o: usize) {
    let mut r = registry(m);
    match r.binary_search_by_key(&(this_ as usize), |e| e.0) {
        Ok(i) => r[i].1 = o,
        Err(i) => r.insert(i, (this_ as usize, o)),
    }
}
fn unregister(m: &Mutex<Vec<(usize, usize)>>, this_: *mut c_void, o: usize) {
    let mut r = registry(m);
    if let Ok(i) = r.binary_search_by_key(&(this_ as usize), |e| e.0) {
        if r[i].1 == o {
            r.remove(i);
        }
    }
}
fn registered(m: &Mutex<Vec<(usize, usize)>>, this_: *mut c_void) -> usize {
    if this_.is_null() {
        return 0;
    }
    let r = registry(m);
    r.binary_search_by_key(&(this_ as usize), |e| e.0).map_or(0, |i| r[i].1)
}

/// Appends bytes to the `Vec<u8>` at `v` (the host's way to hand back a name).
///
/// # Safety
/// `v` is a `Vec<u8>` handed out by this module; `bytes` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_append(v: *mut c_void, bytes: *const u8, length: usize) {
    if length != 0 {
        // SAFETY: the contract
        unsafe { (*(v as *mut Vec<u8>)).extend_from_slice(std::slice::from_raw_parts(bytes, length)) };
    }
}

// ---- a map ordered ignoring case (std::map<std::string, T, StringLessNoCase>)

/// The entries in the original's order; a found key keeps the spelling it was inserted with.
pub struct NoCaseMap<V> {
    entries: Vec<(Vec<u8>, V)>,
}

impl<V> Default for NoCaseMap<V> {
    fn default() -> Self {
        Self::new()
    }
}

impl<V> NoCaseMap<V> {
    pub const fn new() -> Self {
        NoCaseMap { entries: Vec::new() }
    }
    fn search(&self, key: &[u8]) -> Result<usize, usize> {
        self.entries.binary_search_by(|(k, _)| compare_no_case(k, key).cmp(&0))
    }
    pub fn get(&self, key: &[u8]) -> Option<&V> {
        self.search(key).ok().map(|i| &self.entries[i].1)
    }
    /// `insert`: false (and nothing changed) when the key was there.
    pub fn insert(&mut self, key: &[u8], value: V) -> bool {
        match self.search(key) {
            Ok(_) => false,
            Err(i) => {
                self.entries.insert(i, (key.to_vec(), value));
                true
            }
        }
    }
    /// `operator[] =`: the old value back.
    pub fn set(&mut self, key: &[u8], value: V) -> Option<V> {
        match self.search(key) {
            Ok(i) => Some(std::mem::replace(&mut self.entries[i].1, value)),
            Err(i) => {
                self.entries.insert(i, (key.to_vec(), value));
                None
            }
        }
    }
    pub fn get_mut(&mut self, key: &[u8]) -> Option<&mut V> {
        match self.search(key) {
            Ok(i) => Some(&mut self.entries[i].1),
            Err(_) => None,
        }
    }
    pub fn at(&self, i: usize) -> Option<(&[u8], &V)> {
        self.entries.get(i).map(|(k, v)| (k.as_slice(), v))
    }
    pub fn len(&self) -> usize {
        self.entries.len()
    }
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }
    pub fn take(&mut self) -> Vec<(Vec<u8>, V)> {
        std::mem::take(&mut self.entries)
    }
}

// ---- the value parser

unsafe extern "C" fn byte_sink(c: *mut c_void, b: *const u8, n: usize) {
    // SAFETY: `c` is our Vec<u8>, `b` has `n` bytes
    unsafe { ee_won_cc_append(c, b, n) }
}
unsafe extern "C" fn unit_sink(c: *mut c_void, u: *const u32, n: usize) {
    if n != 0 {
        // SAFETY: `c` is our Vec<u32>, `u` has `n` units
        unsafe { (*(c as *mut Vec<u32>)).extend_from_slice(std::slice::from_raw_parts(u, n)) };
    }
}

/// The C++ `StringParser` a read takes its value from (ee_won_net's parser over its text and cursor).
#[derive(Clone, Copy)]
pub struct Sp(pub *mut EeWonCfgStringParser);

impl Sp {
    #[allow(clippy::mut_from_ref)] // the C++ parser behind the copyable handle, as the C++ readers' pointer member
    fn p(&self) -> &mut EeWonCfgStringParser {
        // SAFETY: the C++ parser, alive for the instruction (not otherwise referenced while a read runs)
        unsafe { &mut *self.0 }
    }
    fn unit(&self) -> u32 {
        if self.p().wide {
            WCHAR_SIZE
        } else {
            1
        }
    }
    pub fn skip_ws(&self) {
        let p = self.p();
        let u = self.unit();
        // SAFETY: the parser's text and cursor
        unsafe { ee_won_parser_skip_whitespace(p.text, u, &mut p.pos) }
    }
    pub fn get_char(&self, off: i32) -> u8 {
        let p = self.p();
        // SAFETY: as skip_ws
        unsafe { ee_won_parser_get_char(p.text, self.unit(), p.pos, off) }
    }
    pub fn get_wchar(&self, off: i32) -> u32 {
        let p = self.p();
        // SAFETY: as skip_ws
        unsafe { ee_won_parser_get_wchar(p.text, self.unit(), p.pos, off) }
    }
    pub fn advance(&self, n: i32) {
        let p = self.p();
        p.pos = p.pos.wrapping_add(n);
    }
    pub fn end(&self) -> bool {
        self.get_wchar(0) == 0
    }
    pub fn check(&self, ch: u8) -> bool {
        let p = self.p();
        let u = self.unit();
        // SAFETY: as skip_ws
        unsafe { ee_won_parser_check_next_char(p.text, u, &mut p.pos, i32::from(ch)) }
    }
    pub fn read_string(&self, stop: bool) -> Vec<u8> {
        let p = self.p();
        let u = self.unit();
        let mut v: Vec<u8> = Vec::new();
        // SAFETY: as skip_ws; the sink appends to `v`
        unsafe {
            ee_won_parser_read_string(p.text, u, &mut p.pos, stop, byte_sink, &mut v as *mut Vec<u8> as *mut c_void)
        };
        v
    }
    /// `ReadString(std::wstring&)`: the units as `wchar_t`s.
    pub fn read_wstring(&self, stop: bool) -> Vec<u32> {
        let p = self.p();
        let u = self.unit();
        let mut v: Vec<u32> = Vec::new();
        // SAFETY: as read_string
        unsafe {
            ee_won_parser_read_wstring(p.text, u, &mut p.pos, stop, unit_sink, &mut v as *mut Vec<u32> as *mut c_void)
        };
        v.into_iter().map(|c| c as WChar as u32).collect()
    }
    pub fn read_int(&self) -> Option<i32> {
        let p = self.p();
        let u = self.unit();
        let mut v = 0i32;
        // SAFETY: as skip_ws
        unsafe { ee_won_parser_read_int(p.text, u, &mut p.pos, &mut v) }.then_some(v)
    }
    pub fn read_bool(&self) -> Option<bool> {
        let p = self.p();
        let u = self.unit();
        let mut v = false;
        // SAFETY: as skip_ws
        unsafe { ee_won_parser_read_bool(p.text, u, &mut p.pos, &mut v) }.then_some(v)
    }
}

fn ensure(sp: Sp, ch: u8) -> R<()> {
    if sp.check(ch) {
        Ok(())
    } else if ch == b',' {
        msg(&[b"Expecting ','"])
    } else {
        msg(&[b"Expecting ')'"])
    }
}

/// `PlatStrICmp(a.c_str(), b) == 0` (UTF-8 case folding, to the first NUL).
fn icase(a: &[u8], b: &[u8]) -> bool {
    let a = &a[..a.iter().position(|&c| c == 0).unwrap_or(a.len())];
    ee_base::plat_core::utf8::compare_folded(a, b) == 0
}

/// The index of `units` in a guiconf name table (`ee_won_gui_cfg_name`).
fn name_idx(table: i32, units: &[u32]) -> i32 {
    let w: Vec<WChar> = units.iter().map(|&u| u as WChar).collect();
    // SAFETY: `w` has the units
    unsafe { ee_won_gui_cfg_name(table, w.as_ptr() as *const c_void, w.len() as u16, WCHAR_SIZE) }
}

/// `GUIString(units)` -> `std::string` (UTF-8, D6).
fn narrow(units: &[u32]) -> Vec<u8> {
    guistring::narrow(units, WCHAR_SIZE)
}
fn widen(bytes: &[u8]) -> Vec<u32> {
    guistring::widen(bytes, WCHAR_SIZE)
}

// ---- the resource folder (the original's static std::string)

static RESOURCE_FOLDER: Mutex<Vec<u8>> = Mutex::new(Vec::new());

/// `SetResourceFolder`: a '/' added unless it ends in a slash or a backslash (an empty folder reads its last byte
/// as the original's `[length-1]`, past the start: here it gets the '/').
pub fn set_resource_folder(folder: &[u8]) {
    let mut f = folder.to_vec();
    match f.last() {
        Some(b'/') | Some(b'\\') => {}
        _ => f.push(b'/'),
    }
    *RESOURCE_FOLDER.lock().unwrap_or_else(|e| e.into_inner()) = f;
}

/// `GetResourceFile`: a drive path kept, "./" stripped, else the resource folder first (the UNC and "html" tests
/// only ran for names under two bytes, where they never match).
pub fn resource_file(file: &[u8]) -> Vec<u8> {
    if file.len() >= 2 {
        if file[1] == b':' {
            return file.to_vec();
        }
        if file[0] == b'.' && (file[1] == b'/' || file[1] == b'\\') {
            return file[2..].to_vec();
        }
    }
    let mut out = RESOURCE_FOLDER.lock().unwrap_or_else(|e| e.into_inner()).clone();
    out.extend_from_slice(file);
    out
}

// ---- ResourceConfig

/// The value of a resource and the parse that made it.
type Gen<T> = (T, i32);

/// A resource group (`ResourceConfig`'s members).
pub struct EeWonResCfg {
    this_: *mut c_void,
    allow_overwrite: Cell<bool>,
    parse_count: Cell<i32>,
    strings: RefCell<NoCaseMap<Gen<Vec<u32>>>>,
    images: RefCell<NoCaseMap<Gen<*mut c_void>>>,
    fonts: RefCell<NoCaseMap<Gen<*mut c_void>>>,
    colors: RefCell<NoCaseMap<Gen<u32>>>,
    backgrounds: RefCell<NoCaseMap<Gen<EeWonCcBackground>>>,
    ints: RefCell<NoCaseMap<Gen<i32>>>,
    sounds: RefCell<NoCaseMap<Gen<*mut c_void>>>,
}

/// `Background()`: the default colour 0x8000000d, no border, tiled with offsets.
pub fn default_background() -> EeWonCcBackground {
    EeWonCcBackground { color: 0x8000_000d_u32 as i32, ..color_background(-1) }
}
fn color_background(color: i32) -> EeWonCcBackground {
    EeWonCcBackground {
        image: std::ptr::null_mut(),
        color,
        border_color: -1,
        use_offsets: true,
        stretch_image: false,
        want_grab_bg: false,
    }
}

/// The kinds of resource a group holds, in the original's member order.
#[derive(Clone, Copy, PartialEq, Eq)]
enum ResKind {
    String,
    Image,
    Font,
    Color,
    Background,
    Sound,
}

impl ResKind {
    fn name(self) -> &'static [u8] {
        match self {
            ResKind::String => b"String",
            ResKind::Image => b"Image",
            ResKind::Font => b"Font",
            ResKind::Color => b"Color",
            ResKind::Background => b"Background",
            ResKind::Sound => b"Sound",
        }
    }
}

/// `ResourceConfig_AddToMap`: the key read; a new entry made with the default value and this parse (true: read
/// its value), an entry of an earlier parse kept (false), one of this parse an error.
fn add_to_map<T>(sp: Sp, kind: ResKind, map: &RefCell<NoCaseMap<Gen<T>>>, gen: i32, default: T) -> R<Option<Vec<u8>>> {
    let key = sp.read_string(true);
    if key.is_empty() {
        return msg(&[b"Invalid resource key."]);
    }
    let mut m = map.borrow_mut();
    if m.insert(&key, (default, gen)) {
        return Ok(Some(key));
    }
    if m.get(&key).map(|(_, g)| *g) == Some(gen) {
        return msg(&[kind.name(), b" resource already exists: ", &key]);
    }
    Ok(None)
}

impl EeWonResCfg {
    fn new(this_: *mut c_void) -> Self {
        EeWonResCfg {
            this_,
            allow_overwrite: Cell::new(false),
            parse_count: Cell::new(0),
            strings: RefCell::default(),
            images: RefCell::default(),
            fonts: RefCell::default(),
            colors: RefCell::default(),
            backgrounds: RefCell::default(),
            ints: RefCell::default(),
            sounds: RefCell::default(),
        }
    }

    /// `HandleValueInstruction`: a resource line to its handler, CONTROLID ignored, the rest to `ConfigObject`'s.
    fn handle_value(&self, cx: &Cx, parser: isize, key: &[u8], sp: Sp) -> R<bool> {
        let k = key_index(EE_WON_CFG_KEYS_RESOURCECONFIG_HANDLEVALUEINSTRUCTION_KEY, key);
        let gen = self.parse_count.get();
        match k {
            0 | 1 => {
                if let Some(key) = add_to_map(sp, ResKind::String, &self.strings, gen, Vec::new())? {
                    let text = sp.read_wstring(false);
                    if let Some(v) = self.strings.borrow_mut().get_mut(&key) {
                        v.0 = text;
                    }
                }
            }
            2 | 3 => {
                if let Some(key) = add_to_map(sp, ResKind::Image, &self.images, gen, std::ptr::null_mut())? {
                    let img = safe_read_image(cx, sp)?;
                    self.put_held(cx, &self.images, &key, img, EeWonCcKind::Image);
                }
            }
            4 | 5 => {
                if let Some(key) = add_to_map(sp, ResKind::Sound, &self.sounds, gen, std::ptr::null_mut())? {
                    let s = safe_read_sound(cx, sp)?;
                    self.put_held(cx, &self.sounds, &key, s, EeWonCcKind::Sound);
                }
            }
            6 => {
                if let Some(key) = add_to_map(sp, ResKind::Font, &self.fonts, gen, std::ptr::null_mut())? {
                    let f = safe_read_font(cx, sp)?;
                    self.put_held(cx, &self.fonts, &key, f, EeWonCcKind::Font);
                }
            }
            7 => {
                if let Some(key) = add_to_map(sp, ResKind::Color, &self.colors, gen, 0)? {
                    let c = safe_read_color(sp)?;
                    if let Some(v) = self.colors.borrow_mut().get_mut(&key) {
                        v.0 = c as u32;
                    }
                }
            }
            8 => {
                if let Some(key) = add_to_map(sp, ResKind::Background, &self.backgrounds, gen, default_background())? {
                    let b = safe_read_background(cx, sp)?;
                    let old = self.backgrounds.borrow_mut().get_mut(&key).map(|v| std::mem::replace(&mut v.0, b));
                    match old {
                        Some(o) => cx.release(EeWonCcKind::Image, o.image),
                        None => cx.release(EeWonCcKind::Image, b.image),
                    }
                }
            }
            9 => {}
            _ => {
                return Ok(cx.op(
                    EeWonCcOp::CoHandleValue,
                    self.this_,
                    parser,
                    key.as_ptr() as isize,
                    key.len() as isize,
                    sp.0 as isize,
                )? != 0)
            }
        }
        Ok(true)
    }

    /// A read value (with its reference) into an entry made for it (the SmartPtr assignment: the old one given
    /// back).
    fn put_held(
        &self,
        cx: &Cx,
        map: &RefCell<NoCaseMap<Gen<*mut c_void>>>,
        key: &[u8],
        p: *mut c_void,
        kind: EeWonCcKind,
    ) {
        let old = map.borrow_mut().get_mut(key).map(|v| std::mem::replace(&mut v.0, p));
        match old {
            Some(o) => cx.release(kind, o),
            None => cx.release(kind, p),
        }
    }

    fn get_string(&self, key: &[u8]) -> Option<Vec<u32>> {
        self.strings.borrow().get(key).map(|v| v.0.clone())
    }
    fn get_ptr(map: &RefCell<NoCaseMap<Gen<*mut c_void>>>, key: &[u8]) -> Option<*mut c_void> {
        map.borrow().get(key).map(|v| v.0)
    }

    /// `SafeGet*`: "<type> resource not found: <key> (ResourceGroup: <group>)" when missing.
    fn not_found<T>(&self, cx: &Cx, kind: ResKind, key: &[u8]) -> R<T> {
        let group = cx.bytes(EeWonCcOp::Name, self.this_);
        msg(&[kind.name(), b" resource not found: ", key, b" (ResourceGroup: ", &group, b")"])
    }

    /// Every value given back, in the original's member order.
    fn free(&self, cx: &Cx) {
        for (_, v) in self.sounds.borrow_mut().take() {
            cx.release(EeWonCcKind::Sound, v.0);
        }
        self.ints.borrow_mut().take();
        for (_, v) in self.backgrounds.borrow_mut().take() {
            cx.release(EeWonCcKind::Image, v.0.image);
        }
        self.colors.borrow_mut().take();
        for (_, v) in self.fonts.borrow_mut().take() {
            cx.release(EeWonCcKind::Font, v.0);
        }
        for (_, v) in self.images.borrow_mut().take() {
            cx.release(EeWonCcKind::Image, v.0);
        }
        self.strings.borrow_mut().take();
    }
}

fn hex_digit(c: u8) -> i32 {
    if c.is_ascii_digit() {
        return i32::from(c - b'0');
    }
    let u = c.to_ascii_uppercase();
    if (b'A'..=b'F').contains(&u) {
        i32::from(u - b'A') + 10
    } else {
        -1
    }
}

const STD_COLORS: [&[u8]; 17] = [
    b"3DDarkShadow",
    b"3DFace",
    b"3DHilight",
    b"3DShadow",
    b"Scrollbar",
    b"ButtonText",
    b"GrayText",
    b"Hilight",
    b"HilightText",
    b"ToolTipBack",
    b"ToolTipText",
    b"MenuBack",
    b"MenuText",
    b"Back",
    b"Text",
    b"Link",
    b"LinkDown",
];

/// `SafeReadColor`: (STD,<name>), (DEC,r,g,b) or six hex digits; an unknown standard colour names "STD" (the
/// original's message), another type in parentheses is 0. A DEC component that does not read is 0 (the original's
/// was unset).
pub fn safe_read_color(sp: Sp) -> R<i32> {
    sp.skip_ws();
    if sp.get_char(0) == b'(' {
        sp.advance(1);
        let ty = sp.read_string(false);
        let mut color = 0i32;
        if icase(&ty, b"STD") {
            ensure(sp, b',')?;
            let name = sp.read_string(false);
            match STD_COLORS.iter().position(|n| icase(&name, n)) {
                Some(i) => color = (i as i32) | (0x8000_0000_u32 as i32),
                None => return msg(&[b"Invalid standard color: ", &ty]),
            }
        } else if icase(&ty, b"DEC") {
            ensure(sp, b',')?;
            let r = sp.read_int().unwrap_or(0);
            ensure(sp, b',')?;
            let g = sp.read_int().unwrap_or(0);
            ensure(sp, b',')?;
            let b = sp.read_int().unwrap_or(0);
            color = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
        }
        ensure(sp, b')')?;
        return Ok(color);
    }
    let mut color = 0i32;
    for _ in 0..6 {
        color <<= 4;
        let d = hex_digit(sp.get_char(0));
        if d < 0 {
            return msg(&[b"Invalid color specification."]);
        }
        color |= d;
        sp.advance(1);
    }
    Ok(color)
}

/// `SafeReadFontStyle`: Bold 1, Italic 2, Underline 4, Plain 0, to an empty word.
pub fn safe_read_font_style(sp: Sp) -> R<i32> {
    let mut style = 0;
    loop {
        let name = sp.read_string(true);
        if name.is_empty() {
            return Ok(style);
        }
        if icase(&name, b"Bold") {
            style |= 1;
        } else if icase(&name, b"Italic") {
            style |= 2;
        } else if icase(&name, b"Plain") {
        } else if icase(&name, b"Underline") {
            style |= 4;
        } else {
            return msg(&[b"Unknown font style: ", &name]);
        }
    }
}

/// `SafeReadFont`: face, style, size (> 0) -> the window's font (a reference).
pub fn safe_read_font(cx: &Cx, sp: Sp) -> R<*mut c_void> {
    let face = sp.read_string(false);
    ensure(sp, b',')?;
    let style = safe_read_font_style(sp)?;
    ensure(sp, b',')?;
    let size = match sp.read_int() {
        Some(s) if s > 0 => s,
        _ => return msg(&[b"Invalid font size."]),
    };
    Ok(cx.op(
        EeWonCcOp::FontGet,
        std::ptr::null_mut(),
        face.as_ptr() as isize,
        face.len() as isize,
        style as isize,
        size as isize,
    )? as *mut c_void)
}

/// `SafeReadSound`: a path or (path, flags) with Preload 1, Music 2, Muted 4, decoded (a reference).
pub fn safe_read_sound(cx: &Cx, sp: Sp) -> R<*mut c_void> {
    sp.skip_ws();
    let path;
    let (mut flags, mut has_flags) = (0, false);
    if sp.get_char(0) == b'(' {
        sp.advance(1);
        path = sp.read_string(false);
        ensure(sp, b',')?;
        loop {
            let flag = sp.read_string(true);
            if flag.is_empty() {
                break;
            }
            if icase(&flag, b"Preload") {
                flags |= 1;
            } else if icase(&flag, b"Music") {
                flags |= 2;
            } else if icase(&flag, b"Muted") {
                flags |= 4;
            } else {
                return msg(&[b"Unknown sound flag: ", &flag]);
            }
        }
        has_flags = true;
        ensure(sp, b')')?;
    } else {
        path = sp.read_string(false);
    }
    let file = resource_file(&path);
    // the game's sounds are soundobj.rs's (R6 WON GUI round 41): `DecodeSound` with one reference
    if crate::compobj::is_game() {
        let s = crate::soundobj::decode(if has_flags { flags as i32 } else { 0 }, &file);
        if s.is_null() {
            return msg(&[b"Failed to decode sound: ", &file]);
        }
        // SAFETY: the new sound (a RefCount head like a text's)
        unsafe { crate::guistrobj::gstr_ref(s as *const crate::guistrobj::EeWonGstr) };
        return Ok(s as *mut c_void);
    }
    // the original's local SoundDescriptor: its flags (when the value had them), its path, the decode
    let desc = cx.call(EeWonCcOp::SoundDescNew, std::ptr::null_mut(), 0, 0, 0, 0) as *mut c_void;
    if has_flags {
        cx.call(EeWonCcOp::SoundDescFlags, desc, flags, 0, 0, 0);
    }
    cx.call(EeWonCcOp::SoundDescPath, desc, file.as_ptr() as isize, file.len() as isize, 0, 0);
    let s = cx.op(EeWonCcOp::SoundDecode, desc, 0, 0, 0, 0);
    cx.call(EeWonCcOp::SoundDescFree, desc, 0, 0, 0, 0);
    let s = s? as *mut c_void;
    if s.is_null() {
        return msg(&[b"Failed to decode sound: ", &file]);
    }
    Ok(s)
}

fn decode(cx: &Cx, file: &[u8], raw: bool) -> R<*mut c_void> {
    let op = if raw { EeWonCcOp::DecodeRaw } else { EeWonCcOp::DecodeDelay };
    Ok(cx.op(op, std::ptr::null_mut(), file.as_ptr() as isize, file.len() as isize, 0, 0)? as *mut c_void)
}

/// `SafeReadImage`: a path decoded as a delay-loaded image (a reference).
pub fn safe_read_image(cx: &Cx, sp: Sp) -> R<*mut c_void> {
    let path = sp.read_string(false);
    let img = decode(cx, &resource_file(&path), false)?;
    if img.is_null() {
        return msg(&[b"Failed to decode image: ", &path]);
    }
    Ok(img)
}

/// `SafeReadBackground`: COLOR, IMAGE, STRETCHIMAGE, WATERMARK (no offsets) or NONE (its image held).
pub fn safe_read_background(cx: &Cx, sp: Sp) -> R<EeWonCcBackground> {
    let ty = sp.read_string(false);
    let image = |cx: &Cx| -> R<EeWonCcBackground> {
        ensure(sp, b',')?;
        let img = safe_read_image(cx, sp)?;
        Ok(EeWonCcBackground { image: img, ..color_background(-1) })
    };
    if icase(&ty, b"COLOR") {
        ensure(sp, b',')?;
        return Ok(color_background(safe_read_color(sp)?));
    }
    if icase(&ty, b"IMAGE") {
        return image(cx);
    }
    if icase(&ty, b"STRETCHIMAGE") {
        return Ok(EeWonCcBackground { stretch_image: true, ..image(cx)? });
    }
    if icase(&ty, b"WATERMARK") {
        return Ok(EeWonCcBackground { use_offsets: false, ..image(cx)? });
    }
    if icase(&ty, b"NONE") {
        return Ok(color_background(-1));
    }
    msg(&[b"Invalid background specification"])
}

// ---- ResourceConfigTable

/// A table's groups (a reference each) and its glue functions (ordered by address, as the original's std::set).
pub struct EeWonResTable {
    this_: *mut c_void,
    groups: RefCell<NoCaseMap<*mut c_void>>,
    glue: RefCell<Vec<usize>>,
}

static CONTROL_IDS: Mutex<NoCaseMap<i32>> = Mutex::new(NoCaseMap::new());
static ALLOCATORS: Mutex<NoCaseMap<(Make, Make)>> = Mutex::new(NoCaseMap::new());

/// What an allocator makes its config or component with: a C++ factory (`AddAllocator`'s function pointers; 0 for
/// none), or for the GUI's own registrations (guireg.rs, R6 WON lobby and network round 39) a config of a kind, a
/// shell of a kind with its constructor's arguments, a WON tab control on the game's skin, a copy of a skin template.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Make {
    Fn(usize),
    Kind(EeWonCfgKind),
    Shell(crate::shell::EeWonShellKind, [isize; 3]),
    SkinTab,
    Skin(crate::skinobj::EeWonSkinGet),
}

fn control_ids() -> std::sync::MutexGuard<'static, NoCaseMap<i32>> {
    CONTROL_IDS.lock().unwrap_or_else(|e| e.into_inner())
}
fn allocators() -> std::sync::MutexGuard<'static, NoCaseMap<(Make, Make)>> {
    ALLOCATORS.lock().unwrap_or_else(|e| e.into_inner())
}

/// `ResourceConfigTable::AddAllocator`: the name's config and component factories (the first one with a config
/// factory stays).
pub(crate) fn add_allocator(name: &[u8], config: Make, component: Make) {
    let mut m = allocators();
    if m.get(name).is_none_or(|&(cf, _)| cf == Make::Fn(0)) {
        m.set(name, (config, component));
    }
}

/// `ResourceConfigTable::RegisterControlId`.
pub(crate) fn register_control_id(name: &[u8], id: i32) {
    control_ids().set(name, id);
}

impl EeWonResTable {
    /// `AddConfig`: the named group, made (and named) when absent; whether it existed.
    fn add_config(&self, cx: &Cx, name: &[u8]) -> (*mut c_void, bool) {
        if let Some(&g) = self.groups.borrow().get(name) {
            return (g, true);
        }
        let g = cx.call(EeWonCcOp::NewRes, std::ptr::null_mut(), 0, 0, 0, 0) as *mut c_void;
        cx.call(EeWonCcOp::SetName, g, name.as_ptr() as isize, name.len() as isize, 0, 0);
        self.groups.borrow_mut().insert(name, g);
        (g, false)
    }
    /// `SafeGetConfig`: "Resource group not found: <name>" when missing.
    fn safe_get(&self, name: &[u8]) -> R<*mut c_void> {
        match self.groups.borrow().get(name) {
            Some(&g) => Ok(g),
            None => msg(&[b"Resource group not found: ", name]),
        }
    }
    /// `SafeGlueResources`: every glue function with the table, in their order.
    fn glue_all(&self, cx: &Cx) -> R<()> {
        let mut i = 0;
        loop {
            let f = match self.glue.borrow().get(i) {
                Some(&f) => f,
                None => return Ok(()),
            };
            cx.op(EeWonCcOp::Glue, self.this_, f as isize, 0, 0, 0)?;
            i += 1;
        }
    }
    fn clear_resources(&self, cx: &Cx) {
        let groups = self.groups.borrow_mut().take();
        for (_, g) in groups {
            cx.release(EeWonCcKind::Res, g);
        }
    }
}

/// `AllocateConfig`: a new config and component of a registered type (a reference each), None for an unknown one.
fn allocate(cx: &Cx, ty: &[u8]) -> R<Option<[*mut c_void; 2]>> {
    let a = allocators().get(ty).copied();
    let Some((cf, pf)) = a else { return Ok(None) };
    // the config made first, then the component; the config given back when the component's factory threw
    let config = match cf {
        Make::Kind(k) => {
            let c = cx.call(EeWonCcOp::NewOfKind, std::ptr::null_mut(), k as isize, 0, 0, 0) as *mut c_void;
            let o = cx.call(EeWonCcOp::AsObject, c, 0, 0, 0, 0) as *mut c_void;
            cx.hold(EeWonCcKind::Config, o);
            o
        }
        Make::Fn(f) => cx.op(EeWonCcOp::AllocConfig, std::ptr::null_mut(), f as isize, 0, 0, 0)? as *mut c_void,
        _ => std::ptr::null_mut(),
    };
    // a made component with the reference the factory's SmartPtr gave
    let made = |p: isize| {
        cx.hold(EeWonCcKind::Component, p as *mut c_void);
        Ok(p)
    };
    let shell = |k: crate::shell::EeWonShellKind, a: [isize; 3]| {
        made(cx.call(EeWonCcOp::NewShell, std::ptr::null_mut(), k as isize, a[0], a[1], a[2]))
    };
    let component = match pf {
        Make::Fn(0) | Make::Kind(_) => Ok(0),
        Make::Fn(f) => cx.op(EeWonCcOp::AllocComp, std::ptr::null_mut(), f as isize, 0, 0, 0),
        Make::Shell(k, a) => shell(k, a),
        Make::SkinTab => {
            shell(crate::shell::EeWonShellKind::WonTabCtrl, [crate::skinobj::ee_won_skin_global() as isize, 0, 0])
        }
        // SAFETY: the game's skin (or none: no copy)
        Make::Skin(g) => {
            made(unsafe { crate::skinobj::ee_won_skin_get(crate::skinobj::ee_won_skin_global(), g, 0) } as isize)
        }
    };
    match component {
        Ok(component) => Ok(Some([config, component as *mut c_void])),
        Err(e) => {
            cx.release(EeWonCcKind::Config, config);
            Err(e)
        }
    }
}

// ---- ComponentConfig

/// A config's members (`mConfigMap`, `mCurResourceConfig`, `mConfigParser`, `mStringParser`).
pub struct EeWonCompCfg {
    this_: *mut c_void,
    host: EeWonCcHost,
    map: RefCell<NoCaseMap<*mut c_void>>,
    cur_res: Cell<*mut c_void>,
    parser: Cell<isize>,
    sp: Cell<*mut EeWonCfgStringParser>,
    /// a kind's own two words (the lobby game list's header and column count: ServerListCtrlConfig's mHeader and
    /// mNumColumns), zero at first
    slots: std::cell::UnsafeCell<[isize; 2]>,
}

/// The integer expression's source: this config's parser and its operands.
struct Expr<'a> {
    cfg: &'a EeWonCompCfg,
    cx: &'a Cx,
    err: Option<CcErr>,
}

impl ExprSource for Expr<'_> {
    fn skip_whitespace(&mut self) {
        self.cfg.sp().skip_ws()
    }
    fn peek(&mut self) -> u8 {
        self.cfg.sp().get_char(0)
    }
    fn advance(&mut self, n: i32) {
        self.cfg.sp().advance(n)
    }
    fn int_value(&mut self) -> Option<i32> {
        match self.cfg.get_int_value(self.cx) {
            Ok(v) => Some(v),
            Err(e) => {
                self.err = Some(e);
                None
            }
        }
    }
}

/// The three C++ flags.
const FLAG_WIDTH: isize = 0;
const FLAG_HEIGHT: isize = 1;
const FLAG_INITED: isize = 2;

impl EeWonCompCfg {
    fn sp(&self) -> Sp {
        Sp(self.sp.get())
    }
    fn parent(&self, cx: &Cx) -> *mut c_void {
        cx.call(EeWonCcOp::Parent, self.this_, 0, 0, 0, 0) as *mut c_void
    }

    /// `EndOfString`
    pub fn end_of_string(&self) -> bool {
        self.sp().skip_ws();
        self.sp().end()
    }
    /// `SkipComma`
    pub fn skip_comma(&self) {
        self.sp().skip_ws();
        if self.sp().get_char(0) == b',' {
            self.sp().advance(1);
        }
    }
    /// `EnsureComma`
    pub fn ensure_comma(&self) -> R<()> {
        ensure(self.sp(), b',')
    }
    /// `ParseExpression(level)` (`ReadInt` is level 0).
    pub fn parse_expression(&self, cx: &Cx, level: i32) -> R<i32> {
        let mut e = Expr { cfg: self, cx, err: None };
        match guicfg::parse_expression(&mut e, level) {
            Ok(v) => Ok(v),
            Err(guicfg::ExprError::Paren) => msg(&[b"Expecting ')'"]),
            Err(guicfg::ExprError::DivZero) => msg(&[b"Division by 0."]),
            Err(guicfg::ExprError::Operand) => Err(e.err.take().unwrap_or(CcErr::Host)),
        }
    }
    pub fn read_int(&self, cx: &Cx) -> R<i32> {
        self.parse_expression(cx, 0)
    }
    /// `ReadBool`
    pub fn read_bool(&self) -> R<bool> {
        self.sp().read_bool().map_or_else(|| msg(&[b"Expecting bool"]), Ok)
    }
    /// `ReadChar`: one unit after whitespace.
    pub fn read_char(&self) -> R<i32> {
        self.sp().skip_ws();
        if self.sp().end() {
            return msg(&[b"Expecting char"]);
        }
        let v = self.sp().get_wchar(0) as WChar;
        self.sp().advance(1);
        Ok(v as i32)
    }
    /// `ReadFlag`: a '+' or '-' sets `on`, then the name.
    pub fn read_flag(&self, cx: &Cx, on: &mut Option<bool>) -> R<Vec<u32>> {
        self.sp().skip_ws();
        let next = self.sp().get_char(0);
        if next == b'+' {
            self.sp().advance(1);
            *on = Some(true);
        } else if next == b'-' {
            self.sp().advance(1);
            *on = Some(false);
        }
        self.read_string(cx, true)
    }
    /// `ReadRawString`
    pub fn read_raw_string(&self, stop: bool) -> Vec<u32> {
        self.sp().read_wstring(stop)
    }
    /// `ReadString`: the text, or after '@' the string resource of that name.
    pub fn read_string(&self, cx: &Cx, stop: bool) -> R<Vec<u32>> {
        self.sp().skip_ws();
        if self.sp().get_char(0) == b'@' {
            self.sp().advance(1);
            let key = self.sp().read_string(false);
            return self.string_resource(cx, &key);
        }
        Ok(self.sp().read_wstring(stop))
    }
    fn read_name(&self, cx: &Cx, table: i32, names: &[i32], what: &[u8]) -> R<i32> {
        let v = self.read_string(cx, false)?;
        let i = name_idx(table, &v);
        if i >= 0 && (i as usize) < names.len() {
            return Ok(names[i as usize]);
        }
        msg(&[what])
    }
    /// `ReadDirection`: Left 0, Right 1, Up 2, Down 3.
    pub fn read_direction(&self, cx: &Cx) -> R<i32> {
        self.read_name(cx, EE_WON_CFG_NAMES_COMPONENTCONFIG_READDIRECTION_VALUE, &[0, 1, 2, 3], b"Bad Direction")
    }
    /// `ReadHorzAlign`: Left 0, Center 1, Right 2.
    pub fn read_horz_align(&self, cx: &Cx) -> R<i32> {
        self.read_name(cx, EE_WON_CFG_NAMES_COMPONENTCONFIG_READHORZALIGN_VALUE, &[0, 1, 2], b"Bad HorzAlign")
    }
    /// `ReadVertAlign`: Top 0, Center 1, Bottom 2.
    pub fn read_vert_align(&self, cx: &Cx) -> R<i32> {
        self.read_name(cx, EE_WON_CFG_NAMES_COMPONENTCONFIG_READVERTALIGN_VALUE, &[0, 1, 2], b"Bad VertAlign")
    }

    /// `GetIntValue`: a number, @ID_name, @name, or a named value or function of this or a dotted config. A
    /// function its config does not know is 0 (the original returned an unset int).
    pub fn get_int_value(&self, cx: &Cx) -> R<i32> {
        let sp = self.sp();
        if let Some(v) = sp.read_int() {
            return Ok(v);
        }
        let mut name: Vec<u8> = Vec::new();
        while !sp.end() {
            let next = sp.get_char(0);
            if !guicfg::name_char(next as i8) {
                break;
            }
            name.push(next);
            sp.advance(1);
        }
        let is_function = sp.get_char(0) == b'(';
        let (kind, at) = guicfg::name_kind(&name);
        let mut config = self.this_;
        if kind <= 1 {
            let rest = &name[at.min(name.len())..];
            if kind == 0 {
                let id = control_ids().get(rest).copied();
                return id.map_or_else(|| msg(&[b"Invalid control id: ", &name]), Ok);
            }
            return self.int_resource(cx, rest);
        }
        let mut key = if kind == 2 {
            let path = widen(&name[..at]);
            let k = name[at + 1..].to_vec();
            config = self.component_config(cx, &path, true)?;
            k
        } else {
            name.clone()
        };
        key.make_ascii_uppercase();
        if is_function {
            let mut params: Vec<i32> = Vec::new();
            sp.advance(1);
            if !sp.check(b')') {
                loop {
                    params.push(self.read_int(cx)?);
                    if sp.check(b')') {
                        break;
                    }
                    self.ensure_comma()?;
                    if sp.check(b')') {
                        break;
                    }
                }
            }
            let mut result = 0i32;
            let args = EeWonCcCallArgs {
                key: key.as_ptr(),
                key_len: key.len(),
                params: params.as_ptr(),
                count: params.len(),
                out: &mut result,
            };
            cx.op(EeWonCcOp::VCall, config, &args as *const EeWonCcCallArgs as isize, 0, 0, 0)?;
            return Ok(result);
        }
        let mut result = 0i32;
        if cx.op(
            EeWonCcOp::VNamed,
            config,
            key.as_ptr() as isize,
            key.len() as isize,
            &mut result as *mut i32 as isize,
            0,
        )? != 0
        {
            return Ok(result);
        }
        msg(&[b"Invalid named value: ", &key])
    }

    /// The config's kind (its C++ `mKind`).
    pub fn kind(&self, cx: &Cx) -> EeWonCfgKind {
        let k = cx.call(EeWonCcOp::Kind, self.this_, 0, 0, 0, 0) as i32;
        // SAFETY: the C++ member holds an EeWonCfgKind
        unsafe { std::mem::transmute::<i32, EeWonCfgKind>(k) }
    }
    /// `ComponentConfig::GetNamedValue`: the kind's own names (the WON frame's paddings, the sum of the frame
    /// borders cfgkind.rs names, read in its order), else the component's edges.
    pub fn base_named_value(&self, cx: &Cx, key: &[u8]) -> Option<i32> {
        if let Some(sides) = cfgkind::named_borders(self.kind(cx), key).filter(|s| !s.is_empty()) {
            let component = cx.call(EeWonCcOp::CompOf, self.this_, 0, 0, 0, 0) as *mut c_void;
            let sum = sides.iter().fold(0i32, |s, &side| {
                s.wrapping_add(cx.call(EeWonCcOp::CompFrameBorder, component, side as isize, 0, 0, 0) as i32)
            });
            return Some(sum);
        }
        self.named_value(cx, key)
    }
    /// `GetNamedValue`'s own names: the component's LEFT, RIGHT, TOP, BOTTOM, WIDTH, HEIGHT.
    pub fn named_value(&self, cx: &Cx, key: &[u8]) -> Option<i32> {
        let k = key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_GETNAMEDVALUE_INNAME, key);
        (0..6).contains(&k).then(|| cx.edge(self.this_, k as isize))
    }

    // ---- the resources, this group's then the parents'

    fn up<T>(&self, cx: &Cx, here: impl Fn(&EeWonResCfg) -> Option<T>, kind: ResKind, key: &[u8]) -> R<T> {
        let mut c: Option<&EeWonCompCfg> = Some(self);
        while let Some(cfg) = c {
            if let Some(res) = cx.res(cfg.cur_res.get()) {
                if let Some(v) = here(res) {
                    return Ok(v);
                }
            }
            let p = cfg.parent(cx);
            c = if p.is_null() { None } else { cx.cfg(p) };
        }
        msg(&[kind.name(), b" resource not found: ", key])
    }
    pub fn string_resource(&self, cx: &Cx, key: &[u8]) -> R<Vec<u32>> {
        self.up(cx, |r| r.get_string(key), ResKind::String, key)
    }
    /// borrowed
    pub fn image_resource(&self, cx: &Cx, key: &[u8]) -> R<*mut c_void> {
        self.up(cx, |r| EeWonResCfg::get_ptr(&r.images, key), ResKind::Image, key)
    }
    /// borrowed
    pub fn font_resource(&self, cx: &Cx, key: &[u8]) -> R<*mut c_void> {
        self.up(cx, |r| EeWonResCfg::get_ptr(&r.fonts, key), ResKind::Font, key)
    }
    /// borrowed
    pub fn sound_resource(&self, cx: &Cx, key: &[u8]) -> R<*mut c_void> {
        self.up(cx, |r| EeWonResCfg::get_ptr(&r.sounds, key), ResKind::Sound, key)
    }
    pub fn color_resource(&self, cx: &Cx, key: &[u8]) -> R<u32> {
        self.up(cx, |r| r.colors.borrow().get(key).map(|v| v.0), ResKind::Color, key)
    }
    pub fn background_resource(&self, cx: &Cx, key: &[u8]) -> R<EeWonCcBackground> {
        self.up(cx, |r| r.backgrounds.borrow().get(key).map(|v| v.0), ResKind::Background, key)
    }
    /// `GetIntResource` ("Int resource not found: ").
    pub fn int_resource(&self, cx: &Cx, key: &[u8]) -> R<i32> {
        let mut c: Option<&EeWonCompCfg> = Some(self);
        while let Some(cfg) = c {
            if let Some(res) = cx.res(cfg.cur_res.get()) {
                if let Some(v) = res.ints.borrow().get(key) {
                    return Ok(v.0);
                }
            }
            let p = cfg.parent(cx);
            c = if p.is_null() { None } else { cx.cfg(p) };
        }
        msg(&[b"Int resource not found: ", key])
    }

    /// `ReadColor`: '@' a colour resource, else a colour.
    pub fn read_color(&self, cx: &Cx) -> R<i32> {
        let sp = self.sp();
        sp.skip_ws();
        if sp.get_char(0) == b'@' {
            sp.advance(1);
            let key = sp.read_string(true);
            return Ok(self.color_resource(cx, &key)? as i32);
        }
        safe_read_color(sp)
    }
    /// `ReadSound` (a reference).
    pub fn read_sound(&self, cx: &Cx) -> R<*mut c_void> {
        let sp = self.sp();
        sp.skip_ws();
        if sp.get_char(0) == b'@' {
            sp.advance(1);
            let key = sp.read_string(true);
            let s = self.sound_resource(cx, &key)?;
            cx.hold(EeWonCcKind::Sound, s);
            return Ok(s);
        }
        safe_read_sound(cx, sp)
    }
    /// `ReadBackground` (its image held).
    pub fn read_background(&self, cx: &Cx) -> R<EeWonCcBackground> {
        let sp = self.sp();
        if sp.get_char(0) == b'@' {
            sp.advance(1);
            let key = sp.read_string(true);
            let b = self.background_resource(cx, &key)?;
            cx.hold(EeWonCcKind::Image, b.image);
            if b.want_grab_bg {
                cx.call(EeWonCcOp::CompWantGrab, self.this_, 0, 0, 0, 0);
            }
            return Ok(b);
        }
        safe_read_background(cx, sp)
    }
    /// `ReadFont` (a reference): '@' a resource, (CFG,name) a FontConfig up the parents, else a font. The search past
    /// the root (the original's had no end) finds none.
    pub fn read_font(&self, cx: &Cx) -> R<*mut c_void> {
        let sp = self.sp();
        sp.skip_ws();
        if sp.get_char(0) == b'@' {
            sp.advance(1);
            let name = sp.read_string(false);
            let f = self.font_resource(cx, &name)?;
            cx.hold(EeWonCcKind::Font, f);
            return Ok(f);
        }
        if sp.get_char(0) == b'(' {
            sp.advance(1);
            let ty = sp.read_string(false);
            if icase(&ty, b"CFG") {
                self.ensure_comma()?;
                let name = sp.read_string(false);
                let wname = widen(&name);
                let mut current: *mut c_void = self.this_;
                let mut font: isize = -1;
                while let Some(cfg) = cx.cfg(current) {
                    let found = cfg.component_config(cx, &wname, false)?;
                    font = if found.is_null() || cx.call(EeWonCcOp::IsFontCfg, found, 0, 0, 0, 0) == 0 {
                        -1
                    } else {
                        cx.call(EeWonCcOp::FontOf, found, 0, 0, 0, 0)
                    };
                    if font != -1 {
                        break;
                    }
                    current = cfg.parent(cx);
                }
                if font == -1 {
                    return msg(&[b"Invalid FontConfig: ", &name]);
                }
                if font == 0 {
                    return msg(&[b"NULL Font: ", &name]);
                }
                if !sp.check(b')') {
                    return msg(&[b"Expecting ')'"]);
                }
                cx.hold(EeWonCcKind::Font, font as *mut c_void);
                return Ok(font as *mut c_void);
            }
            return msg(&[b"Bad Font Type: ", &ty]);
        }
        safe_read_font(cx, sp)
    }
    /// `GetImageFromStr` (a reference, NULL for "@NULL").
    pub fn image_from_str(&self, cx: &Cx, spec: &[u8], raw: bool) -> R<*mut c_void> {
        if spec.first() == Some(&b'@') {
            if let Some(split) = spec.iter().position(|&c| c == b'_') {
                let ty = &spec[1.min(split)..split];
                if icase(ty, b"CFG") {
                    let name = widen(&spec[split + 1..]);
                    let mut current: *mut c_void = self.this_;
                    let mut image: isize = -1;
                    while let Some(cfg) = cx.cfg(current) {
                        let found = cfg.component_config(cx, &name, false)?;
                        image = if found.is_null() || cx.call(EeWonCcOp::IsImageCfg, found, 0, 0, 0, 0) == 0 {
                            -1
                        } else {
                            let op = if raw { EeWonCcOp::RawImageOf } else { EeWonCcOp::ImageOf };
                            cx.call(op, found, 0, 0, 0, 0)
                        };
                        if image != -1 {
                            break;
                        }
                        current = cfg.parent(cx);
                    }
                    if image == -1 {
                        return msg(&[b"Invalid ImageConfig: ", spec]);
                    }
                    if image == 0 {
                        return msg(&[b"Image Doesn't Exist: ", spec]);
                    }
                    return Ok(image as *mut c_void);
                }
            } else if icase(spec, b"@NULL") {
                return Ok(std::ptr::null_mut());
            } else {
                let img = self.image_resource(cx, &spec[1..])?;
                cx.hold(EeWonCcKind::Image, img);
                return Ok(img);
            }
            return msg(&[b"Bad Image Specification: ", spec]);
        }
        let img = decode(cx, &resource_file(spec), raw)?;
        if img.is_null() {
            return msg(&[b"Failed to decode image: ", spec]);
        }
        Ok(img)
    }
    /// `ReadImage(raw)` (a reference).
    pub fn read_image(&self, cx: &Cx, raw: bool) -> R<*mut c_void> {
        let name = self.sp().read_string(false);
        self.image_from_str(cx, &name, raw)
    }
    /// `ReadStandardCursor`: Arrow 0 .. NorthSouthEastWest 5 (the StandardCursor values).
    pub fn read_standard_cursor(&self, cx: &Cx) -> R<i32> {
        let v = self.read_string(cx, false)?;
        let i = name_idx(EE_WON_CFG_NAMES_COMPONENTCONFIG_READSTANDARDCURSOR_NAME, &v);
        if (0..6).contains(&i) {
            return Ok(i);
        }
        msg(&[b"Invalid predefined cursor type: ", &narrow(&v)])
    }
    /// `ReadCursor`: only "Predefined, <name>" (the standard cursor's number).
    pub fn read_cursor(&self, cx: &Cx) -> R<i32> {
        let ty = self.read_string(cx, false)?;
        if name_idx(EE_WON_CFG_NAMES_COMPONENTCONFIG_READCURSOR_TYPE, &ty) == 0 {
            self.ensure_comma()?;
            return self.read_standard_cursor(cx);
        }
        msg(&[b"Invalid cursor type: ", &narrow(&ty)])
    }
    /// `ReadSelectionColor`: a background colour, then USEFOREGROUND and a colour, INVERTORIGINAL or USEORIGINAL.
    pub fn read_selection_color(&self, cx: &Cx) -> R<EeWonCcSelColor> {
        let background = self.read_color(cx)? as u32;
        // the modes: 0 UseForegroundColor, 1 InvertOriginalColor, 2 UseOriginalColor
        if self.end_of_string() {
            return Ok(EeWonCcSelColor { background, foreground: 0xffffff, mode: 1 });
        }
        self.ensure_comma()?;
        let ty = self.read_string(cx, false)?;
        match name_idx(EE_WON_CFG_NAMES_COMPONENTCONFIG_READSELECTIONCOLOR_TYPE, &ty) {
            0 => {
                self.ensure_comma()?;
                let foreground = self.read_color(cx)? as u32;
                Ok(EeWonCcSelColor { background, foreground, mode: 0 })
            }
            1 => Ok(EeWonCcSelColor { background, foreground: 0xffffff, mode: 1 }),
            2 => Ok(EeWonCcSelColor { background, foreground: 0xffffff, mode: 2 }),
            _ => msg(&[b"Invalid SelectionColor"]),
        }
    }

    // ---- the parse hooks

    /// `PreParse`: the base group this config inherits from included.
    pub fn pre_parse(&self, cx: &Cx, parser: isize) -> R<bool> {
        let base = cx.bytes(EeWonCcOp::BaseName, self.this_);
        if base.is_empty() {
            return Ok(true);
        }
        Ok(cx.op(EeWonCcOp::IncludeGroup, self.this_, parser, base.as_ptr() as isize, base.len() as isize, 0)? != 0)
    }
    /// `SetDesiredSizeIfNecessary`
    pub fn set_desired_size(&self, cx: &Cx) {
        let c = self.this_;
        let edge = |k: isize| cx.edge(c, k);
        let flag = |k: isize| cx.call(EeWonCcOp::Flag, c, k, 0, 0, 0) != 0;
        let set = |k: isize| cx.call(EeWonCcOp::SetFlag, c, k, 1, 0, 0);
        if edge(4) > 0 {
            set(FLAG_WIDTH);
        }
        if edge(5) > 0 {
            set(FLAG_HEIGHT);
        }
        if flag(FLAG_WIDTH) && flag(FLAG_HEIGHT) {
            return;
        }
        let mut wh = [0i32; 2];
        cx.call(EeWonCcOp::CompDesired, c, wh.as_mut_ptr() as isize, 0, 0, 0);
        if flag(FLAG_WIDTH) {
            wh[0] = edge(4);
            set(FLAG_WIDTH);
        }
        if flag(FLAG_HEIGHT) {
            wh[1] = edge(5);
            set(FLAG_HEIGHT);
        }
        cx.call(EeWonCcOp::CompSetSize, c, wh[0] as isize, wh[1] as isize, 0, 0);
    }
    /// `HandleInstruction`: the base's, MODIFY, RESOURCE, or a component type: a child config made, parsed and kept
    /// under its name.
    pub fn handle_instruction(&self, cx: &Cx, parser: isize, ty: &[u8]) -> R<bool> {
        let k = key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEINSTRUCTION_TYPE, ty);
        if cx.op(EeWonCcOp::CoHandleInstruction, self.this_, parser, ty.as_ptr() as isize, ty.len() as isize, 0)? != 0 {
            return Ok(true);
        }
        if k == 0 {
            return self.handle_modify_group(cx, parser);
        }
        if k == 1 {
            return self.handle_resource_group(cx, parser);
        }
        let Some([config, component]) = allocate(cx, ty)? else { return Ok(false) };
        let r = (|| -> R<bool> {
            cx.op(EeWonCcOp::VSetComponent, config, component as isize, 0, 0, 0)?;
            let ok = cx.op(EeWonCcOp::ParseChild, self.this_, parser, config as isize, 2, 0)? != 0;
            if ok {
                let name = cx.bytes(EeWonCcOp::Name, config);
                if !name.is_empty() {
                    cx.call(EeWonCcOp::SetParent, config, self.this_ as isize, 0, 0, 0);
                    self.map_set(cx, &name, config);
                }
            }
            Ok(ok)
        })();
        cx.release(EeWonCcKind::Config, config);
        cx.release(EeWonCcKind::Component, component);
        r
    }
    /// `mConfigMap[key] = config` (a reference taken, the old one given back).
    fn map_set(&self, cx: &Cx, key: &[u8], config: *mut c_void) {
        cx.hold(EeWonCcKind::Config, config);
        let old = self.map.borrow_mut().set(key, config);
        if let Some(o) = old {
            cx.release(EeWonCcKind::Config, o);
        }
    }
    /// `GetComponentConfig(path, fail)`: this, the parent, a child by name or a predefined '_NAME' component, then
    /// the rest of a dotted path (always failing); NULL or "Component not found: " for none.
    pub fn component_config(&self, cx: &Cx, path: &[u32], fail: bool) -> R<*mut c_void> {
        let dot = guistring::find_char(path, i32::from(b'.'), 0);
        let (name, rest): (&[u32], &[u32]) =
            if dot > 0 { (&path[..dot as usize], &path[dot as usize + 1..]) } else { (path, &[]) };
        let mut config: *mut c_void = std::ptr::null_mut();
        match name_idx(EE_WON_CFG_NAMES_COMPONENTCONFIG_GETCOMPONENTCONFIG_NAME, name) {
            0 => config = self.this_,
            1 => config = self.parent(cx),
            _ => {
                let key = narrow(name);
                let found = self.map.borrow().get(&key).copied();
                if let Some(f) = found {
                    config = f;
                } else if name.first() == Some(&u32::from(b'_')) {
                    let mut pre = narrow(&name[1..]);
                    pre.make_ascii_uppercase();
                    let mut out = [std::ptr::null_mut::<c_void>(); 2];
                    let got = cx.op(
                        EeWonCcOp::VPredefined,
                        self.this_,
                        pre.as_ptr() as isize,
                        pre.len() as isize,
                        out.as_mut_ptr() as isize,
                        0,
                    )? != 0;
                    if !out[0].is_null() {
                        out[0] = cx.call(EeWonCcOp::AsObject, out[0], 0, 0, 0, 0) as *mut c_void;
                    }
                    config = out[0];
                    if got && !out[0].is_null() && !out[1].is_null() {
                        self.map_set(cx, &key, out[0]);
                        cx.op(EeWonCcOp::VSetComponent, out[0], out[1] as isize, 0, 0, 0)?;
                        cx.call(EeWonCcOp::SetParent, out[0], self.this_ as isize, 0, 0, 0);
                    }
                }
            }
        }
        if config.is_null() {
            if fail {
                return msg(&[b"Component not found: ", &narrow(name)]);
            }
            return Ok(std::ptr::null_mut());
        }
        if rest.is_empty() {
            return Ok(config);
        }
        match cx.cfg(config) {
            Some(c) => c.component_config(cx, rest, true),
            None => msg(&[b"Component not found: ", &narrow(rest)]),
        }
    }
    /// `Fn386860`: the component of the child config of that name (borrowed), NULL for none.
    pub fn child_component(&self, cx: &Cx, name: &[u8]) -> *mut c_void {
        let found = self.map.borrow().get(name).copied();
        match found {
            Some(c) => cx.call(EeWonCcOp::CompOf, c, 0, 0, 0, 0) as *mut c_void,
            None => std::ptr::null_mut(),
        }
    }
    /// `ReadComponentConfig`
    pub fn read_component_config(&self, cx: &Cx) -> R<*mut c_void> {
        let name = self.read_string(cx, false)?;
        self.component_config(cx, &name, true)
    }
    /// `ReadComponent` (borrowed)
    pub fn read_component(&self, cx: &Cx) -> R<*mut c_void> {
        let c = self.read_component_config(cx)?;
        Ok(cx.call(EeWonCcOp::CompOf, c, 0, 0, 0, 0) as *mut c_void)
    }
    /// `HandleMapComponent`: a config mapped under a further name (also in another config's map for
    /// 'config.name'), or with no name this config into the named config's map.
    pub fn handle_map_component(&self, cx: &Cx) -> R<bool> {
        let config = self.read_component_config(cx)?;
        if !self.end_of_string() {
            self.ensure_comma()?;
            let mut name = self.read_string(cx, false)?;
            let dot = guistring::rfind_char(&name, i32::from(b'.'), -1);
            if dot >= 0 {
                let component = name[..dot as usize].to_vec();
                name = name[dot as usize + 1..].to_vec();
                let target = self.component_config(cx, &component, true)?;
                if name.is_empty() {
                    name = widen(&cx.bytes(EeWonCcOp::Name, config));
                }
                if let Some(t) = cx.cfg(target) {
                    t.map_set(cx, &narrow(&name), config);
                }
            }
            self.map_set(cx, &narrow(&name), config);
            return Ok(true);
        }
        let key = cx.bytes(EeWonCcOp::Name, self.this_);
        if let Some(c) = cx.cfg(config) {
            c.map_set(cx, &key, self.this_);
        }
        Ok(true)
    }
    /// `AddChildToConfigMap`: none for an empty name (`always`: NEWLAYOUTPOINT's `mConfigMap[name] = config`).
    pub fn add_child(&self, cx: &Cx, name: &[u32], config: *mut c_void, always: bool) {
        if always || !name.is_empty() {
            self.map_set(cx, &narrow(name), config);
        }
    }
    /// `HandleModifyGroup`: the next group parsed into the named config.
    pub fn handle_modify_group(&self, cx: &Cx, parser: isize) -> R<bool> {
        let (got, name, _inherited) = cx.read_group_start(self.this_, parser)?;
        if !got {
            return Ok(true);
        }
        let config = self.component_config(cx, &widen(&name), true)?;
        cx.op(EeWonCcOp::ParseGroup, config, parser, 1, 0, 0)?;
        Ok(true)
    }
    /// `HandleResourceGroup`: a named resource group parsed (overwriting allowed when it existed).
    pub fn handle_resource_group(&self, cx: &Cx, parser: isize) -> R<bool> {
        let (got, name, _inherited) = cx.read_group_start(self.this_, parser)?;
        if !got {
            return Ok(true);
        }
        let Some(table) = cx.table() else { return msg(&[b"Resource group not found: ", &name]) };
        let (res, existed) = table.add_config(cx, &name);
        if let Some(r) = cx.res(res) {
            r.allow_overwrite.set(existed);
        }
        cx.hold(EeWonCcKind::Res, res);
        let ok = cx.op(EeWonCcOp::ParseChild, self.this_, parser, res as isize, 0, 0);
        cx.release(EeWonCcKind::Res, res);
        Ok(ok? != 0)
    }
    /// `HandleModify`: one instruction run on another config ("Unrecognized instruction: " a warning).
    pub fn handle_modify(&self, cx: &Cx) -> R<bool> {
        let config = self.read_component_config(cx)?;
        self.ensure_comma()?;
        let mut instruction = narrow(&self.read_string(cx, false)?);
        self.skip_comma();
        instruction.make_ascii_uppercase();
        let handled = match cx.cfg(config) {
            Some(c) => c.base_handle(cx, self.parser.get(), &instruction, self.sp.get())?,
            None => false,
        };
        if !handled {
            let w = [b"Unrecognized instruction: ".as_slice(), &instruction].concat();
            cx.op(
                EeWonCcOp::AddWarning,
                self.parser.get() as *mut c_void,
                w.as_ptr() as isize,
                w.len() as isize,
                0,
                0,
            )?;
        }
        Ok(true)
    }
    /// `HandleValueInstruction`: the base's, SETRESOURCE (the current resource group), else the instruction.
    pub fn handle_value(&self, cx: &Cx, parser: isize, key: &[u8], sp: *mut EeWonCfgStringParser) -> R<bool> {
        let k = key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_HANDLEVALUEINSTRUCTION_KEY, key);
        if cx.op(
            EeWonCcOp::CoHandleValue,
            self.this_,
            parser,
            key.as_ptr() as isize,
            key.len() as isize,
            sp as isize,
        )? != 0
        {
            return Ok(true);
        }
        if k == 0 {
            let name = Sp(sp).read_string(false);
            let Some(table) = cx.table() else { return msg(&[b"Resource group not found: ", &name]) };
            let res = table.safe_get(&name)?;
            cx.hold(EeWonCcKind::Res, res);
            let old = self.cur_res.replace(res);
            cx.release(EeWonCcKind::Res, old);
            return Ok(true);
        }
        self.base_handle(cx, parser, key, sp)
    }
    /// `BaseHandleInstruction`: the parsers kept for the reads, then the instruction (`Virtual24`).
    pub fn base_handle(&self, cx: &Cx, parser: isize, key: &[u8], sp: *mut EeWonCfgStringParser) -> R<bool> {
        self.parser.set(parser);
        self.sp.set(sp);
        Ok(cx.op(EeWonCcOp::V24, self.this_, key.as_ptr() as isize, key.len() as isize, 0, 0)? != 0)
    }
    /// `InitComponents`: once, this component, then every child config in name order.
    pub fn init_components(&self, cx: &Cx) -> R<()> {
        if cx.call(EeWonCcOp::Flag, self.this_, FLAG_INITED, 0, 0, 0) != 0 {
            return Ok(());
        }
        cx.call(EeWonCcOp::SetFlag, self.this_, FLAG_INITED, 1, 0, 0);
        cx.op(EeWonCcOp::CompInit, self.this_, 0, 0, 0, 0)?;
        let mut i = 0;
        loop {
            let child = match self.map.borrow().at(i) {
                Some((_, &c)) => c,
                None => return Ok(()),
            };
            if let Some(c) = cx.cfg(child) {
                c.init_components(cx)?;
            }
            i += 1;
        }
    }
}

/// `CallNamedFunction`'s own functions: MAX and MIN over the parameters, both from 0 (MIN is never above 0).
pub fn call_named_function(key: &[u8], params: &[i32]) -> Option<i32> {
    match key_index(EE_WON_CFG_KEYS_COMPONENTCONFIG_CALLNAMEDFUNCTION_INNAME, key) {
        0 => Some(params.iter().fold(0, |v, &p| if p > v { p } else { v })),
        1 => Some(params.iter().fold(0, |v, &p| if p < v { p } else { v })),
        _ => None,
    }
}

// ---- the C ABI

/// What `ee_won_cc_call` answers; `o` is the Rust object named, the arguments `a`..`d` as each says.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCcCall {
    /// config: `*(int*)a` = `ReadInt()`
    ReadInt = 0,
    /// config: `*(int*)b` = `ParseExpression(a)`
    ParseExpression = 1,
    /// config: `*(int*)a` = `GetIntValue()`
    GetIntValue = 2,
    /// config: `*(bool*)a` = `ReadBool()`
    ReadBool = 3,
    /// config: `*(int*)a` = `ReadChar()`
    ReadChar = 4,
    /// config: `*(GUIString*)a` = `ReadFlag(b)` (`b` a `bool*` or NULL)
    ReadFlag = 5,
    /// config: `*(GUIString*)a` = `ReadRawString(b)`
    ReadRawString = 6,
    /// config: `*(GUIString*)a` = `ReadString(b)`
    ReadString = 7,
    /// config: `*(int*)a` = `ReadDirection()`
    ReadDirection = 8,
    /// config: `*(int*)a` = `ReadHorzAlign()`
    ReadHorzAlign = 9,
    /// config: `*(int*)a` = `ReadVertAlign()`
    ReadVertAlign = 10,
    /// config: `*(int*)a` = `ReadColor()`
    ReadColor = 11,
    /// config: `*(Sound**)a` = `ReadSound()` (a reference)
    ReadSound = 12,
    /// config: `*(Background*)a` = `ReadBackground()`
    ReadBackground = 13,
    /// config: `*(Font**)a` = `ReadFont()` (a reference)
    ReadFont = 14,
    /// config: `*(Image**)a` = `GetImageFromStr(b, c bytes, d)` (a reference)
    ImageFromStr = 15,
    /// config: `*(Image**)a` = `ReadImage(b)` (a reference)
    ReadImage = 16,
    /// config: `*(int*)a` = `ReadStandardCursor()`
    ReadStandardCursor = 17,
    /// config: `*(int*)a` = `ReadCursor()`'s standard cursor
    ReadCursor = 18,
    /// config: `*(SelectionColor*)a` = `ReadSelectionColor()` (made by the host's `Sel*` ops)
    ReadSelectionColor = 19,
    /// config: `*(bool*)a` = `EndOfString()`
    EndOfString = 20,
    /// config: `SkipComma()`
    SkipComma = 21,
    /// config: `EnsureComma()`
    EnsureComma = 22,
    /// config: `*(int*)a` = `GetIntResource(b, c bytes)`
    GetIntResource = 23,
    /// config: `*(GUIString*)a` = `GetStringResource(b, c bytes)`
    GetStringResource = 24,
    /// config: `*(Image**)a` = `GetImageResource(b, c bytes)` (borrowed)
    GetImageResource = 25,
    /// config: `*(Font**)a` = `GetFontResource(b, c bytes)` (borrowed)
    GetFontResource = 26,
    /// config: `*(Sound**)a` = `GetSoundResource(b, c bytes)` (a reference)
    GetSoundResource = 27,
    /// config: `*(DWORD*)a` = `GetColorResource(b, c bytes)`
    GetColorResource = 28,
    /// config: `*(Background*)a` = `GetBackgroundResource(b, c bytes)`
    GetBackgroundResource = 29,
    /// config: `*(bool*)a` = `GetNamedValue(b, c bytes, *(int*)d)` (the base's: the kind's paddings, the edges)
    GetNamedValue = 30,
    /// config: `*(bool*)a` = `PreParse(b)` (the base's)
    PreParse = 31,
    /// config: `SetDesiredSizeIfNecessary()`
    SetDesiredSize = 32,
    /// config: `*(bool*)a` = `HandleInstruction(b, c, d bytes)` (the base's)
    HandleInstruction = 33,
    /// config: `*(ComponentConfig**)a` = `GetComponentConfig(*(GUIString*)b, c)`
    GetComponentConfig = 34,
    /// config: `*(Component**)a` = `Fn386860(b, c bytes)`
    ChildComponent = 35,
    /// config: `*(ComponentConfig**)a` = `ReadComponentConfig()`
    ReadComponentConfig = 36,
    /// config: `*(Component**)a` = `ReadComponent()`
    ReadComponent = 37,
    /// config: `*(bool*)a` = `HandleMapComponent()`
    HandleMapComponent = 38,
    /// config: `AddChildToConfigMap(the EeWonCcText a, b, c)` (`c`: an empty name too)
    AddChild = 39,
    /// config: `*(bool*)a` = `HandleModifyGroup(b)`
    HandleModifyGroup = 40,
    /// config: `*(bool*)a` = `HandleResourceGroup(b)`
    HandleResourceGroup = 41,
    /// config: `*(bool*)a` = `HandleModify()`
    HandleModify = 42,
    /// config: `*(bool*)a` = `HandleValueInstruction(b, the key at the EeWonCcKey c, d)` (the base's)
    HandleValueInstruction = 43,
    /// config: `*(bool*)a` = `BaseHandleInstruction(b, the key at the EeWonCcKey c, d)`
    BaseHandleInstruction = 44,
    /// config: `InitComponents()`
    InitComponents = 45,
    /// group: `*(bool*)a` = `HandleValueInstruction(b, the key at the EeWonCcKey c, d)`
    ResHandleValue = 46,
    /// group: `PreParse()` (the parse count)
    ResPreParse = 47,
    /// group: `SetAllowOverwrite(a)`
    ResSetAllowOverwrite = 48,
    /// group: `*(bool*)a` = `GetString(b, c bytes, *(GUIString*)d)`
    ResGetString = 49,
    /// group: the image, font or sound (`d`: 0, 1, 2) of `b`, `c` bytes (borrowed) into the `EeWonCcPtrOut` at `a`
    ResGetPtr = 50,
    /// group: `*(bool*)a` = `GetColor(b, c bytes, *(DWORD*)d)`
    ResGetColor = 51,
    /// group: `*(bool*)a` = `GetBackground(b, c bytes, *(Background*)d)`
    ResGetBackground = 52,
    /// group: `*(bool*)a` = `GetInt(b, c bytes, *(int*)d)`
    ResGetInt = 53,
    /// group: `*(GUIString*)a` = `SafeGetString(b, c bytes)`
    ResSafeGetString = 54,
    /// group: `*(void**)a` = `SafeGetImage` (`d` 0) or `SafeGetSound` (`d` 2) of `b`, `c` bytes (borrowed)
    ResSafeGetPtr = 55,
    /// group: `AddSound(b, c bytes, d)`
    ResAddSound = 56,
    /// `*(int*)a` = `SafeReadColor(*(StringParser*)b)`
    SafeReadColor = 57,
    /// `*(int*)a` = `SafeReadFontStyle(b)`
    SafeReadFontStyle = 58,
    /// `*(Font**)a` = `SafeReadFont(b)` (a reference)
    SafeReadFont = 59,
    /// `*(Sound**)a` = `SafeReadSound(b)` (a reference)
    SafeReadSound = 60,
    /// `*(Image**)a` = `SafeReadImage(b)` (a reference)
    SafeReadImage = 61,
    /// `*(Background*)a` = `SafeReadBackground(b)`
    SafeReadBackground = 62,
    /// table: `*(ResourceConfig**)a` = `AddConfig(b, c bytes, (bool*)d)` (borrowed)
    TableAddConfig = 63,
    /// table: `*(ResourceConfig**)a` = `SafeGetConfig(b, c bytes)` (borrowed)
    TableSafeGetConfig = 64,
    /// table: `RegisterGlueResourceFunc(a)`
    TableRegisterGlue = 65,
    /// table: `SafeGlueResources()`
    TableGlue = 66,
    /// `AddAllocator(b, c bytes, a config factory, d component factory)`
    AddAllocator = 67,
    /// `RegisterControlId(b, c bytes, a)`
    RegisterControlId = 68,
    /// `*(bool*)a` = `AllocateConfig(b, c bytes, the two pointers at d)` (a reference each)
    AllocateConfig = 69,
    /// `*(bool*)a` = `GetControlId(b, c bytes, *(int*)d)`
    GetControlId = 70,
    /// table: the glue functions forgotten
    TableClearGlue = 71,
    /// the allocators forgotten
    ClearAllocators = 72,
    /// the control ids forgotten
    ClearControlIds = 73,
    /// table: the groups given back
    TableClearResources = 74,
    /// `SetResourceFolder(b, c bytes)`
    SetResourceFolder = 75,
    /// `GetResourceFile(b, c bytes)` handed to the sink `d` (`EeWonCcSink`) with the context `a`
    GetResourceFile = 76,
    /// config: `*(bool*)a` = `CallNamedFunction` over the `EeWonCcCallArgs` at `b` (the base's: MAX, MIN)
    CallNamedFunction = 77,
    /// config: `*(bool*)a` = `GetPredefinedComponent(the name of the EeWonCcKey b, *(ComponentConfig**)c,
    /// *(Component**)d)` (the base's: the kind's predefined children, cfgkind.rs)
    Predefined = 78,
    /// `ComponentConfigMissing(b, c bytes, the file a, the line d)`: the message "InitComponent error: name
    /// (file:line)" (the original's 512-byte snprintf) thrown as the original's ConfigObjectException
    Missing = 79,
}

/// A key and the value parser of an instruction.
#[repr(C)]
pub struct EeWonCcKey {
    pub key: *const u8,
    pub len: usize,
}

/// A string sink of the C++ (`std::string*` and its append).
pub type EeWonCcSink = Option<unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize)>;

unsafe fn bytes<'a>(p: isize, n: isize) -> &'a [u8] {
    if n <= 0 || p == 0 {
        &[]
    } else {
        // SAFETY: the caller's text
        unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) }
    }
}

/// The size of the record `kind` names (0 `EeWonCcCallArgs`, 1 `EeWonCcBackground`, 2 `EeWonCcSelColor`, 3
/// `EeWonCcKey`, 4 `EeWonCcText`, 5 `EeWonCcPtrOut`) for the C++'s checks; it also names them for cbindgen (which
/// emits a type only when an exported function names it).
#[no_mangle]
pub extern "C" fn ee_won_cc_record_size(
    kind: EeWonCcKind,
    _a: *const EeWonCcCallArgs,
    _b: *const EeWonCcBackground,
    _s: *const EeWonCcSelColor,
    _k: *const EeWonCcKey,
    _t: *const EeWonCcText,
    _p: *const EeWonCcPtrOut,
) -> usize {
    use std::mem::size_of;
    match kind as i32 {
        0 => size_of::<EeWonCcCallArgs>(),
        1 => size_of::<EeWonCcBackground>(),
        2 => size_of::<EeWonCcSelColor>(),
        3 => size_of::<EeWonCcKey>(),
        4 => size_of::<EeWonCcText>(),
        _ => size_of::<EeWonCcPtrOut>(),
    }
}

/// A new config's Rust object over the C++ config `this_` (its `ConfigObject*`).
#[no_mangle]
pub extern "C" fn ee_won_cc_new(this_: *mut c_void, host: EeWonCcHost) -> *mut EeWonCompCfg {
    let o = Box::into_raw(Box::new(EeWonCompCfg {
        this_,
        host,
        map: RefCell::default(),
        cur_res: Cell::new(std::ptr::null_mut()),
        parser: Cell::new(0),
        sp: Cell::new(std::ptr::null_mut()),
        slots: std::cell::UnsafeCell::new([0; 2]),
    }));
    register(&CONFIGS, this_, o as usize);
    o
}

/// The config's two words of its kind's own (see `EeWonCompCfg::slots`).
///
/// # Safety
/// `o` came from `ee_won_cc_new` and lives while the words are used.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_slots(o: *mut EeWonCompCfg) -> *mut isize {
    // SAFETY: the contract
    unsafe { (*o).slots.get() as *mut isize }
}

/// The config's object freed: the current group given back, then the child configs (the members' order).
///
/// # Safety
/// `o` came from `ee_won_cc_new` (or is NULL); nothing uses it after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_free(o: *mut EeWonCompCfg) {
    if o.is_null() {
        return;
    }
    // SAFETY: the contract
    let b = unsafe { Box::from_raw(o) };
    unregister(&CONFIGS, b.this_, o as usize);
    let cx = Cx { host: b.host, frame: std::ptr::null_mut() };
    cx.release(EeWonCcKind::Res, b.cur_res.replace(std::ptr::null_mut()));
    let children = b.map.borrow_mut().take();
    for (_, c) in children {
        cx.release(EeWonCcKind::Config, c);
    }
}

/// A new resource group's Rust object over the C++ group `this_`.
#[no_mangle]
pub extern "C" fn ee_won_cc_res_new(this_: *mut c_void) -> *mut EeWonResCfg {
    let o = Box::into_raw(Box::new(EeWonResCfg::new(this_)));
    register(&GROUPS, this_, o as usize);
    o
}

/// The group's object freed, its values given back.
///
/// # Safety
/// As `ee_won_cc_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_res_free(o: *mut EeWonResCfg, host: EeWonCcHost) {
    if o.is_null() {
        return;
    }
    // SAFETY: the contract
    let b = unsafe { Box::from_raw(o) };
    unregister(&GROUPS, b.this_, o as usize);
    b.free(&Cx { host, frame: std::ptr::null_mut() });
}

/// A new table's Rust object over the C++ table `this_`.
#[no_mangle]
pub extern "C" fn ee_won_cc_table_new(this_: *mut c_void) -> *mut EeWonResTable {
    Box::into_raw(Box::new(EeWonResTable { this_, groups: RefCell::default(), glue: RefCell::default() }))
}

/// The table's object freed, its groups given back.
///
/// # Safety
/// As `ee_won_cc_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_table_free(o: *mut EeWonResTable, host: EeWonCcHost) {
    if o.is_null() {
        return;
    }
    // SAFETY: the contract
    let b = unsafe { Box::from_raw(o) };
    b.clear_resources(&Cx { host, frame: std::ptr::null_mut() });
}

/// Where a `ResGetPtr` puts what it found.
#[repr(C)]
pub struct EeWonCcPtrOut {
    pub found: bool,
    pub ptr: *mut c_void,
}

/// One call of a config (`o` an `EeWonCompCfg`), a group (`EeWonResCfg`), a table (`EeWonResTable`) or none, as
/// [`EeWonCcCall`] says. 0: done; 1: the frame has a message (throw `ConfigObjectException`); 2: a host op threw
/// (rethrow the frame's exception).
///
/// # Safety
/// `o` is the object the call names (NULL for the statics), alive; the pointers in `a`..`d` are what the call says;
/// `host` and `frame` are the C++ end's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cc_call(
    o: *mut c_void,
    host: EeWonCcHost,
    frame: *mut c_void,
    call: EeWonCcCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> i32 {
    let cx = Cx { host, frame };
    // SAFETY: the contract (each arm reads what its call says)
    let r = unsafe { dispatch(&cx, o, call, a, b, c, d) };
    match r {
        Ok(()) => 0,
        Err(CcErr::Msg(m)) => {
            cx.call(EeWonCcOp::SetMessage, std::ptr::null_mut(), m.as_ptr() as isize, m.len() as isize, 0, 0);
            1
        }
        Err(CcErr::Host) => 2,
    }
}

/// `ComponentConfigMissing`'s message: `snprintf(512, "InitComponent error: %s (%s:%d)", name, file, line)`.
unsafe fn missing_text(name: &[u8], file: *const c_char, line: i32) -> Vec<u8> {
    let upto_nul = |b: &[u8]| b[..b.iter().position(|&c| c == 0).unwrap_or(b.len())].to_vec();
    let file: Vec<u8> = if file.is_null() {
        b"(null)".to_vec()
    } else {
        // SAFETY: the caller's NUL-terminated text
        unsafe { std::ffi::CStr::from_ptr(file) }.to_bytes().to_vec()
    };
    let mut m =
        [b"InitComponent error: ".as_slice(), &upto_nul(name), b" (", &file, b":", line.to_string().as_bytes(), b")"]
            .concat();
    m.truncate(511);
    m
}

/// The bytes of the `EeWonCcKey` at `p`.
unsafe fn key_at<'a>(p: isize) -> &'a [u8] {
    // SAFETY: the caller's EeWonCcKey
    let k = unsafe { &*(p as *const EeWonCcKey) };
    // SAFETY: its bytes
    unsafe { bytes(k.key as isize, k.len as isize) }
}

fn o_this(c: &EeWonCompCfg) -> *mut c_void {
    c.this_
}

unsafe fn put<T>(a: isize, v: T) {
    // SAFETY: the caller's out pointer
    unsafe { *(a as *mut T) = v };
}

fn set_text(cx: &Cx, out: isize, units: &[u32]) {
    // the host's wchar_t units (the original appended each unit cast to wchar_t)
    let w: Vec<WChar> = units.iter().map(|&u| u as WChar).collect();
    cx.call(EeWonCcOp::SetText, out as *mut c_void, w.as_ptr() as isize, w.len() as isize, 0, 0);
}

/// A `GUIString*`'s units (its data's `mText`, `mLength`: the first two words of `GUIStringData` after RefCount are
/// not ours to read, so the C++ hands the text as an `EeWonCcText`).
#[repr(C)]
pub struct EeWonCcText {
    pub units: *const c_void,
    pub len: usize,
}

unsafe fn text_units(p: isize) -> Vec<u32> {
    // SAFETY: the caller's EeWonCcText
    let t = unsafe { &*(p as *const EeWonCcText) };
    if t.len == 0 || t.units.is_null() {
        return Vec::new();
    }
    // SAFETY: `len` wchar_t units
    unsafe { std::slice::from_raw_parts(t.units as *const WChar, t.len) }.iter().map(|&u| u as u32).collect()
}

/// `*out` = the background: the original's fresh `Background` with the record's members, assigned.
fn bg_out(cx: &Cx, out: isize, bg: &EeWonCcBackground) {
    let o = out as *mut c_void;
    cx.call(EeWonCcOp::BgReset, o, 0, 0, 0, 0);
    cx.call(EeWonCcOp::BgImage, o, bg.image as isize, 0, 0, 0);
    cx.call(EeWonCcOp::BgColor, o, bg.color as isize, 0, 0, 0);
    cx.call(EeWonCcOp::BgBorder, o, bg.border_color as isize, 0, 0, 0);
    cx.call(EeWonCcOp::BgOffsets, o, bg.use_offsets as isize, 0, 0, 0);
    cx.call(EeWonCcOp::BgStretch, o, bg.stretch_image as isize, 0, 0, 0);
    cx.call(EeWonCcOp::BgGrab, o, bg.want_grab_bg as isize, 0, 0, 0);
}

/// The table reader's reads made on the config's own object (cfgtab.rs, R6 WON lobby and network round 39: the
/// C++ readers ReadChar, ReadFlag, ReadDirection, ReadHorzAlign, ReadVertAlign, ReadStandardCursor, ReadCursor,
/// ReadSelectionColor and ReadRawImage are gone). An int goes to `push`; a value the C++ frame holds is written at
/// the address `put(kind, value)` answers (a string slot, the selection colour) or handed to it (the raw image's
/// reference, the standard cursor). None: a read the C++ host makes.
pub(crate) fn tab_read(
    o: *mut c_void,
    frame: *mut c_void,
    kind: crate::cfgtab::EeWonCfgRead,
    push: &mut dyn FnMut(i32),
    put: &mut dyn FnMut(crate::cfgtab::EeWonCfgRead, isize) -> isize,
) -> Option<R<()>> {
    use crate::cfgtab::EeWonCfgRead as K;
    // SAFETY: the config's own object, alive while the C++ config runs its table
    let cfg = unsafe { &*(o as *const EeWonCompCfg) };
    let cx = Cx { host: cfg.host, frame };
    type Put<'a> = &'a mut dyn FnMut(K, isize) -> isize;
    let text = |put: Put, k: K, v: R<Vec<u32>>| -> R<()> { v.map(|v| set_text(&cx, put(k, 0), &v)) };
    let int = |v: R<i32>, push: &mut dyn FnMut(i32)| -> R<()> { v.map(push) };
    Some(match kind {
        K::Int => int(cfg.read_int(&cx), push),
        K::Bool => int(cfg.read_bool().map(i32::from), push),
        K::Color => int(cfg.read_color(&cx), push),
        K::Char => int(cfg.read_char(), push),
        K::VertAlign => int(cfg.read_vert_align(&cx), push),
        K::HorzAlign => int(cfg.read_horz_align(&cx), push),
        K::Direction => int(cfg.read_direction(&cx), push),
        K::StdCursor => int(cfg.read_standard_cursor(&cx), push),
        K::Str => text(put, K::Str, cfg.read_string(&cx, false)),
        K::Word => text(put, K::Word, cfg.read_string(&cx, true)),
        K::Flag => {
            let mut on = None;
            let v = cfg.read_flag(&cx, &mut on);
            text(put, K::Flag, v).map(|()| push(i32::from(on.unwrap_or(true))))
        }
        K::Cursor => cfg.read_cursor(&cx).map(|v| {
            put(K::Cursor, v as isize);
        }),
        K::SelColor => cfg.read_selection_color(&cx).map(|x| {
            let (op, fore) = match x.mode {
                0 => (EeWonCcOp::SelFore, x.foreground as isize),
                1 => (EeWonCcOp::SelInvert, 0),
                _ => (EeWonCcOp::SelOriginal, 0),
            };
            cx.call(op, put(K::SelColor, 0) as *mut c_void, x.background as isize, fore, 0, 0);
        }),
        K::RawImage => cfg.read_image(&cx, true).map(|p| {
            put(K::RawImage, p as isize);
        }),
        _ => return None,
    })
}

/// The table reader's `EnsureComma` and `EndOfString` on the config's own object.
pub(crate) fn tab_comma(o: *mut c_void) -> R<()> {
    // SAFETY: as tab_read's
    unsafe { &*(o as *const EeWonCompCfg) }.ensure_comma()
}
pub(crate) fn tab_end(o: *mut c_void) -> bool {
    // SAFETY: as tab_read's
    unsafe { &*(o as *const EeWonCompCfg) }.end_of_string()
}

unsafe fn dispatch(cx: &Cx, o: *mut c_void, call: EeWonCcCall, a: isize, b: isize, c: isize, d: isize) -> R<()> {
    use EeWonCcCall as C;
    let cfg = || -> &EeWonCompCfg {
        // SAFETY: a config call's object
        unsafe { &*(o as *const EeWonCompCfg) }
    };
    let res = || -> &EeWonResCfg {
        // SAFETY: a group call's object
        unsafe { &*(o as *const EeWonResCfg) }
    };
    let table = || -> &EeWonResTable {
        // SAFETY: a table call's object
        unsafe { &*(o as *const EeWonResTable) }
    };
    // SAFETY: the text arguments of the calls that have them
    let txt = || unsafe { bytes(b, c) };
    let key = || -> &[u8] {
        // SAFETY: the EeWonCcKey at `c`
        unsafe { key_at(c) }
    };
    // SAFETY (every `put`): the out pointers of the call
    unsafe {
        match call {
            C::ReadInt => put(a, cfg().read_int(cx)?),
            C::ParseExpression => put(b, cfg().parse_expression(cx, a as i32)?),
            C::GetIntValue => put(a, cfg().get_int_value(cx)?),
            C::ReadBool => put(a, cfg().read_bool()?),
            C::ReadChar => put(a, cfg().read_char()?),
            C::ReadFlag => {
                let mut on = None;
                let v = cfg().read_flag(cx, &mut on)?;
                if let (Some(on), true) = (on, b != 0) {
                    put(b, on);
                }
                set_text(cx, a, &v);
            }
            C::ReadRawString => set_text(cx, a, &cfg().read_raw_string(b != 0)),
            C::ReadString => set_text(cx, a, &cfg().read_string(cx, b != 0)?),
            C::ReadDirection => put(a, cfg().read_direction(cx)?),
            C::ReadHorzAlign => put(a, cfg().read_horz_align(cx)?),
            C::ReadVertAlign => put(a, cfg().read_vert_align(cx)?),
            C::ReadColor => put(a, cfg().read_color(cx)?),
            C::ReadSound => put(a, cfg().read_sound(cx)?),
            C::ReadBackground => {
                let bg = cfg().read_background(cx)?;
                bg_out(cx, a, &bg);
                cx.release(EeWonCcKind::Image, bg.image);
            }
            C::ReadFont => put(a, cfg().read_font(cx)?),
            C::ImageFromStr => put(a, cfg().image_from_str(cx, txt(), d != 0)?),
            C::ReadImage => put(a, cfg().read_image(cx, b != 0)?),
            C::ReadStandardCursor => put(a, cfg().read_standard_cursor(cx)?),
            C::ReadCursor => put(a, cfg().read_cursor(cx)?),
            C::ReadSelectionColor => {
                let x = cfg().read_selection_color(cx)?;
                let (op, fore) = match x.mode {
                    0 => (EeWonCcOp::SelFore, x.foreground as isize),
                    1 => (EeWonCcOp::SelInvert, 0),
                    _ => (EeWonCcOp::SelOriginal, 0),
                };
                cx.call(op, a as *mut c_void, x.background as isize, fore, 0, 0);
            }
            C::Missing => return Err(CcErr::Msg(missing_text(txt(), a as *const c_char, d as i32))),
            C::EndOfString => put(a, cfg().end_of_string()),
            C::SkipComma => cfg().skip_comma(),
            C::EnsureComma => cfg().ensure_comma()?,
            C::GetIntResource => put(a, cfg().int_resource(cx, txt())?),
            C::GetStringResource => set_text(cx, a, &cfg().string_resource(cx, txt())?),
            C::GetImageResource => put(a, cfg().image_resource(cx, txt())?),
            C::GetFontResource => put(a, cfg().font_resource(cx, txt())?),
            C::GetSoundResource => {
                let s = cfg().sound_resource(cx, txt())?;
                cx.hold(EeWonCcKind::Sound, s);
                put(a, s);
            }
            C::GetColorResource => put(a, cfg().color_resource(cx, txt())?),
            C::GetBackgroundResource => {
                let bg = cfg().background_resource(cx, txt())?;
                bg_out(cx, a, &bg);
            }
            C::GetNamedValue => match cfg().base_named_value(cx, txt()) {
                Some(v) => {
                    put(d, v);
                    put(a, true);
                }
                None => put(a, false),
            },
            C::Predefined => {
                let name = key_at(b);
                match cfgkind::predefined(cfg().kind(cx), name) {
                    Some((kind, part)) => {
                        // the original's order: the child's config, then the part of this config's component
                        put(c, cx.call(EeWonCcOp::NewOfKind, std::ptr::null_mut(), kind as isize, 0, 0, 0));
                        let component = cx.call(EeWonCcOp::CompOf, o_this(cfg()), 0, 0, 0, 0) as *mut c_void;
                        put(d, cx.call(EeWonCcOp::CompPart, component, part as isize, 0, 0, 0));
                        put(a, true);
                    }
                    None => put(a, false),
                }
            }
            C::PreParse => {
                let r = cfg().pre_parse(cx, b)?;
                if r && cfg().kind(cx) == EeWonCfgKind::WonSkin {
                    cx.call(EeWonCcOp::SkinEnsure, std::ptr::null_mut(), 0, 0, 0, 0);
                }
                put(a, r);
            }
            C::SetDesiredSize => cfg().set_desired_size(cx),
            C::HandleInstruction => put(a, cfg().handle_instruction(cx, b, bytes(c, d))?),
            C::GetComponentConfig => put(a, cfg().component_config(cx, &text_units(b), c != 0)?),
            C::ChildComponent => put(a, cfg().child_component(cx, txt())),
            C::ReadComponentConfig => put(a, cfg().read_component_config(cx)?),
            C::ReadComponent => put(a, cfg().read_component(cx)?),
            C::HandleMapComponent => put(a, cfg().handle_map_component(cx)?),
            C::AddChild => cfg().add_child(cx, &text_units(a), b as *mut c_void, c != 0),
            C::HandleModifyGroup => put(a, cfg().handle_modify_group(cx, b)?),
            C::HandleResourceGroup => put(a, cfg().handle_resource_group(cx, b)?),
            C::HandleModify => put(a, cfg().handle_modify(cx)?),
            C::HandleValueInstruction => put(a, cfg().handle_value(cx, b, key(), d as *mut EeWonCfgStringParser)?),
            C::BaseHandleInstruction => put(a, cfg().base_handle(cx, b, key(), d as *mut EeWonCfgStringParser)?),
            C::InitComponents => cfg().init_components(cx)?,
            C::ResHandleValue => put(a, res().handle_value(cx, b, key(), Sp(d as *mut EeWonCfgStringParser))?),
            C::ResPreParse => res().parse_count.set(res().parse_count.get().wrapping_add(1)),
            C::ResSetAllowOverwrite => res().allow_overwrite.set(a != 0),
            C::ResGetString => match res().get_string(txt()) {
                Some(v) => {
                    set_text(cx, d, &v);
                    put(a, true);
                }
                None => put(a, false),
            },
            C::ResGetPtr => {
                let map = match d {
                    0 => &res().images,
                    1 => &res().fonts,
                    _ => &res().sounds,
                };
                let out = &mut *(a as *mut EeWonCcPtrOut);
                match EeWonResCfg::get_ptr(map, txt()) {
                    Some(p) => {
                        out.found = true;
                        out.ptr = p;
                    }
                    None => out.found = false,
                }
            }
            C::ResGetColor => match res().colors.borrow().get(txt()) {
                Some(v) => {
                    put(d, v.0);
                    put(a, true);
                }
                None => put(a, false),
            },
            C::ResGetBackground => {
                let v = res().backgrounds.borrow().get(txt()).map(|v| v.0);
                match v {
                    Some(bg) => {
                        bg_out(cx, d, &bg);
                        put(a, true);
                    }
                    None => put(a, false),
                }
            }
            C::ResGetInt => match res().ints.borrow().get(txt()) {
                Some(v) => {
                    put(d, v.0);
                    put(a, true);
                }
                None => put(a, false),
            },
            C::ResSafeGetString => match res().get_string(txt()) {
                Some(v) => set_text(cx, a, &v),
                None => return res().not_found(cx, ResKind::String, txt()),
            },
            C::ResSafeGetPtr => {
                let (map, kind) =
                    if d == 0 { (&res().images, ResKind::Image) } else { (&res().sounds, ResKind::Sound) };
                match EeWonResCfg::get_ptr(map, txt()) {
                    Some(p) => put(a, p),
                    None => return res().not_found(cx, kind, txt()),
                }
            }
            C::ResAddSound => {
                let s = d as *mut c_void;
                cx.hold(EeWonCcKind::Sound, s);
                let gen = res().parse_count.get();
                let old = res().sounds.borrow_mut().set(txt(), (s, gen));
                if let Some((p, _)) = old {
                    cx.release(EeWonCcKind::Sound, p);
                }
            }
            C::SafeReadColor => put(a, safe_read_color(Sp(b as *mut EeWonCfgStringParser))?),
            C::SafeReadFontStyle => put(a, safe_read_font_style(Sp(b as *mut EeWonCfgStringParser))?),
            C::SafeReadFont => put(a, safe_read_font(cx, Sp(b as *mut EeWonCfgStringParser))?),
            C::SafeReadSound => put(a, safe_read_sound(cx, Sp(b as *mut EeWonCfgStringParser))?),
            C::SafeReadImage => put(a, safe_read_image(cx, Sp(b as *mut EeWonCfgStringParser))?),
            C::SafeReadBackground => {
                let bg = safe_read_background(cx, Sp(b as *mut EeWonCfgStringParser))?;
                bg_out(cx, a, &bg);
                cx.release(EeWonCcKind::Image, bg.image);
            }
            C::TableAddConfig => {
                let (g, existed) = table().add_config(cx, txt());
                if d != 0 {
                    put(d, existed);
                }
                put(a, g);
            }
            C::TableSafeGetConfig => put(a, table().safe_get(txt())?),
            C::TableRegisterGlue => {
                let mut g = table().glue.borrow_mut();
                if let Err(i) = g.binary_search(&(a as usize)) {
                    g.insert(i, a as usize);
                }
            }
            C::TableGlue => table().glue_all(cx)?,
            C::AddAllocator => add_allocator(txt(), Make::Fn(a as usize), Make::Fn(d as usize)),
            C::RegisterControlId => register_control_id(txt(), a as i32),
            C::AllocateConfig => match allocate(cx, txt())? {
                Some(two) => {
                    *(d as *mut [*mut c_void; 2]) = two;
                    put(a, true);
                }
                None => put(a, false),
            },
            C::GetControlId => {
                let v = control_ids().get(txt()).copied();
                match v {
                    Some(v) => {
                        put(d, v);
                        put(a, true);
                    }
                    None => put(a, false),
                }
            }
            C::TableClearGlue => table().glue.borrow_mut().clear(),
            C::ClearAllocators => {
                allocators().take();
            }
            C::ClearControlIds => {
                control_ids().take();
            }
            C::TableClearResources => table().clear_resources(cx),
            C::SetResourceFolder => set_resource_folder(txt()),
            C::CallNamedFunction => {
                let x = &*(b as *const EeWonCcCallArgs);
                let params = if x.count == 0 { &[][..] } else { std::slice::from_raw_parts(x.params, x.count) };
                let r = call_named_function(bytes(x.key as isize, x.key_len as isize), params);
                if let Some(v) = r {
                    *x.out = v;
                }
                put(a, r.is_some());
            }
            C::GetResourceFile => {
                let f = resource_file(txt());
                let sink: EeWonCcSink = std::mem::transmute::<isize, EeWonCcSink>(d);
                if let Some(s) = sink {
                    s(a as *mut c_void, f.as_ptr(), f.len());
                }
            }
        }
    }
    Ok(())
}

#[cfg(test)]
#[path = "compcfg_tests.rs"]
mod tests;
