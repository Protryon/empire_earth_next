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
        // SAFETY: the statement's live handles and buffers (editor_selt.rs's contract per op)
        _ => unsafe { sys::ee_screenscpp_seltx(op, p.as_ptr(), a.as_ptr()) },
    }
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

/// The text actions: `EESEIAExport::Virtual4` (`import` false) or `EESEIAImport::Virtual4`.
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

/// The buttons' events: `EESEIEExport::ProcessEvent(form)` (`import` false) or `EESEIEImport::ProcessEvent`.
#[no_mangle]
pub extern "C" fn ee_screens_seia_event(form: *mut c_void, import_text: bool) -> c_long {
    (if import_text { import_event(&mut host(), form as usize) } else { export_event(&mut host(), form as usize) })
        as c_long
}
