//! `extern "C"` for the load thread and the text actions (`editor_selt.rs`): the C++ members forward here; the
//! statements go to `ee_screenscpp_seltx` (`Scenario Editor/EESELoadThread.cpp`), the `_R_` ones are answered here.

use core::ffi::{c_long, c_ulong, c_void};

use ee_ui::direct::{self, Cpp};
use ee_ui::formobj::EeUiFormCall;

use crate::screens::editor_selt::*;

/// cbindgen:ignore
mod sys {
    extern "C" {
        pub fn ee_screenscpp_seltx(op: u32, p: *const usize, a: *const i64) -> i64;
    }
}

/// `kFPPrecision53` (`_PC_53`).
const FP_PRECISION53: c_ulong = 0x10000;

/// One statement: the `_R_` ones here, the others the C++'s.
fn statement(op: u32, p: [usize; 3], a: [i64; 4]) -> i64 {
    use ee_base::util::fp;
    match op {
        EE_SCREENS_SELTX_R_FP_IS53 => (fp::ee_util_fp_precision() == FP_PRECISION53) as i64,
        EE_SCREENS_SELTX_R_FP_SET53 => {
            fp::ee_util_fp_set_precision(FP_PRECISION53);
            0
        }
        EE_SCREENS_SELTX_R_FP_ROUNDING => fp::ee_util_fp_rounding() as i64,
        EE_SCREENS_SELTX_R_FP_ROUND0 => {
            fp::ee_util_fp_set_rounding(0);
            0
        }
        EE_SCREENS_SELTX_R_SLEEP => {
            ee_base::plat_core::time::PlatSleep(1);
            0
        }
        EE_SCREENS_SELTX_R_EDIT_TEXT => {
            let edit = direct::form_call(p[0] as Cpp, EeUiFormCall::ControlById, a[0] as c_ulong as isize, 0);
            let text = crate::screens::editor_direct::edit_line(edit as Cpp);
            let n = text.len().min(a[3].max(0) as usize);
            // SAFETY: p2 holds a3 bytes (the statement's contract)
            unsafe { core::ptr::copy_nonoverlapping(text.as_ptr(), p[2] as *mut u8, n) };
            text.len() as i64
        }
        // the text actions' payload (EeScreensSeltInfo, once the actions' members and EESEImportInfo)
        EE_SCREENS_SELTX_EXPORT_FILE | EE_SCREENS_SELTX_IMPORT_FILE => unsafe { info_at(p[0]) }.spec as i64,
        EE_SCREENS_SELTX_IMPORT_VALIDATE => unsafe { info_at(p[0]) }.validate as i64,
        EE_SCREENS_SELTX_INFO_NEW => info_new(p[1], a[0] != 0) as i64,
        EE_SCREENS_SELTX_INFO_DELETE => {
            seia_info_free(p[1] as *mut c_void);
            0
        }
        EE_SCREENS_SELTX_POST_EXPORT => post_text(EE_SCREENS_SEW_TEXT_EXPORT, info_new(p[1], false)),
        EE_SCREENS_SELTX_POST_IMPORT => {
            let info = unsafe { info_at(p[1]) };
            post_text(EE_SCREENS_SEW_TEXT_IMPORT, info_new(info.spec, info.validate))
        }
        EE_SCREENS_SELTX_TEXT_VERSION => EE_SCREENS_SELT_TEXT_VERSION,
        // player p1's mTechnologies->Get(a0)->LoadMedia(p1) / UnloadMedia(p1) (the tree is Rust's: tech_tree objects.rs)
        EE_SCREENS_SELTX_TECH_LOAD_MEDIA | EE_SCREENS_SELTX_TECH_UNLOAD_MEDIA => {
            use crate::tech_tree::objects::{ee_gamecpp_tech_obj_media, get, player_tree};
            let node = get(player_tree(p[1]), a[0] as c_ulong as c_long);
            ee_gamecpp_tech_obj_media(node as *mut c_void, p[1] as *mut c_void, op == EE_SCREENS_SELTX_TECH_LOAD_MEDIA);
            0
        }
        EE_SCREENS_SELTX_WRITE_HEADER => cpp(op, [p[0], p[1], p[2]], [EE_SCREENS_SELT_TEXT_VERSION, a[1], a[2], a[3]]),
        _ => cpp(op, p, a),
    }
}

/// One C++ statement.
fn cpp(op: u32, p: [usize; 3], a: [i64; 4]) -> i64 {
    // SAFETY: the statement's live handles and buffers (editor_selt.rs's contract per op)
    unsafe { sys::ee_screenscpp_seltx(op, p.as_ptr(), a.as_ptr()) }
}

/// The payload behind a text action's or an import question's handle.
///
/// # Safety
/// `info` comes from `info_new` and is live.
unsafe fn info_at(info: usize) -> EeScreensSeltInfo {
    unsafe { *(info as *const EeScreensSeltInfo) }
}

/// A payload with its own copy of the spec `spec` (the original's `FSFileSpec` copy into the action or the info).
fn info_new(spec: usize, validate: bool) -> usize {
    let spec = cpp(EE_SCREENS_SELTX_SPEC_COPY, [0, spec, 0], [0; 4]) as usize;
    Box::into_raw(Box::new(EeScreensSeltInfo { spec, validate })) as usize
}

/// The text action of `kind` on `info` posted to the user interface.
fn post_text(kind: u32, info: usize) -> i64 {
    cpp(EE_SCREENS_SELTX_POST_TEXT_ACTION, [0, info, 0], [kind as i64, 0, 0, 0])
}

/// A payload freed with its spec (the action's destructor, the import question's answer).
pub fn seia_info_free(info: *mut c_void) {
    // SAFETY: `info` comes from `info_new` and is not used after
    let info = unsafe { Box::from_raw(info as *mut EeScreensSeltInfo) };
    cpp(EE_SCREENS_SELTX_SPEC_DELETE, [0, info.spec, 0], [0; 4]);
}

fn host() -> impl FnMut(u32, [usize; 3], [i64; 4]) -> i64 {
    statement
}

/// `EESELoadThread::EESELoadThread(editor, priority)`.
#[no_mangle]
pub extern "C" fn ee_screens_selt_construct(thread: *mut c_void, editor: *mut c_void, priority: c_long) {
    construct(&mut host(), thread as usize, editor as usize, priority as i64);
}

/// `EESELoadThread::~EESELoadThread()`.
#[no_mangle]
pub extern "C" fn ee_screens_selt_destroy(thread: *mut c_void) {
    destroy(&mut host(), thread as usize);
}

/// `EESELoadThread::Main(thread)` (the thread procedure).
#[no_mangle]
pub extern "C" fn ee_screens_selt_main(thread: *mut c_void) -> c_ulong {
    main(&mut host(), thread as usize)
}

/// `EESELoadThread::HandleMessage(message, p1, p2)`.
#[no_mangle]
pub extern "C" fn ee_screens_selt_message(thread: *mut c_void, message: c_ulong, p1: *mut c_void, p2: *mut c_void) {
    handle_message(&mut host(), thread as usize, message, p1 as usize, p2 as usize);
}

/// The text actions' `Virtual4` (`action` its [`EeScreensSeltInfo`]): the export (`import` false) or the import.
#[no_mangle]
pub extern "C" fn ee_screens_seia_run(action: *mut c_void, import_text: bool) {
    if import_text {
        import(&mut host(), action as usize);
    } else {
        export(&mut host(), action as usize);
    }
}

/// The questions' answers: `EESEExportTextAnswer(spec)` (`import` false) or `EESEImportTextAnswer(info)`.
#[no_mangle]
pub extern "C" fn ee_screens_seia_answer(p: *mut c_void, import_text: bool) {
    if import_text {
        import_answer(&mut host(), p as usize);
    } else {
        export_answer(&mut host(), p as usize);
    }
}

/// The buttons' events' `ProcessEvent(form)` (the `EESEERust` kinds `EE_SCREENS_SEE_TEXT_*`): the export (`import`
/// false) or the import.
#[no_mangle]
pub extern "C" fn ee_screens_seia_event(form: *mut c_void, import_text: bool) -> c_long {
    (if import_text { import_event(&mut host(), form as usize) } else { export_event(&mut host(), form as usize) })
        as c_long
}
