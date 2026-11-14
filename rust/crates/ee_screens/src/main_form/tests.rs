use super::*;

/// One recorded host op: `(op, a, b)`, the result, the io before and after (io ops), the payload (texts, tiles).
struct Op {
    head: [i64; 4],
    io: Option<(EeScreensMainFormIo, EeScreensMainFormIo)>,
    payload: Option<Vec<u8>>,
}

fn io_of(v: &[i64]) -> EeScreensMainFormIo {
    EeScreensMainFormIo {
        f: [
            f32::from_bits(v[0] as u32),
            f32::from_bits(v[1] as u32),
            f32::from_bits(v[2] as u32),
            f32::from_bits(v[3] as u32),
        ],
        n: [v[4], v[5]],
    }
}

fn parse(field: &str) -> Op {
    let (nums, payload) = match field.find(" x") {
        Some(i) => {
            let hex = &field[i + 2..];
            let bytes = (0..hex.len() / 2).map(|k| u8::from_str_radix(&hex[2 * k..2 * k + 2], 16).unwrap()).collect();
            (&field[..i], Some(bytes))
        }
        None => (field, None),
    };
    let v: Vec<i64> = nums.split(' ').map(|x| x.parse().unwrap()).collect();
    let io = if v.len() == 16 { Some((io_of(&v[4..10]), io_of(&v[10..16]))) } else { None };
    Op { head: [v[0], v[1], v[2], v[3]], io, payload }
}

/// The host's recorded ops of one golden call: each must come in order, answered with its result.
struct Replay {
    ops: Vec<Op>,
    at: usize,
    line: usize,
}

impl Replay {
    fn next(&mut self, op: u32, a: i64, b: i64) -> &Op {
        let line = self.line;
        let at = self.at;
        let want = self.ops.get(at).unwrap_or_else(|| panic!("line {line}: extra op {op}"));
        assert_eq!([op as i64, a, b], want.head[..3], "line {line}: op {at}");
        self.at += 1;
        &self.ops[at]
    }
}

impl FormHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, io: &mut EeScreensMainFormIo) -> i64 {
        let line = self.line;
        let mine = *io;
        let want = self.next(op, a, b);
        if let Some((before, after)) = want.io {
            assert_eq!(mine.f.map(f32::to_bits), before.f.map(f32::to_bits), "line {line}: io in of op {op}");
            assert_eq!(mine.n, before.n, "line {line}: io in of op {op}");
            *io = after;
        }
        want.head[3]
    }
    fn set_text(&mut self, text: &[u8]) {
        let line = self.line;
        let want = self.next(EE_SCREENS_MF_OP_TEXT_SET, text.len() as i64, 0);
        assert_eq!(want.payload.as_deref(), Some(text), "line {line}: the held text");
    }
    fn text(&mut self) -> Vec<u8> {
        let len = self.next(EE_SCREENS_MF_OP_TEXT_GET, 0, 0).head[3];
        self.next(EE_SCREENS_MF_OP_TEXT_GET, len, 0).payload.clone().unwrap()
    }
    fn preview_wall(&mut self, tiles: &[[i32; 2]]) {
        let line = self.line;
        let bytes: Vec<u8> = tiles.iter().flat_map(|t| t.iter().flat_map(|v| v.to_le_bytes())).collect();
        let want = self.next(EE_SCREENS_MF_OP_PREVIEW_WALL, tiles.len() as i64, 0);
        assert_eq!(want.payload.as_deref(), Some(&bytes[..]), "line {line}: the wall's tiles");
    }
}

const GOLDEN: &str = include_str!("../golden_main_form.txt");

/// `build/sc24/mf/driver.cpp 1 N golden`: per call (C: `CopyData`, S: `RefreshStatus`, D: `DisplayChatLine`,
/// I: `ShowInformation`), the new host's ops and answers; the driver checked them against the original's calls
/// (cut from 3fcc1992).
#[test]
fn golden_main_form() {
    let mut kinds = std::collections::BTreeMap::new();
    for (k, line) in GOLDEN.lines().enumerate() {
        let (kind, rest) = line.split_at(1);
        let ops = rest.split('|').skip(1).map(parse).collect();
        let mut h = Replay { ops, at: 0, line: k + 1 };
        match kind {
            "C" => {
                let result = copy_data(&mut h);
                assert_eq!(result, h.ops[h.at - 1].head[3], "line {}: CopyData's result", k + 1);
            }
            "S" => refresh_status(&mut h),
            "D" => display_chat_line(&mut h),
            _ => show_information(&mut h),
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        *kinds.entry(kind.to_string()).or_insert(0) += 1;
    }
    for kind in ["C", "S", "D", "I"] {
        assert!(kinds.get(kind).copied().unwrap_or(0) >= 200, "{kinds:?}");
    }
}

/// Every op and its answers from a table (a -1 answers any a; then 0); the held text, the walls previewed.
#[derive(Default)]
struct Log {
    ops: Vec<(u32, i64, i64)>,
    answers: Vec<(u32, i64, i64)>,
    text: Vec<u8>,
    walls: Vec<Vec<[i32; 2]>>,
    io: EeScreensMainFormIo,
}

impl FormHost for Log {
    fn op(&mut self, op: u32, a: i64, b: i64, io: &mut EeScreensMainFormIo) -> i64 {
        self.ops.push((op, a, b));
        if op == EE_SCREENS_MF_OP_FPS || op == EE_SCREENS_MF_OP_MOUSE_TILE || op == EE_SCREENS_MF_OP_WALL_START {
            *io = self.io;
        }
        self.answers.iter().find(|x| x.0 == op && (x.1 == a || x.1 == -1)).map_or(0, |x| x.2)
    }
    fn set_text(&mut self, text: &[u8]) {
        self.text = text.to_vec();
    }
    fn text(&mut self) -> Vec<u8> {
        self.text.clone()
    }
    fn preview_wall(&mut self, tiles: &[[i32; 2]]) {
        self.walls.push(tiles.to_vec());
    }
}

#[test]
fn ceil_rounds_up_from_nearest() {
    assert_eq!(um_ceil(59.5), 60);
    assert_eq!(um_ceil(60.0), 60);
    assert_eq!(um_ceil(60.4), 61);
    assert_eq!(um_ceil(2.5), 3);
    assert_eq!(um_ceil(-3.5), -3);
}

/// The frame rate's text is the held text (the string's), ": " and the ceiling.
#[test]
fn frame_rate_text() {
    let mut h = Log {
        answers: vec![(EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_CLOCK_MODE, 2)],
        io: EeScreensMainFormIo { f: [59.2, 0.0, 0.0, 0.0], n: [0, 0] },
        ..Default::default()
    };
    refresh_status(&mut h);
    assert!(h.ops.contains(&(EE_SCREENS_MF_OP_TEXT_STRING, EE_SCREENS_MF_IDS_FPS, 0)));
    // (this Log's string op leaves the population's "0/0" held)
    assert_eq!(h.text, b"0/0: 60");
}

/// A text shown whole on the first free line: one Display, no rest.
#[test]
fn chat_shown_whole_once() {
    let mut h = Log {
        answers: vec![(EE_SCREENS_MF_OP_CHAT_LENGTH, -1, 10), (EE_SCREENS_MF_OP_CHAT_DISPLAY, -1, 10)],
        ..Default::default()
    };
    display_chat_line(&mut h);
    let displays: Vec<_> = h.ops.iter().filter(|o| o.0 == EE_SCREENS_MF_OP_CHAT_DISPLAY).collect();
    assert_eq!(displays, [&(EE_SCREENS_MF_OP_CHAT_DISPLAY, EE_SCREENS_MF_CTL_CHAT_LINE1, 0)]);
    assert!(!h.ops.iter().any(|o| o.0 == EE_SCREENS_MF_OP_CHAT_REST));
}

/// The wall mode's drag previews every tile from the start to the mouse's.
#[test]
fn wall_drag_previews_the_line() {
    let mut h = Log {
        answers: vec![
            (EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_UI_STATE, EE_SCREENS_MF_STATE_WALL),
            (EE_SCREENS_MF_OP_FIELD, EE_SCREENS_MF_FIELD_WALL_STATE, 1),
        ],
        io: EeScreensMainFormIo { f: [2.5, 2.5, 0.0, 0.0], n: [5, 2] },
        ..Default::default()
    };
    copy_data(&mut h);
    assert_eq!(h.walls, [vec![[2, 2], [3, 2], [4, 2], [5, 2]]]);
}

const GOLDEN_MODES: &str = include_str!("../golden_main_form_modes.txt");

/// `build/sc25/mf/driver.cpp 1 N golden`: per call (K the constructor, E `SetEntityGroup`, U `SetUIState`, H
/// `SetHotKeyMode`, B `BuildUnit`, G `StartGate`, W `StartWall`, A `StartCalamity`, F `StartFlare`, P `StartPatrol`)
/// with its arguments, the new host's ops and answers; the driver checked them against the original's calls (cut
/// from 7f5a8b0a).
#[test]
fn golden_main_form_modes() {
    let mut kinds = std::collections::BTreeMap::new();
    for (k, line) in GOLDEN_MODES.lines().enumerate() {
        let mut fields = line.split('|');
        let head: Vec<&str> = fields.next().unwrap().split(' ').collect();
        let (kind, a, b) = (head[0], head[1].parse::<i64>().unwrap(), head[2].parse::<i64>().unwrap());
        let ops = fields.map(parse).collect();
        let mut h = Replay { ops, at: 0, line: k + 1 };
        match kind {
            "K" => construct(&mut h),
            "E" => set_entity_group(&mut h, a),
            "U" => set_ui_state(&mut h, a),
            "H" => set_hot_key_mode(&mut h, a),
            "B" => build_unit(&mut h, a, b),
            "G" => start_gate(&mut h, a),
            "W" => start_wall(&mut h, a),
            "A" => start_calamity(&mut h, a),
            "F" => start_targeting(&mut h, EE_SCREENS_MF_STATE_FLARE),
            _ => start_targeting(&mut h, EE_SCREENS_MF_STATE_PATROL),
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        *kinds.entry(kind.to_string()).or_insert(0) += 1;
    }
    for kind in ["K", "E", "U", "H", "B", "G", "W", "A", "F", "P"] {
        assert!(kinds.get(kind).copied().unwrap_or(0) >= 20, "{kinds:?}");
    }
}

/// The hot key table: 40 control group keys, then 73 more, every event a code or none.
#[test]
fn hot_key_table() {
    let keys = hot_keys();
    assert_eq!(keys.len(), 113);
    assert_eq!(
        keys[0].events,
        [EE_SCREENS_HK_SELECT_GROUP, EE_SCREENS_MF_NO_EVENT, EE_SCREENS_MF_NO_EVENT, EE_SCREENS_HK_SELECT_GROUP_ADD]
    );
    assert!(keys.iter().all(|k| k.modifiers[2] == 0x7fff));
    assert_eq!(keys.iter().filter(|k| k.always).count(), 9);
}

const GOLDEN_MEMBERS: &str = include_str!("../golden_main_form_members.txt");

/// `build/sc28/mf/driver.cpp 1 N golden`: per call (a `Activate`, d `Deactivate`, o `Open`, c `Close`, p
/// `DrawPolygons`, m `UpdateBackgroundMedia`, s `ShowAllBackgrounds`, h `HideAllBackgrounds`, x `HideMessages`, t
/// `SetPauseText`, i `Pause`/`ResumeInformation`, q `SetChatTop`, r `RefreshSavedGroups`, u `UpdateIconBar`) with
/// its argument, the new host's ops and answers; the driver checked them against the original's calls (cut from
/// 2cac466f).
#[test]
fn golden_main_form_members() {
    let mut kinds = std::collections::BTreeMap::new();
    for (k, line) in GOLDEN_MEMBERS.lines().enumerate() {
        let mut fields = line.split('|');
        let head: Vec<&str> = fields.next().unwrap().split(' ').collect();
        let (kind, a) = (head[0], head[1].parse::<i64>().unwrap());
        let ops = fields.map(parse).collect();
        let mut h = Replay { ops, at: 0, line: k + 1 };
        let which = match kind {
            "a" => EE_SCREENS_MF_DO_ACTIVATE,
            "d" => EE_SCREENS_MF_DO_DEACTIVATE,
            "o" => EE_SCREENS_MF_DO_OPEN,
            "c" => EE_SCREENS_MF_DO_CLOSE,
            "p" => EE_SCREENS_MF_DO_DRAW,
            "m" => EE_SCREENS_MF_DO_UPDATE_MEDIA,
            "s" => EE_SCREENS_MF_DO_SHOW_BACKGROUNDS,
            "h" => EE_SCREENS_MF_DO_HIDE_BACKGROUNDS,
            "x" => EE_SCREENS_MF_DO_HIDE_MESSAGES,
            "t" => EE_SCREENS_MF_DO_PAUSE_TEXT,
            "i" => EE_SCREENS_MF_DO_INFO_PAUSE,
            "q" => EE_SCREENS_MF_DO_CHAT_TOP,
            "r" => EE_SCREENS_MF_DO_SAVED_GROUPS,
            _ => EE_SCREENS_MF_DO_ICON_BAR,
        };
        let result = member(&mut h, which, a);
        if kind == "p" {
            assert_eq!(result, h.ops[h.at - 1].head[3], "line {}: DrawPolygons' result", k + 1);
        }
        assert_eq!(h.at, h.ops.len(), "line {}: ops missing", k + 1);
        *kinds.entry(kind.to_string()).or_insert(0) += 1;
    }
    for kind in ["a", "d", "o", "c", "p", "m", "s", "h", "x", "t", "i", "q", "r", "u"] {
        assert!(kinds.get(kind).copied().unwrap_or(0) >= 20, "{kinds:?}");
    }
}

/// `HideAllBackgrounds` hides 0x67..=0x7c, then 0x64, 0x65, 0x90, 0x8f, 0x91; `ShowAllBackgrounds` skips 0x6b and
/// the unit panels.
#[test]
fn backgrounds_shown_and_hidden() {
    let mut log = Log::default();
    hide_all_backgrounds(&mut log);
    let hidden: Vec<i64> = log.ops.iter().filter(|o| o.0 == EE_SCREENS_MF_OP_BACKGROUND).map(|o| o.1).collect();
    assert_eq!(hidden.len(), 27);
    assert_eq!(&hidden[22..], &[0x64, 0x65, 0x90, 0x8f, 0x91]);
    let mut log = Log::default();
    show_all_backgrounds(&mut log);
    let shown: Vec<i64> = log.ops.iter().filter(|o| o.0 == EE_SCREENS_MF_OP_BACKGROUND).map(|o| o.1).collect();
    assert!(!shown.contains(&0x6b) && !shown.contains(&0x70) && shown.contains(&0x72));
    assert_eq!(shown.len(), 22 - 4 + 4);
}
