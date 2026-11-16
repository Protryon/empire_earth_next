//! The improvement button (`EEUIBUpgrade`, the base of the main game form's attribute buttons and status bars):
//! whether its improvement can be bought or raised for the selected unit (or the scenario editor's), whether it is
//! greyed out, its tooltip (the improvement's name, next gain and cost, or why not), its presses (buy, refuse with
//! the reason in the chat log at most once per world-table delay, sell back) and its cost label's update. Over a host
//! ([`UpgradeHost`], `ee_screenscpp_upgrade` in `User Interface Thread/EEUIBUpgrade.cpp`) whose ops are each one read
//! of the button, the unit or the world, or one toolkit, text or game call on the host's call record (the button,
//! the out text, a held text, the mouse location). The C++ class keeps its members, cost label, drawing and the
//! toolkit's base calls.

// ---- the host's ops (ee_screenscpp_upgrade(call, op, a, b, c)): units and nodes are handles (0: none).
/// (field) -> the button's field (`EE_SCREENS_UP_FIELD_*`).
pub const EE_SCREENS_UP_OP_FIELD: u32 = 0;
/// (field, value) stores the field.
pub const EE_SCREENS_UP_OP_SET_FIELD: u32 = 1;
/// -> whether the local player has a selected group.
pub const EE_SCREENS_UP_OP_HAS_GROUP: u32 = 2;
/// -> the group's first unit (`GetFirstUnit`).
pub const EE_SCREENS_UP_OP_FIRST_UNIT: u32 = 3;
/// -> the group's unit count.
pub const EE_SCREENS_UP_OP_GROUP_COUNT: u32 = 4;
/// -> the group's unit 0.
pub const EE_SCREENS_UP_OP_GROUP_UNIT: u32 = 5;
/// -> whether unit improvements are allowed.
pub const EE_SCREENS_UP_OP_ALLOWED: u32 = 6;
/// (unit) -> whether its player can buy the improvement for its type (the reason into the out text, if any).
pub const EE_SCREENS_UP_OP_CAN_BUY: u32 = 7;
/// -> the scenario editor's first selected unit.
pub const EE_SCREENS_UP_OP_EDITOR_UNIT: u32 = 8;
/// (unit) -> whether its stats' improvements can raise the improvement for its class.
pub const EE_SCREENS_UP_OP_CAN_RAISE: u32 = 9;
/// (unit) -> whether its player has the improvement at its maximum for its type.
pub const EE_SCREENS_UP_OP_MAXED: u32 = 10;
/// (unit) -> f32 bits of its class's step factor of the improvement.
pub const EE_SCREENS_UP_OP_STEP: u32 = 11;
/// (unit) -> f32 bits of its class's cost factor of the improvement.
pub const EE_SCREENS_UP_OP_COST_FACTOR: u32 = 12;
/// (unit) -> its player's level of the improvement for its type.
pub const EE_SCREENS_UP_OP_LEVEL: u32 = 13;
/// (unit) -> the local player's tech tree node of its type's technology.
pub const EE_SCREENS_UP_OP_NODE: u32 = 14;
/// (unit) the held text: the improvement's name.
pub const EE_SCREENS_UP_OP_TEXT_NAME: u32 = 15;
/// (unit, level) the held text: the improvement's gain at the level.
pub const EE_SCREENS_UP_OP_TEXT_GAIN: u32 = 16;
/// (string id) the held text: the string.
pub const EE_SCREENS_UP_OP_TEXT_STRING: u32 = 17;
/// (node, unit, f32 bits of the scale) the held text: the node's cost for the unit's type at the scale.
pub const EE_SCREENS_UP_OP_TEXT_COST: u32 = 18;
/// The out text += the held text.
pub const EE_SCREENS_UP_OP_OUT_APPEND: u32 = 19;
/// The out text = the held text.
pub const EE_SCREENS_UP_OP_OUT_SET: u32 = 20;
/// (bytes, length) the out text += the bytes.
pub const EE_SCREENS_UP_OP_OUT_BYTES: u32 = 21;
/// (0 the out text, 1 the held text) the control's own tooltip into it.
pub const EE_SCREENS_UP_OP_BASE_HELP: u32 = 22;
/// (1 the press sound, 2 the refusal sound) plays it.
pub const EE_SCREENS_UP_OP_SOUND: u32 = 23;
/// (unit, raise) raises (or sells back) the improvement for the unit.
pub const EE_SCREENS_UP_OP_UPGRADE: u32 = 24;
/// -> the out text's length.
pub const EE_SCREENS_UP_OP_OUT_LENGTH: u32 = 25;
/// -> the world table's refusal message interval (unsigned).
pub const EE_SCREENS_UP_OP_REFUSAL_INTERVAL: u32 = 26;
/// -> `TMGetTime()`.
pub const EE_SCREENS_UP_OP_TIME: u32 = 27;
/// The out text into the chat log.
pub const EE_SCREENS_UP_OP_CHAT: u32 = 28;
/// The form's help text updated.
pub const EE_SCREENS_UP_OP_UPDATE_HELP: u32 = 29;
/// -> whether the button is pressed.
pub const EE_SCREENS_UP_OP_PRESSED: u32 = 30;
/// `UIButton::ButtonUp()`.
pub const EE_SCREENS_UP_OP_BASE_BUTTON_UP: u32 = 31;
/// -> `UIButton::MouseMove(the call's location)`.
pub const EE_SCREENS_UP_OP_BASE_MOVE: u32 = 32;
/// The button's tooltip (`GetHelpString`) into the held text.
pub const EE_SCREENS_UP_OP_HELP_TEXT: u32 = 33;
/// The form's help label (when it has one) shows the held text.
pub const EE_SCREENS_UP_OP_HELP_LABEL: u32 = 34;
/// -> whether the button's rectangle contains the mouse.
pub const EE_SCREENS_UP_OP_CONTAINS: u32 = 35;
/// Releases the captured mouse.
pub const EE_SCREENS_UP_OP_RELEASE_MOUSE: u32 = 36;
/// -> whether the button has a cost label whose text differs from the out text.
pub const EE_SCREENS_UP_OP_COST_DIFFERS: u32 = 37;
/// The cost label's text = the out text.
pub const EE_SCREENS_UP_OP_COST_SET: u32 = 38;
/// `SetDirty(true)`.
pub const EE_SCREENS_UP_OP_DIRTY: u32 = 39;
/// (f32 bits) `SetBgOpacity`.
pub const EE_SCREENS_UP_OP_BG_OPACITY: u32 = 40;
/// (f32 bits) `SetBorderOpacity`.
pub const EE_SCREENS_UP_OP_BORDER_OPACITY: u32 = 41;
/// (thickness) `SetBorderThickness`.
pub const EE_SCREENS_UP_OP_BORDER_THICKNESS: u32 = 42;
/// -> the scenario editor's first selected unit's complex unit.
pub const EE_SCREENS_UP_OP_EDITOR_COMPLEX: u32 = 43;

// ---- the status bar's (`EEUIFMGStatusBar`, an improvement button drawn as an icon, a bar and two values)
/// The bar's a: `mEnabled` (0), `mPressed` (1), `GetHasMouseCapture()` (2).
pub const EE_SCREENS_UP_OP_BAR_FIELD: u32 = 44;
/// The bar's `mPressed = a != 0`.
pub const EE_SCREENS_UP_OP_BAR_SET_PRESSED: u32 = 45;
/// Whether the button's rectangle contains the form's mouse location (a 0) or the call's location (a 1).
pub const EE_SCREENS_UP_OP_BAR_INSIDE: u32 = 46;
/// The handler under the bar's: `UIButton::MouseLButtonDown` (a 0), `UIButton::MouseLButtonUp` (1),
/// `EEUIBUpgrade::MouseRButtonDown` (2); its result.
pub const EE_SCREENS_UP_OP_BAR_BASE: u32 = 47;
/// The icon moved by (a, a) (`mIcon->SetLocation(its top left + (a, a))`).
pub const EE_SCREENS_UP_OP_BAR_ICON: u32 = 48;

/// The status bar's mouse handlers (`bar_mouse`'s which).
pub const EE_SCREENS_UP_BAR_L_DOWN: u32 = 0;
pub const EE_SCREENS_UP_BAR_L_UP: u32 = 1;
pub const EE_SCREENS_UP_BAR_MOVE: u32 = 2;
pub const EE_SCREENS_UP_BAR_R_DOWN: u32 = 3;
pub const EE_SCREENS_UP_BAR_R_UP: u32 = 4;

/// The fields of [`EE_SCREENS_UP_OP_FIELD`]: the improvement, the scenario editor flag, the level last shown, the
/// greyed-out state of the last cost update, the time of the last refusal line.
pub const EE_SCREENS_UP_FIELD_ATTRIBUTE: i64 = 0;
pub const EE_SCREENS_UP_FIELD_EDITOR: i64 = 1;
pub const EE_SCREENS_UP_FIELD_LEVEL: i64 = 2;
pub const EE_SCREENS_UP_FIELD_DISABLED: i64 = 3;
pub const EE_SCREENS_UP_FIELD_LAST_MESSAGE: i64 = 4;

/// The help-only improvement; the repair improvement (no step factor); the strings.
const HELP_ONLY: i64 = 0x10;
const REPAIR: i64 = 12;
const NO_TECHNOLOGY_NODE: &str = "No Technology Node Associated With This Object";
const IDS_AT_MAXIMUM: i64 = 30020;
const IDS_UNIT_IMPROVEMENTS_NOT_ALLOWED: i64 = 30022;
/// The button's tint (`kBgOpacity`, 0x449d4c).
const BG_OPACITY: f32 = 0.15;
const SOUND_PRESS: i64 = 1;
const SOUND_REFUSAL: i64 = 2;
const TO_OUT: i64 = 0;
const TO_HELD: i64 = 1;

/// The button's calls.
pub trait UpgradeHost {
    /// `EE_SCREENS_UP_OP_*` with up to three arguments; its answer.
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64;
}

fn op<H: UpgradeHost + ?Sized>(h: &mut H, op: u32, a: i64) -> i64 {
    h.op(op, a, 0, 0)
}

fn field<H: UpgradeHost + ?Sized>(h: &mut H, which: i64) -> i64 {
    h.op(EE_SCREENS_UP_OP_FIELD, which, 0, 0)
}

/// The improvement's step factor for the unit's class (none for repair).
fn step<H: UpgradeHost + ?Sized>(h: &mut H, unit: i64, attribute: i64) -> f32 {
    if attribute != REPAIR {
        f32::from_bits(op(h, EE_SCREENS_UP_OP_STEP, unit) as u32)
    } else {
        0.0
    }
}

/// `GetCanUpgrade`: whether the improvement can be bought for the selected group's first unit (improvements
/// allowed; the player's reason why not into the out text), or raised for the scenario editor's first selected
/// unit; with the unit it stored (`None`: not stored).
pub fn can_upgrade<H: UpgradeHost + ?Sized>(h: &mut H) -> (bool, Option<i64>) {
    if field(h, EE_SCREENS_UP_FIELD_EDITOR) == 0 {
        if op(h, EE_SCREENS_UP_OP_HAS_GROUP, 0) != 0 {
            let unit = op(h, EE_SCREENS_UP_OP_FIRST_UNIT, 0);
            if unit != 0 && op(h, EE_SCREENS_UP_OP_ALLOWED, 0) != 0 && op(h, EE_SCREENS_UP_OP_CAN_BUY, unit) != 0 {
                return (true, Some(unit));
            }
        }
        (false, None)
    } else {
        let unit = op(h, EE_SCREENS_UP_OP_EDITOR_UNIT, 0);
        (unit != 0 && op(h, EE_SCREENS_UP_OP_CAN_RAISE, unit) != 0, Some(unit))
    }
}

/// `GetDisabled`: greyed out unless the selected group's first unit's improvement is allowed, not at its maximum
/// and has a step for its class (in the scenario editor: unless the first selected unit can raise it).
pub fn disabled<H: UpgradeHost + ?Sized>(h: &mut H) -> bool {
    if field(h, EE_SCREENS_UP_FIELD_EDITOR) == 0 {
        if op(h, EE_SCREENS_UP_OP_HAS_GROUP, 0) != 0 {
            let unit = op(h, EE_SCREENS_UP_OP_FIRST_UNIT, 0);
            if unit != 0 && op(h, EE_SCREENS_UP_OP_ALLOWED, 0) != 0 && op(h, EE_SCREENS_UP_OP_MAXED, unit) == 0 {
                let attribute = field(h, EE_SCREENS_UP_FIELD_ATTRIBUTE);
                if step(h, unit, attribute) > 0.0 {
                    return false;
                }
            }
        }
        true
    } else {
        let unit = op(h, EE_SCREENS_UP_OP_EDITOR_UNIT, 0);
        !(unit != 0 && op(h, EE_SCREENS_UP_OP_CAN_RAISE, unit) != 0)
    }
}

fn out_bytes<H: UpgradeHost + ?Sized>(h: &mut H, text: &str) {
    h.op(EE_SCREENS_UP_OP_OUT_BYTES, text.as_ptr() as i64, text.len() as i64, 0);
}

/// `GetHelpString` (appending to the out text): the improvement's name, then its next gain and cost (": " between)
/// or " At Maximum"; the control's own tooltip for the help-only button; "not allowed" (replacing the text) when
/// improvements are off; false with no selection. In the scenario editor: the name and " " gain, or " At Maximum";
/// false with no unit.
pub fn help<H: UpgradeHost + ?Sized>(h: &mut H) -> bool {
    let attribute = field(h, EE_SCREENS_UP_FIELD_ATTRIBUTE);
    if field(h, EE_SCREENS_UP_FIELD_EDITOR) == 0 {
        if op(h, EE_SCREENS_UP_OP_GROUP_COUNT, 0) == 0 {
            return false;
        }
        if op(h, EE_SCREENS_UP_OP_ALLOWED, 0) == 0 {
            op(h, EE_SCREENS_UP_OP_TEXT_STRING, IDS_UNIT_IMPROVEMENTS_NOT_ALLOWED);
            op(h, EE_SCREENS_UP_OP_OUT_SET, 0);
            return true;
        }
        if attribute == HELP_ONLY {
            op(h, EE_SCREENS_UP_OP_BASE_HELP, TO_OUT);
            return true;
        }
        let unit = op(h, EE_SCREENS_UP_OP_GROUP_UNIT, 0);
        op(h, EE_SCREENS_UP_OP_TEXT_NAME, unit);
        op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
        if step(h, unit, attribute) > 0.0 && op(h, EE_SCREENS_UP_OP_MAXED, unit) == 0 {
            let level = op(h, EE_SCREENS_UP_OP_LEVEL, unit) as i32;
            h.op(EE_SCREENS_UP_OP_TEXT_GAIN, unit, level as i64, 0);
            op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
            out_bytes(h, ": ");
            let node = op(h, EE_SCREENS_UP_OP_NODE, unit);
            if node != 0 {
                let scale = f32::from_bits(op(h, EE_SCREENS_UP_OP_COST_FACTOR, unit) as u32) * level as f32;
                h.op(EE_SCREENS_UP_OP_TEXT_COST, node, unit, scale.to_bits() as i64);
                op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
            } else {
                out_bytes(h, NO_TECHNOLOGY_NODE);
            }
        } else {
            out_bytes(h, " ");
            op(h, EE_SCREENS_UP_OP_TEXT_STRING, IDS_AT_MAXIMUM);
            op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
        }
        true
    } else {
        let unit = op(h, EE_SCREENS_UP_OP_EDITOR_COMPLEX, 0);
        if unit == 0 {
            return false;
        }
        if op(h, EE_SCREENS_UP_OP_CAN_RAISE, unit) != 0 {
            let level = op(h, EE_SCREENS_UP_OP_LEVEL, unit) as i32;
            op(h, EE_SCREENS_UP_OP_TEXT_NAME, unit);
            op(h, EE_SCREENS_UP_OP_OUT_SET, 0);
            h.op(EE_SCREENS_UP_OP_TEXT_GAIN, unit, level as i64, 0);
            out_bytes(h, " ");
            op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
        } else {
            out_bytes(h, " ");
            op(h, EE_SCREENS_UP_OP_TEXT_STRING, IDS_AT_MAXIMUM);
            op(h, EE_SCREENS_UP_OP_OUT_APPEND, 0);
        }
        true
    }
}

/// `ButtonUp` (the out text is the reason): buys the improvement for the unit when it can (the press sound);
/// otherwise, unless greyed out, the refusal sound and the reason in the chat log at most once per interval; the
/// help text updated; the toolkit's release when pressed.
pub fn button_up<H: UpgradeHost + ?Sized>(h: &mut H) {
    let (can, unit) = can_upgrade(h);
    if can {
        let unit = unit.unwrap_or(0);
        if unit != 0 {
            op(h, EE_SCREENS_UP_OP_SOUND, SOUND_PRESS);
            h.op(EE_SCREENS_UP_OP_UPGRADE, unit, 1, 0);
        }
    } else if !disabled(h) {
        op(h, EE_SCREENS_UP_OP_SOUND, SOUND_REFUSAL);
        if op(h, EE_SCREENS_UP_OP_OUT_LENGTH, 0) as i32 > 0 {
            let interval = op(h, EE_SCREENS_UP_OP_REFUSAL_INTERVAL, 0) as u32;
            let last = field(h, EE_SCREENS_UP_FIELD_LAST_MESSAGE) as u32;
            if (op(h, EE_SCREENS_UP_OP_TIME, 0) as u32).wrapping_sub(last) > interval {
                op(h, EE_SCREENS_UP_OP_CHAT, 0);
                let now = op(h, EE_SCREENS_UP_OP_TIME, 0);
                h.op(EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_FIELD_LAST_MESSAGE, now, 0);
            }
        }
    }
    op(h, EE_SCREENS_UP_OP_UPDATE_HELP, 0);
    if op(h, EE_SCREENS_UP_OP_PRESSED, 0) != 0 {
        op(h, EE_SCREENS_UP_OP_BASE_BUTTON_UP, 0);
    }
}

/// `MouseMove`: with a selection, when the improvement's level changed the help label shows the new tooltip (the
/// control's own for the help-only button); then the toolkit's.
pub fn mouse_move<H: UpgradeHost + ?Sized>(h: &mut H) -> i64 {
    if op(h, EE_SCREENS_UP_OP_GROUP_COUNT, 0) != 0 {
        if field(h, EE_SCREENS_UP_FIELD_ATTRIBUTE) != HELP_ONLY {
            let unit = op(h, EE_SCREENS_UP_OP_GROUP_UNIT, 0);
            let level = op(h, EE_SCREENS_UP_OP_LEVEL, unit);
            if level as i32 == field(h, EE_SCREENS_UP_FIELD_LEVEL) as i32 {
                return op(h, EE_SCREENS_UP_OP_BASE_MOVE, 0);
            }
            h.op(EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_FIELD_LEVEL, level, 0);
            op(h, EE_SCREENS_UP_OP_HELP_TEXT, 0);
        } else {
            op(h, EE_SCREENS_UP_OP_BASE_HELP, TO_HELD);
        }
        op(h, EE_SCREENS_UP_OP_HELP_LABEL, 0);
    }
    op(h, EE_SCREENS_UP_OP_BASE_MOVE, 0)
}

/// `MouseRButtonUp`: over the button, sells back the improvement of the selected group's first unit (or the scenario
/// editor's); the help text updated; the toolkit's release when it can be bought; the mouse released.
pub fn mouse_r_up<H: UpgradeHost + ?Sized>(h: &mut H) -> i64 {
    if op(h, EE_SCREENS_UP_OP_CONTAINS, 0) != 0 {
        let unit = if field(h, EE_SCREENS_UP_FIELD_EDITOR) == 0 {
            if op(h, EE_SCREENS_UP_OP_HAS_GROUP, 0) != 0 {
                op(h, EE_SCREENS_UP_OP_FIRST_UNIT, 0)
            } else {
                0
            }
        } else {
            op(h, EE_SCREENS_UP_OP_EDITOR_UNIT, 0)
        };
        if unit != 0 {
            h.op(EE_SCREENS_UP_OP_UPGRADE, unit, 0, 0);
        }
    }
    op(h, EE_SCREENS_UP_OP_UPDATE_HELP, 0);
    if can_upgrade(h).0 {
        op(h, EE_SCREENS_UP_OP_BASE_BUTTON_UP, 0);
    }
    op(h, EE_SCREENS_UP_OP_RELEASE_MOUSE, 0);
    0
}

/// `UpdateCost` (the out text is the new cost): the cost label's text when it changed; when the greyed-out state
/// changed, the button's tint and border (none when greyed out), dirty.
pub fn update_cost<H: UpgradeHost + ?Sized>(h: &mut H) {
    if op(h, EE_SCREENS_UP_OP_COST_DIFFERS, 0) != 0 {
        op(h, EE_SCREENS_UP_OP_COST_SET, 0);
        op(h, EE_SCREENS_UP_OP_DIRTY, 0);
    }
    if (field(h, EE_SCREENS_UP_FIELD_DISABLED) != 0) == disabled(h) {
        return;
    }
    let now = disabled(h);
    h.op(EE_SCREENS_UP_OP_SET_FIELD, EE_SCREENS_UP_FIELD_DISABLED, now as i64, 0);
    if !now {
        op(h, EE_SCREENS_UP_OP_BG_OPACITY, BG_OPACITY.to_bits() as i64);
        op(h, EE_SCREENS_UP_OP_BORDER_OPACITY, (BG_OPACITY * 3.0).to_bits() as i64);
        op(h, EE_SCREENS_UP_OP_BORDER_THICKNESS, 1);
    } else {
        op(h, EE_SCREENS_UP_OP_BG_OPACITY, 0.0f32.to_bits() as i64);
        op(h, EE_SCREENS_UP_OP_BORDER_THICKNESS, 0);
    }
    op(h, EE_SCREENS_UP_OP_DIRTY, 0);
}

#[cfg(test)]
mod golden;

/// `EEUIFMGStatusBar::MoveIcon` (0x269c43): the icon moved by (by, by) when the improvement can be bought.
fn move_icon<H: UpgradeHost + ?Sized>(h: &mut H, by: i64) {
    if can_upgrade(h).0 {
        op(h, EE_SCREENS_UP_OP_BAR_ICON, by);
    }
}

/// The icon pressed (nudged down-right) or released (moved back) when it is not already.
fn press_icon<H: UpgradeHost + ?Sized>(h: &mut H, pressed: bool) {
    if (op(h, EE_SCREENS_UP_OP_BAR_FIELD, 1) != 0) != pressed {
        move_icon(h, if pressed { 1 } else { -1 });
        op(h, EE_SCREENS_UP_OP_BAR_SET_PRESSED, pressed as i64);
    }
}

/// `EEUIFMGStatusBar`'s mouse handler `which` (`EE_SCREENS_UP_BAR_*`; 0x269a40, 0x269a9c, 0x269afa, 0x269b8b,
/// 0x269be6): a press or release inside an enabled button presses or releases the icon, then the button's own
/// handler (nothing when disabled); a move while captured makes the icon follow the mouse in and out, then the
/// improvement button's move.
pub fn bar_mouse<H: UpgradeHost + ?Sized>(h: &mut H, which: u32) -> i64 {
    if which == EE_SCREENS_UP_BAR_MOVE {
        if op(h, EE_SCREENS_UP_OP_BAR_FIELD, 2) != 0 {
            let inside = op(h, EE_SCREENS_UP_OP_BAR_INSIDE, 1) != 0;
            press_icon(h, inside);
        }
        return mouse_move(h);
    }
    if op(h, EE_SCREENS_UP_OP_BAR_FIELD, 0) == 0 {
        return 0;
    }
    let press = which == EE_SCREENS_UP_BAR_L_DOWN || which == EE_SCREENS_UP_BAR_R_DOWN;
    if op(h, EE_SCREENS_UP_OP_BAR_INSIDE, 0) != 0 {
        press_icon(h, press);
    }
    match which {
        EE_SCREENS_UP_BAR_L_DOWN => op(h, EE_SCREENS_UP_OP_BAR_BASE, 0),
        EE_SCREENS_UP_BAR_L_UP => op(h, EE_SCREENS_UP_OP_BAR_BASE, 1),
        EE_SCREENS_UP_BAR_R_DOWN => op(h, EE_SCREENS_UP_OP_BAR_BASE, 2),
        _ => mouse_r_up(h),
    }
}

#[cfg(test)]
mod bar_tests {
    use super::*;

    /// A host answering the bar's fields and the button's reads from a table (unlisted ops answer 0), recording ops.
    struct Bar {
        enabled: bool,
        pressed: bool,
        captured: bool,
        inside: [bool; 2],
        can_buy: bool,
        log: Vec<(u32, i64)>,
    }

    impl UpgradeHost for Bar {
        fn op(&mut self, op: u32, a: i64, _: i64, _: i64) -> i64 {
            self.log.push((op, a));
            match op {
                EE_SCREENS_UP_OP_BAR_FIELD => [self.enabled, self.pressed, self.captured][a as usize] as i64,
                EE_SCREENS_UP_OP_BAR_SET_PRESSED => {
                    self.pressed = a != 0;
                    0
                }
                EE_SCREENS_UP_OP_BAR_INSIDE => self.inside[a as usize] as i64,
                EE_SCREENS_UP_OP_BAR_BASE => 10 + a,
                // the selection's first unit, allowed, buyable
                EE_SCREENS_UP_OP_HAS_GROUP | EE_SCREENS_UP_OP_FIRST_UNIT | EE_SCREENS_UP_OP_ALLOWED => 1,
                EE_SCREENS_UP_OP_CAN_BUY => self.can_buy as i64,
                _ => 0,
            }
        }
    }

    fn bar(enabled: bool, pressed: bool, inside: bool) -> Bar {
        Bar { enabled, pressed, captured: true, inside: [inside, inside], can_buy: true, log: Vec::new() }
    }

    fn icon(b: &Bar) -> Vec<i64> {
        b.log.iter().filter(|o| o.0 == EE_SCREENS_UP_OP_BAR_ICON).map(|o| o.1).collect()
    }

    #[test]
    fn a_press_inside_an_enabled_bar_nudges_the_icon_then_the_buttons_press() {
        let mut b = bar(true, false, true);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_L_DOWN), 10);
        assert_eq!((icon(&b), b.pressed), (vec![1], true));
        // pressed already: no second nudge
        let mut b = bar(true, true, true);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_R_DOWN), 12);
        assert!(icon(&b).is_empty());
        // the release moves it back
        let mut b = bar(true, true, true);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_L_UP), 11);
        assert_eq!((icon(&b), b.pressed), (vec![-1], false));
        // outside: the button's own only; disabled: nothing at all
        let mut b = bar(true, false, false);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_L_DOWN), 10);
        assert!(icon(&b).is_empty() && !b.pressed);
        let mut b = bar(false, false, true);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_L_DOWN), 0);
        assert_eq!(b.log, [(EE_SCREENS_UP_OP_BAR_FIELD, 0)]);
    }

    #[test]
    fn the_icon_moves_only_when_the_improvement_can_be_bought() {
        let mut b = bar(true, false, true);
        b.can_buy = false;
        bar_mouse(&mut b, EE_SCREENS_UP_BAR_L_DOWN);
        assert!(icon(&b).is_empty());
        assert!(b.pressed, "the state follows the mouse even so");
    }

    #[test]
    fn a_captured_move_follows_the_mouse_in_and_out_then_the_buttons_move() {
        let mut b = bar(true, false, true);
        b.inside = [false, true];
        bar_mouse(&mut b, EE_SCREENS_UP_BAR_MOVE);
        assert_eq!((icon(&b), b.pressed), (vec![1], true));
        assert_eq!(b.log.last(), Some(&(EE_SCREENS_UP_OP_BASE_MOVE, 0)));
        b.inside = [true, false];
        b.log.clear();
        bar_mouse(&mut b, EE_SCREENS_UP_BAR_MOVE);
        assert_eq!((icon(&b), b.pressed), (vec![-1], false));
        // not captured: only the button's move
        let mut b = bar(true, false, true);
        b.captured = false;
        bar_mouse(&mut b, EE_SCREENS_UP_BAR_MOVE);
        assert!(icon(&b).is_empty() && !b.pressed);
    }

    #[test]
    fn the_right_release_is_the_buttons() {
        let mut b = bar(true, true, true);
        assert_eq!(bar_mouse(&mut b, EE_SCREENS_UP_BAR_R_UP), 0);
        assert_eq!(icon(&b), [-1]);
        assert_eq!(b.log.last(), Some(&(EE_SCREENS_UP_OP_RELEASE_MOUSE, 0)));
    }
}
