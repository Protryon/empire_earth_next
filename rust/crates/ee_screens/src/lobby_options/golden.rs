//! The lobby's game options, start check and option events against the real C++ (`golden_lobby2.txt`): a scratch
//! driver (`build/sc2/lobby2_driver.cpp`) ran the unmodified `EEUIELobbyEvents.cpp` over fake controls (every
//! `SetCurrentSelection`, `SetText`, `SetChecked`, `SetDisplayMode` and `SetDirty` a trace line), the real
//! `UWideString`, fake rules lists, variants, starting resources and map generators, and fake messages and lock:
//! 3,000 cases of random options, slots, rules (the unit limit's minimum and step), variants' flags and controls,
//! each one `LobbyShowGameOptions` (the lobby's own options or another set, the host ready or not, the lobby the
//! current form or not), `LobbyCheckStart` (random slots; a loaded game's file missing, unreadable, mismatched or
//! fine) or one of sixteen option events. The replay below rebuilds every line from the case's printed inputs over
//! this module with the C++ forwarders' call sequences; the check boxes' text labels (the toolkit's lookups) and
//! the end epoch box's lines (`LobbyUpdateEpochEnd`, C++) are taken from the golden in place. The driver relinked
//! over the forwarders prints the same file, and the same on seeds 2-4 at ten times the size.
//!
//! The golden is the Windows build's 32-bit `long`: on a 64-bit `long` the cases holding a value past 2^31 are
//! skipped.

#![allow(clippy::needless_range_loop)]

use super::*;
use std::collections::BTreeMap;

const GOLDEN: &str = include_str!("../golden_lobby2.txt");

#[derive(Clone, Default)]
struct Options {
    load: u64,
    mp: bool,
    size: u64,
    res: u64,
    start: i64,
    end: i64,
    limit: u64,
    wonders: u64,
    variant: u64,
    difficulty: u64,
    speed: u64,
    flags: [bool; 6], // record, custom, reveal, lock teams, lock speed, cheats (the driver's order)
    map: String,
}

impl Options {
    fn parse(line: &str) -> Options {
        let t: Vec<&str> = line.trim_start().split(' ').collect();
        let n = |i: usize| t[i].parse::<i64>().unwrap();
        let mut flags = [false; 6];
        for (k, c) in t[23].chars().enumerate() {
            flags[k] = c == '1';
        }
        Options {
            load: n(2) as u64,
            mp: t[4] == "1",
            size: n(6) as u64,
            res: n(8) as u64,
            start: n(10),
            end: n(11),
            limit: n(13) as u64,
            wonders: n(15) as u64,
            variant: n(17) as u64,
            difficulty: n(19) as u64,
            speed: n(21) as u64,
            flags,
            map: t[25].to_string(),
        }
    }
    fn line(&self, what: &str) -> String {
        let f: String = self.flags.iter().map(|&b| if b { '1' } else { '0' }).collect();
        format!(
            "  {what} load {} mp {} size {} res {} epochs {} {} limit {} wonders {} variant {} difficulty {} speed {} \
             flags {f} map {}",
            self.load,
            self.mp as u8,
            self.size,
            self.res,
            self.start,
            self.end,
            self.limit,
            self.wonders,
            self.variant,
            self.difficulty,
            self.speed,
            self.map
        )
    }
}

#[derive(Default)]
struct Form {
    modes: BTreeMap<u32, u64>,
    sel: BTreeMap<u32, i64>,
    on: BTreeMap<u32, bool>,
    bitmaps: BTreeMap<u32, String>,
    order: Vec<(char, u32)>,
    levels: (i64, i64),
    out: Vec<String>,
}

impl Form {
    fn parse(line: &str) -> Form {
        let mut f = Form::default();
        let body = line.strip_prefix("  form ").unwrap();
        let (body, levels) = body.split_at(body.find(" levels ").unwrap());
        let l: Vec<i64> = levels[8..].split(' ').map(|s| s.parse().unwrap()).collect();
        f.levels = (l[0], l[1]);
        for kv in body.split(' ') {
            let (k, v) = kv.split_at(kv.find('=').unwrap());
            let (kind, i) = k.split_at(1);
            let kind = kind.chars().next().unwrap();
            let i: u32 = i.parse().unwrap();
            let v = &v[1..];
            f.order.push((kind, i));
            match kind {
                'm' => {
                    f.modes.insert(i, v.parse().unwrap());
                }
                's' => {
                    f.sel.insert(i, v.parse().unwrap());
                }
                'c' => {
                    f.on.insert(i, v == "1");
                }
                _ => {
                    f.bitmaps.insert(i, v.to_string());
                }
            }
        }
        f
    }
    fn line(&self) -> String {
        let mut s = String::from("  form");
        for &(kind, i) in &self.order {
            let v = match kind {
                'm' => self.modes[&i].to_string(),
                's' => self.sel[&i].to_string(),
                'c' => (self.on[&i] as u8).to_string(),
                _ => self.bitmaps[&i].clone(),
            };
            s += &format!(" {kind}{i}={v}");
        }
        s + &format!(" levels {} {}", self.levels.0, self.levels.1)
    }
    fn mode(&mut self, i: u32, m: u64) {
        self.modes.insert(i, m);
        self.out.push(format!("  mode {i} {m}"));
    }
    fn modes2(&mut self, edit: bool, boxed: u32, label: u32) {
        self.mode(boxed, edit as u64);
        self.mode(label, !edit as u64);
    }
    fn select(&mut self, i: u32, v: i64) {
        let v = long32(v); // the driver prints the selection as a 32-bit long
        self.sel.insert(i, v);
        self.out.push(format!("  sel {i} {v}"));
    }
    fn text(&mut self, i: u32, t: &str) {
        self.out.push(format!("  text {i} {t} 1"));
    }
}

/// `%ld` of a 32-bit `long` (the driver's arithmetic wraps there).
fn long32(v: i64) -> i64 {
    v as i32 as i64
}

fn show(f: &mut Form, o: &Options, lobby: &Options, server: bool, ready: bool, rules: (u64, i64), selectable: &[bool]) {
    let checks = [214u32, 216, 215, 217, 218, 219];
    let view = EeScreensLobbyOptionsView {
        load_type: o.load as c_long,
        lobby_load_type: lobby.load as c_long,
        server,
        host_ready: ready,
        lobby_multiplayer: lobby.mp,
        options_multiplayer: o.mp,
        type_selected: f.sel[&175] != -1,
        type_selection: f.sel[&175] as c_ulong,
        map_found: o.map.starts_with('M'),
        map_line: if o.map.starts_with('M') { o.map[1..].parse().unwrap() } else { 77 },
        epoch_selected: f.sel[&183] != -1,
        unit_limit: o.limit as c_long,
        unit_minimum: rules.0 as c_ulong,
        unit_step: rules.1 as c_long,
        wonders: o.wonders as c_ulong,
        flags: o.flags,
        checked: checks.map(|i| f.on[&i]),
    };
    let p = show_options(&view);
    if p.set_type_line {
        f.select(175, p.type_line as i64);
    }
    let strings = ["S31258", "S31260", "S31259"];
    let t = if p.type_string == EE_SCREENS_LOBBY_NO_STRING { "" } else { strings[p.type_string as usize] };
    f.text(194, t);
    f.modes2(p.type_editable, 175, 194);
    f.select(177, o.size as i64);
    f.text(195, &format!("S{}", 700 + if o.size < 5 { o.size } else { 0 }));
    let rows = |f: &mut Form, label0: u32, boxed: u32, label2: u32| match p.map_rows {
        EE_SCREENS_LOBBY_ROW_HIDDEN => {
            f.mode(label0, 0);
            f.mode(boxed, 0);
            f.mode(label2, 0);
        }
        r => {
            f.mode(label0, 1);
            f.modes2(r == EE_SCREENS_LOBBY_ROW_EDIT, boxed, label2);
        }
    };
    rows(f, 176, 177, 195);
    if p.set_map_line {
        f.select(179, p.map_line as i64);
    }
    f.text(196, &format!("D{}", o.map));
    rows(f, 178, 179, 196);
    if p.scenario_info {
        f.text(29, &lobby.map);
        f.mode(29, 1);
        f.mode(28, 1);
    } else {
        f.mode(29, 0);
        f.mode(28, 0);
    }
    let e = p.editable;
    f.select(181, o.res as i64);
    f.text(197, &format!("S{}", 500 + o.res));
    f.modes2(e, 181, 197);
    f.text(198, &format!("S{}", long32(600 + o.start)));
    if p.set_epoch_start {
        f.select(183, o.start);
    }
    f.modes2(e, 183, 198);
    f.text(199, &format!("S{}", long32(600 + o.end)));
    f.modes2(e, 185, 199);
    f.select(187, p.unit_line as i64);
    f.text(200, &long32(o.limit as i64).to_string());
    f.modes2(e, 187, 200);
    f.select(189, long32(o.wonders as i64));
    let w = if p.wonders_number { long32(o.wonders as i64).to_string() } else { "S30280".to_string() };
    f.text(201, &w);
    f.modes2(e, 189, 201);
    f.select(205, variant_line(selectable, o.variant as c_ulong) as i64);
    f.text(221, &format!("S{}", 400 + o.variant));
    f.modes2(e, 205, 221);
    f.select(207, o.difficulty as i64);
    f.text(222, &format!("S{}", 800 + if o.difficulty < 4 { o.difficulty } else { 0 }));
    f.modes2(e, 207, 222);
    f.select(209, o.speed as i64);
    f.text(223, &format!("S{}", 900 + if o.speed < 4 { o.speed } else { 0 }));
    f.modes2(e, 209, 223);
    // the check boxes: (box, bitmap, label), the flag
    let rows3 =
        [(214u32, 226u32, 249u32), (216, 228, 251), (215, 227, 250), (217, 229, 252), (218, 230, 253), (219, 231, 254)];
    for (k, &(boxed, bitmap, label)) in rows3.iter().enumerate() {
        if p.set_checked[k] {
            f.on.insert(boxed, o.flags[k]);
            f.out.push(format!("  set {boxed} {}", o.flags[k] as u8));
        }
        f.bitmaps.insert(bitmap, if o.flags[k] { "on" } else { "off" }.to_string());
        f.out.push(format!("  dirty {bitmap} 1"));
        f.out.push(format!("  text {label} @"));
        if k == 0 {
            // record games: hidden whatever the row
            f.mode(boxed, 0);
            f.mode(bitmap, 0);
            f.mode(label, 0);
        } else if !(k == 5 && p.cheats_row == EE_SCREENS_LOBBY_ROW_KEEP) {
            let edit = if k == 5 { p.cheats_row == EE_SCREENS_LOBBY_ROW_EDIT } else { e };
            f.mode(boxed, edit as u64);
            f.mode(bitmap, !edit as u64);
            f.mode(label, !edit as u64);
        }
    }
    f.on.insert(220, true);
    f.out.push("  set 220 1".to_string());
    f.mode(220, 0);
    f.mode(232, 0);
    f.mode(255, 0);
}

fn display(f: &mut Form, id: u32) {
    f.out.push(format!("  display S{id} 1 1"));
}

const START_IDS: [u32; 7] = [10058, 10009, 10063, 10016, 30411, 10041, 10052];

fn run_case(lines: &[&str]) -> Option<Vec<String>> {
    let wide = core::mem::size_of::<c_long>() == 8;
    let head: Vec<&str> = lines[0].split(' ').collect();
    let op: u32 = head[3].parse().unwrap();
    let server = head[5] == "1";
    let epochs: i64 = head[7].parse().unwrap();
    let minimum: u64 = head[9].parse().unwrap();
    let step: i64 = head[11].parse().unwrap();
    let selectable: Vec<bool> = head[13..].iter().map(|s| *s == "1").collect();
    let mut o = Options::parse(lines[1]);
    let mut f = Form::parse(lines[2]);
    let big = |v: i64| !(i32::MIN as i64..=i32::MAX as i64).contains(&v);
    if wide && (o.limit > i32::MAX as u64 || o.wonders > i32::MAX as u64 || big(o.start) || big(o.end)) {
        return None;
    }
    f.out = vec![lines[0].to_string(), o.line("options"), f.line()];
    f.out.push(lines[3].to_string());
    if op < 4 {
        let t: Vec<&str> = lines[3].split(' ').collect();
        let other = t[3] == "1";
        let ready = t[5] == "1";
        let current = t[7] == "1";
        let shown = if other {
            f.out.push(lines[4].to_string());
            Options::parse(lines[4])
        } else {
            o.clone()
        };
        if wide
            && (shown.limit > i32::MAX as u64 || shown.wonders > i32::MAX as u64 || big(shown.start) || big(shown.end))
        {
            return None;
        }
        if current {
            show(&mut f, &shown, &o, server, ready, (minimum, step), &selectable);
        }
    } else if op < 6 {
        let t: Vec<&str> = lines[3].split(' ').collect();
        let open_error = t[3] != "0";
        let load_error = t[5] != "0";
        let saved = t[7] == "1";
        let mut slots = [EeScreensLobbyStartSlot::default(); 16];
        for (s, v) in t[9..25].iter().enumerate() {
            let n: Vec<u64> = v.split('.').map(|x| x.parse().unwrap()).collect();
            slots[s] = EeScreensLobbyStartSlot {
                kind: n[0] as c_ulong,
                color: n[1] as c_ulong,
                team: n[2] as c_ulong,
                ready: n[3] == 1,
                transfer: n[4] as c_ulong,
            };
        }
        let mut load_ok = true;
        if o.load != 0 {
            // the load path (C++)
            let dir = if o.load == 2 { "Saved Games\\" } else { "Scenarios\\" };
            f.out.push(format!("  open {} {dir}", o.map));
            load_ok = false;
            if open_error {
                display(&mut f, 10019);
            } else {
                f.out.push("  loadheader".to_string());
                if load_error {
                    display(&mut f, 10036);
                } else {
                    f.out.push("  checksaved 1".to_string());
                    if saved {
                        load_ok = true;
                    } else {
                        f.out.push("  display a".to_string());
                        f.out.push("b".to_string());
                        f.out.push(" 1 1".to_string());
                    }
                }
            }
        }
        let r = start_check(o.load as c_ulong, load_ok, o.mp, &slots);
        for k in 0..r.count as usize {
            display(&mut f, START_IDS[r.messages[k] as usize]);
        }
        if r.hide_rows {
            for i in 257..272 {
                f.mode(i, 0);
            }
        }
        f.out.push(format!("  result {}", r.result as u8));
    } else {
        let t: Vec<&str> = lines[3].split(' ').collect();
        let k: u32 = t[2].parse().unwrap();
        let control: u32 = t[4].parse().unwrap();
        if k == 9 && f.sel[&205] != -1 {
            f.out.push(lines[4].to_string());
            f.sel.insert(205, lines[4][7..].parse().unwrap());
        }
        if k == 13 {
            f.out.push(lines[4].to_string());
        }
        // the options by number (EeScreensLobbyOptionState::current)
        let mut state = EeScreensLobbyOptionState {
            epoch_count: epochs as c_long,
            unit_minimum: minimum as c_ulong,
            unit_step: step as c_long,
            ..Default::default()
        };
        {
            let mut c = [0 as c_long; EE_SCREENS_OPTION_COUNT];
            c[1] = o.size as c_long;
            c[3] = o.res as c_long;
            c[4] = o.start as c_long;
            c[5] = o.end as c_long;
            c[6] = o.limit as c_long;
            c[7] = o.wonders as c_long;
            c[8] = o.variant as c_long;
            c[9] = o.difficulty as c_long;
            c[10] = o.speed as c_long;
            c[11] = o.flags[0] as c_long;
            c[12] = o.flags[1] as c_long;
            c[13] = o.flags[2] as c_long;
            c[14] = o.flags[3] as c_long;
            c[15] = o.flags[4] as c_long;
            c[16] = o.flags[5] as c_long;
            state.current = c;
        }
        // the event's option by the driver's event number
        const OPTIONS: [c_long; 15] = [13, 12, 11, 14, 16, 15, 1, 10, 9, 8, 7, 6, 5, 4, 3];
        let checkbox = k < 6;
        let selection: i64 = if checkbox { f.on[&control] as i64 } else { f.sel[&control] };
        if !checkbox && selection == -1 {
            // no selection: nothing
        } else if k == 15 {
            // the map type (C++: the generator's name)
            f.out.push("  writelock".to_string());
            let name = format!("M{}", (selection as u64) % 4);
            if name != o.map {
                f.out.push(format!("  message 2 {selection} {name}"));
                f.out.push("  send 255".to_string());
                o.map = name;
            }
            f.out.push("  writeunlock".to_string());
        } else {
            f.out.push("  writelock".to_string());
            let option = OPTIONS[k as usize];
            let c = option_change(option, selection as c_ulong, &state, &selectable);
            for m in 0..c.count as usize {
                let msg = c.messages[m];
                if m == 0 && c.update_epoch_end {
                    f.out.push("@epochend".to_string());
                }
                if c.set_difficulty {
                    f.levels = (msg.value as i64, msg.value as i64);
                }
                f.out.push(format!("  message {} {} ", msg.option, msg.value as i64));
                f.out.push("  send 255".to_string());
                let v = msg.value as i64;
                match msg.option {
                    1 => o.size = v as u32 as u64,
                    3 => o.res = v as u32 as u64,
                    4 => o.start = v,
                    5 => o.end = v,
                    6 => o.limit = v as u32 as u64,
                    7 => o.wonders = v as u32 as u64,
                    8 => o.variant = v as u32 as u64,
                    9 => o.difficulty = v as u32 as u64,
                    10 => o.speed = v as u32 as u64,
                    11..=16 => o.flags[[0, 1, 2, 3, 4, 5][(msg.option - 11) as usize]] = v != 0,
                    _ => unreachable!(),
                }
            }
            f.out.push("  writeunlock".to_string());
        }
    }
    f.out.push(o.line("options"));
    f.out.push(f.line());
    // the lines taken from the golden in place: the check boxes' labels, the end epoch box's
    let mut out = Vec::new();
    let mut g = 0;
    for line in f.out {
        if line.starts_with("  text ") && line.ends_with(" @") {
            assert!(lines[g].starts_with(&line[..line.len() - 1]), "{} vs {}", lines[g], line);
            out.push(lines[g].to_string());
            g += 1;
        } else if line == "@epochend" {
            while lines[g].starts_with("  deleteline ") || lines[g].starts_with("  insertline ") {
                out.push(lines[g].to_string());
                g += 1;
            }
        } else {
            out.push(line);
            g += 1;
        }
        if g > lines.len() {
            break;
        }
    }
    Some(out)
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 3000);
    let mut replayed = 0;
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        if let Some(out) = run_case(&lines[a..b]) {
            for (k, (x, y)) in out.iter().zip(&lines[a..b]).enumerate() {
                assert_eq!(x, y, "case {n} line {k}");
            }
            assert_eq!(out.len(), b - a, "case {n}");
            replayed += 1;
        }
    }
    let wide = core::mem::size_of::<c_long>() == 8;
    assert!(replayed > if wide { 2000 } else { 2999 }, "{replayed} cases replayed");
}
