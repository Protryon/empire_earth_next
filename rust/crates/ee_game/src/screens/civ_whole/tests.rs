//! The builder's tests over a fake host: forms with labels, edit boxes, file lists and upgrade lists (real lines
//! and selections), a civilization manager of a few categories, one civilization, in-memory files, the sounds,
//! messages and questions. Every op is logged.

use std::collections::HashMap;

use super::*;

/// A fake upgrade list (the toolkit's base: lines, selection, auto select, the class's flags).
#[derive(Clone, Debug, Default)]
pub(crate) struct FakeList {
    pub lines: Vec<Vec<u8>>,
    pub selection: u32,
    pub has: bool,
    pub auto: bool,
    pub flags: [bool; 4],
    pub dirty: bool,
}

impl FakeList {
    pub(crate) fn new() -> Self {
        Self { flags: [true, true, true, false], ..Self::default() }
    }
}

/// A file list: its lines and selection.
pub(crate) type FileList = (Vec<Vec<u8>>, Option<u32>);

/// A category: its name string id, cost per pick and levels' (value, bonus).
pub(crate) struct FakeCategory {
    pub name_id: i64,
    pub cost: i64,
    pub levels: Vec<(f32, f32)>,
}

pub(crate) struct Fake {
    pub log: Vec<String>,
    pub forms: Vec<i64>,
    pub stack: Vec<i64>,
    pub labels: HashMap<(i64, i64), Vec<u8>>,
    pub edits: HashMap<(i64, i64), Vec<u8>>,
    pub file_lists: HashMap<(i64, i64), FileList>,
    pub lists: HashMap<usize, FakeList>,
    pub list_at: HashMap<(i64, i64), usize>,
    pub strings: HashMap<i64, Vec<u8>>,
    pub categories: Vec<FakeCategory>,
    pub premade: Vec<Vec<u8>>,
    pub civ_name: Vec<u8>,
    pub civ_purchased: Vec<u32>,
    pub civ_available: i64,
    pub civ_max: i64,
    pub civ_unchanged: bool,
    pub multiplayer: bool,
    pub game_result: i64,
    pub player_civ: usize,
    pub chosen: bool,
    pub locked: bool,
    pub free_camera: bool,
    pub can_buy: bool,
    pub custom: bool,
    pub current: i64,
    pub files: HashMap<Vec<u8>, Vec<u8>>,
    pub file_dir: Vec<u8>,
    pub file_name: Vec<u8>,
    pub file_open: bool,
    pub requests: Vec<(i64, Vec<u8>, Vec<u32>)>,
}

pub(crate) const CIV: usize = 0x1000;

impl Fake {
    pub(crate) fn new() -> Self {
        let mut f = Self {
            log: Vec::new(),
            forms: vec![FORM_BUILDER, FORM_NAME, FORM_SAVE, FORM_LOAD, FORM_CHOICE, 11],
            stack: Vec::new(),
            labels: HashMap::new(),
            edits: HashMap::new(),
            file_lists: HashMap::new(),
            lists: HashMap::new(),
            list_at: HashMap::new(),
            strings: HashMap::new(),
            categories: vec![
                FakeCategory { name_id: 500, cost: 2, levels: vec![(0.2, 0.25), (0.3, 0.05), (0.0, 0.0)] },
                FakeCategory { name_id: 501, cost: 3, levels: vec![(0.1, 0.1), (0.15, 0.2)] },
                FakeCategory { name_id: IDS_SPECIAL_POWERS, cost: 15, levels: vec![(0.01, 0.15)] },
            ],
            premade: vec![b"austria".to_vec()],
            civ_name: b"Old".to_vec(),
            civ_purchased: Vec::new(),
            civ_available: 20,
            civ_max: 20,
            civ_unchanged: true,
            multiplayer: false,
            game_result: 0,
            player_civ: CIV,
            chosen: false,
            locked: false,
            free_camera: false,
            can_buy: false,
            custom: false,
            current: -1,
            files: HashMap::new(),
            file_dir: Vec::new(),
            file_name: Vec::new(),
            file_open: false,
            requests: Vec::new(),
        };
        for (id, s) in [
            (500, "Archers"),
            (501, "Cavalry"),
            (IDS_SPECIAL_POWERS, "Special Powers"),
            (1001, "Attack"),
            (1002, "Hit Points"),
            (1003, "Armor"),
            (IDS_DEFAULT_CIVILIZATION, "Default Civilization"),
            (IDS_CIVILIZATION_NAME, "Civilization Name"),
            (IDS_CUSTOM_CIVILIZATION, "Custom Civilization"),
            (IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE, "overwrite?"),
            (IDS_ERROR_SAVING_FILE, "Error Saving File"),
        ] {
            f.strings.insert(id, s.as_bytes().to_vec());
        }
        f.labels.insert((FORM_BUILDER, 0xc4), Vec::new());
        f.labels.insert((FORM_BUILDER, 0xce), Vec::new());
        f.labels.insert((FORM_BUILDER, 0xd2), Vec::new());
        f.edits.insert((FORM_NAME, 0x19a), Vec::new());
        f.edits.insert((FORM_SAVE, 0x232), Vec::new());
        f.file_lists.insert((FORM_SAVE, 0x1a4), (Vec::new(), None));
        f.file_lists.insert((FORM_LOAD, 0x1a9), (Vec::new(), None));
        for (i, c) in [LIST_AVAILABLE, LIST_BOUGHT].into_iter().enumerate() {
            let p = 0x100 + i;
            f.lists.insert(p, FakeList::new());
            f.list_at.insert((FORM_BUILDER, c), p);
        }
        f
    }

    pub(crate) fn list(&self, control: i64) -> &FakeList {
        &self.lists[&self.list_at[&(FORM_BUILDER, control)]]
    }

    fn all_upgrades(&self) -> Vec<u32> {
        let mut out = Vec::new();
        for (c, cat) in self.categories.iter().enumerate() {
            for l in 1..=cat.levels.len() as u32 {
                out.push(((c as u32) << 16) + l);
            }
        }
        out
    }

    fn level(&self, u: u32) -> Option<(f32, f32)> {
        let cat = self.categories.get((u >> 16) as usize)?;
        let l = (u & 0xffff) as usize;
        Some(if l >= 1 && l <= cat.levels.len() { cat.levels[l - 1] } else { (0.0, 0.0) })
    }
}

fn text_of(t: &[u8]) -> String {
    String::from_utf8_lossy(t).into_owned()
}

impl CbHost for Fake {
    fn op(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64, text: &[u8], out: &mut EeScreensCbOut) -> i64 {
        self.log.push(format!("{op} {p:x} {a} {b} {c} {}", text_of(text)));
        let ok = |found: bool| if found { 0 } else { 3 };
        match op {
            EE_SCREENS_CBW_FORM_EXISTS => self.forms.contains(&a) as i64,
            EE_SCREENS_CBW_EDIT_TEXT => match self.edits.get(&(a, b)) {
                Some(t) => {
                    out.text = t.clone();
                    0
                }
                None => 3,
            },
            EE_SCREENS_CBW_SET_EDIT_TEXT => {
                let found = self.edits.contains_key(&(a, b));
                if found {
                    self.edits.insert((a, b), text.to_vec());
                }
                ok(found)
            }
            EE_SCREENS_CBW_EDIT_SELECT => 0,
            EE_SCREENS_CBW_LABEL_TEXT => match self.labels.get(&(a, b)) {
                Some(t) => {
                    out.text = t.clone();
                    0
                }
                None => 3,
            },
            EE_SCREENS_CBW_SET_LABEL_TEXT | EE_SCREENS_CBW_SET_LABEL_NUMBER => {
                let found = self.labels.contains_key(&(a, b));
                if found {
                    let t =
                        if op == EE_SCREENS_CBW_SET_LABEL_TEXT { text.to_vec() } else { c.to_string().into_bytes() };
                    self.labels.insert((a, b), t);
                }
                ok(found)
            }
            EE_SCREENS_CBW_PUSH => {
                if self.forms.contains(&a) {
                    self.stack.push(a);
                    0
                } else {
                    3
                }
            }
            EE_SCREENS_CBW_CLOSE_FORM => {
                self.stack.pop();
                0
            }
            EE_SCREENS_CBW_CURRENT_IS => (self.stack.last().copied().unwrap_or(self.current) == a) as i64,
            EE_SCREENS_CBW_FILE_LIST_FILL => {
                let names: Vec<Vec<u8>> = {
                    let mut v: Vec<Vec<u8>> = self.files.keys().map(|k| k[..k.len() - 4].to_vec()).collect();
                    v.sort();
                    v
                };
                if let Some(fl) = self.file_lists.get_mut(&(a, b)) {
                    fl.0 = names;
                }
                0
            }
            EE_SCREENS_CBW_FILE_SELECTION => {
                let key = if p != 0 { (p as i64, b) } else { (a, b) };
                match self.file_lists.get(&key) {
                    None => -1,
                    Some((_, Some(i))) => {
                        out.values.push(*i as i64);
                        0
                    }
                    Some((_, None)) => {
                        out.values.push(0);
                        0xd
                    }
                }
            }
            EE_SCREENS_CBW_FILE_LINE => {
                let key = if p != 0 { (p as i64, b) } else { (a, b) };
                out.text = self.file_lists[&key].0[c as usize].clone();
                0
            }
            EE_SCREENS_CBW_FILE_DELETE => {
                let (lines, sel) = self.file_lists[&(a, b)].clone();
                let mut name = lines[sel.unwrap() as usize].clone();
                name.extend_from_slice(b".civ");
                (self.files.remove(&name).is_none()) as i64
            }
            EE_SCREENS_CBW_IS_FORM => (p as i64 == a) as i64,
            EE_SCREENS_CBW_FORM_ID => p as i64,
            EE_SCREENS_CBW_LIST => self.list_at.get(&(a, b)).copied().unwrap_or(0) as i64,
            EE_SCREENS_CBW_FORM_EVENTS | EE_SCREENS_CBW_DISPLAY | EE_SCREENS_CBW_DISPLAY_INDEX => 0,
            EE_SCREENS_CBW_L_COUNT..=EE_SCREENS_CBW_L_DRAW_END => self.list_op(op, p, a, b, c, text, out),
            EE_SCREENS_CBW_STRING => match self.strings.get(&a) {
                Some(t) => {
                    out.text = t.clone();
                    0
                }
                None => 3,
            },
            EE_SCREENS_CBW_PERCENT => {
                let v = f32::from_bits(a as u32) as f64;
                out.text = format!("{v:6.2}%").into_bytes();
                0
            }
            EE_SCREENS_CBW_RESERVED => {
                let lower = text.to_ascii_lowercase();
                let found = self.premade.contains(&lower);
                if found {
                    out.text = format!("reserved '{}'", text_of(text)).into_bytes();
                }
                found as i64
            }
            EE_SCREENS_CBW_LOOKUP_LEVEL => match self.level(a as u32) {
                Some((v, b)) => {
                    out.values.push(v.to_bits() as i64);
                    out.values.push(b.to_bits() as i64);
                    1
                }
                None => 0,
            },
            EE_SCREENS_CBW_EXTRA_COST => self.categories[(a as u32 >> 16) as usize].cost,
            EE_SCREENS_CBW_CATEGORY_NAME_ID => match self.categories.get(a as usize) {
                Some(c) => {
                    out.values.push(c.name_id);
                    1
                }
                None => 0,
            },
            EE_SCREENS_CBW_LEVEL_NAME_ID => 1000 + a,
            EE_SCREENS_CBW_IS_POWER_LEVEL => 0,
            EE_SCREENS_CBW_ALL_UPGRADES => {
                out.values = self.all_upgrades().into_iter().map(|u| u as i64).collect();
                0
            }
            EE_SCREENS_CBW_UPGRADE_KEY => {
                out.text = format!("c{}.l{}", a >> 16, a & 0xffff).into_bytes();
                1
            }
            EE_SCREENS_CBW_QUOTE => {
                out.text = [b"\"", text, b"\""].concat();
                0
            }
            EE_SCREENS_CBW_C_BASE_COST => self.categories[(a as u32 >> 16) as usize].cost,
            EE_SCREENS_CBW_C_EXTRA_COST => self.categories[(a as u32 >> 16) as usize].cost,
            EE_SCREENS_CBW_C_COMPUTE => a + b * c,
            EE_SCREENS_CBW_C_COUNT => self.civ_purchased.iter().filter(|&&u| u >> 16 == a as u32 >> 16).count() as i64,
            EE_SCREENS_CBW_C_AVAILABLE => self.civ_available,
            EE_SCREENS_CBW_C_UNCHANGED => self.civ_unchanged as i64,
            EE_SCREENS_CBW_C_MAX => self.civ_max,
            EE_SCREENS_CBW_C_PURCHASED => {
                out.values = self.civ_purchased.iter().map(|&u| u as i64).collect();
                0
            }
            EE_SCREENS_CBW_C_NAME => {
                out.text = self.civ_name.clone();
                0
            }
            EE_SCREENS_CBW_REQUEST => {
                let (name, rest) = text.split_at(b as usize);
                let ups = rest.chunks(4).map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect();
                self.requests.push((a, name.to_vec(), ups));
                0
            }
            EE_SCREENS_CBW_MULTIPLAYER => self.multiplayer as i64,
            EE_SCREENS_CBW_GAME_RESULT => self.game_result,
            EE_SCREENS_CBW_PAUSE | EE_SCREENS_CBW_UNPAUSE => 0,
            EE_SCREENS_CBW_PLAYER_CIV => self.player_civ as i64,
            EE_SCREENS_CBW_PLAYER_ID => 1,
            EE_SCREENS_CBW_PLAYER_FLAG => (if a == 0 { self.chosen } else { self.locked }) as i64,
            EE_SCREENS_CBW_SET_PLAYER_FLAG => {
                if a == 0 {
                    self.chosen = b != 0;
                } else {
                    self.locked = b != 0;
                }
                0
            }
            EE_SCREENS_CBW_FREE_CAMERA => self.free_camera as i64,
            EE_SCREENS_CBW_CAN_BUY => self.can_buy as i64,
            EE_SCREENS_CBW_CAMPAIGN_NAME => 0,
            EE_SCREENS_CBW_CUSTOM_CIVS => self.custom as i64,
            EE_SCREENS_CBW_SOUND_LOAD => 10 + a,
            EE_SCREENS_CBW_F_DIRECTORY => {
                self.file_dir = text.to_vec();
                0
            }
            EE_SCREENS_CBW_F_NAME => {
                self.file_name = text.to_vec();
                0
            }
            EE_SCREENS_CBW_F_OPEN => {
                let exists = self.files.contains_key(&self.file_name);
                if a == 1 || exists {
                    self.file_open = true;
                    0
                } else {
                    3
                }
            }
            EE_SCREENS_CBW_F_CLOSE => {
                self.file_open = false;
                0
            }
            EE_SCREENS_CBW_F_DELETE => {
                self.files.remove(&self.file_name);
                0
            }
            EE_SCREENS_CBW_F_CREATE => {
                if self.file_name.contains(&b'?') {
                    9
                } else {
                    self.files.insert(self.file_name.clone(), Vec::new());
                    0
                }
            }
            EE_SCREENS_CBW_F_WRITE => {
                self.files.insert(self.file_name.clone(), text.to_vec());
                0
            }
            EE_SCREENS_CBW_F_FILE => {
                out.text = self.file_name.clone();
                0
            }
            EE_SCREENS_CBW_STARTUP_DIRECTORY => {
                out.text = b"C:\\Game\\".to_vec();
                0
            }
            EE_SCREENS_CBW_LOAD_CIVILIZATION => {
                let mut name = text.to_vec();
                name.extend_from_slice(b".civ");
                match self.files.get(&name) {
                    Some(bytes) if bytes.len() >= 8 => {
                        let n = u32::from_le_bytes(bytes[0..4].try_into().unwrap()) as usize;
                        out.text = bytes[4..4 + n].to_vec();
                        let count = u32::from_le_bytes(bytes[4 + n..8 + n].try_into().unwrap()) as usize;
                        for i in 0..count {
                            let at = 8 + n + 4 * i;
                            let legacy = u32::from_le_bytes(bytes[at..at + 4].try_into().unwrap());
                            out.values.push((((legacy >> 6) << 16) | (legacy & 0x3f)) as i64);
                        }
                        0
                    }
                    _ => 3,
                }
            }
            _ => 0,
        }
    }
}

impl Fake {
    #[allow(clippy::too_many_arguments)]
    fn list_op(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64, text: &[u8], out: &mut EeScreensCbOut) -> i64 {
        let l = self.lists.get_mut(&p).expect("a list");
        match op {
            EE_SCREENS_CBW_L_COUNT => l.lines.len() as i64,
            EE_SCREENS_CBW_L_LINE => {
                out.text = l.lines[a as usize].clone();
                0
            }
            EE_SCREENS_CBW_L_SET_LINE => {
                l.lines[a as usize] = text.to_vec();
                0
            }
            EE_SCREENS_CBW_L_APPEND => {
                l.lines.push(text.to_vec());
                0
            }
            EE_SCREENS_CBW_L_DELETE => {
                l.lines.remove(a as usize);
                0
            }
            EE_SCREENS_CBW_L_DELETE_ALL => {
                l.lines.clear();
                0
            }
            EE_SCREENS_CBW_L_SELECT => {
                l.selection = a as u32;
                l.has = !l.lines.is_empty();
                0
            }
            EE_SCREENS_CBW_L_CURRENT => {
                out.values.push(l.selection as i64);
                if !l.lines.is_empty() && l.has {
                    0
                } else {
                    0xd
                }
            }
            EE_SCREENS_CBW_L_AUTO => l.auto as i64,
            EE_SCREENS_CBW_L_HAS => l.has as i64,
            EE_SCREENS_CBW_L_INDEX => l.selection as i64,
            EE_SCREENS_CBW_L_DIRTY => {
                l.dirty = true;
                0
            }
            EE_SCREENS_CBW_L_FLAG => l.flags[a as usize] as i64,
            EE_SCREENS_CBW_L_SET_FLAG => {
                l.flags[a as usize] = b != 0;
                0
            }
            EE_SCREENS_CBW_L_DRAW_BEGIN => l.lines.len() as i64,
            EE_SCREENS_CBW_L_COPY_LINE => {
                out.text = l.lines[a as usize].clone();
                0
            }
            EE_SCREENS_CBW_L_DRAW => {
                let _ = (b, c);
                0
            }
            _ => 0,
        }
    }
}

fn names(lines: &[Vec<u8>]) -> Vec<String> {
    lines.iter().map(|l| text_of(l)).collect()
}

/// The language's string ids these modules use, against include/ee/EEStringIDs.h.
#[test]
fn string_ids_match_the_header() {
    let header = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EEStringIDs.h"))
        .expect("EEStringIDs.h");
    let id = |name: &str| -> i64 {
        let line = header.lines().find(|l| l.split_whitespace().nth(1) == Some(name)).expect(name);
        line.split_whitespace().nth(2).unwrap().parse().unwrap()
    };
    assert_eq!(IDS_CIVILIZATION_NAME, id("IDS_CIVILIZATION_NAME"));
    assert_eq!(IDS_ERROR_DELETING_FILE, id("IDS_ERROR_DELETING_FILE"));
    assert_eq!(IDS_ERROR_CREATING_FILE, id("IDS_ERROR_CREATING_FILE"));
    assert_eq!(IDS_ERROR_SAVING_FILE, id("IDS_ERROR_SAVING_FILE"));
    assert_eq!(IDS_INVALID_FILE_NAME, id("IDS_INVALID_FILE_NAME"));
    assert_eq!(IDS_DEFAULT_CIVILIZATION, id("IDS_DEFAULT_CIVILIZATION"));
    assert_eq!(
        IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE,
        id("IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE")
    );
    assert_eq!(IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE, id("IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE"));
    assert_eq!(IDS_YOU_CANT_LOAD_CIVILIZATION_AFTER, id("IDS_YOU_CANT_LOAD_CIVILIZATION_AFTER"));
    assert_eq!(IDS_YOU_HAVE_NOT_SPENT_ALL, id("IDS_YOU_HAVE_NOT_SPENT_ALL"));
    assert_eq!(IDS_CUSTOM_CIVILIZATION, id("IDS_CUSTOM_CIVILIZATION"));
    assert_eq!(IDS_SPECIAL_POWERS, id("IDS_SPECIAL_POWERS"));
}

/// The legacy file of the gate's script (`cb put cbgatelegacy`) and the ids' conversion.
#[test]
fn legacy_file_bytes() {
    let bytes = legacy_file(b"Legacy", &[(3 << 16) | 1, (5 << 16) | 2]);
    let hex: String = bytes.iter().map(|b| format!("{b:02x}")).collect();
    assert_eq!(hex, "060000004c656761637902000000c100000042010000");
    assert_eq!(to_legacy((0x3ff << 16) | 0x3f), (0xffff, true));
    assert_eq!(to_legacy((0x400 << 16) | 1), (0x10001, false));
    assert_eq!(to_legacy((1 << 16) | 0x40), (0x40, false));
    assert_eq!(legacy_file(b"", &[]), vec![0; 8]);
}

/// The keyed file as the unmodified builder wrote cbgate1 (the gate's file step).
#[test]
fn keyed_file_bytes() {
    let keys: Vec<Vec<u8>> = ["special_powers.adaptation", "aircraft_bombers.attack", "aircraft_fighters.attack"]
        .iter()
        .map(|k| k.as_bytes().to_vec())
        .collect();
    let want = "# a civilization (the civilization builder): its upgrades as <category id>.<attribute id>\n\
eedata: civ\nformat: 3\nname: \"cbgate1\"\nupgrades: [special_powers.adaptation, aircraft_bombers.attack, \
aircraft_fighters.attack]\n";
    assert_eq!(keyed_file(b"\"cbgate1\"", &keys), want.as_bytes());
    assert_eq!(keyed_file(b"\"x\"", &[]), b"# a civilization (the civilization builder): its upgrades as <category id>.<attribute id>\needata: civ\nformat: 3\nname: \"x\"\nupgrades: []\n");
}

/// The creator with points: open, buy two, refused past the points, return one, save, overwrite asked, accept.
/// (One test: the builder's state is the process' own.)
#[test]
fn builder_flow() {
    let mut h = Fake::new();
    // the creator over the civilization (no free camera: points)
    assert_eq!(create_civilization(&mut h, CIV, false), 0);
    let e = editable().expect("an edit");
    assert_eq!(e.creator, Some(false));
    assert_eq!(e.name(), b"Old");
    assert_eq!(h.stack, vec![FORM_BUILDER]);
    assert_eq!(h.labels[&(FORM_BUILDER, 0xc4)], b"Old");
    assert_eq!(h.labels[&(FORM_BUILDER, 0xd2)], b"20");
    assert_eq!(h.labels[&(FORM_BUILDER, 0xce)], b"0");
    // the available list: three categories' headers and their upgrades (level 3 of the first has no value)
    assert_eq!(
        names(&h.list(LIST_AVAILABLE).lines),
        [
            "Special Powers/15/Category",
            "Special Powers/Attack/15/131073/15/15/  1.00%",
            "Archers/2/Category",
            "Archers/Attack/25/1/2/2/ 20.00%",
            "Archers/Hit Points/5/2/2/2/ 30.00%",
            "Cavalry/3/Category",
            "Cavalry/Attack/10/65537/3/3/ 10.00%",
            "Cavalry/Hit Points/20/65538/3/3/ 15.00%",
        ]
    );
    // buy the archers' attack (the selected line), then the hit points by id
    let available = h.list_at[&(FORM_BUILDER, LIST_AVAILABLE)];
    h.lists.get_mut(&available).unwrap().selection = 3;
    h.lists.get_mut(&available).unwrap().has = true;
    assert_eq!(buy_upgrade(&mut h, 0), 0);
    assert_eq!(e.points.get(), 18);
    assert_eq!(names(&h.list(LIST_BOUGHT).lines), ["Archers/2/Category", "Archers/Attack/25/1/2/2/ 20.00%"]);
    assert_eq!(buy_upgrade(&mut h, 2), 0);
    assert_eq!(e.points.get(), 14, "the second of a category costs base + step");
    assert_eq!(h.labels[&(FORM_BUILDER, 0xd2)], b"14");
    assert_eq!(h.labels[&(FORM_BUILDER, 0xce)], b"6");
    // the available list lost the archers (header with them)
    assert_eq!(h.list(LIST_AVAILABLE).lines.len(), 5);
    // the special power costs 15 + 0: refused (14 left), the refused sound
    let before = h.log.len();
    assert_eq!(buy_upgrade(&mut h, 0x20001), 0);
    assert_eq!(e.points.get(), 14);
    assert!(h.log[before..].iter().any(|l| l.starts_with(&format!("{EE_SCREENS_CBW_SOUND_PLAY} 0 11 "))));
    // return the selected bought line (the hit points: refund at one fewer picked)
    let bought = h.list_at[&(FORM_BUILDER, LIST_BOUGHT)];
    let at = h.list(LIST_BOUGHT).lines.iter().position(|l| l.starts_with(b"Archers/Hit")).unwrap() as u32;
    h.lists.get_mut(&bought).unwrap().selection = at;
    h.lists.get_mut(&bought).unwrap().has = true;
    assert_eq!(return_upgrade(&mut h), 0);
    assert_eq!(e.points.get(), 18);
    assert_eq!(*e.upgrades.borrow(), vec![1]);
    // save: the keyed text; then again: the overwrite question (nothing written yet)
    h.edits.insert((FORM_SAVE, 0x232), b"mine".to_vec());
    h.stack.push(FORM_SAVE);
    assert_eq!(save_as(&mut h), 0);
    assert_eq!(state().last_file, b"mine");
    let file = h.files[&b"mine.civ".to_vec()].clone();
    assert_eq!(file, keyed_file(b"\"Old\"", &[b"c0.l1".to_vec()]));
    assert_eq!(h.stack, vec![FORM_BUILDER], "a clean write closes the dialog");
    let before = h.log.len();
    assert_eq!(save_civilization(&mut h, b"mine", false), 0);
    assert!(h.log[before..].iter().any(|l| l.starts_with(&format!("{EE_SCREENS_CBW_QUESTION} 0 0 "))));
    // a reserved name
    assert_eq!(save_civilization(&mut h, b"Austria", false), 9);
    // an invalid name: the message, the file deleted
    let before = h.log.len();
    save_civilization(&mut h, b"bad?", true);
    assert!(
        h.log[before..]
            .iter()
            .any(|l| l.contains("Invalid")
                || l.starts_with(&format!("{EE_SCREENS_CBW_STRING} 0 {IDS_INVALID_FILE_NAME}")))
    );
    assert!(!h.files.contains_key(b"bad?.civ".as_slice()));
    // load a legacy file over the edit: everything returned, its upgrades bought
    h.files.insert(b"leg.civ".to_vec(), legacy_file(b"Leg", &[0x10002]));
    assert_eq!(load_civilization(&mut h, b"leg"), 0);
    assert_eq!(e.name(), b"Leg");
    assert_eq!(*e.upgrades.borrow(), vec![0x10002]);
    assert!(state().premade_loaded);
    // accept: the creator applies nothing; the builder closed
    accept_changes(&mut h);
    assert!(h.requests.is_empty());
    assert!(h.stack.is_empty());
    free_editable();
    assert!(editable().is_none());

    // ---- the edit of the player's civilization: the timer with custom civilizations, then accept with a name
    h.custom = true;
    h.free_camera = false;
    h.can_buy = false;
    assert_eq!(process_timer_expired(&mut h), 0);
    let e = editable().expect("an edit");
    assert_eq!(e.creator, None);
    assert!(h.chosen);
    e.set_name(b"New");
    assert!(e.has_choices(&mut h));
    accept_changes(&mut h);
    assert_eq!(h.requests, vec![(1, b"New".to_vec(), vec![])]);
    free_editable();

    // ---- the timer without custom civilizations: the choice form, once
    h.custom = false;
    h.chosen = false;
    h.stack.clear();
    assert_eq!(process_timer_expired(&mut h), 0);
    assert_eq!(h.stack, vec![FORM_CHOICE]);
    assert!(h.chosen && h.locked);
    assert_eq!(process_timer_expired(&mut h), 0, "the choice form on top");
    assert_eq!(h.stack, vec![FORM_CHOICE]);
}

/// The unlimited creator counts its costs up; its points left stay 0.
#[test]
fn unlimited_creator() {
    let mut h = Fake::new();
    let e = crate::screens::civ_edit::Editable::new(&mut h, CIV, 0, Some(true));
    assert_eq!(e.points_left(), 0);
    assert!(e.add_upgrade(&mut h, 1));
    assert!(e.add_upgrade(&mut h, 2));
    assert_eq!(e.max_points(&mut h), 2 + 4);
    assert!(e.remove_upgrade(&mut h, 1));
    assert_eq!(e.max_points(&mut h), 6 - 4, "the refund at the count before the removal less one");
    assert!(!e.add_upgrade(&mut h, 0));
    assert!(!e.remove_upgrade(&mut h, 0x30001));
    assert!(e.apply(&mut h));
    assert!(h.requests.is_empty());
}

/// The edit: can change by the game's kind; the costs count the civilization's own upgrades.
#[test]
fn edit_rules() {
    let mut h = Fake::new();
    h.civ_purchased = vec![1];
    let e = crate::screens::civ_edit::Editable::new(&mut h, CIV, 3, None);
    assert_eq!(e.cost(&mut h, 2), 2 + 2);
    assert_eq!(e.refund(&mut h, 2), 2);
    assert_eq!(e.copy_upgrade_ids(&mut h), vec![1]);
    assert!(e.can_change(&mut h));
    h.game_result = 1;
    assert!(!e.can_change(&mut h));
    h.multiplayer = true;
    assert!(e.can_change(&mut h));
    h.civ_unchanged = false;
    assert!(!e.can_change(&mut h));
    assert!(!e.has_choices(&mut h));
    assert!(!e.apply(&mut h));
    e.points.set(-5);
    assert_eq!(e.points_left(), (-5i32) as u32);
    assert!(e.add_upgrade(&mut h, 2), "an unsigned compare: a negative left is plenty");
}
