use super::*;

struct Class {
    exists: bool,
    enabled: bool,
    placeable: bool,
    category: i32,
    unit_type: i32,
    name: &'static [u8],
}

const fn class(name: &'static [u8]) -> Class {
    Class { exists: true, enabled: true, placeable: true, category: 1, unit_type: 5, name }
}

#[derive(Default)]
struct Fake {
    classes: Vec<Class>,
    log: Vec<String>,
    combo_selection: u32,
    lists: [(i32, u32); 3],
    second_player_code: Option<i32>,
    keys: [i32; 2],
    tile: bool,
    skipped: Vec<i32>,
    in_view: bool,
    buttons: [bool; 2],
    point: Option<[f32; 2]>,
    list_calls: u32,
}

impl UnitHackHost for Fake {
    fn op(&mut self, op: u32, a: i32, b: i32, f: &mut [f32; 2], out: &mut u32, text: &mut [u8]) -> i32 {
        let c = |i: i32| &self.classes[i as usize];
        match op {
            EE_SCREENS_UH_OP_CLASS_COUNT => self.classes.len() as i32,
            EE_SCREENS_UH_OP_CLASS => {
                let k = c(a);
                if k.exists {
                    1 | (k.enabled as i32) << 1 | (k.placeable as i32) << 2
                } else {
                    0
                }
            }
            EE_SCREENS_UH_OP_CLASS_CATEGORY => c(a).category,
            EE_SCREENS_UH_OP_CLASS_TYPE => c(a).unit_type,
            EE_SCREENS_UH_OP_CLASS_NAME => {
                let name = c(a).name;
                if name.len() <= text.len() {
                    text[..name.len()].copy_from_slice(name);
                }
                name.len() as i32
            }
            EE_SCREENS_UH_OP_COMBO_CLEAR => {
                self.log.push("clear".into());
                0
            }
            EE_SCREENS_UH_OP_COMBO_APPEND => {
                self.log.push(format!("append {}", String::from_utf8_lossy(text)));
                0
            }
            EE_SCREENS_UH_OP_COMBO_SELECT_FIRST => {
                self.log.push("select 0".into());
                0
            }
            EE_SCREENS_UH_OP_COMBO_SELECTION => self.combo_selection as i32,
            EE_SCREENS_UH_OP_LIST_SELECTION => {
                self.list_calls += 1;
                let (code, selection) = self.lists[a as usize];
                *out = selection;
                if a == 2 && self.list_calls > 1 {
                    if let Some(second) = self.second_player_code {
                        return second;
                    }
                }
                code
            }
            EE_SCREENS_UH_OP_TILE => {
                self.log.push(format!("tile {} {}", f[0], f[1]));
                self.tile as i32
            }
            EE_SCREENS_UH_OP_KEY => self.keys[a as usize],
            EE_SCREENS_UH_OP_PLAYER_SKIPPED => self.skipped.contains(&a) as i32,
            EE_SCREENS_UH_OP_LOCAL_PLAYER => 1,
            EE_SCREENS_UH_OP_SEND_PLACE => {
                self.log.push(format!("place {a} {} {} {b}", f[0], f[1]));
                0
            }
            EE_SCREENS_UH_OP_SEND_LOAD => {
                self.log.push(format!("load {a}"));
                0
            }
            EE_SCREENS_UH_OP_IN_VIEW => self.in_view as i32,
            EE_SCREENS_UH_OP_LEFT_BUTTON => self.buttons[a as usize] as i32,
            EE_SCREENS_UH_OP_POINT => {
                self.log.push(format!("point {} {}", f[0], f[1]));
                match self.point {
                    Some(p) => {
                        *f = p;
                        1
                    }
                    None => 0,
                }
            }
            _ => panic!("op {op}"),
        }
    }
}

fn world() -> Fake {
    let mut long = class(b"");
    long.name = b"Zeppelin with a name longer than the first buffer of sixty-four bytes, surely";
    Fake {
        classes: vec![
            class(b"Spearman"),
            Class { exists: false, ..class(b"none") },
            class(b"Archer"),
            Class { enabled: false, ..class(b"Disabled") },
            Class { category: 9, ..class(b"Arrow") },
            Class { category: 0x14, ..class(b"Torpedo") },
            Class { placeable: false, ..class(b"Fixed") },
            Class { unit_type: 0x104, ..class(b"Waypoint") },
            Class { unit_type: 0x168, ..class(b"Foundation") },
            class(b"Archer"),
            class(b"archer"),
            long,
            class(b"Arch"),
        ],
        tile: true,
        in_view: true,
        ..Fake::default()
    }
}

#[test]
fn the_list_is_sorted_by_bytes_and_filtered() {
    let mut h = world();
    let hack = UnitHack::new(&mut h);
    let names: Vec<&[u8]> = hack.names.iter().map(|n| n.as_slice()).collect();
    assert_eq!(
        names,
        [
            &b"Arch"[..],
            b"Archer",
            b"Archer",
            b"Spearman",
            b"Zeppelin with a name longer than the first buffer of sixty-four bytes, surely",
            b"archer"
        ]
    );
    // equal names keep their table order: the later goes after
    assert_eq!(hack.class_ids, [12, 2, 9, 0, 11, 10]);
    assert_eq!((hack.mode, hack.l28), (0, -1));
}

#[test]
fn the_combo_box_and_the_reload() {
    let mut h = world();
    let hack = UnitHack::new(&mut h);
    hack.fill_combo_box(&mut h);
    assert_eq!(h.log.len(), 8);
    assert_eq!(h.log[0], "clear");
    assert_eq!(h.log[1], "append Arch");
    assert_eq!(h.log[7], "select 0");
    h.log.clear();
    h.combo_selection = 3;
    hack.load_unit_data(&mut h);
    h.combo_selection = u32::MAX;
    hack.load_unit_data(&mut h);
    assert_eq!(h.log, ["load 0"]);
}

#[test]
fn no_placement_in_mode_0() {
    let mut h = world();
    let hack = UnitHack::new(&mut h);
    h.keys = [1, 1];
    hack.place_unit(&mut h, [3.5, 4.0]);
    assert_eq!(h.log, ["tile 3.5 4"]);
    assert_eq!(hack.selection(&mut h), 0);
}

#[test]
fn placement_in_mode_1() {
    let mut h = world();
    let mut hack = UnitHack::new(&mut h);
    hack.mode = 1;
    h.lists[1] = (0, 1);
    // neither key: nothing
    hack.place_unit(&mut h, [2.0, 2.0]);
    // Shift: the local player
    h.keys = [1, 0];
    hack.place_unit(&mut h, [2.0, 2.0]);
    // Ctrl with the player list's third entry (index 2): the third player past the empty slots and the local player
    h.keys = [0, 1];
    h.lists[2] = (0, 2);
    h.skipped = vec![1, 3];
    hack.place_unit(&mut h, [2.0, 2.0]);
    // the list's error code 0xd on the second call counts too
    h.list_calls = 0;
    h.lists[2] = (5, 0);
    h.second_player_code = Some(0xd);
    hack.place_unit(&mut h, [2.0, 2.0]);
    // another code: the local player
    h.list_calls = 0;
    h.second_player_code = Some(7);
    hack.place_unit(&mut h, [2.0, 2.0]);
    // no tile: nothing
    h.tile = false;
    hack.place_unit(&mut h, [9.0, 9.0]);
    assert_eq!(
        h.log,
        [
            "tile 2 2",
            "tile 2 2",
            "place 2 2 2 1",
            "tile 2 2",
            "place 2 2 2 5",
            "tile 2 2",
            "place 2 2 2 2",
            "tile 2 2",
            "place 2 2 2 1",
            "tile 9 9",
        ]
    );
}

#[test]
fn the_mouse() {
    let mut h = world();
    let mut hack = UnitHack::new(&mut h);
    hack.mode = 1;
    h.keys = [1, 0];
    h.point = Some([7.0, 8.0]);
    // a press finds the point and does nothing more
    h.buttons = [true, true];
    hack.update(&mut h, [100.0, 50.0]);
    // a release places at the point
    h.buttons = [false, true];
    hack.update(&mut h, [100.0, 50.0]);
    // outside the view: nothing
    h.in_view = false;
    hack.update(&mut h, [100.0, 50.0]);
    // no point found: nothing
    h.in_view = true;
    h.point = None;
    hack.update(&mut h, [1.0, 2.0]);
    assert_eq!(h.log, ["point 100 50", "point 100 50", "tile 7 8", "place 12 7 8 1", "point 1 2"]);
}
