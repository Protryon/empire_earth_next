//! WONMisc's `GetVersionOp` and `FindPatchOp` (with their base `VersionBaseOp`) as Rust request ops: each ran
//! one `GetDirOp` on the product's version directory and read its entities when it finished. Here the op is the
//! directory op itself (reqdir.rs's, made at `RunHook` as the original made its child) and reads the entities when
//! the listing succeeds: the valid and filtered versions with their descriptions and the highest valid version
//! (`BuildValidVersionSet`), and `FindPatchOp`'s patches (each service entity's patch id, display name, address,
//! checksum, size and data). A `RustServerOp` runs one (`RustServerOp::NewGetVersion`, `NewFindPatch`); results
//! and queries by number.
//!
//! - The version compare is the default (`std::string::compare`); the original's `SetVersionCompare` hook had no
//!   caller and is gone.
//! - The op's status is the directory op's: the original's own op finished with it, or with `WS_Success` after the
//!   read, which is the same status.
//!
//! Behaviour: `golden_verop.txt` (`tests/won/won_verop_test.cpp` over the original's statements, recorded before
//! the port) replayed by the tests. Reached by nothing in the game (no caller; the patch servers are gone).

use std::collections::{BTreeMap, BTreeSet};

use crate::adminop::Member;
use crate::dir::Entity;
use crate::http::c_str;
use crate::miscops::{self, Patch};
use crate::patch;
use crate::reqdir::{EE_WON_REQOPS_DIR_GET, EE_WON_REQOP_DIR_LIST};
use crate::reqop::{hex, Checked, RequestOp, ST_RECV, ST_SUCCESS};
use crate::text;

/// `GetVersionOp`: the product, the extra config, the filter string
pub const EE_WON_REQOPS_GET_VERSION: i32 = 16;
/// `FindPatchOp`: as `GetVersionOp`'s, then (`FindPatchOp(GetVersionOp*)`) the valid version count and each
/// version and its description, the filtered version count and each version
pub const EE_WON_REQOPS_FIND_PATCH: i32 = 17;

/// `GetHighestValidVersion()` (bytes)
pub const EE_WON_REQOP_VERSION_HIGHEST: i32 = 60;
/// the valid versions (number: the count; bytes: the element), the filtered ones
pub const EE_WON_REQOP_VERSION_VALID: i32 = 61;
pub const EE_WON_REQOP_VERSION_FILTERED: i32 = 62;
/// the valid version's description (bytes; the element: the valid version's index)
pub const EE_WON_REQOP_VERSION_DESCRIPTION: i32 = 63;
/// a setter: `CheckVersion(bytes)`, its result (`Version_Invalid` 0, `_Valid_NotLatest` 1, `_Valid_Latest` 2)
/// read as this number
pub const EE_WON_REQOP_VERSION_CHECK: i32 = 64;
/// the patches: the count (number); each one's from and to versions, name (UTF-16 LE) and location (bytes, none:
/// NULL), checksum and size (number); the element: the patch's index
pub const EE_WON_REQOP_PATCH_COUNT: i32 = 65;
pub const EE_WON_REQOP_PATCH_FROM: i32 = 66;
pub const EE_WON_REQOP_PATCH_TO: i32 = 67;
pub const EE_WON_REQOP_PATCH_NAME: i32 = 68;
pub const EE_WON_REQOP_PATCH_LOCATION: i32 = 69;
pub const EE_WON_REQOP_PATCH_CHECKSUM: i32 = 70;
pub const EE_WON_REQOP_PATCH_SIZE: i32 = 71;
/// a setter: `GetPatchList` (num 0), `GetValidPatchList` (1), `GetHighestPatchList(from, to)` (2) and
/// `GetHighestPatchList(from)` (3), the bytes "from\0to"; the chosen patches' indexes read as this number (the
/// element: the position; -1 past the end)
pub const EE_WON_REQOP_PATCH_QUERY: i32 = 72;

/// `Version_Invalid`, `Version_Valid_NotLatest`, `Version_Valid_Latest`
const VERSION_INVALID: i64 = 0;
const VERSION_NOT_LATEST: i64 = 1;
const VERSION_LATEST: i64 = 2;

const DIR_GF_DECOMPROOT: u32 = 0x1;
const DIR_GF_DECOMPSERVICES: u32 = 0x2;
const DIR_GF_ADDDISPLAYNAME: u32 = 0x20;
const DIR_GF_ADDDOTYPE: u32 = 0x200;
const DIR_GF_ADDDODATA: u32 = 0x400;
const DIR_GF_ADDDATAOBJECTS: u32 = 0x800;
const DIR_GF_SERVADDNAME: u32 = 0x0200_0000;
const DIR_GF_SERVADDNETADDR: u32 = 0x0400_0000;

/// `VersionBaseOp`'s members.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct VersionBase {
    pub product: Vec<u8>,
    pub extra_config: Vec<u8>,
    pub filters: BTreeSet<Vec<u8>>,
    pub valid: BTreeSet<Vec<u8>>,
    pub filtered: BTreeSet<Vec<u8>>,
    pub highest: Vec<u8>,
    pub descriptions: BTreeMap<Vec<u8>, Vec<u8>>,
    pub filtered_descriptions: BTreeMap<Vec<u8>, Vec<u8>>,
}

impl VersionBase {
    /// `GetVersionDir`: "/product/Version[/extra]" (the narrow texts as `StringToWString` read them: to a NUL)
    pub fn version_dir(&self) -> Vec<u16> {
        let mut p = text::from_utf8("/");
        p.extend(text::utf16_from_game(c_str(&self.product)));
        p.extend(text::from_utf8("/Version"));
        if !self.extra_config.is_empty() {
            p.extend(text::from_utf8("/"));
            p.extend(text::utf16_from_game(c_str(&self.extra_config)));
        }
        p
    }

    /// `SetFilterString`: lower case, then the pieces between commas
    pub fn set_filter_string(&mut self, s: &[u8]) {
        let lower: Vec<u8> = s.iter().map(u8::to_ascii_lowercase).collect();
        self.filters = patch::filter_set(&lower);
    }

    /// `BuildValidVersionSet(entity)`: its data objects' types are the versions; a "pt" line naming a filter
    /// outside ours files a version as filtered (its data read to a NUL, as the original's `std::string` of
    /// `data()`); the descriptions keep their earlier entries
    pub fn build(&mut self, e: &Entity) {
        self.valid.clear();
        self.filtered.clear();
        self.highest.clear();
        for o in &e.data_objects {
            let version = &o.data_type;
            let kept = match &o.data {
                None => true,
                Some(d) => !patch::is_filtered(c_str(d), &self.filters),
            };
            let (set, map) = if kept {
                if self.highest.is_empty() || version.as_slice() > self.highest.as_slice() {
                    self.highest = version.clone();
                }
                (&mut self.valid, &mut self.descriptions)
            } else {
                (&mut self.filtered, &mut self.filtered_descriptions)
            };
            set.insert(version.clone());
            let d = map.entry(version.clone()).or_default();
            if let Some(data) = &o.data {
                *d = data.clone();
            }
        }
    }

    /// `CheckVersion(id)`
    pub fn check(&self, id: &[u8]) -> i64 {
        if !self.valid.contains(id) {
            VERSION_INVALID
        } else if id == self.highest.as_slice() {
            VERSION_LATEST
        } else {
            VERSION_NOT_LATEST
        }
    }

    fn describe(&self) -> String {
        let set = |s: &BTreeSet<Vec<u8>>| s.iter().map(|v| hex(v)).collect::<Vec<_>>().join(",");
        let map = |m: &BTreeMap<Vec<u8>, Vec<u8>>| {
            m.iter().map(|(k, v)| format!("{}={}", hex(k), hex(v))).collect::<Vec<_>>().join(",")
        };
        format!(
            "v[{}] f[{}] h{} d[{}] fd[{}]",
            set(&self.valid),
            set(&self.filtered),
            hex(&self.highest),
            map(&self.descriptions),
            map(&self.filtered_descriptions)
        )
    }
}

/// `PatchInfo`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct PatchInfo {
    pub from: Vec<u8>,
    pub to: Vec<u8>,
    pub name: Vec<u16>,
    pub checksum: u32,
    pub size: u32,
    /// `mPatchLocation` (`None`: NULL)
    pub location: Option<Vec<u8>>,
    pub data: BTreeMap<Vec<u8>, Option<Vec<u8>>>,
}

impl PatchInfo {
    /// `DoFinish`'s patch of a service entity
    fn of(e: &Entity) -> PatchInfo {
        let mut p = PatchInfo { location: (e.fields & 0x8 != 0).then(|| e.net_addr.clone()), ..Default::default() };
        // WStringToString(mName): to its first NUL
        let end = e.name.iter().position(|&u| u == 0).unwrap_or(e.name.len());
        let id = text::game_from_utf16(&e.name[..end]);
        match miscops::patch_id_split(&id) {
            None => p.to = id,
            Some(at) => {
                p.from = id[..at].to_vec();
                p.to = id[at + 1..].to_vec();
            }
        }
        p.name = e.display_name.clone();
        for o in &e.data_objects {
            // a NULL data the original dereferenced reads as empty here
            let d = o.data.as_deref().unwrap_or(&[]);
            match o.data_type.as_slice() {
                b"_cs" => {
                    if let Some(v) = miscops::le32_field(d) {
                        p.checksum = v;
                    }
                }
                b"_ps" => {
                    if let Some(v) = miscops::le32_field(d) {
                        p.size = v;
                    }
                }
                t => {
                    p.data.insert(t.to_vec(), o.data.clone());
                }
            }
        }
        p
    }

    fn describe(&self) -> String {
        let data: Vec<String> = self
            .data
            .iter()
            .map(|(k, v)| format!("{}={}", hex(k), v.as_ref().map_or("n".to_string(), |v| hex(v))))
            .collect();
        format!(
            "{}~{} {} {} {} {} [{}]",
            hex(&self.from),
            hex(&self.to),
            hex(&text::utf16_to_le_bytes(&self.name)),
            self.checksum,
            self.size,
            self.location.as_ref().map_or("n".to_string(), |l| hex(l)),
            data.join(",")
        )
    }
}

/// `GetVersionOp` (`find` false) and `FindPatchOp`.
pub struct VersionOp {
    find: bool,
    base: VersionBase,
    patches: Vec<PatchInfo>,
    /// the directory op of the running request (`RunHook` makes it)
    dir: Option<Box<dyn RequestOp>>,
    check: i64,
    query: Vec<usize>,
}

impl VersionOp {
    pub fn new(find: bool, base: VersionBase) -> VersionOp {
        VersionOp { find, base, patches: Vec::new(), dir: None, check: VERSION_INVALID, query: Vec::new() }
    }

    /// the directory op's flags (`RunHook`'s)
    fn flags(&self) -> u32 {
        if !self.find {
            return DIR_GF_DECOMPROOT | DIR_GF_ADDDATAOBJECTS | DIR_GF_ADDDOTYPE | DIR_GF_ADDDODATA;
        }
        let mut f = DIR_GF_DECOMPSERVICES
            | DIR_GF_SERVADDNAME
            | DIR_GF_ADDDISPLAYNAME
            | DIR_GF_SERVADDNETADDR
            | DIR_GF_ADDDATAOBJECTS
            | DIR_GF_ADDDOTYPE
            | DIR_GF_ADDDODATA;
        if self.base.valid.is_empty() {
            f |= DIR_GF_DECOMPROOT;
        }
        f
    }

    /// `DoFinish` after the listing succeeded
    pub fn finish(&mut self, list: &[Entity]) {
        if !self.find {
            // the original read the first entity of an empty listing too (past the list's end): none here
            if let Some(e) = list.first() {
                self.base.build(e);
            }
            return;
        }
        for e in list {
            if e.kind == b'D' {
                self.base.build(e);
            } else {
                self.patches.push(PatchInfo::of(e));
            }
        }
    }

    /// The patch lists (`FindPatchMsg::List`): 0 all from `from`, 1 to a valid version, 2 the highest past `to`
    pub fn patch_query(&self, which: i64, from: &[u8], to: &[u8]) -> Vec<usize> {
        let refs: Vec<Patch> = self.patches.iter().map(|p| Patch { from: &p.from, to: &p.to }).collect();
        let mut cmp = |a: &[u8], b: &[u8]| a.cmp(b) as i32;
        match which {
            0 => miscops::patch_list(from, &refs, &mut cmp),
            1 => miscops::valid_patch_list(from, &refs, &mut cmp, &mut |v| self.base.valid.contains(v)),
            _ => {
                let to = if which == 3 { self.base.highest.as_slice() } else { to };
                miscops::highest_patch_list(from, to, &refs, &mut cmp, &mut |v| self.base.filtered.contains(v))
            }
        }
    }
}

impl RequestOp for VersionOp {
    fn run_hook(&mut self) {
        if self.find {
            // FindPatchOp's RunHook clears its patches only: the version sets stay (and choose the flags)
            self.patches.clear();
        } else {
            let b = &mut self.base;
            b.valid.clear();
            b.filtered.clear();
            b.highest.clear();
        }
        let members = [
            Member { num: i64::from(self.flags()), bytes: Vec::new() },
            Member { num: 0, bytes: text::utf16_to_le_bytes(&self.base.version_dir()) },
            Member { num: 0, bytes: Vec::new() },
        ];
        self.dir = crate::reqdir::new_op(EE_WON_REQOPS_DIR_GET, &members);
    }

    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        match &mut self.dir {
            Some(d) => d.next_request(field_size),
            None => (ST_RECV, None),
        }
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let Some(d) = &mut self.dir else {
            return Checked::Cut;
        };
        let r = d.check_response(response);
        if r == Checked::Status(ST_SUCCESS) {
            let list = d.entities(EE_WON_REQOP_DIR_LIST, 0);
            self.finish(&list);
        }
        r
    }

    fn reset(&mut self) {
        if let Some(d) = &mut self.dir {
            d.reset();
        }
    }

    fn describe(&self) -> String {
        let p: Vec<String> = self.patches.iter().map(PatchInfo::describe).collect();
        format!("{} p[{}]", self.base.describe(), p.join(";"))
    }

    fn num_at(&self, which: i32, index: i64) -> i64 {
        let i = usize::try_from(index).unwrap_or(usize::MAX);
        let patch = self.patches.get(i);
        match which {
            EE_WON_REQOP_VERSION_VALID => self.base.valid.len() as i64,
            EE_WON_REQOP_VERSION_FILTERED => self.base.filtered.len() as i64,
            EE_WON_REQOP_VERSION_CHECK => self.check,
            EE_WON_REQOP_PATCH_COUNT => self.patches.len() as i64,
            EE_WON_REQOP_PATCH_CHECKSUM => patch.map_or(0, |p| i64::from(p.checksum)),
            EE_WON_REQOP_PATCH_SIZE => patch.map_or(0, |p| i64::from(p.size)),
            EE_WON_REQOP_PATCH_QUERY => self.query.get(i).map_or(-1, |&n| n as i64),
            _ => 0,
        }
    }

    fn bytes_at(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        let i = usize::try_from(index).unwrap_or(usize::MAX);
        let patch = self.patches.get(i);
        match which {
            EE_WON_REQOP_VERSION_HIGHEST => Some(self.base.highest.clone()),
            EE_WON_REQOP_VERSION_VALID => self.base.valid.iter().nth(i).cloned(),
            EE_WON_REQOP_VERSION_FILTERED => self.base.filtered.iter().nth(i).cloned(),
            EE_WON_REQOP_VERSION_DESCRIPTION => {
                self.base.valid.iter().nth(i).and_then(|v| self.base.descriptions.get(v)).cloned()
            }
            EE_WON_REQOP_PATCH_FROM => patch.map(|p| p.from.clone()),
            EE_WON_REQOP_PATCH_TO => patch.map(|p| p.to.clone()),
            EE_WON_REQOP_PATCH_NAME => patch.map(|p| text::utf16_to_le_bytes(&p.name)),
            EE_WON_REQOP_PATCH_LOCATION => patch.and_then(|p| p.location.clone()),
            _ => None,
        }
    }

    fn set(&mut self, which: i32, num: i64, data: &[u8]) {
        match which {
            EE_WON_REQOP_VERSION_CHECK => self.check = self.base.check(data),
            EE_WON_REQOP_PATCH_QUERY => {
                let (from, to) = match data.iter().position(|&b| b == 0) {
                    Some(n) => (&data[..n], &data[n + 1..]),
                    None => (data, &[][..]),
                };
                self.query = self.patch_query(num, from, to);
            }
            _ => {}
        }
    }
}

/// The op of `kind` with its members; `None` for another kind.
pub(crate) fn new_op(kind: i32, members: &[Member]) -> Option<Box<dyn RequestOp>> {
    if kind != EE_WON_REQOPS_GET_VERSION && kind != EE_WON_REQOPS_FIND_PATCH {
        return None;
    }
    let mut at = 0;
    let mut next = || {
        let m = members.get(at).cloned().unwrap_or_default();
        at += 1;
        m
    };
    let mut base = VersionBase { product: next().bytes, extra_config: next().bytes, ..Default::default() };
    base.set_filter_string(&next().bytes);
    if kind == EE_WON_REQOPS_FIND_PATCH {
        for _ in 0..next().num.max(0) {
            let v = next().bytes;
            base.descriptions.insert(v.clone(), next().bytes);
            base.valid.insert(v);
        }
        for _ in 0..next().num.max(0) {
            base.filtered.insert(next().bytes);
        }
    }
    Some(Box::new(VersionOp::new(kind == EE_WON_REQOPS_FIND_PATCH, base)))
}

#[cfg(test)]
#[path = "verop_tests.rs"]
mod tests;
