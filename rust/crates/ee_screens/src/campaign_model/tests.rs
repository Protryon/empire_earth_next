use super::*;

/// The string ids equal `ee/EEStringIDs.h`'s.
#[test]
fn string_ids() {
    let header = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EEStringIDs.h"))
        .expect("EEStringIDs.h");
    let id = |name: &str| -> u32 {
        let line =
            header.lines().find(|l| l.split_whitespace().nth(1) == Some(name)).unwrap_or_else(|| panic!("{name}"));
        line.split_whitespace().nth(2).unwrap().parse().unwrap()
    };
    assert_eq!(id("IDS_CAMPAIGN_MUST_HAVE_AT_LEAST"), IDS_CAMPAIGN_MUST_HAVE_AT_LEAST);
    assert_eq!(id("IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN"), IDS_YOU_MUST_ENTER_NAME_FOR_YOUR_CAMPAIGN);
    assert_eq!(id("IDS_EECAMPAIGN"), IDS_EECAMPAIGN);
    assert_eq!(id("IDS_SETUP_NONE_PROVIDED"), IDS_SETUP_NONE_PROVIDED);
    assert_eq!(id("IDS_SETUP_NONE_PROVIDED_1BD3"), IDS_SETUP_NONE_PROVIDED_1BD3);
    assert_eq!(id("IDS_NO_DESCRIPTION_PROVIDED"), IDS_NO_DESCRIPTION_PROVIDED);
    assert_eq!(id("IDS_CAMPAIGN_COULDNT_BE_DELETED"), IDS_CAMPAIGN_COULDNT_BE_DELETED);
}

#[test]
fn extensions() {
    assert_eq!(strip_extension(b"Rome.ssa", b".ssa"), b"Rome");
    assert_eq!(strip_extension(b"Rome.SSA", b".ssa"), b"Rome");
    assert_eq!(strip_extension(b".ssa", b".ssa"), b".ssa"); // four characters: kept
    assert_eq!(strip_extension(b"ab", b".ssa"), b"ab");
    assert_eq!(strip_extension("été.SCN".as_bytes(), b".scn"), "été".as_bytes());
    assert_eq!(strip_extension("été.scnx".as_bytes(), b".scn"), "été.scnx".as_bytes());
    assert_eq!(strip_extension("ééé.x".as_bytes(), b".scn"), "ééé.x".as_bytes());
}

#[test]
fn lines() {
    let l = vec![b"a".to_vec(), Vec::new(), "é".as_bytes().to_vec()];
    assert_eq!(split_lines(&join_lines(&l)), l);
    assert!(split_lines(b"").is_empty());
    assert_eq!(split_lines(b"tail"), Vec::<Vec<u8>>::new());
}

/// A campaign kept in memory: scenarios and fields, the disk answering nothing.
#[derive(Default)]
struct Fake {
    scenarios: Vec<Vec<u8>>,
    fields: [Vec<u8>; 7],
}

impl CampaignModelHost for Fake {
    fn call(&mut self, op: u32, a: u32, text: &[u8]) -> u32 {
        match op {
            EE_SCREENS_CEM_SET_SCENARIOS => self.scenarios = split_lines(text),
            EE_SCREENS_CEM_SET_FIELD => self.fields[a.min(6) as usize] = text.to_vec(),
            EE_SCREENS_CEM_DELETE => return 1,
            _ => {}
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_CEM_TEXT_SCENARIOS => join_lines(&self.scenarios),
            EE_SCREENS_CEM_TEXT_FIELD => self.fields[arg.min(6) as usize].clone(),
            EE_SCREENS_CEM_TEXT_KEPT => b"S".to_vec(),
            _ => Vec::new(),
        }
    }
}

fn names(v: &[&str]) -> Vec<Vec<u8>> {
    v.iter().map(|s| s.as_bytes().to_vec()).collect()
}

#[test]
fn scenario_moves() {
    let mut h = Fake { scenarios: names(&["a", "b", "c", "d"]), ..Fake::default() };
    let mut m = CampaignModel { available: names(&["x", "z"]), ..CampaignModel::default() };
    assert!(m.move_scenario(&mut h, b"a", 2));
    assert_eq!(h.scenarios, names(&["b", "a", "c", "d"]));
    assert!(m.move_scenario(&mut h, b"d", -3));
    assert_eq!(h.scenarios, names(&["d", "b", "a", "c"]));
    assert!(!m.move_scenario(&mut h, b"d", -1));
    assert!(!m.move_scenario(&mut h, b"c", 1));
    assert!(m.move_scenario(&mut h, b"b", 0));
    assert!(!m.move_scenario(&mut h, b"q", 1));
    assert!(m.remove_scenario(&mut h, b"b"));
    assert_eq!(m.available, names(&["b", "x", "z"]));
    assert!(m.add_scenario(&mut h, b"z"));
    assert_eq!(h.scenarios, names(&["d", "a", "c", "z"]));
    assert!(!m.add_scenario(&mut h, b"z"));
    assert!(m.password_entered);
    m.campaigns = names(&["Mine", "Other"]);
    assert!(m.delete(&mut h, b"Mine.ssa"));
    assert_eq!(m.campaigns, names(&["Other"]));
    assert!(!m.password_entered);
}

#[test]
fn locks() {
    let mut h = Fake::default();
    let mut m = CampaignModel::default();
    assert!(m.unlocked(&mut h));
    h.fields[6] = b"pw".to_vec();
    assert!(!m.unlocked(&mut h));
    m.put(&mut h, EE_SCREENS_TEXT_CE_SET_FIELD, EE_SCREENS_CE_FIELD_PASSWORD, b"pw");
    assert!(m.unlocked(&mut h) && m.password_entered);
    assert_eq!(m.text(&mut h, EE_SCREENS_TEXT_CE_FIELD, 9).unwrap(), b"pw");
    assert_eq!(m.new_campaign(&mut h), 0);
    assert!(m.is_new && !m.password_entered && m.password.is_empty());
    assert_eq!(h.fields[EE_SCREENS_CE_FIELD_NAME as usize], b"S");
}

/// A recorded session: the new driver's host log (`build/sb1/cem_driver.cpp` over the new `EEScreens.cpp`, whose
/// fakes printed the same as over the original model): each model op (`M kind which arg text`), the host calls it
/// made (`H op a text answer`, `T which arg bytes`) and its answer (`R`). Replayed from a fresh model: the same host
/// calls in the same order and the same answers.
#[test]
fn replays_the_golden() {
    let golden = include_str!("golden.txt");
    let hex = |s: &str| -> Vec<u8> {
        let s = &s[1..];
        (0..s.len() / 2).map(|i| u8::from_str_radix(&s[2 * i..2 * i + 2], 16).unwrap()).collect()
    };
    struct Replay<'a> {
        lines: std::iter::Peekable<std::slice::Iter<'a, Vec<&'a str>>>,
        hex: &'a dyn Fn(&str) -> Vec<u8>,
    }
    impl CampaignModelHost for Replay<'_> {
        fn call(&mut self, op: u32, a: u32, text: &[u8]) -> u32 {
            let l = self.lines.next().expect("a host call past the golden");
            assert_eq!(l[0], "H", "{l:?} for call {op} {a}");
            assert_eq!((l[1].parse::<u32>().unwrap(), l[2].parse::<u32>().unwrap()), (op, a), "{l:?}");
            assert_eq!((self.hex)(l[3]), text, "{l:?}");
            l[4].parse().unwrap()
        }
        fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
            let l = self.lines.next().expect("a host text past the golden");
            assert_eq!(l[0], "T", "{l:?} for text {which} {arg}");
            assert_eq!((l[1].parse::<u32>().unwrap(), l[2].parse::<u32>().unwrap()), (which, arg), "{l:?}");
            (self.hex)(l[3])
        }
    }
    let rows: Vec<Vec<&str>> = golden.lines().map(|l| l.split(' ').collect()).collect();
    let mut replay = Replay { lines: rows.iter().peekable(), hex: &hex };
    let mut model = CampaignModel::default();
    let mut ops = 0;
    while let Some(l) = replay.lines.next() {
        assert_eq!(l[0], "M", "{l:?}");
        let (kind, which, arg) = (l[1].parse::<u32>().unwrap(), l[2].parse().unwrap(), l[3].parse().unwrap());
        let text = hex(l[4]);
        let answer = match kind {
            0 => model.op(&mut replay, which).map(|r| r.to_string()),
            1 => model
                .text(&mut replay, which, arg)
                .map(|t| format!("x{}", t.iter().map(|b| format!("{b:02x}")).collect::<String>())),
            _ => model.put(&mut replay, which, arg, &text).map(|r| r.to_string()),
        }
        .expect("a model op");
        let r = replay.lines.next().expect("the answer");
        assert_eq!(r[0], "R", "{r:?}");
        assert_eq!(answer, r[1], "op {ops}: {l:?}");
        ops += 1;
    }
    assert!(ops > 1000, "{ops} ops");
}
