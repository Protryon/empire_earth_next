//! `extern "C"` for the generic C++ shells' kinds (`shells.rs`): made, asked and freed by the shells
//! (`ee/EEUIShell.h`); the host is `ee_screenscpp_shell` (`User Interface Thread/EEUIShell.cpp`). The shells by
//! control ([`with`]) for the Rust that holds a shell's control.

use core::ffi::{c_char, c_void};
use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::screens::shells::{make_kind, Cpp, Ctx, EeScreensShell, ShellHost, EE_SCREENS_SH_ID};

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_shell(ctl: *mut c_void, op: u32, a: i64, b: i64, f: *mut f32) -> i64;
        pub fn ee_screenscpp_shell_base(ctl: *mut c_void, v: u32, a: i64, b: i64, f: *mut f32) -> i64;
        pub fn ee_screenscpp_shell_form_base(form: *mut c_void, v: u32, a: i64, b: i64, f: *mut f32) -> i64;
        pub fn ee_screenscpp_shell_form_self(form: *mut c_void, v: u32) -> i64;
    }
}

struct CppHost;

impl ShellHost for CppHost {
    fn call(&self, ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        use crate::screens::shells_editor::{EE_SCREENS_SH_MAP_KIND, EE_SCREENS_SH_TERRAIN_CHECK};
        let form = || ee_ui::direct::with_fields(ctl, |r| r.mForm).unwrap_or(core::ptr::null_mut());
        match op {
            // the Rust that answers (no C++ hop)
            // (`c_ulong` is `u32` on Windows, `u64` on LP64)
            #[allow(clippy::unnecessary_cast)]
            EE_SCREENS_SH_ID => ee_ui::direct::with_fields(ctl, |r| i64::from(r.mId as u32)).unwrap_or(0),
            // SAFETY: the shell's form
            EE_SCREENS_SH_MAP_KIND => unsafe {
                crate::screens::editor_map_ffi::ee_screens_semp_kind(form(), a as u32);
                0
            },
            // SAFETY: the shell's form
            EE_SCREENS_SH_TERRAIN_CHECK => unsafe {
                crate::screens::editor_terrain_ffi::ee_screens_setn_check(form(), a as u32);
                0
            },
            crate::screens::shells_tech::EE_SCREENS_SH_KIND_OP => {
                op_of(a as Cpp, (b >> 32) as u32, b & 0xffff_ffff, 0).unwrap_or(0)
            }
            // SAFETY: the shell's live control; the host's float buffer is four floats
            // the cases that are Rust's over the C++ shims (shell_calls.rs), else the C++ host
            _ => crate::screens::shell_calls::call(&mut crate::screens::game_cpp::Ffi, ctl as usize, op, a, b, f)
                .unwrap_or_else(|| unsafe { cpp_shell(ctl, op, a, b, f) }),
        }
    }
}

/// The C++ host: a base class's virtual (`EE_SCREENS_SH_BASE + v`, a form's `EE_SCREENS_SH_FORM_BASE + v`), a form's
/// own virtual (`EE_SCREENS_SH_FORM_SELF`), else `ee_screenscpp_shell`'s cases.
///
/// # Safety
/// `ctl` is the shell's live control (a form shell's `UIForm*`).
unsafe fn cpp_shell(ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
    use crate::screens::shells::{EE_SCREENS_SH_BASE, EE_SCREENS_SH_FORM_BASE, EE_SCREENS_SH_FORM_SELF};
    // SAFETY: the contract
    unsafe {
        if op >= EE_SCREENS_SH_FORM_BASE {
            sys::ee_screenscpp_shell_form_base(ctl, op - EE_SCREENS_SH_FORM_BASE, a, b, f.as_mut_ptr())
        } else if op == EE_SCREENS_SH_FORM_SELF {
            sys::ee_screenscpp_shell_form_self(ctl, a as u32)
        } else if op >= EE_SCREENS_SH_BASE {
            sys::ee_screenscpp_shell_base(ctl, op - EE_SCREENS_SH_BASE, a, b, f.as_mut_ptr())
        } else {
            sys::ee_screenscpp_shell(ctl, op, a, b, f.as_mut_ptr())
        }
    }
}

/// The live shells by control (the shells' Rust for a Rust caller that holds the control).
static SHELLS: Mutex<BTreeMap<usize, usize>> = Mutex::new(BTreeMap::new());

fn shells() -> std::sync::MutexGuard<'static, BTreeMap<usize, usize>> {
    SHELLS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The kind's op `op` on the shell control `ctl` (`None` when `ctl` is not a shell). No lock is held across the call.
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) fn op_of(ctl: Cpp, op: u32, a: i64, b: i64) -> Option<i64> {
    let s = shells().get(&(ctl as usize)).copied()? as *const EeScreensShell;
    let mut f = [0.0f32; 4];
    // SAFETY: a registered shell lives until its free
    let s = unsafe { &*s };
    Some(s.kind.op(&Ctx { ctl: s.ctl, host: &CppHost }, op, a, b, &mut f))
}

/// A shell's constructor: the kind `kind` of the control `ctl`, made from `args[0..n]`; NULL for an unknown kind.
///
/// # Safety
/// `args` holds `n` words (or is NULL with `n` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_new(
    kind: u32,
    ctl: *mut c_void,
    args: *const i64,
    n: u32,
) -> *mut EeScreensShell {
    let args = if args.is_null() || n == 0 {
        &[][..]
    } else {
        // SAFETY: the contract
        unsafe { core::slice::from_raw_parts(args, n as usize) }
    };
    let Some(k) = make_kind(kind, args) else {
        return core::ptr::null_mut();
    };
    let s = Box::into_raw(Box::new(EeScreensShell { ctl, kind: k }));
    shells().insert(ctl as usize, s as usize);
    s
}

/// A shell's `Make`: the kind made (`ee_screens_shell_new`), stored at `*state` with its override mask at `*over`,
/// then its constructor body run (`ee_screens_shell_init`: the body may reach the shell through its control, so the
/// stores come first).
///
/// # Safety
/// `state` and `over` are the shell's members; `args` holds `n` words (or is NULL with `n` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_make(
    state: *mut *mut EeScreensShell,
    over: *mut u64,
    kind: u32,
    ctl: *mut c_void,
    args: *const i64,
    n: u32,
) {
    // SAFETY: the contract
    unsafe {
        let s = ee_screens_shell_new(kind, ctl, args, n);
        *state = s;
        *over = ee_screens_shell_overrides(s);
        ee_screens_shell_init(s);
    }
}

/// `EEUIShell::Op`: the kind's op on the shell whose control (a form shell's `UIForm*`) is at `ctl`, 0 when none is.
/// `f` is NULL or four floats.
///
/// # Safety
/// `f` is NULL or four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_op_at(ctl: *const c_void, op: u32, a: i64, b: i64, f: *mut f32) -> i64 {
    let Some(s) = shells().get(&(ctl as usize)).copied() else {
        return 0;
    };
    // SAFETY: a registered shell lives until its free; the contract
    unsafe { ee_screens_shell_op(s as *const EeScreensShell, op, a, b, f) }
}

/// The kind's override mask (`EE_SCREENS_SV_*` bits).
///
/// # Safety
/// `s` is NULL or a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_overrides(s: *const EeScreensShell) -> u64 {
    // SAFETY: the contract
    unsafe { s.as_ref() }.map_or(0, |s| s.kind.overrides())
}

/// The kind's constructor body.
///
/// # Safety
/// `s` is NULL or a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_init(s: *const EeScreensShell) {
    // SAFETY: the contract
    if let Some(s) = unsafe { s.as_ref() } {
        s.kind.init(&Ctx { ctl: s.ctl, host: &CppHost });
    }
}

/// The kind's override of the virtual `v` (`EE_SCREENS_SV_*`); `f` the virtual's floats (NULL: none).
///
/// # Safety
/// `s` is NULL or a live shell; `f` is NULL or four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_virtual(
    s: *const EeScreensShell,
    v: u32,
    a: i64,
    b: i64,
    f: *mut f32,
) -> i64 {
    // SAFETY: the contract
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    let mut buf = [0.0f32; 4];
    if !f.is_null() {
        // SAFETY: four floats
        buf.copy_from_slice(unsafe { core::slice::from_raw_parts(f, 4) });
    }
    let r = s.kind.virt(&Ctx { ctl: s.ctl, host: &CppHost }, v, a, b, &mut buf);
    if !f.is_null() {
        // SAFETY: four floats
        unsafe { core::slice::from_raw_parts_mut(f, 4) }.copy_from_slice(&buf);
    }
    r
}

/// The kind's own method `op` (`EE_SCREENS_SKOP_*`).
///
/// # Safety
/// `s` is NULL or a live shell; `f` is NULL or four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_op(s: *const EeScreensShell, op: u32, a: i64, b: i64, f: *mut f32) -> i64 {
    // SAFETY: the contract
    let Some(s) = (unsafe { s.as_ref() }) else {
        return 0;
    };
    let mut buf = [0.0f32; 4];
    if !f.is_null() {
        // SAFETY: four floats
        buf.copy_from_slice(unsafe { core::slice::from_raw_parts(f, 4) });
    }
    let r = s.kind.op(&Ctx { ctl: s.ctl, host: &CppHost }, op, a, b, &mut buf);
    if !f.is_null() {
        // SAFETY: four floats
        unsafe { core::slice::from_raw_parts_mut(f, 4) }.copy_from_slice(&buf);
    }
    r
}

/// The deleted class's name (NUL-terminated, static).
///
/// # Safety
/// `s` is NULL or a live shell.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_name(s: *const EeScreensShell) -> *const c_char {
    // SAFETY: the contract
    unsafe { s.as_ref() }.map_or(c"EEUIShell".as_ptr(), |s| s.kind.name().as_ptr())
}

/// The shell's destructor.
///
/// # Safety
/// `s` is NULL or a shell made by `ee_screens_shell_new`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_shell_free(s: *mut EeScreensShell) {
    if s.is_null() {
        return;
    }
    {
        let mut m = shells();
        // SAFETY: the contract
        let ctl = unsafe { (*s).ctl } as usize;
        if m.get(&ctl) == Some(&(s as usize)) {
            m.remove(&ctl);
        }
    }
    // SAFETY: made by Box::into_raw
    let s = unsafe { Box::from_raw(s) };
    s.kind.free(&Ctx { ctl: s.ctl, host: &CppHost });
}
