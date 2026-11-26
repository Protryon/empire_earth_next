//! The lobby's server list from a directory listing (R6 WON round 34): `LobbyServerList::UpdateFromDirectory` and
//! `LobbyServer::ReadDirEntity` (WONLobby/LobbyModelHost.cpp) over the Rust listing (ee_won_net dirlist.rs). Each
//! entity's server is found or made by its address; a server not connected takes the entity's display name, its
//! address, its population (a `_C` data object) and its address names (an `_L` data object: a comma-separated list
//! read as `StringParser::ReadValue(std::list<std::string>&)`). The C++ statements (the model's setters, the list's
//! insert) are the host's one-statement cases.
//!
//! Reached by nothing in the game today: the directory servers are gone.

use std::ffi::c_void;

use ee_won_net::dirlist::{net_ip, EeWonDirList};
use ee_won_net::ipobj::EeWonIpAddr;

use crate::model::{dir_object, DirObject};

/// The host's statements (`obj` the list or a server, `a`, `b` as each says).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDirReadOp {
    /// `(intptr_t)list->M(LobbyMC::ServerListInsert, addr a)`: the server (borrowed)
    ServerListInsert,
    /// `server->IsConnected()`
    ServerConnected,
    /// `server->MSetText(LobbyMF::ServerName, GUIString(the a, b UTF-16 units))`
    ServerSetName,
    /// `server->MSetAddr(LobbyMF::ServerAddress, addr a)`
    ServerSetAddr,
    /// `server->M(LobbyMC::ServerSetListed, a)`
    ServerSetListed,
    /// `server->M(LobbyMC::ServerAddressesClear)`
    ServerAddressesClear,
    /// `server->M(LobbyMC::ServerAddressAdd, a, b)`: a name's bytes
    ServerAddressAdd,
}

/// The host: `(op, obj, a, b)`; NULL in the game, whose statements are the model's own calls ([`game_op`]).
pub type EeWonDirReadHost =
    Option<unsafe extern "C" fn(op: EeWonDirReadOp, obj: *mut c_void, a: isize, b: isize) -> isize>;

/// The game's statements on the list's and the servers' model objects (R6 WON GUI round 40: LobbyModelHost.cpp's
/// host until then). The model is the shell's member after its vtable and count (LobbyModelHost.cpp asserts it).
fn game_op(op: EeWonDirReadOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    use crate::modelobj::{ee_won_model_call, ee_won_model_set, EeWonModelCall as MC, EeWonModelField as MF};
    type O = EeWonDirReadOp;
    if obj.is_null() {
        return 0;
    }
    // SAFETY: a live LobbyModelObject: its model at its first member
    let m = unsafe { *((obj as usize + 2 * std::mem::size_of::<usize>()) as *const *const crate::modelobj::EeWonModel) };
    let empty = ee_won_gui::guistrobj::ee_won_gstr_empty() as isize;
    // SAFETY: the shell's live model (the calls' contracts as the C++ made them)
    unsafe {
        match op {
            O::ServerListInsert => ee_won_model_call(m, MC::ServerListInsert, a, 0, 0, empty),
            O::ServerConnected => ee_won_model_call(m, MC::ServerConnected, 0, 0, 0, empty),
            // `MSetText(ServerName, GUIString(units))`: the temporary's reference around the set
            O::ServerSetName => {
                let text = crate::lobbytext::held_units(&crate::lobbytext::wide_units(a, b as usize));
                ee_won_model_set(m, MF::ServerName, text);
                crate::lobbytext::release(text);
                0
            }
            O::ServerSetAddr => {
                ee_won_model_set(m, MF::ServerAddress, a);
                0
            }
            O::ServerSetListed => ee_won_model_call(m, MC::ServerSetListed, a, 0, 0, empty),
            O::ServerAddressesClear => ee_won_model_call(m, MC::ServerAddressesClear, 0, 0, 0, empty),
            O::ServerAddressAdd => ee_won_model_call(m, MC::ServerAddressAdd, a, b, 0, empty),
        }
    }
}

/// `UpdateFromDirectory`'s statements over `out`.
pub fn update_from_directory(
    list: *mut c_void,
    dir: &EeWonDirList,
    out: &mut dyn FnMut(EeWonDirReadOp, *mut c_void, isize, isize) -> isize,
) {
    type O = EeWonDirReadOp;
    for e in &dir.entities {
        let addr: EeWonIpAddr = net_ip(e);
        let server = out(O::ServerListInsert, list, &addr as *const EeWonIpAddr as isize, 0) as *mut c_void;
        if out(O::ServerConnected, server, 0, 0) != 0 {
            continue;
        }
        // ReadDirEntity
        out(O::ServerSetName, server, e.display_name.as_ptr() as isize, e.display_name.len() as isize);
        let again = net_ip(e);
        out(O::ServerSetAddr, server, &again as *const EeWonIpAddr as isize, 0);
        for o in &e.data_objects {
            match dir_object(&o.data_type, o.data.as_deref()) {
                DirObject::Population(n) => {
                    out(O::ServerSetListed, server, n as isize, 0);
                }
                DirObject::Addresses => {
                    out(O::ServerAddressesClear, server, 0, 0);
                    // (the original read a NULL data object's bytes: none here)
                    let text = o.data.as_deref().unwrap_or(&[]);
                    for name in ee_won_net::ffi_parse::read_string_list(text) {
                        out(O::ServerAddressAdd, server, name.as_ptr() as isize, name.len() as isize);
                    }
                }
                DirObject::Other => {}
            }
        }
    }
}

/// `LobbyServerList::UpdateFromDirectory(dir)` for the C++ list `list`.
///
/// # Safety
/// `dir` is NULL or a live listing; `host` answers the ops for `list` and its servers.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_dir_update(list: *mut c_void, dir: *const EeWonDirList, host: EeWonDirReadHost) {
    // SAFETY: the contract
    let Some(dir) = (unsafe { dir.as_ref() }) else { return };
    update_from_directory(list, dir, &mut |op, obj, a, b| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(op, obj, a, b) },
        None => game_op(op, obj, a, b),
    });
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_won_net::dir::{DataObject, Entity};

    #[test]
    fn a_connected_server_is_left_and_the_others_read() {
        let dir = EeWonDirList {
            entities: vec![
                Entity {
                    display_name: vec![b'A' as u16],
                    net_addr: vec![0, 1, 10, 0, 0, 1],
                    data_objects: vec![
                        DataObject { data_type: b"_C".to_vec(), data: Some(vec![5, 1]) },
                        DataObject { data_type: b"_L".to_vec(), data: Some(b"one, two\0three".to_vec()) },
                        DataObject { data_type: b"_X".to_vec(), data: None },
                    ],
                    ..Entity::default()
                },
                Entity { net_addr: vec![0, 2, 10, 0, 0, 2], ..Entity::default() },
            ],
        };
        let mut log = Vec::new();
        let mut n = 0;
        update_from_directory(std::ptr::null_mut(), &dir, &mut |op, _obj, a, b| {
            match op {
                EeWonDirReadOp::ServerListInsert => {
                    n += 1;
                    log.push(format!("insert {n}"));
                    return n;
                }
                EeWonDirReadOp::ServerConnected => return (n == 2) as isize,
                EeWonDirReadOp::ServerAddressAdd => {
                    let s = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
                    log.push(format!("add {}", String::from_utf8_lossy(s)));
                }
                o => log.push(format!("{o:?} {}", if o == EeWonDirReadOp::ServerSetListed { a } else { b })),
            }
            0
        });
        assert_eq!(
            log,
            [
                "insert 1",
                "ServerSetName 1",
                "ServerSetAddr 0",
                "ServerSetListed 261",
                "ServerAddressesClear 0",
                "add one",
                "add two",
                "insert 2"
            ]
        );
    }
}
