//! WONDir's entity changes as Rust ops (round 7): `AddEntityOp`, `ModifyEntityOp`, `RemoveEntityOp`,
//! `RenameEntityOp`, `RenewEntityOp`, `EntitySetDataObjectsOp`, `EntityModifyDataObjectsOp` and
//! `EntityClearDataObjectsOp` (each with its service and directory forms), derived from `ServerRequestOp` in Rust
//! ([`DirChangeOp`]). Their C++ classes are deleted; a C++ `ServerRequestOp` runs one made by [`ee_won_dirop_new`].
//!
//! The members are the request's fields (dirops.rs builds the bytes); the reply is the status reply, which hands an
//! added or modified service its address when it asked for it (`DIR_UF_SERVRETURNADDR`). `golden_dirop.txt` was
//! printed by `tests/won/won_dirop_test.cpp` over the original's statements before the port. Reached by nothing in
//! the game (the directory servers are gone).

use crate::dirops::{self, op, EeWonDirRequest, Object, ObjectMod, Request};
use crate::reqop::{hex, Checked, EeWonReqOp, RequestOp, ST_RECV};

/// `DIR_UF_UNIQUEDISPLAYNAME`
pub const EE_WON_DIR_UF_UNIQUE_DISPLAY_NAME: u32 = 0x01;
/// `DIR_UF_SERVRETURNADDR`
pub const EE_WON_DIR_UF_SERV_RETURN_ADDR: u32 = 0x10;

/// A data object (`DirDataObject`): its type and data (`None`: a NULL buffer).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DataObject {
    pub data_type: Vec<u8>,
    pub data: Option<Vec<u8>>,
}

/// A data object change (`DataObjectMod`).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DataObjectMod {
    pub offset: u16,
    pub insert: bool,
    pub object: DataObject,
}

/// An entity change's members.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DirChangeOp {
    /// `dirops::op`'s ADD, MODIFY, REMOVE, RENAME, RENEW, SET_DATA_OBJECTS, MODIFY_DATA_OBJECTS, CLEAR_DATA_OBJECTS
    pub op: i32,
    pub is_service: bool,
    /// the entity flags (add, modify)
    pub flags: u32,
    pub path: Vec<u16>,
    pub name: Vec<u16>,
    pub display_name: Vec<u16>,
    /// `mNetAddr` (`None`: NULL); an add or modify asking for it gets the server's
    pub net_addr: Option<Vec<u8>>,
    pub new_name: Vec<u16>,
    pub new_display_name: Vec<u16>,
    pub new_net_addr: Option<Vec<u8>>,
    /// the lifespan (add, renew) or the new lifespan (modify)
    pub lifespan: u32,
    /// rename: the new display name must be unique
    pub is_unique: bool,
    /// set data objects: `DIR_DO_*`
    pub mode: u8,
    pub objects: Vec<DataObject>,
    pub mods: Vec<DataObjectMod>,
    /// clear data objects: the types
    pub types: Vec<Vec<u8>>,
}

impl DirChangeOp {
    /// The op with its constructor's members (`Init`): an add or modify's flags `DIR_UF_UNIQUEDISPLAYNAME`, a
    /// rename unique, a set's mode `DIR_DO_ADDREPLACE`, lifespans 0.
    pub fn new(op: i32, is_service: bool) -> DirChangeOp {
        let flags = if op == op::ADD || op == op::MODIFY { EE_WON_DIR_UF_UNIQUE_DISPLAY_NAME } else { 0 };
        DirChangeOp { op, is_service, flags, is_unique: op == op::RENAME, ..Default::default() }
    }

    fn object(o: &DataObject) -> Object<'_> {
        Object { data_type: &o.data_type, data: o.data.as_deref() }
    }

    fn request(&self) -> Request<'_> {
        Request {
            op: self.op,
            is_service: self.is_service,
            flags: self.flags,
            path: self.path.clone(),
            name: self.name.clone(),
            display_name: self.display_name.clone(),
            net_addr: self.net_addr.as_deref(),
            new_name: self.new_name.clone(),
            new_display_name: self.new_display_name.clone(),
            new_net_addr: self.new_net_addr.as_deref(),
            lifespan: self.lifespan,
            is_unique: self.is_unique,
            mode: self.mode,
            objects: self.objects.iter().map(Self::object).collect(),
            mods: self
                .mods
                .iter()
                .map(|m| ObjectMod { offset: m.offset, insert: m.insert, object: Self::object(&m.object) })
                .collect(),
            types: self.types.iter().map(|t| t.as_slice()).collect(),
            ..Default::default()
        }
    }

    /// The reply reads the returned address: an added or modified service that asked for it.
    fn reads_addr(&self) -> bool {
        (self.op == op::ADD || self.op == op::MODIFY)
            && self.is_service
            && self.flags & EE_WON_DIR_UF_SERV_RETURN_ADDR != 0
    }
}

impl RequestOp for DirChangeOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        (ST_RECV, Some(dirops::request(&self.request(), field_size)))
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = dirops::status_reply(response, self.reads_addr());
        match r.end {
            1 => return Checked::InvalidHeader,
            2 => return Checked::Cut,
            _ => {}
        }
        if let Some((at, n)) = r.addr {
            self.net_addr = Some(response[at..at + n].to_vec());
        }
        Checked::Status(i32::from(r.status))
    }

    fn describe(&self) -> String {
        match &self.net_addr {
            None => "a-n".into(),
            Some(a) => format!("a{}", hex(a)),
        }
    }
}

// ---- the C ABI

/// An entity change with the members of `fields` (an `EeWonDirRequest` as the original ops filled it), for a C++
/// `ServerRequestOp` to run.
///
/// # Safety
/// `fields` and everything it points to are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirop_new(fields: *const EeWonDirRequest) -> *mut EeWonReqOp {
    let r = unsafe { dirops::owned_request(&*fields) };
    let obj = |o: &Object| DataObject { data_type: o.data_type.to_vec(), data: o.data.map(|d| d.to_vec()) };
    let o = DirChangeOp {
        op: r.op,
        is_service: r.is_service,
        flags: r.flags,
        path: r.path.clone(),
        name: r.name.clone(),
        display_name: r.display_name.clone(),
        net_addr: r.net_addr.map(|d| d.to_vec()),
        new_name: r.new_name.clone(),
        new_display_name: r.new_display_name.clone(),
        new_net_addr: r.new_net_addr.map(|d| d.to_vec()),
        lifespan: r.lifespan,
        is_unique: r.is_unique,
        mode: r.mode,
        objects: r.objects.iter().map(obj).collect(),
        mods: r
            .mods
            .iter()
            .map(|m| DataObjectMod { offset: m.offset, insert: m.insert, object: obj(&m.object) })
            .collect(),
        types: r.types.iter().map(|t| t.to_vec()).collect(),
    };
    EeWonReqOp::new(Box::new(o))
}

#[cfg(test)]
#[path = "dirchange_tests.rs"]
mod tests;
