//! The game's message box (`EEUIFError`, the form `EEUIManager::Display*` news for every message, question and
//! password prompt): what each kind shows (its buttons' strings and callbacks, the hidden controls, the message, the
//! edit box) and, when it opens, where its controls go for that kind, scaled from a 1024x768 design. Over a host
//! ([`DialogHost`]) whose calls are each one toolkit call on the box's controls (`ee_screenscpp_dialog`,
//! `User Interface Thread/EEUIFError.cpp`), and the box's construction ([`construct`]: its controls' order, sizes,
//! colours and layout); the C++ keeps the form, the `new` of each control, the buttons' drawing and the posted
//! callback on close.

/// The kinds (and the modes `Open` lays out): OK (shown or not), Yes/No, Yes/No/Cancel, a text or password with
/// OK, Cancel only.
pub const EE_SCREENS_DIALOG_MESSAGE: u32 = 0;
pub const EE_SCREENS_DIALOG_QUESTION: u32 = 1;
pub const EE_SCREENS_DIALOG_YES_NO_CANCEL: u32 = 2;
pub const EE_SCREENS_DIALOG_PASSWORD: u32 = 3;
pub const EE_SCREENS_DIALOG_CANCEL: u32 = 4;

// ---- the host's ops (ee_screenscpp_dialog on the box). Controls: 0 the message label, 1 OK/Yes, 2 No, 3 Cancel,
// 4 the edit box.

/// (button, string id << 2 | callback) the button's text (the string, Tahoma 12), its callback (0 none, 1 the first
/// with the data, 2 the second with the data, 3 the first with the edit box's text; Cancel's also takes the form it
/// closes), shown, black and transparent.
pub const EE_SCREENS_DLG_BUTTON: u32 = 0;
/// (control, mode) `SetDisplayMode`.
pub const EE_SCREENS_DLG_SHOW: u32 = 1;
/// (alignment) the label's alignment, then its text (the message).
pub const EE_SCREENS_DLG_MESSAGE: u32 = 2;
/// (width, height) the label's rectangle at the origin.
pub const EE_SCREENS_DLG_MESSAGE_SIZE: u32 = 3;
/// The form's tabbing on, its tab stop the edit box.
pub const EE_SCREENS_DLG_TABBING: u32 = 4;
/// (flag) the edit box a password box.
pub const EE_SCREENS_DLG_PASSWORD: u32 = 5;
/// (mode) the mode `Open` lays out.
pub const EE_SCREENS_DLG_MODE: u32 = 6;
/// `gUserInterface->SwitchToForm(the box)`.
pub const EE_SCREENS_DLG_SWITCH: u32 = 7;
/// (left | right << 16, top | bottom << 16) the layout (left and right in 1024ths of the window, top and bottom in
/// 768ths, each a relative side) into the rectangle register.
pub const EE_SCREENS_DLG_CALC: u32 = 8;
/// (control) `SetRectangle(the register)`.
pub const EE_SCREENS_DLG_PLACE: u32 = 9;
/// (0) the font register: the closest font of index 0x1c to Tahoma 16 scaled to the window's height; (button) the
/// button's text in it; (4) the register's font released.
pub const EE_SCREENS_DLG_FONT: u32 = 10;
/// (side: 0 left, 1 top, 2 right, 3 bottom) -> the register's side (f32 bits).
pub const EE_SCREENS_DLG_RECT: u32 = 11;
/// (side, f32 bits) the register's side.
pub const EE_SCREENS_DLG_SET_RECT: u32 = 12;
/// -> Tahoma 12's height.
pub const EE_SCREENS_DLG_LINE_HEIGHT: u32 = 13;
/// -> the message label's line count.
pub const EE_SCREENS_DLG_LINES: u32 = 14;

// The constructor's ops (`EEUIFError::EEUIFError`). Their sizes, colours and layout go first into the host's float
// register (f0..f8).
/// (index, f32 bits) the float register's entry.
pub const EE_SCREENS_DLG_FLOAT: u32 = 15;
/// (control) creates it: the message label (f0 x f1, the font register, index 1), OK (id 1), No (2), Cancel (3), the
/// edit box (f0 x f1, the font register, 0x32 characters, id 4); each 50x20 button in its own class.
pub const EE_SCREENS_DLG_NEW: u32 = 16;
/// The message label's scroll bar: background colour f0..f2, opacity f3.
pub const EE_SCREENS_DLG_SCROLL_BAR: u32 = 17;
/// (alignment) the message label's alignment.
pub const EE_SCREENS_DLG_ALIGN: u32 = 18;
/// (control: the label or the edit box) its text transparent, coloured f0..f2.
pub const EE_SCREENS_DLG_TEXT_COLOR: u32 = 19;
/// (control) `AddControl(control, 0)`.
pub const EE_SCREENS_DLG_ADD: u32 = 20;
/// The edit box's selection background f0..f2, background f3..f5, cursor f6..f8.
pub const EE_SCREENS_DLG_EDIT_COLORS: u32 = 21;
/// The form's entity group 0 enabled.
pub const EE_SCREENS_DLG_ENABLE: u32 = 22;
/// The form's layout (relative sides f0 left, f1 right, f2 top, f3 bottom) and its gold framed background (colour
/// f4..f6) in the manager's UI texture depth.
pub const EE_SCREENS_DLG_BACKGROUND: u32 = 23;

const DLG_LABEL: u32 = 0;
const DLG_OK: u32 = 1;
const DLG_NO: u32 = 2;
const DLG_CANCEL: u32 = 3;
const DLG_EDIT: u32 = 4;
const DLG_HIDDEN: u32 = 0;
const DLG_SHOWN: u32 = 1;
const DLG_FONT_NEW: u32 = 0;
const DLG_FONT_DROP: u32 = 4;
const DLG_CALLBACK_NONE: u32 = 0;
const DLG_CALLBACK_FIRST: u32 = 1;
const DLG_CALLBACK_SECOND: u32 = 2;
const DLG_CALLBACK_EDIT_TEXT: u32 = 3;

/// The strings: IDS_LOBBY_OK, IDS_LOBBY_CANCEL, IDS_YES, IDS_NO.
const DLG_IDS_LOBBY_OK: u32 = 30228;
const DLG_IDS_LOBBY_CANCEL: u32 = 30225;
const DLG_IDS_YES: u32 = 30288;
const DLG_IDS_NO: u32 = 30289;

/// The box's calls.
pub trait DialogHost {
    /// `EE_SCREENS_DLG_*` with up to two arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32;
}

/// `EEUIFError::Display*`: the kind's buttons, message and controls, its mode, then the switch to the box. `flag`
/// is the message's OK shown, the prompt's password box.
pub fn show<H: DialogHost + ?Sized>(h: &mut H, kind: u32, alignment: u32, flag: bool) {
    match kind {
        EE_SCREENS_DIALOG_PASSWORD => {
            h.call(EE_SCREENS_DLG_BUTTON, DLG_OK, DLG_IDS_LOBBY_OK << 2 | DLG_CALLBACK_EDIT_TEXT);
            h.call(EE_SCREENS_DLG_MESSAGE, alignment, 0);
            h.call(EE_SCREENS_DLG_MESSAGE_SIZE, 280, 50);
            h.call(EE_SCREENS_DLG_SHOW, DLG_NO, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_CANCEL, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_EDIT, DLG_SHOWN);
            h.call(EE_SCREENS_DLG_TABBING, 0, 0);
            h.call(EE_SCREENS_DLG_PASSWORD, flag as u32, 0);
        }
        EE_SCREENS_DIALOG_CANCEL => {
            h.call(EE_SCREENS_DLG_BUTTON, DLG_CANCEL, DLG_IDS_LOBBY_CANCEL << 2 | DLG_CALLBACK_FIRST);
            h.call(EE_SCREENS_DLG_MESSAGE, alignment, 0);
            h.call(EE_SCREENS_DLG_SHOW, DLG_NO, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_OK, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_EDIT, DLG_HIDDEN);
        }
        EE_SCREENS_DIALOG_MESSAGE => {
            h.call(EE_SCREENS_DLG_BUTTON, DLG_OK, DLG_IDS_LOBBY_OK << 2 | DLG_CALLBACK_FIRST);
            h.call(EE_SCREENS_DLG_MESSAGE, alignment, 0);
            h.call(EE_SCREENS_DLG_SHOW, DLG_OK, if flag { DLG_SHOWN } else { DLG_HIDDEN });
            h.call(EE_SCREENS_DLG_SHOW, DLG_NO, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_CANCEL, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_EDIT, DLG_HIDDEN);
        }
        EE_SCREENS_DIALOG_QUESTION => {
            h.call(EE_SCREENS_DLG_BUTTON, DLG_OK, DLG_IDS_YES << 2 | DLG_CALLBACK_FIRST);
            h.call(EE_SCREENS_DLG_BUTTON, DLG_NO, DLG_IDS_NO << 2 | DLG_CALLBACK_SECOND);
            h.call(EE_SCREENS_DLG_MESSAGE, alignment, 0);
            h.call(EE_SCREENS_DLG_SHOW, DLG_CANCEL, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_SHOW, DLG_EDIT, DLG_HIDDEN);
        }
        EE_SCREENS_DIALOG_YES_NO_CANCEL => {
            h.call(EE_SCREENS_DLG_BUTTON, DLG_OK, DLG_IDS_YES << 2 | DLG_CALLBACK_FIRST);
            h.call(EE_SCREENS_DLG_BUTTON, DLG_NO, DLG_IDS_NO << 2 | DLG_CALLBACK_SECOND);
            h.call(EE_SCREENS_DLG_BUTTON, DLG_CANCEL, DLG_IDS_LOBBY_CANCEL << 2 | DLG_CALLBACK_NONE);
            h.call(EE_SCREENS_DLG_SHOW, DLG_EDIT, DLG_HIDDEN);
            h.call(EE_SCREENS_DLG_MESSAGE, alignment, 0);
        }
        _ => return,
    }
    h.call(EE_SCREENS_DLG_MODE, kind, 0);
    h.call(EE_SCREENS_DLG_SWITCH, 0, 0);
}

/// One control's place: the layout's sides (1024ths, 768ths) into the register, then the control's rectangle.
fn place<H: DialogHost + ?Sized>(h: &mut H, control: u32, left: u32, right: u32, top: u32, bottom: u32) {
    h.call(EE_SCREENS_DLG_CALC, left | right << 16, top | bottom << 16);
    h.call(EE_SCREENS_DLG_PLACE, control, 0);
}

/// `UMFloor` (round to nearest even as the conversion does, then down when that went up), as the 32-bit `long`.
fn um_floor(value: f32) -> i32 {
    let rounded = value.round_ties_even();
    let result = if (-2_147_483_648.0..2_147_483_648.0).contains(&rounded) { rounded as i32 } else { i32::MIN };
    if result as f32 > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `EEUIFError::Open`'s layout for the mode (`Layout*`): the buttons' row at 465..491 (each in the scaled Tahoma 16,
/// made once after the first button's place), the message at 300..460 (300..420 above the edit box at 430..460 for
/// the prompt); a message box's text centred vertically when it has fewer lines than fit.
pub fn open<H: DialogHost + ?Sized>(h: &mut H, mode: u32) {
    match mode {
        EE_SCREENS_DIALOG_CANCEL => {
            place(h, DLG_CANCEL, 451, 573, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_CANCEL, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
            place(h, DLG_LABEL, 325, 699, 300, 460);
        }
        EE_SCREENS_DIALOG_PASSWORD => {
            place(h, DLG_OK, 451, 573, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_OK, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
            place(h, DLG_LABEL, 325, 699, 300, 420);
            place(h, DLG_EDIT, 325, 699, 430, 460);
        }
        EE_SCREENS_DIALOG_QUESTION => {
            place(h, DLG_OK, 388, 510, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_OK, 0);
            place(h, DLG_NO, 514, 636, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_NO, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
            place(h, DLG_LABEL, 325, 699, 300, 460);
        }
        EE_SCREENS_DIALOG_YES_NO_CANCEL => {
            place(h, DLG_OK, 325, 447, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_OK, 0);
            place(h, DLG_NO, 451, 573, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_NO, 0);
            place(h, DLG_CANCEL, 577, 699, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_CANCEL, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
            place(h, DLG_LABEL, 325, 699, 300, 460);
        }
        EE_SCREENS_DIALOG_MESSAGE => {
            place(h, DLG_OK, 451, 573, 465, 491);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_OK, 0);
            h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
            h.call(EE_SCREENS_DLG_CALC, 325 | 699 << 16, 300 | 460 << 16);
            let top = f32::from_bits(h.call(EE_SCREENS_DLG_RECT, 1, 0));
            let bottom = f32::from_bits(h.call(EE_SCREENS_DLG_RECT, 3, 0));
            // (the line height is a long, so float when divided and multiplied; the line counts unsigned longs)
            let line_height = h.call(EE_SCREENS_DLG_LINE_HEIGHT, 0, 0) as i32 as f32;
            let lines = (um_floor((bottom - top) / line_height) as u32).wrapping_sub(2);
            let count = h.call(EE_SCREENS_DLG_LINES, 0, 0);
            let offset = if lines > count { lines.wrapping_sub(count) as f32 * line_height * 0.5f32 } else { 0.0f32 };
            h.call(EE_SCREENS_DLG_SET_RECT, 1, (top + offset).to_bits());
            h.call(EE_SCREENS_DLG_SET_RECT, 3, (bottom - offset).to_bits());
            h.call(EE_SCREENS_DLG_PLACE, DLG_LABEL, 0);
        }
        _ => {}
    }
}

/// The text's colour (black), the scroll bar's (white, 0.3 opaque), the edit box's (white) and its selection's
/// (gold), the background's (parchment).
const DLG_BLACK: [f32; 3] = [0.0, 0.0, 0.0];
const DLG_WHITE: [f32; 3] = [1.0, 1.0, 1.0];
const DLG_SCROLL_BAR_OPACITY: f32 = 0.3;
const DLG_GOLD: [f32; 3] = [0.61, 0.46, 0.0];
const DLG_PARCHMENT: [f32; 3] = [0.91, 0.85, 0.73];
const DLG_ALIGN_CENTER: u32 = 1;

/// Loads the float register from f0.
fn floats<H: DialogHost + ?Sized>(h: &mut H, values: &[f32]) {
    for (i, v) in values.iter().enumerate() {
        h.call(EE_SCREENS_DLG_FLOAT, i as u32, v.to_bits());
    }
}

/// `EEUIFError::EEUIFError`: the scaled Tahoma 16, the message label (280x135, white scroll bar, centred black
/// text), OK, No and Cancel, the edit box (220x24, black text on white, a gold selection), each added to the form in
/// that order; the entity group on, mode 0, then the form's layout (295..730 of 1024, 262..506 of 768) and its gold
/// framed parchment background; the font released.
pub fn construct<H: DialogHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_DLG_FONT, DLG_FONT_NEW, 0);
    floats(h, &[280.0, 135.0]);
    h.call(EE_SCREENS_DLG_NEW, DLG_LABEL, 0);
    floats(h, &[DLG_WHITE[0], DLG_WHITE[1], DLG_WHITE[2], DLG_SCROLL_BAR_OPACITY]);
    h.call(EE_SCREENS_DLG_SCROLL_BAR, 0, 0);
    h.call(EE_SCREENS_DLG_ALIGN, DLG_ALIGN_CENTER, 0);
    floats(h, &DLG_BLACK);
    h.call(EE_SCREENS_DLG_TEXT_COLOR, DLG_LABEL, 0);
    h.call(EE_SCREENS_DLG_ADD, DLG_LABEL, 0);
    for button in [DLG_OK, DLG_NO, DLG_CANCEL] {
        h.call(EE_SCREENS_DLG_NEW, button, 0);
        h.call(EE_SCREENS_DLG_ADD, button, 0);
    }
    floats(h, &[220.0, 24.0]);
    h.call(EE_SCREENS_DLG_NEW, DLG_EDIT, 0);
    floats(h, &DLG_BLACK);
    h.call(EE_SCREENS_DLG_TEXT_COLOR, DLG_EDIT, 0);
    let mut colors = [0.0f32; 9];
    colors[..3].copy_from_slice(&DLG_GOLD);
    colors[3..6].copy_from_slice(&DLG_WHITE);
    colors[6..].copy_from_slice(&DLG_BLACK);
    floats(h, &colors);
    h.call(EE_SCREENS_DLG_EDIT_COLORS, 0, 0);
    h.call(EE_SCREENS_DLG_ADD, DLG_EDIT, 0);
    h.call(EE_SCREENS_DLG_ENABLE, 0, 0);
    h.call(EE_SCREENS_DLG_MODE, 0, 0);
    floats(
        h,
        &[
            295.0 / 1024.0,
            730.0 / 1024.0,
            262.0 / 768.0,
            506.0 / 768.0,
            DLG_PARCHMENT[0],
            DLG_PARCHMENT[1],
            DLG_PARCHMENT[2],
        ],
    );
    h.call(EE_SCREENS_DLG_BACKGROUND, 0, 0);
    h.call(EE_SCREENS_DLG_FONT, DLG_FONT_DROP, 0);
}

#[cfg(test)]
mod golden;
