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

/// `EEUICChatLine`'s constructor part.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_init(rec: *mut EeScreensChatLineFields, opacity: f32) {
    chat_line::init(&Rec::new(rec), opacity)
}

/// `EEUICChatLine::CopyData`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_copy_data(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::copy_data(&Rec::new(rec), &mut Cpp(ctl))
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
    chat_line::display(&mut Cpp(ctl), color, name, message, time)
}

/// `EEUICChatLine::DrawPolygons`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_draw(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::draw(&Rec::new(rec), &mut Cpp(ctl))
}

/// `EEUICChatLine::DrawTextA`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_chat_line_draw_text(rec: *mut EeScreensChatLineFields, ctl: *mut c_void) -> i64 {
    chat_line::draw_text(&Rec::new(rec), &mut Cpp(ctl))
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
