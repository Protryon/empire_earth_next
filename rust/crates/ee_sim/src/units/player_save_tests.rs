use std::collections::VecDeque;

use super::*;

/// One value in the mock file: the primitives by their overload, a C++ part's record by its name and index.
#[derive(Clone, Debug, PartialEq)]
enum Tok {
    U(i64),
    L(i64),
    F(u32),
    B(bool),
    Obj(&'static str, i64),
}

/// A player's C++ side over a token file: saves append, loads consume (a read past the end fails with 5 and keeps
/// the old value, as the 4-byte adapters do).
#[derive(Default)]
struct Mock {
    out: Vec<Tok>,
    input: VecDeque<Tok>,
    reads_tried: usize,
    /// the load's object operations and pushes, in order
    log: Vec<(i32, u32, i64)>,
    // the save's C++ parts
    upgrades: Vec<bool>,
    patrols: Vec<u8>,
    tech: bool,
    goal_counter: i64,
    queued: Vec<(i64, f32)>,
    constructions: Vec<i64>,
    herds: usize,
    offmap: usize,
    wonders: Vec<i64>,
    // the load's queued map
    queued_loaded: Vec<(i64, f32)>,
}

impl Mock {
    fn read(&mut self) -> Option<Tok> {
        self.reads_tried += 1;
        self.input.pop_front()
    }
    fn obj(&mut self, name: &'static str, index: i64) -> i32 {
        match self.read() {
            Some(Tok::Obj(n, i)) => {
                assert_eq!((n, i), (name, index), "the part's record");
                0
            }
            Some(t) => panic!("{name} {index}: read {t:?}"),
            None => 5,
        }
    }
}

impl PlayerSaveHost for Mock {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            // `long` and `unsigned long` are the same 4 bytes in the file (the save writes mNumCapitols as a long,
            // the load reads it as an unsigned long)
            EE_UNITS_PLAYER_SAVE_READ_ULONG => match self.read() {
                Some(Tok::U(v) | Tok::L(v)) => {
                    io.i = v;
                    0
                }
                Some(t) => panic!("ulong: read {t:?}"),
                None => 5,
            },
            EE_UNITS_PLAYER_SAVE_READ_LONG => match self.read() {
                Some(Tok::L(v) | Tok::U(v)) => {
                    io.i = v;
                    0
                }
                Some(t) => panic!("long: read {t:?}"),
                None => 5,
            },
            EE_UNITS_PLAYER_SAVE_READ_FLOAT => match self.read() {
                Some(Tok::F(v)) => {
                    io.f = f32::from_bits(v);
                    0
                }
                Some(t) => panic!("float: read {t:?}"),
                None => 5,
            },
            EE_UNITS_PLAYER_SAVE_READ_BOOL => match self.read() {
                Some(Tok::B(v)) => {
                    io.b = u8::from(v);
                    0
                }
                Some(t) => panic!("bool: read {t:?}"),
                None => 5,
            },
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG => {
                self.out.push(Tok::U(io.i));
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                self.out.push(Tok::L(io.i));
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => {
                self.out.push(Tok::F(io.f.to_bits()));
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                self.out.push(Tok::B(io.b != 0));
                0
            }
            // the load
            EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD => {
                self.log.push((op, index, io.i));
                self.obj("upgrade", io.i)
            }
            EE_UNITS_PLAYER_SAVE_TECH_LOAD => self.obj("tech", 0),
            EE_UNITS_PLAYER_SAVE_PATROL_LOAD => self.obj("patrol", -1),
            EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_LOAD => self.obj("saved groups", 0),
            EE_UNITS_PLAYER_SAVE_RTS_LOAD => self.obj("rts", 0),
            EE_UNITS_PLAYER_SAVE_AIR_LOAD => self.obj("air", i64::from(index)),
            EE_UNITS_PLAYER_SAVE_ACTIVE_LOAD => self.obj("active", 0),
            EE_UNITS_PLAYER_SAVE_CIV_LOAD => self.obj("civ", 0),
            EE_UNITS_PLAYER_SAVE_GROUP_LOAD => self.obj("group", i64::from(index)),
            EE_UNITS_PLAYER_SAVE_CIVFILE_LOAD => self.obj("civ file", 0),
            EE_UNITS_PLAYER_SAVE_QUEUED_FLOAT => {
                if !self.queued_loaded.iter().any(|e| e.0 == io.i) {
                    self.queued_loaded.push((io.i, 0.0));
                }
                io.f = self.queued_loaded.iter().find(|e| e.0 == io.i).unwrap().1;
                0
            }
            EE_UNITS_PLAYER_SAVE_QUEUED_SET_FLOAT => {
                self.queued_loaded.iter_mut().find(|e| e.0 == io.i).unwrap().1 = io.f;
                0
            }
            EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_LOAD => self.obj("queued list", io.i),
            EE_UNITS_PLAYER_SAVE_HERD_LOAD => self.obj("herd", i64::from(index)),
            EE_UNITS_PLAYER_SAVE_EVENTS_LOAD => self.obj("events", 0),
            EE_UNITS_PLAYER_SAVE_OFFMAP_LOAD => self.obj("off map", i64::from(index)),
            EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW
            | EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_DELETE
            | EE_UNITS_PLAYER_SAVE_SET_GOAL_COUNTER
            | EE_UNITS_PLAYER_SAVE_TECH_DELETE
            | EE_UNITS_PLAYER_SAVE_PATROL_CLEAR
            | EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE
            | EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH
            | EE_UNITS_PLAYER_SAVE_HERDS_RESIZE
            | EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE
            | EE_UNITS_PLAYER_SAVE_WONDER_PUSH
            | EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR => {
                self.log.push((op, index, io.i));
                0
            }
            // the save
            EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS => {
                io.i = self.upgrades.len() as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT => i32::from(self.upgrades[index as usize]),
            EE_UNITS_PLAYER_SAVE_UPGRADE_SAVE => {
                self.out.push(Tok::Obj("upgrade", i64::from(index)));
                0
            }
            EE_UNITS_PLAYER_SAVE_RTS_SAVE => {
                self.out.push(Tok::Obj("rts", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_GOAL_COUNTER => {
                io.i = self.goal_counter;
                0
            }
            EE_UNITS_PLAYER_SAVE_TECH_PRESENT => i32::from(self.tech),
            EE_UNITS_PLAYER_SAVE_TECH_SAVE => {
                if self.tech {
                    self.out.push(Tok::Obj("tech", 0));
                }
                0
            }
            EE_UNITS_PLAYER_SAVE_PATROL_SLOTS => {
                io.i = self.patrols.len() as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_PATROL_KIND => i32::from(self.patrols[index as usize]),
            EE_UNITS_PLAYER_SAVE_PATROL_SAVE => {
                self.log.push((op, index, 0));
                self.out.push(Tok::Obj("patrol", -1));
                0
            }
            EE_UNITS_PLAYER_SAVE_SAVED_GROUPS_SAVE => {
                self.out.push(Tok::Obj("saved groups", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_AIR_SAVE => {
                self.out.push(Tok::Obj("air", i64::from(index)));
                0
            }
            EE_UNITS_PLAYER_SAVE_ACTIVE_SAVE => {
                self.out.push(Tok::Obj("active", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_CIV_SAVE => {
                self.out.push(Tok::Obj("civ", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_GROUP_SAVE => {
                self.out.push(Tok::Obj("group", i64::from(index)));
                0
            }
            EE_UNITS_PLAYER_SAVE_CIVFILE_SAVE => {
                self.out.push(Tok::Obj("civ file", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_QUEUED_COUNT => {
                io.i = self.queued.len() as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_QUEUED_ENTRY => {
                (io.i, io.f) = self.queued[index as usize];
                0
            }
            EE_UNITS_PLAYER_SAVE_QUEUED_VECTOR_SAVE => {
                self.out.push(Tok::Obj("queued list", self.queued[index as usize].0));
                0
            }
            EE_UNITS_PLAYER_SAVE_CONSTRUCTION_COUNT => {
                io.i = self.constructions.len() as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_CONSTRUCTION_ID => {
                io.i = self.constructions[index as usize];
                0
            }
            EE_UNITS_PLAYER_SAVE_HERD_COUNT => {
                io.i = self.herds as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_HERD_SAVE => {
                self.out.push(Tok::Obj("herd", i64::from(index)));
                0
            }
            EE_UNITS_PLAYER_SAVE_EVENTS_SAVE => {
                self.out.push(Tok::Obj("events", 0));
                0
            }
            EE_UNITS_PLAYER_SAVE_OFFMAP_COUNT => {
                io.i = self.offmap as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_OFFMAP_SAVE => {
                self.out.push(Tok::Obj("off map", i64::from(index)));
                0
            }
            EE_UNITS_PLAYER_SAVE_WONDER_COUNT => {
                io.i = self.wonders.len() as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_WONDER_ID => {
                io.i = self.wonders[index as usize];
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

/// A record with every field distinct.
fn sample() -> EeUnitsPlayerSaved {
    let mut s = EeUnitsPlayerSaved::default();
    for (i, v) in s.mScenarioVariables.iter_mut().enumerate() {
        *v = 100 + i as c_ulong;
    }
    for (i, v) in s.mEnemies.iter_mut().enumerate() {
        *v = (i % 2) as c_ulong;
    }
    for (i, v) in s.mTributes.iter_mut().enumerate() {
        *v = 200 + i as c_ulong;
    }
    for (i, v) in s.mResourceCaps.iter_mut().enumerate() {
        *v = -300 - i as c_long;
    }
    for (i, v) in s.mResourceTrade.iter_mut().enumerate() {
        *v = 400 + i as c_long;
    }
    for (i, v) in s.mInsuranceRefunds.iter_mut().enumerate() {
        *v = 500 + i as c_long;
    }
    for (i, v) in s.mSendChatMessage.iter_mut().enumerate() {
        *v = i % 3 == 0;
    }
    s.mChatType = 2;
    s.mCombatState = 1;
    s.mGameResult = 3;
    s.mFreeUnitCounter = 7;
    s.mFreeUnitInterval = 8;
    s.mPopLimit = 200;
    s.mPopulationCount = 61;
    s.mEndOfGameCount = 59;
    s.mNumCapitols = 1;
    s.mNumVitalBuildings = 4;
    s.mHighGroundBonus = 0.25;
    s.mHealTimer = 1.5;
    s.mSharedLOS = true;
    s.mCivilizationChosen = true;
    s.mCivUpgradesPurchased = true;
    s
}

fn save_host() -> Mock {
    Mock {
        upgrades: vec![true, false, true, true, false],
        patrols: vec![2, 1, 0, 2],
        tech: true,
        goal_counter: 4242,
        queued: vec![(5, 0.5), (9, 2.0)],
        constructions: vec![31, 33],
        herds: 2,
        offmap: 1,
        wonders: vec![71, 72],
        ..Default::default()
    }
}

fn saved(major: u32, minor: u32, record: &EeUnitsPlayerSaved) -> (Vec<Tok>, Mock) {
    let mut rec = *record;
    let mut h = save_host();
    // SAFETY: a local record
    let r = save(&mut h, unsafe { Rec::new(&mut rec) }, major, minor);
    assert_eq!(r, 0);
    assert_eq!(rec, *record, "a save leaves the record");
    (h.out.clone(), h)
}

fn loaded(major: u32, minor: u32, file: &[Tok], start: EeUnitsPlayerSaved) -> (i32, EeUnitsPlayerSaved, Mock) {
    let mut rec = start;
    let mut h = Mock { input: file.iter().cloned().collect(), ..Default::default() };
    // SAFETY: a local record
    let r = load(&mut h, unsafe { Rec::new(&mut rec) }, major, minor);
    (r, rec, h)
}

#[test]
fn save_writes_the_original_order() {
    let (file, h) = saved(0, 280, &sample());
    let u = |v: i64| Tok::U(v);
    // the scenario variables, the upgrades' count and version, the present upgrades by index
    let mut head: Vec<Tok> = (100..110).map(u).collect();
    head.extend([u(3), u(0x10)]);
    for i in [0, 2, 3] {
        head.extend([u(i), Tok::Obj("upgrade", i)]);
    }
    head.extend([Tok::Obj("rts", 0), u(4242), Tok::B(true), Tok::Obj("tech", 0), Tok::L(200), u(7)]);
    head.extend((0..7).map(|i| Tok::L(-300 - i)));
    // 0.254 on: every patrol group, the towns' too, in order
    head.extend([u(3), Tok::Obj("patrol", -1), Tok::Obj("patrol", -1), Tok::Obj("patrol", -1)]);
    head.extend([Tok::Obj("saved groups", 0), Tok::L(61), Tok::L(59)]);
    assert_eq!(&file[..head.len()], &head[..]);
    let patrols: Vec<u32> = h.log.iter().filter(|e| e.0 == EE_UNITS_PLAYER_SAVE_PATROL_SAVE).map(|e| e.1).collect();
    assert_eq!(patrols, [0, 1, 3]);
    // the tail: off map, wonders, the free units, the trade and refunds, the vital buildings, the interval
    let mut tail = vec![u(1), Tok::Obj("off map", 0), u(2), u(71), u(72), u(7)];
    tail.extend((0..5).map(|i| Tok::L(400 + i)));
    tail.extend((0..5).map(|i| Tok::L(500 + i)));
    tail.extend([Tok::L(4), u(8)]);
    assert_eq!(&file[file.len() - tail.len()..], &tail[..]);
    // the queued improvements by key, the construction sites, the herds, the tributes' 2 x 5
    let at = |t: &Tok| file.iter().position(|x| x == t).unwrap();
    let q = at(&Tok::Obj("civ file", 0));
    assert_eq!(
        &file[q + 1..q + 12],
        &[
            u(2),
            u(5),
            Tok::F(0.5f32.to_bits()),
            Tok::Obj("queued list", 5),
            u(9),
            Tok::F(2.0f32.to_bits()),
            Tok::Obj("queued list", 9),
            u(2),
            u(31),
            u(33),
            u(2)
        ]
    );
    let t = at(&Tok::Obj("herd", 1));
    assert_eq!(&file[t + 1..t + 3], &[u(2), u(5)]);
    assert_eq!(&file[t + 3..t + 13], &(200..210).map(u).collect::<Vec<_>>()[..]);
}

#[test]
fn an_old_save_leaves_the_towns_patrols_out_and_writes_backwards() {
    let (file, h) = saved(0, 253, &sample());
    let patrols: Vec<u32> = h.log.iter().filter(|e| e.0 == EE_UNITS_PLAYER_SAVE_PATROL_SAVE).map(|e| e.1).collect();
    assert_eq!(patrols, [3, 0]);
    let caps = file.iter().position(|x| *x == Tok::L(-306)).unwrap();
    assert_eq!(file[caps + 1], Tok::U(2));
}

#[test]
fn load_reads_back_what_save_wrote() {
    let record = sample();
    let (file, _) = saved(0, 280, &record);
    let (r, got, h) = loaded(0, 280, &file, EeUnitsPlayerSaved::default());
    assert_eq!(r, 0);
    assert!(h.input.is_empty(), "every value read: {:?}", h.input);
    // mNumCapitols is read and dropped
    assert_eq!(got, EeUnitsPlayerSaved { mNumCapitols: 0, ..record });
    assert_eq!(h.queued_loaded, [(5, 0.5), (9, 2.0)]);
    let pushes = |op: i32| -> Vec<i64> { h.log.iter().filter(|e| e.0 == op).map(|e| e.2).collect() };
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD), [0, 2, 3]);
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_SET_GOAL_COUNTER), [4242]);
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH), [31, 33]);
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_HERDS_RESIZE), [2]);
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_OFFMAP_RESIZE), [1]);
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_WONDER_PUSH), [71, 72]);
    assert!(pushes(EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR).is_empty());
    assert_eq!(pushes(EE_UNITS_PLAYER_SAVE_PATROL_CLEAR).len(), 1);
    // the diplomacy table goes to the store after the enemies, the tech tree is dropped before its read
    let order: Vec<i32> = h.log.iter().map(|e| e.0).collect();
    let pos = |op: i32| order.iter().position(|&o| o == op).unwrap();
    assert!(pos(EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW) < pos(EE_UNITS_PLAYER_SAVE_UPGRADE_LOAD));
    assert!(pos(EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_DELETE) < pos(EE_UNITS_PLAYER_SAVE_TECH_DELETE));
    assert!(pos(EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE) < pos(EE_UNITS_PLAYER_SAVE_CONSTRUCTION_PUSH));
}

#[test]
fn each_era_stops_where_its_body_ended() {
    let (file, _) = saved(0, 280, &sample());
    let mut left = Vec::new();
    for (minor, line, clear) in
        [(0xd6, 4282, false), (0xe0, 4610, false), (0xe1, 4960, true), (0xe7, 4960, false), (0xe8, 5331, false)]
    {
        let (_, got, h) = loaded(0, minor, &file, EeUnitsPlayerSaved { mFreeUnitCounter: 99, ..Default::default() });
        let dummy = h.log.iter().find(|e| e.0 == EE_UNITS_PLAYER_SAVE_UPGRADE_DUMMY_NEW).unwrap();
        assert_eq!(dummy.1, line, "minor {minor:#x}: the era's assertion line");
        assert_eq!(h.log.iter().any(|e| e.0 == EE_UNITS_PLAYER_SAVE_WONDERS_CLEAR), clear, "minor {minor:#x}");
        assert_eq!(got.mFreeUnitCounter, if minor == 0xe8 { 0 } else { 99 }, "minor {minor:#x}");
        left.push(h.input.len());
    }
    // era 1 leaves the off-map units, the wonders and era 4's words; era 2 the wonders and era 4's; 0xe8 sets the
    // free-unit counter without its word and reads no trade, refunds, vital buildings or interval (from 0xf1, 0xf2,
    // 0xf5, 0xf6)
    assert_eq!(left, [18, 16, 13, 13, 13]);
    for (minor, rest) in [(0xe9, 12), (0xf1, 7), (0xf2, 2), (0xf5, 1), (0xf6, 0)] {
        let (_, _, h) = loaded(0, minor, &file, EeUnitsPlayerSaved::default());
        assert_eq!(h.input.len(), rest, "minor {minor:#x}");
    }
    let (_, _, h) = loaded(1, 0, &file, EeUnitsPlayerSaved::default());
    assert!(h.input.is_empty(), "a later major reads everything");
}

#[test]
fn a_short_file_keeps_the_old_values_and_the_ignored_results() {
    let record = sample();
    let (file, _) = saved(0, 280, &record);
    // only three scenario variables
    let start = EeUnitsPlayerSaved { mScenarioVariables: [7; 10], mPopLimit: 11, ..Default::default() };
    let (r, got, h) = loaded(0, 280, &file[..3], start);
    assert_eq!(r, 5);
    assert_eq!(got.mScenarioVariables, [100, 101, 102, 7, 7, 7, 7, 7, 7, 7]);
    assert_eq!(got.mPopLimit, 11);
    // the upgrades' count and version, the goal counter, the tech flag, the saved groups, the population, the
    // end-of-game count, and era 4's free-unit counter: the reads the original makes whatever came before
    assert_eq!(h.reads_tried, 4 + 2 + 1 + 1 + 1 + 1 + 1 + 1 + 1);
    assert!(h.log.iter().any(|e| e.0 == EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE));
}

#[test]
fn a_failed_id_read_goes_on_to_the_next_queued_entry() {
    let (file, _) = saved(0, 280, &sample());
    let q = file.iter().position(|x| *x == Tok::Obj("civ file", 0)).unwrap();
    // the count says 2: the file ends after it; both entries try their id (then era 4's free-unit counter)
    let (r, _, h) = loaded(0, 280, &file[..q + 2], EeUnitsPlayerSaved::default());
    assert_eq!(r, 5);
    assert_eq!(h.reads_tried - (q + 2), 3);
}

#[test]
fn counts_past_the_arrays_read_into_a_scratch_word() {
    // 9 resource caps, then 3 x 5 tributes: no panic, the extra words skipped, the arrays filled
    let (file, _) = saved(0, 280, &sample());
    let caps = file.iter().position(|x| *x == Tok::U(7)).unwrap();
    let mut long = file[..caps].to_vec();
    long.push(Tok::U(9));
    long.extend((0..9).map(|i| Tok::L(i + 1)));
    let rest = &file[caps + 8..];
    let t = rest.iter().position(|x| *x == Tok::Obj("herd", 1)).unwrap();
    long.extend_from_slice(&rest[..t + 1]);
    long.extend([Tok::L(3), Tok::L(5)]);
    long.extend((0..15).map(|i| Tok::U(1000 + i)));
    long.extend_from_slice(&rest[t + 13..]);
    let (r, got, h) = loaded(0, 0xe8, &long, EeUnitsPlayerSaved::default());
    assert_eq!(r, 0);
    assert_eq!(got.mResourceCaps, [1, 2, 3, 4, 5, 6, 7]);
    assert_eq!(got.mTributes, [1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009]);
    // the free-unit counter's word on (0xe8 reads none)
    assert_eq!(h.input.len(), 13);
}

/// A unit's load and save: a file of words, the class fit, the calls made.
#[derive(Default)]
struct UnitIo {
    words: VecDeque<i64>,
    out: Vec<i64>,
    fits: bool,
    kind: (i64, bool, bool),
    log: Vec<(i32, i64, u8)>,
}

impl PlayerSaveHost for UnitIo {
    fn call(&mut self, op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                let Some(w) = self.words.pop_front() else { return 5 };
                if op == EE_UNITS_PLAYER_SAVE_READ_ULONG {
                    io.i = w;
                } else {
                    io.b = w as u8;
                }
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG => {
                self.out.push(io.i);
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                self.out.push(i64::from(io.b));
                0
            }
            EE_UNITS_PLAYER_SAVE_UNIT_FITS => i32::from(self.fits),
            EE_UNITS_PLAYER_SAVE_UNIT_KIND => {
                io.i = self.kind.0;
                io.b = u8::from(self.kind.1);
                i32::from(self.kind.2)
            }
            _ => {
                self.log.push((op, io.i, io.b));
                7
            }
        }
    }
}

#[test]
fn a_unit_loads_into_its_class_or_a_stand_in_and_from_0_223_after_its_saved_flag() {
    let mut h = UnitIo { words: VecDeque::from([42, 1, 1]), fits: true, ..Default::default() };
    assert_eq!(load_unit(&mut h, 0, 0xe0), 7);
    assert_eq!(h.log, [(EE_UNITS_PLAYER_SAVE_UNIT_LOAD, 42, 1)]);
    // not saved: nothing more read
    let mut h = UnitIo { words: VecDeque::from([42, 0, 1]), ..Default::default() };
    assert_eq!(load_unit(&mut h, 0, 0xe0), 0);
    assert!(h.log.is_empty() && h.words.len() == 1);
    // before 0.223 no flag: the kind follows the type; a class that does not fit loads a stand-in
    let mut h = UnitIo { words: VecDeque::from([42, 0]), ..Default::default() };
    load_unit(&mut h, 0, 0xde);
    assert_eq!(h.log, [(EE_UNITS_PLAYER_SAVE_UNIT_STANDIN, 42, 0)]);
    // a short file: the read's error
    let mut h = UnitIo { words: VecDeque::from([42]), ..Default::default() };
    assert_eq!(load_unit(&mut h, 0, 0xe0), 5);
}

#[test]
fn a_path_point_is_saved_as_its_type_only_before_the_live_state() {
    let mut h = UnitIo { kind: (5, false, true), ..Default::default() };
    assert_eq!(save_unit(&mut h, 0, 0xe0), 7);
    assert_eq!(h.out, [5, 1, 1]);
    let mut h = UnitIo { kind: (9, true, false), ..Default::default() };
    assert_eq!(save_unit(&mut h, 0, 0xe0), 0);
    assert_eq!(h.out, [9, 0]);
    assert!(h.log.is_empty());
    let mut h = UnitIo { kind: (9, true, false), ..Default::default() };
    save_unit(&mut h, 0, 0x110);
    assert_eq!(h.out, [9, 1, 0]);
}
