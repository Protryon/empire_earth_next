use super::*;

/// A host whose groups answer distinct values and that records every call.
#[derive(Default)]
struct Fake {
    calls: Vec<String>,
    view: EeScreensGameView,
    observer: bool,
    texts: Vec<(u32, u32, Vec<u8>)>,
}

impl ViewHost for Fake {
    fn view(&mut self, group: u32, arg: u32) -> EeScreensGameView {
        self.calls.push(format!("view {group} {arg:#x}"));
        let mut v = self.view;
        if group == EE_SCREENS_VIEW_PLAYER {
            v.observer = self.observer;
        }
        if group == EE_SCREENS_VIEW_COLOUR {
            v.colour = [arg as f32, 0.5, 0.25];
        }
        v
    }
    fn texts(&mut self, group: u32, arg: u32) -> Vec<u8> {
        self.calls.push(format!("texts {group} {arg:#x}"));
        self.texts.iter().find(|t| t.0 == group && t.1 == arg).map(|t| t.2.clone()).unwrap_or_default()
    }
    fn conductor(&mut self, c: usize, op: Sound, value: u32) -> u32 {
        self.calls.push(format!("conductor {c} {op:?} {value:#x}"));
        7
    }
    fn sound(&mut self, m: usize, op: Sound, value: u32) -> u32 {
        self.calls.push(format!("sound {m} {op:?} {value:#x}"));
        9
    }
    fn chat_log_count(&mut self) -> u32 {
        3
    }
    fn chat_log_text(&mut self, index: u32, which: u32) -> Vec<u8> {
        format!("cl{index}.{which}").into_bytes()
    }
    fn form_exists(&mut self, id: u32) -> bool {
        id == 5
    }
    fn lobby_slot(&mut self, index: u32) -> (u32, bool, u32, Vec<u8>) {
        (0x1234 + index, index == 2, 10 + index, format!("slot{index}").into_bytes())
    }
    fn set_bg_colour(&mut self, control: u32, colour: [f32; 3]) {
        self.calls.push(format!("bg {control} {colour:?}"));
    }
    fn set_bar_colour(&mut self, control: u32, colour: [f32; 3]) {
        self.calls.push(format!("bar {control} {colour:?}"));
    }
}

/// Every read op names a group, and its answer comes from that group's fill alone.
#[test]
fn reads_fill_their_group_once() {
    let mut h = Fake::default();
    h.view.ghosts = 4;
    h.view.speed_low = 0.5;
    h.view.speed_high = 2.0;
    h.view.speed_step = 0.25;
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MG_GHOSTS, 0, 0), Some(4));
    assert_eq!(h.calls, ["view 3 0x0"]);
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MG_SPEED_TABLE, 0, 0), Some(0.25f32.to_bits()));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MG_SPEED_TABLE, 1, 0), Some(0.5f32.to_bits()));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MG_SPEED_TABLE, 2, 0), Some(2.0f32.to_bits()));
    h.calls.clear();
    assert_eq!(op(&mut h, EE_SCREENS_GAME_CH_OBSERVER, 0, 0), Some(0));
    assert_eq!(h.calls, ["view 4 0xffffffff"]);
    // an op of the C++ host's
    assert_eq!(op(&mut h, EE_SCREENS_GAME_PAUSE, 0, 0), None);
}

/// The wheel's and the keys' zoom flags, the float bits, the signed values.
#[test]
fn values_are_the_cases_answers() {
    let v = EeScreensGameView {
        zoom_enabled: true,
        settings_zoom_enabled: false,
        zoom: 1.5,
        difficulty: -1,
        current: 3,
        ..Default::default()
    };
    assert_eq!(value(EE_SCREENS_GAME_CAM_ZOOM_ENABLED, 1, &v), 1);
    assert_eq!(value(EE_SCREENS_GAME_CAM_ZOOM_ENABLED, 0, &v), 0);
    assert_eq!(value(EE_SCREENS_GAME_CAM_ZOOM, 0, &v), 1.5f32.to_bits());
    assert_eq!(value(EE_SCREENS_GAME_DIFFICULTY, 0, &v), 0xffff_ffff);
    assert_eq!(value(EE_SCREENS_GAME_DS_CURRENT, 0, &v), 3);
}

/// `GO_HIDE_RESIGN` reads the world only when the current player does not observe.
#[test]
fn hide_resign_short_circuits() {
    let mut h = Fake { observer: true, ..Default::default() };
    assert_eq!(op(&mut h, EE_SCREENS_GAME_GO_HIDE_RESIGN, 0, 0), Some(1));
    assert_eq!(h.calls, ["view 4 0xffffffff"]);
    let mut h = Fake::default();
    h.view.scenario_game = true;
    assert_eq!(op(&mut h, EE_SCREENS_GAME_GO_HIDE_RESIGN, 0, 0), Some(1));
    assert_eq!(h.calls, ["view 4 0xffffffff", "view 3 0x0"]);
    h.view.scenario_game = false;
    assert_eq!(op(&mut h, EE_SCREENS_GAME_GO_HIDE_RESIGN, 0, 0), Some(0));
}

/// The sound objects' ops go to the manager or the conductor of the sound group; nothing without them.
#[test]
fn sound_ops_on_their_objects() {
    let mut h = Fake::default();
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MASTER_VOLUME, 0x3f00_0000, 0), Some(0));
    assert_eq!(h.calls, ["view 0 0x0"]);
    h.view.sound_manager = 11;
    h.view.conductor = 22;
    h.calls.clear();
    assert_eq!(op(&mut h, EE_SCREENS_GAME_MASTER_VOLUME, 0x3f00_0000, 0), Some(9));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_CHANNELS, 16, 0), Some(0));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_SOUND_STOP, 5, 0), Some(0));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_GET_CONDUCTOR_STATE, 0, 0), Some(7));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_REVERB, 1, 0), Some(0));
    let calls: Vec<_> = h.calls.iter().filter(|c| !c.starts_with("view")).cloned().collect();
    assert_eq!(
        calls,
        [
            "sound 11 SetMasterVolume 0x3f000000",
            "sound 11 SetMaxChannels 0x10",
            "sound 11 Stop 0x5",
            "conductor 22 State 0x0",
            "conductor 22 SetReverb 0x1"
        ]
    );
}

/// The colour ops: the lobby's from the table, the splash bar's from the slot's colour.
#[test]
fn colour_ops() {
    let mut h = Fake::default();
    assert_eq!(op(&mut h, EE_SCREENS_OP_LE_PLAYER_COLOUR, 40, 6), Some(0));
    assert_eq!(op(&mut h, EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR, 41, 2), Some(0));
    assert_eq!(h.calls, ["view 5 0x6", "bg 40 [6.0, 0.5, 0.25]", "view 5 0xc", "bar 41 [12.0, 0.5, 0.25]"]);
    assert_eq!(op(&mut h, EE_SCREENS_GAME_SS_PLAYER, 2, 0), Some(0x36 | 0x100));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_SS_PLAYER, 1, 0), Some(0x35));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_GO_FORM_EXISTS, 5, 0), Some(1));
    assert_eq!(op(&mut h, EE_SCREENS_GAME_CL_COUNT, 0, 0), Some(3));
}

/// The texts: a group's block and the slot of the text in it.
#[test]
fn texts_from_their_slots() {
    let mut block = Vec::new();
    for i in 0..19 {
        block.extend_from_slice(format!("s{i}").as_bytes());
        block.push(0);
    }
    let mut h = Fake {
        texts: vec![
            (EE_SCREENS_VIEW_TEXTS_SETTINGS, 0, block),
            (EE_SCREENS_VIEW_TEXTS_PLAYER, 4, b"Ann\0Rome\0".to_vec()),
            (EE_SCREENS_VIEW_TEXTS_INTRO, 0, b"b\0h\0hist\0".to_vec()),
        ],
        ..Default::default()
    };
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_RASTERIZER_SETTING, 1).unwrap(), b"s1");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_AUDIO_DEVICE, 0).unwrap(), b"s2");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_CAMPAIGN, 7).unwrap(), b"s9");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_CE_TRANSLATED, 0).unwrap(), b"s18");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_DS_CIVILIZATION, 4).unwrap(), b"Rome");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_SI_HISTORY, 0).unwrap(), b"hist");
    // a missing player's: empty
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_DS_NAME, 5).unwrap(), b"");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_SS_NAME, 3).unwrap(), b"slot3");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_CL_TEXT, 1).unwrap(), b"cl1.1");
    assert_eq!(text(&mut h, EE_SCREENS_TEXT_STRING, 1), None);
}
