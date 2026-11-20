//! `extern "C"` for the scenario editor's trigger tab whole in Rust (`editor_tscreen.rs`): the object
//! `EESETriggerScreen` holds, made, called and freed by it. The shared control ops (below 100) are
//! `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the tab's own `ee_screenscpp_sets`
//! (`Scenario Editor/EESETriggerScreen.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::SeplHost;
use crate::screens::editor_tscreen::*;
use crate::screens::editor_tsubs::*;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sets(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

/// The sub-screens' FFI objects (`editor_area_ffi.rs`, `editor_object_ffi.rs`, `editor_subscreens_ffi.rs`,
/// `editor_trigger_ffi.rs`).
mod subs {
    pub use crate::screens::editor_area_ffi::*;
    pub use crate::screens::editor_object_ffi::*;
    pub use crate::screens::editor_subscreens_ffi::*;
    pub use crate::screens::editor_trigger_ffi::*;
}

/// The sub-screen `k`'s slot `slot` (`EE_SCREENS_SETS_SCREEN_*`; `MouseMove` at `x`, `y`), as its class forwarded it.
///
/// # Safety
/// `s` is the tab's live record: its objects are the sub-screens' FFI boxes.
unsafe fn sub_call(s: &Subs, k: i32, slot: i32, x: f32, y: f32) {
    let Some(sub) = s.get(k) else { return };
    let obj = sub.obj.get();
    if obj == 0 {
        return;
    }
    let selection = sub.selection.get();
    match route(k as usize, slot) {
        Route::None => {}
        Route::MouseMove => match k as usize {
            SUB_OBJECT => unsafe { subs::ee_screens_seob_mouse_move(obj as *const _, x, y) },
            _ => unsafe { subs::ee_screens_sear_mouse_move(obj as *const _, x, y) },
        },
        Route::Call(what, a, b) => {
            let (a, b) = (arg(a, selection), arg(b, selection));
            unsafe {
                match k as usize {
                    SUB_TRIGGER => subs::ee_screens_setr_call(obj as *const _, what, a, b),
                    SUB_CONDITION => subs::ee_screens_secd_call(obj as *const _, what, a as i32),
                    SUB_EFFECT => subs::ee_screens_sefx_call(obj as *const _, what, a as i32),
                    SUB_OBJECT => {
                        subs::ee_screens_seob_call(obj as *const _, what, a, b);
                    }
                    _ => subs::ee_screens_sear_call(obj as *const _, what, a as i32, b as i32),
                }
            }
        }
    }
}

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        // the sub-screens' ops: the shell is the tab's record of them (p0; p1 for the names)
        match op {
            EE_SCREENS_SETS_OP_SCREEN => {
                // SAFETY: the tab passes its own record
                unsafe { sub_call(&*(p[0] as *const Subs), i[0], i[1], f[0], f[1]) };
                return 0;
            }
            // SAFETY: as above
            EE_SCREENS_SETS_OP_SELECTION => return unsafe { &*(p[0] as *const Subs) }.selection(i[0], &mut i[1]),
            EE_SCREENS_SETS_OP_SET_SELECTION => {
                // SAFETY: as above
                unsafe { &*(p[0] as *const Subs) }.set_selection(i[0], i[1], i[2]);
                return 0;
            }
            _ => {}
        }
        let mut p = p;
        if op == EE_SCREENS_SETS_OP_NAMES {
            // the C++ fills the list from the sub-screen i0's object
            // SAFETY: as above
            p[1] = unsafe { &*(p[1] as *const Subs) }.get(i[0]).map_or(0, |s| s.obj.get());
        }
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 400 {
                if let Some(r) = crate::screens::editor_direct::sets(op, p.map(|x| x as usize), i) {
                    return r;
                }
                if let Some(r) = crate::screens::editor_trigx_ffi::sets(op, p.map(|x| x as usize), i, f) {
                    return r;
                }
                sys::ee_screenscpp_sets(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                crate::screens::editor_world::host(op, i, f, |i, f| {
                    crate::screens::editor_direct::sepl(op, p.as_ptr(), i, f)
                })
            }
        }
    }
}

/// The trigger tab (opaque to C++) and its five sub-screens (its shell).
pub struct EeScreensSeTriggerTab(TriggerTab, Box<Subs>);

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

/// `EESETriggerScreen`'s constructor: the controls made on `form` from the control index `*index` (advanced), then
/// the five sub-screens (the factory 0x17c1e5: trigger, condition, effect, object, area; the screen's font is the
/// tab's), then their groups disabled. Freed by `ee_screens_sets_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_new(form: *mut c_void, index: *mut u32) -> *mut EeScreensSeTriggerTab {
    let s = Box::<Subs>::default();
    let mut next = unsafe { *index };
    let t = TriggerTab::new(&mut Host, &*s as *const Subs as usize, form as usize, &mut next);
    unsafe {
        let made: [usize; SUB_COUNT] = [
            subs::ee_screens_setr_new(form, &mut next) as usize,
            subs::ee_screens_secd_new(form, &mut next) as usize,
            subs::ee_screens_sefx_new(form, &mut next) as usize,
            subs::ee_screens_seob_new(form, &mut next) as usize,
            subs::ee_screens_sear_new(form, &mut next) as usize,
        ];
        for (sub, obj) in s.0.iter().zip(made) {
            sub.obj.set(obj);
        }
        *index = next;
    }
    t.finish(&mut Host);
    Box::into_raw(Box::new(EeScreensSeTriggerTab(t, s)))
}

/// The destructor: the list and the sub-tabs deleted, the five sub-screens (in order), the rest of the controls,
/// the object freed.
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_free(tab: *mut EeScreensSeTriggerTab) {
    if tab.is_null() {
        return;
    }
    let t = unsafe { Box::from_raw(tab) };
    t.0.destroy_first(&mut Host);
    for (k, sub) in t.1 .0.iter().enumerate() {
        let obj = sub.obj.replace(0);
        unsafe {
            match k {
                SUB_TRIGGER => subs::ee_screens_setr_free(obj as *mut _),
                SUB_CONDITION => subs::ee_screens_secd_free(obj as *mut _),
                SUB_EFFECT => subs::ee_screens_sefx_free(obj as *mut _),
                SUB_OBJECT => subs::ee_screens_seob_free(obj as *mut _),
                _ => subs::ee_screens_sear_free(obj as *mut _),
            }
        }
    }
    t.0.destroy_rest(&mut Host);
}

/// The sub-screen `k`'s object (an `EeScreensSeTrigger*` .. `EeScreensSeArea*` by `k`; NULL out of range); its
/// selection into `selection` and whether it has one into `has`, each unless NULL.
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL); `selection` and `has` are NULL or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_sub(
    tab: *const EeScreensSeTriggerTab,
    k: u32,
    selection: *mut i32,
    has: *mut bool,
) -> *mut c_void {
    let Some(s) = (unsafe { tab.as_ref() }).and_then(|t| t.1.get(k as i32)) else {
        return core::ptr::null_mut();
    };
    unsafe {
        if let Some(out) = selection.as_mut() {
            *out = s.selection.get();
        }
        if let Some(out) = has.as_mut() {
            *out = s.has_selection.get();
        }
    }
    s.obj.get() as *mut c_void
}

/// The sub-screen `k`'s selection set to `selection` (the trigger jump's entry).
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_sub_select(tab: *const EeScreensSeTriggerTab, k: u32, selection: i32) {
    if let Some(s) = (unsafe { tab.as_ref() }).and_then(|t| t.1.get(k as i32)) {
        s.selection.set(selection);
    }
}

/// The sub-screen `k`'s own call `what` (`EE_SCREENS_SETR_*`, `_SESC_*`, `_SEOB_*`, `_SEAR_*` by `k`) with `a` and `b`
/// and its selection where its class passed it (`editor_tsubs.rs` `op_args`) -> the call's answer (the object
/// screen's; 0 for the others).
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_sub_op(
    tab: *const EeScreensSeTriggerTab,
    k: u32,
    what: u32,
    a: u32,
    b: u32,
) -> u32 {
    let Some(sub) = (unsafe { tab.as_ref() }).and_then(|t| t.1.get(k as i32)) else { return 0 };
    let obj = sub.obj.get();
    if obj == 0 {
        return 0;
    }
    let (a, b) = op_args(k as usize, what, a, b, sub.selection.get());
    unsafe {
        match k as usize {
            SUB_TRIGGER => subs::ee_screens_setr_call(obj as *const _, what, a, b),
            SUB_CONDITION => subs::ee_screens_secd_call(obj as *const _, what, a as i32),
            SUB_EFFECT => subs::ee_screens_sefx_call(obj as *const _, what, a as i32),
            SUB_OBJECT => return subs::ee_screens_seob_call(obj as *const _, what, a, b),
            _ => subs::ee_screens_sear_call(obj as *const _, what, a as i32, b as i32),
        }
    }
    0
}

/// The sub-screen `k`'s slot `slot` (`EE_SCREENS_SETS_SCREEN_*`; `MouseMove` at `x`, `y`), as the classes did it.
///
/// # Safety
/// `tab` comes from `ee_screens_sets_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sets_sub_call(
    tab: *const EeScreensSeTriggerTab,
    k: u32,
    slot: i32,
    x: f32,
    y: f32,
) {
    if let Some(t) = unsafe { tab.as_ref() } {
        unsafe { sub_call(&t.1, k as i32, slot, x, y) };
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
