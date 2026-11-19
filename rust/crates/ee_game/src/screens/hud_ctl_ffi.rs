//! `extern "C"` for the HUD controls on their records (`hud_ctl.rs`): each C++ member forwards here with its record
//! (the class's protected base) and the control (the host's object, `ee_screenscpp_hud_ctl`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::screens::chat_line::{self, EeScreensChatLineFields};
use crate::screens::command_units::{self, EeScreensCommandUnitsFields, *};
use crate::screens::hud_ctl::{Cpp, Rec};
use crate::screens::ping::{self, EeScreensPingFields};
use crate::screens::select_units::{self, EeScreensSelectUnitsFields, *};

/// `EEUICPing`'s constructor part: the bars placed from the location (x, y).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ping_init(rec: *mut EeScreensPingFields, ctl: *mut c_void, x: f32, y: f32) {
    if ee_render::ge::list2d::ready() {
        crate::screens::hud_draw::ping_init(ctl);
    }
    ping::init(&Rec::new(rec), &mut PingCpp(ctl), x, y)
}

/// The ping meter's host: its bars on the draw list (`hud_draw.rs`), the rest the C++'s.
struct PingCpp(*mut c_void);

impl crate::screens::hud_ctl::HudHost for PingCpp {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        if let Some(r) = crate::screens::hud_draw::ping(self.0, op, a, f) {
            return r;
        }
        crate::screens::hud_ctl::HudHost::op(&mut Cpp(self.0), op, a, b, f)
    }
}

/// `EEUICPing::DrawPolygons`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ping_draw(rec: *mut EeScreensPingFields, ctl: *mut c_void) -> i64 {
    ping::draw(&Rec::new(rec), &mut PingCpp(ctl))
}

/// `EEUICPing::SetPing`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ping_set(rec: *mut EeScreensPingFields, ctl: *mut c_void, value: f32) {
    ping::set_ping(&Rec::new(rec), &mut PingCpp(ctl), value)
}

/// The chat lines' record offset in `EEUICChatLine` (one class, one offset).
static CHAT_OFFSET: core::sync::atomic::AtomicUsize = core::sync::atomic::AtomicUsize::new(usize::MAX);

/// The record of the chat line `ctl` (an `EEUICChatLine*`); None before one registered or for NULL.
pub(crate) fn chat_record(ctl: *mut c_void) -> Option<*mut EeScreensChatLineFields> {
    let offset = CHAT_OFFSET.load(core::sync::atomic::Ordering::Relaxed);
    (!ctl.is_null() && offset != usize::MAX).then(|| (ctl as usize + offset) as *mut EeScreensChatLineFields)
}

/// A chat line's timer (a `TMPoll` is its `EeTimePoll` record) and its texts' Rust objects (a `GEText`'s only
/// member), in `EE_SCREENS_CHL_NAME`.. order; and the texts' C++ shells.
struct ChatParts {
    timer: *mut ee_base::time::EeTimePoll,
    texts: [*mut ee_render::ge::smallobj::EeGeText; 4],
    shells: [*mut c_void; 4],
}

fn chat_parts(ctl: *mut c_void) -> Option<ChatParts> {
    let r = chat_record(ctl)?;
    // SAFETY: the live line's record; its texts are live GETexts (one pointer: their Rust object)
    unsafe {
        let shells = [(*r).mName, (*r).mMessage, (*r).mDrawnName, (*r).mDrawnMessage];
        if shells.iter().any(|p| p.is_null()) || (*r).mTimer.is_null() {
            return None;
        }
        let texts = shells.map(|p| *(p as *const *mut ee_render::ge::smallobj::EeGeText));
        Some(ChatParts { timer: (*r).mTimer.cast(), texts, shells })
    }
}

/// The chat line's host: its timer's and texts' ops on their Rust objects, the form's clipped draw on the form's;
/// the cut (the toolkit's C++ `UIGetStringCutOffIndex`) and any control's ops go on to [`Cpp`].
pub(crate) struct ChatCpp(pub(crate) *mut c_void);

impl crate::screens::hud_ctl::HudHost for ChatCpp {
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64 {
        use crate::screens::chat_line::*;
        use crate::screens::hud_direct::CppWide;
        use ee_base::time::*;
        use ee_render::ge::smallobj::*;
        let ours = (EE_SCREENS_CHL_POLL..=EE_SCREENS_CHL_DRAW).contains(&op) && op != EE_SCREENS_CHL_CUT;
        let Some(p) = chat_parts(self.0).filter(|_| ours) else {
            return crate::screens::hud_ctl::HudHost::op(&mut Cpp(self.0), op, a, b, f);
        };
        let which = a.clamp(0, 3) as usize;
        // SAFETY: the line's live timer and texts; a and b name the caller's live strings and colour
        unsafe {
            match op {
                EE_SCREENS_CHL_POLL => return ee_time_poll_poll(p.timer) as i64,
                EE_SCREENS_CHL_TIME_LEFT => return ee_time_poll_time_left(p.timer) as i64,
                EE_SCREENS_CHL_STOP => ee_time_poll_stop(p.timer),
                EE_SCREENS_CHL_START => {
                    ee_time_poll_set_time_delay(p.timer, a as u32);
                    ee_time_poll_start(p.timer);
                }
                EE_SCREENS_CHL_COPY_TEXTS => {
                    ee_ge_text_assign(p.texts[2], p.texts[0]);
                    ee_ge_text_assign(p.texts[3], p.texts[1]);
                }
                EE_SCREENS_CHL_TEXT => {
                    let t = p.texts[which];
                    let e = *ee_ge_text_extent_of(t);
                    let at = *ee_ge_text_location(t);
                    f[..6].copy_from_slice(&[e.left, e.top, e.right, e.bottom, at.x, at.y]);
                    return (*(ee_ge_text_string(t) as *const CppWide)).s.length as i64;
                }
                EE_SCREENS_CHL_LOCATE => {
                    let at = ee_ge_text_location(p.texts[which]);
                    (*at).x = f[0];
                    (*at).y = f[1];
                }
                EE_SCREENS_CHL_SET_NAME => {
                    ee_ge_text_set_text(p.texts[0], b as *const c_void);
                    let rgb = core::slice::from_raw_parts(a as *const f32, 3);
                    core::slice::from_raw_parts_mut(ee_ge_text_color(p.texts[0], true), 3).copy_from_slice(rgb);
                }
                EE_SCREENS_CHL_SET_MESSAGE => {
                    if b >= 0 {
                        // the message's first b characters (UWideString::CopyString), set as the text
                        let mut cut: CppWide = core::mem::zeroed();
                        ee_base::ustring::ee_uwstring_init(&mut cut.s);
                        let m = a as *const CppWide;
                        ee_base::ustring::ee_uwstring_copy_text(&mut cut.s, (*m).s.string, b as core::ffi::c_ulong);
                        ee_ge_text_set_text(p.texts[1], &cut as *const CppWide as *const c_void);
                        ee_base::ustring::ee_uwstring_free(&mut cut.s);
                    } else {
                        ee_ge_text_set_text(p.texts[1], a as *const c_void);
                    }
                }
                EE_SCREENS_CHL_LENGTH => return (*(a as *const CppWide)).s.length as i64,
                _ => {
                    // EE_SCREENS_CHL_DRAW: the form's clipped draw of the text in f's rectangle (a GERectangle's
                    // order for the form's call: bottom, left, right, top)
                    let form = ee_ui::direct::with_fields(self.0, |r| r.mForm).unwrap_or(core::ptr::null_mut());
                    let mut r = [f[3], f[0], f[2], f[1]];
                    return ee_ui::direct::form_call_f(
                        form,
                        ee_ui::formobj::EeUiFormCall::DrawClippedText,
                        p.shells[which] as isize,
                        self.0 as isize,
                        &mut r,
                    ) as core::ffi::c_long as i64;
                }
            }
        }
        0
    }
}

/// `EEUICChatLine`'s constructor part (`ctl` the line: its record's place in the class is registered).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_init(rec: *mut EeScreensChatLineFields, ctl: *mut c_void, opacity: f32) {
    CHAT_OFFSET.store(rec as usize - ctl as usize, core::sync::atomic::Ordering::Relaxed);
    chat_line::init(&Rec::new(rec), opacity)
}

/// `EEUICChatLine::CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_copy_data(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::copy_data(&Rec::new(rec), &mut ChatCpp(ctl))
}

/// `EEUICChatLine::Display(colour, name, message, time)` (the colour a `GEColor*`, the texts `UWideString*`s).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_display(
    ctl: *mut c_void,
    color: *const c_void,
    name: *const c_void,
    message: *const c_void,
    time: u32,
) -> u32 {
    chat_line::display(&mut ChatCpp(ctl), color, name, message, time)
}

/// The original's `EEUICChatLine::Copy(from)` on the line `ctl` (the main game form scrolls its lines up): the
/// other line's name, its colour, its message and its time left shown here; 0 for a missing line.
pub(crate) fn chat_line_copy(ctl: *mut c_void, from: *mut c_void) -> u32 {
    let (Some(_), Some(p)) = (chat_record(ctl), chat_parts(from)) else { return 0 };
    // SAFETY: the other line's live timer and texts
    unsafe {
        use ee_render::ge::smallobj::*;
        let time = ee_base::time::ee_time_poll_time_left(p.timer);
        let color = ee_ge_text_color(p.texts[0], true) as *const c_void;
        chat_line::display(&mut ChatCpp(ctl), color, ee_ge_text_string(p.texts[0]), ee_ge_text_string(p.texts[1]), time)
    }
}

/// `EEUICChatLine::DrawPolygons`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_draw(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::draw(&Rec::new(rec), &mut ChatCpp(ctl))
}

/// `EEUICChatLine::DrawTextA`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_draw_text(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::draw_text(&Rec::new(rec), &mut ChatCpp(ctl))
}

/// `EEUICSelectUnits`' members: `EE_SCREENS_SU_DO_*` with the mouse location (x, y) where one is taken.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_select_units(
    rec: *mut EeScreensSelectUnitsFields,
    ctl: *mut c_void,
    which: u32,
    x: f32,
    y: f32,
) -> i64 {
    let (r, h) = (&Rec::new(rec), &mut Cpp(ctl));
    match which {
        EE_SCREENS_SU_DO_COPY_DATA => select_units::copy_data(r, h),
        EE_SCREENS_SU_DO_DRAW => select_units::draw(r, h),
        EE_SCREENS_SU_DO_DOUBLE_CLICK => select_units::double_click(r, h),
        EE_SCREENS_SU_DO_MOUSE_L_UP => select_units::mouse_l_up(r, h),
        EE_SCREENS_SU_DO_MOUSE_MOVE => select_units::mouse_move(r, h, x, y),
        EE_SCREENS_SU_DO_START => {
            select_units::start(r, h, x, y);
            0
        }
        EE_SCREENS_SU_DO_R_DOWN => select_units::r_down(r, h),
        EE_SCREENS_SU_DO_R_UP => select_units::r_up(r, h),
        _ => 0,
    }
}

/// `EEUICCommandUnits`' members: `EE_SCREENS_CU_DO_*` with the mouse location (x, y) where one is taken.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_command_units(
    rec: *mut EeScreensCommandUnitsFields,
    ctl: *mut c_void,
    which: u32,
    x: f32,
    y: f32,
) -> i64 {
    let (r, h) = (&Rec::new(rec), &mut Cpp(ctl));
    match which {
        EE_SCREENS_CU_DO_INIT => {
            command_units::init(r);
            0
        }
        EE_SCREENS_CU_DO_COPY_DATA => command_units::copy_data(r, h),
        EE_SCREENS_CU_DO_DRAW => command_units::draw(r, h),
        EE_SCREENS_CU_DO_MOUSE_MOVE => command_units::mouse_move(r, h, x, y),
        EE_SCREENS_CU_DO_MOUSE_R_UP => command_units::mouse_r_up(r, h),
        EE_SCREENS_CU_DO_START => {
            command_units::start(r, h, x, y);
            0
        }
        _ => 0,
    }
}
