use super::*;

/// A mock world: the towns by player (None an empty slot) and a file of words and byte runs.
#[derive(Default)]
struct Mock {
    counter: c_long,
    towns: Vec<Vec<Option<SavedTown>>>,
    file: Vec<i64>,
    bytes: Vec<Vec<u8>>,
    at: usize,
    at_bytes: usize,
    fail_after: Option<usize>,
    made: Vec<SavedTown>,
    log: Vec<String>,
    area_buf: Vec<u8>,
    unreadable_id: Option<c_long>,
}

impl TownHost for Mock {
    #[allow(clippy::cast_possible_truncation, clippy::cast_possible_wrap, clippy::cast_sign_loss)]
    fn call(&mut self, op: i32, player: u32, index: u32, io: &mut EeUnitsTownIo) -> i32 {
        let p = player as usize;
        let t = index as usize;
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_LONG | EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                if self.fail_after == Some(self.file.len()) {
                    return 1;
                }
                self.file.push(if op == EE_UNITS_PLAYER_SAVE_WRITE_BOOL { i64::from(io.prim.b) } else { io.prim.i });
                OK
            }
            EE_UNITS_PLAYER_SAVE_READ_LONG | EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                if self.at >= self.file.len() {
                    return 1;
                }
                let v = self.file[self.at];
                self.at += 1;
                io.prim.i = v;
                io.prim.b = u8::from(v != 0);
                OK
            }
            EE_UNITS_TOWN_WRITE_BYTES => {
                // SAFETY: the area buffer the walk got from EE_UNITS_TOWN_AREA
                self.bytes.push(unsafe { std::slice::from_raw_parts(io.bytes, io.len) }.to_vec());
                OK
            }
            EE_UNITS_TOWN_READ_BYTES => {
                let run = self.bytes.get(self.at_bytes).cloned().unwrap_or_default();
                self.at_bytes += 1;
                let n = run.len().min(io.len);
                // SAFETY: the walk's buffer of io.len bytes
                unsafe { std::ptr::copy_nonoverlapping(run.as_ptr(), io.bytes, n) };
                io.prim.i = n as i64;
                OK
            }
            EE_UNITS_TOWN_COUNTER => {
                io.prim.i = i64::from(self.counter);
                0
            }
            EE_UNITS_TOWN_SET_COUNTER => {
                self.counter = io.prim.i as c_long;
                0
            }
            EE_UNITS_TOWN_PLAYERS => {
                io.prim.i = self.towns.len() as i64;
                0
            }
            EE_UNITS_TOWN_SLOTS => self.towns.get(p).map_or(0, Vec::len) as i32,
            EE_UNITS_TOWN_GET => match &self.towns[p][t] {
                None => 0,
                Some(town) => {
                    *io = EeUnitsTownIo { points: town.hull.len() as u32, ..town_io(town) };
                    1
                }
            },
            EE_UNITS_TOWN_HULL_POINT => {
                let (x, y) = self.towns[p][t].as_ref().unwrap().hull[io.prim.i as usize];
                io.x = x;
                io.y = y;
                0
            }
            EE_UNITS_TOWN_AREA => {
                self.area_buf = self.towns[p][t].as_ref().unwrap().area.clone();
                io.bytes = self.area_buf.as_mut_ptr();
                io.len = self.area_buf.len();
                0
            }
            EE_UNITS_TOWN_MAKE => {
                if Some(io.id) == self.unreadable_id {
                    return 0;
                }
                // SAFETY: the walk's area
                let area = unsafe { std::slice::from_raw_parts(io.bytes, io.len) }.to_vec();
                self.made.push(SavedTown {
                    id: io.id,
                    counters: io.counters,
                    near_water: io.near_water != 0,
                    near_map_edge: io.near_map_edge != 0,
                    area,
                    continent: io.continent,
                    hull: Vec::new(),
                });
                1
            }
            EE_UNITS_TOWN_MAKE_DISCARD => {
                self.made.clear();
                self.log.push("discard".into());
                0
            }
            EE_UNITS_TOWN_MAKE_COMMIT => {
                let made = std::mem::take(&mut self.made);
                self.towns[p].extend(made.into_iter().map(Some));
                self.log.push("commit".into());
                0
            }
            EE_UNITS_TOWN_SET_SAVED => {
                let town = self.towns[p][t].as_mut().unwrap();
                town.id = io.id;
                town.counters = io.counters;
                town.near_water = io.near_water != 0;
                0
            }
            EE_UNITS_TOWN_REINDEX => {
                self.log.push("reindex".into());
                0
            }
            EE_UNITS_TOWN_REORDER => {
                // SAFETY: the walk's slots
                let order = unsafe { std::slice::from_raw_parts(io.bytes.cast::<i32>(), io.len) }.to_vec();
                let old = std::mem::take(&mut self.towns[p]);
                self.towns[p] = order.iter().map(|&s| old[s as usize].clone()).collect();
                self.log.push(format!("reorder {order:?}"));
                0
            }
            EE_UNITS_TOWN_DEBUG => {
                // SAFETY: the walk's text
                let text = unsafe { std::slice::from_raw_parts(io.bytes, io.len) };
                self.log.push(String::from_utf8_lossy(text).into_owned());
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

fn town(id: c_long, hull: &[(c_long, c_long)], area: &[u8]) -> SavedTown {
    SavedTown {
        id,
        hull: hull.to_vec(),
        counters: [id, 1, 2, 3, 4, 5, 6, 7],
        near_water: id % 2 == 0,
        near_map_edge: false,
        area: area.to_vec(),
        continent: 3,
    }
}

fn world() -> Mock {
    Mock {
        counter: 41,
        towns: vec![vec![], vec![Some(town(7, &[(1, 2), (3, 4)], &[9, 8])), None, Some(town(8, &[(5, 6)], &[]))]],
        ..Default::default()
    }
}

#[test]
fn a_save_loads_back_as_pending_towns() {
    let mut w = world();
    assert_eq!(save(&mut w), OK);
    // counter, players, player 0's count, player 1's count, then the towns
    assert_eq!(&w.file[..4], &[41, 2, 0, 2]);
    assert_eq!(w.bytes, vec![vec![9u8, 8]]);
    let mut r = Mock { file: w.file.clone(), bytes: w.bytes.clone(), ..Default::default() };
    let (result, pending) = load(&mut r, true);
    assert_eq!(result, OK);
    let pending = pending.unwrap();
    assert_eq!(pending.counter, 41);
    assert!(!pending.players.contains_key(&0), "a player without towns has no entry");
    assert_eq!(pending.players[&1], vec![town(7, &[(1, 2), (3, 4)], &[9, 8]), town(8, &[(5, 6)], &[])]);
}

#[test]
fn a_failed_read_keeps_nothing_pending_and_a_short_area_is_beyond_the_end() {
    let mut w = world();
    save(&mut w);
    let mut r = Mock { file: w.file[..w.file.len() - 2].to_vec(), bytes: w.bytes.clone(), ..Default::default() };
    let (result, pending) = load(&mut r, true);
    assert_ne!(result, OK);
    assert!(pending.is_none());
    let mut r = Mock { file: w.file.clone(), bytes: vec![vec![9u8]], ..Default::default() };
    assert_eq!(load(&mut r, true).0, EE_UNITS_TOWN_BEYOND_END);
}

#[test]
fn a_save_stops_at_the_first_failed_write() {
    let mut w = Mock { fail_after: Some(3), ..world() };
    assert_eq!(save(&mut w), 1);
    assert_eq!(w.file.len(), 3);
}

#[test]
fn the_towns_are_made_from_the_save_or_none_when_one_cannot_be_read() {
    let pending = PendingTowns {
        players: BTreeMap::from([(1, vec![town(7, &[], &[1]), town(8, &[], &[2])])]),
        counter: 9,
        areas: true,
    };
    let mut w = Mock { towns: vec![vec![], vec![]], ..Default::default() };
    assert!(make(&mut w, Some(&pending), 1));
    assert_eq!(w.towns[1].iter().map(|t| t.as_ref().unwrap().id).collect::<Vec<_>>(), [7, 8]);
    assert_eq!(w.towns[1][1].as_ref().unwrap().area, [2]);
    let mut w = Mock { towns: vec![vec![], vec![]], unreadable_id: Some(8), ..Default::default() };
    assert!(!make(&mut w, Some(&pending), 1));
    assert!(w.towns[1].is_empty());
    assert_eq!(w.log[1], "discard");
    assert!(w.log[0].contains("player 1's saved town 8 could not be read"));
    assert!(!make(&mut w, Some(&PendingTowns { areas: false, ..pending.clone() }), 1), "no areas: rebuilt");
    assert!(!make(&mut w, None, 1));
}

#[test]
fn rebuilt_towns_take_the_saved_ids_and_order_by_hull() {
    let pending = PendingTowns {
        players: BTreeMap::from([(1, vec![town(20, &[(5, 6)], &[]), town(21, &[(1, 2), (3, 4)], &[])])]),
        counter: 50,
        areas: false,
    };
    let mut w = world();
    apply(&mut w, Some(&pending), 1);
    assert_eq!(w.counter, 50);
    let ids: Vec<c_long> = w.towns[1].iter().map(|t| t.as_ref().unwrap().id).collect();
    assert_eq!(ids, [20, 21], "matched and put in the saved order");
    assert_eq!(w.log, ["reindex", "reorder [2, 0]"]);
    // one saved town unmatched: ids taken where matched, the order kept
    let pending = PendingTowns {
        players: BTreeMap::from([(1, vec![town(30, &[(9, 9)], &[]), town(31, &[(1, 2), (3, 4)], &[])])]),
        ..pending
    };
    let mut w = world();
    apply(&mut w, Some(&pending), 1);
    let ids: Vec<c_long> = w.towns[1].iter().flatten().map(|t| t.id).collect();
    assert_eq!(ids, [31, 8]);
    assert!(w.log[1].contains("player 1 rebuilt 2 towns, 1 of the 2 saved ones"));
    let mut w = world();
    apply(&mut w, None, 1);
    assert_eq!(w.counter, 41, "nothing pending: the counter is the game's");
}
