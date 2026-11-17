//! `extern "C"` for the scenario editor's trigger tab whole in Rust (`editor_tscreen.rs`): the object
//! `EESETriggerScreen` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_sets`
//! (`Scenario Editor/EESETriggerScreen.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::SeplHost;
use crate::screens::editor_tscreen::TriggerTab;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sets(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 400 {
                sys::ee_screenscpp_sets(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                crate::screens::editor_world::host(op, i, f, |i, f| {
                    sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
                })
            }
        }
    }
}

/// The trigger tab (opaque to C++).
pub struct EeScreensSeTriggerTab(TriggerTab);

/// The tab's members: `AlphaSort`, `SelectEntry`, `Leave`, `Update`, `DecPriority`, `DeleteEntry`,
/// `DuplicateEntry`, `ShowTab(a: the button's id)`, `StoreEntry`, `Disable`, `HideText`, `ShowText`,
/// `HideAlphaSort`, `ShowAlphaSort`, `ShowSelection`, `IncPriority`, `HideGroups`, `Load`, `NewEntry`, `Enter`,
/// `SelectName`, `FillList`, `Enable`, `UpdateKeys`, `MouseMove(x, y)`, `RefreshScreen`, `Draw(p: the rectangle)`;
/// the constructor's end after the sub-screens; the destructor's two halves around them.
pub const EE_SCREENS_SETS_ALPHA_SORT: u32 = 1;
pub const EE_SCREENS_SETS_SELECT_ENTRY: u32 = 2;
pub const EE_SCREENS_SETS_LEAVE: u32 = 3;
pub const EE_SCREENS_SETS_UPDATE: u32 = 4;
pub const EE_SCREENS_SETS_DEC_PRIORITY: u32 = 5;
pub const EE_SCREENS_SETS_DELETE_ENTRY: u32 = 6;
pub const EE_SCREENS_SETS_DUPLICATE_ENTRY: u32 = 7;
pub const EE_SCREENS_SETS_SHOW_TAB: u32 = 8;
pub const EE_SCREENS_SETS_STORE_ENTRY: u32 = 9;
pub const EE_SCREENS_SETS_DISABLE: u32 = 10;
pub const EE_SCREENS_SETS_HIDE_TEXT: u32 = 11;
pub const EE_SCREENS_SETS_SHOW_TEXT: u32 = 12;
pub const EE_SCREENS_SETS_HIDE_ALPHA_SORT: u32 = 13;
pub const EE_SCREENS_SETS_SHOW_ALPHA_SORT: u32 = 14;
pub const EE_SCREENS_SETS_SHOW_SELECTION: u32 = 15;
pub const EE_SCREENS_SETS_INC_PRIORITY: u32 = 16;
pub const EE_SCREENS_SETS_HIDE_GROUPS: u32 = 17;
pub const EE_SCREENS_SETS_NEW_ENTRY: u32 = 18;
pub const EE_SCREENS_SETS_ENTER: u32 = 19;
pub const EE_SCREENS_SETS_SELECT_NAME: u32 = 20;
pub const EE_SCREENS_SETS_FILL_LIST: u32 = 21;
pub const EE_SCREENS_SETS_ENABLE: u32 = 22;
pub const EE_SCREENS_SETS_UPDATE_KEYS: u32 = 23;
pub const EE_SCREENS_SETS_MOUSE_MOVE: u32 = 24;
pub const EE_SCREENS_SETS_REFRESH_SCREEN: u32 = 25;
pub const EE_SCREENS_SETS_DRAW: u32 = 26;
pub const EE_SCREENS_SETS_FINISH: u32 = 27;
pub const EE_SCREENS_SETS_DESTROY_FIRST: u32 = 28;
pub const EE_SCREENS_SETS_DESTROY_REST: u32 = 29;

/// The tab's controls by name (`ee_screens_sets_control`): the list, the name and description boxes, the sub-tab
/// `a`; the open sub-screen and its control group.
pub const EE_SCREENS_SETS_LIST: u32 = 1;
pub const EE_SCREENS_SETS_NAME_EDIT: u32 = 2;
pub const EE_SCREENS_SETS_TEXT_EDIT: u32 = 3;
pub const EE_SCREENS_SETS_TAB: u32 = 4;
pub const EE_SCREENS_SETS_CURRENT_SCREEN: u32 = 5;
pub const EE_SCREENS_SETS_CURRENT_GROUP: u32 = 6;

/// `EESETriggerScreen`'s constructor up to its sub-screens: the controls made on `form` from the control index
/// `*index` (advanced); `shell` is the C++ `EESETriggerScreen`. Freed by `ee_screens_sets_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_new(
    shell: *mut c_void,
    form: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeTriggerTab {
    let mut next = unsafe { *index };
    let t = TriggerTab::new(&mut Host, shell as usize, form as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSeTriggerTab(t)))
}

/// The object freed (its controls were deleted by `EE_SCREENS_SETS_DESTROY_FIRST` and `_REST`).
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_free(tab: *mut EeScreensSeTriggerTab) {
    if !tab.is_null() {
        drop(unsafe { Box::from_raw(tab) });
    }
}

/// One of the tab's members (`EE_SCREENS_SETS_ALPHA_SORT` ...) with its arguments.
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL); `p` is what the member takes.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_call(
    tab: *const EeScreensSeTriggerTab,
    what: u32,
    a: u32,
    x: f32,
    y: f32,
    p: *mut c_void,
) {
    if tab.is_null() {
        return;
    }
    // shared: a host call can come back into the tab (a sub-screen's store or load, the editor's events)
    let t = unsafe { &(*tab).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SETS_ALPHA_SORT => t.alpha_sort(h),
        EE_SCREENS_SETS_SELECT_ENTRY => t.select_entry(h),
        EE_SCREENS_SETS_LEAVE => t.leave(h),
        EE_SCREENS_SETS_UPDATE => t.update(h),
        EE_SCREENS_SETS_DEC_PRIORITY => t.dec_priority(h),
        EE_SCREENS_SETS_DELETE_ENTRY => t.delete_entry(h),
        EE_SCREENS_SETS_DUPLICATE_ENTRY => t.duplicate_entry(h),
        EE_SCREENS_SETS_SHOW_TAB => t.show_tab(h, a),
        EE_SCREENS_SETS_STORE_ENTRY => t.store_entry(h),
        EE_SCREENS_SETS_DISABLE => t.disable(h),
        EE_SCREENS_SETS_HIDE_TEXT => t.hide_text(h),
        EE_SCREENS_SETS_SHOW_TEXT => t.show_text(h),
        EE_SCREENS_SETS_HIDE_ALPHA_SORT => t.show_alpha_sort(h, false),
        EE_SCREENS_SETS_SHOW_ALPHA_SORT => t.show_alpha_sort(h, true),
        EE_SCREENS_SETS_SHOW_SELECTION => t.show_selection(h),
        EE_SCREENS_SETS_INC_PRIORITY => t.inc_priority(h),
        EE_SCREENS_SETS_HIDE_GROUPS => t.hide_groups(h),
        EE_SCREENS_SETS_NEW_ENTRY => t.new_entry(h),
        EE_SCREENS_SETS_ENTER => t.enter(h),
        EE_SCREENS_SETS_SELECT_NAME => t.select_name(h),
        EE_SCREENS_SETS_FILL_LIST => t.fill_list(h),
        EE_SCREENS_SETS_ENABLE => t.enable(h),
        EE_SCREENS_SETS_UPDATE_KEYS => {
            t.when_selected(h, crate::screens::editor_tscreen::EE_SCREENS_SETS_SCREEN_UPDATE_KEYS, 0.0, 0.0)
        }
        EE_SCREENS_SETS_MOUSE_MOVE => {
            t.when_selected(h, crate::screens::editor_tscreen::EE_SCREENS_SETS_SCREEN_MOUSE_MOVE, x, y)
        }
        EE_SCREENS_SETS_REFRESH_SCREEN => {
            t.when_selected(h, crate::screens::editor_tscreen::EE_SCREENS_SETS_SCREEN_REFRESH, 0.0, 0.0)
        }
        EE_SCREENS_SETS_DRAW => t.draw(h, p as usize),
        EE_SCREENS_SETS_FINISH => t.finish(h),
        EE_SCREENS_SETS_DESTROY_FIRST => t.destroy_first(h),
        EE_SCREENS_SETS_DESTROY_REST => t.destroy_rest(h),
        _ => {}
    }
}

/// A control (as a handle) or value of the tab (`EE_SCREENS_SETS_LIST` ...; `a` the sub-tab).
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_control(tab: *const EeScreensSeTriggerTab, what: u32, a: u32) -> *mut c_void {
    if tab.is_null() {
        return core::ptr::null_mut();
    }
    let t = unsafe { &(*tab).0 };
    let v = match what {
        EE_SCREENS_SETS_LIST => t.list,
        EE_SCREENS_SETS_NAME_EDIT => t.name_edit,
        EE_SCREENS_SETS_TEXT_EDIT => t.text_edit,
        EE_SCREENS_SETS_TAB => t.tabs.get(a as usize).copied().unwrap_or(0),
        EE_SCREENS_SETS_CURRENT_SCREEN => t.screen.get() as usize,
        EE_SCREENS_SETS_CURRENT_GROUP => t.group.get() as usize,
        _ => 0,
    };
    v as *mut c_void
}
