//! `mouse_modes.rs` tests: the golden replay (`golden_mouse_modes.txt`, the round 30 driver's new side over the
//! original modes' fake world: per call the mode, the mouse and the placements' records; per host op its arguments,
//! floats and answer; after the call the records) and a few scripted modes.

use super::*;

const GOLDEN: &str = include_str!("../golden_mouse_modes.txt");

fn hex(s: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(s, 16).unwrap())
}

/// One logged host op: the op, a, b (or the wall preview's tiles), the io in; the answer, the io out.
#[derive(Debug)]
struct Op {
    op: u32,
    a: i64,
    b: Option<i64>,
    tiles: Vec<[i32; 2]>,
    io_in: EeScreensMainFormIo,
    ret: i64,
    io_out: EeScreensMainFormIo,
}

fn io(t: &[&str]) -> EeScreensMainFormIo {
    EeScreensMainFormIo {
        f: [hex(t[0]), hex(t[1]), hex(t[2]), hex(t[3])],
        n: [t[4].parse().unwrap(), t[5].parse().unwrap()],
    }
}

fn wall_of(t: &[&str]) -> EeScreensBuildWallFields {
    EeScreensBuildWallFields {
        state: t[0].parse().unwrap(),
        start: EeScreensPoint { x: hex(t[1]), y: hex(t[2]) },
        end: EeScreensPoint { x: hex(t[3]), y: hex(t[4]) },
        technology: t[5].parse().unwrap(),
        player: t[6].parse().unwrap(),
    }
}

fn gate_from(t: &[&str]) -> EeScreensBuildGateFields {
    let mut g = [0i32; 6];
    for (v, s) in g.iter_mut().zip(t) {
        *v = s.parse().unwrap();
    }
    gate_of(g)
}

fn gate_of(g: [i32; 6]) -> EeScreensBuildGateFields {
    EeScreensBuildGateFields {
        unit_class: core::ptr::null_mut(),
        tile_x: g[0],
        tile_y: g[1],
        technology: g[2],
        wall_type: g[3],
        direction: g[4],
        player: g[5],
    }
}

/// The host replaying a call's logged ops in order.
struct Replay {
    ops: std::vec::IntoIter<Op>,
}

impl Replay {
    fn next(&mut self, op: u32) -> Op {
        let o = self.ops.next().unwrap_or_else(|| panic!("op {op} past the log"));
        assert_eq!(o.op, op, "{o:?}");
        o
    }
}

impl FormHost for Replay {
    fn op(&mut self, op: u32, a: i64, b: i64, io: &mut EeScreensMainFormIo) -> i64 {
        let o = self.next(op);
        assert_eq!((o.a, o.b, o.io_in), (a, Some(b), *io), "{o:?}");
        *io = o.io_out;
        o.ret
    }
    fn set_text(&mut self, _: &[u8]) {
        panic!("no texts in the modes");
    }
    fn text(&mut self) -> Vec<u8> {
        panic!("no texts in the modes");
    }
    fn preview_wall(&mut self, tiles: &[[i32; 2]]) {
        let o = self.next(EE_SCREENS_MF_OP_PREVIEW_WALL);
        assert_eq!((o.a, o.tiles.as_slice()), (tiles.len() as i64, tiles), "{o:?}");
    }
}

#[test]
fn replays_the_golden() {
    let mut lines = GOLDEN.lines().peekable();
    let (mut calls, mut placements, mut ops_seen) = (0, 0, 0);
    while let Some(line) = lines.next() {
        let t: Vec<&str> = line.split(' ').collect();
        assert_eq!(t[0], ">", "{line}");
        let (x, y) = (hex(t[3 - (t[1] != "M") as usize]), hex(t[4 - (t[1] != "M") as usize]));
        let mut ops = Vec::new();
        while lines.peek().is_some_and(|l| l.starts_with("o ")) {
            let t: Vec<&str> = lines.next().unwrap().split(' ').collect();
            let op: u32 = t[1].parse().unwrap();
            let a = t[2].parse().unwrap();
            let (b, tiles, rest) = if op == EE_SCREENS_MF_OP_PREVIEW_WALL {
                let tiles = if t[4] == "-" {
                    Vec::new()
                } else {
                    let n: Vec<i32> = t[4].split(',').map(|s| s.parse().unwrap()).collect();
                    n.chunks(2).map(|c| [c[0], c[1]]).collect()
                };
                (None, tiles, &t[5..])
            } else {
                (Some(t[3].parse().unwrap()), Vec::new(), &t[4..])
            };
            assert_eq!(rest[6], "=");
            ops.push(Op {
                op,
                a,
                b,
                tiles,
                io_in: io(&rest[..6]),
                ret: rest[7].parse().unwrap(),
                io_out: io(&rest[8..14]),
            });
        }
        ops_seen += ops.len();
        let end: Vec<&str> = lines.next().unwrap().split(' ').collect();
        assert_eq!(end[0], "=");
        let mut host = Replay { ops: ops.into_iter() };
        match t[1] {
            "M" => {
                let res = mouse(&mut host, t[2].parse().unwrap(), x, y);
                assert_eq!(res, end[1].parse::<i64>().unwrap());
                calls += 1;
            }
            "W" => {
                let mut wall = wall_of(&t[4..]);
                // SAFETY: the record lives through the call
                wall_update(&mut host, &unsafe { Rec::new(&mut wall) }, x, y);
                assert_eq!(wall, wall_of(&end[1..]), "{line}");
                placements += 1;
            }
            _ => {
                let mut gate = gate_from(&t[4..]);
                // SAFETY: as the wall's
                gate_update(&mut host, &unsafe { Rec::new(&mut gate) }, x, y);
                assert_eq!(gate, gate_from(&end[1..]), "{line}");
                placements += 1;
            }
        }
        assert!(host.ops.next().is_none(), "{line}: ops left");
    }
    assert!(
        calls > 1000 && placements > 100 && ops_seen > 5000,
        "{calls} calls, {placements} placements, {ops_seen} ops"
    );
}

/// A host answering ops from a table of (op, a, b, answer), `i64::MIN` matching any a or b (unlisted ops answer
/// 0), and recording every op.
#[derive(Default)]
struct Script {
    answers: Vec<(u32, i64, i64, i64)>,
    log: Vec<(u32, i64, i64)>,
}

impl FormHost for Script {
    fn op(&mut self, op: u32, a: i64, b: i64, _: &mut EeScreensMainFormIo) -> i64 {
        self.log.push((op, a, b));
        let hit = |x: i64, y: i64| x == y || x == i64::MIN;
        self.answers.iter().find(|w| w.0 == op && hit(w.1, a) && hit(w.2, b)).map_or(0, |w| w.3)
    }
    fn set_text(&mut self, _: &[u8]) {}
    fn text(&mut self) -> Vec<u8> {
        Vec::new()
    }
    fn preview_wall(&mut self, _: &[[i32; 2]]) {}
}

fn run(h: &mut Script, mode: i64) {
    assert_eq!(mouse(h, mode, 1.0, 2.0), 0);
}

#[test]
fn the_none_mode_asks_nothing() {
    let mut h = Script::default();
    run(&mut h, EE_SCREENS_MF_MODE_NONE);
    run(&mut h, 99);
    assert!(h.log.is_empty());
}

#[test]
fn a_captured_mouse_only_locks_the_cursor_in_the_flare_mode() {
    let mut h = Script { answers: vec![(EE_SCREENS_MF_OP_MD_CAPTURED, 0, 0, 1)], ..Default::default() };
    run(&mut h, EE_SCREENS_MF_MODE_FLARE);
    assert_eq!(
        h.log,
        [
            (EE_SCREENS_MF_OP_SET_FIELD, EE_SCREENS_MF_FIELD_CURSOR_LOCKED, 1),
            (EE_SCREENS_MF_OP_MD_POINTER, EE_SCREENS_MM_PTR_FLARE, 0),
            (EE_SCREENS_MF_OP_MD_CAPTURED, 0, 0),
        ]
    );
}

#[test]
fn an_unplaceable_ghost_stops_the_build_unless_it_is_one_of_several_farms() {
    for (count, ty, built) in [(2, EE_SCREENS_MF_TYPE_FARM, true), (1, EE_SCREENS_MF_TYPE_FARM, false), (2, 7, false)] {
        let mut h = Script {
            answers: vec![
                (EE_SCREENS_MF_OP_MD_BUTTONS, 0, 0, EE_SCREENS_MF_BTN_L_RELEASED),
                (EE_SCREENS_MF_OP_MD_GHOST, -1, 0, count),
                (EE_SCREENS_MF_OP_MD_GHOST, i64::MIN, EE_SCREENS_MF_GHOST_TYPE, ty),
            ],
            ..Default::default()
        };
        run(&mut h, EE_SCREENS_MF_MODE_BUILD);
        assert_eq!(h.log.iter().any(|o| o.0 == EE_SCREENS_MF_OP_MD_BUILD), built, "{count} {ty}");
    }
}
