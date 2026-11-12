//! The civilization builder's events against the real C++: a scratch driver (`build/sc9/cb_driver.cpp`) ran the
//! unmodified `EECBUIEvents.cpp` (and the builder form's open and close events' bodies out of
//! `EECBCivilizationFiles.cpp`) over fake `CivBuilder*` helpers (each printing its call and answering a drawn
//! result), the edited civilization, the file lists' selections, the forms and the builder's buttons: one session of
//! 2,000 cases, each drawing the results and flags (printed), then firing one event on its form, the open event with
//! each of its 32 flag sets (`golden_civ_builder.txt`). The replay keeps the first-change flag across the session
//! and prints what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp` and this
//! crate prints the same file, and the same on seeds 2-4 at 30,000 cases.

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_civ_builder.txt");

const RESULT_NAMES: [&str; 19] = [
    "shownamedialog",
    "typedname",
    "typedfilename",
    "acceptname",
    "close",
    "saveas",
    "showload",
    "showsave",
    "loadcivilization",
    "setname",
    "closeform",
    "savenameselected",
    "resetavailable",
    "resetbought",
    "buy",
    "return",
    "askaccept",
    "askdelete",
    "hidepoints",
];

#[derive(Default)]
struct World {
    out: Vec<String>,
    results: Vec<i32>,
    typed: [String; 2],
    can_change: bool,
    lists: [bool; 2],
    selected: [bool; 2],
    locked: bool,
    unchanged: bool,
    points: u32,
    multi: bool,
    files: [String; 2],
    civ: Option<String>,
    kept: String,
    form: u32,
    entity: bool,
    modes: [u32; 4],
}

impl World {
    fn r(&mut self, name: &str) -> u32 {
        let r = self.results[RESULT_NAMES.iter().position(|n| *n == name).unwrap()];
        self.out.push(format!("  {name} -> {r}"));
        r as u32
    }
}

impl CivBuilderHost for World {
    fn call(&mut self, op: u32, a: u32) -> u32 {
        let form = self.form;
        match op {
            EE_SCREENS_GAME_CB_SHOW_NAME_DIALOG => self.r("shownamedialog"),
            EE_SCREENS_GAME_CB_TYPED => {
                let (name, kind) = if a != 0 { ("typedfilename", 1) } else { ("typedname", 0) };
                self.kept = self.typed[kind].clone();
                let r = self.results[RESULT_NAMES.iter().position(|n| *n == name).unwrap()];
                self.out.push(format!("  {name} '{}' -> {r}", self.kept));
                r as u32
            }
            EE_SCREENS_GAME_CB_ACCEPT_NAME => self.r("acceptname"),
            EE_SCREENS_GAME_CB_CLOSE => {
                let r = self.results[4];
                self.out.push(format!("  close {a} -> {r}"));
                r as u32
            }
            EE_SCREENS_GAME_CB_SAVE_AS => self.r("saveas"),
            EE_SCREENS_GAME_CB_SHOW_LOAD => self.r("showload"),
            EE_SCREENS_GAME_CB_SHOW_SAVE => self.r("showsave"),
            EE_SCREENS_GAME_CB_SELECTION => {
                let k = a as usize;
                if k == 0 {
                    self.out.push(format!("  loadfilelist -> {}", self.lists[0] as i32));
                } else {
                    self.out.push(format!("  controlbyid {form} 420 -> {}", self.lists[1] as i32));
                }
                if !self.lists[k] {
                    return EE_SCREENS_CB_NO_LIST;
                }
                self.out.push(format!("  currentselection {} -> {}", ["load", "save"][k], self.selected[k] as i32));
                if !self.selected[k] {
                    return 0xd;
                }
                if k == 0 {
                    self.kept = self.files[0].clone();
                }
                0
            }
            EE_SCREENS_GAME_CB_SET_NAME_FROM_CIVILIZATION => {
                let civ = self.civ.clone().unwrap();
                self.out.push(format!("  civname '{civ}'"));
                let r = self.results[9];
                self.out.push(format!("  setname '{civ}' -> {r}"));
                r as u32
            }
            EE_SCREENS_GAME_CB_CLOSE_FORM => self.r("closeform"),
            EE_SCREENS_GAME_CB_UNCHANGED => {
                self.out.push(format!("  unchanged -> {}", self.unchanged as i32));
                self.unchanged as u32
            }
            EE_SCREENS_GAME_CB_SAVE_NAME_SELECTED => self.r("savenameselected"),
            EE_SCREENS_GAME_CB_CAN_CHANGE => {
                self.out.push(format!("  canchange -> {}", self.can_change as i32));
                self.can_change as u32
            }
            EE_SCREENS_GAME_CB_RESET_AVAILABLE | EE_SCREENS_GAME_CB_RESET_BOUGHT | EE_SCREENS_GAME_CB_ASK_ACCEPT => {
                let name = match op {
                    EE_SCREENS_GAME_CB_RESET_AVAILABLE => "resetavailable",
                    EE_SCREENS_GAME_CB_RESET_BOUGHT => "resetbought",
                    _ => "askaccept",
                };
                let r = self.results[RESULT_NAMES.iter().position(|n| *n == name).unwrap()];
                self.out.push(format!("  {name} {form} -> {r}"));
                r as u32
            }
            EE_SCREENS_GAME_CB_LOCKED => self.locked as u32,
            EE_SCREENS_GAME_CB_BUY => {
                let r = self.results[14];
                self.out.push(format!("  buy 0 -> {r}"));
                r as u32
            }
            EE_SCREENS_GAME_CB_RETURN => self.r("return"),
            EE_SCREENS_GAME_CB_POINTS_LEFT => self.points,
            EE_SCREENS_GAME_MULTIPLAYER_SESSION => self.multi as u32,
            EE_SCREENS_GAME_CB_ACCEPT_CHANGES => {
                self.out.push("  acceptchanges null".into());
                0
            }
            EE_SCREENS_GAME_CB_FORM_ID => form,
            EE_SCREENS_GAME_CB_ASK_DELETE => {
                let r = self.results[17];
                self.out.push(format!("  askdelete {form} {} -> {r}", self.entity as i32));
                r as u32
            }
            EE_SCREENS_GAME_CB_CLEAN_UP => {
                self.civ = None;
                0
            }
            EE_SCREENS_OP_CB_SHOW => {
                let (control, shown) = (a >> 1, a & 1);
                self.out.push(format!("  mode {control} {shown}"));
                let slot = [2, 15, 16, 17].iter().position(|&c| c == control).unwrap();
                self.modes[slot] = shown;
                0
            }
            EE_SCREENS_GAME_CB_HIDE_POINTS => self.r("hidepoints"),
            other => panic!("unexpected op {other:#x}"),
        }
    }
    fn text(&mut self, which: u32) -> Vec<u8> {
        assert_eq!(which, EE_SCREENS_TEXT_CB_NAME);
        self.kept.clone().into_bytes()
    }
    fn put_text(&mut self, which: u32, text: &[u8]) -> u32 {
        let text = String::from_utf8(text.to_vec()).unwrap();
        match which {
            EE_SCREENS_TEXT_CB_CIVILIZATION_NAME => {
                self.out.push(format!("  setcivilizationname '{text}'"));
                0
            }
            EE_SCREENS_TEXT_CB_SET_NAME => {
                let r = self.results[9];
                self.out.push(format!("  setname '{text}' -> {r}"));
                r as u32
            }
            EE_SCREENS_TEXT_CB_LOAD => {
                let r = self.results[8];
                self.out.push(format!("  loadcivilization '{text}' -> {r}"));
                r as u32
            }
            other => panic!("unexpected put {other:#x}"),
        }
    }
}

/// The `'...'` texts of a line, in order.
fn quoted(line: &str) -> Vec<String> {
    line.split('\'').skip(1).step_by(2).map(str::to_string).collect()
}

fn read_state(w: &mut World, line: &str) {
    let t: Vec<&str> = line.split_whitespace().collect();
    w.results = t[2..21].iter().map(|v| v.parse().unwrap()).collect();
    let q = quoted(line);
    w.typed = [q[0].clone(), q[1].clone()];
    w.files = [q[2].clone(), q[3].clone()];
    let rest = line.split_once("canchange ").unwrap().1;
    let r: Vec<&str> = rest.split_whitespace().collect();
    // canchange C lists L S selected A B locked K unchanged U points P multi M files ...
    w.can_change = r[0] == "1";
    w.lists = [r[2] == "1", r[3] == "1"];
    w.selected = [r[5] == "1", r[6] == "1"];
    w.locked = r[8] == "1";
    w.unchanged = r[10] == "1";
    w.points = r[12].parse().unwrap();
    w.multi = r[14] == "1";
    let civ = line.rsplit_once(" civ ").unwrap().1;
    w.civ = if civ == "none" { None } else { Some(civ.to_string()) };
    w.modes = [9; 4];
}

fn event_of(kind: &str) -> u32 {
    match kind {
        "showchangename" => EE_SCREENS_CB_SHOW_CHANGE_NAME,
        "acceptchangename" => EE_SCREENS_CB_ACCEPT_CHANGE_NAME,
        "showload" => EE_SCREENS_CB_SHOW_LOAD,
        "acceptload" => EE_SCREENS_CB_ACCEPT_LOAD,
        "showsave" => EE_SCREENS_CB_SHOW_SAVE,
        "cancelsave" => EE_SCREENS_CB_CANCEL_SAVE,
        "cancelload" => EE_SCREENS_CB_CANCEL_LOAD,
        "cancelname" => EE_SCREENS_CB_CANCEL_NAME,
        "acceptsave" => EE_SCREENS_CB_ACCEPT_SAVE,
        "savenameselected" => EE_SCREENS_CB_SAVE_NAME_SELECTED,
        "purchasedselected" => EE_SCREENS_CB_PURCHASED_SELECTED,
        "potentialselected" => EE_SCREENS_CB_POTENTIAL_SELECTED,
        "purchase" => EE_SCREENS_CB_PURCHASE,
        "returned" => EE_SCREENS_CB_RETURNED,
        "updatecontrols" => EE_SCREENS_CB_UPDATE_CONTROLS,
        "cancelchanges" => EE_SCREENS_CB_CANCEL_CHANGES,
        "acceptchanges" => EE_SCREENS_CB_ACCEPT_CHANGES,
        "deleteinload" | "deleteinsave" => EE_SCREENS_CB_DELETE,
        "cleanup" => EE_SCREENS_CB_CLEAN_UP,
        _ => EE_SCREENS_CB_BUTTONS + kind.strip_prefix("buttons").unwrap().parse::<u32>().unwrap(),
    }
}

#[test]
fn golden_civ_builder() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let state = Mutex::new(CivBuilder::new());
    let mut w = World::default();
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        // case N KIND form F entity E
        let head: Vec<&str> = case[0].split(' ').collect();
        let event = event_of(head[2]);
        kinds.insert(event);
        w.form = head[4].parse().unwrap();
        w.entity = head[6] == "1";
        read_state(&mut w, case[1]);
        w.out.clear();
        let result = dispatch(&mut w, &state, event).unwrap();
        w.out.push(format!("  result {result}"));
        w.out.push(format!(
            "  after civ {} modes {} {} {} {}",
            w.civ.is_some() as i32,
            w.modes[0],
            w.modes[1],
            w.modes[2],
            w.modes[3]
        ));
        let expected = &case[2..];
        for (k, e) in expected.iter().enumerate() {
            assert!(k < w.out.len(), "case {n} ({}): missing line {k}: {e}", case[0]);
            assert_eq!(w.out[k], *e, "case {n} ({}) line {k}", case[0]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 19 + 32);
}
