//! The scenario editor's condition types whole in Rust (`EESETSConditionType` and its seven `EESETSCT*` classes, once
//! `EESETSConditionType.cpp`): what each condition type shows on the condition screen's shared controls
//! (`EESETSConditionControls`), stores into the condition and loads from it.
//!
//! The same shape as the effect types (`editor_effects.rs`): every control is addressed by its array and index
//! (`EE_SCREENS_SEET_A_*`; the condition controls' one slider is `(EE_SCREENS_SEET_A_SLIDER, 0)`) through the
//! generic host's condition twin, `ee_screenscpp_sect` (`Scenario Editor/EESETSEffect.cpp`, the same control ops);
//! the condition's numbers are its `EeScenarioConditionFields`, read and written in place, its chat text a host op.
//!
//! A host call can come back into the types (a radio button's event): they are used through `&self`, the research
//! list in a `RefCell` never borrowed across a host call.

use core::cell::RefCell;

use ee_sim::scenario::fields::EeScenarioConditionFields;

use crate::screens::editor_effects::*;
use crate::screens::editor_events::EE_SCREENS_SEE_CHECK_DISPLAY;
use crate::screens::editor_players::SeplHost;
use crate::screens::scenario_editor::{EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE, EE_SCREENS_SE_SELECT_NAME};

/// One call's context over a condition.
pub type Cx<'a, H> = Fx<'a, H, EeScenarioConditionFields>;

/// `kControlTriggerUnchecked`, `EnterPressed`.
const T_UNCHECKED: i32 = 0x3ea;
const T_ENTER: i32 = 0x3f1;

/// The condition types (`kSMCondition*`): always true, player attribute, player technology, trigger attribute,
/// player diplomacy, game attribute, chat message.
const ALWAYS_TRUE: u32 = 0;
const PLAYER_ATTRIBUTE: u32 = 2;
const PLAYER_TECHNOLOGY: u32 = 3;
const TRIGGER_ATTRIBUTE: u32 = 4;
const PLAYER_DIPLOMACY: u32 = 5;
const GAME_ATTRIBUTE: u32 = 6;
const CHAT_MESSAGE: u32 = 7;

/// `kSETSConditionTitleLabel`, `kSETSConditionTypeComboBox`, `kSETriggerScreenNameEditBox`, `NameLabel`,
/// `DuplicateButton`, `DeleteButton`: what the always-true type hides while shown.
pub const EE_SCREENS_SECT_ALWAYS_TRUE_CONTROLS: [i32; 6] = [0x8fc, 0x8fd, 0x7db, 0x7dc, 0x7d6, 0x7d7];

/// Every condition type (`EESETSCondition::mTypes`): the research type's technologies (`mTechnologies`, listed on
/// its first Show) and the player attribute type's radio buttons' event (`mEvent`).
#[derive(Debug, Default)]
pub struct ConditionTypes {
    pub techs: RefCell<Vec<i32>>,
    pub resource_event: usize,
}

impl ConditionTypes {
    /// The constructors (`EESETSCondition`'s `new`s): the player attribute type's event made.
    pub fn new<H: SeplHost + ?Sized>(c: &mut Cx<H>) -> ConditionTypes {
        ConditionTypes {
            techs: RefCell::new(Vec::new()),
            resource_event: c.screen_event(EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE),
        }
    }

    /// The destructors: the player attribute type's event released.
    pub fn destroy<H: SeplHost + ?Sized>(&self, c: &mut Cx<H>) {
        c.release(self.resource_event);
    }

    /// The type `ty`'s slot (`EE_SCREENS_SEET_STORE`, `_HIDE`, `_LOAD`, `_SHOW`).
    pub fn call<H: SeplHost + ?Sized>(&self, c: &mut Cx<H>, ty: u32, slot: u32) {
        match ty {
            ALWAYS_TRUE => always_true(c, slot),
            PLAYER_ATTRIBUTE => self.player_attribute(c, slot),
            PLAYER_TECHNOLOGY => self.player_technology(c, slot),
            TRIGGER_ATTRIBUTE => trigger_attribute(c, slot),
            PLAYER_DIPLOMACY => diplomacy(c, slot),
            GAME_ATTRIBUTE => game(c, slot),
            CHAT_MESSAGE => chat(c, slot),
            _ => {}
        }
    }

    /// `EESETSCTPlayerResearch`: a player has researched a technology.
    fn player_technology<H: SeplHost + ?Sized>(&self, c: &mut Cx<H>, slot: u32) {
        match slot {
            EE_SCREENS_SEET_STORE => {
                c.e().mPlayer = c.player(0);
                // the original indexes its list with an unset selection when there is none: kept then
                if let Some(s) = c.selection(COMBO, 0) {
                    if let Some(&t) = self.techs.borrow().get(s as usize) {
                        c.e().mTechnology = t;
                    }
                }
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(LABEL, 0);
                c.hide(PLAYER, 0);
                c.hide(LABEL, 1);
                c.hide(COMBO, 0);
            }
            EE_SCREENS_SEET_LOAD => {
                let (p, t) = (c.e().mPlayer, c.e().mTechnology);
                c.set_player(0, p);
                let line = self.techs.borrow().iter().position(|&x| x == t).unwrap_or(0);
                c.select(COMBO, 0, line as u32);
            }
            EE_SCREENS_SEET_SHOW => {
                // listed on the first Show of this object (made with every scenario editor)
                if self.techs.borrow().is_empty() {
                    let techs = c.techs_of(true);
                    *self.techs.borrow_mut() = techs;
                }
                label(c, 0, IDS_PLAYER_CONDITION_DETAILS, 4.0, 616.0, 300.0, 20.0);
                let y = c.bottom(LABEL, 0) + 10.0;
                c.locate(PLAYER, 0, 4.0, y);
                c.player_flags(0, 0xde);
                c.show(PLAYER, 0);
                let x = c.right(PLAYER, 0) + 10.0;
                label(c, 1, IDS_HAS_RESEARCHED, x, y, 200.0, 20.0);
                let y = c.bottom(LABEL, 1) + 10.0;
                c.clear(COMBO, 0);
                let techs = snapshot(&self.techs);
                for t in techs {
                    let name = c.tech_name(t);
                    c.append(COMBO, 0, &name);
                }
                c.width(COMBO, 0, 0xfa);
                c.locate(COMBO, 0, x, y);
                c.show(COMBO, 0);
            }
            _ => {}
        }
    }

    /// `EESETSCTPlayerResource`: a player's attribute against an amount or another player's attribute.
    fn player_attribute<H: SeplHost + ?Sized>(&self, c: &mut Cx<H>, slot: u32) {
        match slot {
            EE_SCREENS_SEET_STORE => {
                c.e().mPlayer = c.player(0);
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mAttribute = s;
                }
                if let Some(s) = c.selection(COMBO, 1) {
                    c.e().mOperator = s;
                }
                c.e().mAmount = c.text_int(EDIT, 0) as u32;
                c.e().mOtherPlayer = c.player(1);
                if let Some(s) = c.selection(COMBO, 2) {
                    c.e().mOtherAttribute = s;
                }
                c.e().mUsesPlayerAttribute = c.checked(RADIO, 1);
            }
            EE_SCREENS_SEET_HIDE => {
                for (a, k) in [
                    (LABEL, 0),
                    (PLAYER, 0),
                    (LABEL, 2),
                    (COMBO, 0),
                    (COMBO, 1),
                    (RADIO, 0),
                    (RADIO, 1),
                    (EDIT, 0),
                    (PLAYER, 1),
                    (COMBO, 2),
                ] {
                    c.hide(a, k);
                }
                c.event_null(RADIO, 0, T_CHECKED);
                c.event_null(RADIO, 1, T_CHECKED);
            }
            EE_SCREENS_SEET_LOAD => {
                let e = *c.e();
                c.set_player(0, e.mPlayer);
                c.select(COMBO, 0, e.mAttribute);
                c.select(COMBO, 1, e.mOperator);
                c.number(EDIT, 0, e.mAmount as i32);
                c.set_player(1, e.mOtherPlayer);
                c.select(COMBO, 2, e.mOtherAttribute);
                let k = if e.mUsesPlayerAttribute { 1 } else { 0 };
                c.check(k);
                c.event_process(self.resource_event, RADIO, k);
            }
            EE_SCREENS_SEET_SHOW => {
                c.set_rect(LABEL, 0, 4.0, 616.0, 300.0, 20.0);
                c.text_id(LABEL, 0, IDS_PLAYER_ATTRIBUTE_CONDITION_DETAILS);
                c.show(LABEL, 0);
                let y = c.bottom(LABEL, 0) + 10.0;
                c.locate(PLAYER, 0, 4.0, y);
                c.player_flags(0, 0xde);
                c.show(PLAYER, 0);
                let x = c.right(PLAYER, 0) + 2.0;
                c.set_rect(LABEL, 2, x, y, 10.0, 20.0);
                c.align(LABEL, 2, AL_CENTER);
                c.text_id(LABEL, 2, IDS_S);
                c.show(LABEL, 2);

                let attributes = attribute_names(c);
                let x = c.right(LABEL, 2) + 2.0;
                c.clear(COMBO, 0);
                c.clear(COMBO, 2);
                for a in &attributes {
                    c.append(COMBO, 0, a);
                    c.append(COMBO, 2, a);
                }
                c.font_530_both(COMBO, 0);
                c.width(COMBO, 0, 125);
                c.locate(COMBO, 0, x, y);
                c.show(COMBO, 0);

                let x = c.right(COMBO, 0) + 5.0;
                c.clear(COMBO, 1);
                c.append_ids(COMBO, 1, IDS_TRIGGER_COMPARISON_FIRST, IDS_TRIGGER_COMPARISON_FIRST + 4);
                c.width(COMBO, 1, 50);
                c.locate(COMBO, 1, x, y);
                c.show(COMBO, 1);

                let x = c.right(COMBO, 1) + 5.0;
                radio(c, 0, IDS_USE_AMOUNT, x, y);
                let y1 = c.bottom(RADIO, 0) + 5.0;
                radio(c, 1, IDS_USE_PLAYER_ATTRIBUTE, x, y1);

                let x = c.right(RADIO, 0) + 5.0;
                let y = c.top(RADIO, 0);
                c.number(EDIT, 0, 0);
                c.locate(EDIT, 0, x, y);
                c.show(EDIT, 0);
                c.locate(PLAYER, 1, x, y);
                c.player_flags(1, 0xde);
                c.show(PLAYER, 1);
                let y = c.bottom(PLAYER, 1) + 5.0;
                c.font_530_both(COMBO, 2);
                c.width(COMBO, 2, 125);
                c.locate(COMBO, 2, x, y);
                c.show(COMBO, 2);
                c.event_set(RADIO, 0, T_CHECKED, self.resource_event);
                c.event_set(RADIO, 1, T_CHECKED, self.resource_event);
                c.check(0);
            }
            _ => {}
        }
    }
}

/// The player attribute type's attributes: the resources, ten variables ("Variable0".."Variable9"), each resource
/// sent and received, the statistics (two of them under the last statistic's name), the player settings.
fn attribute_names<H: SeplHost + ?Sized>(c: &mut Cx<H>) -> Vec<Vec<u8>> {
    let mut out = Vec::new();
    for id in IDS_TRIGGER_RESOURCE_FIRST..=IDS_TRIGGER_RESOURCE_FIRST + 10 {
        out.push(c.string(id));
    }
    let variable = c.string(IDS_VARIABLE);
    for i in 0..10 {
        let mut v = variable.clone();
        v.extend_from_slice(i.to_string().as_bytes());
        out.push(v);
    }
    for id in IDS_RESOURCE_NAME_FIRST..=IDS_RESOURCE_NAME_IRON {
        let name = c.string(id);
        for what in [IDS_SENT, IDS_RECEIVED] {
            let word = c.string(what);
            let mut v = name.clone();
            v.push(b' ');
            v.extend_from_slice(&word);
            out.push(v);
        }
    }
    for id in IDS_BUILDINGS_LOST..=IDS_STAT_FOR_FUTURE_USE6 {
        let k = id - IDS_VARIABLE;
        out.push(c.string(if k != 0x16 && k != 0x17 { id } else { IDS_STAT_FOR_FUTURE_USE6 }));
    }
    for id in IDS_TRIGGER_PLAYER_SETTING_FIRST..=IDS_TRIGGER_PLAYER_SETTING_FIRST + 5 {
        out.push(c.string(id));
    }
    out
}

/// `EESETSCTAlwaysTrue`: its Show hides the condition title, type combo box and the entry's name, duplicate and
/// delete controls; its Hide shows them again.
fn always_true<H: SeplHost + ?Sized>(c: &mut Cx<H>, slot: u32) {
    let shown = match slot {
        EE_SCREENS_SEET_HIDE => true,
        EE_SCREENS_SEET_SHOW => false,
        _ => return,
    };
    for id in EE_SCREENS_SECT_ALWAYS_TRUE_CONTROLS {
        c.form_display(id, shown);
    }
}

/// `EESETSCTChatMessage`: a chat message contains a text.
fn chat<H: SeplHost + ?Sized>(c: &mut Cx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let text = c.get_text(TEXT, 0);
            c.set_effect_text(EE_SCREENS_SEET_S_CHAT, &text);
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(TEXT, 0);
            c.hide(LABEL, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let text = c.effect_text(EE_SCREENS_SEET_S_CHAT);
            c.text(TEXT, 0, &text);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 0, IDS_GAME_CHAT, 4.0, 616.0, 300.0, 20.0);
            c.text(TEXT, 0, b"");
            c.set_rect(TEXT, 0, 14.0, 641.0, 290.0, 20.0);
            c.show(TEXT, 0);
            let ev = c.screen_event(EE_SCREENS_SE_SELECT_NAME);
            c.event_set(TEXT, 0, T_ENTER, ev);
            c.release(ev);
        }
        _ => {}
    }
}

/// `EESETSCTDiplomacy`: a player's diplomacy towards another.
fn diplomacy<H: SeplHost + ?Sized>(c: &mut Cx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            c.e().mPlayer = c.player(0);
            c.e().mOtherPlayer = c.player(1);
            if let Some(s) = c.selection(COMBO, 0) {
                c.e().mDiplomacy = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 0);
            c.hide(PLAYER, 0);
            c.hide(LABEL, 1);
            c.align(LABEL, 1, AL_LEFT);
            c.hide(PLAYER, 1);
            c.hide(LABEL, 2);
            c.align(LABEL, 2, AL_LEFT);
            c.hide(COMBO, 0);
            c.hide(LABEL, 3);
        }
        EE_SCREENS_SEET_LOAD => {
            let e = *c.e();
            c.set_player(0, e.mPlayer);
            c.set_player(1, e.mOtherPlayer);
            c.select(COMBO, 0, e.mDiplomacy);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 0, IDS_DIPLOMACY_CONDITION_DETAILS, 4.0, 616.0, 300.0, 20.0);
            let y = c.bottom(LABEL, 0) + 8.0;
            label(c, 3, IDS_SE_PLAYER, 4.0, y, 150.0, 20.0);
            let y = c.bottom(LABEL, 0) + 30.0;
            c.locate(PLAYER, 0, 4.0, y);
            c.player_flags(0, 0xde);
            c.show(PLAYER, 0);
            let x = c.right(PLAYER, 0) + 10.0;
            let y = c.bottom(LABEL, 0) + 8.0;
            c.set_rect(LABEL, 1, x, y, 323.0, 20.0);
            c.text_id(LABEL, 1, IDS_DIPLOMACY_TOWARDS);
            c.align(LABEL, 1, AL_CENTER);
            c.show(LABEL, 1);
            let x = c.right(PLAYER, 0) + 10.0;
            let y = c.bottom(LABEL, 0) + 30.0;
            c.locate(PLAYER, 1, x, y);
            c.player_flags(1, 0xde);
            c.show(PLAYER, 1);
            let x = c.right(PLAYER, 1) + 2.0;
            c.set_rect(LABEL, 2, x, y, 20.0, 20.0);
            c.text_id(LABEL, 2, IDS_IS);
            c.align(LABEL, 2, AL_CENTER);
            c.show(LABEL, 2);
            let x = c.right(LABEL, 2) + 2.0;
            c.clear(COMBO, 0);
            c.append_ids(COMBO, 0, IDS_ALLIED, IDS_TRIGGER_ENEMY);
            c.width(COMBO, 0, 0x96);
            c.locate(COMBO, 0, x, y);
            c.show(COMBO, 0);
        }
        _ => {}
    }
}

/// A check box of the game condition at (x, y), 200 wide: its text, shown, its checked and unchecked event showing
/// or hiding `controls` with it, yellow.
fn game_check<H: SeplHost + ?Sized>(c: &mut Cx<H>, k: u32, id: u32, x: f32, y: f32, controls: &[(u32, u32)]) {
    c.locate(CHECK, k, x, y);
    c.width(CHECK, k, 200);
    c.text_font(CHECK, k, id, FONT_534);
    c.show(CHECK, k);
    let ev = c.see_event(EE_SCREENS_SEE_CHECK_DISPLAY);
    c.event_set(CHECK, k, T_CHECKED, ev);
    c.event_set(CHECK, k, T_UNCHECKED, ev);
    for &(a, i) in controls {
        c.event_push(ev, a, i);
    }
    c.release(ev);
    c.yellow(CHECK, k);
}

/// `EESETSCTGame`: the difficulty, an elapsed time window and the hour.
fn game<H: SeplHost + ?Sized>(c: &mut Cx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            c.e().mUsesDifficulty = c.checked(CHECK, 0);
            if let Some(s) = c.selection(COMBO, 0) {
                c.e().mDifficulty = s;
            }
            c.e().mUsesElapsedTime = c.checked(CHECK, 1);
            c.e().mMinTime = c.text_int(EDIT, 0) as u32;
            c.e().mMaxTime = c.text_int(EDIT, 1) as u32;
            // a typed minimum above the maximum raises the maximum
            if c.e().mMinTime > c.e().mMaxTime {
                c.e().mMaxTime = c.e().mMinTime;
            }
            c.e().mUsesHour = c.checked(CHECK, 2);
            c.e().mHour = c.slider(0) as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            for (a, k) in [(LABEL, 0), (CHECK, 0), (COMBO, 0), (CHECK, 1), (LABEL, 1), (EDIT, 0), (LABEL, 2), (EDIT, 1)]
            {
                c.hide(a, k);
            }
            c.align(LABEL, 2, AL_LEFT);
            c.align(LABEL, 1, AL_LEFT);
            c.hide(CHECK, 2);
            c.hide(SLIDER, 0);
            c.event_null(SLIDER, 0, T_RANGE);
            c.hide(LABEL, 3);
            for k in [2, 0, 1] {
                c.event_null(CHECK, k, T_CHECKED);
                c.event_null(CHECK, k, T_UNCHECKED);
            }
        }
        EE_SCREENS_SEET_LOAD => {
            let e = *c.e();
            c.set_checked(0, e.mUsesDifficulty);
            c.select(COMBO, 0, e.mDifficulty);
            c.set_checked(1, e.mUsesElapsedTime);
            c.number(EDIT, 0, e.mMinTime as i32);
            c.number(EDIT, 1, e.mMaxTime as i32);
            c.set_checked(2, e.mUsesHour);
            c.set_slider(0, e.mHour as i32);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 0, IDS_GAME_CONDITION_DETAILS, 4.0, 616.0, 300.0, 20.0);
            let x = c.left(LABEL, 0) + 10.0;
            let y = c.bottom(LABEL, 0) + 10.0;
            game_check(c, 0, IDS_DIFFICULTY_LEVEL_AT, x, y, &[(COMBO, 0)]);
            let y = c.bottom(CHECK, 0) + 3.0;
            c.clear(COMBO, 0);
            c.append_ids(COMBO, 0, IDS_TRIGGER_EASY, IDS_TRIGGER_HARD);
            c.width(COMBO, 0, 200);
            c.locate(COMBO, 0, x, y);
            c.show(COMBO, 0);

            let x = c.right(COMBO, 0) + 10.0;
            let y = c.top(CHECK, 0);
            game_check(c, 1, IDS_ELAPSED_TIME_IS, x, y, &[(LABEL, 1), (EDIT, 0), (LABEL, 2), (EDIT, 1)]);

            let x = c.left(CHECK, 1) + 40.0;
            let y = c.bottom(CHECK, 1) + 3.0;
            c.set_rect(LABEL, 1, x, y, 60.0, 20.0);
            c.text_id(LABEL, 1, IDS_TRIGGER_EDITOR_MIN);
            c.align(LABEL, 1, AL_CENTER);
            c.show(LABEL, 1);
            let y = c.bottom(LABEL, 1) + 3.0;
            c.locate(EDIT, 0, x, y);
            c.number(EDIT, 0, 0);
            c.show(EDIT, 0);

            let x = c.right(LABEL, 1) + 10.0;
            let y = c.top(LABEL, 1);
            c.set_rect(LABEL, 2, x, y, 60.0, 20.0);
            c.text_id(LABEL, 2, IDS_TRIGGER_EDITOR_MAX);
            c.align(LABEL, 2, AL_CENTER);
            c.show(LABEL, 2);
            let y = c.bottom(LABEL, 2) + 3.0;
            c.locate(EDIT, 1, x, y);
            c.number(EDIT, 1, 0);
            c.show(EDIT, 1);

            let x = c.right(CHECK, 1) + 10.0;
            let y = c.top(CHECK, 0);
            game_check(c, 2, IDS_HOUR_IS_AT_LEAST, x, y, &[(SLIDER, 0), (LABEL, 3)]);

            let x = c.left(CHECK, 2) + 60.0;
            let y = c.bottom(CHECK, 2) + 3.0;
            c.range(0, 0, 0x17);
            c.locate(SLIDER, 0, x, y);
            c.page_size(0, 1);
            c.show(SLIDER, 0);
            let ev = c.event_new(EE_SCREENS_SEET_EV_SLIDER_FORMAT, [3, 0, 0], [1.0, 0.0], 0);
            c.event_set(SLIDER, 0, T_RANGE, ev);
            c.release(ev);
            let x = c.right(SLIDER, 0) + 2.0;
            c.set_rect(LABEL, 3, x, y, 20.0, 20.0);
            let v = c.slider(0);
            c.number(LABEL, 3, v);
            c.align(LABEL, 3, AL_CENTER);
            c.show(LABEL, 3);
        }
        _ => {}
    }
}

/// `EESETSCTTriggerAttribute`: a trigger is on or has fired.
fn trigger_attribute<H: SeplHost + ?Sized>(c: &mut Cx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(SCEN, 0) {
                c.e().mTrigger = s;
            }
            c.e().mIsTriggerOn = c.checked(CHECK, 0);
            c.e().mHasTriggerFired = c.checked(CHECK, 1);
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(CHECK, 1);
            c.hide(CHECK, 0);
            c.hide(SCEN, 0);
            c.hide(LABEL, 0);
            c.hide(LABEL, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let e = *c.e();
            c.select(SCEN, 0, e.mTrigger);
            c.set_checked(0, e.mIsTriggerOn);
            c.set_checked(1, e.mHasTriggerFired);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 0, IDS_TRIGGER_CONDITION_ATTRIBUTES, 4.0, 616.0, 300.0, 20.0);
            label(c, 1, IDS_TRIGGER_EDITOR_TRIGGER, 14.0, 641.0, 290.0, 20.0);
            c.clear(SCEN, 0);
            c.fill_names(SCEN, 0, NAMES_TRIGGERS, false);
            c.locate(SCEN, 0, 14.0, 666.0);
            c.width(SCEN, 0, 0x122);
            c.show(SCEN, 0);
            c.scen_mode(0, MODE_TRIGGER);
            let x = c.right(SCEN, 0) + 10.0;
            let mut y = 666.0;
            for (k, id) in [(0, IDS_IS_ON), (1, IDS_HAS_FIRED)] {
                c.locate(CHECK, k, x, y);
                c.width(CHECK, k, 100);
                c.text_font(CHECK, k, id, FONT_534);
                c.show(CHECK, k);
                c.yellow(CHECK, k);
                y = c.bottom(CHECK, k) + 10.0;
            }
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host recording each op and answering fixed values.
    #[derive(Default)]
    struct Rec {
        ops: Vec<(u32, [i32; 4])>,
        texts: Vec<Vec<u8>>,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            self.ops.push((op, *i));
            match op {
                EE_SCREENS_SEET_OP_RECT => {
                    *f = [10.0, 20.0, 110.0, 40.0];
                    0
                }
                EE_SCREENS_SEET_OP_SELECTION => 2,
                EE_SCREENS_SEET_OP_PLAYER => 3,
                EE_SCREENS_SEET_OP_TEXT_INT => 7,
                EE_SCREENS_SEET_OP_SLIDER => 9,
                EE_SCREENS_SEET_OP_CHECKED => 1,
                EE_SCREENS_SEET_OP_TECHS => 3,
                EE_SCREENS_SEET_OP_TECH => 100 + i[2],
                EE_SCREENS_SEET_OP_STRING | EE_SCREENS_SEET_OP_TECH_NAME => {
                    let s = format!("s{}", i[2]);
                    if (i[1] as usize) >= s.len() {
                        // SAFETY: the buffer p2 holds i1 bytes
                        unsafe { core::ptr::copy_nonoverlapping(s.as_ptr(), p[2] as *mut u8, s.len()) };
                    }
                    s.len() as i32
                }
                EE_SCREENS_SEET_OP_APPEND | EE_SCREENS_SEET_OP_SET_EFFECT_TEXT => {
                    let n = if op == EE_SCREENS_SEET_OP_APPEND { i[2] } else { i[3] };
                    // SAFETY: p2 holds n bytes
                    let b = unsafe { core::slice::from_raw_parts(p[2] as *const u8, n as usize) };
                    self.texts.push(b.to_vec());
                    0
                }
                _ => 0,
            }
        }
    }

    fn run(ty: u32, slot: u32, fields: &mut EeScenarioConditionFields, types: &ConditionTypes) -> Rec {
        let mut h = Rec::default();
        let mut c = Cx { h: &mut h, controls: 1, effect: 2, fields, me: 0 };
        types.call(&mut c, ty, slot);
        h
    }

    #[test]
    fn game_store_raises_the_maximum_and_reads_the_hour() {
        let mut f = EeScenarioConditionFields::default();
        run(GAME_ATTRIBUTE, EE_SCREENS_SEET_STORE, &mut f, &ConditionTypes::default());
        assert!(f.mUsesDifficulty && f.mUsesElapsedTime && f.mUsesHour);
        assert_eq!((f.mDifficulty, f.mMinTime, f.mMaxTime, f.mHour), (2, 7, 7, 9));
    }

    #[test]
    fn research_lists_once_and_stores_the_picked_technology() {
        let types = ConditionTypes::default();
        let mut f = EeScenarioConditionFields::default();
        let h = run(PLAYER_TECHNOLOGY, EE_SCREENS_SEET_SHOW, &mut f, &types);
        assert_eq!(*types.techs.borrow(), vec![100, 101, 102]);
        assert!(h.ops.iter().any(|(op, i)| *op == EE_SCREENS_SEET_OP_TECHS && i[2] == 1));
        assert_eq!(h.texts, vec![b"s100".to_vec(), b"s101".to_vec(), b"s102".to_vec()]);
        let h = run(PLAYER_TECHNOLOGY, EE_SCREENS_SEET_SHOW, &mut f, &types);
        assert!(!h.ops.iter().any(|(op, _)| *op == EE_SCREENS_SEET_OP_TECHS));
        run(PLAYER_TECHNOLOGY, EE_SCREENS_SEET_STORE, &mut f, &types);
        assert_eq!((f.mPlayer, f.mTechnology), (3, 102));
        // a technology not in the list loads as the first line
        f.mTechnology = 5;
        let h = run(PLAYER_TECHNOLOGY, EE_SCREENS_SEET_LOAD, &mut f, &types);
        assert!(h.ops.iter().any(|(op, i)| *op == EE_SCREENS_SEET_OP_SET_SELECTION && i[2] == 0));
    }

    #[test]
    fn attribute_list_has_the_originals_entries() {
        let types = ConditionTypes::default();
        let mut f = EeScenarioConditionFields::default();
        let h = run(PLAYER_ATTRIBUTE, EE_SCREENS_SEET_SHOW, &mut f, &types);
        // 11 resources, 10 variables, 5 resources sent and received, 30 statistics, 6 settings; both combo boxes
        let n = 11 + 10 + 10 + (IDS_STAT_FOR_FUTURE_USE6 - IDS_BUILDINGS_LOST + 1) as usize + 6;
        assert_eq!(h.texts.len(), 2 * n);
        assert_eq!(h.texts[2 * 11], format!("s{IDS_VARIABLE}0").into_bytes());
        assert_eq!(h.texts[2 * 21], format!("s{IDS_RESOURCE_NAME_FIRST} s{IDS_SENT}").into_bytes());
        // statistics 0x16 and 0x17 past the variable string read the last statistic's name
        let stat = |id: u32| h.texts[2 * (31 + (id - IDS_BUILDINGS_LOST) as usize)].clone();
        assert_eq!(stat(IDS_VARIABLE + 0x16), format!("s{IDS_STAT_FOR_FUTURE_USE6}").into_bytes());
        assert_eq!(stat(IDS_VARIABLE + 0x15), format!("s{}", IDS_VARIABLE + 0x15).into_bytes());
    }

    #[test]
    fn player_attribute_load_checks_and_processes_the_radio() {
        let types = ConditionTypes { resource_event: 77, ..Default::default() };
        let mut f = EeScenarioConditionFields { mUsesPlayerAttribute: true, ..Default::default() };
        let h = run(PLAYER_ATTRIBUTE, EE_SCREENS_SEET_LOAD, &mut f, &types);
        let tail: Vec<_> = h.ops.iter().rev().take(2).collect();
        assert_eq!(tail[1].0, EE_SCREENS_SEET_OP_CHECK);
        assert_eq!(tail[1].1[1], 1);
        assert_eq!(tail[0].0, EE_SCREENS_SEET_OP_EVENT_PROCESS);
    }

    #[test]
    fn always_true_shows_and_hides_the_six_controls() {
        let mut f = EeScenarioConditionFields::default();
        let h = run(ALWAYS_TRUE, EE_SCREENS_SEET_SHOW, &mut f, &ConditionTypes::default());
        assert_eq!(h.ops.len(), 6);
        assert!(h.ops.iter().all(|(op, i)| *op == EE_SCREENS_SEET_OP_FORM_DISPLAY && i[3] == 0));
        let h = run(ALWAYS_TRUE, EE_SCREENS_SEET_STORE, &mut f, &ConditionTypes::default());
        assert!(h.ops.is_empty());
    }
}
