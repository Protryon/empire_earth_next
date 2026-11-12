//! The diplomacy form (form 27) whole: the original `EEUIEDiplomacyEvents.cpp` (its open event, the tribute buttons'
//! left and right clicks, OK, Clear Tributes, Cancel and the check boxes' empty events; the tribute helpers
//! `GetTributeLimit`, `GetTributeResource`, `RefundTributes` and `AddTribute`) over a host ([`DiplomacyHost`]) whose
//! calls are each one toolkit or game call (`UI Events/EEScreens.cpp`). The players reach Rust as records
//! ([`EeScreensDiplomacyPlayer`]); the pending tributes live where the original kept them, in the buttons' texts.
//!
//! The form's controls: 11 the current player's status icon, 12 its name, 13 its civilization, 14..18 its food,
//! wood, stone, gold and iron; then 15 rows of 11 from 19 (the other players in slot order, the current one skipped):
//! the icon, the custom chat check box, the name, the civilization, the stance, the allied check box and the five
//! tribute buttons (food, wood, stone, gold, iron); 184 OK, 185 Clear Tributes, 186 Cancel.

use core::ffi::c_long;

use crate::hot_keys::EE_SCREENS_GAME_HK_SHIFT;
use crate::main_game::EE_SCREENS_GAME_MG_HAS_PLAYER;
use crate::menus::{EE_SCREENS_OP_ENTITY_INDEX, EE_SCREENS_OP_SHOW, EE_SCREENS_TEXT_LABEL};
use crate::settings_form::{
    EE_SCREENS_OP_CHECKBOX, EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_OP_LABEL_NUMBER, EE_SCREENS_OP_SET_CHECKBOX,
    EE_SCREENS_OP_TRIGGER_HERE,
};

// ---- the events (EEScreenEvent::mEvent)

/// The form's open event (`EECreateFormEvent`'s `kFormEventDSOpen`).
pub const EE_SCREENS_DS_OPEN: u32 = 0x1b01;
/// The allied and custom chat check boxes' events (empty: OK reads the boxes).
pub const EE_SCREENS_DS_ALLIED: u32 = 0x1b02;
pub const EE_SCREENS_DS_CUSTOM_CHAT: u32 = 0x1b03;
/// A tribute button's left click (raise) and right click (lower).
pub const EE_SCREENS_DS_TRIBUTE_RAISE: u32 = 0x1b04;
pub const EE_SCREENS_DS_TRIBUTE_LOWER: u32 = 0x1b05;
pub const EE_SCREENS_DS_OK: u32 = 0x1b06;
pub const EE_SCREENS_DS_CLEAR: u32 = 0x1b07;
pub const EE_SCREENS_DS_CANCEL: u32 = 0x1b08;

pub fn is_diplomacy_event(event: u32) -> bool {
    (EE_SCREENS_DS_OPEN..=EE_SCREENS_DS_CANCEL).contains(&event)
}

// ---- the host's ops (ee_screenscpp_call on the diplomacy form)

pub const EE_SCREENS_GAME_DS_PLAYERS: u32 = 0x270; // -> gWorld.GetNumPlayers()
pub const EE_SCREENS_GAME_DS_CURRENT: u32 = 0x271; // -> gWorld.mCurrentPlayer
pub const EE_SCREENS_GAME_DS_TEAMS_LOCKED: u32 = 0x272; // -> gWorld.GetTeamsLocked()
pub const EE_SCREENS_OP_DS_UV: u32 = 0x273; // (bitmap, u0, u1, v) its texture coordinates (u0,0) (u1,0) (u0,v) (u1,v)
pub const EE_SCREENS_OP_DS_BG: u32 = 0x274; // (control, red, green, blue) SetBgColor(GEColor)
pub const EE_SCREENS_OP_DS_FG: u32 = 0x275; // (control, player, button) its text's colour the player's
pub const EE_SCREENS_OP_DS_DISABLE: u32 = 0x276; // (check box) SetDisabled(true)
pub const EE_SCREENS_OP_DS_BUTTON_NUMBER: u32 = 0x277; // (button) -> atoi(its text)
pub const EE_SCREENS_OP_DS_LABEL_NUMBER: u32 = 0x278; // (label) -> GetTextAsInt(0)
pub const EE_SCREENS_GAME_DS_RESOURCES: u32 = 0x279; // (player, resource, amount, add) EEMAddRemoveResources
pub const EE_SCREENS_GAME_DS_STANCE: u32 = 0x27a; // (player, enemy) EEMSetDiplomaticStance
pub const EE_SCREENS_GAME_DS_CHAT: u32 = 0x27b; // (player, selected) EEMSelectCustomPlayerForChat
/// Texts in (ee_screenscpp_text) and out (ee_screenscpp_put_text).
pub const EE_SCREENS_TEXT_DS_NAME: u32 = 0x7b; // (player) its name
pub const EE_SCREENS_TEXT_DS_CIVILIZATION: u32 = 0x7c; // (player) its civilization's name
pub const EE_SCREENS_TEXT_DS_BUTTON: u32 = 0x7d; // put (button): SetText(text, its own font)

/// A player as the diplomacy form reads it (`ee_screenscpp_diplomacy_player`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensDiplomacyPlayer {
    pub player_id: u32,
    pub game_result: u32,
    pub observer: bool,
    pub human: bool,
    pub slot: u8,
    /// `GetResource(kResourceFood..kResourceIron)`.
    pub resources: [i32; 5],
    pub enemies: [u32; 17],
    pub send_chat: [bool; 17],
    pub tribute_fee: f32,
}

// the controls (ee/EEUIManager.h), triggers (lle/UIControl.h), strings
const DS_STATUS: u32 = 11;
const DS_FIRST_ROW: u32 = 19;
const DS_LAST_ROW_CONTROL: u32 = 183;
const DS_OK: u32 = 184;
const DS_CLEAR: u32 = 185;
const DS_CANCEL: u32 = 186;
const TRIGGER_CHECKED: u32 = 0x3e9;
const TRIGGER_UNCHECKED: u32 = 0x3ea;
const TRIGGER_BUTTON_UP: u32 = 0x3f0;
const TRIGGER_RIGHT_BUTTON_UP: u32 = 0x3f6;
const IDS_FRIENDLY: u32 = 29513;
const IDS_HOSTILE: u32 = 29514;
const SLOT_NONE: u8 = 0xff;
const HIDDEN: u32 = 0;

/// The toolkit's and the game's calls of the diplomacy form, on the event's form.
pub trait DiplomacyHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to four arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    /// Player `index` of the world (`gWorld.GetPlayer`); `None` for an empty entry.
    fn player(&mut self, index: u32) -> Option<EeScreensDiplomacyPlayer>;
    /// A text of the game.
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
    /// A resource string.
    fn string(&mut self, id: u32) -> Vec<u8>;
    /// `EEMPayTribute` to player `to`: food, wood, gold, stone, iron (the message's argument order).
    fn pay_tribute(&mut self, to: u32, amounts: [u32; 5]);
}

fn op<H: DiplomacyHost + ?Sized>(h: &mut H, op: u32, a: u32) -> u32 {
    h.call(op, a, 0, 0, 0)
}
fn bits(value: f32) -> u32 {
    value.to_bits()
}
/// A float's truncation as x64's `cvttss2si` (the original's `(long)` casts): out of range or NaN gives `i32::MIN`.
fn truncate(value: f32) -> i32 {
    if value.is_nan() || value >= 2147483648.0 || value < -2147483648.0 {
        i32::MIN
    } else {
        value as i32
    }
}
fn current<H: DiplomacyHost + ?Sized>(h: &mut H) -> Option<EeScreensDiplomacyPlayer> {
    let index = op(h, EE_SCREENS_GAME_DS_CURRENT, 0);
    h.player(index)
}
/// The first control of player `player`'s row (the current player `local` has none).
fn row_of(player: u32, local: u32) -> u32 {
    let base = player.wrapping_mul(11).wrapping_add(DS_FIRST_ROW);
    if player > local {
        base.wrapping_sub(22)
    } else {
        base.wrapping_sub(11)
    }
}

/// The status icon's texture columns: defeated, observing, playing (and, for a row's human without a slot, gone).
const ICON_DEFEATED: (f32, f32) = (0.5859375, 0.7734375);
const ICON_GONE: (f32, f32) = (0.1953125, 0.3828125);
const ICON_OBSERVER: (f32, f32) = (0.390625, 0.578125);
const ICON_PLAYING: (f32, f32) = (0.0, 0.1875);
const ICON_V: f32 = 0.1875;
const HUMAN_COLOUR: [f32; 3] = [0.667, 0.616, 0.0];
const COMPUTER_COLOUR: [f32; 3] = [0.85, 0.85, 0.85];

fn uv<H: DiplomacyHost + ?Sized>(h: &mut H, control: u32, icon: (f32, f32)) {
    h.call(EE_SCREENS_OP_DS_UV, control, bits(icon.0), bits(icon.1), bits(ICON_V));
}
fn bg<H: DiplomacyHost + ?Sized>(h: &mut H, control: u32, colour: [f32; 3]) {
    h.call(EE_SCREENS_OP_DS_BG, control, bits(colour[0]), bits(colour[1]), bits(colour[2]));
}
fn label<H: DiplomacyHost + ?Sized>(h: &mut H, control: u32, text: &[u8]) {
    h.put_text(EE_SCREENS_TEXT_LABEL, control, text);
}
fn fg<H: DiplomacyHost + ?Sized>(h: &mut H, control: u32, player: u32, button: bool) {
    h.call(EE_SCREENS_OP_DS_FG, control, player, button as u32, 0);
}

/// `EEUIEDSOpen`: the current player's icon, name, civilization and resources, OK, Clear and Cancel's events, then a
/// row per other player (empty rows past the last; a row hidden for no player, an observer, a human without a slot
/// and a defeated player): icon, custom chat box, name, civilization, stance, allied box (disabled with the teams
/// locked), five tribute buttons at 0.
fn open<H: DiplomacyHost + ?Sized>(h: &mut H) {
    let current_index = op(h, EE_SCREENS_GAME_DS_CURRENT, 0);
    let Some(local) = h.player(current_index) else { return };
    let local_slot = local.player_id;
    let mut opponent: u32 = 1;
    let friend = h.string(IDS_FRIENDLY);
    let enemy = h.string(IDS_HOSTILE);
    let icon = if local.game_result == 2 {
        ICON_DEFEATED
    } else if local.observer {
        ICON_OBSERVER
    } else {
        ICON_PLAYING
    };
    uv(h, DS_STATUS, icon);
    bg(h, DS_STATUS, HUMAN_COLOUR);
    let name = h.text(EE_SCREENS_TEXT_DS_NAME, current_index);
    label(h, DS_STATUS + 1, &name);
    fg(h, DS_STATUS + 1, current_index, false);
    let civilization = h.text(EE_SCREENS_TEXT_DS_CIVILIZATION, current_index);
    label(h, DS_STATUS + 2, &civilization);
    fg(h, DS_STATUS + 2, current_index, false);
    for (k, amount) in local.resources.iter().enumerate() {
        let control = DS_STATUS + 3 + k as u32;
        h.call(EE_SCREENS_OP_LABEL_NUMBER, control, *amount as u32, 0, 0);
        fg(h, control, current_index, false);
    }
    h.call(EE_SCREENS_OP_TRIGGER_HERE, DS_OK, TRIGGER_BUTTON_UP, EE_SCREENS_DS_OK, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, DS_CLEAR, TRIGGER_BUTTON_UP, EE_SCREENS_DS_CLEAR, 0);
    h.call(EE_SCREENS_OP_TRIGGER_HERE, DS_CANCEL, TRIGGER_BUTTON_UP, EE_SCREENS_DS_CANCEL, 0);
    let mut index = DS_FIRST_ROW;
    if opponent == local_slot {
        opponent += 1;
    }
    while index <= DS_LAST_ROW_CONTROL {
        let other = if opponent < op(h, EE_SCREENS_GAME_DS_PLAYERS, 0) { h.player(opponent) } else { None };
        let hide = match &other {
            None => true,
            Some(p) => p.observer || (p.human && p.slot == SLOT_NONE) || p.game_result == 2,
        };
        match &other {
            None => {
                h.call(EE_SCREENS_OP_SHOW, index, HIDDEN, 0, 0);
            }
            Some(p) => {
                let icon = if p.game_result == 2 {
                    bg(h, index, if p.human { HUMAN_COLOUR } else { COMPUTER_COLOUR });
                    ICON_DEFEATED
                } else if p.human {
                    let icon = if p.slot == SLOT_NONE {
                        ICON_GONE
                    } else if p.observer {
                        ICON_OBSERVER
                    } else {
                        ICON_PLAYING
                    };
                    bg(h, index, HUMAN_COLOUR);
                    icon
                } else {
                    bg(h, index, COMPUTER_COLOUR);
                    ICON_PLAYING
                };
                uv(h, index, icon);
            }
        }
        let hide_or_colour = |h: &mut H, control: u32, button: bool| {
            if hide {
                h.call(EE_SCREENS_OP_SHOW, control, HIDDEN, 0, 0);
            } else {
                fg(h, control, opponent, button);
            }
        };
        // the custom chat box
        index += 1;
        let chat = other.is_some() && local.send_chat[opponent as usize];
        h.call(EE_SCREENS_OP_SET_CHECKBOX, index, chat as u32, 0, 0);
        if hide {
            h.call(EE_SCREENS_OP_SHOW, index, HIDDEN, 0, 0);
        }
        h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_CHECKED, EE_SCREENS_DS_CUSTOM_CHAT, 0);
        h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_UNCHECKED, EE_SCREENS_DS_CUSTOM_CHAT, 0);
        // the name, the civilization, the stance
        index += 1;
        let name = if other.is_some() { h.text(EE_SCREENS_TEXT_DS_NAME, opponent) } else { Vec::new() };
        label(h, index, &name);
        hide_or_colour(h, index, false);
        index += 1;
        let civilization = if other.is_some() { h.text(EE_SCREENS_TEXT_DS_CIVILIZATION, opponent) } else { Vec::new() };
        label(h, index, &civilization);
        hide_or_colour(h, index, false);
        index += 1;
        let stance: &[u8] = match &other {
            None => b"",
            Some(p) if p.enemies[local.player_id as usize] == 0 => &friend,
            Some(_) => &enemy,
        };
        label(h, index, stance);
        hide_or_colour(h, index, false);
        // the allied box
        index += 1;
        let allied = other.is_some() && local.enemies[opponent as usize] == 0;
        h.call(EE_SCREENS_OP_SET_CHECKBOX, index, allied as u32, 0, 0);
        if hide {
            h.call(EE_SCREENS_OP_SHOW, index, HIDDEN, 0, 0);
        }
        if op(h, EE_SCREENS_GAME_DS_TEAMS_LOCKED, 0) == 0 {
            h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_CHECKED, EE_SCREENS_DS_ALLIED, 0);
            h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_UNCHECKED, EE_SCREENS_DS_ALLIED, 0);
        } else {
            op(h, EE_SCREENS_OP_DS_DISABLE, index);
        }
        // the five tribute buttons
        for _ in 0..5 {
            index += 1;
            h.put_text(EE_SCREENS_TEXT_DS_BUTTON, index, b"0");
            hide_or_colour(h, index, true);
            h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_BUTTON_UP, EE_SCREENS_DS_TRIBUTE_RAISE, 0);
            h.call(EE_SCREENS_OP_TRIGGER_HERE, index, TRIGGER_RIGHT_BUTTON_UP, EE_SCREENS_DS_TRIBUTE_LOWER, 0);
        }
        index += 1;
        opponent += 1;
        if opponent == local_slot {
            opponent += 1;
        }
    }
}

/// `GetTributeLimit`: how much more of a tribute button's resource the player can give, the fee added.
fn tribute_limit<H: DiplomacyHost + ?Sized>(h: &mut H, control: u32, fee: f32) -> i32 {
    let label = control.wrapping_sub(DS_FIRST_ROW) % 11 + 8;
    let available = op(h, EE_SCREENS_OP_DS_LABEL_NUMBER, label) as i32;
    truncate((available as f32 + 0.0001) / (fee + 1.0001))
}

/// `GetTributeResource`: a tribute button's resource (2 food, 3 wood, 4 stone, 5 gold, 6 iron; 0 for no button).
pub fn tribute_resource(control: u32) -> u32 {
    match (control.wrapping_sub(DS_FIRST_ROW) % 11).wrapping_sub(6) {
        k @ 0..=4 => 2 + k,
        _ => 0,
    }
}

/// `AddTribute`: an amount (negative: removed) added to the player's shown resource, and that change posted.
fn add_tribute<H: DiplomacyHost + ?Sized>(h: &mut H, resource: u32, amount: i32) {
    if op(h, EE_SCREENS_GAME_MG_HAS_PLAYER, 0) == 0 {
        return;
    }
    let control = match resource {
        2..=6 => 12 + resource,
        _ => 13,
    };
    let value = op(h, EE_SCREENS_OP_DS_LABEL_NUMBER, control) as i32;
    h.call(EE_SCREENS_OP_LABEL_NUMBER, control, value.wrapping_add(amount) as u32, 0, 0);
    let Some(player) = current(h) else { return };
    let magnitude = if amount > 0 { amount } else { amount.wrapping_neg() };
    h.call(EE_SCREENS_GAME_DS_RESOURCES, player.player_id, resource, magnitude as u32, (amount > 0) as u32);
}

/// `RefundTributes`: the resources held in every row's tribute buttons, each with its fee, given back.
fn refund<H: DiplomacyHost + ?Sized>(h: &mut H) {
    let Some(player) = current(h) else { return };
    let local = player.player_id;
    let mut sums = [0i32; 5];
    let mut i = 1;
    while i < op(h, EE_SCREENS_GAME_DS_PLAYERS, 0) {
        if i != local {
            let first = row_of(i, local).wrapping_add(6);
            for (k, sum) in sums.iter_mut().enumerate() {
                *sum = sum.wrapping_add(op(h, EE_SCREENS_OP_DS_BUTTON_NUMBER, first + k as u32) as i32);
            }
        }
        i += 1;
    }
    for (k, sum) in sums.into_iter().enumerate() {
        if sum > 0 {
            let fee = current(h).map_or(0.0, |p| p.tribute_fee);
            add_tribute(h, 2 + k as u32, truncate(fee * sum as f32).wrapping_add(sum));
        }
    }
}

/// `EEUIEDSTributeLClick` / `RClick`: a tribute raised by 100 (500 with Shift), capped at 99999 and what the player
/// can afford, or lowered as much down to 0; the difference (with the fee) taken from or given back to the player.
fn tribute_click<H: DiplomacyHost + ?Sized>(h: &mut H, raise: bool) {
    let step: i32 = if op(h, EE_SCREENS_GAME_HK_SHIFT, 0) as i32 > 0 { 500 } else { 100 };
    let control = op(h, EE_SCREENS_OP_ENTITY_INDEX, 0);
    let old = op(h, EE_SCREENS_OP_DS_BUTTON_NUMBER, control) as i32;
    let amount = if raise {
        let fee = current(h).map_or(0.0, |p| p.tribute_fee);
        let limit = tribute_limit(h, control, fee).wrapping_add(old);
        limit.min(99999.min(old.wrapping_add(step)))
    } else {
        let lowered = old.wrapping_sub(step);
        if lowered < 0 {
            0
        } else {
            lowered
        }
    };
    h.put_text(EE_SCREENS_TEXT_DS_BUTTON, control, amount.to_string().as_bytes());
    if old == amount {
        return;
    }
    let fee = current(h).map_or(0.0, |p| p.tribute_fee);
    let change = old.wrapping_sub(amount);
    add_tribute(h, tribute_resource(control), truncate((change as f32 + 0.0001) * (fee + 1.0001)));
}

/// `EEUIEDSOKButton`: per other player its tribute, its stance (unless the teams are locked) and its custom chat
/// choice posted where they changed; the form closed.
fn ok<H: DiplomacyHost + ?Sized>(h: &mut H) {
    let Some(player) = current(h) else { return };
    let local = player.player_id;
    let mut i = 1;
    while i < op(h, EE_SCREENS_GAME_DS_PLAYERS, 0) {
        if i != local {
            let row = row_of(i, local);
            let custom = op(h, EE_SCREENS_OP_CHECKBOX, row.wrapping_add(1)) != 0;
            let allied = op(h, EE_SCREENS_OP_CHECKBOX, row.wrapping_add(5)) != 0;
            let mut amounts = [0u32; 5];
            for (k, amount) in amounts.iter_mut().enumerate() {
                *amount = op(h, EE_SCREENS_OP_DS_BUTTON_NUMBER, row.wrapping_add(6 + k as u32));
            }
            let [food, wood, stone, gold, iron] = amounts;
            if amounts.iter().any(|&a| a > 0) {
                h.pay_tribute(i, [food, wood, gold, stone, iron]);
            }
            let player = current(h).unwrap_or(player);
            if op(h, EE_SCREENS_GAME_DS_TEAMS_LOCKED, 0) == 0 && allied != (player.enemies[i as usize] == 0) {
                h.call(EE_SCREENS_GAME_DS_STANCE, i, (!allied) as u32, 0, 0);
            }
            if custom != player.send_chat[i as usize] {
                h.call(EE_SCREENS_GAME_DS_CHAT, i, custom as u32, 0, 0);
            }
        }
        i += 1;
    }
    op(h, EE_SCREENS_OP_CLOSE_FORM, 0);
}

/// `EEUIEDSClearTribute`: the pending tributes given back, every tribute button at 0.
fn clear<H: DiplomacyHost + ?Sized>(h: &mut H) {
    refund(h);
    let mut index = DS_FIRST_ROW;
    while index <= DS_LAST_ROW_CONTROL {
        for k in 6..=10 {
            h.put_text(EE_SCREENS_TEXT_DS_BUTTON, index + k, b"0");
        }
        index += 11;
    }
}

/// One of the diplomacy form's events; `None` for another form's, else its `ProcessEvent` result (always 0).
pub fn dispatch<H: DiplomacyHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    match event {
        EE_SCREENS_DS_OPEN => open(h),
        EE_SCREENS_DS_ALLIED | EE_SCREENS_DS_CUSTOM_CHAT => {}
        EE_SCREENS_DS_TRIBUTE_RAISE => tribute_click(h, true),
        EE_SCREENS_DS_TRIBUTE_LOWER => tribute_click(h, false),
        EE_SCREENS_DS_OK => ok(h),
        EE_SCREENS_DS_CLEAR => clear(h),
        EE_SCREENS_DS_CANCEL => {
            if op(h, EE_SCREENS_GAME_MG_HAS_PLAYER, 0) != 0 {
                refund(h);
                op(h, EE_SCREENS_OP_CLOSE_FORM, 0);
            }
        }
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert!(is_diplomacy_event(EE_SCREENS_DS_OPEN) && is_diplomacy_event(EE_SCREENS_DS_CANCEL));
        assert!(!is_diplomacy_event(0x1b00) && !is_diplomacy_event(0x1b09));
    }

    #[test]
    fn rows_and_resources() {
        // the rows skip the current player's: player 1's is the first when the current one is 3, player 4's the third
        assert_eq!(row_of(1, 3), 19);
        assert_eq!(row_of(4, 3), 41);
        assert_eq!(tribute_resource(19 + 6), 2);
        assert_eq!(tribute_resource(41 + 10), 6);
        assert_eq!(tribute_resource(41 + 5), 0);
        assert_eq!(truncate(3.0e9), i32::MIN);
        assert_eq!(truncate(-7.9), -7);
    }
}
