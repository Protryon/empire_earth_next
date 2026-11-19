//! The widget registry (R6 WONLobby round 19): each C++ widget's Rust objects by the widget's address.
//!
//! Every Rust widget object is made for one C++ component (`ee_won_*_new(component, host)`), and the C++ class's
//! methods are one-line forwards to it (`ee_won_input_call(this->mRust, ...)`). The registry records the pair when
//! the object is made and forgets it when the object is freed, so a Rust caller that holds only the C++ pointer (the
//! lobby's dialogs and controls hold their controls by it) can make the forward itself instead of asking a C++ host
//! to make it. One widget has several objects (a `ListArea` is a container, a scroll pane and a list): the key is the
//! address and the object's kind.
//!
//! The maps are only looked up, never iterated, so their order is not behaviour; the lock is held for the look-up
//! alone, never across a call into an object.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

/// A Rust widget object's kind (the module that made it).
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonGuiKind {
    /// inputobj.rs `EeWonInput`
    Input = 1,
    /// labelobj.rs `EeWonLabel`
    Label,
    /// btnobj.rs `EeWonButton`
    Button,
    /// msbtnobj.rs `EeWonMsButton`
    MsButton,
    /// comboobj.rs `EeWonComboBox`
    Combo,
    /// listobj.rs `EeWonList`
    List,
    /// multiobj.rs `EeWonMulti`
    Multi,
    /// treeobj.rs `EeWonTree`
    Tree,
    /// ctobj.rs `EeWonContainer`
    Container,
    /// dialogobj.rs `EeWonDialog`
    Dialog,
    /// textobj.rs `EeWonText`
    Text,
    /// menuobj.rs `EeWonMenu`
    Menu,
    /// wonbtnobj.rs `EeWonWonButton`
    WonButton,
    /// paneobj.rs `EeWonScrollPane`
    Pane,
    /// treeobj.rs `EeWonTNode` (a tree row, by its `TreeItem`)
    TreeNode,
}

struct Maps {
    by_widget: BTreeMap<(usize, EeWonGuiKind), usize>,
    by_object: BTreeMap<usize, (usize, EeWonGuiKind)>,
}

static MAPS: Mutex<Maps> = Mutex::new(Maps { by_widget: BTreeMap::new(), by_object: BTreeMap::new() });

fn maps() -> std::sync::MutexGuard<'static, Maps> {
    MAPS.lock().unwrap_or_else(|e| e.into_inner())
}

/// Records `object` as `widget`'s object of `kind` (a later one of the same widget and kind replaces it).
pub fn register(widget: *const c_void, kind: EeWonGuiKind, object: *const c_void) {
    if widget.is_null() || object.is_null() {
        return;
    }
    let mut m = maps();
    let key = (widget as usize, kind);
    if let Some(old) = m.by_widget.insert(key, object as usize) {
        m.by_object.remove(&old);
    }
    m.by_object.insert(object as usize, key);
}

/// Forgets `object` (its widget's entry too, if it is still this object's).
pub fn unregister(object: *const c_void) {
    let mut m = maps();
    if let Some(key) = m.by_object.remove(&(object as usize)) {
        if m.by_widget.get(&key) == Some(&(object as usize)) {
            m.by_widget.remove(&key);
        }
    }
}

/// `widget`'s object of `kind`, or NULL.
pub fn find(widget: isize, kind: EeWonGuiKind) -> *const c_void {
    maps().by_widget.get(&(widget as usize, kind)).map_or(std::ptr::null(), |&o| o as *const c_void)
}

/// `widget`'s object of `kind` as its type.
///
/// # Safety
/// `T` is the type `kind` names; the reference is used only while the object lives (the widget is held).
pub unsafe fn get<'a, T>(widget: isize, kind: EeWonGuiKind) -> Option<&'a T> {
    let p = find(widget, kind) as *const T;
    // SAFETY: the contract
    unsafe { p.as_ref() }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn register_find_unregister() {
        let (w, o1, o2) = (0x7000_0010usize, 0x7100_0020usize, 0x7100_0040usize);
        register(w as *const c_void, EeWonGuiKind::Input, o1 as *const c_void);
        register(w as *const c_void, EeWonGuiKind::Label, o2 as *const c_void);
        assert_eq!(find(w as isize, EeWonGuiKind::Input) as usize, o1);
        assert_eq!(find(w as isize, EeWonGuiKind::Label) as usize, o2);
        assert!(find(w as isize, EeWonGuiKind::List).is_null());
        unregister(o1 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Input).is_null());
        assert_eq!(find(w as isize, EeWonGuiKind::Label) as usize, o2);
        unregister(o2 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Label).is_null());
    }

    #[test]
    fn a_replaced_object_does_not_drop_the_new_one() {
        let (w, o1, o2) = (0x7200_0010usize, 0x7300_0020usize, 0x7300_0040usize);
        register(w as *const c_void, EeWonGuiKind::Combo, o1 as *const c_void);
        register(w as *const c_void, EeWonGuiKind::Combo, o2 as *const c_void);
        unregister(o1 as *const c_void);
        assert_eq!(find(w as isize, EeWonGuiKind::Combo) as usize, o2);
        unregister(o2 as *const c_void);
        assert!(find(w as isize, EeWonGuiKind::Combo).is_null());
        // NULLs are never recorded
        register(std::ptr::null(), EeWonGuiKind::Combo, o1 as *const c_void);
        assert!(find(0, EeWonGuiKind::Combo).is_null());
    }
}
