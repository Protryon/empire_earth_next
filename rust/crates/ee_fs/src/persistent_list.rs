//! The registry of persistent types (`FSPersistentList` before; LLE lane round 2): one prototype per chunk tag,
//! registered at start-up, asked for a fresh default object when a save's chunk is read. The prototypes are C++
//! objects (`FSPersistent`, with their virtual `CreateDefault` and destructor): the registry keeps their pointers
//! by tag and C++ makes the virtual calls.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

/// Tag -> prototype (opaque C++ pointers; Send because only C++ dereferences them).
struct Registry(BTreeMap<i32, usize>);

static REGISTRY: Mutex<Registry> = Mutex::new(Registry(BTreeMap::new()));

fn registry() -> std::sync::MutexGuard<'static, Registry> {
    REGISTRY.lock().unwrap_or_else(|e| e.into_inner())
}

/// Registers a prototype under its tag -> whether it was taken (false: the tag was already registered, the caller
/// deletes the prototype).
#[no_mangle]
pub extern "C" fn ee_fs_persistent_register(tag: i32, prototype: *mut c_void) -> bool {
    let mut r = registry();
    if r.0.contains_key(&tag) {
        return false;
    }
    r.0.insert(tag, prototype as usize);
    true
}

/// The prototype registered under a tag, or NULL.
#[no_mangle]
pub extern "C" fn ee_fs_persistent_find(tag: i32) -> *mut c_void {
    registry().0.get(&tag).map_or(std::ptr::null_mut(), |&p| p as *mut c_void)
}

/// Takes one prototype out of the registry for the caller to delete (the lowest tag first), NULL when empty: the
/// shutdown loops until NULL.
#[no_mangle]
pub extern "C" fn ee_fs_persistent_take() -> *mut c_void {
    registry().0.pop_first().map_or(std::ptr::null_mut(), |(_, p)| p as *mut c_void)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn register_find_take() {
        let (a, b) = (8usize as *mut c_void, 16usize as *mut c_void);
        assert!(ee_fs_persistent_register(4, a));
        assert!(!ee_fs_persistent_register(4, b));
        assert!(ee_fs_persistent_register(1, b));
        assert_eq!(ee_fs_persistent_find(4), a);
        assert!(ee_fs_persistent_find(5).is_null());
        assert_eq!(ee_fs_persistent_take(), b);
        assert_eq!(ee_fs_persistent_take(), a);
        assert!(ee_fs_persistent_take().is_null());
    }
}
