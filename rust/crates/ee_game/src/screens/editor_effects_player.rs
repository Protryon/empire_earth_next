//! The player effect types (`EESETSETPlayer` and its twelve actions: `EESETSETPlayerVictory` (victory, defeat),
//! `EESETSETPlayerView` (the script camera), `EESETSETPlayerDiplomacy`, `EESETSETPlayerResource`,
//! `EESETSETPlayerTechnology`, `EESETSETPlayerCivPoint`, the camera fades and visibility, `EESETSETMiniMapFlare`) over
//! the effect screen's controls (`editor_effects.rs`).

use core::cell::RefCell;

use crate::screens::editor_effects::*;
use crate::screens::editor_players::SeplHost;

/// `kSMPlayerAction*`.
const SCRIPT_CAMERA: u32 = 1;
const DIPLOMACY: u32 = 3;
const ATTRIBUTE: u32 = 4;
const TECH_TREE: u32 = 5;
const CIV_POINTS: u32 = 6;
const MINI_MAP_FLARE: u32 = 11;

/// The player family: its action and the technology type's list (`mTechnologies`, built on its first Show).
#[derive(Debug, Default)]
pub struct PlayerTypes {
    pub family: Family,
    pub technologies: RefCell<Vec<i32>>,
}

impl PlayerTypes {
    pub fn call<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        let cur = || self.family.current.get();
        match slot {
            EE_SCREENS_SEET_CHANGE_ACTION => {
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
                if let Some(s) = c.selection(COMBO, 0) {
                    self.family.current.set(s);
                }
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
                self.update_name(c);
            }
            EE_SCREENS_SEET_STORE => {
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mPlayerAction = s;
                    self.family.current.set(s);
                }
                let p = c.player(0);
                c.e().mPlayer = p;
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
                self.sub(c, cur(), EE_SCREENS_SEET_STORE);
                c.event_null(COMBO, 0, T_SELECTION);
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(LABEL, 0);
                c.hide(LABEL, 1);
                c.hide(LABEL, 2);
                c.hide(PLAYER, 0);
                c.hide(COMBO, 0);
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
            }
            EE_SCREENS_SEET_LOAD => {
                let action = c.e().mPlayerAction;
                load_action(c, &self.family, action);
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                let p = c.e().mPlayer;
                c.set_player(0, p);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
            }
            EE_SCREENS_SEET_SHOW => {
                show_actions(c, IDS_PLAYER_EFFECT, 210.0, IDS_TRIGGER_PLAYER_EFFECT_FIRST, IDS_MINI_MAP_FLARE);
                label(c, 2, IDS_SE_PLAYER, 220.0, 641.0, 175.0, 20.0);
                c.locate(PLAYER, 0, 220.0, 666.0);
                c.player_flags(0, 0x80);
                c.show(PLAYER, 0);
            }
            EE_SCREENS_SEET_UPDATE_NAME => self.update_name(c),
            _ => {}
        }
    }

    /// `UpdateName`: `<first word of the action> <player> <detail>` (the other player and stance, the attribute,
    /// the technology added or removed, the amount).
    fn update_name<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>) {
        if !default_named(c) {
            return;
        }
        let p = c.player(0);
        let player = player_name(c, p);
        let cur = self.family.current.get();
        let action = c.string(cur.wrapping_add(IDS_TRIGGER_PLAYER_EFFECT_FIRST));
        let mut detail = Vec::new();
        match cur {
            DIPLOMACY => {
                let other = c.player(1);
                detail = player_name(c, other);
                detail.push(b' ');
                detail.extend_from_slice(&c.get_text(ECOMBO, 1));
            }
            ATTRIBUTE => detail = c.get_text(ECOMBO, 1),
            TECH_TREE => {
                detail.extend_from_slice(if c.checked(RADIO, 2) { b" + " } else { b" - " });
                detail.extend_from_slice(&c.get_text(ECOMBO, 1));
            }
            CIV_POINTS => detail = c.get_text(EDIT, 0),
            _ => {}
        }
        let mut name = b"<".to_vec();
        name.extend_from_slice(first_word(&action));
        name.push(b' ');
        name.extend_from_slice(&player);
        name.push(b' ');
        name.extend_from_slice(&detail);
        name.push(b'>');
        rename(c, &name);
    }

    fn sub<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, k: u32, slot: u32) {
        match k {
            SCRIPT_CAMERA => camera(c, slot),
            DIPLOMACY => diplomacy(c, slot),
            ATTRIBUTE => attribute(c, slot),
            TECH_TREE => self.technology(c, slot),
            CIV_POINTS => civ_points(c, slot),
            MINI_MAP_FLARE => flare(c, slot),
            // victory, defeat, the camera fades, visibility on and off
            0..=11 => match slot {
                EE_SCREENS_SEET_LOAD => {
                    let p = c.e().mPlayer;
                    c.set_player_silent(0, p);
                }
                EE_SCREENS_SEET_SHOW => c.player_flags(0, 0xde),
                _ => {}
            },
            _ => {}
        }
    }

    /// `EESETSETPlayerTechnology`: add (`mRadioButtons[2]`) or remove (`[3]`) the technology (`mEffectComboBoxes[1]`).
    fn technology<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        match slot {
            EE_SCREENS_SEET_STORE => {
                // the original indexes its list with an unset selection when there is none: nothing stored then
                if let Some(s) = c.selection(ECOMBO, 1) {
                    if let Some(&t) = self.technologies.borrow().get(s as usize) {
                        c.e().mTechnology = t;
                    }
                }
                let add = c.checked(RADIO, 2);
                c.e().mIsAddTechnology = add;
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(RADIO, 2);
                c.hide(RADIO, 3);
                c.hide(LABEL, 3);
                c.hide(ECOMBO, 1);
            }
            EE_SCREENS_SEET_LOAD => {
                let p = c.e().mPlayer;
                c.set_player_silent(0, p);
                let held = c.event_hold(ECOMBO, 1, T_SELECTION);
                let t = c.e().mTechnology;
                let found = self.technologies.borrow().iter().position(|&x| x == t);
                c.select(ECOMBO, 1, found.map_or(0, |p| p as u32));
                c.event_restore(ECOMBO, 1, T_SELECTION, held);
                let add = c.e().mIsAddTechnology;
                c.check(if add { 2 } else { 3 });
            }
            EE_SCREENS_SEET_SHOW => {
                if self.technologies.borrow().is_empty() {
                    let techs = c.techs();
                    *self.technologies.borrow_mut() = techs;
                }
                radio(c, 2, IDS_TRIGGER_EDITOR_ADD, 405.0, 616.0);
                let y = c.bottom(RADIO, 2) + 1.0;
                radio(c, 3, IDS_SE_REMOVE, 405.0, y);
                label(c, 3, IDS_TECHNOLOGY, 405.0, 666.0, 254.0, 20.0);
                let y = c.bottom(LABEL, 3) + 3.0;
                c.clear(ECOMBO, 1);
                for t in snapshot(&self.technologies) {
                    let line = c.tech_line(t);
                    c.append(ECOMBO, 1, &line);
                }
                c.width(ECOMBO, 1, 0xfe);
                c.locate(ECOMBO, 1, 405.0, y);
                c.show(ECOMBO, 1);
                c.player_flags(0, 0x98);
            }
            _ => {}
        }
    }
}

/// `EESETSETPlayerCivPoint`: the amount (`mEditBoxes[0]`).
fn civ_points<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let n = c.text_int(EDIT, 0);
            c.e().mCivPoints = n as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(EDIT, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let (p, n) = (c.e().mPlayer, c.e().mCivPoints);
            c.set_player_silent(0, p);
            c.number(EDIT, 0, n as i32);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_AMOUNT, 405.0, 641.0, 75.0, 20.0);
            let y = (f64::from(c.bottom(LABEL, 3)) + 5.0) as f32;
            c.number(EDIT, 0, 1);
            c.locate(EDIT, 0, 405.0, y);
            c.show(EDIT, 0);
            c.player_flags(0, 0x88);
        }
        _ => {}
    }
}

/// `EESETSETPlayerDiplomacy`: the other player (`mPlayers[1]`) and the stance (`mEffectComboBoxes[1]`).
fn diplomacy<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let other = c.player(1);
            c.e().mDiplomacyPlayer = other;
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mDiplomacy = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(LABEL, 4);
            c.hide(PLAYER, 1);
            c.hide(LABEL, 5);
            c.hide(ECOMBO, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let (p, other, stance) = (c.e().mPlayer, c.e().mDiplomacyPlayer, c.e().mDiplomacy);
            c.set_player_silent(0, p);
            c.set_player_silent(1, other);
            c.select_silent(ECOMBO, 1, stance);
        }
        EE_SCREENS_SEET_SHOW => {
            c.set_rect(LABEL, 3, 405.0, 666.0, 20.0, 20.0);
            c.text_id(LABEL, 3, IDS_TO);
            c.align(LABEL, 3, AL_CENTER);
            c.show(LABEL, 3);
            let x = c.right(LABEL, 3) + 10.0;
            label(c, 4, IDS_SE_PLAYER, x, 641.0, 175.0, 20.0);
            c.locate(PLAYER, 1, x, 666.0);
            c.player_flags(1, 0x99);
            c.show(PLAYER, 1);
            let y = c.bottom(PLAYER, 1) + 5.0;
            label(c, 5, IDS_TRIGGER_EDITOR_DIPLOMACY, x, y, 100.0, 20.0);
            let y = c.bottom(LABEL, 5) + 3.0;
            c.locate(ECOMBO, 1, x, y);
            c.width(ECOMBO, 1, 100);
            c.clear(ECOMBO, 1);
            c.append_ids(ECOMBO, 1, IDS_ALLIED, IDS_TRIGGER_ENEMY);
            c.show(ECOMBO, 1);
            c.player_flags(0, 0x99);
        }
        _ => {}
    }
}

/// The two statistics listed as "for future use" (`j - 0x79e0` 22 and 23).
const STAT_BASE: u32 = 0x79e0;

/// `EESETSETPlayerResource`: the attribute (`mEffectComboBoxes[1]`), the mode (`mComboBoxes[2]`), the amount
/// (`mEditBoxes[0]`).
fn attribute<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mPlayerAttribute = s;
            }
            if let Some(s) = c.selection(COMBO, 2) {
                c.e().mPlayerAttributeMode = s;
            }
            let n = c.text_int(EDIT, 0);
            c.e().mPlayerAttributeAmount = n as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(ECOMBO, 1);
            c.hide(COMBO, 2);
            c.hide(LABEL, 4);
            c.hide(EDIT, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let p = c.e().mPlayer;
            c.set_player_silent(0, p);
            let held = c.event_hold(ECOMBO, 1, T_SELECTION);
            let a = c.e().mPlayerAttribute;
            if c.selection(ECOMBO, 1) != Some(a) {
                c.select(ECOMBO, 1, a);
            }
            c.event_restore(ECOMBO, 1, T_SELECTION, held);
            let mode = c.e().mPlayerAttributeMode;
            if c.selection(COMBO, 2) != Some(mode) {
                c.select(COMBO, 2, mode);
            }
            let n = c.e().mPlayerAttributeAmount;
            c.number(EDIT, 0, n as i32);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_TRIGGER_EDITOR_ATTRIBUTE, 405.0, 641.0, 75.0, 20.0);
            let x = c.right(LABEL, 3) + 5.0;
            c.clear(ECOMBO, 1);
            for i in 0..=10 {
                c.append_id(ECOMBO, 1, i + IDS_TRIGGER_RESOURCE_FIRST);
            }
            let variable = c.string(IDS_VARIABLE);
            for j in 0..10 {
                let mut line = variable.clone();
                line.extend_from_slice(j.to_string().as_bytes());
                c.append(ECOMBO, 1, &line);
            }
            for j in IDS_RESOURCE_NAME_FIRST..=IDS_RESOURCE_NAME_IRON {
                let first = c.string(j);
                for which in [IDS_SENT, IDS_RECEIVED] {
                    let second = c.string(which);
                    let mut line = first.clone();
                    line.push(b' ');
                    line.extend_from_slice(&second);
                    c.append(ECOMBO, 1, &line);
                }
            }
            for j in IDS_BUILDINGS_LOST..=IDS_STAT_FOR_FUTURE_USE6 {
                let future = matches!(j.wrapping_sub(STAT_BASE), 22 | 23);
                c.append_id(ECOMBO, 1, if future { IDS_STAT_FOR_FUTURE_USE6 } else { j });
            }
            for i in 0..=5 {
                c.append_id(ECOMBO, 1, i + IDS_TRIGGER_PLAYER_SETTING_FIRST);
            }
            c.font_530(ECOMBO, 1);
            c.width(ECOMBO, 1, 0x7d);
            c.locate(ECOMBO, 1, x, 641.0);
            c.show(ECOMBO, 1);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.clear(COMBO, 2);
            c.append_ids(COMBO, 2, IDS_TO_EXACTLY, IDS_TO_DECREASE_BY);
            c.width(COMBO, 2, 0xa5);
            c.locate(COMBO, 2, x, y);
            c.show(COMBO, 2);
            let y = c.bottom(COMBO, 2) + 5.0;
            label(c, 4, IDS_AMOUNT, x, y, 75.0, 20.0);
            let x = c.right(COMBO, 2) - c.get_width(EDIT, 0) as u32 as f32;
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, x, y);
            c.show(EDIT, 0);
            c.player_flags(0, 0x98);
        }
        _ => {}
    }
}

/// The area/object radios (`mRadioButtons[radio]`, `[radio + 1]`) and their label and scenario combo box set from
/// the effect: the area or the object `which` picked with the combo box's event held off.
fn load_area_object<H: SeplHost + ?Sized>(
    c: &mut Fx<H>,
    radio: u32,
    label: u32,
    scen: u32,
    area: bool,
    which: (u32, u32),
) {
    if area {
        c.check(radio);
    } else {
        c.check(radio + 1);
    }
    c.fill_area_object(label, scen, area);
    let held = c.event_hold(SCEN, scen, T_SELECTION);
    c.select(SCEN, scen, if area { which.0 } else { which.1 });
    c.event_restore(SCEN, scen, T_SELECTION, held);
}

/// An area/object radio pair's event (`EESEERust(kind)`, one event on both).
fn area_object_event<H: SeplHost + ?Sized>(c: &mut Fx<H>, radio: u32, kind: u32) {
    let ev = c.see_event(kind);
    c.event_set(RADIO, radio, T_CHECKED, ev);
    c.event_set(RADIO, radio + 1, T_CHECKED, ev);
    c.release(ev);
}

/// "----" over an area/object combo box (`mLabels[k]`, 175 by 18).
fn dashes<H: SeplHost + ?Sized>(c: &mut Fx<H>, k: u32, x: f32, y: f32) {
    c.set_rect(LABEL, k, x, y, 175.0, 18.0);
    c.text(LABEL, k, b"----");
    c.show(LABEL, k);
}

/// The player label and picker moved below (the camera and flare types).
fn player_below<H: SeplHost + ?Sized>(c: &mut Fx<H>, x: f32, label_y: f32, picker_y: f32) {
    c.player_flags(0, 0x88);
    c.locate(LABEL, 2, x, label_y);
    c.locate(PLAYER, 0, x, picker_y);
    c.player_flags(0, 0x88);
}

/// `EESETSETPlayerView` (the script camera): move to (`mRadioButtons[0..2]`, `mScenarioComboBoxes[1]`), face
/// (`[2..4]`, `[2]`), follow and track (`mCheckBoxes[1]`, `[0]`), the scroll speed and zoom sliders (`[0]`, `[1]`).
fn camera<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let area = c.checked(RADIO, 0);
            c.e().mUsesLocationArea = area;
            if let Some(s) = c.selection(SCEN, 1) {
                if area {
                    c.e().mLocationArea = s;
                } else {
                    c.e().mLocationObject = s;
                }
            }
            let area = c.checked(RADIO, 2);
            c.e().mUsesTargetArea = area;
            if let Some(s) = c.selection(SCEN, 2) {
                if area {
                    c.e().mTargetArea = s;
                } else {
                    c.e().mTargetObject = s;
                }
            }
            let follow = c.checked(CHECK, 1);
            c.e().mIsAttack = follow;
            let track = c.checked(CHECK, 0);
            c.e().mUsesFavorites = track;
            let v = c.slider(0);
            c.e().mIntValue1 = v;
            let v = c.slider(1);
            c.e().mIntValue2 = v;
            for k in [1, 0, 3, 2] {
                c.event_null(RADIO, k, T_CHECKED);
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(SCEN, 1);
            c.hide(RADIO, 1);
            c.hide(RADIO, 0);
            c.hide(LABEL, 5);
            c.hide(LABEL, 6);
            c.hide(CHECK, 1);
            c.hide(CHECK, 0);
            c.hide(LABEL, 4);
            c.hide(SCEN, 2);
            c.hide(RADIO, 3);
            c.hide(RADIO, 2);
            c.hide(SLIDER, 0);
            c.hide(SLIDER, 1);
            c.event_null(SLIDER, 0, T_RANGE);
            c.event_null(SLIDER, 1, T_RANGE);
            for k in [7, 9, 8, 10] {
                c.hide(LABEL, k);
            }
            c.form_display(CONTROL_8, true);
            for k in [1, 0, 3, 2] {
                c.event_null(RADIO, k, T_CHECKED);
            }
            c.locate(LABEL, 2, 220.0, 641.0);
            c.locate(PLAYER, 0, 220.0, 666.0);
        }
        EE_SCREENS_SEET_LOAD => {
            let p = c.e().mPlayer;
            c.set_player_silent(0, p);
            let e = *c.e();
            load_area_object(c, 0, 3, 1, e.mUsesLocationArea, (e.mLocationArea, e.mLocationObject));
            load_area_object(c, 2, 4, 2, e.mUsesTargetArea, (e.mTargetArea, e.mTargetObject));
            c.set_checked_if(1, e.mIsAttack);
            c.set_checked_if(0, e.mUsesFavorites);
            c.set_slider_if(0, e.mIntValue1);
            c.fire_range(0);
            c.set_slider_if(1, e.mIntValue2);
            c.fire_range(1);
            area_object_event(c, 0, EE_SCREENS_SEE_USE_AREA_OBJECT);
            area_object_event(c, 2, EE_SCREENS_SEE_FACE_AREA_OBJECT);
        }
        EE_SCREENS_SEET_SHOW => {
            radio(c, 0, IDS_USE_AREA, 220.0, 641.0);
            let y = c.bottom(RADIO, 0) + 1.0;
            radio(c, 1, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, 220.0, y);
            let y = c.bottom(RADIO, 1) + 3.0;
            dashes(c, 3, 220.0, y);
            let y = c.bottom(LABEL, 3) + 1.0;
            c.locate(SCEN, 1, 220.0, y);
            c.width(SCEN, 1, 0xaf);
            c.clear(SCEN, 1);
            c.show(SCEN, 1);
            c.event_null(SCEN, 1, T_SELECTION);
            player_below(c, 10.0, 690.0, 713.0);
            label(c, 5, IDS_MOVE_CAMERA, 220.0, 616.0, 175.0, 20.0);
            let x = c.right(LABEL, 5) + 5.0;
            radio(c, 2, IDS_USE_AREA, x, 641.0);
            let y = c.bottom(RADIO, 2) + 1.0;
            radio(c, 3, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, x, y);
            let (x, y) = (c.right(LABEL, 5) + 5.0, c.bottom(RADIO, 3) + 3.0);
            dashes(c, 4, x, y);
            let y = c.bottom(LABEL, 4) + 1.0;
            c.locate(SCEN, 2, x, y);
            c.width(SCEN, 2, 0xaf);
            c.clear(SCEN, 2);
            c.show(SCEN, 2);
            player_below(c, 10.0, 690.0, 713.0);
            let x = c.right(LABEL, 5) + 5.0;
            label(c, 6, IDS_FACE_CAMERA, x, 616.0, 175.0, 20.0);
            let x = c.right(LABEL, 6) + 5.0;
            for (k, id) in [(1, IDS_FOLLOW), (0, IDS_TRACK)] {
                let x = if k == 1 { x } else { c.right(CHECK, 1) + 10.0 };
                c.locate(CHECK, k, x, 616.0);
                c.text_font(CHECK, k, id, FONT_534);
                c.width(CHECK, k, 100);
                c.show(CHECK, k);
                c.yellow(CHECK, k);
            }
            let (x, y) = (c.right(LABEL, 6) + 5.0, c.bottom(RADIO, 3) + 3.0);
            label(c, 8, IDS_ZOOM_LEVEL, x, y, 140.0, 20.0);
            let x2 = c.right(LABEL, 8) + 1.0;
            c.set_rect(LABEL, 10, x2, y, 70.0, 20.0);
            c.show(LABEL, 10);
            let y = c.bottom(LABEL, 8) + 1.0;
            c.set_rect(SLIDER, 1, x, y, 210.0, 20.0);
            c.range(1, 0, 0x65);
            c.page_size(1, 1);
            c.show(SLIDER, 1);
            let ev = c.slider_event(10, 1.0, -51);
            for (v, s) in [
                (-51.0, &b"Current"[..]),
                (-50.0, b"HighPitch"),
                (-1.0, b"LowPitch"),
                (0.0, b"None"),
                (1.0, b"Close In"),
                (50.0, b"UpFarBack"),
            ] {
                c.event_string(ev, v, s);
            }
            c.event_set(SLIDER, 1, T_RANGE, ev);
            c.release(ev);
            let v = c.slider(1);
            c.number(LABEL, 10, v.wrapping_sub(10));
            label(c, 7, IDS_SCROLL_SPEED, x, 641.0, 140.0, 20.0);
            let x2 = c.right(LABEL, 7) + 1.0;
            c.set_rect(LABEL, 9, x2, 641.0, 70.0, 20.0);
            c.show(LABEL, 9);
            let y = c.bottom(LABEL, 7) + 1.0;
            c.set_rect(SLIDER, 0, x, y, 210.0, 20.0);
            c.range(0, 0, 100);
            c.page_size(0, 1);
            c.show(SLIDER, 0);
            let ev = c.slider_event(9, 1.0, 0);
            c.event_string_id(ev, 0.0, IDS_SNAP);
            c.event_string_id(ev, 1.0, IDS_INGAME_FAST);
            c.event_string_id(ev, 100.0, IDS_INGAME_SLOW);
            c.event_set(SLIDER, 0, T_RANGE, ev);
            c.release(ev);
            let v = c.slider(0);
            c.number(LABEL, 9, v);
            c.form_display(CONTROL_8, false);
        }
        _ => {}
    }
}

/// `EESETSETMiniMapFlare`: the flare's area or object (`mRadioButtons[0..2]`, `mScenarioComboBoxes[1]`).
fn flare<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let area = c.checked(RADIO, 0);
            c.e().mUsesLocationArea = area;
            if let Some(s) = c.selection(SCEN, 1) {
                if area {
                    c.e().mLocationArea = s;
                } else {
                    c.e().mLocationObject = s;
                }
            }
            c.event_null(RADIO, 1, T_CHECKED);
            c.event_null(RADIO, 0, T_CHECKED);
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(SCEN, 1);
            c.hide(RADIO, 1);
            c.hide(RADIO, 0);
            c.event_null(RADIO, 1, T_CHECKED);
            c.event_null(RADIO, 0, T_CHECKED);
            c.form_display(CONTROL_8, true);
        }
        EE_SCREENS_SEET_LOAD => {
            let p = c.e().mPlayer;
            c.set_player_silent(0, p);
            let e = *c.e();
            load_area_object(c, 0, 3, 1, e.mUsesLocationArea, (e.mLocationArea, e.mLocationObject));
            area_object_event(c, 0, EE_SCREENS_SEE_USE_AREA_OBJECT);
        }
        EE_SCREENS_SEET_SHOW => {
            radio(c, 0, IDS_USE_AREA, 430.0, 641.0);
            let y = c.bottom(RADIO, 0) + 1.0;
            radio(c, 1, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, 430.0, y);
            let x = c.right(RADIO, 1) + 10.0;
            dashes(c, 3, x, 641.0);
            let y = c.bottom(LABEL, 3) + 1.0;
            c.locate(SCEN, 1, x, y);
            c.width(SCEN, 1, 0xaf);
            c.clear(SCEN, 1);
            c.show(SCEN, 1);
            c.event_null(SCEN, 1, T_SELECTION);
            player_below(c, 220.0, 641.0, 666.0);
            c.form_display(CONTROL_8, false);
        }
        _ => {}
    }
}
