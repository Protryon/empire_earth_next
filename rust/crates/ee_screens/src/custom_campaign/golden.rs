//! The custom campaign form against the real C++: a scratch driver (`build/sc5/custom_driver.cpp`) ran the unmodified
//! `EEUIEChooseCustomCampaignEvents.cpp` over fake forms, manager, user interface thread, campaign files, file listing
//! and strings, the real `UWideString` and `PlatMbsLwr`: 2,000 cases of the form's six events (its open, the
//! campaign's selection, OK, Cancel, its close, the no-campaigns callback) with campaign files of every kind (the
//! extension in any case, four characters or fewer, UTF-8 names, a lone lead byte, built-in campaigns, repeats),
//! the list's lines and selection, loads that succeed, fail with or without an error text, the Please wait string
//! missing; the campaign form manager's fields carried from case to case (`golden_custom_campaign.txt`). The replay
//! runs [`dispatch`] over a host that prints what the driver's fakes print for each host call. The driver relinked
//! over `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at ten times the size.

use std::collections::BTreeSet;

use super::*;
use crate::menus::{
    EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN, EE_SCREENS_OP_OPEN_FORM, EE_SCREENS_TEXT_CAMPAIGN_CIV,
    EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION, EE_SCREENS_TEXT_CAMPAIGN_FIRST, EE_SCREENS_TEXT_CAMPAIGN_LOAD,
    EE_SCREENS_TEXT_CAMPAIGN_LOADED, EE_SCREENS_TEXT_CAMPAIGN_NAME, EE_SCREENS_TEXT_LABEL_LINES,
    EE_SCREENS_TEXT_MESSAGE,
};

const GOLDEN: &[u8] = include_bytes!("../golden_custom_campaign.txt");
const BUILT_IN: [&[u8]; 3] = [b"AOCRoman", b"Learning", b"abc"];

#[derive(Default)]
struct Driver {
    out: Vec<Vec<u8>>,
    files: Vec<Vec<u8>>,
    lines: Vec<Vec<u8>>,
    selection: u32,
    salt: usize,
    missing: bool,
    open: bool,
    // the campaign form manager's and its campaign's fields, carried from case to case
    name: Vec<u8>,
    first: Vec<u8>,
    description: Vec<u8>,
    loaded_description: Vec<u8>,
    loaded_civ: Vec<u8>,
    error: Vec<u8>,
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

impl Driver {
    fn line(&mut self, text: &str) {
        self.out.push(format!("  {text}").into_bytes());
    }
}

fn event_name(event: u32) -> &'static str {
    match event {
        EE_SCREENS_CC_SELECT => "class EEUIECCCESelectCampaign",
        EE_SCREENS_CC_OK => "class EEUIECCCEOk",
        EE_SCREENS_CC_CANCEL => "class EEUIECCCECancel",
        crate::scenario_screens::EE_SCREENS_CS_OPEN => "class EEUIECSSEFormOpen",
        crate::scenario_screens::EE_SCREENS_CS_CLOSE => "class EEUIECSSEFormClose",
        _ => panic!("event {event:#x}"),
    }
}

impl CustomCampaignHost for Driver {
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32) {
        assert_eq!(form, 34);
        self.line(&format!("trigger {control} {trigger} {}", event_name(event)));
    }
    fn set_open_event(&mut self, form: u32, event: u32) {
        self.line(&format!("openevent {form} {}", event_name(event)));
    }
    fn set_close_event(&mut self, form: u32, event: u32) {
        self.line(&format!("closeevent {form} {}", event_name(event)));
    }
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        match op {
            EE_SCREENS_OP_FORM_LIST_CLEAR => {
                assert_eq!((a, b), (34, 2));
                self.lines.clear();
                self.line("listclear 2");
            }
            EE_SCREENS_OP_FORM_LIST_COUNT => return self.lines.len() as u32,
            EE_SCREENS_OP_FORM_LIST_SELECT => {
                self.selection = c;
                self.line(&format!("listselect 2 {c} 1 0"));
            }
            EE_SCREENS_OP_FORM_LIST_SELECTION => return self.selection,
            EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN => self.open = a != 0,
            crate::settings_form::EE_SCREENS_OP_CLOSE_FORM => self.line("closeform 999"),
            EE_SCREENS_OP_OPEN_FORM => self.line(&format!("openform {a}")),
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_CAMPAIGN_FILES => {
                self.line("volume 'C:'");
                self.line("directory '\\Game\\data\\Campaigns\\'");
                self.line("adddirectory 1");
                self.line("listfiles '*.ssa'");
                self.line("deletepath");
                let set: BTreeSet<Vec<u8>> = self.files.iter().cloned().collect();
                set.into_iter().flat_map(|f| cat(&[&f, b"\0"])).collect()
            }
            EE_SCREENS_TEXT_CAMPAIGN_EXTENSION => b".ssa".to_vec(),
            EE_SCREENS_TEXT_FORM_LIST_DATA => {
                assert_eq!(arg >> 16, 34 << 8 | 2);
                self.lines.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_CAMPAIGN_LOADED => self.loaded_description.clone(),
            EE_SCREENS_TEXT_CAMPAIGN_CIV => self.loaded_civ.clone(),
            EE_SCREENS_TEXT_CAMPAIGN_ERROR => self.error.clone(),
            _ => panic!("text {which:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        match which {
            EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN => return BUILT_IN.contains(&text) as u32,
            EE_SCREENS_TEXT_FORM_LIST_APPEND => {
                assert_eq!(arg, 34 << 16 | 2);
                self.lines.push(text.to_vec());
                self.out.push(cat(&[b"  listappend 2 '", text, b"'"]));
            }
            EE_SCREENS_TEXT_MESSAGE_NONE_FOUND => self.out.push(cat(&[b"  display '", text, b"' 1 999 1 1"])),
            EE_SCREENS_TEXT_MESSAGE => {
                self.out.push(cat(&[b"  display '", text, format!("' 1 999 0 {arg}").as_bytes()]))
            }
            EE_SCREENS_TEXT_CAMPAIGN_LOAD => {
                let file = cat(&[b"Campaigns\\", text, b".ssa"]);
                let ok = (utf8::char_len(&file) + self.salt) & 1 != 0;
                self.out.push(cat(&[b"  load '", &file, format!("' 0 -> {}", ok as u8).as_bytes()]));
                self.error.clear();
                if ok {
                    self.loaded_description = cat(&[b"desc of ", &file]);
                    self.loaded_civ = format!("civ{}", utf8::char_len(&file)).into_bytes();
                } else if self.salt & 2 != 0 {
                    self.error = cat(&[b"error ", &file]);
                }
                return ok as u32;
            }
            EE_SCREENS_TEXT_CAMPAIGN_NAME => self.name = text.to_vec(),
            EE_SCREENS_TEXT_CAMPAIGN_FIRST => self.first = text.to_vec(),
            EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION => self.description = text.to_vec(),
            EE_SCREENS_TEXT_LABEL_LINES => {
                assert_eq!(arg, 34 << 16 | 1);
                self.line("deletearray 1");
                self.out.push(cat(&[b"  appendtext 1 '", text, b"'"]));
            }
            _ => panic!("put {which:#x}"),
        }
        0
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.line(&format!("string {id}"));
        format!("S{id}").into_bytes()
    }
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>> {
        let s = self.string(id);
        (!(self.missing && id == 31842)).then_some(s)
    }
}

/// The quoted texts of a header line, in order.
fn quoted(line: &[u8]) -> Vec<Vec<u8>> {
    line.split(|&b| b == b'\'').skip(1).step_by(2).map(|t| t.to_vec()).collect()
}

const KINDS: [u32; 6] = [
    EE_SCREENS_CC_OPEN,
    EE_SCREENS_CC_SELECT,
    EE_SCREENS_CC_OK,
    EE_SCREENS_CC_CANCEL,
    EE_SCREENS_CC_CLOSE,
    EE_SCREENS_CC_NONE_FOUND,
];

#[test]
fn replays_the_cpp_golden() {
    // the driver's text-mode stdout ends its lines CR LF
    let lines: Vec<&[u8]> =
        GOLDEN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with(b"case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut d = Driver::default();
    let mut seen = [0usize; 6];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let head = String::from_utf8_lossy(lines[a]).into_owned();
        let t: Vec<&str> = head.split(' ').collect();
        let kind: usize = t[3].parse().unwrap();
        let files: usize = t[5].parse().unwrap();
        let texts = quoted(lines[a]);
        d.files = texts[..files].to_vec();
        let field = |key: &str| t[t.iter().rposition(|&s| s == key).unwrap() + 1];
        if kind != 0 {
            d.lines = texts[files..].to_vec();
        }
        d.selection = field("sel").parse().unwrap();
        d.salt = field("salt").parse().unwrap();
        d.missing = field("missing") == "1";
        d.open = field("open") == "1";
        d.out = vec![lines[a].to_vec()];
        assert!(dispatch(&mut d, KINDS[kind]));
        let result = cat(&[
            b"  result name '",
            &d.name,
            b"' first '",
            &d.first,
            b"' desc '",
            &d.description,
            format!("' open {}", d.open as u8).as_bytes(),
        ]);
        d.out.push(result);
        let want: Vec<Vec<u8>> = lines[a..b].iter().map(|l| l.to_vec()).collect();
        for (i, (got, want)) in d.out.iter().zip(&want).enumerate() {
            assert_eq!(String::from_utf8_lossy(got), String::from_utf8_lossy(want), "case {n} line {i}");
        }
        assert_eq!(d.out.len(), want.len(), "case {n}");
        seen[kind] += 1;
    }
    assert!(seen.iter().all(|&s| s > 200), "{seen:?}");
}
