//! WONInternal's `TitleConfig` and `CommunityConfig` whole in Rust (WON round 22; TitleConfig.cpp, WONTitleConfig.h
//! deleted): the config objects, their typed key values, their hooks (`PreParse`'s copy of the parent title's
//! keys, `HandleInstruction`'s children and nesting errors, `CommunityConfig::PostParse`'s rules), the dummy
//! community and both `ToString` texts. The rules themselves are `titlecfg`'s.
//!
//! The objects are `cfgobject` groups whose owner and host are Rust ([`title_host`]): the group's calls into its
//! object (the hooks, the name, the parent, references), into the typed values (bound to the object's fields, as
//! the C++ `ConfigValue<T>` bound to the members) and into the parser (`p` is a Rust [`EeWonCfgParser`] here) are
//! answered without C++. A plain object (a skipped group's) is one of kind [`EeWonTitleObjKind::Plain`].
//!
//! Reached by nothing in the game (the auth server's title files; the WON servers are gone): the C ABI below is
//! the golden driver's (tests/won/won_titleobj_test.cpp, the C++ classes as they were against these).

use std::ffi::c_void;

use crate::cfgobject::{
    self, ee_won_cfgo_copy_key_vals_from, ee_won_cfgo_free, ee_won_cfgo_new, ee_won_cfgo_register_key_value, Cx,
    EeWonCfgObject, EeWonCfgStringParser, EeWonCfgoOp, Exc,
};
use crate::cfgparser::EeWonCfgParser;
use crate::ffi::EeWonByteSink;
use crate::ffi_parse::{
    ee_won_parser_check_next_char, ee_won_parser_format_bool, ee_won_parser_format_i64, ee_won_parser_read_bool,
    ee_won_parser_read_int, ee_won_parser_read_string, ee_won_parser_read_wstring,
};
use crate::titlecfg::{self, EeWonTitleFlags, EeWonTitleInstr, Title};

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = u32;

/// The object's class: a plain `ConfigObject`, a `TitleConfig` or a `CommunityConfig`.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonTitleObjKind {
    Plain,
    Title,
    Community,
}

/// `TitleConfig`'s members (WONTitleConfig.h, in its order).
#[derive(Default, Clone, Debug, PartialEq, Eq)]
pub struct TitleFields {
    pub already_in_title: bool,
    pub network: Vec<u8>,
    pub net_port: u16,
    pub force_cd_key: bool,
    pub image_path: Vec<u8>,
    pub hash_string: Vec<u8>,
    pub key_prefix: Vec<u8>,
    pub require_crc: bool,
    pub allow_beta_key: bool,
    pub require_cd_key: bool,
    pub fail_to_guest: bool,
    pub allow_guest: bool,
    pub concurrent_user_login: bool,
    pub concurrent_cd_key_login: bool,
    pub get_user_data: bool,
    pub get_forgeable_user_data: bool,
    pub cd_key_for_invite: bool,
    pub save_session_key: bool,
    pub get_key_id: bool,
    pub ascii_account_names: bool,
    pub admin_community_name: Vec<WChar>,
}

/// `CommunityConfig`'s own members.
#[derive(Default, Clone, Debug, PartialEq, Eq)]
pub struct CommunityFields {
    pub already_in_community: bool,
    pub community_id: i32,
    pub community_name: Vec<WChar>,
    pub name_login_str: Vec<u8>,
    pub name_login: i32,
    /// `mTrustLevelLifespanMap`, in key order.
    pub trust_level_lifespans: Vec<(i32, i32)>,
    pub certificate_life_span_list: Vec<Vec<i32>>,
}

/// One config object (a `RefCount`: made with no reference).
pub struct EeWonTitleObj {
    refs: i32,
    kind: EeWonTitleObjKind,
    name: Vec<u8>,
    base_name: Vec<u8>,
    parent: *mut EeWonTitleObj,
    obj: *mut EeWonCfgObject,
    pub title: TitleFields,
    pub community: CommunityFields,
}

impl Drop for EeWonTitleObj {
    fn drop(&mut self) {
        // ~ConfigObject: the group (it releases the values, which point into this object, and the children)
        unsafe { ee_won_cfgo_free(self.obj) };
    }
}

/// A key's typed value (`SimpleConfigValue<T>`, `ConfigStringValue<T>`, `ConfigListValue<T>`), bound to a field.
enum Val {
    Bool(*mut bool, bool),
    U16(*mut u16, u16),
    Int(*mut i32, i32),
    Str(*mut Vec<u8>, Option<Vec<u8>>),
    WStr(*mut Vec<WChar>, Option<Vec<WChar>>),
    IntListList(*mut Vec<Vec<i32>>, Option<Vec<u8>>, bool),
}

struct ValObj {
    refs: i32,
    is_default: bool,
    v: Val,
}

/// The exception record of a Rust entry (the C++ `CfgExc`).
#[derive(Default)]
struct ExcRecord {
    kind: i32,
    flag: bool,
    text: Vec<u8>,
}

unsafe extern "C" fn sink_vec(context: *mut c_void, bytes: *const u8, length: usize) {
    let out = unsafe { &mut *(context as *mut Vec<u8>) };
    if !bytes.is_null() && length > 0 {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

unsafe extern "C" fn sink_units(context: *mut c_void, units: *const u32, count: usize) {
    let out = unsafe { &mut *(context as *mut Vec<WChar>) };
    if !units.is_null() {
        // `*(std::wstring*) += (wchar_t)unit`
        out.extend(unsafe { std::slice::from_raw_parts(units, count) }.iter().map(|&u| u as WChar));
    }
}

fn give(sink: EeWonByteSink, context: *mut c_void, bytes: &[u8]) {
    unsafe { sink(context, bytes.as_ptr(), bytes.len()) };
}

/// `WStringToString`: the text to its first NUL as the game's UTF-8.
pub fn narrow(w: &[WChar]) -> Vec<u8> {
    let w = &w[..w.iter().position(|&u| u == 0).unwrap_or(w.len())];
    #[cfg(windows)]
    {
        crate::text::game_from_utf16(w)
    }
    #[cfg(not(windows))]
    {
        crate::text::game_from_utf32(w)
    }
}

/// `StringToWString`: the text to its first NUL widened.
pub fn widen(s: &[u8]) -> Vec<WChar> {
    let s = &s[..s.iter().position(|&b| b == 0).unwrap_or(s.len())];
    #[cfg(windows)]
    {
        crate::text::utf16_from_game(s)
    }
    #[cfg(not(windows))]
    {
        crate::text::utf32_from_game(s)
    }
}

fn sp_unit(sp: &EeWonCfgStringParser) -> u32 {
    if sp.wide {
        std::mem::size_of::<WChar>() as u32
    } else {
        1
    }
}

/// `StringParser::ReadValue(std::list<int>&)` (ReadList): the elements up to a failed one (dropped).
fn read_int_list(sp: &mut EeWonCfgStringParser, list: &mut Vec<i32>) -> bool {
    let unit = sp_unit(sp);
    loop {
        let mut v = 0i32;
        if !unsafe { ee_won_parser_read_int(sp.text, unit, &mut sp.pos, &mut v) } {
            return false;
        }
        list.push(v);
        if !unsafe { ee_won_parser_check_next_char(sp.text, unit, &mut sp.pos, b',' as i32) } {
            return true;
        }
    }
}

/// `StringParser::ReadValue(std::list<std::list<int> >&)`: "(1, 2), (3)"; the groups read so far stay.
fn read_int_list_list(sp: &mut EeWonCfgStringParser, list: &mut Vec<Vec<i32>>) -> bool {
    let unit = sp_unit(sp);
    while unsafe { ee_won_parser_check_next_char(sp.text, unit, &mut sp.pos, b'(' as i32) } {
        list.push(Vec::new());
        let ok = read_int_list(sp, list.last_mut().unwrap());
        if !ok || !unsafe { ee_won_parser_check_next_char(sp.text, unit, &mut sp.pos, b')' as i32) } {
            return false;
        }
        if !unsafe { ee_won_parser_check_next_char(sp.text, unit, &mut sp.pos, b',' as i32) } {
            break;
        }
    }
    true
}

fn format_i64(v: i64, out: &mut Vec<u8>) {
    unsafe { ee_won_parser_format_i64(v, sink_vec, out as *mut Vec<u8> as *mut c_void) };
}

impl ValObj {
    /// `InitializeDefaultHook`.
    unsafe fn init(&mut self) {
        unsafe {
            match &self.v {
                Val::Bool(p, d) => **p = *d,
                Val::U16(p, d) => **p = *d,
                Val::Int(p, d) => **p = *d,
                Val::Str(p, d) => **p = d.clone().unwrap_or_default(),
                Val::WStr(p, d) => **p = d.clone().unwrap_or_default(),
                Val::IntListList(p, d, _) => {
                    (**p).clear();
                    if let Some(d) = d {
                        let mut t = d.clone();
                        t.push(0);
                        let mut sp = EeWonCfgStringParser { text: t.as_ptr() as *const c_void, pos: 0, wide: false };
                        read_int_list_list(&mut sp, &mut **p);
                    }
                }
            }
        }
    }
    /// `ReadValue`.
    unsafe fn read(&mut self, sp: &mut EeWonCfgStringParser) -> bool {
        let unit = sp_unit(sp);
        unsafe {
            match &self.v {
                Val::Bool(p, _) => ee_won_parser_read_bool(sp.text, unit, &mut sp.pos, *p),
                Val::U16(p, _) => {
                    let mut v = 0i32;
                    let r = ee_won_parser_read_int(sp.text, unit, &mut sp.pos, &mut v);
                    if r {
                        **p = v as u16;
                    }
                    r
                }
                Val::Int(p, _) => {
                    let mut v = 0i32;
                    if !ee_won_parser_read_int(sp.text, unit, &mut sp.pos, &mut v) {
                        return false;
                    }
                    **p = v;
                    true
                }
                Val::Str(p, _) => {
                    (**p).clear();
                    ee_won_parser_read_string(sp.text, unit, &mut sp.pos, false, sink_vec, *p as *mut c_void);
                    true
                }
                Val::WStr(p, _) => {
                    (**p).clear();
                    ee_won_parser_read_wstring(sp.text, unit, &mut sp.pos, false, sink_units, *p as *mut c_void);
                    true
                }
                Val::IntListList(p, _, additive) => {
                    if !*additive {
                        (**p).clear();
                    }
                    read_int_list_list(sp, &mut **p)
                }
            }
        }
    }
    /// `WriteValue`: the value's text appended.
    unsafe fn write(&self, out: &mut Vec<u8>) {
        unsafe {
            match &self.v {
                Val::Bool(p, _) => ee_won_parser_format_bool(**p, sink_vec, out as *mut Vec<u8> as *mut c_void),
                Val::U16(p, _) => format_i64(**p as i64, out),
                Val::Int(p, _) => format_i64(**p as i64, out),
                Val::Str(p, _) => out.extend_from_slice(&**p),
                Val::WStr(p, _) => out.extend_from_slice(&narrow(&**p)),
                Val::IntListList(p, _, _) => {
                    for (i, l) in (**p).iter().enumerate() {
                        out.push(b'(');
                        for (j, v) in l.iter().enumerate() {
                            format_i64(*v as i64, out);
                            if j + 1 < l.len() {
                                out.extend_from_slice(b", ");
                            }
                        }
                        out.push(b')');
                        if i + 1 < (**p).len() {
                            out.extend_from_slice(b", ");
                        }
                    }
                }
            }
        }
    }
    /// `CopyValueHook`: the value copied from a value of the same type (`dynamic_cast<ConfigValue<T>*>`).
    unsafe fn copy_from(&mut self, from: &ValObj) -> bool {
        unsafe {
            match (&self.v, &from.v) {
                (Val::Bool(a, _), Val::Bool(b, _)) => **a = **b,
                (Val::U16(a, _), Val::U16(b, _)) => **a = **b,
                (Val::Int(a, _), Val::Int(b, _)) => **a = **b,
                (Val::Str(a, _), Val::Str(b, _)) => **a = (**b).clone(),
                (Val::WStr(a, _), Val::WStr(b, _)) => **a = (**b).clone(),
                (Val::IntListList(a, _, _), Val::IntListList(b, _, _)) => **a = (**b).clone(),
                _ => return false,
            }
        }
        true
    }
}

fn cx(x: *mut c_void) -> Cx {
    Cx { host: title_host, x }
}

impl EeWonTitleObj {
    fn alloc(kind: EeWonTitleObjKind) -> *mut EeWonTitleObj {
        let me = Box::into_raw(Box::new(EeWonTitleObj {
            refs: 0,
            kind,
            name: Vec::new(),
            base_name: Vec::new(),
            parent: std::ptr::null_mut(),
            obj: std::ptr::null_mut(),
            title: TitleFields::default(),
            community: CommunityFields::default(),
        }));
        unsafe { (*me).obj = ee_won_cfgo_new(me as *mut c_void, title_host) };
        me
    }

    fn key(me: *mut EeWonTitleObj, key: &str, v: Val) {
        let val = Box::into_raw(Box::new(ValObj { refs: 0, is_default: false, v }));
        unsafe { ee_won_cfgo_register_key_value((*me).obj, key.as_ptr(), key.len(), val as *mut c_void) };
    }

    /// `new ConfigObject`.
    pub fn new_plain() -> *mut EeWonTitleObj {
        Self::alloc(EeWonTitleObjKind::Plain)
    }

    fn register_title(me: *mut EeWonTitleObj) {
        // the values point into the live boxed object's fields while the group holds them
        macro_rules! f {
            ($field:ident) => {
                unsafe { std::ptr::addr_of_mut!((*me).title.$field) }
            };
        }
        Self::key(me, "ForceCDKey", Val::Bool(f!(force_cd_key), false));
        Self::key(me, "RequireCRC", Val::Bool(f!(require_crc), false));
        Self::key(me, "AllowBetaKey", Val::Bool(f!(allow_beta_key), false));
        Self::key(me, "RequireCDKey", Val::Bool(f!(require_cd_key), false));
        Self::key(me, "FailToGuestLogin", Val::Bool(f!(fail_to_guest), true));
        Self::key(me, "AllowGuestLogin", Val::Bool(f!(allow_guest), false));
        Self::key(me, "ConcurrentUserLogin", Val::Bool(f!(concurrent_user_login), true));
        Self::key(me, "ConcurrentCDKeyLogin", Val::Bool(f!(concurrent_cd_key_login), false));
        Self::key(me, "GetUserData", Val::Bool(f!(get_user_data), false));
        Self::key(me, "GetForgeableUserData", Val::Bool(f!(get_forgeable_user_data), false));
        Self::key(me, "CDKeyForInvite", Val::Bool(f!(cd_key_for_invite), false));
        Self::key(me, "SaveSessionKey", Val::Bool(f!(save_session_key), false));
        Self::key(me, "GetKeyID", Val::Bool(f!(get_key_id), false));
        Self::key(me, "ASCIIAccountNames", Val::Bool(f!(ascii_account_names), false));
        Self::key(me, "NetPort", Val::U16(f!(net_port), 15111));
        Self::key(me, "Network", Val::Str(f!(network), None));
        Self::key(me, "ImagePath", Val::Str(f!(image_path), None));
        Self::key(me, "HashString", Val::Str(f!(hash_string), None));
        Self::key(me, "KeyPrefix", Val::Str(f!(key_prefix), None));
        Self::key(me, "AdminCommunityName", Val::WStr(f!(admin_community_name), Some(Vec::new())));
    }

    /// `new TitleConfig(alreadyInTitle)`.
    pub fn new_title(already_in_title: bool) -> *mut EeWonTitleObj {
        let me = Self::alloc(EeWonTitleObjKind::Title);
        unsafe { (*me).title.already_in_title = already_in_title };
        Self::register_title(me);
        me
    }

    /// `new CommunityConfig(alreadyInCommunity)`.
    pub fn new_community(already_in_community: bool) -> *mut EeWonTitleObj {
        let me = Self::alloc(EeWonTitleObjKind::Community);
        Self::register_title(me);
        unsafe { (*me).community.already_in_community = already_in_community };
        let (id, list, login) = unsafe {
            (
                std::ptr::addr_of_mut!((*me).community.community_id),
                std::ptr::addr_of_mut!((*me).community.certificate_life_span_list),
                std::ptr::addr_of_mut!((*me).community.name_login_str),
            )
        };
        Self::key(me, "CommunityID", Val::Int(id, 0));
        Self::key(me, "CertificateLifeSpan", Val::IntListList(list, None, true));
        Self::key(me, "NameLogin", Val::Str(login, Some(b"Required".to_vec())));
        me
    }

    /// `new CommunityConfig(theCommunityName, theCommunityID)`, the authentication's dummy: the title's keys only
    /// (the original registered no community key here), reset, then the name and id and no guest fallback. The
    /// original left `mAlreadyInCommunity` and `mNameLogin` uninitialised: false and 0 here.
    pub fn new_dummy_community(name: &[WChar], id: u32) -> *mut EeWonTitleObj {
        let me = Self::alloc(EeWonTitleObjKind::Community);
        Self::register_title(me);
        unsafe {
            cfgobject::reset(cx(std::ptr::null_mut()), (*me).obj, true);
            (*me).community.community_name = name.to_vec();
            (*me).community.community_id = id as i32;
            (*me).title.fail_to_guest = false;
        }
        me
    }

    fn is_title(me: *mut EeWonTitleObj) -> bool {
        !me.is_null() && unsafe { (*me).kind } != EeWonTitleObjKind::Plain
    }

    /// `PreParse`: a title or community copies its parent title's (or community's) keys.
    fn pre_parse(me: *mut EeWonTitleObj) -> bool {
        let parent = unsafe { (*me).parent };
        if Self::is_title(parent) {
            unsafe { ee_won_cfgo_copy_key_vals_from((*me).obj, (*parent).obj) };
        }
        true
    }

    /// A child title or community parsed and added (`ParseTitleChild`, `ParseCommunityChild`).
    fn parse_new_child(cx: Cx, me: *mut EeWonTitleObj, p: *mut c_void, child: *mut EeWonTitleObj) -> Result<bool, Exc> {
        unsafe { title_host(EeWonCfgoOp::ObjAddRef, child as *mut c_void, 0, 0, 0, 0, cx.x) };
        let r = cfgobject::parse_child(cx, unsafe { (*me).obj }, p, child as *mut c_void, true, true);
        unsafe { title_host(EeWonCfgoOp::ObjRelease, child as *mut c_void, 0, 0, 0, 0, cx.x) };
        r
    }

    /// `HandleInstruction`: the base's, else a title's or community's child or a nesting error.
    fn handle_instruction(cx: Cx, me: *mut EeWonTitleObj, p: *mut c_void, ty: &[u8]) -> Result<bool, Exc> {
        if cfgobject::handle_instruction(cx, unsafe { (*me).obj }, p, ty)? {
            return Ok(true);
        }
        let (community, already) = unsafe {
            match (*me).kind {
                EeWonTitleObjKind::Plain => return Ok(false),
                EeWonTitleObjKind::Title => (false, (*me).title.already_in_title),
                EeWonTitleObjKind::Community => (true, (*me).community.already_in_community),
            }
        };
        match titlecfg::instruction(community, already, ty) {
            EeWonTitleInstr::NotMine => Ok(false),
            EeWonTitleInstr::ChildTitle => Self::parse_new_child(cx, me, p, Self::new_title(true)),
            EeWonTitleInstr::ChildCommunity => Self::parse_new_child(cx, me, p, Self::new_community(community)),
            verdict => {
                cx.parser(p).set_error(titlecfg::instr_error(verdict).as_bytes(), false);
                Ok(false)
            }
        }
    }

    /// `CommunityConfig::PostParse`: the name-login mode, the image path and hash string defaults (from the
    /// parent title's name, else this one's), the lifespan map and the community name.
    fn post_parse(me: *mut EeWonTitleObj) -> bool {
        let parent = unsafe { (*me).parent };
        let parent_name = if Self::is_title(parent) { Some(unsafe { (*parent).name.clone() }) } else { None };
        let o = unsafe { &mut *me };
        if o.kind != EeWonTitleObjKind::Community {
            return true;
        }
        o.community.name_login = titlecfg::name_login(&o.community.name_login_str);
        let name = parent_name.unwrap_or_else(|| o.name.clone());
        if o.title.image_path.is_empty() {
            o.title.image_path = [b"./".as_slice(), &name].concat();
        }
        if o.title.hash_string.is_empty() {
            o.title.hash_string = name;
        }
        let mut flat = Vec::new();
        for l in &o.community.certificate_life_span_list {
            flat.push(l.len() as i32);
            flat.extend_from_slice(l);
        }
        o.community.trust_level_lifespans = titlecfg::lifespans(&flat);
        if o.community.community_name.is_empty() {
            o.community.community_name = widen(&o.name);
        }
        true
    }

    fn flags(&self) -> EeWonTitleFlags {
        let t = &self.title;
        EeWonTitleFlags {
            force_cd_key: t.force_cd_key,
            require_crc: t.require_crc,
            allow_beta_key: t.allow_beta_key,
            require_cd_key: t.require_cd_key,
            fail_to_guest: t.fail_to_guest,
            allow_guest: t.allow_guest,
            concurrent_user_login: t.concurrent_user_login,
            concurrent_cd_key_login: t.concurrent_cd_key_login,
            get_user_data: t.get_user_data,
            get_forgeable_user_data: t.get_forgeable_user_data,
            cd_key_for_invite: t.cd_key_for_invite,
            save_session_key: t.save_session_key,
            get_key_id: t.get_key_id,
            ascii_account_names: t.ascii_account_names,
        }
    }

    fn title_text(&self, with_name: bool) -> Title<'_> {
        Title {
            name: if with_name { Some(&self.name) } else { None },
            network: &self.title.network,
            net_port: self.title.net_port,
            image_path: &self.title.image_path,
            hash_string: &self.title.hash_string,
            key_prefix: &self.title.key_prefix,
            flags: self.flags(),
        }
    }

    /// `TitleConfig::ToString(includeName)`, or `CommunityConfig::ToString()` for a community.
    pub fn to_text(&self, include_name: bool) -> Vec<u8> {
        if self.kind == EeWonTitleObjKind::Community {
            let header = if self.community.community_name.is_empty() {
                self.name.clone()
            } else {
                narrow(&self.community.community_name)
            };
            titlecfg::community_text(
                &header,
                self.community.community_id,
                &self.title_text(false),
                &self.community.name_login_str,
                &self.community.trust_level_lifespans,
            )
        } else {
            titlecfg::title_text(&self.title_text(include_name))
        }
    }
}

/// The group's host for a Rust object (`o` an [`EeWonTitleObj`] or a value, `p` a Rust parser, `x` an
/// [`ExcRecord`] or NULL).
///
/// # Safety
/// The group's contract: every op on live objects of this module.
pub unsafe extern "C" fn title_host(
    op: EeWonCfgoOp,
    o: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    x: *mut c_void,
) -> isize {
    type Op = EeWonCfgoOp;
    let me = o as *mut EeWonTitleObj;
    let v = o as *mut ValObj;
    let ret = |r: Result<bool, Exc>| match r {
        Ok(b) => b as isize,
        Err(Exc::Config) => -1,
        Err(Exc::File) => -2,
    };
    unsafe {
        match op {
            Op::ObjAddRef => (*me).refs += 1,
            Op::ObjRelease => {
                (*me).refs -= 1;
                if (*me).refs <= 0 {
                    drop(Box::from_raw(me));
                }
            }
            Op::ObjNew => return EeWonTitleObj::new_plain() as isize,
            Op::ObjFactory => {
                let f: extern "C" fn() -> *mut EeWonTitleObj = std::mem::transmute(a);
                return f() as isize;
            }
            Op::ObjRust => return (*me).obj as isize,
            Op::ObjPreParse => return EeWonTitleObj::pre_parse(me) as isize,
            Op::ObjPostParse => return EeWonTitleObj::post_parse(me) as isize,
            Op::ObjHandleInstruction => {
                let ty = std::slice::from_raw_parts(b as *const u8, c as usize).to_vec();
                return ret(EeWonTitleObj::handle_instruction(cx(x), me, a as *mut c_void, &ty));
            }
            Op::ObjHandleValueInstruction => {
                let key = std::slice::from_raw_parts(b as *const u8, c as usize).to_vec();
                return ret(cfgobject::handle_value_instruction(
                    cx(x),
                    (*me).obj,
                    a as *mut c_void,
                    &key,
                    d as *mut EeWonCfgStringParser,
                ));
            }
            Op::ObjReset => cfgobject::reset(cx(x), (*me).obj, a != 0),
            Op::ObjSetParent => (*me).parent = a as *mut EeWonTitleObj,
            Op::ObjSetName => {
                let t =
                    if c > 0 { std::slice::from_raw_parts(b as *const u8, c as usize).to_vec() } else { Vec::new() };
                if a != 0 {
                    (*me).base_name = t
                } else {
                    (*me).name = t
                }
            }
            Op::ValAddRef => (*v).refs += 1,
            Op::ValRelease => {
                (*v).refs -= 1;
                if (*v).refs <= 0 {
                    drop(Box::from_raw(v));
                }
            }
            Op::ValInit => (*v).init(),
            Op::ValRead => return (*v).read(&mut *(a as *mut EeWonCfgStringParser)) as isize,
            Op::ValWrite => {
                let mut s = Vec::new();
                (*v).write(&mut s);
                let sink: EeWonByteSink = std::mem::transmute(a);
                give(sink, b as *mut c_void, &s);
            }
            Op::ValCopy => {
                let from = &*(a as *const ValObj);
                if (*v).copy_from(from) {
                    (*v).is_default = from.is_default;
                }
            }
            Op::ValIsDefault => return (*v).is_default as isize,
            Op::ValSetDefault => (*v).is_default = a != 0,
            Op::GroupSet => {
                // `SmartPtr<ConfigObject>` assignment: the new one held, the old one released
                let slot = a as *mut *mut EeWonTitleObj;
                let new = b as *mut EeWonTitleObj;
                if !new.is_null() {
                    title_host(Op::ObjAddRef, new as *mut c_void, 0, 0, 0, 0, x);
                }
                let old = *slot;
                *slot = new;
                if !old.is_null() {
                    title_host(Op::ObjRelease, old as *mut c_void, 0, 0, 0, 0, x);
                }
            }
            Op::ParserNew => return Box::into_raw(Box::new(EeWonCfgParser::new())) as isize,
            Op::ParserFree => drop(Box::from_raw(a as *mut EeWonCfgParser)),
            Op::ParserRust => return a,
            Op::SetExc => {
                if let Some(e) = (x as *mut ExcRecord).as_mut() {
                    e.kind = a as i32;
                    e.flag = b != 0;
                    e.text = if d > 0 {
                        std::slice::from_raw_parts(c as *const u8, d as usize).to_vec()
                    } else {
                        Vec::new()
                    };
                }
            }
            Op::TakeExc => {
                if let Some(e) = (x as *mut ExcRecord).as_mut() {
                    let sink: EeWonByteSink = std::mem::transmute(a);
                    give(sink, b as *mut c_void, &e.text);
                    let f = e.flag;
                    e.kind = 0;
                    e.text.clear();
                    return f as isize;
                }
            }
        }
    }
    0
}

// ---- the C ABI (the golden driver's: tests/won/won_titleobj_test.cpp)

/// A new object of `kind` (`already`: `alreadyInTitle` / `alreadyInCommunity`), holding one reference.
#[no_mangle]
pub extern "C" fn ee_won_titleobj_new(kind: EeWonTitleObjKind, already: bool) -> *mut EeWonTitleObj {
    let me = match kind {
        EeWonTitleObjKind::Plain => EeWonTitleObj::new_plain(),
        EeWonTitleObjKind::Title => EeWonTitleObj::new_title(already),
        EeWonTitleObjKind::Community => EeWonTitleObj::new_community(already),
    };
    unsafe { (*me).refs = 1 };
    me
}

/// `new CommunityConfig(name, id)` (`name` `count` `wchar_t`s), holding one reference.
///
/// # Safety
/// `name` holds `count` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_new_dummy(name: *const c_void, count: usize, id: u32) -> *mut EeWonTitleObj {
    let name =
        if name.is_null() { &[][..] } else { unsafe { std::slice::from_raw_parts(name as *const WChar, count) } };
    let me = EeWonTitleObj::new_dummy_community(name, id);
    unsafe { (*me).refs = 1 };
    me
}

/// `AddRef` (`add` true) or `Release`.
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_ref(o: *mut EeWonTitleObj, add: bool) {
    let op = if add { EeWonCfgoOp::ObjAddRef } else { EeWonCfgoOp::ObjRelease };
    unsafe { title_host(op, o as *mut c_void, 0, 0, 0, 0, std::ptr::null_mut()) };
}

/// The object's group (for `cfgobject`'s queries: the keys, the children).
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_group(o: *mut EeWonTitleObj) -> *mut EeWonCfgObject {
    unsafe { (*o).obj }
}

/// The object's kind.
///
/// # Safety
/// `o` is a live object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_kind(o: *mut EeWonTitleObj) -> EeWonTitleObjKind {
    unsafe { (*o).kind }
}

/// `ParseTopLevel` of the file `path` (`n` bytes) on the parser `p`: 1 or 0, or -2 for a `FileReaderException`
/// (its text to the sink).
///
/// # Safety
/// `o` is a live object, `p` a live parser; `path` holds `n` bytes; the sink is valid with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_parse(
    o: *mut EeWonTitleObj,
    p: *mut EeWonCfgParser,
    path: *const u8,
    n: usize,
    exc: EeWonByteSink,
    exc_cx: *mut c_void,
) -> i32 {
    let path = if n == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(path, n) } };
    let mut rec = ExcRecord::default();
    let r = cfgobject::parse_top_level(
        cx(&mut rec as *mut ExcRecord as *mut c_void),
        unsafe { (*o).obj },
        p as *mut c_void,
        Some(path),
    );
    match r {
        Ok(ok) => ok as i32,
        Err(_) => {
            give(exc, exc_cx, &rec.text);
            -2
        }
    }
}

/// `ToString` (`include_name` a title's flag) to the sink.
///
/// # Safety
/// `o` is a live object; the sink is valid with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_text(
    o: *mut EeWonTitleObj,
    include_name: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    give(sink, context, &unsafe { &*o }.to_text(include_name));
}

/// The object's members as one text (the driver prints the C++ object's the same way): the kind, the names, every
/// member in declaration order (bytes in hex, wide texts as hex units, lists in brackets).
///
/// # Safety
/// `o` is a live object; the sink is valid with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_won_titleobj_dump(o: *mut EeWonTitleObj, sink: EeWonByteSink, context: *mut c_void) {
    give(sink, context, &dump(unsafe { &*o }));
}

fn hex(out: &mut Vec<u8>, b: &[u8]) {
    for c in b {
        out.extend_from_slice(format!("{c:02x}").as_bytes());
    }
    out.push(b';');
}

/// [`ee_won_titleobj_dump`]'s text.
pub fn dump(o: &EeWonTitleObj) -> Vec<u8> {
    let mut s = format!("k{};", o.kind as i32).into_bytes();
    hex(&mut s, &o.name);
    hex(&mut s, &o.base_name);
    if o.kind == EeWonTitleObjKind::Plain {
        return s;
    }
    let t = &o.title;
    s.extend_from_slice(format!("a{};", t.already_in_title as i32).as_bytes());
    hex(&mut s, &t.network);
    s.extend_from_slice(format!("p{};", t.net_port).as_bytes());
    hex(&mut s, &t.image_path);
    hex(&mut s, &t.hash_string);
    hex(&mut s, &t.key_prefix);
    for f in [
        t.force_cd_key,
        t.require_crc,
        t.allow_beta_key,
        t.require_cd_key,
        t.fail_to_guest,
        t.allow_guest,
        t.concurrent_user_login,
        t.concurrent_cd_key_login,
        t.get_user_data,
        t.get_forgeable_user_data,
        t.cd_key_for_invite,
        t.save_session_key,
        t.get_key_id,
        t.ascii_account_names,
    ] {
        s.push(if f { b'1' } else { b'0' });
    }
    s.push(b';');
    for u in &t.admin_community_name {
        s.extend_from_slice(format!("{:x},", *u as u32).as_bytes());
    }
    s.push(b';');
    if o.kind == EeWonTitleObjKind::Community {
        let c = &o.community;
        s.extend_from_slice(format!("c{};i{};", c.already_in_community as i32, c.community_id).as_bytes());
        for u in &c.community_name {
            s.extend_from_slice(format!("{:x},", *u as u32).as_bytes());
        }
        s.push(b';');
        hex(&mut s, &c.name_login_str);
        s.extend_from_slice(format!("n{};", c.name_login).as_bytes());
        for (k, v) in &c.trust_level_lifespans {
            s.extend_from_slice(format!("{k}:{v},").as_bytes());
        }
        s.push(b';');
        for l in &c.certificate_life_span_list {
            s.push(b'(');
            for v in l {
                s.extend_from_slice(format!("{v},").as_bytes());
            }
            s.push(b')');
        }
        s.push(b';');
    }
    s
}

#[cfg(test)]
#[path = "titleobj_tests.rs"]
mod tests;
