//! The computer players' names of classes and techs (dbobjects / dbtechtree "id": "citizen", "b_barracks",
//! "inf01_clubman"), were EECPNext.cpp's: the db text loader hands each load's row ids here (`ee_ai_names_capture`,
//! EEDataTextFile.cpp: were EEDbRowIds' map), the tables are made from them at the first question and kept for the
//! process (as the C++'s statics were), and a key without an id is named "<table>_<key>".

use std::collections::BTreeMap;
use std::ffi::{c_char, c_void, CStr};
use std::sync::{Mutex, OnceLock};

use super::names::Names;
use crate::world::view::{ClassView, WorldRead};

/// The two tables: 0 dbobjects (the classes), 1 dbtechtree (the techs).
const TABLES: [(&[u8], &[u8]); 2] = [(b"dbobjects", b"dbobjects_"), (b"dbtechtree", b"dbtechtree_")];

/// The last load's row ids of each table (key -> id; a repeated key keeps its last id, as the C++ map did).
static IDS: Mutex<[BTreeMap<i64, Vec<u8>>; 2]> = Mutex::new([BTreeMap::new(), BTreeMap::new()]);
static NAMES: OnceLock<[Names; 2]> = OnceLock::new();

fn table_index(name: &[u8]) -> Option<usize> {
    TABLES.iter().position(|(t, _)| *t == name)
}

fn names(table: usize) -> &'static Names {
    &NAMES.get_or_init(|| {
        let ids = IDS.lock().unwrap_or_else(|e| e.into_inner());
        [0, 1].map(|k| Names::new(ids[k].iter().map(|(&key, id)| (key, id.clone())), TABLES[k].1))
    })[table]
}

unsafe extern "C" fn visit(ctx: *mut c_void, key: i32, id: *const u8, len: usize) {
    let map = &mut *(ctx as *mut BTreeMap<i64, Vec<u8>>);
    let bytes = if len == 0 { Vec::new() } else { std::slice::from_raw_parts(id, len).to_vec() };
    map.insert(i64::from(key), bytes);
}

/// The row ids of a table just loaded from its text (dbobjects, dbtechtree; other tables: nothing): kept for the
/// names tables (emptied first; a table that does not parse leaves what was read before the failure).
///
/// # Safety
/// `content` is the db text content (an `EeDataContent*`), `table` NUL ended.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_names_capture(content: *mut c_void, table: *const c_char) {
    let name = CStr::from_ptr(table).to_bytes();
    let Some(k) = table_index(name) else { return };
    let mut ids = IDS.lock().unwrap_or_else(|e| e.into_inner());
    ids[k].clear();
    let map = &mut ids[k] as *mut BTreeMap<i64, Vec<u8>> as *mut c_void;
    ee_data::ffi::ee_data_ids(content as *mut ee_data::ffi::EeDataContent, table, visit, map, std::ptr::null_mut());
}

/// `EECPNextClassName`: a class's data id ("dbobjects_<n>" without one).
pub fn class_name(class: i64) -> Vec<u8> {
    names(0).name_of(class).into_owned()
}

/// `EECPNextTechName`: a tech's data id ("dbtechtree_<n>" without one).
pub fn tech_name(tech: i64) -> Vec<u8> {
    names(1).name_of(tech).into_owned()
}

/// `EECPNextClassByName`: the class of a data id, -1 for none or a class the game does not have.
pub fn class_by_name(name: &[u8]) -> i64 {
    let class = names(0).key_of(name);
    if class < 0 {
        return -1;
    }
    // SAFETY: the unit classes are the database's, live while the computer players run
    let t = unsafe { WorldRead::begin() };
    if (class as u64) < ClassView::count(&t) as u64 && ClassView::by_index(&t, class as usize).is_some() {
        class
    } else {
        -1
    }
}

/// `EECPNextTechByName`: the tech of a data id, -1 for none.
pub fn tech_by_name(name: &[u8]) -> i64 {
    names(1).key_of(name)
}

/// A name into `buf` (room for `cap`, truncated past it) -> its length: `op` 0 a class's, else a tech's (the game
/// log's orders, EEPhysics.cpp `ee_units_order_log_host`).
///
/// # Safety
/// `buf` holds `cap` writable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_name_copy(op: u8, key: i64, buf: *mut u8, cap: usize) -> usize {
    let name = if op != 0 { tech_name(key) } else { class_name(key) };
    let n = name.len().min(cap);
    if n > 0 {
        std::ptr::copy_nonoverlapping(name.as_ptr(), buf, n);
    }
    n
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn capture_visit_keeps_the_last_id_of_a_key() {
        let mut map = BTreeMap::new();
        let ctx = &mut map as *mut BTreeMap<i64, Vec<u8>> as *mut c_void;
        // SAFETY: the map and the bytes are live
        unsafe {
            visit(ctx, 3, b"citizen".as_ptr(), 7);
            visit(ctx, 3, b"citizen_2".as_ptr(), 9);
            visit(ctx, -1, std::ptr::null(), 0);
        }
        assert_eq!(map.get(&3i64).map(Vec::as_slice), Some(&b"citizen_2"[..]));
        assert_eq!(map.get(&-1i64).map(Vec::as_slice), Some(&b""[..]));
    }

    #[test]
    fn tables_by_name() {
        assert_eq!(table_index(b"dbobjects"), Some(0));
        assert_eq!(table_index(b"dbtechtree"), Some(1));
        assert_eq!(table_index(b"dbworld"), None);
    }
}
