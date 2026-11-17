//! `extern "C"` for the scenario editor's player tab whole in Rust (`editor_players.rs`): the object
//! `EESEPlayerScreen` holds, made, called and freed by it. The host is `ee_screenscpp_sepl`
//! (`Scenario Editor/EESERBUnitTypes.cpp`).

use core::ffi::c_void;

use crate::screens::editor_players::{Players, SeplHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            crate::screens::sgvw::editor(op, i, f, |i, f| {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            })
        }
    }
}

/// The player tab (opaque to C++).
pub struct EeScreensSePlayers(Players);

/// `Leave`, `Load`, `Enter`, `Store`, `Refresh`, `SetPlayerCount(a)`, `SelectPage(a)`, `FillTechLists`,
/// `RebuildTechLists`, `CopyFirstPlayerToAll`, `GetPlayerCount` (answered), `MarkPlayerChanged(a, b)`.
pub const EE_SCREENS_SEPL_LEAVE: u32 = 1;
pub const EE_SCREENS_SEPL_LOAD: u32 = 2;
pub const EE_SCREENS_SEPL_ENTER: u32 = 3;
pub const EE_SCREENS_SEPL_STORE: u32 = 4;
pub const EE_SCREENS_SEPL_REFRESH: u32 = 5;
pub const EE_SCREENS_SEPL_SET_PLAYER_COUNT: u32 = 6;
pub const EE_SCREENS_SEPL_SELECT_PAGE: u32 = 7;
pub const EE_SCREENS_SEPL_FILL_TECH_LISTS: u32 = 8;
pub const EE_SCREENS_SEPL_REBUILD_TECH_LISTS: u32 = 9;
pub const EE_SCREENS_SEPL_COPY_FIRST_TO_ALL: u32 = 10;
pub const EE_SCREENS_SEPL_PLAYER_COUNT: u32 = 11;
pub const EE_SCREENS_SEPL_MARK_CHANGED: u32 = 12;

/// `EESEPlayerScreen`'s constructor: every control made on `form` from the control index `*index` (advanced);
/// `player_selection` is the editor's player combo box. Freed by `ee_screens_sepl_free`.
///
/// # Safety
/// `index` points at the form's next control index.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sepl_new(
    form: *mut c_void,
    player_selection: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSePlayers {
    let mut next = unsafe { *index };
    let players = Players::new(&mut Host, form as usize, player_selection as usize, &mut next);
    unsafe { *index = next };
    Box::into_raw(Box::new(EeScreensSePlayers(players)))
}

/// The destructor: every control deleted (the original's order), the object freed.
///
/// # Safety
/// `screen` comes from `ee_screens_sepl_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sepl_free(screen: *mut EeScreensSePlayers) {
    if screen.is_null() {
        return;
    }
    let b = unsafe { Box::from_raw(screen) };
    b.0.destroy(&mut Host);
}

/// One of the tab's members (`EE_SCREENS_SEPL_LEAVE` ...) with its arguments; `GetPlayerCount`'s answer.
///
/// # Safety
/// `screen` comes from `ee_screens_sepl_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sepl_call(screen: *const EeScreensSePlayers, what: u32, a: u32, b: u32) -> u32 {
    if screen.is_null() {
        return 0;
    }
    // shared: a host call can come back into the tab (a page tab's Check runs SelectPage)
    let p = unsafe { &(*screen).0 };
    let h = &mut Host;
    match what {
        EE_SCREENS_SEPL_LEAVE => p.leave(h),
        EE_SCREENS_SEPL_LOAD => p.load(h),
        EE_SCREENS_SEPL_ENTER => p.enter(h),
        EE_SCREENS_SEPL_STORE => p.store(h),
        EE_SCREENS_SEPL_REFRESH => p.refresh(h),
        EE_SCREENS_SEPL_SET_PLAYER_COUNT => p.set_player_count(h, a),
        EE_SCREENS_SEPL_SELECT_PAGE => p.select_page(h, a),
        EE_SCREENS_SEPL_FILL_TECH_LISTS => p.fill_tech_lists(h),
        EE_SCREENS_SEPL_REBUILD_TECH_LISTS => p.rebuild_tech_lists(h),
        EE_SCREENS_SEPL_COPY_FIRST_TO_ALL => p.copy_first_to_all(h),
        EE_SCREENS_SEPL_PLAYER_COUNT => return p.player_count(h),
        EE_SCREENS_SEPL_MARK_CHANGED => p.mark_changed(a, b != 0),
        _ => {}
    }
    0
}

/// A player row's control (`EE_SCREENS_SEPL_ROW_*`), or the player combo box for row `u32::MAX`; NULL for none.
///
/// # Safety
/// `screen` comes from `ee_screens_sepl_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sepl_control(
    screen: *const EeScreensSePlayers,
    row: u32,
    which: u32,
) -> *mut c_void {
    if screen.is_null() {
        return core::ptr::null_mut();
    }
    let p = unsafe { &(*screen).0 };
    let c = if row == u32::MAX { p.player_selection } else { p.row_control(row, which) };
    c as *mut c_void
}
