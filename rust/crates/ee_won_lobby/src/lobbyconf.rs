//! The lobby's configuration (LobbyConfig, WONLobby.cfg; R6 WON lobby and network round 40): its values are this
//! record's ([`EeWonLobbyConf`]: the server lists, the product name and directory, the versions, the language, the
//! resource directory, the flags and ports, the LOBBYDEFINE map, EELobbyConfig's file transfer port), registered with
//! the C++ ConfigObject's Rust group (ee_won_net cfgobject.rs) as Rust-owned values (cfgval.rs `rust_*`), in the
//! original constructor's order and with its defaults. HandleValueInstruction's LOBBYDEFINE line and PostParse's
//! checks are here too (round 33's lobbyconf.rs and round 32's lobbycfg.rs composed them over C++ ops); the C++
//! LobbyConfig keeps only what ConfigObject's vtable needs (WONLobbyGUI.h).
//!
//! The lobby's record ([`current`]) is set when WONLobbyPrv::Init makes the config (lobbyprv.rs `ConfigNew`) and
//! cleared when the record goes (the original's `GetLobbyConfig()`: NULL without a running lobby). Its readers (the
//! logics, the controls, the private state) read it here.
//!
//! UseLocalIP: the registry's HKLM\SOFTWARE\Sierra\WONLobby value (config.yaml's block, regkey.rs); the config key
//! writes another bool nobody reads (the original's global gLobbyUseLocalIP, 0x96634d), so a UseLocalIP line in
//! WONLobby.cfg has no effect.
//!
//! Reach: every lobby start (the LAN game) constructs and parses the config and reads it; the registry value found,
//! a LOBBYDEFINE line and PostParse's errors run only in the drivers (won_lobby33_test, won_split32_test).

use std::ffi::c_void;
use std::sync::atomic::{AtomicPtr, Ordering};

use ee_won_net::cfgobject::{
    ee_won_cfgo_handle_value_instruction, ee_won_cfgo_register_key_value, EeWonCfgObject, EeWonCfgStringParser,
};
use ee_won_net::cfgparser::{ee_won_cfgp_set_error, EeWonCfgParser};
use ee_won_net::ffi::EeWonByteSink;
use ee_won_net::ffi_parse::ee_won_parser_read_string;
use ee_won_net::strcase::compare_no_case;

use crate::cfgval::{self, EeWonCfgVal, EeWonCfgValUnits};

#[cfg(windows)]
pub type WChar = u16;
#[cfg(not(windows))]
pub type WChar = u32;

/// LobbyConfig's values (the original's members from +0x54).
#[derive(Default)]
pub struct EeWonLobbyConf {
    pub dir_servers: Vec<Vec<u8>>,
    pub room_dir_servers: Vec<Vec<u8>>,
    pub auth_servers: Vec<Vec<u8>>,
    pub product_name: Vec<u8>,
    pub product_directory: Vec<WChar>,
    pub version: Vec<u8>,
    pub lan_version: Vec<u8>,
    pub language: Vec<u8>,
    pub resource_directory: Vec<u8>,
    pub crc_check: bool,
    pub cd_key_check: bool,
    pub public: bool,
    pub lan_broadcast_port: u16,
    pub use_local_ip: bool,
    pub no_ping_in_progress: bool,
    pub lobby_port: u16,
    pub check_for_internet: bool,
    pub ping_ceiling: u16,
    pub allow_duplicate_names: bool,
    pub append_number_to_default_lan_name: bool,
    pub allow_join_game_without_ping: bool,
    /// The LOBBYDEFINE map (`std::map<std::string, std::string, StringLessNoCase>`): ordered ignoring case.
    pub defines: Vec<(Vec<u8>, Vec<u8>)>,
    /// EELobbyConfig's "EEFileTransferPort".
    pub file_transfer_port: u16,
    /// The UseLocalIP key's target (the original's global gLobbyUseLocalIP: written, never read).
    use_local_ip_key: bool,
}

/// The caller's bytes (none for NULL or 0).
///
/// # Safety
/// `data` holds `n` bytes.
unsafe fn bytes<'a>(data: *const u8, n: usize) -> &'a [u8] {
    if data.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(data, n) }
    }
}

impl EeWonLobbyConf {
    /// `mDefines[name] = value`: an entry equal ignoring case keeps its name and takes the value.
    pub fn define(&mut self, name: &[u8], value: &[u8]) {
        match self.defines.binary_search_by(|(k, _)| compare_no_case(k, name).cmp(&0)) {
            Ok(i) => self.defines[i].1 = value.to_vec(),
            Err(i) => self.defines.insert(i, (name.to_vec(), value.to_vec())),
        }
    }

    /// A server list.
    pub fn list(&self, which: EeWonLobbyConfList) -> &Vec<Vec<u8>> {
        match which {
            EeWonLobbyConfList::DirServers => &self.dir_servers,
            EeWonLobbyConfList::RoomDirServers => &self.room_dir_servers,
            EeWonLobbyConfList::AuthServers => &self.auth_servers,
        }
    }

    fn text(&mut self, which: EeWonLobbyConfText) -> &mut Vec<u8> {
        match which {
            EeWonLobbyConfText::ProductName => &mut self.product_name,
            EeWonLobbyConfText::Version => &mut self.version,
            EeWonLobbyConfText::LanVersion => &mut self.lan_version,
            EeWonLobbyConfText::Language => &mut self.language,
            EeWonLobbyConfText::ResourceDirectory => &mut self.resource_directory,
        }
    }

    /// `PostParse(parser)`: WONLobby.cfg names directory servers, a product, a version and a language (else the
    /// parser's error and false); the LAN version, resource and product directories filled in; the Product define.
    pub fn post_parse(&mut self, error: &mut dyn FnMut(&[u8])) -> bool {
        for (empty, text) in [
            (self.dir_servers.is_empty(), &b"No Directory Servers specified."[..]),
            (self.product_name.is_empty(), b"No product name specified."),
            (self.version.is_empty(), b"No version specified."),
            (self.language.is_empty(), b"No language specified."),
        ] {
            if empty {
                error(text);
                return false;
            }
        }
        if self.lan_version.is_empty() {
            self.lan_version = self.version.clone();
        }
        if self.resource_directory.is_empty() {
            self.resource_directory = [&b".\\"[..], &self.product_name].concat();
        }
        if self.product_directory.is_empty() {
            // L"/" + StringToWString(mProductName)
            self.product_directory =
                std::iter::once(b'/' as WChar).chain(ee_won_net::titleobj::widen(&self.product_name)).collect();
        }
        let name = self.product_name.clone();
        self.define(b"Product", &name);
        true
    }
}

/// The scalar values (a getter's and the drivers' setter's names).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyConfNum {
    CrcCheck = 1,
    CdKeyCheck,
    Public,
    LanBroadcastPort,
    UseLocalIp,
    NoPingInProgress,
    LobbyPort,
    CheckForInternet,
    PingCeiling,
    AllowDuplicateNames,
    AppendNumberToDefaultLanName,
    AllowJoinGameWithoutPing,
    FileTransferPort,
}

/// The narrow texts.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyConfText {
    ProductName = 1,
    Version,
    LanVersion,
    Language,
    ResourceDirectory,
}

/// The server lists.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyConfList {
    DirServers = 1,
    RoomDirServers,
    AuthServers,
}

/// `LobbyTestPortOffset()` (TEST ONLY): the harness slot's port offset (`gPlatSimPortOffset` when 1..4095, else 0),
/// added to the lobby port.
pub fn test_port_offset() -> u16 {
    let o = ee_base::plat_core::sim::gPlatSimPortOffset.get();
    if o > 0 && o < 4096 {
        o as u16
    } else {
        0
    }
}

static CURRENT: AtomicPtr<EeWonLobbyConf> = AtomicPtr::new(std::ptr::null_mut());

/// The running lobby's config (`GetLobbyConfig()`), if any. The lobby runs on the game's thread: no reference is
/// kept across a call that could end the lobby.
pub fn current<'a>() -> Option<&'a EeWonLobbyConf> {
    // SAFETY: a record `ee_won_lobbyconf_set_current` stored, live until it is cleared (its free)
    unsafe { CURRENT.load(Ordering::Acquire).as_ref() }
}

/// Sets the running lobby's config (NULL: none).
#[no_mangle]
pub extern "C" fn ee_won_lobbyconf_set_current(conf: *mut EeWonLobbyConf) {
    CURRENT.store(conf, Ordering::Release);
}

/// The running lobby's config, or NULL.
#[no_mangle]
pub extern "C" fn ee_won_lobbyconf_current() -> *mut EeWonLobbyConf {
    CURRENT.load(Ordering::Acquire)
}

/// `LobbyConfig::LobbyConfig()`: a record with every WONLobby.cfg key registered on the group `group` (NULL: none,
/// the drivers' bare record), in the original's order with its defaults; UseLocalIP from the registry.
///
/// # Safety
/// `group` is NULL or the C++ LobbyConfig's group, freed before the record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_new(group: *mut EeWonCfgObject) -> *mut EeWonLobbyConf {
    let c = Box::into_raw(Box::<EeWonLobbyConf>::default());
    if group.is_null() {
        return c;
    }
    // SAFETY: just made; its members outlive the group's values (the record is freed after the group)
    let r = unsafe { &mut *c };
    let key = |k: &[u8], v: *mut EeWonCfgVal| {
        // SAFETY: the contract
        unsafe { ee_won_cfgo_register_key_value(group, k.as_ptr(), k.len(), v as *mut c_void) }
    };
    let null = std::ptr::null();
    // SAFETY (the values): the targets are the record's, the defaults static
    unsafe {
        key(b"DirServer", cfgval::rust_string_list(&mut r.dir_servers, null, true));
        key(b"RoomDirServer", cfgval::rust_string_list(&mut r.room_dir_servers, null, true));
        key(b"AuthServers", cfgval::rust_string_list(&mut r.auth_servers, null, true));
        key(b"ProductName", cfgval::rust_string(&mut r.product_name, null));
        key(b"ProductDirectory", cfgval::rust_wstring(&mut r.product_directory, std::ptr::null()));
        key(b"ResourceDirectory", cfgval::rust_string(&mut r.resource_directory, null));
        key(b"Version", cfgval::rust_string(&mut r.version, null));
        key(b"LanVersion", cfgval::rust_string(&mut r.lan_version, null));
        key(b"Language", cfgval::rust_string(&mut r.language, c"".as_ptr() as *const u8));
        key(b"CRCCheck", cfgval::ee_won_cfgval_bool(&mut r.crc_check, true));
        key(b"CDKeyCheck", cfgval::ee_won_cfgval_bool(&mut r.cd_key_check, true));
        key(b"Public", cfgval::ee_won_cfgval_bool(&mut r.public, false));
        key(b"LanBroadcastPort", cfgval::ee_won_cfgval_u16(&mut r.lan_broadcast_port, 8585));
        key(b"NoPingInProgress", cfgval::ee_won_cfgval_bool(&mut r.no_ping_in_progress, true));
        key(b"LobbyPort", cfgval::ee_won_cfgval_u16(&mut r.lobby_port, 8888));
        key(b"CheckForInternet", cfgval::ee_won_cfgval_bool(&mut r.check_for_internet, true));
        key(b"PingCeiling", cfgval::ee_won_cfgval_u16(&mut r.ping_ceiling, 1000));
        key(b"AllowDuplicateNames", cfgval::ee_won_cfgval_bool(&mut r.allow_duplicate_names, true));
        key(
            b"AppendNumberToDefaultLanName",
            cfgval::ee_won_cfgval_bool(&mut r.append_number_to_default_lan_name, true),
        );
        key(b"AllowJoinGameWithoutPing", cfgval::ee_won_cfgval_bool(&mut r.allow_join_game_without_ping, true));
    }
    // UseLocalIP: the registry's value (a missing or non-integer value reads as 0)
    let mut reg = ee_won_net::regkey::EeWonRegKey::default();
    reg.open_new_key(2, b"SOFTWARE\\Sierra\\WONLobby");
    let (status, value) = reg.query_dword(b"UseLocalIP");
    let use_local_ip = status == 0 && value != 0;
    r.use_local_ip = use_local_ip;
    // SAFETY (both): as above
    unsafe {
        key(b"UseLocalIP", cfgval::ee_won_cfgval_bool(&mut r.use_local_ip_key, use_local_ip));
        key(b"EEFileTransferPort", cfgval::ee_won_cfgval_u16(&mut r.file_transfer_port, 8889));
    }
    c
}

/// Frees a record (`~LobbyConfig`, after its group); the running lobby's is cleared.
///
/// # Safety
/// `conf` is from `ee_won_lobbyconf_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_free(conf: *mut EeWonLobbyConf) {
    if conf.is_null() {
        return;
    }
    let _ = CURRENT.compare_exchange(conf, std::ptr::null_mut(), Ordering::AcqRel, Ordering::Acquire);
    // SAFETY: the contract
    drop(unsafe { Box::from_raw(conf) });
}

unsafe extern "C" fn sink_vec(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: the context is the caller's Vec<u8>; the bytes hold `length` bytes
    unsafe { &mut *(context as *mut Vec<u8>) }.extend_from_slice(unsafe { bytes(data, length) });
}

/// `HandleValueInstruction(parser, key, values)`: a LOBBYDEFINE line (its name, then its value; a nameless one
/// ignored) is the record's (1); any other key the group's base reader's (its answer; -1/-2 an exception in `exc`,
/// as `ee_won_cfgo_handle_value_instruction`).
///
/// # Safety
/// `conf` a record, `group` its group, `parser` the C++ ConfigParser, `key` `n` bytes, `sp` a live StringParser,
/// `exc` the caller's ConfigObject::CfgExc.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_handle_value(
    conf: *mut EeWonLobbyConf,
    group: *mut EeWonCfgObject,
    parser: *mut c_void,
    key: *const u8,
    n: usize,
    sp: *mut EeWonCfgStringParser,
    exc: *mut c_void,
) -> i32 {
    // SAFETY: the contract
    if unsafe { bytes(key, n) } != b"LOBBYDEFINE" {
        return unsafe { ee_won_cfgo_handle_value_instruction(group, parser, key, n, sp, exc) };
    }
    let sp = unsafe { &mut *sp };
    let unit = if sp.wide { std::mem::size_of::<WChar>() as u32 } else { 1 };
    let mut read = |stop_at_whitespace: bool| {
        let mut out: Vec<u8> = Vec::new();
        // SAFETY: the parser's text holds its units to a NUL
        unsafe {
            ee_won_parser_read_string(
                sp.text,
                unit,
                &mut sp.pos,
                stop_at_whitespace,
                sink_vec,
                &mut out as *mut Vec<u8> as *mut c_void,
            )
        };
        out
    };
    let name = read(true);
    let value = read(false);
    if !name.is_empty() {
        unsafe { &mut *conf }.define(&name, &value);
    }
    1
}

/// `PostParse(parser)` (its error set on the Rust parser `parser`).
///
/// # Safety
/// `conf` a record, `parser` a live parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_post_parse(conf: *mut EeWonLobbyConf, parser: *mut EeWonCfgParser) -> bool {
    // SAFETY: the contract
    unsafe { &mut *conf }.post_parse(&mut |e| unsafe { ee_won_cfgp_set_error(parser, e.as_ptr(), e.len(), false) })
}

/// A scalar value (`conf` NULL: 0).
///
/// # Safety
/// `conf` a record or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_num(conf: *const EeWonLobbyConf, which: EeWonLobbyConfNum) -> i32 {
    use EeWonLobbyConfNum as N;
    // SAFETY: the contract
    let Some(c) = (unsafe { conf.as_ref() }) else { return 0 };
    match which {
        N::CrcCheck => c.crc_check as i32,
        N::CdKeyCheck => c.cd_key_check as i32,
        N::Public => c.public as i32,
        N::LanBroadcastPort => c.lan_broadcast_port as i32,
        N::UseLocalIp => c.use_local_ip as i32,
        N::NoPingInProgress => c.no_ping_in_progress as i32,
        N::LobbyPort => c.lobby_port as i32,
        N::CheckForInternet => c.check_for_internet as i32,
        N::PingCeiling => c.ping_ceiling as i32,
        N::AllowDuplicateNames => c.allow_duplicate_names as i32,
        N::AppendNumberToDefaultLanName => c.append_number_to_default_lan_name as i32,
        N::AllowJoinGameWithoutPing => c.allow_join_game_without_ping as i32,
        N::FileTransferPort => c.file_transfer_port as i32,
    }
}

/// Sets a scalar value (the drivers' worlds).
///
/// # Safety
/// `conf` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_set_num(conf: *mut EeWonLobbyConf, which: EeWonLobbyConfNum, v: i32) {
    use EeWonLobbyConfNum as N;
    // SAFETY: the contract
    let c = unsafe { &mut *conf };
    let b = v != 0;
    match which {
        N::CrcCheck => c.crc_check = b,
        N::CdKeyCheck => c.cd_key_check = b,
        N::Public => c.public = b,
        N::LanBroadcastPort => c.lan_broadcast_port = v as u16,
        N::UseLocalIp => c.use_local_ip = b,
        N::NoPingInProgress => c.no_ping_in_progress = b,
        N::LobbyPort => c.lobby_port = v as u16,
        N::CheckForInternet => c.check_for_internet = b,
        N::PingCeiling => c.ping_ceiling = v as u16,
        N::AllowDuplicateNames => c.allow_duplicate_names = b,
        N::AppendNumberToDefaultLanName => c.append_number_to_default_lan_name = b,
        N::AllowJoinGameWithoutPing => c.allow_join_game_without_ping = b,
        N::FileTransferPort => c.file_transfer_port = v as u16,
    }
}

/// A narrow text to `sink`.
///
/// # Safety
/// `conf` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_text(
    conf: *mut EeWonLobbyConf,
    which: EeWonLobbyConfText,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    // SAFETY: the contract
    let t = unsafe { &mut *conf }.text(which);
    unsafe { sink(context, t.as_ptr(), t.len()) }
}

/// Sets a narrow text (the drivers' worlds).
///
/// # Safety
/// `conf` a record; `data` holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_set_text(
    conf: *mut EeWonLobbyConf,
    which: EeWonLobbyConfText,
    data: *const u8,
    n: usize,
) {
    // SAFETY: the contract
    *unsafe { &mut *conf }.text(which) = unsafe { bytes(data, n) }.to_vec();
}

/// The product directory's `wchar_t` units to `sink`.
///
/// # Safety
/// `conf` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_product_dir(
    conf: *const EeWonLobbyConf,
    sink: EeWonCfgValUnits,
    context: *mut c_void,
) {
    // SAFETY: the contract
    let d = &unsafe { &*conf }.product_directory;
    unsafe { sink(context, d.as_ptr() as *const c_void, d.len()) }
}

/// Sets the product directory (`n` `wchar_t` units; the drivers' worlds).
///
/// # Safety
/// `conf` a record; `units` holds `n` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_set_product_dir(conf: *mut EeWonLobbyConf, units: *const c_void, n: usize) {
    let d = if n == 0 || units.is_null() {
        Vec::new()
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(units as *const WChar, n) }.to_vec()
    };
    // SAFETY: the contract
    unsafe { &mut *conf }.product_directory = d;
}

/// A server list's item `index` to `sink` (when there is one); the list's size.
///
/// # Safety
/// `conf` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_list(
    conf: *const EeWonLobbyConf,
    which: EeWonLobbyConfList,
    index: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> usize {
    // SAFETY: the contract
    let l = unsafe { &*conf }.list(which);
    if let Some(item) = l.get(index) {
        unsafe { sink(context, item.as_ptr(), item.len()) }
    }
    l.len()
}

/// Appends to a server list (the drivers' worlds).
///
/// # Safety
/// `conf` a record; `data` holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_list_push(
    conf: *mut EeWonLobbyConf,
    which: EeWonLobbyConfList,
    data: *const u8,
    n: usize,
) {
    use EeWonLobbyConfList as L;
    // SAFETY: the contract
    let c = unsafe { &mut *conf };
    let item = unsafe { bytes(data, n) }.to_vec();
    match which {
        L::DirServers => c.dir_servers.push(item),
        L::RoomDirServers => c.room_dir_servers.push(item),
        L::AuthServers => c.auth_servers.push(item),
    }
}

/// The define `index` (in the map's order): its name then its value to `sink`; false past the end.
///
/// # Safety
/// `conf` a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_define_at(
    conf: *const EeWonLobbyConf,
    index: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    // SAFETY: the contract
    match unsafe { &*conf }.defines.get(index) {
        Some((k, v)) => {
            unsafe { sink(context, k.as_ptr(), k.len()) };
            unsafe { sink(context, v.as_ptr(), v.len()) };
            true
        }
        None => false,
    }
}

/// `mDefines[name] = value` (the drivers' worlds).
///
/// # Safety
/// `conf` a record; the texts hold their bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbyconf_set_define(
    conf: *mut EeWonLobbyConf,
    name: *const u8,
    n: usize,
    value: *const u8,
    m: usize,
) {
    // SAFETY: the contract
    unsafe { &mut *conf }.define(unsafe { bytes(name, n) }, unsafe { bytes(value, m) });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defines_ignore_case() {
        let mut c = EeWonLobbyConf::default();
        c.define(b"product", b"a");
        c.define(b"Old", b"1");
        c.define(b"PRODUCT", b"b");
        assert_eq!(c.defines, [(b"Old".to_vec(), b"1".to_vec()), (b"product".to_vec(), b"b".to_vec())]);
    }

    #[test]
    fn the_checks_and_the_defaults() {
        let mut c = EeWonLobbyConf::default();
        let mut errors = Vec::new();
        assert!(!c.post_parse(&mut |e| errors.push(e.to_vec())));
        assert_eq!(errors, [b"No Directory Servers specified.".to_vec()]);
        c.dir_servers.push(b"d".to_vec());
        c.product_name = b"EE".to_vec();
        c.version = b"2.0".to_vec();
        errors.clear();
        assert!(!c.post_parse(&mut |e| errors.push(e.to_vec())));
        assert_eq!(errors, [b"No language specified.".to_vec()]);
        c.language = b"English".to_vec();
        assert!(c.post_parse(&mut |_| panic!()));
        assert_eq!(c.lan_version, b"2.0");
        assert_eq!(c.resource_directory, b".\\EE");
        assert_eq!(c.product_directory, [b'/' as WChar, b'E' as WChar, b'E' as WChar]);
        assert_eq!(c.defines, [(b"Product".to_vec(), b"EE".to_vec())]);
    }
}
