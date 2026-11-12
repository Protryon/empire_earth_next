//! The campaign scenario select and scenario intro forms against the real C++: a scratch driver
//! (`build/sc6/scenario_driver.cpp`) ran the unmodified `EEUIECampaignSelectScreenEvents.cpp` and
//! `EEUIEScenarioIntroEvents.cpp` (and the intro's factory, form events and empty mouse events as `EEUIManager.cpp`
//! had them) over fake forms, lists, combo box, labels, backgrounds, manager, user interface thread, sound manager,
//! server, campaign, progress, data path, files, headers and strings, the real `UWideString` and the intro form
//! manager's own members: 2,000 cases of the 19 events and a sweep of the factory with campaigns that load or not,
//! built-in ones whose checksum matches or not, archives and maps found or not, scenario lists, completed scenarios
//! (marked lines, progress that loads or not, all open or not), the lists' lines and selections, the difficulty, the
//! intro's flags, media and files (found by the hash of their names), the backdrops; the intro's files, the sound,
//! the campaign's and the options' fields carried from case to case (`golden_scenario_screens.txt`). The replay runs
//! [`dispatch`] over a host that prints what the driver's fakes print for each host call. The driver relinked over
//! `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at ten times the size.

use std::collections::BTreeMap;
use std::sync::Mutex;

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_scenario_screens.txt");

/// The replay's sound (the game's is the FFI's static).
static STATE: Mutex<ScenarioScreens> = Mutex::new(ScenarioScreens::new());

#[derive(Default)]
struct Driver {
    out: Vec<Vec<u8>>,
    salt: u32,
    loads: bool,
    progress: bool,
    campaign: Vec<u8>,
    description: Vec<u8>,
    all_open: bool,
    checksum: u32,
    difficulty: u32,
    flags: u32,
    media: [Vec<u8>; 6], // briefing, hints, history, map, movie, sound
    scenarios: Vec<Vec<u8>>,
    completed: Vec<Vec<u8>>,
    lists: BTreeMap<u32, (u32, Vec<Vec<u8>>)>,
    labels: BTreeMap<u32, usize>,
    combo: u32,
    backgrounds: BTreeMap<(u32, i32), Vec<u8>>,
    triggers: Vec<u32>,
    // carried
    live: i32,
    file: Option<Vec<u8>>,
    files: [Option<Vec<u8>>; 3], // the intro manager's map, movie, sound
    spec: Vec<u8>,
    error: Vec<u8>,
    scenario_name: Vec<u8>,
    manager_difficulty: u32,
    difficulty2: u32,
    options: (u32, u32, Vec<u8>, Vec<u8>, Vec<u8>, Vec<u8>),
    last: Vec<u8>,
    next_sound: u32,
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

fn hash(salt: u32, text: &[u8]) -> u32 {
    text.iter().fold(salt, |h, &b| h.wrapping_mul(31).wrapping_add(b as u32))
}

fn event_name(event: u32) -> &'static str {
    match event {
        EE_SCREENS_CS_OPEN => "class EEUIECSSEFormOpen",
        EE_SCREENS_CS_CLOSE => "class EEUIECSSEFormClose",
        EE_SCREENS_CS_SELECT_CAMPAIGN => "class EEUIECSSESelectCampaign",
        EE_SCREENS_CS_SELECT_SCENARIO => "class EEUIECSSESelectScenario",
        EE_SCREENS_CS_DIFFICULTY => "class EEUIECSSESelectDifficultyLevel",
        EE_SCREENS_CS_START => "class EEUIECSSEStartScenario",
        EE_SCREENS_CS_BACK => "class EEUIECSSEBack",
        EE_SCREENS_SI_OPEN => "class EEUIESIEOpen",
        EE_SCREENS_SI_CLOSE => "class EEUIESIEClose",
        EE_SCREENS_SI_BACK => "class EEUIESIEBack",
        EE_SCREENS_SI_BRIEFING => "class EEUIESIEBriefing",
        EE_SCREENS_SI_HINTS => "class EEUIESIEHints",
        EE_SCREENS_SI_HISTORY => "class EEUIESIEHistory",
        EE_SCREENS_SI_MAP => "class EEUIESIEMap",
        EE_SCREENS_SI_MOVIE => "class EEUIESIEMovie",
        EE_SCREENS_SI_SOUND_OVER => "class EEUIESIESoundOver",
        EE_SCREENS_SI_START => "class EEUIESIEStart",
        EE_SCREENS_SI_MOUSE => "class EEUISIEMouse",
        _ => panic!("event {event:#x}"),
    }
}

impl Driver {
    fn line(&mut self, text: &str) {
        self.out.push(format!("  {text}").into_bytes());
    }
    fn bytes(&mut self, parts: &[&[u8]]) {
        self.out.push(cat(&[b"  ", &cat(parts)]));
    }
    /// EEOpenSaveGameFile: found unless the hash says no or the name has no stem
    fn open_save(&mut self, name: &[u8], directory: &[u8]) -> bool {
        let ok = !hash(self.salt, name).is_multiple_of(3) && !name.starts_with(b".");
        self.bytes(&[b"opensave '", name, b"' '", directory, format!("' -> {}", if ok { 0 } else { 1 }).as_bytes()]);
        if ok {
            self.live += 1;
        }
        ok
    }
    fn find_file(&mut self, name: &[u8]) -> bool {
        let ok = !(hash(self.salt, name) >> 4).is_multiple_of(4);
        self.bytes(&[b"findfile '", name, format!("' -> {}", if ok { 0 } else { 1 }).as_bytes()]);
        if ok {
            self.live += 1;
        }
        ok
    }
    /// OpenMap, OpenMovie, OpenSound (k 0, 1, 2): the old file deleted, the media name with its extensions
    fn open_media(&mut self, k: usize) -> bool {
        if self.files[k].take().is_some() {
            self.live -= 1;
        }
        let stem = self.media[3 + k].clone();
        let (extensions, directory): (&[&[u8]], &[u8]) = match k {
            0 => (&[b".jpg", b".png"], b"Scenarios\\"),
            1 => (&[b".bik"], b"Movies\\"),
            _ => (&[b".mp3"], b"Sounds\\"),
        };
        for extension in extensions {
            let name = cat(&[&stem, extension]);
            if self.open_save(&name, directory) {
                self.files[k] = Some(name);
                return true;
            }
        }
        false
    }
    fn map_backdrop(&mut self, form: u32, file: &[u8]) {
        self.line("lockbackground");
        self.bytes(&[b"mapbackground -2 '", file, format!("' {form} 2").as_bytes()]);
        self.backgrounds.insert((form, -2), file.to_vec());
        self.bytes(&[b"backgroundopen '", file, b"'"]);
        self.line("unlockbackground");
    }
    fn background_name(form: u32, id: i32) -> String {
        format!("{form}:{id}")
    }
}

impl ScenarioHost for Driver {
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_SHOW_SINGLE_PLAYER => self.line("showsingle"),
            EE_SCREENS_OP_FORM_LIST_CLEAR => {
                self.line(&format!("listclear {b}"));
                self.lists.get_mut(&b).unwrap().1.clear();
            }
            EE_SCREENS_OP_FORM_LIST_COUNT => return self.lists[&b].1.len() as u32,
            EE_SCREENS_OP_FORM_LIST_SELECTION => return self.lists[&b].0,
            EE_SCREENS_OP_FORM_LIST_SELECT => {
                self.line(&format!("listselect {b} {c} 1 0"));
                self.lists.get_mut(&b).unwrap().0 = c;
            }
            EE_SCREENS_OP_COMBO_CLEAR => self.line(&format!("comboclear {a}")),
            EE_SCREENS_OP_COMBO_SELECT => self.line(&format!("combosel {a} {b}")),
            EE_SCREENS_OP_REMOVE_BACKGROUND => {
                if self.backgrounds.remove(&(form, a as i32)).is_some() {
                    self.line(&format!("removebackground {}", Self::background_name(form, a as i32)));
                }
            }
            EE_SCREENS_OP_BACKGROUND => {
                assert!(self.backgrounds.contains_key(&(form, a as i32)));
                self.line(&format!("visibility {} {}", Self::background_name(form, a as i32), b));
            }
            EE_SCREENS_OP_SHOW => self.line(&format!("mode {a} {b}")),
            EE_SCREENS_OP_TRIGGER => {
                assert_eq!(a, form);
                self.line(&format!("trigger {b} {c} {}", event_name(d)));
            }
            EE_SCREENS_OP_GROUP => {
                assert_eq!(a, 0);
                self.line(if b != 0 { "groupenable" } else { "groupdisable" });
            }
            EE_SCREENS_OP_TAB_STOP => self.line(&format!("tabstop {form} {a}")),
            EE_SCREENS_OP_LABEL_CLEAR => {
                self.line(&format!("deletearray {a}"));
                self.labels.insert(a, 0);
            }
            EE_SCREENS_OP_LABEL_LINE_COUNT => return self.labels[&a] as u32,
            EE_SCREENS_OP_LABEL_START_LINE => self.line(&format!("startline {b}")),
            EE_SCREENS_OP_ADD_TRIGGER => self.triggers.push(a),
            EE_SCREENS_OP_SLEEP => self.line(&format!("sleep {a}")),
            EE_SCREENS_OP_OPEN_FORM => self.line(&format!("openform {a}")),
            EE_SCREENS_OP_CLOSE_FORM => self.line("closeform 999"),
            EE_SCREENS_OP_OPEN_MAIN_GAME_FORM => self.line("openform 11"),
            EE_SCREENS_OP_POST_EVENT => {
                self.line(&format!("postevent {} 999", event_name(a)));
                assert!(dispatch(self, &STATE, a));
            }
            EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM => {
                self.checksum = hash(self.salt, &self.campaign) % 3 + 2;
                self.line(&format!("updatechecksum {}", self.checksum));
            }
            EE_SCREENS_GAME_CAMPAIGN_CHECKSUM => return self.checksum,
            EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET => return self.all_open as u32,
            EE_SCREENS_GAME_DIFFICULTY => return self.difficulty,
            EE_SCREENS_GAME_CS_SET_DIFFICULTY => {
                self.manager_difficulty = a;
                self.difficulty = a;
                self.difficulty2 = a;
                self.line("writelock");
                self.options.1 = a;
                self.line("writeunlock");
            }
            EE_SCREENS_GAME_CS_MAP_BACKDROP => {
                let file = b"Scenarios\\een1ve1s_campaign.png".to_vec();
                if self.find_file(&file) {
                    self.map_backdrop(form, &file);
                    self.live -= 1;
                }
            }
            EE_SCREENS_GAME_SI_FLAGS => return self.flags,
            EE_SCREENS_GAME_SI_SET_FLAG => {
                if b != 0 {
                    self.flags |= a;
                } else {
                    self.flags &= !a;
                }
            }
            EE_SCREENS_GAME_SI_BLINK_OFF => {
                self.line("blinking 109 0");
                self.line("mode 235 0");
                self.line("blinking 235 0");
            }
            EE_SCREENS_GAME_SOUND_STOP => self.line(&format!("soundstop {a} 0")),
            EE_SCREENS_GAME_SOUND_LATENCY => self.line(&format!("latency {a}")),
            EE_SCREENS_GAME_SI_OPEN_MAP => return self.open_media(0) as u32,
            EE_SCREENS_GAME_SI_OPEN_MOVIE => return self.open_media(1) as u32,
            EE_SCREENS_GAME_SI_OPEN_SOUND => return self.open_media(2) as u32,
            EE_SCREENS_GAME_SI_PLAY_MOVIE => {
                let movie = if self.open_media(1) { self.files[1].clone().unwrap() } else { Vec::new() };
                self.bytes(&[b"playmovie '", &movie, format!("' {}", event_name(a)).as_bytes()]);
                assert!(dispatch(self, &STATE, a));
            }
            EE_SCREENS_GAME_SI_PLAY_SOUND => {
                let sound = if self.open_media(2) { self.files[2].clone().unwrap() } else { Vec::new() };
                let file = cat(&[b"Sounds\\", &sound]);
                self.line("latency 500");
                let id = self.next_sound;
                self.next_sound += 1;
                self.bytes(&[b"loadsound '", &file, format!("' 49 -> {id}").as_bytes()]);
                self.line(&format!("register 7 {id}"));
                let _ = a;
                return id;
            }
            EE_SCREENS_GAME_SI_MAP_BACKDROP => {
                let had = self.backgrounds.get(&(form, -2)).cloned();
                let map = if self.open_media(0) { self.files[0].clone().unwrap() } else { Vec::new() };
                let file = cat(&[b"Scenarios\\", &map]);
                if had.as_deref() != Some(file.as_slice()) {
                    if had.is_some() {
                        self.backgrounds.remove(&(form, -2));
                        self.line(&format!("removebackground {form}:-2"));
                    }
                    if self.open_media(0) {
                        self.map_backdrop(form, &file);
                    }
                }
            }
            EE_SCREENS_GAME_SI_GET_SCENARIO => {
                let Some(name) = self.file.take() else { return 0 };
                self.live -= 1;
                if self.open_save(&name, b"Scenarios\\") || self.find_file(&name) {
                    self.file = Some(name.clone());
                    self.spec = name;
                    return 1;
                }
                return 0;
            }
            EE_SCREENS_GAME_MASTER_VOLUME => self.line("mastervolume 0"),
            EE_SCREENS_GAME_SI_POST_LOAD => {
                let spec = self.spec.clone();
                self.bytes(&[b"postload '", &spec, b"'"]);
            }
            EE_SCREENS_GAME_SI_START_GAME => {
                self.line("writelock");
                self.line("setdefaults");
                if a != 0 {
                    self.options.0 = 4;
                    self.options.1 = self.manager_difficulty;
                    self.options.2 = self.campaign.clone();
                    self.options.3 = self.scenario_name.clone();
                    self.options.4 = Vec::new();
                } else {
                    self.options.0 = 1;
                    self.last = self.spec[..self.spec.len() - 4].to_vec();
                }
                self.options.5 = self.spec.clone();
                self.line("writeunlock");
                self.line("server EEName 0");
                self.line("startthreads");
                self.line("localinput 255 startloading 1 0");
            }
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, _form: u32, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_CS_CAMPAIGN_NAME => self.campaign.clone(),
            EE_SCREENS_TEXT_CAMPAIGN_EXTENSION => b".ssa".to_vec(),
            EE_SCREENS_TEXT_CAMPAIGN_ERROR => self.error.clone(),
            EE_SCREENS_TEXT_CS_SCENARIOS => self.scenarios.iter().flat_map(|s| cat(&[s, b"\0"])).collect(),
            EE_SCREENS_TEXT_CS_COMPLETED => {
                let campaign = self.campaign.clone();
                self.bytes(&[
                    b"progress 'default' '",
                    &campaign,
                    format!("' {} 0 -> {}", self.checksum, self.progress as u8).as_bytes(),
                ]);
                if self.progress {
                    self.completed.iter().flat_map(|s| cat(&[s, b"\0"])).collect()
                } else {
                    Vec::new()
                }
            }
            EE_SCREENS_TEXT_CS_PROGRESS => {
                self.line("string 31805");
                format!("P{} of {}", arg >> 16, arg & 0xffff).into_bytes()
            }
            EE_SCREENS_TEXT_CS_DESCRIPTION => self.description.clone(),
            EE_SCREENS_TEXT_FORM_LIST_DATA => {
                let list = &self.lists[&((arg >> 16) & 0xff)];
                list.1.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_SI_BRIEFING => self.media[0].clone(),
            EE_SCREENS_TEXT_SI_HINTS => self.media[1].clone(),
            EE_SCREENS_TEXT_SI_HISTORY => self.media[2].clone(),
            _ => panic!("text {which:#x}"),
        }
    }
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32 {
        match which {
            EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN => {
                return match text {
                    b"AOCRoman" => 1,
                    b"Scenario 1" => 2,
                    _ if text == "Zo\u{eb}".as_bytes() => 3,
                    b"Last" => 4,
                    _ => 0,
                };
            }
            EE_SCREENS_TEXT_COMBO_APPEND => self.bytes(&[format!("comboappend {arg} '").as_bytes(), text, b"'"]),
            EE_SCREENS_TEXT_CAMPAIGN_LOAD => {
                let file = cat(&[b"Campaigns\\", text, b".ssa"]);
                self.bytes(&[b"load '", &file, format!("' 0 -> {}", self.loads as u8).as_bytes()]);
                self.error = if self.loads { Vec::new() } else { cat(&[b"error ", &file]) };
                return self.loads as u32;
            }
            EE_SCREENS_TEXT_CS_ARCHIVE => {
                let found = self.find_file(text);
                let added = found && arg != 0;
                if added {
                    self.bytes(&[b"setarchive '", text, b"'"]);
                    self.line("addarchive 1");
                    self.line("setdatapaths");
                }
                if found {
                    self.live -= 1;
                }
                return added as u32;
            }
            EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN => self.bytes(&[b"display '", text, b"' 1 999 1 1"]),
            EE_SCREENS_TEXT_FORM_LIST_APPEND => {
                let control = arg & 0xffff;
                self.bytes(&[format!("listappend {control} '").as_bytes(), text, b"'"]);
                self.lists.get_mut(&control).unwrap().1.push(text.to_vec());
            }
            crate::menus::EE_SCREENS_TEXT_LABEL_LINES => {
                let control = arg & 0xffff;
                self.line(&format!("deletearray {control}"));
                self.bytes(&[format!("appendtext {control} '").as_bytes(), text, b"'"]);
                self.labels.insert(control, 1);
            }
            crate::menus::EE_SCREENS_TEXT_LABEL => self.bytes(&[format!("settext {arg} '").as_bytes(), text, b"' 1"]),
            EE_SCREENS_TEXT_CAMPAIGN_NAME => self.campaign = text.to_vec(),
            EE_SCREENS_TEXT_CS_SCENARIO_NAME => self.scenario_name = text.to_vec(),
            EE_SCREENS_TEXT_CS_SCENARIO_HEADER | EE_SCREENS_TEXT_CS_START => {
                let file = cat(&[b"Scenarios\\een1ve1s_", text, b".scn"]);
                if !self.find_file(&file) {
                    return 0;
                }
                let loads = !(hash(self.salt, &file) >> 6).is_multiple_of(3);
                self.bytes(&[b"header '", &file, format!("' -> {}", if loads { 0 } else { 1 }).as_bytes()]);
                if !loads || which == EE_SCREENS_TEXT_CS_SCENARIO_HEADER {
                    self.live -= 1;
                    return (loads || which == EE_SCREENS_TEXT_CS_START) as u32;
                }
                self.call(form, EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM, 0, 0, 0, 0);
                if self.file.take().is_some() {
                    self.live -= 1;
                }
                self.file = Some(file);
                self.flags |= EE_SCREENS_SI_FLAG_CAMPAIGN;
                return 2;
            }
            _ => panic!("put {which:#x}"),
        }
        0
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.line(&format!("string {id}"));
        format!("S{id}").into_bytes()
    }
    fn combo_selection(&mut self, _form: u32, control: u32) -> u32 {
        assert_eq!(control, 6);
        self.combo
    }
}

/// A header line's tokens: words, quoted texts and tagged quoted texts (`s'..'`).
enum Token {
    Word(String),
    Quoted(Vec<u8>),
    Tagged(u8, Vec<u8>),
}

fn tokens(line: &[u8]) -> Vec<Token> {
    let mut out = Vec::new();
    let mut i = 0;
    while i < line.len() {
        if line[i] == b' ' {
            i += 1;
            continue;
        }
        let tagged = i + 1 < line.len() && line[i + 1] == b'\'' && line[i] != b'\'';
        let start = if tagged { i + 1 } else { i };
        if line[start] == b'\'' {
            let end = start + 1 + line[start + 1..].iter().position(|&b| b == b'\'').unwrap();
            let text = line[start + 1..end].to_vec();
            out.push(if tagged { Token::Tagged(line[i], text) } else { Token::Quoted(text) });
            i = end + 1;
        } else {
            let end = line[i..].iter().position(|&b| b == b' ').map_or(line.len(), |p| i + p);
            out.push(Token::Word(String::from_utf8_lossy(&line[i..end]).into_owned()));
            i = end;
        }
    }
    out
}

const KINDS: [u32; 19] = [
    EE_SCREENS_CS_OPEN,
    EE_SCREENS_CS_SELECT_CAMPAIGN,
    EE_SCREENS_CS_SELECT_SCENARIO,
    EE_SCREENS_CS_DIFFICULTY,
    EE_SCREENS_CS_START,
    EE_SCREENS_CS_BACK,
    EE_SCREENS_CS_CLOSE,
    EE_SCREENS_CS_CLOSE_CALLBACK,
    EE_SCREENS_SI_OPEN,
    EE_SCREENS_SI_CLOSE,
    EE_SCREENS_SI_BACK,
    EE_SCREENS_SI_BRIEFING,
    EE_SCREENS_SI_HINTS,
    EE_SCREENS_SI_HISTORY,
    EE_SCREENS_SI_MAP,
    EE_SCREENS_SI_MOVIE,
    EE_SCREENS_SI_SOUND_OVER,
    EE_SCREENS_SI_START,
    EE_SCREENS_SI_MOUSE,
];

#[test]
fn replays_the_cpp_golden() {
    let lines: Vec<&[u8]> =
        GOLDEN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with(b"case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut d = Driver { next_sound: 40, ..Default::default() };
    *STATE.lock().unwrap() = ScenarioScreens::new();
    let mut seen = [0usize; 20];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let t = tokens(lines[a]);
        let word = |k: usize| match &t[k] {
            Token::Word(w) => w.clone(),
            _ => panic!("case {n} token {k}"),
        };
        let quoted = |k: usize| match &t[k] {
            Token::Quoted(q) => q.clone(),
            _ => panic!("case {n} token {k}"),
        };
        let num = |k: usize| word(k).parse::<u32>().unwrap();
        let kind = num(3) as usize;
        d.salt = num(5);
        d.loads = num(7) == 1;
        d.progress = num(9) == 1;
        d.campaign = quoted(11);
        d.description = if num(13) == 1 { b"the description".to_vec() } else { Vec::new() };
        d.all_open = num(15) == 1;
        d.checksum = num(17);
        d.difficulty = num(19);
        d.flags = (0..4).map(|k| num(21 + k) << k).sum();
        let file = quoted(26);
        d.file = (!file.is_empty()).then_some(file);
        for k in 0..6 {
            d.media[k] = quoted(28 + k);
        }
        let mut k = 34;
        d.scenarios.clear();
        d.completed.clear();
        while let Token::Tagged(tag, text) = &t[k] {
            if *tag == b's' {
                d.scenarios.push(text.clone())
            } else {
                d.completed.push(text.clone())
            }
            k += 1;
        }
        d.lists.clear();
        for list in [2u32, 9, 300] {
            assert_eq!(word(k), format!("list{list}"));
            let selection = num(k + 1);
            k += 2;
            let mut texts = Vec::new();
            while let Token::Quoted(q) = &t[k] {
                texts.push(q.clone());
                k += 1;
            }
            d.lists.insert(list, (selection, texts));
        }
        d.labels.clear();
        for label in [4u32, 7, 8, 10] {
            assert_eq!(word(k), format!("label{label}"));
            d.labels.insert(label, num(k + 1) as usize);
            k += 2;
        }
        d.combo = num(k + 1);
        d.live = word(k + 3).parse().unwrap();
        k += 4;
        d.backgrounds.clear();
        while k < t.len() {
            let at = word(k + 1);
            let (form, id) = at.split_once(':').unwrap();
            d.backgrounds.insert((form.parse().unwrap(), id.parse().unwrap()), quoted(k + 2));
            k += 3;
        }
        d.out = vec![lines[a].to_vec()];
        if kind == 19 {
            for id in 0..140u32 {
                let code = scenario_intro_event_for(id);
                if code != 0 {
                    d.line(&format!("factory {id} {}", event_name(code)));
                }
                match id {
                    7 => d.line(&format!("formfactory 7 {}", event_name(EE_SCREENS_SI_OPEN))),
                    10 => d.line(&format!("formfactory 10 {}", event_name(EE_SCREENS_SI_CLOSE))),
                    _ => {}
                }
            }
        } else {
            if kind != 7 {
                d.line(&format!("event {}", event_name(KINDS[kind])));
            }
            assert!(dispatch(&mut d, &STATE, KINDS[kind]));
        }
        for code in std::mem::take(&mut d.triggers) {
            d.line(&format!("totrigger {} 19", event_name(code)));
        }
        let intro_file = d.file.clone().unwrap_or_default();
        let result = cat(&[
            b"  result name '",
            &d.campaign,
            b"' scenario '",
            &d.scenario_name,
            format!(
                "' campaign {} difficulty {} {} options {} {} '",
                d.manager_difficulty, d.difficulty, d.difficulty2, d.options.0, d.options.1
            )
            .as_bytes(),
            &d.options.2,
            b"' '",
            &d.options.3,
            b"' '",
            &d.options.4,
            b"' '",
            &d.options.5,
            format!(
                "' intro {} {} {} {} file '",
                d.flags & 1,
                (d.flags >> 1) & 1,
                (d.flags >> 2) & 1,
                (d.flags >> 3) & 1
            )
            .as_bytes(),
            &intro_file,
            format!("' files {} last '", d.live).as_bytes(),
            &d.last,
            b"'",
        ]);
        d.out.push(result);
        // (a message's line break splits it as the golden's lines)
        let got: Vec<Vec<u8>> = d
            .out
            .iter()
            .flat_map(|l| l.split(|&b| b == b'\n').map(|s| s.to_vec()).collect::<Vec<_>>())
            .filter(|l| !l.is_empty())
            .collect();
        let want: Vec<Vec<u8>> = lines[a..b].iter().map(|l| l.to_vec()).collect();
        for (i, (got, want)) in got.iter().zip(&want).enumerate() {
            assert_eq!(String::from_utf8_lossy(got), String::from_utf8_lossy(want), "case {n} line {i}");
        }
        assert_eq!(got.len(), want.len(), "case {n}");
        seen[kind] += 1;
    }
    assert!(seen.iter().all(|&s| s > 50), "{seen:?}");
}
