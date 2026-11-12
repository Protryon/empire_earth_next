//! The lobby's player rows against the real C++ (`golden_lobby.txt`): a scratch driver (`build/sc1/lobby_driver.cpp`)
//! ran the unmodified `EEUIELobbyEvents.cpp` (with the real `UWideString.cpp` and `UString.cpp`) over a lobby form
//! of fake controls (every `SetDisplayMode`, `SetDirty`, `SetCurrentSelection`, `SetChecked`, `SetText`,
//! `AppendLine`, `SetTriggerEvent`, `SetBgOpacity`, `SetBgColor` and `SetPing` a trace line; the name colours and the
//! ready bitmap read back after each case): 3,000 cases of a random slot and lobby (host or client, single or
//! multiplayer, new game, scenario, saved game or a load type past them, the host ready or not, every slot type
//! and one past them), each `LobbyShowPlayer` with one of its flags, `LobbyFillPlayerTypes` with a menu 0..6, or
//! `PlayerTypeFromMenu` over every menu and line. The replay below rebuilds every line from the case's printed
//! inputs over this module, with the C++ forwarders' call sequences; the driver relinked over the forwarders
//! prints the same file, and the same on seeds 2-4 at ten times the size.

use super::*;

const GOLDEN: &str = include_str!("../golden_lobby.txt");
const IDS_OPEN: u32 = 30240;
const IDS_COMPUTER: u32 = 30241;
const IDS_CLOSED: u32 = 30242;
const IDS_OBSERVE: u32 = 30250;
const ENGINE_STRINGS: u32 = 33170;
const SELECTION_CHANGED: u32 = 1003;
const ENGINE_COUNT: c_ulong = 4;

fn colour(r: f32, g: f32, b: f32) -> String {
    format!("{:08x}.{:08x}.{:08x}", r.to_bits(), g.to_bits(), b.to_bits())
}

struct Row {
    s: c_ulong,
    modes: Vec<(c_ulong, u64)>,
    sel: Vec<(c_ulong, i64)>,
    ready_checked: bool,
    bitmap: String,
    t30: String,
    t46: String,
    menu: c_long,
    prefix: String,
    event: bool,
    out: Vec<String>,
}

impl Row {
    fn parse(line: &str, s: c_ulong, event: bool) -> Row {
        let mut r = Row {
            s,
            modes: vec![],
            sel: vec![],
            ready_checked: false,
            bitmap: String::new(),
            t30: String::new(),
            t46: String::new(),
            menu: 0,
            prefix: String::new(),
            event,
            out: vec![],
        };
        for kv in line.strip_prefix("  row ").unwrap().split(' ') {
            let (k, v) = kv.split_at(kv.find('=').unwrap());
            let v = &v[1..];
            match &k[..1] {
                "m" if k != "menu" => r.modes.push((k[1..].parse().unwrap(), v.parse().unwrap())),
                "s" => r.sel.push((k[1..].parse().unwrap(), v.parse().unwrap())),
                "c" => r.ready_checked = v == "1",
                "b" => r.bitmap = v.to_string(),
                "t" if k[1..].parse::<c_ulong>().unwrap() == 30 + s => r.t30 = v.to_string(),
                "t" => r.t46 = v.to_string(),
                "p" => r.prefix = v.to_string(),
                _ => r.menu = v.parse().unwrap(),
            }
        }
        r
    }
    fn line(&self) -> String {
        let mut l = String::from("  row");
        for (i, m) in &self.modes {
            l += &format!(" m{i}={m}");
        }
        for (i, v) in &self.sel {
            l += &format!(" s{i}={v}");
        }
        l += &format!(" c12={} b233={}", self.ready_checked as u8, self.bitmap);
        l += &format!(" t{}={} t{}={}", 30 + self.s, self.t30, 46 + self.s, self.t46);
        l + &format!(" menu={} prefix={}", self.menu, self.prefix)
    }
    fn push(&mut self, s: String) {
        self.out.push(s);
    }
    fn mode(&mut self, base: c_ulong, m: c_ulong) {
        let i = base + self.s;
        for e in self.modes.iter_mut() {
            if e.0 == i {
                e.1 = m as u64;
            }
        }
        self.push(format!("  mode {i} {m}"));
    }
    fn selection(&self, base: c_ulong) -> i64 {
        self.sel.iter().find(|e| e.0 == base + self.s).unwrap().1
    }
    fn select(&mut self, base: c_ulong, v: i64) {
        let i = base + self.s;
        for e in self.sel.iter_mut() {
            if e.0 == i {
                e.1 = v;
            }
        }
        self.push(format!("  sel {i} {v}"));
    }
    fn text(&mut self, base: c_ulong, t: &str) {
        self.push(format!("  text {} {t} 1", base + self.s));
    }
    fn dirty(&mut self, base: c_ulong) {
        self.push(format!("  dirty {} 1", base + self.s));
    }
}

struct Case {
    ctx: EeScreensLobbyContext,
    p: EeScreensLobbySlot,
    team: c_ulong,
    colour: c_ulong,
    citizens: c_ulong,
    engine: c_ulong,
    ping: String,
    name: String,
    teams: Vec<i64>,
}

fn fill(r: &mut Row, c: &Case, menu: c_long) {
    r.push(format!("  clear {}", 30 + r.s));
    for &l in player_type_lines(menu) {
        let text = match l {
            EE_SCREENS_LOBBY_LINE_NAME => c.name.clone(),
            EE_SCREENS_LOBBY_LINE_OPEN => format!("S{IDS_OPEN}"),
            EE_SCREENS_LOBBY_LINE_COMPUTER => format!("S{IDS_COMPUTER}"),
            EE_SCREENS_LOBBY_LINE_CLOSED => format!("S{IDS_CLOSED}"),
            _ => format!("S{IDS_OBSERVE}"),
        };
        r.push(format!("  append {} {text}", 30 + r.s));
    }
    // LobbySelectPlayerType: the box's change event lifted around the selection
    if r.event {
        r.push(format!("  trigger {} {SELECTION_CHANGED} -", 30 + r.s));
    }
    if let Some(line) = player_type_line(menu, c.p.kind) {
        r.select(30, line as i64);
    }
    if r.event {
        r.push(format!("  trigger {} {SELECTION_CHANGED} class EEUIELEChangePlayerType", 30 + r.s));
    }
}

fn row_controls(r: &mut Row, controls: [c_ulong; 4]) {
    let pairs = [(233, 12), (94, 62), (110, 78), (142, 126)];
    for (k, &(label, control)) in pairs.iter().enumerate() {
        let m = row_pair(controls[k]);
        r.mode(label, m[0]);
        r.mode(control, m[1]);
        if k == 1 {
            let kind = if controls[1] == 1 { "class EEUIELEChangeTeam" } else { "-" };
            r.push(format!("  trigger {} {SELECTION_CHANGED} {kind}", 62 + r.s));
        }
    }
}

fn show_player(r: &mut Row, c: &Case, flags: c_long) {
    let (p, ctx) = (&c.p, &c.ctx);
    if p.is_local && ctx.multiplayer {
        r.prefix = c.name.clone();
    }
    let plan = player_plan(p, ctx);
    if flags == 0 || flags == 8 {
        let row = if ctx.server { host_row(p, ctx) } else { client_row(p, ctx) };
        if r.menu != row.menu {
            fill(r, c, row.menu);
            r.menu = row.menu;
        }
        let text = match row.text {
            EE_SCREENS_LOBBY_TEXT_COMPUTER => format!("S{IDS_COMPUTER}"),
            EE_SCREENS_LOBBY_TEXT_NAME => c.name.clone(),
            EE_SCREENS_LOBBY_TEXT_OPEN => format!("S{IDS_OPEN}"),
            _ => format!("S{IDS_CLOSED}"),
        };
        r.text(46, &text);
        row_controls(r, row.controls);
        let n = name_plan(p, ctx);
        r.mode(30, n.box_mode);
        r.mode(46, n.label_mode);
        let col = match n.colour {
            EE_SCREENS_LOBBY_RED => colour(1.0, 0.0, 0.0),
            EE_SCREENS_LOBBY_YELLOW => colour(1.0, 1.0, 0.0),
            EE_SCREENS_LOBBY_WHITE => colour(1.0, 1.0, 1.0),
            _ => colour(0.0, 1.0, 0.0),
        };
        if n.box_dirty {
            r.t30 = col.clone();
        }
        r.t46 = col;
        if n.box_dirty {
            r.dirty(30);
        }
        r.dirty(46);
    }
    if flags == 5 || flags == 8 {
        r.mode(12, plan.ready[0]);
        r.mode(233, plan.ready[1]);
        r.ready_checked = p.is_ready;
        r.push(format!("  set {} {}", 12 + r.s, p.is_ready as u8));
        r.bitmap = if p.is_ready { "on" } else { "off" }.to_string();
        r.dirty(233);
    }
    if flags == 2 || flags == 8 {
        r.mode(78, plan.colour[0]);
        r.mode(110, plan.colour[1]);
        let k = (c.colour % 20) as f32;
        let col = [k * 0.05f32, 1.0f32 - k * 0.05f32, 0.5f32];
        for base in [78, 110] {
            r.push(format!("  opacity {} {:08x}", base + r.s, 1.0f32.to_bits()));
            r.push(format!(
                "  bgcolor {} {:08x} {:08x} {:08x}",
                base + r.s,
                col[0].to_bits(),
                col[1].to_bits(),
                col[2].to_bits()
            ));
        }
    }
    if flags == 3 || flags == 8 {
        r.mode(62, plan.team[0]);
        r.mode(94, plan.team[1]);
        let text = if c.team == 0 { "-".to_string() } else { c.team.to_string() };
        let want = c.teams[r.s as usize];
        let have = r.selection(62);
        if have == -1 || have != want {
            r.select(62, want);
        }
        r.text(94, &text);
    }
    if flags == 4 || flags == 8 {
        r.mode(126, plan.citizens[0]);
        r.mode(142, plan.citizens[1]);
        r.select(126, c.citizens as i64 - 1);
        r.text(142, &c.citizens.to_string());
    }
    if flags == 0 || flags == 6 || flags == 8 {
        let engine = engine_choice(c.engine, ENGINE_COUNT);
        let name = |k: c_ulong| format!("S{}", ENGINE_STRINGS + (k == 1) as u32);
        for k in 0..ENGINE_COUNT {
            r.push(format!("  append {} {}", 275 + r.s, name(k)));
        }
        r.mode(275, plan.engine[0]);
        r.mode(291, plan.engine[1]);
        let have = r.selection(275);
        if have == -1 || have != engine as i64 {
            r.select(275, engine as i64);
        }
        r.text(291, &name(engine));
    }
    if flags != 1 && flags != 8 {
        return;
    }
    r.mode(158, plan.ping);
    r.push(format!("  ping {} {}", 158 + r.s, c.ping));
}

fn run_case(lines: &[&str]) -> Vec<String> {
    let h: Vec<&str> = lines[0].split(' ').collect();
    let n = |i: usize| h[i].parse::<i64>().unwrap();
    let op = n(3);
    let c = Case {
        ctx: EeScreensLobbyContext {
            server: n(5) != 0,
            multiplayer: n(7) != 0,
            load_type: n(9) as c_ulong,
            host_ready: n(11) != 0,
        },
        p: EeScreensLobbySlot {
            kind: n(13) as c_ulong,
            loaded_type: n(15) as c_long,
            slot: n(17) as c_ulong,
            is_local: n(19) != 0,
            is_ready: n(21) != 0,
            b36: n(23) != 0,
        },
        team: n(25) as c_ulong,
        colour: n(27) as c_ulong,
        citizens: n(29) as c_ulong,
        engine: n(31) as c_ulong,
        ping: h[33].to_string(),
        name: h[35].to_string(),
        teams: lines[1].split(' ').skip(3).map(|s| s.parse().unwrap()).collect(),
    };
    let mut r = Row::parse(lines[2], c.p.slot, n(37) != 0);
    r.push(lines[0].to_string());
    r.push(lines[1].to_string());
    let before = r.line();
    r.push(before);
    if op < 6 {
        let flags: c_long = lines[3][6..].parse().unwrap();
        r.push(format!(" show {flags}"));
        show_player(&mut r, &c, flags);
    } else if op == 6 {
        let menu: c_long = lines[3][6..].parse().unwrap();
        r.push(format!(" fill {menu}"));
        fill(&mut r, &c, menu);
    } else {
        for m in 0..=6 {
            let types: Vec<String> = (0..=4).map(|k| player_type_from_menu(k, m).to_string()).collect();
            r.push(format!(" types {m} {}", types.join(" ")));
        }
    }
    let after = r.line();
    r.push(after);
    r.out
}

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&str> = GOLDEN.lines().collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with("case ")).collect();
    assert_eq!(starts.len(), 3000);
    for (k, &a) in starts.iter().enumerate() {
        let b = starts.get(k + 1).copied().unwrap_or(lines.len());
        assert_eq!(run_case(&lines[a..b]), lines[a..b], "case {k}");
    }
}
