//! WONInternal's admin ops as Rust ops (round 7): the master key, key check, banned key, key subscription, CD key
//! account, version, patch and module list ops, each a `DBProxyOp` derived in Rust ([`AdminOp`] over
//! [`DbProxy`]). Their C++ classes (`MasterKeyOp.cpp`, `BannedKeyOp.cpp`, `KeySubscribeOp.cpp`,
//! `GetAccountFromCDKeyOp.cpp`, `GetAccountFromCDKeyExOp.cpp`, the version and patch op files and
//! `GetPatchServerModuleList.cpp`) are deleted; a C++ `ServerRequestOp` runs one made by [`ee_won_adminop_new`].
//!
//! The members are the request's values in the op's table order (admin.rs, the version and patch records after the
//! product); the members the replies fill are the op's lists, kept as the C++ kept them (some cleared by `RunHook`,
//! some by the reply, some never). `golden_adminop.txt` was printed by `tests/won/won_adminop_test.cpp` over the
//! original's statements before the port. Reached by nothing in the game (the WON admin servers are gone).

use crate::admin::{self, Value, EE_WON_ADMIN_CUT};
use crate::db::{self, EeWonProfile};
use crate::patch;
use crate::reqop::{hex, Checked, DbProxy, EeWonReqOp, RequestOp, ST_INVALID_MESSAGE, ST_SUCCESS};
use crate::wire::Writer;

// the ops (1-15: admin.rs's request tables)
pub const EE_WON_ADMINOP_INSERT_MASTER_KEY: i32 = 1;
pub const EE_WON_ADMINOP_UPDATE_MASTER_KEY: i32 = 2;
pub const EE_WON_ADMINOP_DELETE_MASTER_KEY: i32 = 3;
pub const EE_WON_ADMINOP_QUERY_MASTER_KEY: i32 = 4;
pub const EE_WON_ADMINOP_AUTH_CHECK_KEY: i32 = 5;
pub const EE_WON_ADMINOP_INSERT_BANNED_KEY: i32 = 6;
pub const EE_WON_ADMINOP_QUERY_BANNED_KEY: i32 = 7;
pub const EE_WON_ADMINOP_QUERY_LIST_BANNED_KEY: i32 = 8;
pub const EE_WON_ADMINOP_REMOVE_BANNED_KEY: i32 = 9;
pub const EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE: i32 = 10;
pub const EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE: i32 = 11;
pub const EE_WON_ADMINOP_REMOVE_KEY_SUBSCRIBE: i32 = 12;
pub const EE_WON_ADMINOP_QUERY_KEY_SUBSCRIBE: i32 = 13;
/// `GetAccountFromCDKeyOp`: the key, the community (and the profile)
pub const EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY: i32 = 14;
/// `GetAccountFromCDKeyExOp`: the same and the action flag
pub const EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX: i32 = 15;
/// `AddVersionOp`: the product, then the version record's config, version, type count, types, description, state
pub const EE_WON_ADMINOP_ADD_VERSION: i32 = 30;
/// `UpdateVersionOp`: as `AddVersionOp` (the state is sent)
pub const EE_WON_ADMINOP_UPDATE_VERSION: i32 = 31;
/// `RemoveVersionOp`: product, config, version
pub const EE_WON_ADMINOP_REMOVE_VERSION: i32 = 32;
/// `GetVersionListOp`: product, config, state filter
pub const EE_WON_ADMINOP_GET_VERSION_LIST: i32 = 33;
/// `AddPatchOp`: the product, the patch record's config, from, to, display name, net address, checksum, size,
/// host name, host URL, host text, host image URL, must-visit and active flags, then the update flag
pub const EE_WON_ADMINOP_ADD_PATCH: i32 = 34;
/// `RemovePatchOp`: product, config, from, to, net address
pub const EE_WON_ADMINOP_REMOVE_PATCH: i32 = 35;
/// `ActivatePatchOp`: the same and the active flag
pub const EE_WON_ADMINOP_ACTIVATE_PATCH: i32 = 36;
/// `PatchCleanupOp`: no members
pub const EE_WON_ADMINOP_PATCH_CLEANUP: i32 = 37;
/// `GetPatchServerModuleListOp`: no members
pub const EE_WON_ADMINOP_MODULE_LIST: i32 = 38;
/// WONDB's `RequestAccountInfoEmailOp`: the email (and the profile)
pub const EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL: i32 = 40;
/// WONDB's `RequestPasswordEmailOp`: the user name (and the profile)
pub const EE_WON_ADMINOP_PASSWORD_EMAIL: i32 = 41;
/// WONMisc's `GetPatchListOp`: product, config, from, to, state filter
pub const EE_WON_ADMINOP_GET_PATCH_LIST: i32 = 42;

/// A member: a number, or bytes (a wide string's as UTF-16 LE).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Member {
    pub num: i64,
    pub bytes: Vec<u8>,
}

/// `QueryMasterKeyOp::MasterKey`
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct MasterKey {
    pub sequence: u16,
    pub activate: i64,
    pub deactivate: i64,
    pub beta: bool,
    pub flags: u32,
}

/// `QueryKeySubscribeOp::KEY_DATA`
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct KeyData {
    pub status: i32,
    pub allocate_date: i64,
    pub allocate_duration: u32,
    pub activate_time: i64,
    pub activate_duration: u32,
    pub activate_user_seq: u32,
}

/// `VersionData`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Version {
    pub config: Vec<u8>,
    pub version: Vec<u8>,
    pub types: Vec<Vec<u8>>,
    pub description: Vec<u8>,
    /// `VersionState` (`VersionState_Invalid`, 0, when made)
    pub state: i32,
}

impl Version {
    /// `ReadFromBuffer` over a new record: `None` when the read ran past the end (a throw)
    pub fn read(data: &[u8]) -> Option<Version> {
        let mut v = Version::default();
        for f in patch::read_version(data) {
            let s = || if f.whole { data[f.at as usize..(f.at + f.len) as usize].to_vec() } else { Vec::new() };
            match f.slot {
                patch::EE_WON_VERSION_CONFIG => v.config = s(),
                patch::EE_WON_VERSION_VERSION => v.version = s(),
                patch::EE_WON_VERSION_TYPE => v.types.push(s()),
                patch::EE_WON_VERSION_DESCRIPTION => v.description = s(),
                patch::EE_WON_VERSION_STATE => v.state = f.num,
                patch::EE_WON_PATCH_END => return Some(v),
                _ => return None,
            }
        }
        None
    }

    fn write(&self, w: &mut Writer, flags: u32) {
        let types: Vec<&[u8]> = self.types.iter().map(|t| t.as_slice()).collect();
        patch::write_version(w, &self.config, &self.version, &types, &self.description, self.state as u8, flags);
    }

    fn describe(&self) -> String {
        let t: Vec<String> = self.types.iter().map(|t| hex(t)).collect();
        let t = if t.is_empty() { "-".to_string() } else { t.join("+") };
        format!("{},{},{},{},{}", hex(&self.config), hex(&self.version), t, hex(&self.description), self.state)
    }
}

/// `PatchData`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PatchRecord {
    pub config: Vec<u8>,
    pub from: Vec<u8>,
    pub to: Vec<u8>,
    pub display_name: Vec<u8>,
    pub net_address: Vec<u8>,
    pub checksum: u32,
    pub size: u32,
    pub host_name: Vec<u8>,
    pub host_url: Vec<u8>,
    pub host_text: Vec<u8>,
    pub host_image_url: Vec<u8>,
    pub must_visit_host: bool,
    pub active: bool,
}

impl PatchRecord {
    /// `ReadFromBuffer(record, true)` over a new record (a patch list's: with the config and versions)
    fn read(data: &[u8]) -> PatchRecord {
        let mut p = PatchRecord::default();
        for f in patch::read_patch(data, 0, true) {
            let s = || if f.whole { data[f.at as usize..(f.at + f.len) as usize].to_vec() } else { Vec::new() };
            match f.slot {
                patch::EE_WON_PATCH_CONFIG => p.config = s(),
                patch::EE_WON_PATCH_FROM => p.from = s(),
                patch::EE_WON_PATCH_TO => p.to = s(),
                patch::EE_WON_PATCH_ACTIVE => p.active = f.num != 0,
                patch::EE_WON_PATCH_DISPLAY => p.display_name = s(),
                patch::EE_WON_PATCH_NET_ADDRESS => p.net_address = s(),
                patch::EE_WON_PATCH_CHECKSUM => p.checksum = f.num as u32,
                patch::EE_WON_PATCH_SIZE => p.size = f.num as u32,
                patch::EE_WON_PATCH_HOST_NAME => p.host_name = s(),
                patch::EE_WON_PATCH_HOST_URL => p.host_url = s(),
                patch::EE_WON_PATCH_HOST_TEXT => p.host_text = s(),
                patch::EE_WON_PATCH_HOST_IMAGE => p.host_image_url = s(),
                patch::EE_WON_PATCH_MUST_VISIT => p.must_visit_host = f.num != 0,
                _ => {}
            }
        }
        p
    }

    fn describe(&self) -> String {
        format!(
            "{},{},{},{},{},{},{},{},{},{},{},{},{}",
            hex(&self.config),
            hex(&self.from),
            hex(&self.to),
            hex(&self.display_name),
            hex(&self.net_address),
            self.checksum,
            self.size,
            hex(&self.host_name),
            hex(&self.host_url),
            hex(&self.host_text),
            hex(&self.host_image_url),
            u8::from(self.must_visit_host),
            u8::from(self.active)
        )
    }
}

/// What the replies fill.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct AdminOut {
    pub master_keys: Vec<MasterKey>,
    /// the key check's `mKeyStatusList`, the others' `mStatusList`
    pub statuses: Vec<i16>,
    /// `QueryBannedKeyOp::mKeyList`
    pub banned_keys: Vec<(Vec<u8>, i64)>,
    /// `QueryListBannedKeyOp::mKeyDataList`
    pub banned_data: Vec<(i64, i16)>,
    pub key_data: Vec<KeyData>,
    /// the CD key account's user name (UTF-16 LE), email, password (UTF-16 LE), last login, activation
    pub account: [Vec<u8>; 5],
    pub profile: EeWonProfile,
    pub versions: Vec<Version>,
    pub modules: Vec<u32>,
    pub patches: Vec<PatchRecord>,
}

/// The members the constructors set (the key flags 1, the subscriptions' dates -1, the version list's config `%`
/// and state filter `VersionState_BadState`; strings empty, numbers 0, key lists empty).
fn defaults(kind: i32) -> Vec<Member> {
    let n = |num: i64| Member { num, bytes: Vec::new() };
    let s = || Member::default();
    match kind {
        EE_WON_ADMINOP_INSERT_MASTER_KEY => {
            vec![s(), s(), n(0), n(0), n(0), n(0), n(0), Member { num: 0, bytes: vec![0; 20] }, n(1)]
        }
        EE_WON_ADMINOP_UPDATE_MASTER_KEY => vec![s(), s(), n(0), n(0), n(0), n(0), n(1)],
        EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE => vec![s(), s(), n(0), n(-1), n(0), n(-1), n(0)],
        EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE => vec![s(), s(), n(0), n(0), n(-1), n(0), n(-1), n(0), n(0)],
        EE_WON_ADMINOP_ADD_VERSION | EE_WON_ADMINOP_UPDATE_VERSION => vec![s(), s(), s(), n(0), s(), n(0)],
        EE_WON_ADMINOP_GET_VERSION_LIST => vec![s(), Member { num: 0, bytes: b"%".to_vec() }, n(3)],
        EE_WON_ADMINOP_ADD_PATCH => {
            let mut v = vec![s(); 6];
            v.extend([n(0), n(0), s(), s(), s(), s(), n(0), n(0), n(0)]);
            v
        }
        EE_WON_ADMINOP_REMOVE_PATCH => vec![s(); 5],
        EE_WON_ADMINOP_ACTIVATE_PATCH => vec![s(), s(), s(), s(), s(), n(0)],
        EE_WON_ADMINOP_PATCH_CLEANUP | EE_WON_ADMINOP_MODULE_LIST => Vec::new(),
        EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL | EE_WON_ADMINOP_PASSWORD_EMAIL => vec![s()],
        EE_WON_ADMINOP_GET_PATCH_LIST => vec![s(), s(), s(), s(), n(2)],
        // the community and product (or key and community), then the numbers and key counts 0
        _ => vec![s(), s(), n(0), n(0)],
    }
}

/// An admin op.
#[derive(Debug)]
pub struct AdminOp {
    pub kind: i32,
    pub members: Vec<Member>,
    pub proxy: DbProxy,
    pub out: AdminOut,
}

impl AdminOp {
    /// The op with its members (none: the constructor's defaults).
    pub fn new(kind: i32, members: Vec<Member>) -> AdminOp {
        let members = if members.is_empty() { defaults(kind) } else { members };
        AdminOp { kind, members, proxy: DbProxy::default(), out: AdminOut::default() }
    }

    fn m(&self, i: usize) -> Member {
        self.members.get(i).cloned().unwrap_or_default()
    }

    fn values(&self, from: usize) -> Vec<Value<'_>> {
        self.members[from.min(self.members.len())..].iter().map(|m| Value { num: m.num, bytes: &m.bytes }).collect()
    }

    /// The (message, sub-message) types `RunHook` sets.
    fn types(&self) -> (i16, i16) {
        let sub = match self.kind {
            EE_WON_ADMINOP_INSERT_MASTER_KEY => 18,
            EE_WON_ADMINOP_UPDATE_MASTER_KEY => 22,
            EE_WON_ADMINOP_DELETE_MASTER_KEY => 24,
            EE_WON_ADMINOP_QUERY_MASTER_KEY => 26,
            EE_WON_ADMINOP_AUTH_CHECK_KEY => 28,
            EE_WON_ADMINOP_INSERT_BANNED_KEY => 20,
            EE_WON_ADMINOP_QUERY_BANNED_KEY => 32,
            EE_WON_ADMINOP_QUERY_LIST_BANNED_KEY => 34,
            EE_WON_ADMINOP_REMOVE_BANNED_KEY => 30,
            EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE => 1,
            EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE => 5,
            EE_WON_ADMINOP_REMOVE_KEY_SUBSCRIBE => 7,
            EE_WON_ADMINOP_QUERY_KEY_SUBSCRIBE => 3,
            EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY => 10,
            EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX => 12,
            EE_WON_ADMINOP_ADD_VERSION => 3,
            EE_WON_ADMINOP_UPDATE_VERSION => 7,
            EE_WON_ADMINOP_REMOVE_VERSION => 5,
            EE_WON_ADMINOP_GET_VERSION_LIST => 9,
            EE_WON_ADMINOP_ADD_PATCH => 11,
            EE_WON_ADMINOP_REMOVE_PATCH => 13,
            EE_WON_ADMINOP_ACTIVATE_PATCH => 15,
            EE_WON_ADMINOP_PATCH_CLEANUP => 21,
            EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL => 14,
            EE_WON_ADMINOP_PASSWORD_EMAIL => 8,
            EE_WON_ADMINOP_GET_PATCH_LIST => 17,
            _ => 1,
        };
        let message = match self.kind {
            EE_WON_ADMINOP_MODULE_LIST => 6,
            EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL | EE_WON_ADMINOP_PASSWORD_EMAIL => 1002,
            k if k >= EE_WON_ADMINOP_ADD_VERSION => 1004,
            k if (EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE..=EE_WON_ADMINOP_QUERY_KEY_SUBSCRIBE).contains(&k) => 1006,
            _ => 1002,
        };
        (message, sub)
    }

    /// The version record of the version ops (members 1..).
    fn version(&self) -> Version {
        let n = self.m(3).num.max(0) as usize;
        Version {
            config: self.m(1).bytes,
            version: self.m(2).bytes,
            types: (0..n).map(|i| self.m(4 + i).bytes).collect(),
            description: self.m(4 + n).bytes,
            state: self.m(5 + n).num as i32,
        }
    }

    /// The request data `RunHook` stores.
    fn request_data(&self) -> Option<Vec<u8>> {
        let mut w = Writer::new(0, true);
        match self.kind {
            EE_WON_ADMINOP_MODULE_LIST => return None,
            EE_WON_ADMINOP_PATCH_CLEANUP => {}
            EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL => {
                return Some(db::email_data(None, &self.m(0).bytes, &self.out.profile))
            }
            EE_WON_ADMINOP_PASSWORD_EMAIL => {
                let name = crate::text::utf16_from_le_bytes(&self.m(0).bytes);
                return Some(db::email_data(Some(&name), &[], &self.out.profile));
            }
            EE_WON_ADMINOP_GET_PATCH_LIST => {
                let s: Vec<Vec<u8>> = (0..4).map(|i| self.m(i).bytes).collect();
                let s: Vec<&[u8]> = s.iter().map(|x| x.as_slice()).collect();
                return Some(patch::strings_and_byte(&s, self.m(4).num as u8));
            }
            EE_WON_ADMINOP_ADD_VERSION | EE_WON_ADMINOP_UPDATE_VERSION => {
                w.append_string(&self.m(0).bytes, 2);
                let flags = if self.kind == EE_WON_ADMINOP_ADD_VERSION { 2 } else { 3 };
                self.version().write(&mut w, flags);
            }
            EE_WON_ADMINOP_ADD_PATCH => {
                w.append_string(&self.m(0).bytes, 2);
                let s: Vec<Vec<u8>> = [1, 2, 3, 4, 5, 8, 9, 10, 11].iter().map(|&i| self.m(i).bytes).collect();
                let p = patch::Patch {
                    strings: [&s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7], &s[8]],
                    checksum: self.m(6).num as i32,
                    size: self.m(7).num as i32,
                    must_visit: self.m(12).num != 0,
                    active: self.m(13).num != 0,
                };
                patch::write_patch(&mut w, &p, self.m(14).num != 0);
            }
            k => {
                let table = match k {
                    EE_WON_ADMINOP_REMOVE_VERSION => admin::EE_WON_ADMIN_REMOVE_VERSION,
                    EE_WON_ADMINOP_GET_VERSION_LIST => admin::EE_WON_ADMIN_GET_VERSION_LIST,
                    EE_WON_ADMINOP_REMOVE_PATCH => admin::EE_WON_ADMIN_REMOVE_PATCH,
                    EE_WON_ADMINOP_ACTIVATE_PATCH => admin::EE_WON_ADMIN_ACTIVATE_PATCH,
                    k => k,
                };
                w.append_bytes(&admin::request(table, &self.values(0)));
                if k == EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY || k == EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX {
                    self.out.profile.pack(&mut w, true);
                }
            }
        }
        Some(w.data().to_vec())
    }

    /// The extended unpack of a successful reply.
    fn unpack(&mut self) -> Checked {
        let data = self.proxy.reply_data().to_vec();
        let k = self.kind;
        if k == EE_WON_ADMINOP_GET_PATCH_LIST {
            // each patch record pushed as read; a cut throws (the records before it stay)
            for f in patch::read_patch_reply(&data, false) {
                match f.slot {
                    patch::EE_WON_PATCH_RECORD => {
                        let at = f.at as usize;
                        self.out.patches.push(PatchRecord::read(&data[at..at + f.len as usize]));
                    }
                    patch::EE_WON_PATCH_END => {}
                    _ => return Checked::Cut,
                }
            }
            return Checked::Status(ST_SUCCESS);
        }
        let table = match k {
            EE_WON_ADMINOP_REMOVE_KEY_SUBSCRIBE | EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE => {
                admin::EE_WON_ADMIN_INSERT_KEY_SUBSCRIBE
            }
            EE_WON_ADMINOP_GET_VERSION_LIST => admin::EE_WON_ADMIN_GET_VERSION_LIST,
            EE_WON_ADMINOP_MODULE_LIST => admin::EE_WON_ADMIN_MODULE_LIST,
            EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX => admin::EE_WON_ADMIN_ACCOUNT_FROM_CDKEY,
            k if k > EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY => return Checked::Status(ST_SUCCESS),
            k => k,
        };
        let f = admin::reply(table, &data);
        let (values, end) = f.split_at(f.len() - 1);
        let cut = end[0].tag == EE_WON_ADMIN_CUT;
        let s = |i: usize| {
            let x = &values[i];
            if x.whole {
                data[x.at as usize..(x.at + x.len) as usize].to_vec()
            } else {
                Vec::new()
            }
        };
        let n = |i: usize| values[i].num;
        let o = &mut self.out;
        let invalid_if_cut = if cut { Checked::Status(ST_INVALID_MESSAGE) } else { Checked::Status(ST_SUCCESS) };
        match k {
            EE_WON_ADMINOP_QUERY_MASTER_KEY => {
                o.master_keys.clear();
                for i in (0..values.len() / 5).map(|r| r * 5) {
                    o.master_keys.push(MasterKey {
                        sequence: n(i) as u16,
                        activate: i64::from(n(i + 1)),
                        deactivate: i64::from(n(i + 2)),
                        beta: n(i + 3) != 0,
                        flags: n(i + 4) as u32,
                    });
                }
                invalid_if_cut
            }
            EE_WON_ADMINOP_AUTH_CHECK_KEY
            | EE_WON_ADMINOP_INSERT_BANNED_KEY
            | EE_WON_ADMINOP_REMOVE_BANNED_KEY
            | EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE
            | EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE
            | EE_WON_ADMINOP_REMOVE_KEY_SUBSCRIBE => {
                // the key check and the subscriptions clear the list first; the banned keys' RunHook did
                if k == EE_WON_ADMINOP_AUTH_CHECK_KEY || k >= EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE {
                    o.statuses.clear();
                }
                o.statuses.extend((0..values.len()).map(|i| n(i) as i16));
                invalid_if_cut
            }
            EE_WON_ADMINOP_QUERY_BANNED_KEY => {
                for i in (0..values.len() / 2).map(|r| r * 2) {
                    o.banned_keys.push((s(i), i64::from(n(i + 1))));
                }
                invalid_if_cut
            }
            EE_WON_ADMINOP_QUERY_LIST_BANNED_KEY => {
                for i in (0..values.len() / 2).map(|r| r * 2) {
                    o.banned_data.push((i64::from(n(i)), n(i + 1) as i16));
                }
                invalid_if_cut
            }
            EE_WON_ADMINOP_QUERY_KEY_SUBSCRIBE => {
                o.key_data.clear();
                for i in (0..values.len() / 6).map(|r| r * 6) {
                    o.key_data.push(KeyData {
                        status: i32::from(n(i) as i16),
                        allocate_date: i64::from(n(i + 1)),
                        allocate_duration: n(i + 2) as u32,
                        activate_time: i64::from(n(i + 3)),
                        activate_duration: n(i + 4) as u32,
                        activate_user_seq: n(i + 5) as u32,
                    });
                }
                invalid_if_cut
            }
            EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY | EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX => {
                // each string assigned as read; the profile read (quietly short) when none was cut
                for (i, x) in values.iter().enumerate().take(5) {
                    // the wide strings' lengths count UTF-16 units
                    let len = if i == 0 || i == 2 { x.len as usize * 2 } else { x.len as usize };
                    o.account[i] = if x.whole { data[x.at as usize..x.at as usize + len].to_vec() } else { Vec::new() };
                }
                if !cut {
                    o.profile.unpack(&data, end[0].at as usize);
                }
                Checked::Status(ST_SUCCESS)
            }
            EE_WON_ADMINOP_GET_VERSION_LIST => {
                // each whole record read by a new VersionData (a cut record throws), then the list's own cut
                for i in 0..values.len() {
                    match Version::read(&s(i)) {
                        Some(v) => o.versions.push(v),
                        None => return Checked::Cut,
                    }
                }
                if cut {
                    Checked::Cut
                } else {
                    Checked::Status(ST_SUCCESS)
                }
            }
            EE_WON_ADMINOP_MODULE_LIST => {
                o.modules.extend((0..values.len()).map(|i| n(i) as u32));
                if cut {
                    Checked::Cut
                } else {
                    Checked::Status(ST_SUCCESS)
                }
            }
            _ => Checked::Status(ST_SUCCESS),
        }
    }
}

impl RequestOp for AdminOp {
    fn run_hook(&mut self) {
        let (message, sub) = self.types();
        self.proxy.message = message;
        self.proxy.sub = sub;
        match self.kind {
            EE_WON_ADMINOP_INSERT_BANNED_KEY | EE_WON_ADMINOP_REMOVE_BANNED_KEY => self.out.statuses.clear(),
            EE_WON_ADMINOP_QUERY_BANNED_KEY => self.out.banned_keys.clear(),
            EE_WON_ADMINOP_QUERY_LIST_BANNED_KEY => self.out.banned_data.clear(),
            _ => {}
        }
        self.proxy.request_data = self.request_data();
    }

    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        self.proxy.next_request(field_size)
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = self.proxy.check_response(response);
        if r != Checked::Status(ST_SUCCESS) {
            return r;
        }
        let answers = if self.kind == EE_WON_ADMINOP_MODULE_LIST {
            self.proxy.reply_message == 7
        } else {
            self.proxy.answers_sub()
        };
        if !answers {
            return Checked::InvalidHeader;
        }
        self.unpack()
    }

    fn describe(&self) -> String {
        let mut s = self.proxy.describe();
        let o = &self.out;
        match self.kind {
            EE_WON_ADMINOP_QUERY_MASTER_KEY => {
                for m in &o.master_keys {
                    s += &format!(" m{}:{}:{}:{}:{}", m.sequence, m.activate, m.deactivate, u8::from(m.beta), m.flags);
                }
            }
            EE_WON_ADMINOP_AUTH_CHECK_KEY => o.statuses.iter().for_each(|x| s += &format!(" k{x}")),
            EE_WON_ADMINOP_INSERT_BANNED_KEY
            | EE_WON_ADMINOP_REMOVE_BANNED_KEY
            | EE_WON_ADMINOP_INSERT_KEY_SUBSCRIBE
            | EE_WON_ADMINOP_UPDATE_KEY_SUBSCRIBE
            | EE_WON_ADMINOP_REMOVE_KEY_SUBSCRIBE => o.statuses.iter().for_each(|x| s += &format!(" s{x}")),
            EE_WON_ADMINOP_QUERY_BANNED_KEY => o.banned_keys.iter().for_each(|(k, t)| s += &format!(" {}:{t}", hex(k))),
            EE_WON_ADMINOP_QUERY_LIST_BANNED_KEY => o.banned_data.iter().for_each(|(t, v)| s += &format!(" {t}:{v}")),
            EE_WON_ADMINOP_QUERY_KEY_SUBSCRIBE => {
                for d in &o.key_data {
                    s += &format!(
                        " {}:{}:{}:{}:{}:{}",
                        d.status,
                        d.allocate_date,
                        d.allocate_duration,
                        d.activate_time,
                        d.activate_duration,
                        d.activate_user_seq
                    );
                }
            }
            EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY | EE_WON_ADMINOP_ACCOUNT_FROM_CDKEY_EX => {
                let mut w = Writer::new(0, true);
                o.profile.pack(&mut w, true);
                for a in &o.account {
                    s += &format!(" {}", hex(a));
                }
                s += &format!(" p{}", hex(w.data()));
            }
            EE_WON_ADMINOP_GET_VERSION_LIST => o.versions.iter().for_each(|v| s += &format!(" v{}", v.describe())),
            EE_WON_ADMINOP_MODULE_LIST => o.modules.iter().for_each(|m| s += &format!(" {m}")),
            EE_WON_ADMINOP_GET_PATCH_LIST => o.patches.iter().for_each(|p| s += &format!(" {}", p.describe())),
            EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL | EE_WON_ADMINOP_PASSWORD_EMAIL => {
                let mut w = Writer::new(0, true);
                o.profile.pack(&mut w, true);
                s += &format!(" p{}", hex(w.data()));
            }
            _ => {}
        }
        s
    }
}

/// `GetPatchServerModuleListOp::IsModuleAvailable`: a module id's low word the message type, its high word the
/// sub-message type.
pub fn is_module_available(modules: &[u32], message: u16, sub: u16) -> bool {
    modules.iter().any(|&m| message == m as u16 && sub == (m >> 16) as u16)
}

/// `GetAccountFromCDKeyExOp::GetActionString`
pub fn action_name(action: i64) -> &'static str {
    match action {
        0 => "RESET",
        1 => "BAN",
        _ => "Unknown Action",
    }
}

// ---- the C ABI

/// An admin op of `kind` (`EE_WON_ADMINOP_*`) with its members, for a C++ `ServerRequestOp` to run.
///
/// # Safety
/// `values` holds `count` values whose byte ranges are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_adminop_new(
    kind: i32,
    values: *const admin::EeWonAdminValue,
    count: usize,
) -> *mut EeWonReqOp {
    let members = if values.is_null() || count == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(values, count) }
            .iter()
            .map(|v| Member { num: v.num, bytes: unsafe { crate::ffi::bytes(v.data, v.len) }.to_vec() })
            .collect()
    };
    EeWonReqOp::new(Box::new(AdminOp::new(kind, members)))
}

/// The lobby's lost password request (`RequestPasswordEmailOp` with its user name, UTF-16 LE).
///
/// # Safety
/// `name` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_password_email(name: *const u8, len: usize) -> *mut EeWonReqOp {
    let m = Member { num: 0, bytes: unsafe { crate::ffi::bytes(name, len) }.to_vec() };
    EeWonReqOp::new(Box::new(AdminOp::new(EE_WON_ADMINOP_PASSWORD_EMAIL, vec![m])))
}

/// The lobby's lost user name request (`RequestAccountInfoEmailOp` with its email).
///
/// # Safety
/// `email` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_info_email(email: *const u8, len: usize) -> *mut EeWonReqOp {
    let m = Member { num: 0, bytes: unsafe { crate::ffi::bytes(email, len) }.to_vec() };
    EeWonReqOp::new(Box::new(AdminOp::new(EE_WON_ADMINOP_ACCOUNT_INFO_EMAIL, vec![m])))
}

#[cfg(test)]
#[path = "adminop_tests.rs"]
mod tests;
