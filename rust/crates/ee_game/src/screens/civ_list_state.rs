//! The civilization builder list box's own state (`EECBUIListBox`'s members until player/tech round 3: mC13C, mC148,
//! mSelectUpgradesOnly, mCopiedShowPoints, mShowPoints, mSortLines, mIsTabable), kept by the list's C++ object
//! (`static_cast<UIControl*>(this)`). Its constructor's body sets the flags (hud_bodies.rs), the builder's and the
//! choice form's ops read and set them (civ_direct.rs, the screens' `cb_list_set_*`), the C++ asks `GetTabable`
//! here and drops the state in its destructor. A list the state never saw reads as zeros (the C++ members'
//! uninitialized bytes were never read before the constructor's body set them).

use std::collections::BTreeMap;
use std::sync::Mutex;

/// One list's members.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct CbListState {
    /// `mC13C`: the header colour (black; the choice form's own).
    pub header: [f32; 3],
    /// `mC148`: the header colour as of the last CopyData.
    pub copied_header: [f32; 3],
    /// `mSelectUpgradesOnly`: only the upgrade lines selectable.
    pub upgrades_only: bool,
    /// `mCopiedShowPoints`: the points shown as of the last CopyData.
    pub copied_points: bool,
    /// `mShowPoints`.
    pub show_points: bool,
    /// `mSortLines`.
    pub sort: bool,
    /// `mIsTabable`: the constructor's last argument.
    pub tabable: bool,
}

static LISTS: Mutex<BTreeMap<usize, CbListState>> = Mutex::new(BTreeMap::new());

fn lists() -> std::sync::MutexGuard<'static, BTreeMap<usize, CbListState>> {
    LISTS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The list `list`'s state (zeros when it has none).
pub fn get(list: usize) -> CbListState {
    lists().get(&list).copied().unwrap_or_default()
}

/// The list `list`'s state changed by `f`.
pub fn update(list: usize, f: impl FnOnce(&mut CbListState)) {
    f(lists().entry(list).or_default());
}

/// `EECBUIListBox::~EECBUIListBox`: the list's state dropped.
#[no_mangle]
pub extern "C" fn ee_screens_cbb_destroy(list: usize) {
    lists().remove(&list);
}

/// `EECBUIListBox::GetTabable` (`mIsTabable`).
#[no_mangle]
pub extern "C" fn ee_screens_cbl_tabable(list: usize) -> bool {
    get(list).tabable
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_list_keeps_its_members_until_it_is_destroyed() {
        assert_eq!(get(0x10), CbListState::default());
        update(0x10, |s| {
            s.tabable = true;
            s.header = [0.0, 0.0, 0.39];
        });
        assert!(ee_screens_cbl_tabable(0x10));
        assert_eq!(get(0x10).header, [0.0, 0.0, 0.39]);
        ee_screens_cbb_destroy(0x10);
        assert_eq!(get(0x10), CbListState::default());
    }
}
