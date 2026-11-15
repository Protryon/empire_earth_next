//! The media, game and AI effect types over the effect screen's controls (`editor_effects.rs`): `EESETSETMedia`
//! (`EESETSETMediaPlayGraphic`, `EESETSETMediaChangeText`, `EESETSETMediaSendDialog`, `EESETSETMediaSendMessage`),
//! `EESETSETGame` (`EESETSETGameDayNight`, `EESETSETGameCycleTime`, `EESETSETGameCalamity`,
//! `EESETSETGameGraphicEffect`, `EESETSETGameStartCinematic`, `EESETSETGameEndCinematic`,
//! `EESETSETGameAmbientWeather`) and `EESETSETAI` (`EESETSETModifyAIMilitaryTarget`,
//! `EESETSETCreateAIMilitaryGroup`, `EESETSETBuildObject`, `EESETSETMaintainObject`, `EESETSETToggleAI`,
//! `EESETSETSetAIAttribute`, `EESETSETSetFavoriteUnit`).

use core::cell::RefCell;

use crate::editor_effects::*;
use crate::editor_players::SeplHost;

// ---- media

/// The media family (`EESETSETMedia`; its play-sound type always uses the file name).
#[derive(Debug, Default)]
pub struct MediaTypes {
    pub family: Family,
}

impl MediaTypes {
    pub fn call<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        let cur = || self.family.current.get();
        match slot {
            EE_SCREENS_SEET_CHANGE_ACTION => {
                media(c, cur(), EE_SCREENS_SEET_HIDE);
                if let Some(s) = c.selection(COMBO, 0) {
                    self.family.current.set(s);
                }
                media(c, cur(), EE_SCREENS_SEET_SHOW);
                media(c, cur(), EE_SCREENS_SEET_LOAD);
                rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_MEDIA_EFFECT_FIRST));
            }
            EE_SCREENS_SEET_STORE => {
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mMediaAction = s;
                    self.family.current.set(s);
                }
                media(c, cur(), EE_SCREENS_SEET_HIDE);
                media(c, cur(), EE_SCREENS_SEET_STORE);
                c.event_null(COMBO, 0, T_SELECTION);
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(LABEL, 0);
                c.hide(LABEL, 1);
                c.hide(COMBO, 0);
                media(c, cur(), EE_SCREENS_SEET_HIDE);
            }
            EE_SCREENS_SEET_LOAD => {
                let action = c.e().mMediaAction;
                load_action(c, &self.family, action);
                media(c, cur(), EE_SCREENS_SEET_SHOW);
                media(c, cur(), EE_SCREENS_SEET_LOAD);
            }
            EE_SCREENS_SEET_SHOW => {
                show_actions(c, IDS_MEDIA_EFFECT, 206.0, IDS_TRIGGER_MEDIA_EFFECT_FIRST, IDS_SEND_CHAT_MESSAGE)
            }
            EE_SCREENS_SEET_UPDATE_NAME => rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_MEDIA_EFFECT_FIRST)),
            _ => {}
        }
    }
}

/// "File name (include .mp3 or .wav)".
fn file_name_text<H: SeplHost + ?Sized>(c: &mut Fx<H>) -> Vec<u8> {
    let mut text = c.string(IDS_FILE_NAME);
    text.extend_from_slice(&c.string(IDS_INCLUDE_MP3_OR_WAV_EXTENSION));
    text
}

/// A media action's slot: play sound, change text, send dialogue, send chat message.
fn media<H: SeplHost + ?Sized>(c: &mut Fx<H>, k: u32, slot: u32) {
    match (k, slot) {
        // play sound (`EESETSETMediaPlayGraphic`): the file name (`mTextEditBox`)
        (0, EE_SCREENS_SEET_STORE) => {
            let name = c.get_text(TEXT, 0);
            c.set_effect_text(EE_SCREENS_SEET_S_FILE_NAME, &name);
        }
        (0, EE_SCREENS_SEET_HIDE) => {
            c.hide(LABEL, 2);
            c.hide(TEXT, 0);
        }
        (0, EE_SCREENS_SEET_LOAD) => {
            let name = c.effect_text(EE_SCREENS_SEET_S_FILE_NAME);
            c.text(TEXT, 0, &name);
        }
        (0, EE_SCREENS_SEET_SHOW) => {
            let text = file_name_text(c);
            c.set_rect(LABEL, 2, 220.0, 641.0, 250.0, 20.0);
            c.text(LABEL, 2, &text);
            c.show(LABEL, 2);
            c.width(TEXT, 0, 0xfa);
            c.text(TEXT, 0, b"");
            c.locate(TEXT, 0, 220.0, 666.0);
            c.show(TEXT, 0);
        }
        // change text: the text object (`mEffectComboBoxes[1]`), clear or add a line (`mRadioButtons[0..2]`), the line
        (1, EE_SCREENS_SEET_STORE) => {
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mTextObject = s;
            }
            let clear = c.checked(RADIO, 0);
            c.e().mIsClearText = clear;
            if !clear {
                let text = c.get_text(TEXT, 0);
                c.set_effect_text(EE_SCREENS_SEET_S_DESCRIPTION, &text);
            }
        }
        (1, EE_SCREENS_SEET_HIDE) => {
            c.hide(LABEL, 2);
            c.hide(ECOMBO, 1);
            c.hide(RADIO, 0);
            c.hide(RADIO, 1);
            c.hide(LABEL, 3);
            c.hide(TEXT, 0);
            c.event_null(RADIO, 0, T_CHECKED);
            c.event_null(RADIO, 1, T_CHECKED);
        }
        (1, EE_SCREENS_SEET_LOAD) => {
            let (object, clear) = (c.e().mTextObject, c.e().mIsClearText);
            c.select_silent(ECOMBO, 1, object);
            if clear {
                c.check(0);
                c.hide(TEXT, 0);
            } else {
                c.check(1);
                let text = c.effect_text(EE_SCREENS_SEET_S_DESCRIPTION);
                c.text(TEXT, 0, &text);
            }
        }
        (1, EE_SCREENS_SEET_SHOW) => {
            label(c, 2, IDS_TEXT_OBJECT, 220.0, 641.0, 150.0, 20.0);
            c.locate(ECOMBO, 1, 220.0, 666.0);
            c.width(ECOMBO, 1, 0x96);
            c.clear(ECOMBO, 1);
            c.append_ids(ECOMBO, 1, IDS_TRIGGER_HINTS, IDS_TRIGGER_HISTORY);
            c.show(ECOMBO, 1);
            let x = c.right(ECOMBO, 1) + 10.0;
            c.text_font(RADIO, 0, IDS_CLEAR, FONT_534);
            let ev = c.screen_event(EE_SCREENS_SE_CLEAR_ADD_LINE);
            c.event_set(RADIO, 0, T_CHECKED, ev);
            c.yellow(RADIO, 0);
            c.locate(RADIO, 0, x, 641.0);
            c.show(RADIO, 0);
            let y = c.bottom(RADIO, 0) + 1.0;
            c.text_font(RADIO, 1, IDS_ADD_LINE, FONT_534);
            c.event_set(RADIO, 1, T_CHECKED, ev);
            c.release(ev);
            c.yellow(RADIO, 1);
            c.locate(RADIO, 1, x, y);
            c.show(RADIO, 1);
            let (x, y) = (c.left(ECOMBO, 1), c.bottom(ECOMBO, 1) + 5.0);
            label(c, 3, IDS_DESCRIPTION_76A8, x, y, 150.0, 20.0);
            let x = c.left(RADIO, 0);
            c.width(TEXT, 0, 0xfa);
            c.text(TEXT, 0, b"");
            c.locate(TEXT, 0, x, y);
            c.show(TEXT, 0);
        }
        // send dialogue (`EESETSETMediaSendDialog`)
        (2, EE_SCREENS_SEET_STORE) => {
            if let Some(s) = c.selection(SCEN, 1) {
                c.e().mObject = s;
            }
            let p = c.player(0);
            c.e().mMessagePlayer = p;
            let d = c.text_int(EDIT, 0);
            c.e().mDuration = d as u32;
            let message = c.get_all_text(EDIT2, 1);
            c.set_effect_text(EE_SCREENS_SEET_S_MESSAGE, &message);
            let file = c.get_text(TEXT, 0);
            c.set_effect_text(EE_SCREENS_SEET_S_FILE_NAME, &file);
            let v = c.slider(0);
            c.e().mIntValue1 = v.wrapping_sub(1);
        }
        (2, EE_SCREENS_SEET_HIDE) => {
            c.hide(LABEL, 2);
            c.hide(PLAYER, 0);
            c.hide(LABEL, 3);
            c.hide(EDIT, 0);
            c.hide(LABEL, 4);
            c.hide(EDIT2, 1);
            c.hide(TEXT, 0);
            c.hide(LABEL, 5);
            c.hide(LABEL, 6);
            c.hide(SCEN, 1);
            c.hide(SLIDER, 0);
            c.hide(LABEL, 7);
            c.hide(LABEL, 8);
            c.form_display(CONTROL_8, true);
        }
        (2, EE_SCREENS_SEET_LOAD) => {
            let (p, d, o, v) = (c.e().mMessagePlayer, c.e().mDuration, c.e().mObject, c.e().mIntValue1);
            c.set_player_silent(0, p);
            c.number(EDIT, 0, d as i32);
            let message = c.effect_text(EE_SCREENS_SEET_S_MESSAGE);
            c.text(EDIT2, 1, &message);
            c.select_silent(SCEN, 1, o);
            let file = c.effect_text(EE_SCREENS_SEET_S_FILE_NAME);
            c.text(TEXT, 0, &file);
            c.set_slider_if(0, v.wrapping_add(1));
            show_camera_scroll_speed(c);
        }
        (2, EE_SCREENS_SEET_SHOW) => {
            label(c, 2, IDS_SE_PLAYER, 220.0, 616.0, 175.0, 20.0);
            let y = c.bottom(LABEL, 2) + 3.0;
            c.locate(PLAYER, 0, 220.0, y);
            c.player_flags(0, 0x8a);
            c.show(PLAYER, 0);
            let text = file_name_text(c);
            c.set_rect(LABEL, 5, 220.0, 690.0, 175.0, 20.0);
            c.text(LABEL, 5, &text);
            c.show(LABEL, 5);
            let y = c.bottom(LABEL, 5) + 2.0;
            c.width(TEXT, 0, 0xaf);
            c.text(TEXT, 0, b"");
            c.locate(TEXT, 0, 220.0, y);
            c.show(TEXT, 0);
            let x = c.right(PLAYER, 0) + 10.0;
            label(c, 4, IDS_MESSAGE, x, 616.0, 225.0, 20.0);
            let y = c.bottom(LABEL, 4) + 3.0;
            c.locate(EDIT2, 1, x, y);
            c.text(EDIT2, 1, b"");
            c.show(EDIT2, 1);
            let x = c.right(LABEL, 4) + 5.0;
            label(c, 7, IDS_CAMERA_SCROLL_SPEED, x, 616.0, 160.0, 20.0);
            let (x, y) = (c.left(LABEL, 7), c.bottom(LABEL, 7) + 1.0);
            c.set_rect(SLIDER, 0, x, y, 110.0, 20.0);
            c.show(SLIDER, 0);
            let ev = c.event_new(
                EE_SCREENS_SEET_EV_OBJECTS,
                [EE_SCREENS_SEE_MSD_CAMERA_SCROLL as i32, 0, 0],
                [0.0; 2],
                c.me,
            );
            c.event_set(SLIDER, 0, T_RANGE, ev);
            c.release(ev);
            c.range(0, 0, 0xb);
            c.set_slider(0, 0);
            let x2 = c.right(SLIDER, 0) + 1.0;
            c.set_rect(LABEL, 8, x2, y, 49.0, 20.0);
            c.show(LABEL, 8);
            c.align(LABEL, 8, AL_RIGHT);
            let v = c.slider(0);
            c.number(LABEL, 8, v);
            let (x, y) = (c.left(LABEL, 7), c.bottom(LABEL, 8) + 5.0);
            label(c, 3, IDS_DURATION_IN_SECONDS, x, y, 160.0, 20.0);
            let y = c.bottom(LABEL, 3) + 1.0;
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, x, y);
            c.show(EDIT, 0);
            label(c, 6, IDS_TRIGGER_SCREEN_TAB_OBJECT, 10.0, 690.0, 200.0, 20.0);
            let y = c.bottom(LABEL, 6) + 2.0;
            c.locate(SCEN, 1, 10.0, y);
            c.width(SCEN, 1, 200);
            c.clear(SCEN, 1);
            c.scen_mode(1, MODE_OBJECT);
            c.fill_names(SCEN, 1, NAMES_OBJECTS, false);
            c.show(SCEN, 1);
            c.form_display(CONTROL_8, false);
        }
        // send chat message (`EESETSETMediaSendMessage`)
        (3, EE_SCREENS_SEET_STORE) => {
            let p = c.player(0);
            c.e().mMessagePlayer = p;
            let d = c.text_int(EDIT, 0);
            c.e().mDuration = d as u32;
            let message = c.get_all_text(EDIT2, 0);
            c.set_effect_text(EE_SCREENS_SEET_S_MESSAGE, &message);
        }
        (3, EE_SCREENS_SEET_HIDE) => {
            c.hide(LABEL, 2);
            c.hide(PLAYER, 0);
            c.hide(LABEL, 3);
            c.hide(EDIT, 0);
            c.hide(LABEL, 4);
            c.hide(EDIT2, 0);
        }
        (3, EE_SCREENS_SEET_LOAD) => {
            let (p, d) = (c.e().mMessagePlayer, c.e().mDuration);
            c.set_player_silent(0, p);
            c.number(EDIT, 0, d as i32);
            let message = c.effect_text(EE_SCREENS_SEET_S_MESSAGE);
            c.text(EDIT2, 0, &message);
        }
        (3, EE_SCREENS_SEET_SHOW) => {
            label(c, 2, IDS_SE_PLAYER, 220.0, 616.0, 175.0, 20.0);
            let y = c.bottom(LABEL, 2) + 3.0;
            c.locate(PLAYER, 0, 220.0, y);
            c.player_flags(0, 0x8a);
            c.show(PLAYER, 0);
            let y = c.bottom(PLAYER, 0) + 5.0;
            label(c, 3, IDS_DURATION_IN_SECONDS, 220.0, y, 175.0, 20.0);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, 220.0, y);
            c.show(EDIT, 0);
            let x = c.right(PLAYER, 0) + 10.0;
            label(c, 4, IDS_MESSAGE, x, 616.0, 250.0, 20.0);
            let y = c.bottom(LABEL, 4) + 3.0;
            c.text(EDIT2, 0, b"");
            c.locate(EDIT2, 0, x, y);
            c.show(EDIT2, 0);
        }
        _ => {}
    }
}

/// `EESETSETMediaSendDialog::ShowCameraScrollSpeed` (the speed slider's event): "Off" at 0, "Snap" at 1, else the
/// speed (the value - 1) into `mLabels[8]`.
pub fn show_camera_scroll_speed<H: SeplHost + ?Sized>(c: &mut Fx<H>) {
    match c.slider(0) {
        0 => c.text_id(LABEL, 8, IDS_GAME_OFF),
        1 => c.text_id(LABEL, 8, IDS_SNAP),
        v => c.number(LABEL, 8, v.wrapping_sub(1)),
    }
}

// ---- game

/// The game family (`EESETSETGame`): the calamity and graphic effect types' lists, rebuilt by their Show.
#[derive(Debug, Default)]
pub struct GameTypes {
    pub family: Family,
    pub calamities: RefCell<Vec<i32>>,
    pub graphic_effects: RefCell<Vec<i32>>,
}

impl GameTypes {
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
                rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_GAME_EFFECT_FIRST));
            }
            EE_SCREENS_SEET_STORE => {
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mGameAction = s;
                    self.family.current.set(s);
                }
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
                self.sub(c, cur(), EE_SCREENS_SEET_STORE);
                c.event_null(COMBO, 0, T_SELECTION);
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(LABEL, 0);
                c.hide(LABEL, 1);
                c.hide(COMBO, 0);
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
            }
            EE_SCREENS_SEET_LOAD => {
                let action = c.e().mGameAction;
                load_action(c, &self.family, action);
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
            }
            EE_SCREENS_SEET_SHOW => {
                show_actions(c, IDS_GAME_EFFECT, 206.0, IDS_TRIGGER_GAME_EFFECT_FIRST, IDS_AMBIENT_WEATHER)
            }
            EE_SCREENS_SEET_UPDATE_NAME => rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_GAME_EFFECT_FIRST)),
            _ => {}
        }
    }

    fn sub<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, k: u32, slot: u32) {
        match k {
            0 => hour_slider(c, false, slot),
            1 => cycle_time(c, slot),
            2 => place(c, &self.calamities, false, slot),
            3 => place(c, &self.graphic_effects, true, slot),
            4 => hour_slider(c, true, slot),
            6 => ambient_weather(c, slot),
            _ => {}
        }
    }
}

/// `EESETSETGameDayNight` (the hour 0..23) and `EESETSETGameStartCinematic` (`speed`: the game speed 0..29 shown
/// as 0.1..3.0): `mSliders[0]` and its value label `mLabels[3]`; both store into `mDaylightHour`.
fn hour_slider<H: SeplHost + ?Sized>(c: &mut Fx<H>, speed: bool, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let v = c.slider(0);
            c.e().mDaylightHour = v as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            c.align(LABEL, 2, AL_LEFT);
            c.hide(LABEL, 2);
            c.hide(SLIDER, 0);
            c.event_null(SLIDER, 0, T_RANGE);
            c.align(LABEL, 3, AL_LEFT);
            c.hide(LABEL, 3);
        }
        EE_SCREENS_SEET_LOAD => {
            let hour = c.e().mDaylightHour;
            if speed {
                let text = c.format_float(b"%#2.1f\0", (hour as f32 + 1.0) * 0.1);
                c.text(LABEL, 3, &text);
            }
            c.set_slider_if(0, hour as i32);
        }
        EE_SCREENS_SEET_SHOW => {
            c.set_rect(LABEL, 2, 220.0, 641.0, 100.0, 20.0);
            c.text_id(LABEL, 2, if speed { IDS_MSG_GAME_SPEED } else { IDS_24_HOUR_TIME });
            c.align(LABEL, 2, AL_CENTER);
            c.show(LABEL, 2);
            let y = c.bottom(LABEL, 2) + 5.0;
            c.range(0, 0, if speed { 0x1d } else { 0x17 });
            c.locate(SLIDER, 0, 220.0, y);
            c.page_size(0, 1);
            c.show(SLIDER, 0);
            let (format, scale, offset) = if speed { (1, 0.1, 0.1) } else { (0, 1.0, 0.0) };
            let ev = c.event_new(EE_SCREENS_SEET_EV_SLIDER_FORMAT, [3, format, 0], [scale, offset], 0);
            c.event_set(SLIDER, 0, T_RANGE, ev);
            c.release(ev);
            let (x, y) = (c.left(SLIDER, 0) + 40.0, c.bottom(SLIDER, 0) + 5.0);
            c.set_rect(LABEL, 3, x, y, if speed { 40.0 } else { 20.0 }, 20.0);
            let v = c.slider(0);
            c.number(LABEL, 3, v);
            c.align(LABEL, 3, AL_CENTER);
            c.show(LABEL, 3);
        }
        _ => {}
    }
}

/// `EESETSETGameCycleTime`: the day/night cycle time (`mEditBoxes[0]`).
fn cycle_time<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let v = c.text_int(EDIT, 0);
            c.e().mCycleTime = v as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            c.align(LABEL, 2, AL_LEFT);
            c.hide(LABEL, 2);
            c.hide(EDIT, 0);
            c.hide(LABEL, 3);
        }
        EE_SCREENS_SEET_LOAD => {
            let v = c.e().mCycleTime;
            c.number(EDIT, 0, v as i32);
        }
        EE_SCREENS_SEET_SHOW => {
            c.set_rect(LABEL, 2, 220.0, 641.0, 200.0, 20.0);
            c.text_id(LABEL, 2, IDS_TRIGGER_EDITOR_DAY_NIGHT_CYCLE_TIME);
            c.align(LABEL, 2, AL_CENTER);
            c.show(LABEL, 2);
            let (x, y) = (c.left(LABEL, 2) + 50.0, c.bottom(LABEL, 2) + 3.0);
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, x, y);
            c.show(EDIT, 0);
            let x = c.right(EDIT, 0) + 1.0;
            label(c, 3, IDS_SECONDS, x, y, 90.0, 20.0);
        }
        _ => {}
    }
}

/// `EESETSETGameCalamity` and `EESETSETGameGraphicEffect` (`graphic`): the calamity or graphic effect
/// (`mComboBoxes[1]`, its ids in `list`), its place by area or object (`mRadioButtons[0..2]`, `mComboBoxes[2]`).
fn place<H: SeplHost + ?Sized>(c: &mut Fx<H>, list: &RefCell<Vec<i32>>, graphic: bool, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(COMBO, 1) {
                if let Some(&id) = list.borrow().get(s as usize) {
                    c.e().mPlacedEffect = id as u32;
                }
            }
            let area = c.checked(RADIO, 0);
            c.e().mUsesPlaceArea = area;
            if let Some(s) = c.selection(COMBO, 2) {
                if area {
                    c.e().mPlaceArea = s;
                } else {
                    c.e().mPlaceObject = s;
                }
            }
            c.event_null(RADIO, 0, T_CHECKED);
            c.event_null(RADIO, 1, T_CHECKED);
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 2);
            c.hide(COMBO, 1);
            c.hide(RADIO, 0);
            c.hide(RADIO, 1);
            c.hide(LABEL, 3);
            c.hide(COMBO, 2);
            c.event_null(RADIO, 0, T_CHECKED);
            c.event_null(RADIO, 1, T_CHECKED);
        }
        EE_SCREENS_SEET_LOAD => {
            let e = *c.e();
            let pick = |c: &mut Fx<H>| {
                let found = list.borrow().iter().position(|&x| x == e.mPlacedEffect as i32);
                match found {
                    Some(p) => c.select(COMBO, 1, p as u32),
                    None => {
                        if c.line_count(COMBO, 1) > 0 {
                            c.select(COMBO, 1, 0);
                        }
                    }
                }
            };
            let area = e.mUsesPlaceArea;
            let at = if area { e.mPlaceArea } else { e.mPlaceObject };
            if graphic {
                let held = c.event_hold(COMBO, 1, T_SELECTION);
                pick(c);
                c.event_restore(COMBO, 1, T_SELECTION, held);
                place_event(c);
                c.fill_place(3, 2, area);
                let radio = if area { 0 } else { 1 };
                let held = c.event_hold(RADIO, radio, T_CHECKED);
                c.check(radio);
                c.event_restore(RADIO, radio, T_CHECKED, held);
                c.select(COMBO, 2, at);
            } else {
                c.event_null(RADIO, 0, T_CHECKED);
                c.event_null(RADIO, 1, T_CHECKED);
                pick(c);
                if area {
                    c.check(0);
                    c.fill_place(3, 2, true);
                } else {
                    c.fill_place(3, 2, false);
                    c.check(1);
                }
                c.select(COMBO, 2, at);
                place_event(c);
            }
        }
        EE_SCREENS_SEET_SHOW => {
            label(
                c,
                2,
                if graphic { IDS_GAME_GRAPHIC_EFFECT } else { IDS_TRIGGER_EDITOR_CALAMITY },
                220.0,
                641.0,
                200.0,
                20.0,
            );
            c.clear(COMBO, 1);
            list.borrow_mut().clear();
            let entries = if graphic { c.graphic_effects() } else { c.calamities() };
            for (id, name) in entries {
                list.borrow_mut().push(id as i32);
                c.append_id(COMBO, 1, name);
            }
            c.width(COMBO, 1, 200);
            c.locate(COMBO, 1, 220.0, 666.0);
            c.show(COMBO, 1);
            let x = c.right(LABEL, 2) + 10.0;
            radio(c, 0, IDS_USE_AREA, x, 616.0);
            let y = c.bottom(RADIO, 0) + 1.0;
            radio(c, 1, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, x, y);
            c.set_rect(LABEL, 3, x, 666.0, 200.0, 20.0);
            c.text(LABEL, 3, b"");
            c.show(LABEL, 3);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.clear(COMBO, 2);
            c.width(COMBO, 2, 200);
            c.locate(COMBO, 2, x, y);
            c.show(COMBO, 2);
        }
        _ => {}
    }
}

/// The place radios' screen event (`EE_SCREENS_SE_PLACE`, one event on both).
fn place_event<H: SeplHost + ?Sized>(c: &mut Fx<H>) {
    let ev = c.screen_event(EE_SCREENS_SE_PLACE);
    c.event_set(RADIO, 0, T_CHECKED, ev);
    c.event_set(RADIO, 1, T_CHECKED, ev);
    c.release(ev);
}

/// `EESETSETGameAmbientWeather`: on or off (`mCheckBoxes[0]`).
fn ambient_weather<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let on = c.checked(CHECK, 0);
            c.e().mIsOn = on;
        }
        EE_SCREENS_SEET_HIDE => c.hide(CHECK, 0),
        EE_SCREENS_SEET_LOAD => {
            let on = c.e().mIsOn;
            c.set_checked(0, on);
        }
        EE_SCREENS_SEET_SHOW => {
            c.text_font(CHECK, 0, IDS_ACTIVE_AMBIENT_WEATHER, FONT_538);
            c.locate(CHECK, 0, 220.0, 666.0);
            c.width(CHECK, 0, 200);
            c.yellow(CHECK, 0);
            c.show(CHECK, 0);
        }
        _ => {}
    }
}

// ---- AI

/// The AI family (`EESETSETAI`): the military group's two object lists, the AI attributes listed, the favourite
/// air, land and sea unit types.
#[derive(Debug, Default)]
pub struct AiTypes {
    pub family: Family,
    pub to_add: RefCell<Vec<u32>>,
    pub added: RefCell<Vec<u32>>,
    pub attributes: RefCell<Vec<i32>>,
    pub air: RefCell<Vec<i32>>,
    pub land: RefCell<Vec<i32>>,
    pub sea: RefCell<Vec<i32>>,
}

impl AiTypes {
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
                rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_AI_EFFECT_FIRST));
            }
            EE_SCREENS_SEET_STORE => {
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mAIAction = s;
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
                c.hide(COMBO, 0);
                c.hide(PLAYER, 0);
                c.hide(PLAYER, 2);
                c.hide(LABEL, 2);
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
            }
            EE_SCREENS_SEET_LOAD => {
                let (action, p) = (c.e().mAIAction, c.e().mPlayer);
                load_action(c, &self.family, action);
                c.set_player_silent(0, p);
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
            }
            EE_SCREENS_SEET_SHOW => {
                show_actions(c, IDS_AI_EFFECT, 206.0, IDS_TRIGGER_AI_EFFECT_FIRST, IDS_SET_FAVORITE_UNITS);
                label(c, 2, IDS_SE_PLAYER, 220.0, 641.0, 175.0, 20.0);
                c.locate(PLAYER, 0, 220.0, 666.0);
                c.player_flags(0, 0x94);
                c.show(PLAYER, 0);
            }
            EE_SCREENS_SEET_UPDATE_NAME => rename_by_action(c, cur().wrapping_add(IDS_TRIGGER_AI_EFFECT_FIRST)),
            _ => {}
        }
    }

    fn sub<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, k: u32, slot: u32) {
        match k {
            0 => military_target(c, slot),
            1 => self.military_group(c, slot),
            2 | 3 => build_object(c, slot),
            4 => toggle_ai(c, slot),
            5 => self.ai_attribute(c, slot),
            6 => self.favorite_units(c, slot),
            _ => {}
        }
    }

    /// `EESETSETCreateAIMilitaryGroup::AddObject` (`add`: the "to add" list's line clicked, into the "added" list
    /// kept sorted by object) and `RemoveObject` (back).
    pub fn move_object<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, add: bool, line: u32) {
        let (from_list, to_list, from, to) =
            if add { (0, 1, &self.to_add, &self.added) } else { (1, 0, &self.added, &self.to_add) };
        let Some(&object) = from.borrow().get(line as usize) else { return };
        let text = c.list_data(from_list, line);
        let at = to.borrow().iter().position(|&o| object < o).unwrap_or(to.borrow().len());
        c.list_insert(to_list, &text, at as u32);
        to.borrow_mut().insert(at, object);
        c.list_delete(from_list, line);
        from.borrow_mut().remove(line as usize);
    }

    /// `EESETSETCreateAIMilitaryGroup`: attack and favourites (`mCheckBoxes[0..2]`), the target by area or object
    /// (`mRadioButtons[0..2]`, `mScenarioComboBoxes[1]`), the objects to add and added (`mListBoxes[0..2]`).
    fn military_group<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        match slot {
            EE_SCREENS_SEET_STORE => {
                let s = c.selection(SCEN, 1);
                let area = c.checked(RADIO, 0);
                c.e().mUsesTargetArea = area;
                // the original stores an unset selection when there is none: kept then
                if let Some(s) = s {
                    if area {
                        c.e().mTargetArea = s;
                    } else {
                        c.e().mTargetObject = s;
                    }
                }
                let attack = c.checked(CHECK, 0);
                c.e().mIsAttack = attack;
                let favorites = c.checked(CHECK, 1);
                c.e().mUsesFavorites = favorites;
                let added = snapshot(&self.added);
                c.set_objects(&added);
                c.event_null(RADIO, 0, T_CHECKED);
                c.event_null(RADIO, 1, T_CHECKED);
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(RADIO, 0);
                c.hide(RADIO, 1);
                c.hide(SCEN, 1);
                c.hide(LABEL, 3);
                c.hide(CHECK, 1);
                c.hide(CHECK, 0);
                c.hide(LABEL, 4);
                c.hide(LABEL, 5);
                c.align(LABEL, 5, AL_LEFT);
                c.hide(LIST, 0);
                c.hide(LIST, 1);
                c.event_null(LIST, 0, T_BUTTON_UP);
                c.event_null(LIST, 1, T_BUTTON_UP);
                c.event_null(RADIO, 0, T_CHECKED);
                c.event_null(RADIO, 1, T_CHECKED);
                c.form_display(CONTROL_8, true);
            }
            EE_SCREENS_SEET_LOAD => {
                let e = *c.e();
                let area = e.mUsesTargetArea;
                c.check(if area { 0 } else { 1 });
                c.fill_scenario(3, 1, area);
                let held = c.event_hold(SCEN, 1, T_SELECTION);
                c.select(SCEN, 1, if area { e.mTargetArea } else { e.mTargetObject });
                c.event_restore(SCEN, 1, T_SELECTION, held);
                c.set_checked_if(0, e.mIsAttack);
                c.set_checked_if(1, e.mUsesFavorites);
                self.to_add.borrow_mut().clear();
                self.added.borrow_mut().clear();
                c.clear(LIST, 1);
                c.clear(LIST, 0);
                let added = c.objects();
                *self.added.borrow_mut() = added.clone();
                let names = c.names(NAMES_OBJECTS);
                for (i, name) in names.iter().enumerate().skip(1) {
                    let list = if added.contains(&(i as u32)) {
                        1
                    } else {
                        self.to_add.borrow_mut().push(i as u32);
                        0
                    };
                    c.append(LIST, list, name);
                }
                let ev = c.event_new(EE_SCREENS_SEET_EV_USE_OBJECT_AREA, [3, 1, 0], [0.0; 2], 0);
                c.event_set(RADIO, 0, T_CHECKED, ev);
                c.event_set(RADIO, 1, T_CHECKED, ev);
                c.release(ev);
            }
            EE_SCREENS_SEET_SHOW => {
                for (k, id) in [(0, IDS_SE_ATTACK), (1, IDS_FAVORITES)] {
                    let y = if k == 0 { 692.0 } else { c.bottom(CHECK, 0) + 1.0 };
                    c.locate(CHECK, k, 4.0, y);
                    c.text_font(CHECK, k, id, FONT_534);
                    c.width(CHECK, k, 0x55);
                    c.show(CHECK, k);
                    c.yellow(CHECK, k);
                }
                let x = c.right(CHECK, 0) + 5.0;
                radio(c, 0, IDS_USE_AREA, x, 692.0);
                let y = c.bottom(RADIO, 0) + 1.0;
                radio(c, 1, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, x, y);
                let x = c.right(RADIO, 0) + 5.0;
                c.set_rect(LABEL, 3, x, 692.0, 175.0, 20.0);
                c.text(LABEL, 3, b"----");
                c.show(LABEL, 3);
                let y = c.bottom(LABEL, 3) + 3.0;
                c.locate(SCEN, 1, x, y);
                c.width(SCEN, 1, 175);
                c.clear(SCEN, 1);
                c.show(SCEN, 1);
                label(c, 4, IDS_OBJECTS_TO_ADD, 405.0, 616.0, 195.0, 20.0);
                let x = c.right(LABEL, 4) + 1.0;
                c.set_rect(LABEL, 5, x, 616.0, 195.0, 20.0);
                c.text_id(LABEL, 5, IDS_OBJECTS_ADDED);
                c.align(LABEL, 5, AL_RIGHT);
                c.show(LABEL, 5);
                let x = c.left(LABEL, 4);
                c.set_rect(LIST, 0, x, 641.0, 195.0, 90.0);
                c.list_opacity(0, 0.4);
                let ev = c.event_new(
                    EE_SCREENS_SEET_EV_OBJECTS,
                    [EE_SCREENS_SEE_CAMG_MODIFY_OBJECT as i32, 0, 0],
                    [0.0; 2],
                    c.me,
                );
                c.event_set(LIST, 0, T_BUTTON_UP, ev);
                c.yellow(LIST, 0);
                c.show(LIST, 0);
                let x = c.right(LIST, 0) + 1.0;
                c.set_rect(LIST, 1, x, 641.0, 195.0, 90.0);
                c.list_opacity(1, 0.4);
                c.event_set(LIST, 1, T_BUTTON_UP, ev);
                c.release(ev);
                c.yellow(LIST, 1);
                c.show(LIST, 1);
                c.form_display(CONTROL_8, false);
            }
            _ => {}
        }
    }

    /// `EESETSETSetAIAttribute`: the AI variable (`mComboBoxes[1]`, the settable behaviours `attributes`), its two
    /// integer values and its float (`mSliders[0..3]` with their range and value labels).
    fn ai_attribute<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        match slot {
            EE_SCREENS_SEET_STORE => {
                // the original indexes its list with an unset selection when there is none: nothing stored then
                let Some(b) = c.selection(COMBO, 1).and_then(|s| self.attributes.borrow().get(s as usize).copied())
                else {
                    return;
                };
                let scale = c.behavior(b as u32, EE_SCREENS_SEET_B_FLOAT_SCALE);
                c.e().mAIAttribute = b as u32;
                let v = c.text_int(LABEL, 6);
                c.e().mIntValue1 = v;
                let v = c.text_int(LABEL, 9);
                c.e().mIntValue2 = v;
                let v = c.slider(2);
                c.e().mFloatValue = v as f32 / scale as f32;
            }
            EE_SCREENS_SEET_HIDE => {
                for k in [4, 7, 10, 3] {
                    c.hide(LABEL, k);
                }
                c.hide(COMBO, 1);
                for k in 0..3 {
                    c.hide(SLIDER, k);
                }
                for k in [6, 9, 12] {
                    c.hide(LABEL, k);
                }
                for k in [6, 9, 12] {
                    c.align(LABEL, k, AL_LEFT);
                }
                for k in [5, 8, 11] {
                    c.hide(LABEL, k);
                }
                for k in [5, 8, 11] {
                    c.align(LABEL, k, AL_LEFT);
                }
                for k in 0..3 {
                    c.hide(SLIDER, k);
                }
                for k in 0..3 {
                    c.event_null(SLIDER, k, T_RANGE);
                }
                for (a, k) in [
                    (LABEL, 4),
                    (LABEL, 6),
                    (SLIDER, 0),
                    (LABEL, 5),
                    (LABEL, 7),
                    (LABEL, 9),
                    (SLIDER, 1),
                    (LABEL, 8),
                    (LABEL, 10),
                    (LABEL, 12),
                    (SLIDER, 2),
                    (LABEL, 11),
                ] {
                    c.string_table(a, k, 0);
                }
                c.event_null(COMBO, 1, T_SELECTION);
            }
            EE_SCREENS_SEET_LOAD => {
                c.event_null(COMBO, 1, T_SELECTION);
                let e = *c.e();
                let found = self.attributes.borrow().iter().position(|&a| a == e.mAIAttribute as i32);
                let at = found.unwrap_or(0);
                c.select(COMBO, 1, at as u32);
                // the original reads past an empty list: nothing more then
                let Some(b) = self.attributes.borrow().get(at).copied() else { return };
                self.show_ranges(c, at as u32);
                c.set_slider_if(0, e.mIntValue1);
                c.number(LABEL, 6, e.mIntValue1);
                c.set_slider_if(1, e.mIntValue2);
                c.number(LABEL, 9, e.mIntValue2);
                let mut scale = c.behavior(b as u32, EE_SCREENS_SEET_B_FLOAT_SCALE);
                if scale == 0 {
                    scale = 1;
                }
                let v = (e.mFloatValue * scale as f32 + 0.0001f32) as i32;
                c.set_slider_if(2, v);
                let text = c.format_float(b"%g\0", e.mFloatValue);
                c.text(LABEL, 12, &text);
                let ev = c.event_new(
                    EE_SCREENS_SEET_EV_OBJECTS,
                    [EE_SCREENS_SEE_SAA_ATTRIBUTE as i32, 0, 0],
                    [0.0; 2],
                    c.me,
                );
                c.event_set(COMBO, 1, T_SELECTION, ev);
                c.release(ev);
            }
            EE_SCREENS_SEET_SHOW => {
                label(c, 3, IDS_TRIGGER_EDITOR_ATTRIBUTE_7695, 405.0, 641.0, 200.0, 20.0);
                c.locate(COMBO, 1, 405.0, 666.0);
                c.width(COMBO, 1, 200);
                c.clear(COMBO, 1);
                self.attributes.borrow_mut().clear();
                let n = c.op(EE_SCREENS_SEET_OP_BEHAVIOR_COUNT, 0, 0, 0, 0).max(0) as u32;
                for i in 0..n {
                    if c.behavior(i, EE_SCREENS_SEET_B_SETTABLE) == 0 {
                        continue;
                    }
                    c.append_id(COMBO, 1, i + IDS_CP_BEHAVIOR_FIRST);
                    self.attributes.borrow_mut().push(i as i32);
                }
                c.show(COMBO, 1);
                // the three sliders' rows: the name (the second is never shown), range label, slider, value label
                for (k, x, name) in [
                    (0u32, 10.0f32, IDS_SET_INT_VALUE_1),
                    (1, 220.0, IDS_SET_INT_VALUE_2),
                    (2, 405.0, IDS_SET_FLOAT_VALUE),
                ] {
                    let (title, range, value) = (4 + 3 * k, 5 + 3 * k, 6 + 3 * k);
                    c.set_rect(LABEL, title, x, 692.0, 125.0, 20.0);
                    c.text_id(LABEL, title, name);
                    if k != 1 {
                        c.show(LABEL, title);
                    }
                    let x2 = c.right(LABEL, title) + 1.0;
                    c.set_rect(LABEL, range, x2, 692.0, 49.0, 20.0);
                    c.show(LABEL, range);
                    c.align(LABEL, range, AL_RIGHT);
                    let y = c.bottom(LABEL, title) + 1.0;
                    c.set_rect(SLIDER, k, x, y, 125.0, 20.0);
                    c.show(SLIDER, k);
                    let ev = c.slider_event(value, 1.0, 0);
                    c.event_set(SLIDER, k, T_RANGE, ev);
                    c.release(ev);
                    let x2 = c.right(SLIDER, k) + 1.0;
                    c.set_rect(LABEL, value, x2, y, 49.0, 20.0);
                    c.show(LABEL, value);
                    c.align(LABEL, value, AL_RIGHT);
                }
            }
            _ => {}
        }
    }

    /// `EESETSETSetAIAttribute::ShowAttributeRanges` (also the attribute combo box's event): each slider of the
    /// behaviour at `line` given its range, its first value and the behaviour's help, or hidden when its range is
    /// empty.
    pub fn show_ranges<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, line: u32) {
        // the original reads past its list for a line out of range (the event's none): nothing then
        let Some(b) = self.attributes.borrow().get(line as usize).copied() else { return };
        let b = b as u32;
        let help = b.wrapping_add(IDS_CP_BEHAVIOR_HELP_FIRST);
        for k in 0..3u32 {
            let (lo, hi) = (c.behavior(b, 1 + 2 * k as i32), c.behavior(b, 2 + 2 * k as i32));
            let (title, range, value) = (4 + 3 * k, 5 + 3 * k, 6 + 3 * k);
            let controls = [(LABEL, title), (LABEL, value), (SLIDER, k), (LABEL, range)];
            if lo == hi {
                for (a, i) in controls {
                    c.hide(a, i);
                }
                continue;
            }
            c.text(LABEL, range, format!("{lo}:{hi}").as_bytes());
            if k < 2 {
                c.range(k, lo, hi);
                c.set_slider(k, lo);
                let v = c.slider(k);
                c.number(LABEL, value, v);
            } else {
                let mut scale = c.behavior(b, EE_SCREENS_SEET_B_FLOAT_SCALE);
                if scale == 0 {
                    scale = 1;
                }
                c.range(k, lo, hi);
                c.set_slider(k, lo);
                let scale = scale as f32;
                c.slider_modifier(k, 1.0 / scale);
                let v = c.slider(k);
                let text = c.format_float(b"%g\0", v as f32 / scale);
                c.text(LABEL, value, &text);
            }
            for (a, i) in controls {
                c.show(a, i);
            }
            for (a, i) in controls {
                c.string_table(a, i, help);
            }
        }
    }

    /// `EESETSETSetFavoriteUnit`: the favourite air, land and sea unit types (`mComboBoxes[1..4]`).
    fn favorite_units<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        let lists = [(1u32, &self.air), (2, &self.land), (3, &self.sea)];
        match slot {
            EE_SCREENS_SEET_STORE => {
                for (k, list) in lists {
                    let Some(s) = c.selection(COMBO, k) else { continue };
                    let Some(&t) = list.borrow().get(s as usize) else { continue };
                    match k {
                        1 => c.e().mObject = t as u32,
                        2 => c.e().mIntValue1 = t,
                        _ => c.e().mIntValue2 = t,
                    }
                }
            }
            EE_SCREENS_SEET_HIDE => {
                for k in [3, 4, 5] {
                    c.hide(LABEL, k);
                }
                for k in [1, 2, 3] {
                    c.hide(COMBO, k);
                }
            }
            EE_SCREENS_SEET_LOAD => {
                let e = *c.e();
                for (k, list) in lists {
                    let want = match k {
                        1 => e.mObject as i32,
                        2 => e.mIntValue1,
                        _ => e.mIntValue2,
                    };
                    let found = list.borrow().iter().position(|&t| t == want);
                    c.select(COMBO, k, found.map_or(0, |p| p as u32));
                }
            }
            EE_SCREENS_SEET_SHOW => {
                for (_, list) in lists {
                    list.borrow_mut().clear();
                }
                let n = c.op(EE_SCREENS_SEET_OP_UNIT_TYPE_COUNT, 0, 0, 0, 0).max(0);
                for i in 0..n {
                    let (domain, ty) = c.unit_type(i as u32);
                    let list = match domain {
                        EE_SCREENS_SEET_D_AIR => &self.air,
                        EE_SCREENS_SEET_D_SEA => &self.sea,
                        EE_SCREENS_SEET_D_LAND => &self.land,
                        _ => continue,
                    };
                    list.borrow_mut().push(ty);
                }
                let y = c.bottom(PLAYER, 0) + 5.0;
                label(c, 3, IDS_FAVORITE_AIR, 10.0, y, 200.0, 20.0);
                let x = c.right(LABEL, 3) + 10.0;
                label(c, 4, IDS_FAVORITE_LAND, x, y, 200.0, 20.0);
                let x = c.right(LABEL, 4) + 10.0;
                label(c, 5, IDS_FAVORITE_SEA, x, y, 200.0, 20.0);
                for (k, list) in lists {
                    let (x, y) = (c.left(LABEL, k + 2), c.bottom(LABEL, k + 2) + 3.0);
                    c.locate(COMBO, k, x, y);
                    c.width(COMBO, k, 200);
                    c.clear(COMBO, k);
                    for t in snapshot(list) {
                        let name = c.op(EE_SCREENS_SEET_OP_UNIT_NAME, 0, 0, t, 0);
                        c.append_id(COMBO, k, name as u32);
                    }
                    c.show(COMBO, k);
                }
            }
            _ => {}
        }
    }
}

/// `EESETSETModifyAIMilitaryTarget`: add or remove (`mRadioButtons[0..2]`), the target by area or object
/// (`[2..4]`, `mScenarioComboBoxes[1]`), attack or defend (`[4..6]`).
fn military_target<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let s = c.selection(SCEN, 1);
            let area = c.checked(RADIO, 2);
            c.e().mUsesTargetArea = area;
            // the original stores an unset selection when there is none: kept then
            if let Some(s) = s {
                if area {
                    c.e().mTargetArea = s;
                } else {
                    c.e().mTargetObject = s;
                }
            }
            let add = c.checked(RADIO, 0);
            c.e().mIsAddTarget = add;
            let attack = c.checked(RADIO, 4);
            c.e().mIsAttack = attack;
            c.event_null(RADIO, 2, T_CHECKED);
            c.event_null(RADIO, 3, T_CHECKED);
        }
        EE_SCREENS_SEET_HIDE => {
            for k in [0, 1, 3, 2, 4, 5] {
                c.hide(RADIO, k);
            }
            c.hide(SCEN, 1);
            c.hide(LABEL, 3);
            c.event_null(RADIO, 2, T_CHECKED);
            c.event_null(RADIO, 3, T_CHECKED);
        }
        EE_SCREENS_SEET_LOAD => {
            let e = *c.e();
            let held = c.event_hold(SCEN, 1, T_SELECTION);
            let area = e.mUsesTargetArea;
            c.check(if area { 2 } else { 3 });
            c.fill_scenario(3, 1, area);
            c.select(SCEN, 1, if area { e.mTargetArea } else { e.mTargetObject });
            c.scen_mode(1, if area { MODE_AREA } else { MODE_OBJECT });
            c.event_restore(SCEN, 1, T_SELECTION, held);
            c.check(if e.mIsAddTarget { 0 } else { 1 });
            c.check(if e.mIsAttack { 4 } else { 5 });
            let ev = c.event_new(EE_SCREENS_SEET_EV_USE_OBJECT_AREA, [3, 1, 2], [0.0; 2], 0);
            c.event_set(RADIO, 2, T_CHECKED, ev);
            c.event_set(RADIO, 3, T_CHECKED, ev);
            c.release(ev);
        }
        EE_SCREENS_SEET_SHOW => {
            radio(c, 0, IDS_TRIGGER_EDITOR_ADD, 405.0, 641.0);
            let y = c.bottom(RADIO, 0) + 1.0;
            radio(c, 1, IDS_SE_REMOVE, 405.0, y);
            let x = c.right(RADIO, 1) + 5.0;
            radio(c, 4, IDS_SE_ATTACK, x, 641.0);
            let y = c.bottom(RADIO, 4) + 1.0;
            radio(c, 5, IDS_SE_DEFEND, x, y);
            let y = c.bottom(PLAYER, 0) + 5.0;
            radio(c, 2, IDS_USE_AREA, 270.0, y);
            let y = c.bottom(RADIO, 2) + 1.0;
            radio(c, 3, IDS_TRIGGER_EDITOR_USE_OBJECT_76AF, 270.0, y);
            let y = c.bottom(PLAYER, 0) + 5.0;
            c.set_rect(LABEL, 3, 405.0, y, 175.0, 20.0);
            c.text(LABEL, 3, b"----");
            c.show(LABEL, 3);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.locate(SCEN, 1, 405.0, y);
            c.width(SCEN, 1, 0xaf);
            c.clear(SCEN, 1);
            c.show(SCEN, 1);
        }
        _ => {}
    }
}

/// `EESETSETBuildObject` and `EESETSETMaintainObject` (one body): the object (`mScenarioComboBoxes[1]`).
fn build_object<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(SCEN, 1) {
                c.e().mTargetObject = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(SCEN, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let o = c.e().mTargetObject;
            c.select_silent(SCEN, 1, o);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_TRIGGER_SCREEN_TAB_OBJECT, 405.0, 641.0, 200.0, 20.0);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.locate(SCEN, 1, 405.0, y);
            c.width(SCEN, 1, 200);
            c.scen_mode(1, MODE_OBJECT);
            c.clear(SCEN, 1);
            c.fill_names(SCEN, 1, NAMES_OBJECTS, true);
            c.show(SCEN, 1);
        }
        _ => {}
    }
}

/// `EESETSETToggleAI`: on or off (`mRadioButtons[0..2]`) and the AI behaviour (`mComboBoxes[1]`).
fn toggle_ai<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let on = c.checked(RADIO, 0);
            c.e().mIsOn = on;
            if let Some(s) = c.selection(COMBO, 1) {
                c.e().mAIToggle = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(COMBO, 1);
            c.hide(RADIO, 0);
            c.hide(RADIO, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let (on, toggle) = (c.e().mIsOn, c.e().mAIToggle);
            c.check(if on { 0 } else { 1 });
            c.select_silent(COMBO, 1, toggle);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_AI_TOGGLE, 405.0, 641.0, 200.0, 20.0);
            let y = c.bottom(LABEL, 3) + 3.0;
            c.locate(COMBO, 1, 405.0, y);
            c.width(COMBO, 1, 200);
            c.clear(COMBO, 1);
            c.append_ids(COMBO, 1, IDS_TRIGGER_ACTIVE, IDS_TRIGGER_GATHER);
            c.show(COMBO, 1);
            radio(c, 0, IDS_GAME_ON, 405.0, 692.0);
            let y = c.bottom(RADIO, 0) + 1.0;
            radio(c, 1, IDS_GAME_OFF, 405.0, y);
        }
        _ => {}
    }
}
